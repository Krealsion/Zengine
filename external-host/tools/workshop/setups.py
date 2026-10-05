# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Setup descriptions: what a named Workshop setup is for and what preparing it involves.

A setup is a directory holding ``setup.json`` (this module reads it), the desk it arranges as
Workshop's own ``WorkshopSetup`` file, a guide, and the assets it names. Describing or loading a
setup reads files and nothing else: it runs no command and confers no authority. The launcher
(``external-host/demo.py``) and the preparation service (``demo_setup.py``) read the same
description. docs/workshop/demo-setups.md is the guide."""
import hashlib
import json
import os
from pathlib import Path
import posixpath
import re
import unicodedata

FORMAT, VERSION = "zengine-setup", "1"
HERE = Path(__file__).resolve().parent
COLLECTION = HERE / "setups"
POWERS = ("input", "capture", "inspect", "inventory", "toolbox", "demo", "open")
MEDIA = ("sdl", "tui")
VIEW_KINDS = ("row", "column", "single")
# The panes whose owners implement PaneResetRequested (workshop/setup_control.hpp).
RESETTABLE = ("zengine.inventory-pane", "zengine.info", "zengine.composer")
CHORD = re.compile(r"^((shift|ctrl|alt)\+)*[a-z0-9]+$")
TEXT_FIELDS = ("title", "summary", "task", "choose")
RECIPES_FILE = "build-recipes.json"  # the launcher writes it into the project
WALK_NAME = re.compile(r"^[a-z0-9][a-z0-9-]*$")
PICTURE_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")  # it becomes a file name
IGNORED = ("__pycache__",)


class SetupError(ValueError):
    """A description that cannot be used, with every reason found."""


def same_name(name):
    """How a filesystem may compare `name`: case, Unicode form and Windows' trailing dots and
    spaces ignored, so two names that could land on one file anywhere count as one."""
    parts = unicodedata.normalize("NFC", name).casefold().split("/")
    return "/".join(p.rstrip(". ") for p in parts)


def inside(child, parent):
    return child.startswith(parent + "/")


def contents(path):
    """A hash of a file's bytes, or of a directory's relative names and bytes (caches ignored)."""
    h = hashlib.sha256()
    if path.is_file():
        h.update(path.read_bytes())
    else:
        for f in sorted(p for p in path.rglob("*") if p.is_file()):
            if set(f.relative_to(path).parts) & set(IGNORED):
                continue
            h.update(f.relative_to(path).as_posix().encode() + b"\0" + f.read_bytes())
    return h.hexdigest()


def project_target_problems(files):
    """Why a project's `files` targets cannot be written inside its project directory."""
    out, seen = [], {}
    for target, source in files.items():
        name = str(target).replace("\\", "/")
        parts = name.split("/")
        if (not name or name.startswith("/") or ":" in name or
                any(p in ("", ".", "..") for p in parts)):
            out.append("project file %r must be a relative path inside the project, without `.` or "
                       "`..` parts" % target)
            continue
        key = same_name(name)
        if key == RECIPES_FILE:
            out.append("project file %r is the launcher's own %s" % (target, RECIPES_FILE))
        for other, first in seen.items():
            if key == other or inside(key, other) or inside(other, key):
                out.append("project files %r and %r would land on one path" % (first, target))
        seen[key] = target
    return out


