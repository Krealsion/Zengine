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
from pathlib import Path
import re

FORMAT, VERSION = "zengine-setup", "1"
HERE = Path(__file__).resolve().parent
COLLECTION = HERE / "setups"
POWERS = ("input", "capture", "inspect", "inventory", "toolbox", "demo", "open")
MEDIA = ("sdl", "tui")
# The panes whose owners implement PaneResetRequested (workshop/setup_control.hpp).
RESETTABLE = ("zengine.inventory-pane", "zengine.info", "zengine.composer")
CHORD = re.compile(r"^((shift|ctrl|alt)\+)*[a-z0-9]+$")
TEXT_FIELDS = ("title", "summary", "task", "choose")


class SetupError(ValueError):
    """A description that cannot be used, with every reason found."""


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
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.name = self.root.name
        try:
            self.data = json.loads((self.root / "setup.json").read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise SetupError("%s/setup.json cannot be read: %s" % (self.root, error))
        problems = self.problems()
        if problems:
            raise SetupError("setup %r is not usable:\n  - %s" % (self.name, "\n  - ".join(problems)))

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
        """The WorkshopSetup envelope to apply, with each portable view in `views` seated in the
        next declared view slot (Workshop refuses a setup naming a view that does not exist)."""
        desk = json.loads(self.asset(self.data["desk"]).read_text(encoding="utf-8"))
        rows = desk["fields"]["panes"]
        for slot, view in zip(self.data.get("view_slots", []), views):
            rows.append({"provider": "zengine.inventory-pane", "pane": view,
                         "place": {"mode": "subcells", "x": str(slot["x"] * 48), "y": str(slot["y"] * 48)},
                         "width": {"mode": "subcells", "amount": str(slot["width"] * 48)},
                         "height": {"mode": "subcells", "amount": str(slot["height"] * 48)},
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

    def digest(self):
        """A hash of the description and every declared file, so a running instance can say
        whether what it prepared is still what the directory describes."""
        h = hashlib.sha256()
        for name, path in self.assets():
            h.update(name.encode())
            for f in ([path] if path.is_file() else sorted(p for p in path.rglob("*") if p.is_file())):
                h.update(f.read_bytes())
        return h.hexdigest()[:16]

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
        for i, p in enumerate(self.providers()):
            if not (p.get("role") and p.get("prepare") in ("build",)):
                out.append("providers[%d] needs a role and prepare: build" % i)
        if self.providers() and not (d.get("project") or {}).get("recipes"):
            out.append("a provider built by this setup needs a project with recipes")
        try:
            for name, path in self.assets():
                if not path.exists():
                    out.append("declared asset %s is missing (%s)" % (name, path))
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
        reset = d.get("reset") or {}
        if reset.get("scope"):
            lines.append("Reset:       " + reset["scope"])
        if reset.get("keeps"):
            lines.append("Reset keeps: " + reset["keeps"])
        if d.get("guide"):
            lines.append("Guide:       " + str(self.asset(d["guide"].split("#")[0])) +
                         ("#" + d["guide"].split("#", 1)[1] if "#" in d["guide"] else ""))
        return "\n".join(lines)

    def export(self, destination):
        """Copy the description and every declared asset into `destination`, flattening each
        reference to a name inside it, so the copy is one relocatable directory."""
        import shutil
        destination = Path(destination)
        if destination.exists():
            raise SetupError("refusing to export over %s" % destination)
        destination.mkdir(parents=True)
        renamed = {}
        for name, path in self.assets():
            if name == "setup.json":
                continue
            local = name.replace("\\", "/").lstrip("./")
            local = "/".join(part for part in local.split("/") if part not in ("..", "."))
            renamed[name] = local
            target = destination / local
            target.parent.mkdir(parents=True, exist_ok=True)
            if path.is_dir():
                shutil.copytree(path, target, ignore=shutil.ignore_patterns("__pycache__"))
            else:
                shutil.copy2(path, target)
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
        (destination / "setup.json").write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")
        return Setup(destination)
