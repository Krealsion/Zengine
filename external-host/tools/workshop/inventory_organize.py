# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/inventory-organize -- name stored entries and file them in a named folder through the
Inventory owner's own operations (list, read, create a folder, rename, file), with no input
session. loom-tool.json describes the plan.

FINDING AN ENTRY. Entries stored from one Compose form share a label and a schema, so a rule finds
its entry by label (and schema, when given) together with `contains`: text that must occur in the
entry's stored bytes -- for a command, a field value such as `wave`. Every rule must match exactly
one entry and no two rules the same entry, or the run stops before changing anything. It is a
byte search over the self-describing pair, not a decoder: choose text no other entry holds.

WHAT IT CHANGES. The folder is created under Root when no Root folder has that name. Each entry
is renamed at the revision it was listed with and filed into the folder; an entry changed since
the listing is refused by the owner, and the run says which. Nothing is executed and no authority
is granted: a stored command still needs its invoker's own permission to run."""
import json


def run(ctx):
    link = ctx.inputs["link"]
    plan = json.loads(ctx.inputs["plan"])
    ctx.check(isinstance(plan, dict) and isinstance(plan.get("entries"), list),
              "plan is {\"folder\": name, \"entries\": [{\"find\": {...}, \"label\": new label}]}")

    def ask(shape, fields, **kw):
        return ctx.ask("zengine.inventory", shape, fields, via=link, **kw)

    ctx.step("list")
    listed = ask("InventoryList", {}, version=2)
    owner, entries = listed["owner"], listed["entries"]
    chosen = []
    for rule in plan["entries"]:
        find = rule.get("find", {})
        found = [e for e in entries if e["label"] == find.get("label")
                 and (not find.get("schema") or e["schema"] == find["schema"])]
        if find.get("contains"):
            wanted = find["contains"].encode("utf-8")
            found = [e for e in found
                     if wanted in bytes(ask("InventoryRead", {"reference": e["reference"]})["pair"])]
        ctx.check(len(found) == 1, "rule %s matches %d entries; it must match exactly one"
                  % (json.dumps(find), len(found)))
        ctx.check(all(found[0]["reference"] != c["reference"] for c, _ in chosen),
                  "two rules found the same entry (%s)" % json.dumps(find))
        chosen.append((found[0], rule))

    target = ""
    name = plan.get("folder", "")
    if name:
        ctx.step("folder " + name)
        same = [f for f in listed["folders"] if f["name"] == name and f["parent"] == ""]
        if not same:
            ask("InventoryFolderCreate", {"parent": {"owner": owner, "folder": ""}, "name": name})
            same = [f for f in ask("InventoryList", {}, version=2)["folders"]
                    if f["name"] == name and f["parent"] == ""]
            ctx.check(len(same) == 1, "the owner did not list one new Root folder named %r" % name)
        target = same[0]["folder"]["folder"]

    for entry, rule in chosen:
        ctx.step("entry " + rule.get("label", entry["label"]))
        if rule.get("label") and rule["label"] != entry["label"]:
            ask("InventoryRename", {"reference": entry["reference"], "revision": entry["revision"],
                                    "label": rule["label"]})
        if name and entry["folder"] != target:
            ask("InventoryFile", {"reference": entry["reference"],
                                  "from": {"owner": owner, "folder": entry["folder"]},
                                  "into": {"owner": owner, "folder": target}})

    ctx.step("read back")
    after = ask("InventoryList", {}, version=2)
    names = dict((f["folder"]["folder"], f["name"]) for f in after["folders"])
    kept = []
    for entry, rule in chosen:
        now = [e for e in after["entries"] if e["reference"] == entry["reference"]]
        ctx.check(now, "entry %s left the collection" % rule.get("label"))
        kept.append({"label": now[0]["label"], "schema": now[0]["schema"],
                     "folder": names.get(now[0]["folder"], "Root"), "revision": now[0]["revision"]})
        ctx.check(now[0]["label"] == rule.get("label", entry["label"]) and now[0]["folder"] == target,
                  "entry %s reads %r in %r after the changes" % (rule.get("label"), now[0]["label"],
                                                                names.get(now[0]["folder"], "Root")))
    ctx.produce("organize.json", json.dumps({"folder": name, "entries": kept}, indent=1).encode())
    return "%d entr%s named and filed in %s" % (len(kept), "y" if len(kept) == 1 else "ies",
                                               name or "Root")
