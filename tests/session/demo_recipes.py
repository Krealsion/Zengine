# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Recipe ownership and failure checks, without a window or another host."""
import base64
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "external-host/tools/workshop"))
from demo_setup import prepare, layout, Measured


class Owner:
    def __init__(self):
        self.entries = []
        self.calls = []
        self.next_id = 0
        self.fail = None

    def check(self, ok, why):
        if not ok:
            raise ValueError(why)

    def ask(self, role, shape, fields, **kwargs):
        self.calls.append((role, shape, fields))
        if self.fail == shape:
            raise ValueError(role + ": deliberately refused " + shape)
        if shape == "PaneViewRequested":
            return {"rows": ["ready"]}
        if shape == "InventoryList":
            return {"entries": deepcopy(self.entries)}
        if shape in ("InventoryCaptureAdd", "InventoryAdd"):
            self.next_id += 1
            item = dict(reference={"owner": "inventory", "entry": str(self.next_id)}, revision=1,
                        label=fields["label"], pair=fields.get("pair", b"original"))
            self.entries.append(item)
            return deepcopy(item)
        if shape in ("InventoryWrite", "InventoryRename"):
            item = next(e for e in self.entries if e["reference"] == fields["reference"])
            self.check(item["revision"] == fields["revision"], "stale revision")
            item.update({k: v for k, v in fields.items() if k != "revision"})
            item["revision"] += 1
            return deepcopy(item)
        return {}


class Recipes(unittest.TestCase):
    def test_editor_materials_resets_inventory_applies_its_desk_and_creates_no_values(self):
        owner, state = Owner(), {"fixtures": []}
        prepare(owner, "editor-materials", state, "workshop")
        resets = [(role, fields["pane"]) for role, shape, fields in owner.calls if shape == "PaneResetRequested"]
        self.assertEqual(resets, [("zengine.inventory-pane", "inventory")])
        self.assertEqual(owner.entries, [])
        panes = [(p["provider"], p["pane"]) for p in layout("editor-materials")["fields"]["panes"]]
        self.assertEqual(panes, [("zengine.editor", "editor"), ("zengine.inventory-pane", "inventory"),
                                 ("zengine.files", "project-files"), ("zengine.terminal", "terminal"),
                                 ("zengine.demo", "controls")])

    def test_warm_reset_restores_owned_values_and_labels_but_preserves_user_copies(self):
        owner, state = Owner(), {"fixtures": []}
        prepare(owner, "values", state, "workshop")
        owner.entries[0].update(pair=b"changed", label="renamed")
        owner.entries[0]["revision"] += 1
        copy = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"mine", "label": "my work"})
        prepare(owner, "values", state, "workshop")
        self.assertEqual(len(owner.entries), 10)
        self.assertEqual(owner.entries[-1], copy)
        self.assertEqual(owner.entries[0]["pair"], b"original")
        self.assertEqual(owner.entries[0]["label"], "Demo input 1")
        self.assertFalse(any(shape == "InputSessionRequested" for _, shape, _ in owner.calls))
        owner.calls.clear()
        prepare(owner, "values", state, "workshop")
        self.assertFalse(any(shape == "InventoryWrite" for _, shape, _ in owner.calls))

    def test_missing_owned_entry_is_recreated_without_reusing_its_reference(self):
        owner, state = Owner(), {"fixtures": []}
        prepare(owner, "commands", state, "workshop")
        removed = owner.entries.pop(0)
        prepare(owner, "commands", state, "workshop")
        self.assertEqual(len(owner.entries), 9)
        self.assertNotEqual(state["fixtures"][0]["reference"], removed["reference"])
        self.assertEqual(owner.entries[-1]["pair"], removed["pair"])

    def test_partial_first_prepare_can_finish_and_restore_earlier_fixtures(self):
        owner, state = Owner(), {"fixtures": []}
        first = owner.ask("zengine.inventory", "InventoryCaptureAdd", {"label": "Demo input 1"})
        state["fixtures"].append(dict(reference=first["reference"], label=first["label"],
                                      pair=base64.b64encode(first["pair"]).decode()))
        owner.entries[0]["pair"] = b"edited during recovery"
        prepare(owner, "values", state, "workshop")
        self.assertEqual(len(state["fixtures"]), 9)
        self.assertEqual(owner.entries[0]["pair"], b"original")

    def test_owner_refusal_does_not_claim_ready_or_change_inventory(self):
        owner, state = Owner(), {"fixtures": []}
        owner.fail = "PaneResetRequested"
        measured = Measured(owner)
        with self.assertRaisesRegex(ValueError, "zengine.inventory-pane"):
            prepare(measured, "values", state, "workshop")
        self.assertEqual(owner.entries, [])
        self.assertEqual(measured.outcomes["failed_or_unanswered"], 1)
        self.assertEqual(sum(measured.calls.values()), 1)

    def test_presets_reset_both_editors_and_preserve_nonfixture_presets(self):
        owner, state = Owner(), {"fixtures": []}
        prepare(owner, "presets", state, "workshop")
        saved = owner.ask("zengine.inventory", "InventoryAdd", {"pair": b"partial", "label": "my preset"})
        owner.calls.clear()
        prepare(owner, "presets", state, "workshop")
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


if __name__ == "__main__":
    unittest.main()
