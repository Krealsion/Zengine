# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Setup descriptions and their preparation's ownership and failure checks, without a window or
another host."""
import base64
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import types
import unittest

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "external-host/tools/workshop"))
# workshop_steps spells chords and names Loom's value kinds; the kinds are not needed here.
sys.modules.setdefault("loom_session", types.ModuleType("loom_session"))
values = types.ModuleType("loom_session.values")
values.BOOL, values.BYTES, values.FLOAT, values.INT, values.TEXT = "bool", "bytes", "float", "int", "text"
sys.modules.setdefault("loom_session.values", values)
import setups as described  # noqa: E402
from demo_setup import prepare, layout, Measured, failure_note  # noqa: E402
_spec = importlib.util.spec_from_file_location("demo_launcher", REPO / "external-host/demo.py")
launcher = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(launcher)

CANNOT = "setup names a pane this Workshop cannot present: "
LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)\)")
PUBLISHED = re.compile(r"^https://github\.com/Krealsion/Zengine/(blob|tree)/main/([^#]+)(?:#(.*))?$")


def anchors(page):
    """The heading anchors of a Markdown page, as GitHub spells them."""
    heads = re.findall(r"^#+\s+(.*?)\s*$", page.read_text(encoding="utf-8"), re.M)
    return {re.sub(r"[^\w\- ]", "", h.lower()).replace(" ", "-") for h in heads}


class Owner:
    """Workshop's owners as the service meets them: answers, refusals and the calls made. Its
    portable views behave as inventory-pane/slots.hpp does: a removed entry keeps its place and
    binding, the door refuses an entry it cannot name, a rebind keeps the item's switch, an item in
    no view is live while main Inventory's context is ON, and an edit leaving two live items on one
    chord is refused with the arrangement kept."""
    def __init__(self):
        self.entries, self.calls, self.next_id, self.fail = [], [], 0, None
        self.toolbox, self.views, self.bindings = [], [], []
        self.inventory_active = False
        self.held = set()        # providers Workshop can present beyond its own
        self.refuse_enable = None
        self.desk = None
        self.pictures, self.unsettled = set(), set()  # providers whose panes draw, or never show
        self.blank = set()        # providers whose text panes show no row
        self.versions = []        # the PaneView version each reading asked

    def check(self, ok, why):
        if not ok:
            raise ValueError(why)

    def step(self, _):
        pass

    def entry(self, fields, label=None, folder=""):
        self.next_id += 1
        item = dict(reference={"owner": "inventory", "entry": str(self.next_id)}, revision=1,
                    label=label or fields["label"], pair=fields.get("pair", b"original"), folder=folder)
        self.entries.append(item)
        return deepcopy(item)

    def ask(self, role, shape, fields, **kwargs):
        self.calls.append((role, shape, fields))
        if self.fail == shape:
            raise ValueError(role + ": deliberately refused " + shape)
        if shape == "PaneViewRequested":
            # Workshop's PaneView version 3: a text pane's rows as words, or the words and parts a
            # canvas pane drew -- here none, only that it drew. Version 1 reads rows by number, and
            # a pane drawing a picture refuses it.
            self.versions.append(kwargs.get("version", 1))
            if fields["provider"] == "zengine.workshop" or fields["provider"] in self.unsettled:
                raise ValueError("pane view unavailable: no settled text picture")
            if kwargs.get("version", 1) != 3:
                if fields["provider"] in self.pictures:
                    raise ValueError("pane view unavailable: the pane draws a picture, not text rows")
                return {"picture": 1, "rows": [{"row": 0, "text": "ready", "x": 0, "y": 0, "space": 2}]}
            if fields["provider"] in self.pictures:
                return {"provider": fields["provider"], "pane": fields["pane"], "picture": 2,
                        "canvas": True, "words": [], "parts": []}
            words = [] if fields["provider"] in self.blank else [
                {"word": 0, "text": "ready", "place": {"x": 0, "y": 0, "w": 60, "h": 12},
                 "x": 30, "y": 6, "space": 2}]
            return {"provider": fields["provider"], "pane": fields["pane"], "picture": 1,
                    "canvas": False, "words": words, "parts": []}
        if shape == "InventoryList":
            return {"owner": "inventory", "entries": deepcopy(self.entries), "folders": []}
        if shape in ("InventoryCaptureAdd", "InventoryAdd"):
            return self.entry(fields, folder=(fields.get("folder") or {}).get("folder", ""))
        if shape == "InventoryRead":
            return {"pair": next(e for e in self.entries if e["reference"] == fields["reference"])["pair"]}
        if shape == "InventoryRemove":
            self.entries = [e for e in self.entries if e["reference"] != fields["reference"]]
            return {}
        if shape in ("InventoryWrite", "InventoryRename"):
            item = next(e for e in self.entries if e["reference"] == fields["reference"])
            self.check(item["revision"] == fields["revision"], "stale revision")
            item.update({k: v for k, v in fields.items() if k != "revision"})
            item["revision"] += 1
            return deepcopy(item)
        if shape == "InventoryFile":
            item = next(e for e in self.entries if e["reference"] == fields["reference"])
            self.check(item["folder"] == fields["from"]["folder"], "not in that folder")
            item["folder"] = fields["into"]["folder"]
            return deepcopy(item)
        if shape == "InventoryToolboxRestore":
            self.check(not self.entries or fields["replace"], "nonempty collection")
            for label, folder in self.toolbox:
                self.entry({"label": label, "pair": ("toolbox " + label).encode()}, folder=folder)
            if self.toolbox_view:
                view = {"id": "inventory.%d" % (len(self.views) + 1), "kind": "row", "active": False,
                        "entries": [e["reference"] for e in self.entries if e["label"] in self.toolbox_view]}
                self.views.append(view)
            return {"operation": "restore", "entries": len(self.toolbox)}
        if shape == "InventoryViewsRequested":
            return {"views": deepcopy(self.views), "bindings": deepcopy(self.bindings),
                    "inventory_active": self.inventory_active}
        if shape == "InventoryViewEdit":
            return self.edit(fields)
        if shape == "SetupApplyRequested":
            desk = json.loads(fields["setup"])
            for p in desk["fields"]["panes"]:
                if p["provider"].startswith("td.") and p["provider"] not in self.held:
                    raise ValueError(CANNOT + p["provider"] + " " + p["pane"])
            self.desk = desk
            return {}
        return {}

    toolbox_view = ()

    def edit(self, f):
        if f["entry"]["entry"] and not any(e["reference"] == f["entry"] for e in self.entries):
            raise ValueError("The entry's owner or identity is unavailable")
        kept = deepcopy((self.views, self.bindings, self.inventory_active))
        try:
            self.edited(f)
            live = [(b["scancode"], b["modifiers"]) for b in self.bindings
                    if b["enabled"] and self.live(b["reference"])]
            if len(live) != len(set(live)):
                raise ValueError("View change refused; previous arrangement retained: a chord is authored for both")
        except ValueError:
            self.views, self.bindings, self.inventory_active = kept
            raise
        return {}

    def live(self, ref):
        """Whether an enabled item's key is registered: its view's context, or main Inventory's."""
        view = next((v for v in self.views if ref in v["entries"]), None)
        return view["active"] if view else self.inventory_active

    def edited(self, f):
        op, ref = f["operation"], f["entry"]
        if op == "move" and f["view"] != "inventory" and not any(v["id"] == f["view"] for v in self.views):
            raise ValueError("That inventory view is unavailable")
        for v in self.views:
            if op in ("create", "move") and ref in v["entries"]:
                v["entries"].remove(ref)
        if op == "create":
            self.views.append({"id": "inventory.%d" % (len(self.views) + 1), "kind": f["text"],
                               "active": False, "entries": [ref]})
        elif op == "move" and f["view"] != "inventory":
            view = next(v for v in self.views if v["id"] == f["view"])
            if view["kind"] == "single":
                view["entries"].clear()
            view["entries"].append(ref)
        elif op == "bind":
            bound = next((b for b in self.bindings if b["reference"] == ref), None)
            if bound is None:
                bound = {"reference": ref, "enabled": False}
                self.bindings.append(bound)
            bound.update(target=f["text"], scancode=f["scancode"], modifiers=f["modifiers"])
        elif op == "enable":
            if self.refuse_enable:
                raise ValueError(self.refuse_enable)
            next(b for b in self.bindings if b["reference"] == ref)["enabled"] = f["enabled"]
        elif op == "context" and f["view"] == "inventory":
            self.inventory_active = f["enabled"]
        elif op == "context":
            next(v for v in self.views if v["id"] == f["view"])["active"] = f["enabled"]

    def shapes(self, shape):
        return [fields for _, s, fields in self.calls if s == shape]