def walk_problems(d):
    """Why the setup's named walks cannot be replayed. A walk is a list of `workshop/act` steps that
    `demo.py walk` replays against a running root; act spells each step's verb before any Workshop
    contact. The replay decides where pictures go, so a picture is named here and no step names a
    path."""
    walks = d.get("walks", {})
    if not isinstance(walks, dict):
        return ["walks maps each walk's name to {about, steps}"]
    out = []
    for name, walk in walks.items():
        if not WALK_NAME.match(name):
            out.append("walk name %r is lower-case letters, digits and hyphens" % name)
        if not (isinstance(walk, dict) and isinstance(walk.get("about"), str) and walk["about"].strip()):
            out.append("walk %r needs `about`: what it checks" % name)
        steps = walk.get("steps") if isinstance(walk, dict) else None
        if not (isinstance(steps, list) and steps and all(isinstance(s, dict) and s for s in steps)):
            out.append("walk %r needs `steps`: a non-empty list of workshop/act steps" % name)
            continue
        pictures = [s["picture"] for s in steps if "picture" in s]
        bad = [p for p in pictures if not (isinstance(p, str) and PICTURE_NAME.match(p))]
        if bad:
            out.append("walk %r names picture(s) %s; a picture's name is letters, digits, '.', '-' "
                       "and '_'" % (name, bad))
        twice = sorted({p for p in pictures if isinstance(p, str) and pictures.count(p) > 1})
        if twice:
            out.append("walk %r names picture(s) %s twice" % (name, twice))
        if any("save" in s for s in steps):
            out.append("walk %r: a step names no `save` path; the replay says where its pictures go"
                       % name)
    return out


def leaves(name):
    """Whether a relative reference reaches outside the directory it is relative to."""
    normal = posixpath.normpath(str(name).replace("\\", "/"))
    return normal == ".." or normal.startswith("../")


def guide_problems(d):
    """Why `guide_files` cannot travel with the guide. The guide is read from a prepared or exported
    copy, where only what the description declares exists, so the files it shows are declared and
    stay at their own paths inside the setup, beside a guide that does too."""
    shown = d.get("guide_files", [])
    if not isinstance(shown, list) or not all(isinstance(f, str) and f for f in shown):
        return ["guide_files is a list of paths inside the setup's directory"]
    out = ["guide file %r must be inside the setup's directory, where the copy keeps its path" % f
           for f in shown if leaves(f)]
    if shown and (not d.get("guide") or leaves(d["guide"].split("#")[0])):
        out.append("guide_files needs a guide inside the setup's directory")
    return out


def roots(extra=()):
    """Where named setups are found: this package's collection, then each extra directory."""
    return [COLLECTION] + [Path(r) for r in extra]


def find(where, extra=()):
    """The setup directory `where` names: a directory holding setup.json, or a name in a root."""
    path = Path(where)
    if (path / "setup.json").is_file():
        return path.resolve()
    if path.name == str(where) and "/" not in str(where) and "\\" not in str(where):
        for root in roots(extra):
            if (root / where / "setup.json").is_file():
                return (root / where).resolve()
    raise SetupError("no setup named %r (setups are listed by `demo.py list`; a path must be a "
                     "directory holding setup.json)" % str(where))


def collection(extra=()):
    """Every setup in the roots, by name; the first root holding a name wins."""
    found = {}
    for root in roots(extra):
        if root.is_dir():
            for child in sorted(root.iterdir()):
                if (child / "setup.json").is_file() and child.name not in found:
                    found[child.name] = child.resolve()
    return found


def load(where, extra=()):
    return Setup(find(where, extra))


