# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/inventory-controls -- make a portable Inventory view of named commands and give each a
target office and a key, through the Inventory pane's own configuration door (InventoryViewEdit),
with no input session. loom-tool.json describes the controls.

CONFIGURATION, NOT EXECUTION. Creating a view, moving entries into it, binding a key and enabling an
item run nothing. A key pressed later runs its command only while the item and the view's context
are both on, and only with the pressing actor's own permission for that office and shape: a
guest's injected key is judged by the guest's grant, a maker's hand by the maker's.

FINDING THE ENTRIES. Each control names an entry by its label inside `folder` (Root when empty);
a label must name exactly one entry there. The new view is the one the pane lists afterwards that
did not exist before; the run passes only when the view holds the entries in the order given and
every binding reads back with its target, key and enabled state."""
import json

from workshop_steps import chord

PANE = "zengine.inventory-pane"


def run(ctx):
    link = ctx.inputs["link"]
    controls = json.loads(ctx.inputs["controls"])
    ctx.check(isinstance(controls, list) and controls, "controls is a JSON list of {\"label\": .., \"key\": ..}")
    target = ctx.inputs.get("target", "")
    kind = ctx.inputs.get("kind", "row")

    def edit(**fields):
        # The gate refuses an ask that leaves out a declared field, so every one is written.
        none = {"owner": "", "entry": ""}
        whole = dict(operation="", view="", text="", entry=none, before=none, scancode=0,
                     modifiers=0, enabled=False)
        whole.update(fields)
        return ctx.ask(PANE, "InventoryViewEdit", whole, via=link)

    def views():
        return ctx.ask(PANE, "InventoryViewsRequested", {}, via=link)

    ctx.step("find the entries")
    listed = ctx.ask("zengine.inventory", "InventoryList", {}, via=link, version=2)
    names = dict((f["folder"]["folder"], f["name"]) for f in listed["folders"])
    folder = ctx.inputs.get("folder", "")
    home = "" if not folder else next((k for k, v in names.items() if v == folder), None)
    ctx.check(home is not None, "no folder named %r" % folder)
    refs = []
    for c in controls:
        found = [e for e in listed["entries"] if e["label"] == c["label"] and e["folder"] == home]
        ctx.check(len(found) == 1, "%d entries are labelled %r in %s" % (len(found), c["label"], folder or "Root"))
        refs.append(found[0]["reference"])
    keys = [chord(c["key"]) for c in controls]

    ctx.step("create a %s view" % kind)
    before = set(v["id"] for v in views()["views"])
    edit(operation="create", text=kind, entry=refs[0])
    made = [v for v in views()["views"] if v["id"] not in before]
    ctx.check(len(made) == 1, "the pane lists %d new views after one create" % len(made))
    view = made[0]["id"]
    for ref in refs[1:]:
        edit(operation="move", view=view, entry=ref)

    ctx.step("bind and enable")
    for ref, (code, mods) in zip(refs, keys):
        edit(operation="bind", entry=ref, text=target, scancode=code, modifiers=mods)
        edit(operation="enable", entry=ref, enabled=bool(ctx.inputs.get("enable", True)))
    edit(operation="context", view=view, enabled=bool(ctx.inputs.get("context", False)))

    ctx.step("read back")
    after = views()
    mine = [v for v in after["views"] if v["id"] == view][0]
    ctx.check([r["entry"] for r in mine["entries"]] == [r["entry"] for r in refs],
              "the view holds its entries in another order, or others")
    bound = []
    for c, ref, (code, mods) in zip(controls, refs, keys):
        b = [x for x in after["bindings"] if x["reference"]["entry"] == ref["entry"]]
        ctx.check(b and b[0]["target"] == target and b[0]["scancode"] == code and b[0]["modifiers"] == mods,
                  "the binding of %r does not read back as %s -> %s" % (c["label"], c["key"], target))
        bound.append({"label": c["label"], "key": c["key"], "target": target, "enabled": b[0]["enabled"]})
    result = {"view": view, "kind": kind, "context": mine["active"], "controls": bound}
    ctx.produce("controls.json", json.dumps(result, indent=1).encode())
    return "view %s: %d control(s) for %s, context %s" % (view, len(bound), target or "(none)",
                                                         "ON" if mine["active"] else "OFF")
