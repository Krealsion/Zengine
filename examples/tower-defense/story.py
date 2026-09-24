# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Replay how the tower-defense example was made, from an empty game directory, through an external
Loom host (ELH) driving a fresh Workshop. README.md beside this file is the guide.

Every project operation is a run of a maintained tool in Zengine's `workshop` package: Neovim
types the game through `nvim-edit`, the Builder builds and reloads it through `builder`, Info
places panes through `place`, Inventory keeps the commands. This script only starts those runs,
waits for their verdicts, keeps the pace and writes down what happened. It never reads or writes
the game project itself.

  python story.py start  --root DIR --build TREE --loom-prefix PREFIX --zengine-prefix PREFIX
                         [--toolchain-bin DIR] [--viewport 180x80]
  python story.py replay --root DIR [--speed watch|fast|FACTOR] [--paused]
  python story.py pause | resume | step | cancel | status --root DIR
  python story.py speed  --root DIR watch|fast|FACTOR
  python story.py play | check --root DIR [--speed ...]
  python story.py stop   --root DIR [--force] [--discard-unsaved]
  python story.py again  --root DIR [--tui]  (after stop: Workshop again on the kept game, checked)
  python story.py reset  --root DIR [--discard-unsaved]  (stop, then retire the root by renaming it)
  python story.py steps
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
STORY = HERE / "story"
ZENGINE = HERE.parent.parent
PACKAGE = ZENGINE / "external-host" / "tools" / "workshop"
NT = os.name == "nt"
EXE, LIB = (".exe", ".dll") if NT else ("", ".so")


class StepFailed(Exception):
    pass


class Cancelled(Exception):
    pass


def save(path, value):
    temporary = Path(str(path) + ".new")
    temporary.write_text(json.dumps(value, indent=1) + "\n", encoding="utf-8")
    temporary.replace(path)


def load(path, default=None):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return default


def session_class(prefix):
    sys.path.insert(0, str(Path(prefix) / "lib" / "loom" / "python"))
    from loom_session.session import Session
    return Session


def pace_of(speed):
    """How fast to show the work. Speed changes typing, pauses and gestures a person watches;
    builds, loads and every check still wait on the real operation at any speed."""
    f = {"watch": 1.0, "fast": 0.0}.get(speed)
    f = float(speed) if f is None else f
    return {"factor": f, "chunk": 1500 if f <= 0 else max(200, int(1500 - 1300 * min(f, 1.0))),
            "type_ms": int(150 * f), "act_ms": int(350 * f), "gap_s": 1.0 * f}


def cell(x, y):
    """Where a map cell is painted in the game pane: row 0 is the header, a cell two columns."""
    return ["td.game", "td", y + 1, 1 + 2 * x]


def edits_of(folder):
    """A milestone's edits for workshop/nvim-edit: "@name" values are the folder's text files."""
    edits = json.loads((STORY / folder / "edits.json").read_text(encoding="utf-8"))
    for edit in edits:
        for key, value in list(edit.items()):
            if isinstance(value, str) and value.startswith("@"):
                edit[key] = (STORY / folder / value[1:]).read_text(encoding="utf-8")
    return edits


def title_of(fn):
    """A step's title: the first sentence of its docstring, on one line."""
    return " ".join((fn.__doc__ or "").split()).split(". ")[0].rstrip(".") + "."


