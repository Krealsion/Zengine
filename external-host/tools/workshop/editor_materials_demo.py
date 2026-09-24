# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Editor materials: text, a command and a file place carried between the Editor and named
Inventory folders by a maker's own gestures, kept in a toolbox, and brought back in another
Workshop.

Zengine owns every meaning here -- what a press on the highlight is, what a drop inserts, where
a location opens, who may do each -- and every step reads the owners' current answers by name.
docs/workshop/editor.md#carrying-text-commands-and-file-places is the guide.

  prepare   pointer route (SDL): a timed linear sweep selects, a Bezier drag carries the copy
            into Snippets; right-click Extract; ctrl+l carries the place into Places; a real
            Terminal command, captured into Commands, drops back as generated C++ (undone) and
            as its Terminal line; the place refuses over unsaved work, then reopens its line;
            the toolbox is saved.
  keyboard  the terminal medium's route: ctrl+e and ctrl+l pick up, a click places; Enter picks
            up a stored copy, a click drops it into the Editor.
  retrieve  a fresh Workshop: the toolbox restored, the place reopened from the other root, the
            snippet dropped into a document.
  neovim    the Neovim-backed Editor holding the office (`demo.py start --neovim <program>`): a
            Visual selection dragged from its highlight along a Bezier, ctrl+r for a linewise
            one, and a stored copy dropped into Neovim as data and taken back with u.
"""
import json
from pathlib import Path
import time

from demo_setup import Measured
from hand import Hand
from workbench_folders import entry, folder_row, open_path, organization, to_root
from workshop_steps import link_session, moment, picture

EDITOR = ("zengine.editor", "editor")
INV = ("zengine.inventory-pane", "inventory")
TERM = ("zengine.terminal", "terminal")
FILES = ("zengine.files", "project-files")
FOLDERS = ("Snippets", "Commands", "Places")

BEAT = """#include <cstdio>

// A beat for the editor-materials demo.
int beats(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += i;
    }
    return total;
}

int main() { std::printf("%d\\n", beats(4)); }
"""
NOTES = """Notes for the editor-materials demo

