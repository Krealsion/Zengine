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
  python story.py again  --root DIR [--tui] [--hold]  (after stop: Workshop again on the kept game)
  python story.py reset  --root DIR [--force] [--discard-unsaved]  (stop, then retire the root)
  python story.py steps

WHAT A WAIT MEANS. A run is started and waited for; when the story's wait runs out first, the run
is UNRESOLVED: still the run manager's, neither failed nor cancelled. Its handle (name and host
lifetime) stays in story-status.json until the run is seen to settle, and nothing new starts on
the root meanwhile. `status` reads the run again, `cancel` asks the manager to cancel it and
reports the ending it then sees; neither a lost connection nor a cleared field is taken for an
ending. Nothing that may have run is started a second time.

WHAT STOPPED MEANS. A root keeps, for each process it starts, its id and the start time its
operating system gives it, so a later command knows it is the same process and not a new one with
a reused id. `stop` asks Workshop to quit through the ELH, and believes it gone only when that
process is seen to have ended; only then is the ELH session ended, and it too must be seen to end.
`--force` ends a process that will not quit -- only one whose identity is confirmed -- and says so
only once the ending is seen. Anything less is reported with what still runs, and the root is
neither marked stopped nor retired. While a replay still runs in the root, `stop` asks nothing
unless forced, and `play` and `check` start nothing: the replay's status is its own to write.
"""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import signal
import subprocess
import sys
import threading
import time

HERE = Path(__file__).resolve().parent
STORY = HERE / "story"
ZENGINE = HERE.parent.parent
PACKAGE = ZENGINE / "external-host" / "tools" / "workshop"
NT = os.name == "nt"
EXE, LIB = (".exe", ".dll") if NT else ("", ".so")
FINAL = ("passed", "failed", "error", "cancelled", "crashed", "interrupted")
SETTLED = FINAL + ("absent",)  # absent: the manager holds no run of that name -- it never started
ENDED = ("exited", "killed", "unknown")


class StepFailed(Exception):
    pass


class Cancelled(Exception):
    pass


class Unresolved(Exception):
    """The story stopped waiting for a run without learning how it ended; its handle is kept."""


def save(path, value):
    temporary = Path(str(path) + ".new")
    temporary.write_text(json.dumps(value, indent=1) + "\n", encoding="utf-8")
    temporary.replace(path)


def load(path, default=None):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return default


def session_module(python_dir):
    """Loom's Python session client, from the installed Loom's `lib/loom/python`."""
    sys.path.insert(0, str(python_dir))
    import loom_session.session as module
    return module


def programs(loom_prefix, zengine_prefix):
    """The files a launch needs, as the installed Loom and Zengine packages lay them out."""
    loom, zen = Path(loom_prefix), Path(zengine_prefix)
    return {"loom_host": str(loom / "bin" / ("loom-host" + EXE)),
            "loom_runs": str(loom / "lib" / "loom" / ("loom-runs" + LIB)),
            "loom_python": str(loom / "lib" / "loom" / "python"),
            "vocabulary": str(zen / "lib" / "zengine" / ("zengine-guest-vocabulary" + LIB))}


# ---- custody: the processes a root starts, known by more than a number -----------------------------
#
# A process id is handed out again once its process has ended, so a root keeps each process it
# starts as {pid, started, exe}: the start time the operating system gives that process (Windows'
# creation time, Linux's start time since boot) and the program it runs, read right after the
# start. `process_state` answers "ended" (no process has that id, or the one that has it started
# at another time, or it is a zombie), "running" (this very process), or "unknown" (it cannot be
# told here). Only a "running" process is ever terminated, and an ending is reported only once it
# is seen. This is the story's own custody of the two processes it starts, not a process manager.
if NT:
    import ctypes
    from ctypes import wintypes

    _K32 = ctypes.WinDLL("kernel32", use_last_error=True)
    _K32.OpenProcess.restype = wintypes.HANDLE
    _K32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    _K32.GetProcessTimes.argtypes = (wintypes.HANDLE,) + (ctypes.POINTER(wintypes.FILETIME),) * 4
    _K32.QueryFullProcessImageNameW.argtypes = (wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                                                ctypes.POINTER(wintypes.DWORD))
    _K32.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
    _K32.WaitForSingleObject.restype = wintypes.DWORD
    _K32.TerminateProcess.argtypes = (wintypes.HANDLE, wintypes.UINT)
    _K32.CloseHandle.argtypes = (wintypes.HANDLE,)
    _QUERY, _SYNC, _TERMINATE, _WAIT_TIMEOUT = 0x1000, 0x00100000, 0x0001, 0x102

    def _identity_of_handle(h):
        times = [wintypes.FILETIME() for _ in range(4)]
        if not _K32.GetProcessTimes(h, *[ctypes.byref(t) for t in times]):
            return {"unknown": "Windows would not say when pid's process started (error %d)"
                    % ctypes.get_last_error()}
        size = wintypes.DWORD(32768)
        name = ctypes.create_unicode_buffer(32768)
        exe = name.value if _K32.QueryFullProcessImageNameW(h, 0, name, ctypes.byref(size)) else ""
        started = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        return {"started": str(started), "exe": exe,
                "alive": _K32.WaitForSingleObject(h, 0) == _WAIT_TIMEOUT}

    def identity_of(pid):
        h = _K32.OpenProcess(_QUERY | _SYNC, False, int(pid))
        if not h:
            error = ctypes.get_last_error()
            return None if error == 87 else {"unknown": "Windows would not open pid %s (error %d)"
                                                        % (pid, error)}
        try:
            return _identity_of_handle(h)
        finally:
            _K32.CloseHandle(h)
else:
    def identity_of(pid):
        try:
            stat = Path("/proc/%d/stat" % int(pid)).read_text()
        except FileNotFoundError:
            return None if Path("/proc/self/stat").exists() else {
                "unknown": "this system has no /proc to say which process pid %s is" % pid}
        except OSError as error:
            return {"unknown": "/proc/%s/stat could not be read: %s" % (pid, error)}
        fields = stat[stat.rindex(")") + 2:].split()
        try:
            exe = os.readlink("/proc/%d/exe" % int(pid))
        except OSError:
            exe = ""
        return {"started": fields[19], "exe": exe, "alive": fields[0] not in ("Z", "X")}


def custody(pid):
    """What a root keeps of a process it has just started."""
    who = identity_of(pid) or {"unknown": "pid %s ended as soon as it started" % pid}
    return dict({"pid": pid}, **dict((k, who[k]) for k in ("started", "exe", "unknown") if k in who))


def process_state(kept):
    """("ended" | "running" | "unknown", words) for a process a root kept."""
    pid = kept["pid"]
    now = identity_of(pid)
    if now is None:
        return "ended", "no process has id %s now" % pid
    if "unknown" in now:
        return "unknown", now["unknown"]
    if not now["alive"]:
        # Whatever process holds the id, it has ended -- and while it holds it, no other can.
        return "ended", "pid %s has ended" % pid
    if not kept.get("started"):
        return "unknown", ("a process with id %s runs (%s); this root kept no start time to tell "
                           "whether it is the one it started" % (pid, now["exe"] or "program unknown"))
    if now["started"] != kept["started"]:
        return "ended", "pid %s is another process now (started %s, not %s)" % (pid, now["started"],
                                                                               kept["started"])
    return "running", "pid %s is running (%s)" % (pid, now["exe"] or "program unknown")


def seen_ending(kept, seconds):
    """Wait up to `seconds` for a kept process to be seen ended."""
    end = time.monotonic() + seconds
    while True:
        state, why = process_state(kept)
        if state == "ended" or time.monotonic() >= end:
            return state, why
        time.sleep(0.2)


def terminate(kept):
    """Ask the operating system to end exactly the process `kept` names: (asked, words)."""
    pid = kept["pid"]
    if NT:
        h = _K32.OpenProcess(_QUERY | _SYNC | _TERMINATE, False, int(pid))
        if not h:
            return False, "Windows would not open pid %s to end it (error %d)" % (pid, ctypes.get_last_error())
        try:
            # THE SAME PROCESS, checked on the handle that will end it: a handle names one process.
            who = _identity_of_handle(h)
            if who.get("started") != kept["started"]:
                return False, "pid %s is not the process this root started; it was not touched" % pid
            if not _K32.TerminateProcess(h, 1):
                return False, "TerminateProcess refused pid %s (error %d)" % (pid, ctypes.get_last_error())
            return True, "TerminateProcess ended pid %s" % pid
        finally:
            _K32.CloseHandle(h)
    pidfd = None
    try:
        pidfd = os.pidfd_open(pid)  # pins this very process between the check and the signal
    except (AttributeError, OSError):
        pidfd = None
    try:
        if process_state(kept)[0] != "running":
            return False, "pid %s is not the process this root started; it was not touched" % pid
        if pidfd is not None:
            signal.pidfd_send_signal(pidfd, signal.SIGKILL)
        else:
            os.kill(pid, signal.SIGKILL)
        return True, "SIGKILL sent to pid %s" % pid
    except OSError as error:
        return False, "pid %s could not be signalled: %s" % (pid, error)
    finally:
        if pidfd is not None:
            os.close(pidfd)


def end_process(kept, seconds=20.0):
    """End a process this root started, if it still runs: (ended, words). Ended is True only
    once the ending is SEEN; a process whose identity cannot be confirmed is never touched."""
    state, why = process_state(kept)
    if state == "ended":
        return True, "had already ended (%s)" % why
    if state == "unknown":
        return False, ("not ended: %s, so it was not touched -- confirm the process is this root's "
                       "and end it yourself" % why)
    asked, said = terminate(kept)
    if not asked:
        return False, "not ended: " + said
    state, why = seen_ending(kept, seconds)
    if state == "ended":
        return True, said + "; seen ended"
    return False, "%s, but it was not seen to end within %gs (%s)" % (said, seconds, why)


def kept_process(record, which):
    """The custody a root's record keeps of its Workshop or host; an older record kept only an id."""
    kept = record.get(which + "_process")
    return kept if kept else {"pid": record[which + "_pid"]}


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
        self.loom = session_module(self.record.get("loom_python") or
                                   programs(self.record["loom_prefix"], ".")["loom_python"])
        self.Session = self.loom.Session
        self.lost = (self.loom.SessionGone, self.loom.Disconnected, self.loom.NotAnswered, OSError)
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
    def held(self):
        """The run this root still holds unresolved, as story-status.json says: {name, lifetime}."""
        return (load(self.status_path, {}) or {}).get("current_run") or None

    def look_up(self, held):
        """What the run manager says NOW of a held run: (run or None, words). A session that does
        not answer is not an ending: the run's last saved record is read from disk and said to be
        only that."""
        try:
            with self.Session.attach(self.record["session"]) as s:
                if s.lifetime != held["lifetime"]:
                    return None, ("the ELH session is lifetime %s now, not %s: run %s belonged to a "
                                  "host that has ended" % (s.lifetime, held["lifetime"], held["name"]))
                try:
                    r = s.run(held["name"], lifetime=held["lifetime"])
                except self.loom.Refused as error:
                    if "no run named" not in str(error):
                        raise
                    # THE MANAGER'S OWN ANSWER, not silence: nothing of that name ever ran here.
                    return ({"state": "absent", "process": "exited", "summary": "", "failure": str(error)},
                            "the run manager holds no run named %s in this lifetime: it never started"
                            % held["name"])
                return r, "run %s is %s, process %s (step %r)" % (held["name"], r["state"],
                                                                   r["process"], r.get("step", ""))
        except self.lost + (self.loom.Refused,) as error:
            saved = load(Path(held.get("directory", "")) / "run.json") if held.get("directory") else None
            fields = (saved or {}).get("fields", {})
            return None, ("the ELH session does not answer (%s); run %s's last record on disk says %s, "
                          "process %s -- a saved record, not the run's live state"
                          % (str(error).splitlines()[0], held["name"], fields.get("state", "unknown"),
                             fields.get("process", "unknown")))

    def settle(self, held, r):
        """Write down a held run's ending once it is seen, and let go of its handle -- read afresh,
        and only while the file still holds that run: a replay that waited on it may have written
        its own ending in the meantime, and that record stands. Returns whether it wrote."""
        self.status = load(self.status_path, {}) or {}
        if (self.status.get("current_run") or {}).get("name") != held["name"]:
            return False
        for entry in self.status.get("runs", []):
            if entry["name"] == held["name"]:
                entry.update(state=r["state"], summary=r.get("summary", ""), failure=r.get("failure", ""),
                             settled_after_wait=True)
        self.note(current_run=None, state="settled: " + r["state"])
        return True

    def guard(self):
        """Nothing new starts while an earlier run is unresolved: Workshop's one input session is
        probably still its, and a new run would lose the only handle to it."""
        held = self.held()
        if not held:
            return
        r, words = self.look_up(held)
        if r is not None and r["state"] in SETTLED and r["process"] in ENDED:
            self.settle(held, r)
            return
        raise StepFailed("run %s is still unresolved (%s): `story.py status` follows it and `story.py "
                         "cancel` asks the manager to cancel it; nothing new was started"
                         % (held["name"], words))

    def run(self, tool, label, inputs, wait=600.0):
        """One run of a maintained tool; returns its record, or raises naming the run. When the
        wait runs out, or the session stops answering, the run is Unresolved: its handle stays."""
        self.guard()
        name = "%02d-%s" % (self.index, label)
        runs = self.status.setdefault("runs", [])
        name += "" if name not in [r["name"] for r in runs] else "-%d" % len(runs)
        if tool not in ("workshop/verify-recipe", "workshop/source", "workshop/lane"):
            inputs = dict({"link": "workshop"}, **inputs)  # local-file tools take no link
        started = time.monotonic()
        held = {"name": name, "tool": tool, "lifetime": self.record.get("lifetime"), "directory": ""}
        rec, sent = None, False
        try:
            with self.Session.attach(self.record["session"]) as s:
                held["lifetime"] = s.lifetime
                # HELD BEFORE IT STARTS: a start whose answer is lost is still found by this name.
                # The story's own state is the replay's to write: a run that stop, play or check
                # makes is not the story running.
                self.note(current_run=held)
                typed = dict((k, json.dumps(v) if isinstance(v, (list, dict)) else v) for k, v in inputs.items())
                sent = True
                try:
                    s.start(tool, name, typed)
                except self.loom.Refused as error:
                    if "release finished ones first" not in str(error):
                        self.note(current_run=None)  # refused before it started: nothing to hold
                        raise StepFailed("run %s was refused before it started: %s" % (name, error))
                    # The run manager holds 64 runs a lifetime and REFUSED this start: nothing ran.
                    # Release finished records (their directories stay: the evidence), start again.
                    for r in s.runs().get("rows", []):
                        if r.get("process") in ENDED and r.get("state") in FINAL:
                            s.release(r["name"])
                    s.start(tool, name, typed)
                held["directory"] = s.run(name).get("directory", "")
                self.note(current_run=held)
                try:
                    rec = s.wait(name, timeout=wait)
                except self.loom.NotAnswered as error:  # this client stopped waiting; the run did not
                    self.unresolved(held, s.run(name), "the story's wait of %gs ran out" % wait,
                                    started, runs)
        except self.lost as error:
            if not sent:  # nothing was asked of the run manager: nothing to hold
                self.note(current_run=None)
                raise StepFailed("run %s was not started: the ELH session does not answer (%s)"
                                 % (name, str(error).splitlines()[0]))
            self.unresolved(held, {}, "the ELH session stopped answering (%s)"
                            % str(error).splitlines()[0], started, runs)
        entry = {"name": name, "tool": tool, "state": rec.get("state"), "summary": rec.get("summary", ""),
                 "failure": rec.get("failure", ""), "seconds": round(time.monotonic() - started, 2),
                 "asks": len(rec.get("asks", [])) + int(rec.get("asks_dropped", 0)),
                 "directory": rec.get("directory", "")}
        runs.append(entry)
        self.note(current_run=None)
        if rec.get("state") == "cancelled":
            raise Cancelled("run %s was cancelled" % name)
        if rec.get("state") != "passed":
            raise StepFailed("run %s %s: %s" % (name, rec.get("state"), (rec.get("failure") or "").strip()))
        return rec

    def unresolved(self, held, r, why, started, runs):
        runs.append({"name": held["name"], "tool": held["tool"], "state": "unresolved",
                     "run_state": r.get("state", "unknown"), "process": r.get("process", "unknown"),
                     "failure": why, "seconds": round(time.monotonic() - started, 2),
                     "directory": held.get("directory", "")})
        self.note(current_run=held, state="unresolved", why=why)
        raise Unresolved("run %s (lifetime %s) is %s, process %s: %s. It is still the run manager's; "
                         "`story.py status` reads it again and `story.py cancel` asks the manager to "
                         "cancel it" % (held["name"], held["lifetime"], r.get("state", "unknown"),
                                        r.get("process", "unknown"), why))

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
        """One Builder act; returns (run record, builder.json) -- the build and the realization
        outcomes are two entries of the second, never one word."""
        rec = self.run("workshop/builder", label, dict({"act": act, "recipe": "tower-defense"}, **more))
        return rec, json.loads(self.artifact(rec, "builder.json") or "{}")

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
FILES = ["zengine.files", "project-files"]


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
                                 {"rows": FILES, "as": "files"},
                                 {"rows": ["zengine.builder-pane", "builder"], "as": "builder"}])


