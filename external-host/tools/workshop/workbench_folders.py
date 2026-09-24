# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The organized inspection workbench: file the packaged workbench into named folders, browse
them to reuse its entries, and measure a retrieval in a flat and an organized toolbox.

Zengine owns folders, membership, navigation, hit testing and authority. Every step here reads
the owner's current listing by name and acts through visible controls; no folder or entry id is
remembered between runs. docs/workshop/inventory-folders.md is the guide."""
import itertools
import re
import time
from workshop_steps import picture, moment, chord_moments

INV = ("zengine.inventory-pane", "inventory")
ROW_VIEW = "row"
LOCATION = ("[Up]", "(Up)")
FOLDERS = {"sample": ["Workbench", "Samples"], "note": ["Workbench", "Samples"],
           "command": ["Workbench", "Commands"], "preset": ["Workbench", "Commands", "Drafts"]}
EMPTY = {"owner": "", "entry": ""}


def organization(hand):
    """What the owner lists now: every entry with its folder, and every folder by id."""
    listed = hand.ask("zengine.inventory", "InventoryList", {}, version=2)
    return listed, {f["folder"]["folder"]: f for f in listed["folders"]}


def names(folders, folder):
    """The folder names from the root down to `folder`."""
    out = []
    while folder:
        out.insert(0, folders[folder]["name"])
        folder = folders[folder]["parent"]
    return out


def entry(hand, label):
    listed, folders = organization(hand)
    found = [e for e in listed["entries"] if e["label"] == label]
    hand.ctx.check(len(found) == 1, "expected exactly one entry named " + label)
    return found[0], names(folders, found[0]["folder"])


def location(hand):
    """Inventory's location row, or empty for a collection without folders."""
    return next((r["text"] for r in hand.view(*INV)["rows"] if r["text"].startswith(LOCATION)), "")


def folder_row(hand, name):
    return hand.row(*INV, name + "/  (", scroll=True)


def to_root(hand):
    hand.click(hand.view(*INV)["rows"][0])  # the heading: Inventory takes the keys
    hand.key("alt+home")


def opened(hand, name):
    """Is `name` the folder shown? The location row ends with it, after an optional count of
    members placed in portable views."""
    text = re.sub(r"\s+\+\d+( in views)?$", "", location(hand).rstrip())
    return text.endswith(" " + name)


def open_child(hand, name):
    """Open a folder shown here the maker's way: a press selects its row, a second press on it
    opens it."""
    hand.click(folder_row(hand, name))
    hand.click(folder_row(hand, name))
    hand.ctx.check(opened(hand, name), "did not open %s: %s" % (name, location(hand)))


def open_path(hand, path):
    """From the root, open each folder in turn."""
    to_root(hand)
    for name in path:
        open_child(hand, name)


def retrieve(hand, label, view):
    """Bring a stored entry into an Info view the way a maker finds it: through its folders when
    it has any, otherwise by scrolling the flat list. Returns the folders it passed through."""
    _, path = entry(hand, label)
    if path:
        open_path(hand, path)
    hand.drag(hand.row(*INV, label, scroll=True), hand.view(*view)["rows"][0], 350)
    return path


def organize(ctx, hand, labels, out, pictures, shots, link):
    """File the restored flat workbench into named folders with the maker's own gestures, pop the
    command into a row with an explicit hotkey (restores leave both OFF), and save the result."""
    listed, folders = organization(hand)
    ctx.check(not folders and all(not e["folder"] for e in listed["entries"]),
              "restore the flat workbench first; organize starts from its root")
    ctx.step("create Workbench/Samples and Workbench/Commands/Drafts from the keyboard")
    hand.click(hand.view(*INV)["rows"][0])

    def create(name):
        hand.key("ctrl+d"); hand.text(name); hand.key("enter")
        folder_row(hand, name)

    create("Workbench"); hand.key("enter")  # a new folder is selected where it was made
    create("Samples"); create("Commands"); hand.key("enter")
    create("Drafts")
    ctx.step("file entries by dropping them on folder rows")
    to_root(hand)
    for key in ("sample", "note", "command", "preset"):
        hand.drag(hand.row(*INV, labels[key], scroll=True), folder_row(hand, "Workbench"), 350)
    open_path(hand, ["Workbench"])
    for key in ("sample", "note"):
        hand.drag(hand.row(*INV, labels[key]), folder_row(hand, "Samples"), 350)
    hand.drag(hand.row(*INV, labels["command"]), folder_row(hand, "Commands"), 350)
    ctx.step("a distant destination: pick the preset, open Commands/Drafts, move it there")
    hand.click(hand.row(*INV, labels["preset"])); hand.key("ctrl+x")
    open_path(hand, FOLDERS["preset"])
    hand.key("ctrl+v")
    for key, path in FOLDERS.items():
        ctx.check(entry(hand, labels[key])[1] == path, "%s is not in %s" % (labels[key], "/".join(path)))
    ctx.step("pop the command into a row with an explicit hotkey; configuration runs nothing")
    command, _ = entry(hand, labels["command"])
    edit = {"operation": "create", "view": "", "text": ROW_VIEW, "entry": command["reference"], "before": EMPTY,
            "scancode": 0, "modifiers": 0, "enabled": False}
    hand.ask("zengine.inventory-pane", "InventoryViewEdit", edit, settle=True)
    edit.update(operation="bind", text="zengine.inventory", scancode=30, modifiers=4)  # alt+1
    hand.ask("zengine.inventory-pane", "InventoryViewEdit", edit, settle=True)
    seat_views(hand)
    open_path(hand, ["Workbench"])
    pictures.append(picture(shots, link, "organized")[0])
    ctx.step("save the organized toolbox")
    return hand.ask("zengine.inventory-pane", "InventoryToolboxSave", {"path": out}, settle=True)