class Setup:
    """One revision of a setup: the description and desk as read when it was loaded, and a hash of
    every declared file then. Preparing from a loaded Setup never mixes in a later edit: the desk
    is not read again, and `unchanged` says whether a file read later is still what was loaded."""
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.name = self.root.name
        try:
            self.data = json.loads((self.root / "setup.json").read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise SetupError("%s/setup.json cannot be read: %s" % (self.root, error))
        try:
            self._desk = self.asset(self.data.get("desk")).read_text(encoding="utf-8")
        except (SetupError, OSError) as error:
            self._desk = error
        problems = self.problems()
        if problems:
            raise SetupError("setup %r is not usable:\n  - %s" % (self.name, "\n  - ".join(problems)))
        self.loaded = [(name, contents(path)) for name, path in self.assets()]

    # ---- the description's parts -------------------------------------------------------------
    def get(self, key, default=None):
        return self.data.get(key, default)

    def asset(self, relative):
        """A named file or directory, relative to this setup. It may be shared with other
        maintained material (`..`), never absolute; `export` gathers it into one directory."""
        if not isinstance(relative, str) or not relative or Path(relative).is_absolute() or ":" in relative:
            raise SetupError("asset %r must be a relative path" % (relative,))
        return (self.root / relative).resolve()

    def assets(self):
        """(relative name as written, resolved path) of every file this description declares."""
        named = [("setup.json", self.root / "setup.json"), (self.data["desk"], self.asset(self.data["desk"]))]
        if self.data.get("guide"):
            named.append((self.data["guide"].split("#")[0], self.asset(self.data["guide"].split("#")[0])))
        for shown in self.data.get("guide_files", []):
            named.append((shown, self.asset(shown)))
        toolbox = self.material().get("toolbox")
        if toolbox:
            named.append((toolbox, self.asset(toolbox)))
        project = self.data.get("project") or {}
        for target, source in sorted((project.get("files") or {}).items()):
            named.append((source, self.asset(source)))
        if project.get("recipes"):
            named.append((project["recipes"], self.asset(project["recipes"])))
        for tool in self.data.get("tools", []):
            named.append((tool, self.asset(tool)))
        return named

    def material(self):
        return self.data.get("material") or {}

    def desk(self, views=()):
        """The WorkshopSetup envelope to apply, as loaded, with each portable view in `views`
        seated in the next declared view slot (Workshop refuses a setup naming a view that does
        not exist)."""
        if isinstance(self._desk, Exception):
            raise self._desk
        desk = json.loads(self._desk)
        rows = desk["fields"]["panes"]
        for slot, view in zip(self.data.get("view_slots", []), views):
            rows.append({"provider": "zengine.inventory-pane", "pane": view,
                         "place": {"mode": "pixels", "x": str(slot["x"] * 12), "y": str(slot["y"] * 12)},
                         "width": {"mode": "pixels", "amount": str(slot["width"] * 12)},
                         "height": {"mode": "pixels", "amount": str(slot["height"] * 12)},
                         "front": str(len(rows))})
        return desk

    def without(self, desk, providers):
        """`desk` less the panes whose provider is one of `providers`."""
        desk = json.loads(json.dumps(desk))
        desk["fields"]["panes"] = [p for p in desk["fields"]["panes"] if p["provider"] not in providers]
        for i, p in enumerate(desk["fields"]["panes"]):
            p["front"] = str(i)
        return desk

    def resets(self):
        """(provider, pane) of every desk pane whose owner resets its own transient state."""
        rows = self.desk()["fields"]["panes"]
        return [(p["provider"], p["pane"]) for p in rows if p["provider"] in RESETTABLE]

    def providers(self):
        return self.data.get("providers", [])

    def hotkeys(self):
        return self.data.get("hotkeys", [])

    def walks(self):
        """The named walks: {name: {about, steps}}, each replayed by `demo.py walk`."""
        return self.data.get("walks", {})

    def views(self):
        """The portable views the hotkeys declare, in order: (view kind, [hotkeys]). Hotkeys naming
        one kind share one view; declared view i sits in view slot i."""
        grouped = {}
        for hk in self.hotkeys():
            grouped.setdefault(hk.get("view", "row"), []).append(hk)
        return list(grouped.items())

    def digest(self):
        """A hash of the description and every declared file as loaded, so a running instance can
        say whether what it prepared is still what the directory describes."""
        h = hashlib.sha256()
        for name, content in self.loaded:
            h.update(name.encode() + b"\0" + content.encode())
        return h.hexdigest()[:16]

    def unchanged(self, name):
        """Whether the declared file `name` still holds what was loaded."""
        loaded = dict(self.loaded)[name]
        path = self.asset(name)
        return path.exists() and contents(path) == loaded

    # ---- validation -------------------------------------------------------------------------------
    def problems(self):
        d, out = self.data, []
        if d.get("format") != FORMAT or d.get("format_version") != VERSION:
            out.append("format must be %r version %r" % (FORMAT, VERSION))
        for key in TEXT_FIELDS:
            if not isinstance(d.get(key), str) or not d[key].strip():
                out.append("%s is required text" % key)
        first = d.get("first_task")
        if not (isinstance(first, dict) and first.get("do") and first.get("expect")):
            out.append("first_task needs `do` and `expect`")
        medium = d.get("medium") or {}
        if not medium.get("supports") or any(m not in MEDIA for m in medium["supports"]):
            out.append("medium.supports lists %s" % " and/or ".join(MEDIA))
        view = medium.get("viewport")
        if not (isinstance(view, list) and len(view) == 2 and all(isinstance(v, int) for v in view)):
            out.append("medium.viewport is [columns, rows]")
        may = (d.get("authority") or {}).get("may", [])
        unknown = [p for p in may if p not in POWERS]
        if unknown:
            out.append("authority.may names unknown power(s) %s" % unknown)
        if "demo" not in may:
            out.append("authority.may must include demo: preparation and Reset use its doors")
        try:
            desk = self.desk()
            if desk.get("schema") != "WorkshopSetup":
                out.append("desk must be a WorkshopSetup file")
            elif not any(p["provider"] == "zengine.demo" for p in desk["fields"]["panes"]):
                out.append("desk must hold the zengine.demo controls pane (the visible Reset)")
        except (SetupError, OSError, ValueError, KeyError) as error:
            out.append("desk: %s" % error)
        material = self.material()
        if set(material) - {"capture", "toolbox"} or len(material) > 1:
            out.append("material is one of {capture: {...}} or {toolbox: <file>}")
        if "toolbox" in material and "toolbox" not in may:
            out.append("a toolbox material needs the toolbox power")
        for i, hk in enumerate(self.hotkeys()):
            if not (hk.get("entry") and hk.get("target") and hk.get("means")):
                out.append("hotkeys[%d] needs entry, target and means" % i)
            if not CHORD.match(str(hk.get("key", ""))):
                out.append("hotkeys[%d].key %r is not a chord such as alt+1" % (i, hk.get("key")))
        keys = [hk.get("key") for hk in self.hotkeys()]
        if len(keys) != len(set(keys)):
            out.append("two declared hotkeys share one chord")
        views = self.views()
        for kind, group in views:
            if kind not in VIEW_KINDS:
                out.append("hotkey view %r is not one of %s" % (kind, ", ".join(VIEW_KINDS)))
            if kind == "single" and len(group) > 1:
                out.append("a single view holds one hotkey; %d name it" % len(group))
        if len(views) > len(d.get("view_slots", [])):
            out.append("the hotkeys declare %d view(s) but view_slots places %d: each declared view "
                       "needs a slot to be on the desk" % (len(views), len(d.get("view_slots", []))))
        for i, p in enumerate(self.providers()):
            if not (p.get("role") and p.get("prepare") in ("build",)):
                out.append("providers[%d] needs a role and prepare: build" % i)
        if self.providers() and not (d.get("project") or {}).get("recipes"):
            out.append("a provider built by this setup needs a project with recipes")
        out += guide_problems(d)
        out += walk_problems(d)
        files = (d.get("project") or {}).get("files") or {}
        out += project_target_problems(files)
        try:
            for name, path in self.assets():
                if not path.exists():
                    out.append("declared asset %s is missing (%s)" % (name, path))
            for target, source in files.items():
                if self.asset(source).is_dir():
                    out.append("project file %r names a directory (%s); name each file" % (target, source))
        except SetupError as error:
            out.append(str(error))
        return out

    # ---- what a reader is shown -----------------------------------------------------------------
    def summary(self):
        """The machine-readable description: the authored fields plus what can be read from the
        directory. Never live identities, credentials or an instance's paths."""
        d = dict(self.data)
        d.update(name=self.name, directory=str(self.root), digest=self.digest(),
                 guide_path=str(self.asset(d["guide"].split("#")[0])) if d.get("guide") else "",
                 panes=[{"provider": p["provider"], "pane": p["pane"]} for p in self.desk()["fields"]["panes"]],
                 resets=[{"provider": p, "pane": k} for p, k in self.resets()],
                 assets=[name for name, _ in self.assets()])
        return d

    def describe(self):
        d = self.data
        lines = ["%s -- %s" % (self.name, d["title"]), "  " + d["summary"], "",
                 "Task:        " + d["task"], "Choose when: " + d["choose"]]
        if d.get("prepares"):
            lines += ["Prepares:"] + ["  - " + p for p in d["prepares"]]
        medium = d["medium"]
        lines.append("Media:       %s at %dx%d cells" % (", ".join(medium["supports"]), *medium["viewport"]))
        requires = d.get("requires") or {}
        needs = ["a built Zengine (--build) and an installed Loom (--loom-prefix)"]
        needs += ["--%s: %s" % (k.replace("_", "-"), v) for k, v in sorted((requires.get("inputs") or {}).items())]
        needs += ["built artifact %s" % a for a in requires.get("artifacts", [])]
        needs += list(requires.get("notes", []))
        lines += ["Requires:"] + ["  - " + n for n in needs]
        authority = d.get("authority") or {}
        lines.append("Authority:   its own guest may %s" % ", ".join(authority.get("may", [])))
        if authority.get("observe"):
            lines.append("             and observe %s" % ", ".join(
                "%s %s v%s" % (o["producer"], o["shape"], o["version"]) for o in authority["observe"]))
        if self.hotkeys():
            lines.append("Hotkeys (turned ON by preparation; Inventory's item and view switches turn them off):")
            for hk in self.hotkeys():
                lines.append("  %-7s %s -> %s: %s" % (hk["key"], hk["entry"], hk["target"], hk["means"]))
        lines += ["First task:  " + d["first_task"]["do"], "Expect:      " + d["first_task"]["expect"]]
        if self.walks():
            lines.append("Walks (demo.py walk NAME --root ROOT --pictures DIR replays one):")
            for name, walk in sorted(self.walks().items()):
                lines.append("  %-12s %s" % (name, walk["about"]))
        reset = d.get("reset") or {}
        if reset.get("scope"):
            lines.append("Reset:       " + reset["scope"])
        if reset.get("keeps"):
            lines.append("Reset keeps: " + reset["keeps"])
        if d.get("guide"):
            lines.append("Guide:       " + str(self.asset(d["guide"].split("#")[0])) +
                         ("#" + d["guide"].split("#", 1)[1] if "#" in d["guide"] else ""))
        return "\n".join(lines)

    def placements(self):
        """Where `export` puts each declared asset: {reference as written: path inside the copy}.
        A reference inside this directory keeps its path; one reaching shared material with `..`
        takes the path after its `..` parts, its first part numbered (`shared-2/code.cpp`) when
        another source already holds that name, or a file holds a directory of it, as a filesystem
        could compare them. One source keeps one place however often
        it is named, and a file inside a declared directory stays inside that directory's copy.
        Nothing is written."""
        def normal(name):
            return posixpath.normpath(name.replace("\\", "/"))

        def within(path, directory):
            try:
                return path != directory and path.relative_to(directory) is not None
            except ValueError:
                return False

        claims = [(same_name("setup.json"), "setup.json", self.root / "setup.json")]

        def clashes(place, source):
            parts = place.split("/")
            for key, other, held in claims:
                if key == same_name(place):
                    if held != source:
                        return True
                elif inside(same_name(place), key):
                    rest = "/".join(parts[len(other.split("/")):])
                    if not (held.is_dir() and (held / rest).resolve() == source):
                        return True
                elif inside(key, same_name(place)):
                    rest = "/".join(other.split("/")[len(parts):])
                    if not (source.is_dir() and (source / rest).resolve() == held):
                        return True
            return False

        named = [(n, p) for n, p in self.assets() if n != "setup.json"]
        placed, by_source = {}, {}
        for name, source in [x for x in named if not leaves(x[0])] + [x for x in named if leaves(x[0])]:
            if source in by_source:
                placed[name] = by_source[source]
                continue
            home = next(((p, s) for _, p, s in claims if s.is_dir() and within(source, s)), None)
            if home:
                first = home[0] + "/" + source.relative_to(home[1]).as_posix()
            else:
                first = "/".join(x for x in normal(name).split("/") if x not in ("..", ".")) or source.name
            place, number = first, 1
            while clashes(place, source):
                number += 1
                if number > 999:
                    raise SetupError("no place in an export for %s" % name)
                top, _, rest = first.partition("/")
                stem, ext = posixpath.splitext(top) if not rest else (top, "")
                place = "%s-%d%s" % (stem, number, ext) + ("/" + rest if rest else "")
            claims.append((same_name(place), place, source))
            placed[name] = by_source[source] = place
        return placed

    def export(self, destination):
        """Copy the description and every declared asset into the new directory `destination`,
        each at its `placements()` path, so the copy is one relocatable directory. The copy is
        built in `<destination>.partial`, checked against what this Setup loaded, and only then
        renamed; a failure removes the partial copy and says why, so a directory at `destination`
        is always a complete setup."""
        import shutil
        destination = Path(destination)
        partial = destination.with_name(destination.name + ".partial")
        if destination.exists():
            raise SetupError("refusing to export over %s" % destination)
        if partial.exists():
            raise SetupError("%s is left from an export that did not finish; remove it and export "
                             "again" % partial)
        renamed, sources = self.placements(), dict(self.assets())
        partial.mkdir(parents=True)
        try:
            copied = []
            for name in sorted(renamed, key=lambda n: not sources[n].is_dir()):
                place = renamed[name]
                if place in copied or any(inside(same_name(place), same_name(c)) for c in copied):
                    continue
                target = partial / place
                target.parent.mkdir(parents=True, exist_ok=True)
                if sources[name].is_dir():
                    shutil.copytree(sources[name], target, ignore=shutil.ignore_patterns(*IGNORED))
                else:
                    shutil.copy2(sources[name], target)
                copied.append(place)
            self._write_export(partial, renamed)
            for name, content in self.loaded:
                if name != "setup.json" and contents(partial / renamed[name]) != content:
                    raise SetupError("%s changed while it was exported, or was copied over; nothing "
                                     "was exported" % name)
        except BaseException:
            shutil.rmtree(partial, ignore_errors=True)
            raise
        os.replace(partial, destination)
        return Setup(destination)

    def _write_export(self, destination, renamed):
        data = json.loads(json.dumps(self.data))
        data["desk"] = renamed[data["desk"]]
        if data.get("guide"):
            base, _, anchor = data["guide"].partition("#")
            data["guide"] = renamed[base] + ("#" + anchor if anchor else "")
        if "toolbox" in data.get("material", {}):
            data["material"]["toolbox"] = renamed[data["material"]["toolbox"]]
        project = data.get("project") or {}
        if project.get("files"):
            project["files"] = dict((k, renamed[v]) for k, v in project["files"].items())
        if project.get("recipes"):
            project["recipes"] = renamed[project["recipes"]]
        data["tools"] = [renamed[t] for t in data.get("tools", [])]
        if data.get("guide_files"):
            data["guide_files"] = [renamed[f] for f in data["guide_files"]]
        (destination / "setup.json").write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
        return Setup(destination)