def s_m1(st):
    """Milestone 1, the first picture: create td.cpp in the empty game directory through Neovim."""
    return st.edit("m1-first-picture", st.game + "/td.cpp", edits_of("m1-first-picture"))


def s_recipe(st):
    """Author its recipe in Files: pick buildable (a), name, stem, installed prefixes, links."""
    r = st.record
    st.act("recipe", [
        {"into": FILES + ["Files"]}, {"press": "r"},
        {"expect": FILES + ["td.cpp"], "seconds": 5}, {"press": "a"},
        # The candidate is chosen by its name, whatever else the directory holds.
        {"select": FILES + ["td.cpp"]}, {"press": "enter"},
        {"expect": FILES + ["recipe name>"], "seconds": 5},
        {"press": "ctrl+a"}, {"press": "backspace"}, {"type": "tower-defense"}, {"press": "enter"},
        {"expect": FILES + ["artifact stem> tower-defense"], "seconds": 5},
        {"press": "enter"},
        {"expect": FILES + ["package prefix (comma-separated)>"], "seconds": 5},
        {"type": Path(r["zengine_prefix"]).as_posix() + "," + Path(r["loom_prefix"]).as_posix()},
        {"press": "enter"},
        {"expect": FILES + ["link targets (comma-separated)>"], "seconds": 5},
        {"type": "zengine::pane,zengine::activation,zengine::input,zengine::timer,loom::switchboard"},
        {"press": "enter"},
        {"expect": FILES + ["authored recipe `tower-defense`"], "seconds": 5}])
    return st.run("workshop/verify-recipe", "recipe-check",
                  {"project": st.game, "recipe": "tower-defense", "artifact": "tower-defense"})


