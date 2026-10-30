# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/inventory-list -- every entry Inventory holds, by message alone: each one's label, its
folder as the path of folder names from Root, its schema and version, its revision and its
reference, as `entries.json`. With `read=<label>`, the one entry under that label read back whole,
its pair kept as `entry.pair.bin` beside `entry.json`. It changes nothing in Inventory and opens no
input session.

A pair is Loom's native binary envelope (inventory/docs/inventory.md): this tool keeps it as the
bytes Inventory answered, which the installed `zengine-inventory-read` program decodes; nothing here
decodes it. A label two entries share, or none holds, is refused for `read`, naming which."""
import json


def folder_path(folders, folder):
    """`Work/Done` for a folder id, `""` for Root; a parent missing from the listing ends it."""
    names, seen = [], set()
    while folder and folder in folders and folder not in seen:
        seen.add(folder)
        names.append(folders[folder]["name"])
        folder = folders[folder]["parent"]
    return "/".join(reversed(names))


def written(ctx, name, value):
    ctx.produce(name, (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8"))


def run(ctx):
    link = ctx.inputs["link"]
    ctx.step("list")
    listed = ctx.ask("zengine.inventory", "InventoryList", {}, via=link, version=2)
    folders = dict((f["folder"]["folder"], f) for f in listed.get("folders") or [])
    entries = [{"label": e["label"], "folder": folder_path(folders, e["folder"]),
                "folder_id": e["folder"], "schema": e["schema"], "version": e["version"],
                "revision": e["revision"], "reference": dict(e["reference"]),
                "capture_slot": bool(e.get("capture_slot"))}
               for e in listed.get("entries") or []]
    written(ctx, "entries.json", {"owner": listed.get("owner", ""),
                                  "revision": listed.get("revision", 0), "entries": entries})
    places = {}
    for e in entries:
        places[e["folder"] or "Root"] = places.get(e["folder"] or "Root", 0) + 1
    said = "%d entries in Inventory at its revision %d (%s); entries.json" % (
        len(entries), listed.get("revision", 0),
        ", ".join("%d in %s" % (n, where) for where, n in sorted(places.items())) or "none")
    label = ctx.inputs.get("read") or ""
    if not label:
        return said
    held = [e for e in entries if e["label"] == label]
    ctx.check(held, "no entry is labelled %r; entries.json lists every label" % label)
    ctx.check(len(held) == 1, "%d entries are labelled %r, in %s; read names one label one entry "
              "holds" % (len(held), label, ", ".join(e["folder"] or "Root" for e in held)))
    ctx.step("read " + label)
    got = ctx.ask("zengine.inventory", "InventoryRead", {"reference": held[0]["reference"]},
                  via=link)
    ctx.produce("entry.pair.bin", bytes(got["pair"]))
    written(ctx, "entry.json", dict(held[0], revision=got["revision"]))
    return "%s; read %r (%s, %d bytes) as entry.pair.bin, which zengine-inventory-read decodes" % (
        said, label, held[0]["schema"], len(got["pair"]))
