# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Setup descriptions and their preparation's ownership and failure checks, without a window or
another host."""
import base64
from copy import deepcopy
import json
from pathlib import Path
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
from demo_setup import prepare, layout, Measured  # noqa: E402

CANNOT = "setup names a pane this Workshop cannot present: "


class Owner:
    """Workshop's owners as the service meets them: answers, refusals and the calls made."""
    def __init__(self):
        self.entries, self.calls, self.next_id, self.fail = [], [], 0, None
        self.toolbox, self.views, self.bindings = [], [], []
        self.held = set()        # providers Workshop can present beyond its own
        self.refuse_enable = None
        self.desk = None

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
            return {"rows": ["ready"]}
        if shape == "InventoryList":
            return {"owner": "inventory", "entries": deepcopy(self.entries), "folders": []}
        if shape in ("InventoryCaptureAdd", "InventoryAdd"):
            return self.entry(fields, folder=(fields.get("folder") or {}).get("folder", ""))
        if shape == "InventoryRead":
            return {"pair": next(e for e in self.entries if e["reference"] == fields["reference"])["pair"]}
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
            return {"views": deepcopy(self.views), "bindings": deepcopy(self.bindings), "inventory_active": False}
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
        op, ref = f["operation"], f["entry"]
        for v in self.views:
            if op in ("create", "move") and ref in v["entries"]:
                v["entries"].remove(ref)
        if op == "create":
            self.views.append({"id": "inventory.%d" % (len(self.views) + 1), "kind": f["text"],
                               "active": False, "entries": [ref]})
        elif op == "move":
            next(v for v in self.views if v["id"] == f["view"])["entries"].append(ref)
        elif op == "bind":
            self.bindings = [b for b in self.bindings if b["reference"] != ref]
            self.bindings.append({"reference": ref, "target": f["text"], "scancode": f["scancode"],
                                  "modifiers": f["modifiers"], "enabled": False})
        elif op == "enable":
            if self.refuse_enable:
                raise ValueError(self.refuse_enable)
            next(b for b in self.bindings if b["reference"] == ref)["enabled"] = f["enabled"]
        elif op == "context":
            next(v for v in self.views if v["id"] == f["view"])["active"] = f["enabled"]
        return {}

    def shapes(self, shape):
        return [fields for _, s, fields in self.calls if s == shape]


def make(tmp, name="made", **fields):
    """A small described setup in `tmp`, desk included."""
    root = Path(tmp) / name
    root.mkdir()
    desk = {"zen": 1, "schema": "WorkshopSetup", "version": 3, "fields": {
        "format": "zengine-workshop-setup", "format_version": "3", "name": name, "panes": [
            {"provider": p, "pane": k, "place": {"mode": "subcells", "x": "48", "y": "96"},
             "width": {"mode": "subcells", "amount": "480"}, "height": {"mode": "subcells", "amount": "480"},
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
                                 ("zengine.demo", "controls")])

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
                         ["inventory", "info", "controls"])
        self.assertEqual([p["pane"] for p in layout("commands")["fields"]["panes"]],
                         ["inventory", "loaded", "compose", "controls"])
        with self.assertRaises(ValueError):
            layout("value")

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

    def test_a_removed_hotkey_entry_comes_back_into_its_view_with_its_binding(self):
        owner, state = Owner(), {"fixtures": []}
        owner.toolbox = [("Run", "")]
        setup = self.toolbox_setup()
        prepare(owner, setup, state, "workshop")
        owner.entries.clear()
        owner.views[0]["entries"].clear()
        prepare(owner, setup, state, "workshop")
        (entry,) = owner.entries
        self.assertEqual(owner.views[0]["entries"], [entry["reference"]])
        self.assertEqual(len(owner.views), 1)
        self.assertTrue(any(b["reference"] == entry["reference"] and b["enabled"] for b in owner.bindings))

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

    def test_a_pane_the_setup_cannot_provide_is_named_in_the_failure(self):
        owner, state = Owner(), {"fixtures": []}
        with self.assertRaisesRegex(ValueError, "cannot present: td.game td; this setup does not prepare td.game"):
            prepare(owner, self.provider_setup(providers=False), state, "workshop")

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


if __name__ == "__main__":
    unittest.main()