def s_load(st):
    """Put the artifact into the project's load plan with its role (Builder o)."""
    rec, said = st.builder("load-it", "load-it", role="td.game", seconds=30)
    outcome = (said.get("realization") or {}).get("outcome")
    if outcome not in ("pending", "resolved"):
        raise StepFailed("the plan row for tower-defense was written but its load is %r: %s"
                         % (outcome, said.get("confirmation")))
    return rec


CATALOG_TAKEN = "build recipes: build-recipes.json (2 recipes) in "


def use_recipes(st, label):
    """Make the game's catalog current again: Files r, the cursor walked to build-recipes.json by
    its name -- whatever sorts before it -- and u. Then the owners' answers, read back: Files says
    which file it took and how many recipes it holds before where it is, so a room too narrow for
    a long root cuts the directory and not the catalog's name -- and it says so only as the answer
    to this press (`r` and the walk spent any sentence before it). What it seats of the directory
    must be the game's own, and the Builder must list the recipe from the catalog now in force."""
    rec = st.act(label, [
        {"into": FILES + ["Files"]}, {"press": "r"},
        {"expect": FILES + ["build-recipes.json"], "seconds": 5},
        {"select": FILES + ["build-recipes.json"]},
        {"press": "u"},
        {"expect": FILES + [CATALOG_TAKEN], "seconds": 5},
        {"expect": ["zengine.builder-pane", "builder", "recipe   tower-defense -> tower-defense"],
         "seconds": 5}])
    said = [row for s in json.loads(st.artifact(rec, "steps.json") or "[]")
            if s["verb"] == "expect" and s["args"][2] == CATALOG_TAKEN for row in s.get("matched", [])]
    where = said[0][said[0].index(CATALOG_TAKEN) + len(CATALOG_TAKEN):] if said else ""
    cut = where.endswith("...")
    seated, game = (where[:-3] if cut else where), st.game
    if NT:
        seated, game = seated.casefold(), game.casefold()
    if not said or not (game.startswith(seated) if cut else game == seated):
        raise StepFailed("Files took a catalog named build-recipes.json, but not in the game directory "
                         "%s: it said %r" % (st.game, said[0] if said else None))
    return rec