def make(tmp, name="made", **fields):
    """A small described setup in `tmp`, desk included."""
    root = Path(tmp) / name
    root.mkdir()
    desk = {"zen": 1, "schema": "WorkshopSetup", "version": 4, "fields": {
        "format": "zengine-workshop-setup", "format_version": "4", "name": name, "panes": [
            {"provider": p, "pane": k, "place": {"mode": "pixels", "x": "12", "y": "0"},
             "width": {"mode": "pixels", "amount": "120"}, "height": {"mode": "pixels", "amount": "120"},
             "front": str(i)} for i, (p, k) in enumerate(fields.pop("panes", [
                 ("zengine.inventory-pane", "inventory"), ("zengine.demo", "controls")]))]}}
    (root / "desk.json").write_text(json.dumps(desk), encoding="utf-8")
    (root / "README.md").write_text("# guide\n", encoding="utf-8")
    data = {"format": "zengine-setup", "format_version": "1", "title": "Made", "summary": "s",
            "task": "t", "choose": "c", "guide": "README.md", "first_task": {"do": "d", "expect": "e"},
            "medium": {"supports": ["sdl"], "viewport": [120, 56]},
            "authority": {"may": ["input", "capture", "inventory", "toolbox", "demo"]}, "desk": "desk.json"}
    data.update(fields)
    (root / "setup.json").write_text(json.dumps(data), encoding="utf-8")
    return root