def seat_views(hand):
    """Put the portable views that hold entries on the organized desk (Workshop refuses a setup
    naming a view that does not exist yet, so the demo's own layout cannot name them)."""
    from demo_setup import layout
    import json
    views = [v["id"] for v in hand.ask("zengine.inventory-pane", "InventoryViewsRequested", {})["views"] if v["entries"]]
    hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(layout("folders", views))}, settle=True)
    return views


def placement(hand, reference):
    views = hand.ask("zengine.inventory-pane", "InventoryViewsRequested", {})
    placed = [v["id"] for v in views["views"] if reference in v["entries"]]
    binding = next((b for b in views["bindings"] if b["reference"] == reference), None)
    return placed, binding


def story(ctx, hand, labels, views, pictures, shots, link):
    """The maker story in an organized workbench, reactive to whatever organization is stored."""
    sample_view, preset_view = views
    results = sum(e["label"] == "Workbench result" for e in organization(hand)[0]["entries"])
    command, _ = entry(hand, labels["command"])
    placed, binding = placement(hand, command["reference"])
    ctx.check(len(placed) == 1 and binding is not None and not binding["enabled"],
              "the command should sit in one portable view with its hotkey OFF")
    seat_views(hand)

    ctx.step("drill down to the sample and keep it in the Sample view")
    started = time.monotonic()
    path = retrieve(hand, labels["sample"], sample_view)
    ctx.check(any("COPY zen.PokeStructure" in r["text"] for r in hand.view(*sample_view)["rows"]),
              "the sample did not arrive in the Sample view")
    retrieval_ms = (time.monotonic() - started) * 1000
    pictures.append(picture(shots, link, "drilled")[0])

    ctx.step("climb to the preset's folder by a crumb and link it in the Preset view")
    _, preset_path = entry(hand, labels["preset"])
    shared = [n for n, _ in itertools.takewhile(lambda pair: pair[0] == pair[1], zip(path, preset_path))]
    if shared and shared != path:
        hand.click(hand.spot(*INV, shared[-1], LOCATION[0]))  # an ancestor crumb
        for name in preset_path[len(shared):]:
            open_child(hand, name)
    else:
        open_path(hand, preset_path)
    hand.click(hand.row(*INV, labels["preset"])); hand.key("ctrl+enter")
    hand.click(hand.view(*preset_view)["rows"][3])
    ctx.check(any("PRESET LINKED" in r["text"] for r in hand.view(*preset_view)["rows"]), "the preset is not linked")

    ctx.step("fill the preset's target from the sample, field to field, and save")
    hand.field(*preset_view, "target_role")
    hand.drag(hand.field(*sample_view, "meta[0].requested_role"), hand.field(*preset_view, "target_role"), 500)
    hand.control(*preset_view, "Save")
    ctx.check(any("saved rev" in r["text"] or "Saved" in r["text"] for r in hand.view(*preset_view)["rows"]),
              "the filled preset was not saved")
    pictures.append(picture(shots, link, "filled-organized")[0])

    ctx.step("reorganize while the command sits in its row: rename its folder and move Drafts")
    _, command_path = entry(hand, labels["command"])
    folder = command_path[-1]
    renamed = "Command sets" if folder == "Commands" else "Commands"
    open_path(hand, command_path[:-1])
    hand.click(folder_row(hand, folder)); hand.key("ctrl+n"); hand.text(renamed); hand.key("enter")
    folder_row(hand, renamed)
    _, preset_path = entry(hand, labels["preset"])
    drafts_parent, home = preset_path[:-1], command_path[:-1]
    # Alternate between the command's folder and its parent, so every run moves Drafts once.
    target = home + [renamed] if drafts_parent == home else home
    open_path(hand, drafts_parent)
    hand.click(folder_row(hand, preset_path[-1])); hand.key("ctrl+x")
    open_path(hand, target)
    hand.key("ctrl+v")
    ctx.check(entry(hand, labels["preset"])[1] == target + [preset_path[-1]], "Drafts did not move")
    now, _ = entry(hand, labels["command"])
    placed_now, binding_now = placement(hand, command["reference"])
    ctx.check(now["reference"] == command["reference"] and placed_now == placed and binding_now == binding,
              "reorganizing changed the command's identity, placement or hotkey")
    after = sum(e["label"] == "Workbench result" for e in organization(hand)[0]["entries"])
    ctx.check(after == results, "organizing or browsing ran the stored command")
    pictures.append(picture(shots, link, "reorganized")[0])
    return {"sample_path": path, "retrieval_ms": retrieval_ms, "command_folder": renamed,
            "drafts_in": "/".join(target), "command_view": placed_now, "command_binding": binding_now,
            "results_before": results, "results_after": after}