def s_first_build(st):
    """Build what the project waits on (Builder f). A recipe from Files borrows no toolchain; where
    CMake's own default cannot build for this Workshop, read the failure, name the toolchain the
    Workshop was built with (and a fresh build workspace) in the recipe through Neovim, and build
    again. Where the default works, the first build is the only one."""
    first, said = st.builder("first-build", "frontier", expect="any")
    if said["build"]["outcome"] == "succeeded":
        if said["realization"].get("outcome") != "realized":
            raise StepFailed("the first build succeeded and its load was %r: %s" % (
                said["realization"].get("outcome"), said["realization"].get("detail")))
        return first
    output = st.artifact(first, "output.txt")
    if "CMAKE_CXX_COMPILER" not in output and "Does not match the generator" not in output:
        raise StepFailed("the first build failed for a reason this story does not repair:\n" + output[-800:])
    read = st.run("workshop/source", "recipe-read", {"root": st.game, "op": "read",
                                                    "path": "build-recipes.json"})
    # THE RECIPE'S OWN LINE, found by what it says: `source` numbers each line (`%5d  text`).
    rows = [m.group(1) for m in re.finditer(r"^ *\d+  (.*)$", st.artifact(read, "result.txt"), re.M)
            if '"recipe":"tower-defense"' in m.group(1)]
    if len(rows) != 1:
        raise StepFailed("build-recipes.json holds %d lines naming recipe tower-defense" % len(rows))
    line = rows[0]
    toolchain = Path(st.record["build"]).as_posix()
    workspace = (Path(st.record["root"]) / "game-build").as_posix()
    fixed = line.replace('"toolchain_from":""', '"toolchain_from":"%s"' % toolchain).replace(
        '"workspace":""', '"workspace":"%s"' % workspace)
    if fixed == line:
        raise StepFailed("the recipe already names a toolchain and a workspace; read the build output")
    st.edit("recipe-toolchain", st.game + "/build-recipes.json",
            [{"replace": '"recipe":"tower-defense"', "text": fixed}], to_unix=True)
    use_recipes(st, "use-recipes")
    return st.builder("second-build", "frontier", realize="realized")[0]


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
    return st.builder("arm", "arm", seconds=20)[0]