class Story:
    """One story root: its record (story.json), its control file and its status file."""

    def __init__(self, root):
        self.root = Path(root).resolve()
        self.record = load(self.root / "story.json")
        if not self.record:
            raise SystemExit("%s holds no story.json: `story.py start` makes one" % self.root)
        self.Session = session_class(self.record["loom_prefix"])
        self.control_path = self.root / "story-control.json"
        self.status_path = self.root / "story-status.json"
        self.status = load(self.status_path, {}) or {}
        self.index = 0
        self.speed = "fast"

    # ---- pacing, pause, step and cancel ---------------------------------------------------------
    def control(self):
        return load(self.control_path, {}) or {}

    def gate(self, index, name, title):
        """Before a step: honour cancel, pause and single steps; pick up a speed change."""
        said = False
        while True:
            c = self.control()
            if c.get("cancel"):
                raise Cancelled("cancelled before step %d (%s)" % (index, name))
            self.speed = c.get("speed", self.speed)
            if not c.get("paused"):
                return
            if c.get("steps", 0) > 0:
                c["steps"] -= 1
                save(self.control_path, c)
                return
            if not said:
                self.note(state="paused", step=index, name=name, title=title)
                print("  paused before step %d (%s): `story.py resume` or `story.py step`" % (index, name),
                      flush=True)
                said = True
            time.sleep(0.3)

    def pace(self):
        return pace_of(self.speed)

    def note(self, **fields):
        self.status.update(fields, updated_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
        save(self.status_path, self.status)

    # ---- runs ---------------------------------------------------------------------------------
    def run(self, tool, label, inputs, wait=600.0):
        """One run of a maintained tool; returns its record, or raises naming the failed run."""
        name = "%02d-%s" % (self.index, label)
        runs = self.status.setdefault("runs", [])
        name += "" if name not in [r["name"] for r in runs] else "-%d" % len(runs)
        if tool not in ("workshop/verify-recipe", "workshop/source", "workshop/lane"):
            inputs = dict({"link": "workshop"}, **inputs)  # local-file tools take no link
        self.note(current_run=name, state="running")
        started = time.monotonic()
        with self.Session.attach(self.record["session"]) as s:
            typed = dict((k, json.dumps(v) if isinstance(v, (list, dict)) else v) for k, v in inputs.items())
            try:
                s.start(tool, name, typed)
            except Exception as error:
                if "release finished ones first" not in str(error):
                    raise
                # The run manager holds 64 runs a lifetime: release finished records, keep their
                # directories (the evidence), and start again.
                for r in s.runs().get("rows", []):
                    if r.get("process") in ("exited", "killed", "unknown") and r.get("state") in (
                            "passed", "failed", "error", "cancelled", "crashed", "interrupted"):
                        s.release(r["name"])
                s.start(tool, name, typed)
            try:
                rec = s.wait(name, timeout=wait)
            except Exception as error:  # a wait that ran out says nothing about the run
                rec = dict(s.run(name), state="still " + s.run(name).get("state", "?"),
                           failure="the story's wait of %gs ran out: %s" % (wait, error))
        entry = {"name": name, "tool": tool, "state": rec.get("state"), "summary": rec.get("summary", ""),
                 "failure": rec.get("failure", ""), "seconds": round(time.monotonic() - started, 2),
                 "asks": len(rec.get("asks", [])) + int(rec.get("asks_dropped", 0)),
                 "directory": rec.get("directory", "")}
        runs.append(entry)
        self.note(current_run="")
        if rec.get("state") == "cancelled":
            raise Cancelled("run %s was cancelled" % name)
        if rec.get("state") != "passed":
            raise StepFailed("run %s %s: %s" % (name, rec.get("state"), (rec.get("failure") or "").strip()))
        return rec

    def act(self, label, steps, wait=600.0):
        # Every picture is also kept as a PNG: a small, viewable copy beside the BMP.
        steps = [dict(s, png=True) if "picture" in s else s for s in steps]
        return self.run("workshop/act", label, {"steps": steps, "pace_ms": self.pace()["act_ms"]}, wait)

    def artifact(self, rec, name):
        found = [a for a in rec.get("artifacts", []) if a.get("name") == name]
        return Path(found[0]["path"]).read_text(encoding="utf-8") if found else ""

    def edit(self, label, path, edits, **more):
        p = self.pace()
        return self.run("workshop/nvim-edit", label, dict({"path": path, "edits": edits, "chunk": p["chunk"],
                                                            "pace_ms": p["type_ms"]}, **more))

    def builder(self, label, act, **more):
        return self.run("workshop/builder", label, dict({"act": act, "recipe": "tower-defense"}, **more))

    def arrange(self, label, plan):
        """Open and place panes in the order a plan gives: `open` steps and `place` steps."""
        for n, item in enumerate(plan):
            if "open" in item:
                self.act("%s-open-%d" % (label, n), [{"open": item["open"]}, {"wait": 0.5}])
            else:
                self.run("workshop/place", "%s-place-%d" % (label, n), {"panes": [item["place"]]})

    @property
    def game(self):
        return Path(self.record["game"]).as_posix()


# ---- the steps, in the order the story tells them -------------------------------------------------
def s_connect(st):
    """Check the admitted link: which far session this ELH is, and as whom Workshop knows it."""
    return st.run("workshop/connections", "connect", {})


def s_editor(st):
    """Switch the Editor to Neovim from Workshop's Terminal (docs/workshop/neovim.md)."""
    return st.act("editor", [
        {"press": "ctrl+t"}, {"expect": ["zengine.terminal", "terminal", "TERMINAL"], "seconds": 5},
        {"type": "ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim"},
        {"press": "enter"},
        {"expect": ["zengine.terminal", "terminal", "EditorSwitchAnswered"], "seconds": 60}])


def s_desk(st):
    """Arrange the build desk: Neovim large on the left, Files and the Builder on the right."""
    st.arrange("desk", load(STORY / "workspace.json")["build"])
    return st.act("desk-check", [{"rows": ["zengine.editor", "editor"], "as": "editor"},
                                 {"rows": ["zengine.files", "project-files"], "as": "files"},
                                 {"rows": ["zengine.builder-pane", "builder"], "as": "builder"}])


def s_m1(st):
    """Milestone 1, the first picture: create td.cpp in the empty game directory through Neovim."""
    return st.edit("m1-first-picture", st.game + "/td.cpp", edits_of("m1-first-picture"))


def s_recipe(st):
    """Author its recipe in Files: pick buildable (a), name, stem, installed prefixes, links."""
    r = st.record
    st.act("recipe", [
        {"into": ["zengine.files", "project-files", "Files"]}, {"press": "r"},
        {"expect": ["zengine.files", "project-files", "td.cpp"], "seconds": 5}, {"press": "a"},
        {"expect": ["zengine.files", "project-files", "> td.cpp"], "seconds": 5}, {"press": "enter"},
        {"expect": ["zengine.files", "project-files", "recipe name>"], "seconds": 5},
        {"press": "ctrl+a"}, {"press": "backspace"}, {"type": "tower-defense"}, {"press": "enter"},
        {"expect": ["zengine.files", "project-files", "artifact stem> tower-defense"], "seconds": 5},
        {"press": "enter"},
        {"expect": ["zengine.files", "project-files", "package prefix (comma-separated)>"], "seconds": 5},
        {"type": Path(r["zengine_prefix"]).as_posix() + "," + Path(r["loom_prefix"]).as_posix()},
        {"press": "enter"},
        {"expect": ["zengine.files", "project-files", "link targets (comma-separated)>"], "seconds": 5},
        {"type": "zengine::pane,zengine::activation,zengine::input,zengine::timer,loom::switchboard"},
        {"press": "enter"},
        {"expect": ["zengine.files", "project-files", "authored recipe `tower-defense`"], "seconds": 5}])
    return st.run("workshop/verify-recipe", "recipe-check",
                  {"project": st.game, "recipe": "tower-defense", "artifact": "tower-defense"})


def s_load(st):
    """Put the artifact into the project's load plan with its role (Builder o)."""
    return st.builder("load-it", "load-it", role="td.game", seconds=30)


def use_recipes(st, label):
    """Make the game's catalog current again (Files u on build-recipes.json)."""
    return st.act(label, [
        {"into": ["zengine.files", "project-files", "Files"]}, {"press": "r"},
        {"press": "up", "repeat": 4},
        {"expect": ["zengine.files", "project-files", "> build-recipes.json"], "seconds": 5},
        {"press": "u"},
        {"expect": ["zengine.files", "project-files", "build-recipes.json (2 recipes)"], "seconds": 5}])


def s_first_build(st):
    """Build what the project waits on (Builder f). A recipe from Files borrows no toolchain; where
    CMake's own default cannot build for this Workshop, read the failure, name the toolchain the
    Workshop was built with (and a fresh build workspace) in the recipe through Neovim, and build
    again. Where the default works, the first build is the only one."""
    first = st.builder("first-build", "frontier", expect="any")
    if "succeeded" in first.get("summary", ""):
        return first
    said = st.artifact(first, "output.txt")
    if "CMAKE_CXX_COMPILER" not in said and "Does not match the generator" not in said:
        raise StepFailed("the first build failed for a reason this story does not repair:\n" + said[-800:])
    read = st.run("workshop/source", "recipe-read", {"root": st.game, "op": "read",
                                                    "path": "build-recipes.json"})
    line = st.artifact(read, "result.txt").split("\n")[1][7:]  # after "%5d  ", the line number
    toolchain = Path(st.record["build"]).as_posix()
    workspace = (Path(st.record["root"]) / "game-build").as_posix()
    fixed = line.replace('"toolchain_from":""', '"toolchain_from":"%s"' % toolchain).replace(
        '"workspace":""', '"workspace":"%s"' % workspace)
    if fixed == line:
        raise StepFailed("the recipe already names a toolchain and a workspace; read the build output")
    st.edit("recipe-toolchain", st.game + "/build-recipes.json",
            [{"replace": '"recipe":"tower-defense"', "text": fixed}], to_unix=True)
    use_recipes(st, "use-recipes")
    return st.builder("second-build", "frontier")


def s_game_pane(st):
    """Open the game's pane and give it its place on the desk."""
    st.act("game-open", [{"open": "Tower Defense"}, {"wait": 0.5}])
    st.run("workshop/place", "game-place", {"panes": [load(STORY / "workspace.json")["game"]]})
    return st.act("game-look", [{"press": "ctrl+p"},
                                {"expect": ["td.game", "td", "TOWER DEFENSE"], "seconds": 5},
                                {"picture": "first-picture"}])


def s_save_desk(st):
    """Save the desk through its owner: put the game pane down (Escape), then s."""
    st.act("desk-save", [{"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "escape"},
                         {"press": "s"}, {"wait": 0.8}])
    listed = st.run("workshop/source", "desk-saved", {"root": st.game, "op": "list"})
    if "workshop-setup.json" not in st.artifact(listed, "result.txt"):
        raise StepFailed("no workshop-setup.json in the game directory after s")
    return listed


def s_arm(st):
    """Turn load-after-build on, so each build reloads the running game in place."""
    return st.builder("arm", "arm", seconds=20)


def milestone(st, folder):
    st.edit(folder, st.game + "/td.cpp", edits_of(folder))
    rec = st.builder(folder + "-build", "build")
    if "reloaded in place" not in rec.get("summary", ""):
        raise StepFailed("%s built but did not reload in place: %s" % (folder, rec.get("summary")))
    return rec


def s_m2(st):
    """Milestone 2, interaction: cursor keys, towers with a cost, presses on cells. Try them."""
    milestone(st, "m2-interaction")
    return st.act("m2-try", [
        {"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "right", "repeat": 2}, {"press": "up"},
        {"press": "t"}, {"expect": ["td.game", "td", "Tower built at 5,2."], "seconds": 5},
        {"at": cell(8, 4)}, {"expect": ["td.game", "td", "Cell 8,4 chosen"], "seconds": 5},
        {"at": cell(8, 4)}, {"expect": ["td.game", "td", "Tower built at 8,4."], "seconds": 5},
        {"press": "left", "repeat": 2}, {"press": "t"},
        {"expect": ["td.game", "td", "Towers go beside the road"], "seconds": 5}])


def s_m3(st):
    """Milestone 3, the rules: waves, towers that shoot, lives, winning and losing. Hold wave 1."""
    milestone(st, "m3-rules")
    return st.act("m3-try", [
        {"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "space"},
        {"expect": ["td.game", "td", "Wave 1:"], "seconds": 5}, {"picture": "wave-1"},
        {"expect": ["td.game", "td", "Wave 1 held"], "seconds": 90}])


def s_m4(st):
    """Milestone 4, checks and commands: the rules checked on scratch games (c), and TdCommand."""
    milestone(st, "m4-checks")
    return check(st, "m4-check")


def check(st, label):
    return st.act(label, [{"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "c"},
                          {"expect": ["td.game", "td", "rules check: 12/12 passed"], "seconds": 5},
                          {"rows": ["td.game", "td"], "as": "game"}])


def s_toolbox(st):
    """A second layout for the toolbox: store the five game commands through Compose, name and
    file them in a folder, bind them to keys in a portable row, and save the toolbox."""
    tb = load(STORY / "toolbox.json")
    st.act("toolbox-layout", [{"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "escape"},
                              {"press": "="}, {"expect": ["zengine.info", "info", "PANES"], "seconds": 5}])
    st.arrange("toolbox", load(STORY / "workspace.json")["toolbox"])
    steps = [{"click": ["zengine.introspection", "loaded", tb["compose"]["weave"]]},
             {"expect": ["zengine.composer", "compose", tb["compose"]["shape"]], "seconds": 10},
             {"into": ["zengine.composer", "compose", "to @"]},
             {"press": "down", "repeat": tb["compose"]["down"]},
             {"expect": ["zengine.composer", "compose", "> " + tb["compose"]["shape"]], "seconds": 5},
             {"press": "enter"}, {"expect": ["zengine.composer", "compose", "verb:Text"], "seconds": 5}]
    for n, c in enumerate(tb["commands"]):
        steps += [{"press": "ctrl+a"}, {"press": "backspace"}, {"type": c["verb"]}, {"press": "ctrl+s"},
                  {"expect": ["zengine.inventory-pane", "inventory", "INVENTORY %d" % (n + 1)], "seconds": 5}]
    st.act("toolbox-store", steps)
    plan = {"folder": tb["folder"], "entries": [
        {"find": {"label": "TdCommand", "schema": "TdCommand", "contains": c["verb"]}, "label": c["label"]}
        for c in tb["commands"]]}
    st.run("workshop/inventory-organize", "toolbox-organize", {"plan": plan})
    st.run("workshop/inventory-controls", "toolbox-controls", {
        "folder": tb["folder"], "target": tb["target"], "kind": "row",
        "controls": [{"label": c["label"], "key": c["key"]} for c in tb["commands"]]})
    st.act("toolbox-row", [{"open": "Inventory row 1"}, {"wait": 0.5}])
    st.run("workshop/place", "toolbox-row-place", {"panes": [load(STORY / "workspace.json")["row"]]})
    # THE BOUNDARY, SHOWN: a guest's gesture runs a stored command only with the guest's own grant.
    st.act("toolbox-refusal", [
        {"click": ["zengine.inventory-pane", "inventory.1", "Start"], "button": "right"}, {"wait": 0.6},
        {"press": "down", "repeat": 7}, {"press": "enter"},
        {"expect": ["zengine.inventory-pane", "inventory.1", "no authority"], "seconds": 5},
        {"picture": "toolbox"}])
    return st.run("workshop/toolbox", "toolbox-save", {"operation": "save", "path": tb["file"]})


def s_back(st):
    """Back to the build desk, restored from the setup file saved earlier (Escape, `,`, r)."""
    return st.act("back", [{"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "escape"},
                           {"press": ","}, {"press": "r"}, {"wait": 1.0},
                           {"rows": ["zengine.editor", "editor"], "as": "editor"}])


def play(st, label="play"):
    """Play the planned session from a new game: build towers between waves, win all five."""
    plan = load(STORY / "play.json")
    steps = [{"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "r"},
             {"expect": ["td.game", "td", "Place towers beside the road"], "seconds": 5}]
    for n, r in enumerate(plan["rounds"]):
        for x, y in r["towers"]:
            steps += [{"at": cell(x, y)}, {"at": cell(x, y)},
                      {"expect": ["td.game", "td", "Tower built at %d,%d." % (x, y)], "seconds": 5}]
        steps += [{"into": ["td.game", "td", "TOWER DEFENSE"]}, {"press": "space"},
                  {"expect": ["td.game", "td", "Wave %d:" % (n + 1)], "seconds": 5}]
        if n == len(plan["rounds"]) - 1:
            steps += [{"wait": 6}, {"picture": "last-wave"}]
        steps += [{"expect": ["td.game", "td", r["expect"]], "seconds": plan["wave_seconds"]}]
    steps += [{"rows": ["td.game", "td"], "as": "won"}, {"picture": "won"}]
    return st.act(label, steps, wait=900)


def s_play(st):
    """A play session through the game's own keys and presses, to a win."""
    return play(st)


def s_keep(st):
    """Keep the game: promote the running image, so the next launch of this runtime runs it."""
    return st.builder("keep", "promote", seconds=30)


def s_same(st):
    """The typed game is byte for byte the example's td.cpp (a local comparison)."""
    return st.run("workshop/source", "same", {"root": st.game, "op": "compare", "path": "td.cpp",
                                              "other": (HERE / "td.cpp").as_posix()})


STEPS = [("connect", s_connect), ("editor", s_editor), ("desk", s_desk), ("m1", s_m1),
         ("recipe", s_recipe), ("load", s_load), ("first-build", s_first_build),
         ("game-pane", s_game_pane), ("save-desk", s_save_desk), ("arm", s_arm), ("m2", s_m2),
         ("m3", s_m3), ("m4", s_m4), ("toolbox", s_toolbox), ("back", s_back), ("play", s_play),
         ("keep", s_keep), ("same", s_same)]


# ---- the commands ------------------------------------------------------------------------------------
def native(p):
    """Git Bash hands `/g/x` to a Windows Python unconverted when it is not the first word."""
    if NT and len(p) > 2 and p[0] == "/" and p[1].isalpha() and p[2] == "/":
        return p[1].upper() + ":" + p[2:]
    return p


def wait_for(fn, seconds, what):
    end, last = time.monotonic() + seconds, None
    while time.monotonic() < end:
        try:
            got = fn()
            if got:
                return got
        except Exception as error:  # not ready yet is the expected answer while waiting
            last = error
        time.sleep(0.2)
    raise SystemExit("%s did not happen within %gs: %s" % (what, seconds, last))


def launch(runtime, game, wdir, sdir, loom, zprefix, env, viewport, plan, extra=()):
    """Workshop from `runtime` with `game` as its project and a window of `viewport` cells, then a
    Loom session in `sdir` linked to it as a guest. Returns what a record keeps of the two."""
    width, height = (int(v) for v in viewport.split("x"))
    desk = {"format": "zengine-workshop-setup", "format_version": "3", "name": "Default",
            "panes": [{"provider": "zengine.info", "pane": "info",
                       "place": {"mode": "subcells", "x": str((width - 46) * 48), "y": str(2 * 48)},
                       "width": {"mode": "subcells", "amount": str(44 * 48)},
                       "height": {"mode": "subcells", "amount": str(16 * 48)}, "front": "0"}]}
    # A guest cannot size the window: a session file's viewport does, beside Workshop's first desk.
    save(wdir / "session.json", {"zen": 1, "schema": "WorkshopSession", "version": 6, "fields": {
        "format": "zengine-workshop-session", "format_version": "6",
        "viewport": {"width": str(width), "height": str(height)}, "active": "0",
        "placement": {"mode": "none", "x": "0", "y": "0", "window": "normal"},
        "layouts": [{"desk": desk, "link": {"path": "", "known": {"format": "zengine-workshop-setup",
                                                                 "format_version": "3", "name": "",
                                                                 "panes": []}}}]}})
    credential = secrets.token_urlsafe(32)
    powers = ["input", "capture", "inspect", "inventory", "toolbox"]
    save(wdir / "guests.json", {"listen": "127.0.0.1:0", "port_file": (wdir / "guests.port").as_posix(),
                                "guests": [{"name": "td-maker", "credential": credential, "may": powers}]})
    wargs = [str(runtime / ("zengine-workshop" + EXE)), "--isolated", "--session", str(wdir / "session.json"),
             "--guests", str(wdir / "guests.json"), "--log", str(wdir / "workshop.log"), "--log-refusals",
             "--demo-history", "--dump", str(wdir / "history.txt")]
    # No plan named: the project's own workshop-plan.json, else the runtime's terminal plan.
    wargs += (["--load-plan", str(runtime / plan)] if plan else []) + list(extra)
    flags = subprocess.CREATE_NO_WINDOW if NT else 0
    workshop = subprocess.Popen(wargs, cwd=str(game), stdout=(wdir / "process.log").open("wb"),
                                stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, env=env, creationflags=flags)
    port = wait_for(lambda: (wdir / "guests.port").read_text().strip(), 90, "Workshop's guest port")
    runs = loom / "lib" / "loom" / ("loom-runs" + LIB)
    vocab = zprefix / "lib" / "zengine" / ("zengine-guest-vocabulary" + LIB)
    save(sdir / "loom-boot.json", {
        "boot": [{"name": "runs", "path": str(runs), "role": "loom.runs"}, {"name": "vocab", "path": str(vocab)}],
        "links": [{"name": "workshop", "connect": "127.0.0.1:" + port, "identity": "td-story",
                   "credential": credential}],
        "history": {"log": "session.log", "recent": "8192", "payload_budget": "134217728",
                    "keep": [{"shape": "InputInjected"}, {"shape": "SurfaceCaptured"},
                             {"shape": "loom.link.Outcome"}, {"shape": "InventoryToolboxFinished"}]}})
    save(sdir / "loom-tools.json", {"python": sys.executable, "runtime": str(loom / "lib" / "loom" / "python"),
                                    "packages": [{"path": str(PACKAGE), "approve": "any-revision"}]})
    approvals = ["authority trust runs --rebuilds", "authority trust vocab --rebuilds"]
    approvals += ["authority allow runs %s v1 -> role loom.session" % s
                  for s in ("loom.session.ExpectRun", "loom.session.ForgetRun")]
    approvals += ["authority allow runs loom.runs.%s v1 -> any target" % s
                  for s in ("Tools", "ToolDescription", "Run", "RunList", "Directive")]
    approvals += ["authority allow runs loom.link.%s v1 -> role loom.link.workshop" % s
                  for s in ("Ask", "StatusRequested")]
    approvals += ["start vocab %s" % vocab, "start runs %s loom.runs" % runs]
    host = subprocess.Popen([str(loom / "bin" / ("loom-host" + EXE)), "--serve", str(sdir)], cwd=str(sdir),
                            stdout=(sdir / "process.log").open("wb"), stderr=subprocess.STDOUT,
                            stdin=subprocess.PIPE, env=env, creationflags=flags)
    host.stdin.write(("\n".join(approvals) + "\n").encode())
    host.stdin.close()
    Session = session_class(loom)

    def admitted():
        with Session.attach(str(sdir)) as s:
            s.tools()
            return any(r["name"] == "workshop" and r["state"] == "admitted" for r in s.describe()["links"])

    wait_for(admitted, 90, "the ELH's link to Workshop")
    with Session.attach(str(sdir)) as s:
        lifetime = s.lifetime
    return {"workshop_pid": workshop.pid, "host_pid": host.pid, "endpoint": "127.0.0.1:" + port,
            "lifetime": lifetime, "powers": powers, "viewport": viewport, "load_plan": plan or "the project's"}


def start(args):
    """A new story root: a development runtime copied from the build tree, an EMPTY game
    directory, Workshop launched from the runtime with that directory as its project, and a Loom
    session linked to it that trusts Zengine's workshop tool package. Refuses an existing root."""
    root = Path(native(args.root)).resolve()
    build, loom, zprefix = (Path(native(p)).resolve() for p in (args.build, args.loom_prefix, args.zengine_prefix))
    if root.exists():
        raise SystemExit("refusing: %s exists -- a story root is never reused (`story.py reset` retires one)" % root)
    t0 = time.monotonic()
    env = dict(os.environ)
    if args.toolchain_bin:
        env["PATH"] = str(Path(native(args.toolchain_bin))) + os.pathsep + env.get("PATH", "")
    root.mkdir(parents=True)
    runtime, game, wdir, sdir = root / "runtime", root / "game", root / "workshop", root / "elh"
    for d in (game, wdir, sdir):
        d.mkdir()
    with (root / "runtime-make.log").open("wb") as out:
        made = subprocess.run(["cmake", "-DZEN_RUNTIME=" + runtime.as_posix(), "-P",
                               str(build / "workshop" / "development-runtime.cmake")],
                              stdout=out, stderr=subprocess.STDOUT, env=env)
    if made.returncode:
        raise SystemExit("the runtime script failed (exit %d): %s" % (made.returncode, root / "runtime-make.log"))
    record = {"root": str(root), "runtime": str(runtime), "game": str(game), "session": str(sdir),
              "build": str(build), "loom_prefix": str(loom), "zengine_prefix": str(zprefix),
              "toolchain_bin": native(args.toolchain_bin)}
    record.update(launch(runtime, game, wdir, sdir, loom, zprefix, env, args.viewport, "graphical-load-plan.json"))
    record.update(started_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                  start_seconds=round(time.monotonic() - t0, 2))
    save(root / "story.json", record)
    print(json.dumps(record, indent=1))


def replay(args):
    st = Story(native(args.root))
    save(st.control_path, {"cancel": False, "paused": bool(args.paused), "steps": 0, "speed": args.speed})
    st.speed = args.speed
    t0 = time.monotonic()
    st.status = {"state": "running", "speed": args.speed, "runs": [], "steps": [],
                 "replay_started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    name = ""
    try:
        for i, (name, fn) in enumerate(STEPS, 1):
            st.index = i
            title = title_of(fn)
            st.gate(i, name, title)
            st.note(state="running", step=i, name=name, title=title)
            print("[%02d/%02d] %s -- %s" % (i, len(STEPS), name, title), flush=True)
            began = time.monotonic()
            rec = fn(st) or {}
            took = time.monotonic() - began
            st.status["steps"].append({"step": i, "name": name, "seconds": round(took, 2)})
            print("        passed in %.1fs: %s" % (took, (rec.get("summary") or "")[:140]), flush=True)
            if st.pace()["gap_s"]:
                time.sleep(st.pace()["gap_s"])
    except Cancelled as why:
        st.note(state="cancelled", why=str(why))
        print("CANCELLED: %s -- a cancelled replay does not resume: `story.py reset`, start, replay" % why)
        return 3
    except StepFailed as why:
        st.note(state="failed", why=str(why))
        print("FAILED at step %d (%s): %s" % (st.index, name, why))
        return 1
    runs = st.status["runs"]
    st.note(state="done", step=len(STEPS), seconds=round(time.monotonic() - t0, 1), runs_total=len(runs),
            asks_total=sum(r["asks"] for r in runs))
    print("DONE in %.1fs: %d ELH runs, %d asks recorded by the run manager" % (
        time.monotonic() - t0, len(runs), sum(r["asks"] for r in runs)))
    return 0


def controls(args):
    st = Story(native(args.root))
    c = st.control()
    if args.command == "pause":
        c["paused"] = True
    elif args.command == "resume":
        c.update(paused=False, steps=0)
    elif args.command == "step":
        c.update(paused=True, steps=c.get("steps", 0) + 1)
    elif args.command == "speed":
        pace_of(args.value)
        c["speed"] = args.value
    elif args.command == "cancel":
        c["cancel"] = True
    save(st.control_path, c)
    current = (load(st.status_path, {}) or {}).get("current_run")
    if args.command == "cancel" and current:
        with st.Session.attach(st.record["session"]) as s:
            s.cancel(current, "story.py cancel")
        print("asked the run manager to cancel %s; its cleanup gives Workshop's input back" % current)
    print(json.dumps(c))


def status(args):
    st = Story(native(args.root))
    s = load(st.status_path, {}) or {}
    with st.Session.attach(st.record["session"]) as session:
        links = [(r["name"], r["state"]) for r in session.describe()["links"]]
        lifetime = session.lifetime
    print(json.dumps({"state": s.get("state"), "step": s.get("step"), "name": s.get("name"),
                      "title": s.get("title"), "current_run": s.get("current_run"),
                      "runs": len(s.get("runs", [])), "control": st.control(), "links": links,
                      "lifetime": lifetime, "recorded_lifetime": st.record["lifetime"]}, indent=1))


def one(args):
    st = Story(native(args.root))
    st.speed = args.speed
    st.index = 90 if args.command == "play" else 91
    st.status.setdefault("runs", [])
    rec = play(st, "play-again") if args.command == "play" else check(st, "check-again")
    print(rec.get("summary"))


def quit_workshop(st, force, discard=False):
    """Ask a story's Workshop to quit as a maker would -- a pane put down with Escape, then the
    desk's q -- and confirm it by the guest link closing; then end its ELH session. A pane that
    keeps Escape for itself (Info does) cannot give the desk its keys, so each candidate is tried
    until the link closes. A Workshop that will not quit is left running and said, unless force.
    Workshop will not quit over Neovim's unsaved work, which a cancelled edit leaves behind:
    `discard` first discards it in Neovim (Escape, :e!), as Workshop's own notice asks.
    Returns whether it quit."""
    st.index = 99

    def closed(s, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if all(r["name"] != "workshop" or r["state"] != "admitted" for r in s.describe()["links"]):
                return True
            time.sleep(0.3)
        return False

    with st.Session.attach(st.record["session"]) as s:
        gone = closed(s, 0.1)
        if discard and not gone:
            try:
                st.act("discard", [{"into": ["zengine.editor", "editor", "UNSAVED"], "seconds": 2},
                                   {"press": "escape"}, {"type": ":e!\n"},
                                   {"expect": ["zengine.editor", "editor", "saved "], "seconds": 5}], wait=60)
            except (StepFailed, Cancelled) as why:
                print("discard: %s" % str(why).splitlines()[0])
        for n, (provider, pane, text) in enumerate([("zengine.terminal", "terminal", "TERMINAL"),
                                                    ("td.game", "td", "TOWER DEFENSE"), ("", "", "")]):
            # Escape reaches the desk from these two: the Terminal's once its line and list are
            # empty, the game's because it binds no Escape. Files, the Builder, Info and the Pane
            # Manager keep Escape for themselves. Last, Escape and q go to whatever holds the keys.
            # The quit ends the link, so the run that asks it reports the link lost: the link's
            # closing, not the run's verdict, is the evidence.
            if gone:
                break
            into = [{"into": [provider, pane, text], "seconds": 2}] if provider else []
            try:
                st.act("quit-%d" % n, into + [{"press": "escape"}, {"press": "q"}], wait=60)
            except (StepFailed, Cancelled) as why:
                print("quit through %s: %s" % (pane or "the keys' holder", str(why).splitlines()[0]))
            gone = closed(s, 10)
        if not gone and not force:
            raise SystemExit("Workshop did not quit (pid %s); read its notice, or stop with --force"
                             % st.record["workshop_pid"])
        s.shutdown("story.py stop")
    if not gone and force and NT:
        subprocess.run(["taskkill", "/PID", str(st.record["workshop_pid"]), "/F"])
    print("stopped: Workshop %s, ELH lifetime %s ended" % ("quit" if gone else "was killed",
                                                             st.record["lifetime"]))
    return gone


def stop(args):
    st = Story(native(args.root))
    if st.record.get("stopped_utc"):
        print("already stopped at %s" % st.record["stopped_utc"])
        return
    gone = quit_workshop(st, args.force, args.discard_unsaved)
    save(st.root / "story.json", dict(st.record, stopped="quit" if gone else "killed",
                                      stopped_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())))


def reset(args):
    """The story's reset: stop a running story, then retire its root by renaming it. Nothing is
    deleted -- the old root keeps its evidence -- and nothing outside the root is touched."""
    root = Path(native(args.root)).resolve()
    record = load(root / "story.json")
    if not record or Path(record["root"]).resolve() != root:
        raise SystemExit("refusing: %s holds no story.json of its own" % root)
    try:
        stop(argparse.Namespace(root=str(root), force=args.force, discard_unsaved=args.discard_unsaved))
    except SystemExit as why:
        if "did not quit" in str(why):
            raise
        print("stop: %s" % why)
    except Exception as why:  # an ended session cannot be attached: nothing to stop
        print("stop: %s" % why)
    retired = root.with_name(root.name + ".retired-" + time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()))
    for attempt in range(40):  # a process just stopped can hold the directory for a moment
        try:
            root.rename(retired)
            break
        except PermissionError:
            if attempt == 39:
                raise
            time.sleep(0.25)
    print("retired %s -> %s (nothing deleted)" % (root, retired))


def again(args):
    """Launch Workshop again on a stopped story's game, with a new Loom session, and check it.
    In the window, Workshop launches in the game directory with no plan named, so the project's
    own plan loads the image `keep` promoted. With --tui it is the terminal medium: a plan names
    its skin, so the runtime's terminal plan runs in a project of its own with the story's
    recipes, and the Builder's `o` loads the kept game, as a maker would. Either way the game must
    pass its rules check and run a wave under its keys, and the example's toolbox must restore
    beside it; then that Workshop is asked to quit. Its files go to <root>/again-N/, and the
    story's game directory is written by Workshop alone."""
    st = Story(native(args.root))
    if not st.record.get("stopped_utc"):
        raise SystemExit("refusing: the story in %s still runs -- `story.py stop` it first" % st.root)
    adir = st.root / ("again-%d" % (1 + len(list(st.root.glob("again-*")))))
    wdir, sdir = adir / "workshop", adir / "elh"
    for d in (wdir, sdir):
        d.mkdir(parents=True)
    game = Path(st.record["game"])
    if args.tui:
        project, plan = adir / "project", "default-load-plan.json"
        extra = ["--recipes", str(game / "build-recipes.json")]
        project.mkdir()
    else:
        project, plan, extra = game, None, []
    env = dict(os.environ)
    tbin = native(args.toolchain_bin) or st.record.get("toolchain_bin", "")
    if tbin:
        env["PATH"] = str(Path(tbin)) + os.pathsep + env.get("PATH", "")
    t0 = time.monotonic()
    record = launch(Path(st.record["runtime"]), project, wdir, sdir, Path(st.record["loom_prefix"]),
                    Path(st.record["zengine_prefix"]), env, args.viewport, plan, extra)
    record.update(session=str(sdir), project=str(project), medium="terminal" if args.tui else "window",
                  started_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                  start_seconds=round(time.monotonic() - t0, 2))
    save(adir / "again.json", record)
    print(json.dumps(record, indent=1))
    st.record = dict(st.record, **record)
    st.status_path, st.control_path = adir / "again-status.json", adir / "again-control.json"
    st.status, st.speed, st.index = {"runs": []}, args.speed, 1
    td = ["td.game", "td"]
    verdict = "passed"
    try:
        st.run("workshop/connections", "connect", {})
        if args.tui:
            st.index = 2
            st.act("builder", [{"open": "Builder"}, {"expect": ["zengine.builder-pane", "builder", "BUILDER"],
                                                     "seconds": 10}])
            st.builder("load-it", "load-it", role="td.game", seconds=180)
        st.index = 3
        # The cursor of a new game starts on 3,3, beside the road; two cells right is 5,3.
        st.act("kept-game", [{"open": "Tower Defense"}, {"expect": td + ["TOWER DEFENSE"], "seconds": 10},
                             {"press": "r"}, {"expect": td + ["Place towers beside the road"], "seconds": 5},
                             {"press": "c"}, {"expect": td + ["rules check: 12/12 passed"], "seconds": 10},
                             {"press": "t"}, {"expect": td + ["Tower built at 3,3."], "seconds": 5},
                             {"press": "right"}, {"press": "right"}, {"press": "t"},
                             {"expect": td + ["Tower built at 5,3."], "seconds": 5},
                             {"press": "space"}, {"expect": td + ["Wave 1:"], "seconds": 5},
                             {"wait": 4}, {"rows": td, "as": "mid-wave"}, {"picture": "mid-wave"}])
        st.index = 4
        # THE EXAMPLE'S TOOLBOX, RESTORED BESIDE THE KEPT GAME: its folder, its five entries and
        # their row come back, and the hotkeys stay OFF until a maker enables them.
        st.run("workshop/toolbox", "toolbox", {"operation": "restore", "replace": True,
                                               "path": (HERE / "tower-defense.toolbox").as_posix()})
        inv = ["zengine.inventory-pane", "inventory"]
        st.act("toolbox-look", [{"open": "Inventory"}, {"expect": inv + ["Tower Defense/"], "seconds": 10},
                                {"expect": inv + ["hotkeys OFF"], "seconds": 5}, {"rows": inv, "as": "inventory"}])
    except (StepFailed, Cancelled) as why:
        verdict = "failed: %s" % why
    finally:
        gone = quit_workshop(st, force=True)
    record.update(verdict=verdict, quit="quit" if gone else "killed", runs=st.status["runs"],
                  seconds=round(time.monotonic() - t0, 1))
    save(adir / "again.json", record)
    print("again (%s): %s -- %s" % (record["medium"], verdict, adir))
    return 0 if verdict == "passed" else 1


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("command", choices=["start", "replay", "pause", "resume", "step", "speed", "cancel",
                                       "status", "play", "check", "stop", "reset", "again", "steps"])
    p.add_argument("value", nargs="?", default="")
    p.add_argument("--root")
    p.add_argument("--build")
    p.add_argument("--loom-prefix")
    p.add_argument("--zengine-prefix")
    p.add_argument("--toolchain-bin", default="")
    p.add_argument("--viewport", default="180x80", help="window size in cells; the story places panes for 180x80")
    p.add_argument("--speed", default="fast")
    p.add_argument("--paused", action="store_true")
    p.add_argument("--force", action="store_true")
    p.add_argument("--discard-unsaved", action="store_true",
                   help="stop, reset: discard Neovim's unsaved buffer first (a cancelled edit leaves one)")
    p.add_argument("--tui", action="store_true", help="again: the terminal medium, not the window")
    args = p.parse_args()
    if args.command == "steps":
        for i, (name, fn) in enumerate(STEPS, 1):
            print("%2d  %-12s %s" % (i, name, title_of(fn)))
        return 0
    if not args.root:
        p.error("--root is required")
    code = {"start": start, "replay": replay, "pause": controls, "resume": controls, "step": controls,
            "speed": controls, "cancel": controls, "status": status, "play": one, "check": one,
            "stop": stop, "reset": reset, "again": again}[args.command](args) or 0
    if args.command == "start":
        # Workshop and the Loom host outlive this command by design. Ending here skips the
        # interpreter's teardown, which polls their process handles as it finalizes and, on
        # Windows, reports one of them invalid.
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(code)
    return code


if __name__ == "__main__":
    sys.exit(main())