def act_module():
    """workshop/act, imported with Loom's runtime stood in for; its verbs are what a test reads."""
    tool = types.ModuleType("loom_session.tool")
    tool.Refused = type("Refused", (Exception,), {})
    saved = sys.modules.get("loom_session.tool")
    sys.modules["loom_session.tool"] = tool
    try:
        spec = importlib.util.spec_from_file_location("act_verbs", REPO / "external-host/tools/workshop/act.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if saved is None:
            sys.modules.pop("loom_session.tool", None)
        else:
            sys.modules["loom_session.tool"] = saved


class Recipes(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp)

    def run_setup(self, owner, name, state):
        prepare(owner, described.load(name), state, "workshop")

    # ---- the migrated setups keep what they did ------------------------------------------------
    def test_editor_materials_resets_inventory_applies_its_desk_and_creates_no_values(self):
        owner, state = Owner(), {"fixtures": []}
        self.run_setup(owner, "editor-materials", state)
        resets = [(role, fields["pane"]) for role, shape, fields in owner.calls if shape == "PaneResetRequested"]
        self.assertEqual(resets, [("zengine.inventory-pane", "inventory")])
        self.assertEqual(owner.entries, [])
        panes = [(p["provider"], p["pane"]) for p in layout("editor-materials")["fields"]["panes"]]
        self.assertEqual(panes, [("zengine.editor", "editor"), ("zengine.inventory-pane", "inventory"),
                                 ("zengine.files", "project-files"), ("zengine.terminal", "terminal"),
                                 ("zengine.demo", "controls"), ("zengine.workshop", "layouts")])

    def test_warm_reset_restores_owned_values_and_labels_but_preserves_user_copies(self):
        owner, state = Owner(), {"fixtures": []}
        self.run_setup(owner, "values", state)
        owner.entries[0].update(pair=b"changed", label="renamed")
        owner.entries[0]["revision"] += 1
        copy = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"mine", "label": "my work"})
        self.run_setup(owner, "values", state)
        self.assertEqual(len(owner.entries), 10)
        self.assertEqual(owner.entries[-1], copy)
        self.assertEqual(owner.entries[0]["pair"], b"original")
        self.assertEqual(owner.entries[0]["label"], "Demo input 1")
        self.assertFalse(any(shape == "InputSessionRequested" for _, shape, _ in owner.calls))
        owner.calls.clear()
        self.run_setup(owner, "values", state)
        self.assertFalse(owner.shapes("InventoryWrite"))

    def test_missing_owned_entry_is_recreated_without_reusing_its_reference(self):
        owner, state = Owner(), {"fixtures": []}
        self.run_setup(owner, "commands", state)
        removed = owner.entries.pop(0)
        self.run_setup(owner, "commands", state)
        self.assertEqual(len(owner.entries), 9)
        self.assertNotEqual(state["fixtures"][0]["reference"], removed["reference"])
        self.assertEqual(owner.entries[-1]["pair"], removed["pair"])

    def test_partial_first_prepare_can_finish_and_restore_earlier_fixtures(self):
        owner, state = Owner(), {"fixtures": []}
        first = owner.ask("zengine.inventory", "InventoryCaptureAdd", {"label": "Demo input 1"})
        state["fixtures"].append(dict(reference=first["reference"], label=first["label"],
                                      pair=base64.b64encode(first["pair"]).decode()))
        owner.entries[0]["pair"] = b"edited during recovery"
        self.run_setup(owner, "values", state)
        self.assertEqual(len(state["fixtures"]), 9)
        self.assertEqual(owner.entries[0]["pair"], b"original")

    def test_owner_refusal_does_not_claim_ready_or_change_inventory(self):
        owner, state = Owner(), {"fixtures": []}
        owner.fail = "PaneResetRequested"
        measured = Measured(owner)
        with self.assertRaisesRegex(ValueError, "zengine.inventory-pane"):
            prepare(measured, described.load("values"), state, "workshop")
        self.assertEqual(owner.entries, [])
        self.assertEqual(measured.outcomes["failed_or_unanswered"], 1)
        self.assertEqual(sum(measured.calls.values()), 1)
        self.assertEqual(state["reached"], "pane resets")

    def test_presets_reset_both_editors_and_preserve_nonfixture_presets(self):
        owner, state = Owner(), {"fixtures": []}
        self.run_setup(owner, "presets", state)
        saved = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"partial", "label": "my preset"})
        owner.calls.clear()
        self.run_setup(owner, "presets", state)
        self.assertEqual(owner.entries[-1], saved)
        self.assertEqual({role for role, shape, _ in owner.calls if shape == "PaneResetRequested"},
                         {"zengine.inventory-pane", "zengine.info", "zengine.composer"})

    def test_named_layouts_resolve_to_distinct_stories_and_reject_typos(self):
        self.assertEqual([p["pane"] for p in layout("values")["fields"]["panes"]],
                         ["inventory", "info", "controls", "layouts"])
        self.assertEqual([p["pane"] for p in layout("commands")["fields"]["panes"]],
                         ["inventory", "loaded", "compose", "controls", "layouts"])
        with self.assertRaises(ValueError):
            layout("value")

    def test_every_shipped_desk_shows_layouts_and_readiness_asks_its_providers_alone(self):
        # Workshop applies a desk as written, so a desk that leaves Layouts out opens without it.
        found = described.collection([REPO / "examples"])
        for name, directory in found.items():
            rows = described.Setup(directory).desk()["fields"]["panes"]
            self.assertIn(("zengine.workshop", "layouts"), [(r["provider"], r["pane"]) for r in rows], name)
        owner, state = Owner(), {"fixtures": []}
        self.run_setup(owner, "values", state)
        self.assertEqual(state["reached"], "ready")
        asked = [f["provider"] for f in owner.shapes("PaneViewRequested")]
        self.assertNotIn("zengine.workshop", asked)
        self.assertEqual(sorted(asked), ["zengine.demo", "zengine.info", "zengine.inventory-pane"])
        # Readiness reads a pane's words (version 3), which a pane drawing a picture answers too.
        self.assertEqual(set(owner.versions), {3})

    # ---- descriptions ---------------------------------------------------------------------------
    def test_every_shipped_setup_is_usable_and_says_how_to_use_it(self):
        found = described.collection([REPO / "examples"])
        self.assertTrue({"values", "commands", "presets", "workbench", "folders", "editor-materials",
                         "tower-defense"} <= set(found))
        for name, directory in found.items():
            setup = described.Setup(directory)
            text = setup.describe()
            for words in ("First task:", "Expect:", "Guide:", "Authority:", "Reset:"):
                self.assertIn(words, text, name)
            summary = json.dumps(setup.summary())
            self.assertNotIn("credential", summary, name)
            self.assertTrue(Path(setup.summary()["guide_path"]).is_file(), name)

    def test_every_shipped_guide_works_from_a_copy_outside_the_repository(self):
        # `start` prepares from an export of the setup (demo.py), so a copy out of the tree is what
        # a weaver opens: its pictures and local links must be in the copy, and every other page of
        # this repository must be a published link to a file and heading that exist here.
        found = described.collection([REPO / "examples"])
        checked = 0
        for name, directory in found.items():
            copy = described.Setup(directory).export(Path(self.tmp) / "copies" / name)
            guide, _, anchor = copy.get("guide").partition("#")
            links = [(guide, "#" + anchor if anchor else "")]
            text = copy.asset(guide).read_text(encoding="utf-8")
            links += [(guide, target) for target in LINK.findall(text)]
            for where, target in links:
                checked += 1
                published = PUBLISHED.match(target)
                if published:
                    kind, path, heading = published.groups()
                    self.assertTrue((REPO / path).is_dir() if kind == "tree" else (REPO / path).is_file(),
                                    "%s: %s" % (name, target))
                    if heading:
                        self.assertIn(heading, anchors(REPO / path), "%s: %s" % (name, target))
                    continue
                self.assertFalse(re.match(r"^[a-z]+:", target), "%s links to %s, not a page of this "
                                 "repository or a file in the setup" % (name, target))
                path, _, heading = target.partition("#")
                resolved = (copy.asset(where).parent / path).resolve() if path else copy.asset(where)
                self.assertTrue(resolved.is_relative_to(copy.root) and resolved.exists(),
                                "%s: %s is not in its copy" % (name, target))
                if heading:
                    self.assertIn(heading, anchors(resolved), "%s: %s" % (name, target))
        self.assertGreater(checked, len(found))

    def test_guide_files_stay_inside_the_setup_beside_its_guide(self):
        (Path(self.tmp) / "shared.png").write_bytes(b"png")
        make(self.tmp, "out", guide_files=["../shared.png"])
        with self.assertRaisesRegex(described.SetupError, "guide file '../shared.png' must be inside"):
            described.Setup(Path(self.tmp) / "out")
        (Path(self.tmp) / "shared.md").write_text("# guide\n", encoding="utf-8")
        with self.assertRaisesRegex(described.SetupError, "needs a guide inside"):
            described.Setup(make(self.tmp, "away", guide="../shared.md", guide_files=["README.md"]))
        root = make(self.tmp, "kept", guide_files=["images"])
        (root / "images").mkdir()
        (root / "images" / "a.png").write_bytes(b"png")
        setup = described.Setup(root)
        self.assertIn(("images", (root / "images").resolve()), setup.assets())
        copy = setup.export(Path(self.tmp) / "kept-copy")
        self.assertEqual((copy.root / "images" / "a.png").read_bytes(), b"png")
        self.assertEqual(copy.get("guide_files"), ["images"])
        (root / "images" / "a.png").write_bytes(b"changed")
        self.assertNotEqual(described.Setup(root).digest(), setup.digest())

    def test_a_description_that_cannot_be_used_names_every_reason(self):
        make(self.tmp, "bad", authority={"may": ["input", "everything"]}, material={"toolbox": "C:/x.toolbox"},
             hotkeys=[{"key": "alt+1", "entry": "a", "target": "t", "means": "m"},
                      {"key": "alt+1", "entry": "b", "target": "t", "means": "m"}])
        with self.assertRaises(described.SetupError) as refused:
            described.Setup(Path(self.tmp) / "bad")
        said = str(refused.exception)
        for words in ("unknown power", "must include demo", "relative path", "share one chord",
                      "needs the toolbox power"):
            self.assertIn(words, said)

    def test_loading_or_describing_a_setup_asks_nothing(self):
        owner = Owner()
        setup = described.load("tower-defense", [REPO / "examples"])
        setup.describe(); setup.summary()
        self.assertEqual(owner.calls, [])

    # ---- walks ------------------------------------------------------------------------------------
    def test_a_walk_needs_a_name_what_it_checks_and_steps_that_name_no_path(self):
        make(self.tmp, "walks", walks={
            "Bad Name": {"about": "a", "steps": [{"press": "x"}]},
            "no-about": {"steps": [{"press": "x"}]},
            "no-steps": {"about": "a", "steps": []},
            "pictures": {"about": "a", "steps": [{"picture": "p/1"}, {"picture": "two"}, {"picture": "two"},
                                                  {"picture": "three", "save": "C:/x.png"}]}})
        with self.assertRaises(described.SetupError) as refused:
            described.Setup(Path(self.tmp) / "walks")
        said = str(refused.exception)
        for words in ("walk name 'Bad Name'", "walk 'no-about' needs `about`", "walk 'no-steps' needs `steps`",
                      "names picture(s) ['p/1']", "names picture(s) ['two'] twice", "names no `save` path"):
            self.assertIn(words, said)
        with self.assertRaisesRegex(described.SetupError, "walks maps each walk's name"):
            described.Setup(make(self.tmp, "listed", walks=[{"press": "x"}]))

    def test_every_shipped_walk_names_one_of_acts_verbs_per_step(self):
        verbs, walked = act_module().VERBS, set()
        for name, directory in described.collection([REPO / "examples"]).items():
            setup = described.Setup(directory)
            for walk, described_walk in setup.walks().items():
                walked.add(name + "/" + walk)
                for i, step in enumerate(described_walk["steps"]):
                    self.assertEqual(len([v for v in verbs if v in step]), 1, "%s %s step %d" % (name, walk, i))
            if setup.walks():
                self.assertIn("Walks (demo.py walk", setup.describe(), name)
        self.assertTrue({"values/layouts", "presets/escape", "editor-materials/escape"} <= walked, walked)

    def test_a_walk_replays_through_act_and_keeps_what_it_made_in_the_folder_named(self):
        root = make(self.tmp, "walked", walks={"look": {"about": "a look", "steps": [
            {"press": "ctrl+p"}, {"picture": "one", "crop": [0, 0, 8, 8]}, {"expect": ["p", "k", "x"]}]}})
        made = Path(self.tmp) / "run-out"
        made.mkdir()
        for name, data in (("one.bmp", b"bmp"), ("one.png", b"png"), ("steps.json", b"[]")):
            (made / name).write_bytes(data)

        class Session:
            def __init__(self, state="passed"):
                self.state, self.started = state, []

            def start(self, tool, name, inputs):
                self.started.append((tool, json.loads(inputs["steps"])))

            def wait(self, name, timeout):
                if self.state == "late":
                    raise TimeoutError(name)
                return {"state": self.state, "failure": "step 3 of 3: expect: p/k never painted 'x' within 10s",
                        "artifacts": [{"name": n, "path": str(made / n)} for n in ("one.bmp", "one.png", "steps.json")]}
        module = types.SimpleNamespace(NotAnswered=TimeoutError)
        config = {"directory": str(root), "digest": described.Setup(root).digest(), "medium": "sdl",
                  "session": "S", "prepared": "P"}
        session, out = Session(), Path(self.tmp) / "pictures"
        result = launcher.walk(module, session, config, "look", out, 60)
        self.assertEqual(session.started, [("workshop/act", [{"press": "ctrl+p"},
                         {"picture": "one", "crop": [0, 0, 8, 8], "png": True}, {"expect": ["p", "k", "x"]}])])
        self.assertEqual(sorted(p.name for p in out.iterdir()), ["one.png", "steps.json"])
        self.assertEqual((result["state"], result["kept"], result["steps"]), ("passed", ["one.png", "steps.json"], 3))
        self.assertNotIn("description_changed", result)
        with self.assertRaisesRegex(RuntimeError, "already holds files"):
            launcher.walk(module, Session(), config, "look", out, 60)
        with self.assertRaisesRegex(RuntimeError, "carries no walk 'gone'; it carries: look"):
            launcher.walk(module, Session(), config, "gone", Path(self.tmp) / "unknown", 60)
        with self.assertRaisesRegex(RuntimeError, "had not finished within 60s and goes on; `loom-session show S walk-look-"):
            launcher.walk(module, Session("late"), config, "look", Path(self.tmp) / "late", 60)
        failed = launcher.walk(module, Session("failed"), config, "look", Path(self.tmp) / "failed", 60)
        self.assertEqual(failed["state"], "failed")
        self.assertIn("never painted", failed["failure"])
        self.assertTrue((Path(self.tmp) / "failed" / "one.png").is_file())
        # An edited walk is read as it is now; the answer says the root prepared another revision.
        data = json.loads((root / "setup.json").read_text(encoding="utf-8"))
        data["walks"]["look"]["steps"].append({"wait": 0})
        (root / "setup.json").write_text(json.dumps(data), encoding="utf-8")
        changed = launcher.walk(module, Session(), config, "look", Path(self.tmp) / "edited", 60)
        self.assertEqual(changed["steps"], 4)
        self.assertIn("this root prepared an earlier revision (P)", changed["description_changed"])

    # ---- toolbox material and hotkeys ---------------------------------------------------------------
    def toolbox_setup(self, **more):
        (Path(self.tmp) / "box.toolbox").write_bytes(b"fake")
        fields = dict(material={"toolbox": "../box.toolbox"},
                      hotkeys=[{"key": "alt+1", "entry": "Run", "target": "zengine.inventory", "view": "row",
                                "means": "capture"}],
                      view_slots=[{"x": 1, "y": 40, "width": 40, "height": 8}])
        fields.update(more)
        return described.Setup(make(self.tmp, **fields))

    def test_toolbox_material_is_restored_once_and_then_kept_by_identity(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Sample", "f1"), ("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        self.assertEqual(len(owner.shapes("InventoryToolboxRestore")), 1)
        self.assertEqual(owner.shapes("InventoryToolboxRestore")[0]["replace"], False)
        owner.entries[0].update(pair=b"edited", label="mine now", folder="")
        owner.entries[0]["revision"] += 1
        mine = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"new", "label": "my capture"})
        prepare(owner, setup, state, "workshop")
        self.assertEqual(len(owner.shapes("InventoryToolboxRestore")), 1)
        self.assertEqual((owner.entries[0]["pair"], owner.entries[0]["label"], owner.entries[0]["folder"]),
                         (b"toolbox Sample", "Sample", "f1"))
        self.assertIn(mine, owner.entries)

    def test_a_toolbox_setup_refuses_a_collection_it_did_not_make(self):
        owner, state = Owner(), {"fixtures": []}
        owner.entry({"label": "someone's"})
        with self.assertRaisesRegex(ValueError, "already holds 1 entries"):
            prepare(owner, self.toolbox_setup(), state, "workshop")
        self.assertFalse(owner.shapes("InventoryToolboxRestore"))

    def test_hotkeys_are_placed_bound_and_turned_on_once_and_the_view_is_seated(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        (binding,) = owner.bindings
        self.assertEqual((binding["target"], binding["scancode"], binding["modifiers"], binding["enabled"]),
                         ("zengine.inventory", 30, 4, True))
        self.assertTrue(owner.views[0]["active"])
        self.assertIn(("zengine.inventory-pane", "inventory.1"),
                      [(p["provider"], p["pane"]) for p in owner.desk["fields"]["panes"]])
        edits = len(owner.shapes("InventoryViewEdit"))
        prepare(owner, setup, state, "workshop")
        again = [f["operation"] for f in owner.shapes("InventoryViewEdit")[edits:]]
        self.assertNotIn("create", again)
        self.assertNotIn("bind", again)
        self.assertEqual(len(owner.views), 1)

    def seated(self, owner):
        return [p["pane"] for p in owner.desk["fields"]["panes"] if p["pane"].startswith("inventory.")]

    def test_a_removed_hotkey_entry_comes_back_in_a_fresh_row_and_the_old_row_is_retired(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        gone = owner.entries[0]
        owner.ask("zengine.inventory", "InventoryRemove", {"reference": gone["reference"], "revision": 1})
        prepare(owner, setup, state, "workshop")
        (entry,) = owner.entries
        old, new = owner.views
        self.assertEqual((old["entries"], old["active"]), ([gone["reference"]], False))
        self.assertEqual((new["entries"], new["active"]), ([entry["reference"]], True))
        self.assertTrue(any(b["reference"] == entry["reference"] and b["enabled"] for b in owner.bindings))
        self.assertEqual(self.seated(owner), [new["id"]])
        prepare(owner, setup, state, "workshop")
        self.assertEqual(len(owner.views), 2)

    def test_reset_puts_the_declared_command_back_in_its_own_row(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", ""), ("Note", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        run, note = (e["reference"] for e in owner.entries)
        owner.edit(dict(operation="create", text="single", entry=run))
        owner.edit(dict(operation="move", view="inventory.1", entry=note))
        prepare(owner, setup, state, "workshop")
        row, single = owner.views
        self.assertEqual((row["entries"], row["active"]), ([run], True))
        self.assertEqual((single["entries"], single["active"]), ([], False))
        self.assertEqual(self.seated(owner), ["inventory.1"])
        edits = len(owner.shapes("InventoryViewEdit"))
        prepare(owner, setup, state, "workshop")
        again = [f["operation"] for f in owner.shapes("InventoryViewEdit")[edits:]]
        self.assertEqual(set(again), {"enable"})

    def test_a_users_view_keeps_its_entries_and_switch_when_reset_takes_the_command_back(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        run = owner.entries[0]["reference"]
        mine = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"mine", "label": "My command"})["reference"]
        owner.edit(dict(operation="create", text="row", entry=mine))
        owner.edit(dict(operation="bind", entry=mine, text="zengine.inventory", scancode=31, modifiers=4))
        owner.edit(dict(operation="enable", entry=mine, enabled=True))
        owner.edit(dict(operation="move", view="inventory.2", entry=run))
        prepare(owner, setup, state, "workshop")
        ours, theirs = owner.views
        self.assertEqual((ours["entries"], ours["active"]), ([run], True))
        self.assertEqual((theirs["entries"], theirs["active"]), ([mine], False))
        self.assertTrue(next(b for b in owner.bindings if b["reference"] == mine)["enabled"])
        self.assertEqual(self.seated(owner), ["inventory.1"])

    def arrange(self, owner, **fields):
        """A weaver's own edit through Inventory's door, refused as Inventory refuses it."""
        whole = dict(operation="", view="", text="", entry={"owner": "", "entry": ""}, scancode=0,
                     modifiers=0, enabled=False)
        whole.update(fields)
        owner.edit(whole)

    def independent(self, owner, key):
        """A command of the weaver's in a row of its own, bound to `key` (alt+N), enabled and ON."""
        mine = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"mine", "label": "My command"})["reference"]
        self.arrange(owner, operation="create", text="row", entry=mine)
        self.arrange(owner, operation="bind", entry=mine, text="zengine.inventory", scancode=29 + key, modifiers=4)
        self.arrange(owner, operation="enable", entry=mine, enabled=True)
        view = next(v["id"] for v in owner.views if mine in v["entries"])
        self.arrange(owner, operation="context", view=view, enabled=True)
        return mine, view

    def rearranged(self, owner, run, key):
        """The review's arrangement: the setup's command, still enabled, rebound to `key` in a single
        view of the weaver's that is OFF."""
        self.arrange(owner, operation="create", text="single", entry=run)
        self.arrange(owner, operation="bind", entry=run, text="zengine.inventory", scancode=29 + key, modifiers=4)
        single = next(v["id"] for v in owner.views if run in v["entries"])
        self.assertTrue(next(b for b in owner.bindings if b["reference"] == run)["enabled"])
        return single

    def keys(self, owner, ref):
        """(target, scancode, modifiers, item enabled, key registered)."""
        b = next(b for b in owner.bindings if b["reference"] == ref)
        return (b["target"], b["scancode"], b["modifiers"], b["enabled"], b["enabled"] and owner.live(ref))

    def test_reset_rebinds_a_moved_command_before_its_row_makes_the_key_live(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        run = owner.entries[0]["reference"]
        single = self.rearranged(owner, run, 2)
        mine, theirs = self.independent(owner, 2)  # valid: only one Alt+2 is live
        for _ in range(2):  # the repair, then a Reset with nothing to repair
            prepare(owner, setup, state, "workshop")
            self.assertEqual(state["reached"], "ready")
            ours = next(v for v in owner.views if v["id"] == state["views"]["row"])
            self.assertEqual((ours["entries"], ours["active"]), ([run], True))
            self.assertEqual(self.keys(owner, run), ("zengine.inventory", 30, 4, True, True))
            self.assertEqual(self.keys(owner, mine), ("zengine.inventory", 31, 4, True, True))
            self.assertEqual(next(v for v in owner.views if v["id"] == theirs)["entries"], [mine])
            self.assertFalse(next(v for v in owner.views if v["id"] == single)["active"])
            self.assertEqual(self.seated(owner), [ours["id"]])  # one declared view slot
            self.assertNotIn("held_off", state)

    def test_reset_swaps_two_declared_chords_without_a_transient_collision(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", ""), ("Note", "")]
        setup = self.toolbox_setup(hotkeys=[
            {"key": "alt+1", "entry": "Run", "target": "zengine.inventory", "view": "row", "means": "m"},
            {"key": "alt+2", "entry": "Note", "target": "zengine.inventory", "view": "row", "means": "m"}])
        prepare(owner, setup, state, "workshop")
        run, note = (e["reference"] for e in owner.entries)
        self.arrange(owner, operation="enable", entry=note, enabled=False)
        self.arrange(owner, operation="bind", entry=note, text="zengine.inventory", scancode=32, modifiers=4)
        self.arrange(owner, operation="bind", entry=run, text="zengine.inventory", scancode=31, modifiers=4)
        self.arrange(owner, operation="enable", entry=note, enabled=True)  # Run on Alt+2, Note on Alt+3
        self.arrange(owner, operation="bind", entry=note, text="zengine.inventory", scancode=30, modifiers=4)
        prepare(owner, setup, state, "workshop")
        self.assertEqual(state["reached"], "ready")
        self.assertEqual(self.keys(owner, run), ("zengine.inventory", 30, 4, True, True))
        self.assertEqual(self.keys(owner, note), ("zengine.inventory", 31, 4, True, True))

    def test_a_chord_another_live_command_holds_is_refused_and_that_command_keeps_it(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        run = owner.entries[0]["reference"]
        self.rearranged(owner, run, 1)
        mine, theirs = self.independent(owner, 1)  # Alt+1 in the weaver's live row: a real conflict
        with self.assertRaises(ValueError) as refused:
            prepare(owner, setup, state, "workshop")
        self.assertEqual(state["reached"], "hotkey activation")
        # The controls keep 256 bytes of the note: the service's own account comes first.
        note = failure_note(refused.exception, state)[:256]
        self.assertTrue(note.startswith("Failed at hotkey activation; this setup's alt+1 left OFF until a Reset "
                                        "completes: hotkey alt+1 (Run) not switched on; the chord is also enabled "
                                        "in %s (ON). Inventory: View change refused" % theirs), note)
        self.assertEqual(self.keys(owner, mine), ("zengine.inventory", 30, 4, True, True))
        self.assertEqual(self.keys(owner, run), ("zengine.inventory", 30, 4, False, False))
        self.assertEqual(next(v for v in owner.views if v["id"] == state["views"]["row"])["entries"], [run])
        self.assertEqual(state["held_off"], ["alt+1"])
        # the weaver settles it
        self.arrange(owner, operation="context", view=theirs, enabled=False)
        prepare(owner, setup, state, "workshop")
        self.assertEqual(state["reached"], "ready")
        self.assertEqual(self.keys(owner, run), ("zengine.inventory", 30, 4, True, True))
        self.assertEqual(self.keys(owner, mine), ("zengine.inventory", 30, 4, True, False))
        self.assertNotIn("held_off", state)

    def test_the_first_preparation_adopts_the_row_its_toolbox_brought(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox, owner.toolbox_view = [("Run", "")], ("Run",)
        prepare(owner, self.toolbox_setup(), state, "workshop")
        self.assertNotIn("create", [f["operation"] for f in owner.shapes("InventoryViewEdit")])
        self.assertEqual((len(owner.views), owner.views[0]["active"]), (1, True))

    def test_declared_views_need_a_kind_and_a_slot_each(self):
        with self.assertRaises(described.SetupError) as refused:
            self.toolbox_setup(view_slots=[], hotkeys=[
                {"key": "alt+1", "entry": "Run", "target": "t", "view": "grid", "means": "m"},
                {"key": "alt+2", "entry": "B", "target": "t", "view": "single", "means": "m"},
                {"key": "alt+3", "entry": "C", "target": "t", "view": "single", "means": "m"}])
        for words in ("not one of row, column, single", "a single view holds one hotkey",
                      "declare 2 view(s) but view_slots places 0"):
            self.assertIn(words, str(refused.exception))

    def test_a_hotkey_conflict_is_reported_after_the_desk_and_never_as_ready(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        owner.refuse_enable = "alt+1 is already active for another command"
        with self.assertRaisesRegex(ValueError, "hotkey alt\\+1 \\(Run\\).*already active"):
            prepare(owner, self.toolbox_setup(), state, "workshop")
        self.assertEqual(state["reached"], "hotkey activation")
        self.assertIsNotNone(owner.desk)

    # ---- providers --------------------------------------------------------------------------------------
    def provider_setup(self, providers=True):
        (Path(self.tmp) / "recipes.json").write_text("{}", encoding="utf-8")
        panes = [("zengine.builder-pane", "builder"), ("td.game", "td"), ("zengine.demo", "controls")]
        more = dict(project={"recipes": "../recipes.json"}, providers=[{"role": "td.game", "prepare": "build"}])
        return described.Setup(make(self.tmp, panes=panes, **(more if providers else {})))

    def test_a_missing_provider_is_built_then_its_pane_is_seated(self):
        owner, state, built = Owner(), {"fixtures": []}, []
        fake = types.ModuleType("builder")
        fake.perform = lambda ctx, inputs, name: (built.append(inputs), owner.held.add("td.game"), "built")[2]
        sys.modules["builder"] = fake
        self.addCleanup(sys.modules.pop, "builder")
        setup = self.provider_setup()
        prepare(owner, setup, state, "workshop")
        self.assertEqual([b["act"] for b in built], ["frontier"])
        applied = [[p["provider"] for p in json.loads(f["setup"])["fields"]["panes"]]
                   for f in owner.shapes("SetupApplyRequested")]
        self.assertEqual(applied[1], ["zengine.builder-pane", "zengine.demo"])
        self.assertIn("td.game", applied[-1])
        prepare(owner, setup, state, "workshop")
        self.assertEqual(len(built), 1)

    def test_a_pane_nobody_offers_is_left_off_a_ready_desk_and_named(self):
        # A described view nobody has run yet: Workshop cannot present it and the setup does not
        # build it, and the rest of the desk is usable, so the demo is ready and says what is missing.
        import demo_setup
        owner, state = Owner(), {"fixtures": []}
        setup = self.provider_setup(providers=False)
        prepare(owner, setup, state, "workshop")
        self.assertEqual(state["reached"], "ready")
        self.assertEqual(state["unseated"], ["td.game td"])
        self.assertEqual([p["provider"] for p in owner.desk["fields"]["panes"]],
                         ["zengine.builder-pane", "zengine.demo"])
        self.assertIn("Not on the desk: td.game td", demo_setup.ready_note(setup, state))
        # Offered by the next Reset, it takes its seat and is no longer named.
        owner.held.add("td.game")
        prepare(owner, setup, state, "workshop")
        self.assertEqual(state["unseated"], [])
        self.assertIn("td.game", [p["provider"] for p in owner.desk["fields"]["panes"]])

    def test_a_start_builds_and_seats_whatever_order_a_built_pane_and_an_unoffered_one_stand_in(self):
        # A pane the setup builds and a pane nobody offers, in both orders: the desk without the
        # built pane leaves the unoffered one off too, so the build runs, and the final desk leaves
        # it off and names it.
        built_then_missing = [("zengine.builder-pane", "builder"), ("td.game", "td"),
                              ("td.missing", "view"), ("zengine.demo", "controls")]
        missing_then_built = [("zengine.builder-pane", "builder"), ("td.missing", "view"),
                              ("td.game", "td"), ("zengine.demo", "controls")]
        for name, panes in (("built-first", built_then_missing), ("missing-first", missing_then_built)):
            with self.subTest(order=name):
                owner, state, built = Owner(), {"fixtures": []}, []
                fake = types.ModuleType("builder")
                fake.perform = lambda ctx, inputs, name, owner=owner, built=built: (
                    built.append(inputs), owner.held.add("td.game"), "built")[2]
                sys.modules["builder"] = fake
                self.addCleanup(sys.modules.pop, "builder", None)
                (Path(self.tmp) / "recipes.json").write_text("{}", encoding="utf-8")
                setup = described.Setup(make(self.tmp, name=name, panes=panes,
                                             project={"recipes": "../recipes.json"},
                                             providers=[{"role": "td.game", "prepare": "build"}]))
                prepare(owner, setup, state, "workshop")
                self.assertEqual(state["reached"], "ready")
                self.assertEqual([b["act"] for b in built], ["frontier"])
                self.assertEqual(state["unseated"], ["td.missing view"])
                self.assertEqual(sorted(p["provider"] for p in owner.desk["fields"]["panes"]),
                                 ["td.game", "zengine.builder-pane", "zengine.demo"])

    def test_a_pane_that_draws_a_picture_is_ready_and_one_that_never_shows_is_named(self):
        import demo_setup
        self.addCleanup(setattr, demo_setup, "READY_SECONDS", demo_setup.READY_SECONDS)
        demo_setup.READY_SECONDS = 0.3
        root = make(self.tmp, panes=[("zengine.view.builder", "view-builder"), ("zengine.demo", "controls")])
        owner, state = Owner(), {"fixtures": []}
        owner.pictures = {"zengine.view.builder"}
        prepare(owner, described.Setup(root), state, "workshop")
        self.assertEqual((state["reached"], state["not_showing"]), ("ready", []))
        self.assertEqual(set(owner.versions), {3})
        # A text pane that shows no row has not shown.
        owner, state = Owner(), {"fixtures": []}
        owner.blank = {"zengine.view.builder"}
        prepare(owner, described.Setup(root), state, "workshop")
        self.assertEqual(state["not_showing"], ["zengine.view.builder view-builder (no rows)"])
        # A pane that never shows does not make a usable desk a failure: it is named.
        owner, state = Owner(), {"fixtures": []}
        owner.unsettled = {"zengine.view.builder"}
        setup = described.Setup(root)
        prepare(owner, setup, state, "workshop")
        self.assertEqual(state["reached"], "ready")
        self.assertEqual(state["not_showing"],
                         ["zengine.view.builder view-builder (pane view unavailable: no settled text picture)"])
        self.assertIn("Not showing after", demo_setup.ready_note(setup, state))

    def test_a_refused_stop_says_what_kept_workshop_open(self):
        tool = types.ModuleType("loom_session.tool")

        class LinkOutcome(Exception):
            state = "lost"
        tool.LinkOutcome = LinkOutcome
        sys.modules["loom_session.tool"] = tool
        self.addCleanup(sys.modules.pop, "loom_session.tool")
        sys.modules.pop("demo_stop", None)
        import demo_stop
        self.addCleanup(sys.modules.pop, "demo_stop")
        demo_stop.chord_moments = lambda ctx, spelling: []

        class Ctx:
            inputs = {"link": "workshop"}

            def __init__(self, answer):
                self.answer, self.asked = answer, []

            def ask(self, role, shape, fields, **kwargs):
                self.asked.append(shape)
                if shape == "InputSessionRequested":
                    return {"session": 1}
                if shape == "WorkshopQuitRequested":
                    return self.answer()
                return {}

            def on_cleanup(self, fn, why):
                pass

            def check(self, ok, why):
                if not ok:
                    raise ValueError(why)

        def refused():
            raise ValueError("The View Builder has an unsaved view. Save it before quitting.")
        with self.assertRaisesRegex(ValueError, "Workshop stays open: The View Builder has an unsaved view"):
            demo_stop.run(Ctx(refused))
        accepted = Ctx(lambda: {})
        self.assertIn("Quit accepted", demo_stop.run(accepted))
        self.assertEqual(accepted.asked, ["InputSessionRequested", "InjectInput", "WorkshopQuitRequested"])

    def test_a_wait_that_runs_out_names_what_it_waited_for(self):
        with self.assertRaisesRegex(RuntimeError, "Workshop closing its link did not happen within 0.2s"):
            launcher.wait_for(lambda: False, 0.2, "Workshop closing its link")

    # ---- the prepared revision ------------------------------------------------------------------------------
    def test_reset_prepares_the_revision_it_loaded_not_a_later_edit(self):
        root = make(self.tmp, panes=[("zengine.inventory-pane", "inventory"), ("zengine.composer", "compose"),
                                     ("zengine.demo", "controls")])
        owner, state, setup = Owner(), {"fixtures": []}, described.Setup(root)
        prepare(owner, setup, state, "workshop")
        first = json.dumps(owner.desk)
        desk = json.loads((root / "desk.json").read_text())
        desk["fields"]["panes"] = [p for p in desk["fields"]["panes"] if p["provider"] != "zengine.composer"]
        desk["fields"]["panes"][0]["place"]["x"] = "960"
        (root / "desk.json").write_text(json.dumps(desk))
        owner.calls.clear()
        prepare(owner, setup, state, "workshop")
        self.assertEqual(json.dumps(owner.desk), first)
        self.assertIn(("zengine.composer", "PaneResetRequested"), [(r, s) for r, s, _ in owner.calls])
        self.assertNotEqual(described.Setup(root).digest(), setup.digest())

    def test_a_toolbox_changed_before_it_is_read_is_refused_before_any_request(self):
        for change in ("edit", "remove"):
            owner, state = Owner(), {"fixtures": []}
            owner.toolbox = [("Run", "")]  # what Inventory would restore if the file were read anyway
            setup = self.toolbox_setup()
            box = Path(self.tmp) / "box.toolbox"
            if change == "edit":
                box.write_bytes(b"changed")
            else:
                box.unlink()
            with self.assertRaisesRegex(ValueError, "box.toolbox changed since this setup was loaded"):
                prepare(owner, setup, state, "workshop")
            self.assertEqual((owner.calls, state["reached"]), ([], "revision"))
            shutil.rmtree(setup.root)

    # ---- export -------------------------------------------------------------------------------------------
    def test_an_exported_setup_is_one_directory_that_describes_the_same_desk(self):
        setup = described.load("workbench")
        copy = setup.export(Path(self.tmp) / "copied")
        for name, path in copy.assets():
            self.assertTrue(path.exists(), name)
            self.assertTrue(str(path).startswith(str(copy.root)), name)
        self.assertEqual(copy.desk(), setup.desk())
        self.assertEqual(copy.hotkeys(), setup.hotkeys())
        with self.assertRaises(described.SetupError):
            setup.export(Path(self.tmp) / "copied")

    def shared_setup(self, files, **more):
        tmp = Path(self.tmp)
        (tmp / "shared").mkdir(exist_ok=True)
        (tmp / "shared/code.cpp").write_text("shared source", encoding="utf-8")
        (tmp / "recipes.json").write_text("{}", encoding="utf-8")
        root = make(tmp, project={"recipes": "../recipes.json", "files": files}, **more)
        (root / "shared").mkdir()
        (root / "shared/code.cpp").write_text("local source", encoding="utf-8")
        return root

    def exported_bytes(self, root, where):
        setup = described.Setup(root)
        copy = setup.export(Path(self.tmp) / where)
        read = lambda s: dict((k, s.asset(v).read_bytes()) for k, v in s.get("project")["files"].items())
        self.assertEqual(read(copy), read(setup))
        return copy

    def test_export_keeps_distinct_sources_apart_and_one_source_in_one_place(self):
        root = self.shared_setup({"first.cpp": "../shared/code.cpp", "second.cpp": "shared/code.cpp",
                                  "again.cpp": "../shared/code.cpp"})
        files = self.exported_bytes(root, "copy").get("project")["files"]
        self.assertEqual(files["second.cpp"], "shared/code.cpp")
        self.assertEqual(files["first.cpp"], files["again.cpp"])
        self.assertNotEqual(files["first.cpp"], files["second.cpp"])

    def test_export_moves_a_name_a_file_or_a_case_variant_already_holds(self):
        root = self.shared_setup({"first.cpp": "../shared/code.cpp", "second.cpp": "Shared/Code.cpp",
                                  "third.cpp": "../lib/code.cpp"})
        (root / "shared").rename(root / "Shared")
        (root / "Shared/code.cpp").rename(root / "Shared/Code.cpp")
        (Path(self.tmp) / "lib").mkdir()
        (Path(self.tmp) / "lib/code.cpp").write_text("lib source", encoding="utf-8")
        (root / "README.md").rename(root / "lib")  # a file where the flattened directory would go
        data = json.loads((root / "setup.json").read_text())
        data["guide"] = "lib"
        (root / "setup.json").write_text(json.dumps(data))
        copy = self.exported_bytes(root, "copy")
        files = copy.get("project")["files"]
        self.assertEqual((copy.get("guide"), files["second.cpp"]), ("lib", "Shared/Code.cpp"))
        self.assertEqual((files["first.cpp"], files["third.cpp"]), ("shared-2/code.cpp", "lib-2/code.cpp"))

    def test_an_export_that_fails_leaves_nothing_that_looks_like_a_setup(self):
        root = self.shared_setup({"first.cpp": "../shared/code.cpp", "second.cpp": "shared/code.cpp"})
        setup = described.Setup(root)
        real, calls = shutil.copy2, []

        def failing(*args, **kwargs):
            calls.append(args)
            if len(calls) == 2:
                raise OSError("disk full")
            return real(*args, **kwargs)
        shutil.copy2 = failing
        try:
            with self.assertRaisesRegex(OSError, "disk full"):
                setup.export(Path(self.tmp) / "copy")
        finally:
            shutil.copy2 = real
        self.assertFalse((Path(self.tmp) / "copy").exists())
        self.assertFalse((Path(self.tmp) / "copy.partial").exists())
        (Path(self.tmp) / "copy.partial").mkdir()
        with self.assertRaisesRegex(described.SetupError, "left from an export that did not finish"):
            setup.export(Path(self.tmp) / "copy")

    # ---- project files --------------------------------------------------------------------------------------
    def test_project_files_reach_nested_and_flat_targets_inside_the_project(self):
        (Path(self.tmp) / "rules.hpp").write_text("rules", encoding="utf-8")
        setup = described.Setup(self.shared_setup({"src/game/code.cpp": "../shared/code.cpp",
                                                   "src/game/rules.hpp": "../rules.hpp",
                                                   "local.cpp": "shared/code.cpp"}))
        instance = Path(self.tmp) / "instance"
        instance.mkdir()
        project = launcher.make_project(types.SimpleNamespace(zengine_prefix=""), setup, instance, instance, instance)
        self.assertEqual((project / "src/game/code.cpp").read_text(), "shared source")
        self.assertEqual((project / "src/game/rules.hpp").read_text(), "rules")
        self.assertEqual((project / "local.cpp").read_text(), "local source")
        self.assertTrue((project / "build-recipes.json").is_file())

    def test_project_targets_that_leave_the_project_are_refused_with_the_description(self):
        root = self.shared_setup({"../out.cpp": "shared/code.cpp", "/abs.cpp": "shared/code.cpp",
                                  "C:/x.cpp": "shared/code.cpp", "build-recipes.json": "shared/code.cpp",
                                  "src": "shared/code.cpp", "SRC/a.cpp": "shared/code.cpp", "dir": "shared"})
        with self.assertRaises(described.SetupError) as refused:
            described.Setup(root)
        said = str(refused.exception)
        for words in ("'../out.cpp' must be a relative path inside the project", "'/abs.cpp' must",
                      "'C:/x.cpp' must", "is the launcher's own build-recipes.json",
                      "'src' and 'SRC/a.cpp' would land on one path", "'dir' names a directory"):
            self.assertIn(words, said)


if __name__ == "__main__":
    unittest.main()