def milestone(st, folder):
    st.edit(folder, st.game + "/td.cpp", edits_of(folder))
    rec, said = st.builder(folder + "-build", "build", realize="realized")
    if "reloaded in place" not in said["realization"].get("detail", ""):
        raise StepFailed("%s built and realized, but not in place: %s" % (folder, said["realization"]))
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
    return st.builder("keep", "promote", seconds=30)[0]


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


def launch(runtime, game, wdir, sdir, tools, env, viewport, plan, extra=()):
    """Workshop from `runtime` with `game` as its project and a window of `viewport` cells, then a
    Loom session in `sdir` linked to it as a guest. `tools` names the Loom host, run manager,
    Python session runtime and Zengine guest vocabulary (`programs`). Returns what a record keeps
    of the two, their custody included. A launch that fails ends what it started."""
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
    started, kept = [], {}
    try:
        workshop = subprocess.Popen(wargs, cwd=str(game), stdout=(wdir / "process.log").open("wb"),
                                    stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, env=env,
                                    creationflags=flags)
        started.append(("Workshop", workshop))
        kept["workshop_process"] = custody(workshop.pid)
        port = wait_for(lambda: (wdir / "guests.port").read_text().strip(), 90, "Workshop's guest port")
        save(sdir / "loom-boot.json", {
            "boot": [{"name": "runs", "path": tools["loom_runs"], "role": "loom.runs"},
                     {"name": "vocab", "path": tools["vocabulary"]}],
            "links": [{"name": "workshop", "connect": "127.0.0.1:" + port, "identity": "td-story",
                       "credential": credential}],
            "history": {"log": "session.log", "recent": "8192", "payload_budget": "134217728",
                        "keep": [{"shape": "InputInjected"}, {"shape": "SurfaceCaptured"},
                                 {"shape": "loom.link.Outcome"}, {"shape": "InventoryToolboxFinished"}]}})
        save(sdir / "loom-tools.json", {"python": sys.executable, "runtime": tools["loom_python"],
                                        "packages": [{"path": str(PACKAGE), "approve": "any-revision"}]})
        approvals = ["authority trust runs --rebuilds", "authority trust vocab --rebuilds"]
        approvals += ["authority allow runs %s v1 -> role loom.session" % s
                      for s in ("loom.session.ExpectRun", "loom.session.ForgetRun")]
        approvals += ["authority allow runs loom.runs.%s v1 -> any target" % s
                      for s in ("Tools", "ToolDescription", "Run", "RunList", "Directive")]
        approvals += ["authority allow runs loom.link.%s v1 -> role loom.link.workshop" % s
                      for s in ("Ask", "StatusRequested")]
        approvals += ["start vocab %s" % tools["vocabulary"], "start runs %s loom.runs" % tools["loom_runs"]]
        host = subprocess.Popen([tools["loom_host"], "--serve", str(sdir)], cwd=str(sdir),
                                stdout=(sdir / "process.log").open("wb"), stderr=subprocess.STDOUT,
                                stdin=subprocess.PIPE, env=env, creationflags=flags)
        started.append(("the Loom host", host))
        kept["host_process"] = custody(host.pid)
        host.stdin.write(("\n".join(approvals) + "\n").encode())
        host.stdin.close()
        Session = session_module(tools["loom_python"]).Session

        def admitted():
            with Session.attach(str(sdir)) as s:
                s.tools()
                return any(r["name"] == "workshop" and r["state"] == "admitted" for r in s.describe()["links"])

        wait_for(admitted, 90, "the ELH's link to Workshop")
        with Session.attach(str(sdir)) as s:
            lifetime = s.lifetime
    except BaseException as why:
        # THIS PROCESS STILL HOLDS WHAT IT STARTED, so it ends them itself and waits to see it.
        said = []
        for label, p in reversed(started):
            if p.poll() is None:
                p.kill()
            try:
                said.append("%s pid %d ended (exit %s)" % (label, p.pid, p.wait(timeout=30)))
            except subprocess.TimeoutExpired:
                said.append("%s pid %d was killed and has NOT been seen to end" % (label, p.pid))
        raise SystemExit("%s -- the launch ended what it had started: %s"
                         % (str(why).strip() or type(why).__name__, "; ".join(said) or "nothing"))
    return dict({"workshop_pid": workshop.pid, "host_pid": host.pid, "endpoint": "127.0.0.1:" + port,
                 "lifetime": lifetime, "powers": powers, "viewport": viewport,
                 "load_plan": plan or "the project's"}, **kept)


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
    tools = programs(loom, zprefix)
    record = {"root": str(root), "runtime": str(runtime), "game": str(game), "session": str(sdir),
              "build": str(build), "loom_prefix": str(loom), "zengine_prefix": str(zprefix),
              "loom_python": tools["loom_python"], "toolchain_bin": native(args.toolchain_bin)}
    record.update(launch(runtime, game, wdir, sdir, tools, env, args.viewport, "graphical-load-plan.json"))
    record.update(started_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                  start_seconds=round(time.monotonic() - t0, 2))
    save(root / "story.json", record)
    print(json.dumps(record, indent=1))