FOLDER_MENU = {"move": 2, "remove": 3}  # a folder row's menu order: Open, Rename, Move, Remove


def menus(ctx, hand, labels, pictures, shots, link):
    """While Info holds the keys, the folder menu choices that wait for a key take them -- Move
    then Escape, Remove empty folder then Delete -- and one ordinary filing, onto [Up] and back.
    Every change is undone, so the stored organization is as it was."""
    info = ("zengine.info", "info")

    def says(text):  # a narrow room cuts a notice's tail, so callers read its head
        return any(text in r["text"] for r in hand.view(*INV)["rows"])

    def type_in_info():
        hand.click(hand.view(*info)["rows"][0])

    def choose(row, index, before=None):
        hand.actions["menu"] += 1
        hand.inject([moment(hand.ctx, "PointerButton", button=3, pressed=p,
                            x=row["x"], y=row["y"], space=row["space"]) for p in (True, False)])
        hand.inject(chord_moments(hand.ctx, "down", repeat=index))
        if before:
            pictures.append(picture(shots, link, before)[0])
        hand.key("enter")

    ctx.step("make an empty Scratch folder at Root by keys, then type in Info")
    to_root(hand)
    hand.key("ctrl+d"); hand.text("Scratch"); hand.key("enter")
    folder_row(hand, "Scratch")
    type_in_info()
    ctx.step("Move to another folder... from Workbench's menu takes the keys; Escape cancels")
    choose(folder_row(hand, "Workbench"), FOLDER_MENU["move"], before="menu-open")
    ctx.check(says("[moving]"), "the menu did not pick Workbench to move")
    pictures.append(picture(shots, link, "moving")[0])
    hand.key("escape")
    ctx.check(not says("[moving]") and says("Move cancelled"), "Escape did not reach Inventory")
    ctx.step("Remove empty folder from Scratch's menu takes the keys; Delete removes it")
    type_in_info()
    choose(folder_row(hand, "Scratch"), FOLDER_MENU["remove"])
    ctx.check(says("Press Delete again to remove"), "the menu did not arm Scratch's removal")
    hand.key("delete")
    ctx.check(all(f["name"] != "Scratch" for f in organization(hand)[1].values()), "Delete did not reach Inventory")
    ctx.step("an ordinary filing: the note onto [Up], then back into its folder")
    _, path = entry(hand, labels["note"])
    ctx.check(len(path) >= 1, "the note should be filed in a folder; restore the organized workbench")
    open_path(hand, path)
    hand.drag(hand.row(*INV, labels["note"]), hand.spot(*INV, LOCATION[0], LOCATION[0]), 350)
    ctx.check(entry(hand, labels["note"])[1] == path[:-1] and says("Filed '" + labels["note"] + "'"),
              "the note was not filed in its folder's parent")
    pictures.append(picture(shots, link, "filed")[0])
    open_path(hand, path[:-1])
    hand.drag(hand.row(*INV, labels["note"]), folder_row(hand, path[-1]), 350)
    ctx.check(entry(hand, labels["note"])[1] == path, "the note did not go back")
    return {"note_path": path}