Dropped here:
"""


def run(ctx):
    measured = Measured(ctx)
    link = ctx.inputs["link"]
    link_session(ctx, link)
    hand = Hand(measured, link)
    phase, label = ctx.inputs["phase"], ctx.inputs["label"]
    ctx.check(phase in ("prepare", "keyboard", "retrieve", "neovim"),
              "phase must be prepare, keyboard, retrieve or neovim")
    ctx.check(label.isascii() and label.isalnum() and len(label) <= 24, "use a short ASCII word for label")
    folder = Path(ctx.inputs["folder"])
    ctx.check(folder.is_absolute(), "folder must be an absolute directory on the Workshop host")
    box = ctx.inputs["toolbox"]
    beat, notes = (folder / "beat.cpp").as_posix(), (folder / "notes.txt").as_posix()
    shots, evidence = [], {"phase": phase, "label": label, "steps": []}

    def shot(name):
        shots.append(picture(ctx, link, name)[0]["frame"])

    def note(what, **facts):
        evidence["steps"].append(dict(what=what, **facts))

    def editor_view():
        return hand.view(*EDITOR)

    def editor_rows():
        return [r["text"] for r in editor_view()["rows"]]

    def says(text):
        return any(text in r for r in editor_rows())

    def cell(text, offset=0, occurrence=0):
        """Where Workshop paints the `offset`th character of the `occurrence`th painted `text`."""
        view = editor_view()
        found = [(r, r["text"].find(text)) for r in view["rows"] if text in r["text"]]
        ctx.check(len(found) > occurrence, "the Editor paints no %r: %s" % (text, [r["text"] for r in view["rows"]]))
        row, at = found[occurrence]
        return hand.point(*EDITOR, row["row"], at + offset, view["picture"])

    def open_source(path):
        """Open a file the way a maker does, from Files: it starts at this Workshop's project,
        where `folder` is; a press selects a row and Return enters a directory or opens a file.
        (The opening office answers an office's request, never a guest's anonymous one.)"""
        name = Path(path).name
        hand.control(*FILES, "look again")  # Files is a snapshot; this run may have made the files
        rows = [r for r in hand.view(*FILES)["rows"] if r["text"].strip().endswith(name)]
        if not rows:
            hand.click(hand.row(*FILES, folder.name + "/", scroll=True))
            hand.key("enter")
        hand.click(hand.row(*FILES, name, scroll=True))
        hand.key("enter")
        for _ in range(100):
            if says(name):
                return
            time.sleep(0.05)
        ctx.fail("the Editor does not show %s; Files shows %s" % (
            path, [r["text"] for r in hand.view(*FILES)["rows"]]))

    def typed(text):
        """Text into whatever holds the keys, without the clear `Hand.text` does first."""
        hand.inject([moment(measured, "TextEntered", text=text)])

    def name_new(name):
        hand.row(*INV, "Name:")
        hand.text(name)
        hand.key("enter")
        found, path = entry(hand, name)
        return found, path

    def right_click(where):
        hand.inject([moment(measured, "PointerButton", button=3, pressed=p,
                            x=where["x"], y=where["y"], space=where["space"]) for p in (True, False)])

    def pair_of(stored):
        return hand.ask("zengine.inventory", "InventoryRead", {"reference": stored["reference"]})["pair"]

    def status_row():
        return editor_view()["rows"][0]

    def waits(text, seconds=10):
        for _ in range(int(seconds / 0.05)):
            if says(text):
                return True
            time.sleep(0.05)
        ctx.fail("the Editor never showed %r: %s" % (text, editor_rows()))

    if phase in ("prepare", "keyboard", "neovim"):
        folder.mkdir(parents=True, exist_ok=True)
        for path, text in ((beat, BEAT), (notes, NOTES)):
            if not Path(path).exists():
                Path(path).write_text(text, encoding="ascii", newline="\n")
        listed, known = organization(hand)
        ctx.check(not any(e["label"].startswith(label) for e in listed["entries"]), "choose a fresh label")
        ctx.step("make the three folders from the keyboard, where they are missing")
        to_root(hand)
        present = {f["name"] for f in known.values() if not f["parent"]}
        for name in FOLDERS:
            if name not in present:
                hand.key("ctrl+d"); hand.text(name); hand.key("enter")
                folder_row(hand, name)
        note("folders", made=[n for n in FOLDERS if n not in present])

    if phase == "prepare":
        ctx.step("send a real Terminal command and capture it into Commands")
        hand.click(hand.row(*TERM, ">    Tab:"))
        hand.text("send @zengine.skin SurfaceText 1 slot=score text=" + label)
        hand.key("enter")
        sent = [r for r in hand.view(*TERM)["rows"] if r["text"].startswith("^ SurfaceText")]
        ctx.check(bool(sent), "the Terminal shows no submitted SurfaceText")
        to_root(hand)
        hand.drag(sent[-1], folder_row(hand, "Commands"), 400)
        command, path = name_new(label + " command")
        ctx.check(path == ["Commands"], "the command is not in Commands: %s" % path)
        note("command captured", folder=path, reference=command["reference"])

        ctx.step("a timed linear sweep selects; a Bezier drag from the highlight carries the copy")
        open_source(beat)
        start, end = cell("int total = 0;"), cell("int total = 0;", len("int total = 0;"))
        hand.drag(start, end, ctx.inputs["duration_ms"], 0)
        shot("selected")
        to_root(hand)
        motion = hand.drag(cell("int total = 0;", 4), folder_row(hand, "Snippets"),
                           ctx.inputs["duration_ms"], ctx.inputs["bend"],
                           during=lambda: shot("carrying"))
        snippet, path = name_new(label + " snippet")
        ctx.check(path == ["Snippets"], "the snippet is not in Snippets: %s" % path)
        ctx.check(b"int total = 0;" in pair_of(snippet), "the snippet does not hold the selected text")
        ctx.check(Path(beat).read_text(encoding="ascii") == BEAT, "carrying changed the file")
        note("selection carried", folder=path, motion=motion.fields, reference=snippet["reference"])

        ctx.step("right-click the highlight: Extract, then place it in Snippets")
        to_root(hand)  # before the pick-up: a press in Inventory while carrying places the copy
        hand.drag(cell("beats(4)"), cell("beats(4)", len("beats(4)")), 300, 0)
        right_click(cell("beats(4)", 2))
        hand.key("enter")  # the menu's first row: Extract selection to Inventory
        hand.click(folder_row(hand, "Snippets"))
        call, path = name_new(label + " call")
        ctx.check(path == ["Snippets"] and b"beats(4)" in pair_of(call), "Extract did not file the call")
        note("extracted by the menu", folder=path)

        ctx.step("ctrl+l carries this file's place into Places")
        hand.click(cell("for (int i", 0))
        hand.key("ctrl+l")
        hand.click(folder_row(hand, "Places"))
        place, path = name_new(label + " place")
        ctx.check(path == ["Places"] and beat.encode() in pair_of(place), "the place was not filed with its path")
        note("place carried", folder=path)
        shot("filed")

        ctx.step("the captured command drops in as C++ by choice, and one undo removes it")
        open_path(hand, ["Commands"])
        last = cell("int main()")
        hand.drag(hand.row(*INV, label + " command"), last, 400, ctx.inputs["bend"])
        hand.key("down"); hand.key("enter")  # Generate C++ that builds it
        ctx.check(says("make_surface_text_v1"), "no generated C++ was inserted")
        shot("generated")
        hand.click(status_row())
        hand.key("ctrl+z")
        ctx.check(not says("make_surface_text_v1"), "one undo did not remove the generated C++")
        ctx.check(Path(beat).read_text(encoding="ascii") == BEAT, "generation wrote the file")
        note("C++ generated and undone")

        ctx.step("in a text file the command is its Terminal line; the place refuses over unsaved work")
        open_source(notes)
        hand.drag(hand.row(*INV, label + " command"), cell("Dropped here:", len("Dropped here:")), 400, 0)
        ctx.check(says("send @zengine.skin SurfaceText 1"), "the Terminal line was not inserted")
        shot("terminal-line")
        to_root(hand)
        open_path(hand, ["Places"])
        hand.drag(hand.row(*INV, label + " place"), status_row(), 400, 0)
        ctx.check(says("notes.txt") and not says("int total"), "the place replaced unsaved work")
        refused = " ".join(editor_rows())
        hand.click(status_row())
        hand.key("ctrl+d")  # discard, deliberately
        hand.drag(hand.row(*INV, label + " place"), status_row(), 400, 0)
        ctx.check(says("for (int i") and says("beat.cpp"), "the place did not reopen beat.cpp")
        shot("reopened")
        note("place refused over unsaved work, then reopened", refused=refused, status=status_row()["text"])

        ctx.step("save the toolbox")
        saved = hand.ask("zengine.inventory-pane", "InventoryToolboxSave", {"path": box}, settle=True)
        note("toolbox saved", entries=saved["entries"], path=saved["path"])

    elif phase == "neovim":
        ctx.step("a Visual selection in Neovim, dragged from its highlight into Snippets along a Bezier")
        open_source(beat)
        hand.click(status_row())  # Neovim's keys; a press on the status row moves nothing in Neovim
        typed("5G0wvf;")  # `int total = 0;`, characterwise
        waits("VISUAL")
        to_root(hand)  # before the pick-up; Neovim keeps its selection meanwhile
        motion = hand.drag(cell("int total = 0;", 4), folder_row(hand, "Snippets"),
                           ctx.inputs["duration_ms"], ctx.inputs["bend"], during=lambda: shot("nvim-carrying"))
        snippet, path = name_new(label + " snippet")
        ctx.check(path == ["Snippets"] and b"int total = 0;" in pair_of(snippet), "Neovim's copy is wrong")
        note("Neovim selection carried", folder=path, motion=motion.fields)
        ctx.step("ctrl+r carries a linewise selection; a click places it")
        hand.click(status_row())
        hand.key("escape")
        typed("9GV")  # `    return total;`, linewise
        waits("V-LINE")
        hand.key("ctrl+r")
        hand.click(folder_row(hand, "Snippets"))
        lines, path = name_new(label + " lines")
        ctx.check(path == ["Snippets"] and b"return total;\n" in pair_of(lines), "the linewise copy is wrong")
        shot("nvim-lines")
        hand.click(status_row())
        hand.key("escape")
        ctx.step("a stored copy dropped into Neovim is data where it lands, and u takes it back")
        open_source(notes)
        open_path(hand, ["Snippets"])
        hand.drag(hand.row(*INV, label + " snippet"), cell("Dropped here:", len("Dropped here:")),
                  ctx.inputs["duration_ms"], 0)
        waits("Dropped here:int total = 0;")
        shot("nvim-dropped")
        hand.click(status_row())
        typed("u")
        for _ in range(100):
            if not says("Dropped here:int total = 0;"):
                break
            time.sleep(0.05)
        ctx.check(not says("Dropped here:int total = 0;"), "u did not take the drop back")
        note("Neovim drop inserted and undone")

    elif phase == "keyboard":
        ctx.step("ctrl+e picks the selection up; a click places it in Snippets")
        open_source(beat)
        hand.click(cell("return total;"))
        for _ in range(len("return total;")):
            hand.key("shift+right")
        hand.key("ctrl+e")
        shot("picked")
        hand.click(folder_row(hand, "Snippets"))
        snippet, path = name_new(label + " snippet")
        ctx.check(path == ["Snippets"] and b"return total;" in pair_of(snippet), "the keyboard copy is wrong")
        ctx.step("ctrl+l picks up this file's place; a click places it in Places")
        hand.click(cell("int main()"))
        hand.key("ctrl+l")
        hand.click(folder_row(hand, "Places"))
        place, path = name_new(label + " place")
        ctx.check(path == ["Places"], "the place is not in Places")
        ctx.step("Enter picks a stored copy up in Inventory; a click drops it into the Editor")
        open_source(notes)
        open_path(hand, ["Snippets"])
        hand.click(hand.row(*INV, label + " snippet"))
        hand.key("enter")
        hand.click(cell("Dropped here:", len("Dropped here:")))
        ctx.check(says("Dropped here:return total;"), "the picked copy did not land where clicked")
        shot("placed")
        hand.click(status_row())
        hand.key("ctrl+d")
        note("keyboard route", snippet=snippet["reference"], place=place["reference"])

    else:
        folder.mkdir(parents=True, exist_ok=True)
        if not Path(notes).exists():
            Path(notes).write_text(NOTES, encoding="ascii", newline="\n")
        ctx.step("restore the toolbox in this fresh Workshop")
        restored = hand.ask("zengine.inventory-pane", "InventoryToolboxRestore",
                            {"path": box, "replace": False}, settle=True)
        note("toolbox restored", entries=restored["entries"])
        for suffix, where in (("snippet", ["Snippets"]), ("call", ["Snippets"]), ("place", ["Places"]),
                              ("command", ["Commands"])):
            _, path = entry(hand, label + " " + suffix)
            ctx.check(path == where, "%s restored into %s" % (suffix, path))
        ctx.step("the place reopens beat.cpp, saved under the other root")
        open_source(notes)
        open_path(hand, ["Places"])
        hand.drag(hand.row(*INV, label + " place"), status_row(), ctx.inputs["duration_ms"], ctx.inputs["bend"])
        ctx.check(says("for (int i"), "the restored place did not reopen its file")
        note("place reopened", status=status_row()["text"], notice=" ".join(editor_rows()[:2]))
        shot("restored-place")
        ctx.step("the restored snippet drops into notes.txt")
        open_source(notes)
        to_root(hand)
        open_path(hand, ["Snippets"])
        hand.drag(hand.row(*INV, label + " snippet"), cell("Dropped here:", len("Dropped here:")),
                  ctx.inputs["duration_ms"], ctx.inputs["bend"])
        ctx.check(says("Dropped here:int total = 0;"), "the restored snippet did not land")
        shot("restored-snippet")
        hand.click(status_row())
        hand.key("ctrl+d")
        note("snippet retrieved")

    evidence["frames"] = shots
    evidence["request_calls"] = dict(measured.calls)
    evidence["actions"] = dict(hand.actions)
    evidence["entries"] = organization(hand)[0].fields
    ctx.produce("result.json", json.dumps(evidence, indent=2, default=str).encode())
    hand.close()
    return "%s: %d steps, %d pictures" % (phase, len(evidence["steps"]), len(shots))