def replay(args):
    st = Story(native(args.root))
    st.guard()
    save(st.control_path, {"cancel": False, "paused": bool(args.paused), "steps": 0, "speed": args.speed})
    st.speed = args.speed
    t0 = time.monotonic()
    st.status = {"state": "running", "speed": args.speed, "runs": [], "steps": [],
                 "replay": custody(os.getpid()),
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
        st.note(state="cancelled", why=str(why), replay=None)
        print("CANCELLED: %s -- a cancelled replay does not resume: `story.py reset`, start, replay" % why)
        return 3
    except Unresolved as why:
        st.note(replay=None)
        print("UNRESOLVED at step %d (%s): %s -- the replay stops here and does not resume" % (st.index, name, why))
        return 4
    except StepFailed as why:
        st.note(state="failed", why=str(why), replay=None)
        print("FAILED at step %d (%s): %s" % (st.index, name, why))
        return 1
    runs = st.status["runs"]
    st.note(state="done", step=len(STEPS), seconds=round(time.monotonic() - t0, 1), runs_total=len(runs),
            asks_total=sum(r.get("asks", 0) for r in runs), replay=None)
    print("DONE in %.1fs: %d ELH runs, %d asks recorded by the run manager" % (
        time.monotonic() - t0, len(runs), sum(r.get("asks", 0) for r in runs)))
    return 0


def replay_running(st):
    """Whether a replay process still writes this root's status (it keeps its own custody there)."""
    kept = (load(st.status_path, {}) or {}).get("replay")
    return bool(kept) and process_state(kept)[0] == "running"


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
    print(json.dumps(c))
    if args.command != "cancel":
        return 0
    held = st.held()
    if not held:
        print("no run is in progress: the replay stops before its next step")
        return 0
    r, words = st.look_up(held)
    if r is not None and r["state"] == "absent":
        print(words + "; its handle is let go")
        if not replay_running(st):
            st.settle(held, r)
        return 0
    try:
        with st.Session.attach(st.record["session"]) as s:
            asked = s.cancel(held["name"], "story.py cancel", lifetime=held["lifetime"])
            print("asked the run manager to cancel %s (lifetime %s): cancellation %s"
                  % (held["name"], held["lifetime"], "requested" if asked.get("cancel_requested")
                     else "not requested -- " + json.dumps(asked)))
            # REQUESTED IS NOT DONE: the run is followed until its manager says how it ended (a
            # cancelled run's cleanup has 30 s of its own).
            end = time.monotonic() + 40
            while True:
                r = s.run(held["name"], lifetime=held["lifetime"])
                if r["state"] in FINAL and r["process"] in ENDED:
                    break
                if time.monotonic() >= end:
                    print("run %s is still %s, process %s, 40 s after the request: not yet ended -- "
                          "`story.py status` follows it" % (held["name"], r["state"], r["process"]))
                    return 2
                time.sleep(0.2)
    except st.lost + (st.loom.Refused,) as error:
        print("could not reach the run manager to cancel %s (%s): the run is not known to have "
              "stopped, and its handle is kept" % (held["name"], str(error).splitlines()[0]))
        return 1
    cleanup = [n for n in r.get("notes", []) if n.startswith("cleanup")]
    print("run %s ended %s, process %s%s" % (held["name"], r["state"], r["process"],
                                              ("; " + "; ".join(cleanup)) if cleanup else ""))
    if not replay_running(st) and not st.settle(held, r):
        print("the replay that waited on it had already written its ending")
    return 0


def status(args):
    st = Story(native(args.root))
    s = load(st.status_path, {}) or {}
    out = {"state": s.get("state"), "step": s.get("step"), "name": s.get("name"), "title": s.get("title"),
           "current_run": s.get("current_run"), "runs": len(s.get("runs", [])), "control": st.control(),
           "recorded_lifetime": st.record["lifetime"], "replay_running": replay_running(st),
           "stopped": st.record.get("stopped"), "stopped_utc": st.record.get("stopped_utc")}
    held = s.get("current_run")
    if held:
        r, words = st.look_up(held)
        out["current_run_now"] = words
        if r is not None and r["state"] in SETTLED and r["process"] in ENDED and not replay_running(st):
            out["current_run_now"] += (" -- settled, and written down" if st.settle(held, r) else
                                       " -- settled; the replay had written its ending")
    try:
        with st.Session.attach(st.record["session"]) as session:
            out["links"] = [(r["name"], r["state"]) for r in session.describe()["links"]]
            out["lifetime"] = session.lifetime
    except st.lost as error:
        out["links"] = "the ELH session does not answer: %s" % str(error).splitlines()[0]
    for which in ("workshop", "host"):
        out[which + "_process"] = "%s: %s" % process_state(kept_process(st.record, which))
    print(json.dumps(out, indent=1))


def one(args):
    st = Story(native(args.root))
    if replay_running(st):
        raise SystemExit("refusing: a replay still runs in %s, and `%s` would share Workshop's input with "
                         "it and write over its status -- cancel it, or let it end" % (st.root, args.command))
    st.speed = args.speed
    st.index = 90 if args.command == "play" else 91
    st.status.setdefault("runs", [])
    try:
        rec = play(st, "play-again") if args.command == "play" else check(st, "check-again")
    except Unresolved as why:
        print("UNRESOLVED: %s" % why)
        return 4
    except (StepFailed, Cancelled) as why:
        print("%s FAILED: %s" % (args.command, why))
        return 1
    print(rec.get("summary"))


def stop_processes(st, force, discard=False):
    """Stop a story's Workshop, then its ELH, and say what was SEEN of each. In order: (1) ask
    Workshop to quit as a maker would -- a pane put down with Escape, then the desk's q -- through
    the ELH; (2) believe Workshop gone only when its process is seen to have ended; (3) only then
    end the ELH session, and believe that only when the Loom host is seen to have ended. `force`
    ends a process that will not, once its identity is confirmed. Workshop will not quit over
    Neovim's unsaved work, which a cancelled edit leaves behind: `discard` first abandons all of
    it in Neovim (Escape, :qa!, which ends Neovim), as Workshop's own notice allows. A Workshop that still
    runs keeps its ELH, so the route that can reach it stays open. While a replay still runs in the
    root, or a run is unresolved, nothing is asked unless `force`. Returns {"stopped", "workshop",
    "host", "notes"}."""
    st.index = 99
    kept_w, kept_h = kept_process(st.record, "workshop"), kept_process(st.record, "host")
    seen = {"stopped": False, "workshop": None, "host": None, "notes": []}
    note = seen["notes"].append
    replay = (load(st.status_path, {}) or {}).get("replay")
    if replay and process_state(replay)[0] == "running" and not force:
        # A REPLAY STILL RUNNING here would take the quit for a failure of its own next step, and
        # both would write this root's status: it is stopped first, by cancel.
        note("a replay still runs in this root (pid %s): `story.py cancel` stops it before its next "
             "step; stop once it has ended, or stop with --force" % replay["pid"])
        return seen

    def link_closed(s, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if all(r["name"] != "workshop" or r["state"] != "admitted" for r in s.describe()["links"]):
                return True
            time.sleep(0.3)
        return False

    state, why = process_state(kept_w)
    if state == "ended":
        seen["workshop"] = "had ended (%s)" % why
    else:
        # THE MAKER'S QUIT needs no identity: the link reaches the Workshop it was admitted to. Only
        # the ending is judged by the process, and only a confirmed identity is ever forced.
        held = st.held()
        if held:
            r, words = st.look_up(held)
            if (r is None or r["state"] not in SETTLED) and not force:
                note("run %s is unresolved and may hold Workshop's input (%s): `story.py cancel` it "
                     "first, or stop with --force" % (held["name"], words))
                return seen
        try:
            with st.Session.attach(st.record["session"]) as s:
                gone = link_closed(s, 0.1)
                if discard and not gone:
                    # DISCARD MEANS ALL OF IT: `:qa!` is Neovim's own way to abandon every unsaved
                    # buffer -- a hidden one included, which `:e!` on the current one would leave
                    # holding Workshop's quit -- and it ends Neovim, which the quit would anyway.
                    try:
                        st.act("discard", [{"into": ["zengine.editor", "editor"], "seconds": 2},
                                           {"press": "escape"}, {"type": ":qa!\n"},
                                           {"absent": ["zengine.editor", "editor", "UNSAVED"], "seconds": 10}],
                               wait=60)
                    except (StepFailed, Cancelled, Unresolved) as err:
                        note("discard: %s" % str(err).splitlines()[0])
                for n, (provider, pane, text) in enumerate([("zengine.terminal", "terminal", "TERMINAL"),
                                                            ("td.game", "td", "TOWER DEFENSE"),
                                                            ("ctrl+t", "terminal", "TERMINAL"), ("", "", "")]):
                    # Escape reaches the desk from these two: the Terminal's once its line and list
                    # are empty, the game's because it binds no Escape. Files, the Builder, Info and
                    # the Pane Manager keep Escape for themselves; when neither pane is on the desk,
                    # Ctrl+T opens the Terminal with the keys. Last, Escape and q go to whatever
                    # holds the keys. The quit ends the link, so the run that asks it reports the
                    # link lost: the link's closing is the evidence the quit was taken, and the
                    # process's ending is the evidence Workshop is gone.
                    if gone:
                        break
                    into = ([{"press": "ctrl+t"}, {"expect": ["zengine.terminal", pane, text], "seconds": 3}]
                            if provider == "ctrl+t" else
                            [{"into": [provider, pane, text], "seconds": 2}] if provider else [])
                    try:
                        st.act("quit-%d" % n, into + [{"press": "escape"}, {"press": "q"}], wait=60)
                    except (StepFailed, Cancelled, Unresolved) as err:
                        note("quit through %s: %s" % (pane or "the keys' holder", str(err).splitlines()[0]))
                    gone = link_closed(s, 10)
                if gone:
                    state, why = seen_ending(kept_w, 30)
                    if state == "ended":
                        seen["workshop"] = "quit (its link closed and its process was seen to end)"
                    else:
                        note("Workshop's link closed but its process still runs after 30 s (%s)" % why)
                else:
                    note("Workshop did not quit: it keeps its link (read its notice -- Neovim's unsaved "
                         "work is the usual reason; --discard-unsaved discards it)")
        except st.lost as error:
            note("the ELH session does not answer (%s), so Workshop could not be asked to quit"
                 % str(error).splitlines()[0])
        if seen["workshop"] is None and force:
            ended, words = end_process(kept_w)
            if ended:
                seen["workshop"] = "killed (%s)" % words
            else:
                note("--force could not end Workshop: %s" % words)
    if seen["workshop"] is None:
        state, why = process_state(kept_w)
        note("Workshop is not seen ended (%s); the ELH session is not ended, so a route that still "
             "reaches Workshop stays open%s" % (why, "" if force or state != "running" else
                                                "; --force ends Workshop once its identity is confirmed"))
        return seen
    # (3) The ELH, only once Workshop is gone.
    state, why = process_state(kept_h)
    if state == "ended":
        seen["host"] = "had ended (%s)" % why
    else:
        try:
            with st.Session.attach(st.record["session"]) as s:
                s.shutdown("story.py stop")
        except st.lost as error:
            note("the ELH session could not be asked to end (%s)" % str(error).splitlines()[0])
        state, why = seen_ending(kept_h, 20)
        if state == "ended":
            seen["host"] = "shut down (its process was seen to end)"
        elif force:
            ended, words = end_process(kept_h)
            seen["host"] = "killed (%s)" % words if ended else None
            if not ended:
                note("--force could not end the Loom host: %s" % words)
    if seen["host"] is None:
        note("the Loom host is not seen ended (%s)%s" % (process_state(kept_h)[1],
                                                         "" if force else "; --force ends it"))
        return seen
    seen["stopped"] = True
    return seen


def stop(args):
    st = Story(native(args.root))
    seen = stop_processes(st, args.force, args.discard_unsaved)
    for line in seen["notes"]:
        print(line)
    print("Workshop: %s; ELH: %s" % (seen["workshop"] or "NOT stopped", seen["host"] or "NOT stopped"))
    if not seen["stopped"]:
        print("the story in %s is NOT stopped" % st.root)
        return 1
    save(st.root / "story.json", dict(st.record, stopped={"workshop": seen["workshop"], "host": seen["host"]},
                                      stopped_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())))
    print("stopped: ELH lifetime %s ended" % st.record["lifetime"])
    return 0


def reset(args):
    """The story's reset: stop a running story, then retire its root by renaming it. Nothing is
    deleted -- the old root keeps its evidence -- and nothing outside the root is touched. A root
    whose Workshop or Loom host is not seen ended is not retired, whatever the ELH answered."""
    root = Path(native(args.root)).resolve()
    record = load(root / "story.json")
    if not record or Path(record["root"]).resolve() != root:
        raise SystemExit("refusing: %s holds no story.json of its own" % root)
    if stop(argparse.Namespace(root=str(root), force=args.force, discard_unsaved=args.discard_unsaved)):
        raise SystemExit("refusing to retire %s: its processes are not seen ended (above); nothing "
                         "was renamed" % root)
    retired = root.with_name(root.name + ".retired-" + time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()))
    for attempt in range(40):  # a process just ended can hold the directory for a moment
        try:
            root.rename(retired)
            break
        except PermissionError:
            if attempt == 39:
                raise
            time.sleep(0.25)
    print("retired %s -> %s (nothing deleted)" % (root, retired))
    return 0


def hold_until(release):
    """Wait for a line on stdin or for the file `release` to exist, whichever comes first. A
    stdin that is closed (a command started in the background) leaves only the file."""
    said = threading.Event()

    def read():
        try:
            if sys.stdin is not None and sys.stdin.readline():
                said.set()
        except (OSError, ValueError):
            pass

    threading.Thread(target=read, daemon=True).start()
    while not said.is_set() and not Path(release).exists():
        time.sleep(0.5)


def again(args):
    """Launch Workshop again on a stopped story's game, with a new Loom session, and check it.
    In the window, Workshop launches in the game directory with no plan named, so the project's
    own plan loads the image `keep` promoted. With --tui it is the terminal medium: a plan names
    its skin, so the runtime's terminal plan runs in a project of its own with the story's
    recipes, and the Builder's `o` loads the kept game, as a maker would. Either way the game must
    pass its rules check and run a wave under its keys, and the example's toolbox must restore
    beside it; then that Workshop is asked to quit, and ended only if it will not. Its files go to
    <root>/again-N/, and the story's game directory is written by Workshop alone."""
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
    tools = programs(st.record["loom_prefix"], st.record["zengine_prefix"])
    record = launch(Path(st.record["runtime"]), project, wdir, sdir, tools, env, args.viewport, plan, extra)
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
            # THE PLAN ROW IS THE BUILDER'S ANSWER; THE LOAD'S END IS THE GAME'S. A row can be resolved
            # now, loaded by a build now (realized), or still loading -- a load conversation the
            # Builder does not paint the end of. The game pane answering below is that end.
            said = st.builder("load-it", "load-it", role="td.game", seconds=180)[1]
            outcome = (said.get("realization") or {}).get("outcome")
            if outcome not in ("resolved", "realized", "loading"):
                raise StepFailed("the kept game was not loaded: %s (%s)" % (outcome, said.get("confirmation")))
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
        if args.hold:
            # A MAKER'S OWN HAND, which this command cannot be: the checked Workshop stays up, in
            # this command's custody, until a person says so -- then it is stopped as always.
            release = adir / "release"
            print("HOLDING %s: the kept game is loaded and the example's toolbox restored with its "
                  "hotkeys OFF. Press Return here, or create %s, and this Workshop is stopped."
                  % (adir, release), flush=True)
            hold_until(release)
    except (StepFailed, Cancelled, Unresolved) as why:
        verdict = "failed: %s" % why
    finally:
        # A WORKSHOP THIS COMMAND STARTED is asked to quit, and ended -- once its identity is
        # confirmed -- only if it will not; whatever happens is written down as it was seen.
        seen = stop_processes(st, force=True)
    record.update(verdict=verdict, stopped=seen, runs=st.status["runs"], seconds=round(time.monotonic() - t0, 1))
    save(adir / "again.json", record)
    for line in seen["notes"]:
        print(line)
    print("again (%s): %s -- Workshop: %s; ELH: %s -- %s" % (record["medium"], verdict, seen["workshop"] or "NOT stopped",
                                                           seen["host"] or "NOT stopped", adir))
    return 0 if verdict == "passed" and seen["stopped"] else 1


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
    p.add_argument("--force", action="store_true",
                   help="stop, reset: go ahead while a replay runs or a run is unresolved, and end a Workshop "
                        "or Loom host that will not stop, once its identity is confirmed")
    p.add_argument("--discard-unsaved", action="store_true",
                   help="stop, reset: first abandon Neovim's unsaved buffers with :qa! (a cancelled edit leaves one)")
    p.add_argument("--tui", action="store_true", help="again: the terminal medium, not the window")
    p.add_argument("--hold", action="store_true",
                   help="again: once checked, keep that Workshop up for a maker's own hand until Return "
                        "or DIR/again-N/release")
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
