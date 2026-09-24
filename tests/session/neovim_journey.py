# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""WORKSHOP'S NEOVIM, EDITED THROUGH A LOOM SESSION: `workshop/nvim-edit` against a real Neovim in a
real Workshop, and the tower-defense story's stop over the unsaved work Neovim holds.

    neovim_journey.py --neovim <nvim> --story <examples/tower-defense/story.py>
                      --workshop <zengine-workshop> --loom-host <loom-host> --loom-runs <loom-runs>
                      --loom-runtime <loom_session dir> --vocabulary <zengine-guest-vocabulary>
                      --work <dir> [--evidence <file.json>]

A story root is launched by the story's own `launch` (this build tree's Workshop on the terminal
plan, isolated, with the named Neovim under its clean profile) and the Editor is switched to
Neovim from Workshop's Terminal, as the story does. Every claim about an edit is read three ways:
the tool's verdict, the buffer as the Neovim pane paints it, and the file on disk. A new empty
document is created; a draft never saved, an unsaved change to a file on disk, an interrupted
creation's leftover typing and a buffer Neovim would not leave are refused with nothing typed
into them; a retry after the leftover is discarded succeeds. Then `story.py stop` refuses to quit
over the unsaved work, `--discard-unsaved` discards it and quits, and `--force` ends a Workshop
that will not quit -- each seen to end. Exit 0 only when every check held.
"""

import argparse
import json
import os
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from story_rig import Rig, arguments, check, write_evidence  # noqa: E402

EDITOR = ["zengine.editor", "editor"]


def rows(st, label):
    rec = st.act(label, [{"rows": EDITOR, "as": "r"}])
    return json.loads(Path([a for a in rec["artifacts"] if a["name"] == "r.json"][0]["path"])
                      .read_text(encoding="utf-8"))["rows"]


def names(status, path):
    """Whether the Neovim pane's status row names `path`: the row cuts a long path from the left
    (`...` and its end), so what it shows must be the path's end."""
    shown = status.split(" -- ", 1)[1] if " -- " in status else ""
    if shown.startswith("..."):
        return path.endswith(shown[3:])
    return shown == path


def edit(story, st, label, path, edits):
    try:
        rec = st.edit(label, Path(path).as_posix(), edits)
        return "passed", rec.get("summary", "")
    except story.StepFailed as why:
        return "refused", str(why).splitlines()[0]


def prepare(rig, name):
    """A root whose Editor is Neovim, its pane open and placed where nothing covers it."""
    story = rig.story
    root = rig.root(name)
    st = story.Story(root)
    st.index = 1
    story.s_editor(st)
    st.run("workshop/place", "place-terminal", {"panes": [{"pane": "Terminal", "x": 0, "y": 62,
                                                           "width": 120, "height": 14}]})
    st.act("open-neovim", [{"open": "Neovim"}, {"wait": 0.5}])
    # The Pane Manager stays on the desk after it opens a pane: out of the way, as the story's desk
    # puts it, so nothing covers Neovim (a covered pane is not described).
    st.run("workshop/place", "place-pane-manager", {"panes": [{"pane": "Pane Manager", "x": 134, "y": 20,
                                                               "width": 44, "height": 40}]})
    # Row 2, not row 1: in the terminal medium a pane whose frame starts on row 1 is not
    # addressable, and Workshop describes no pane it cannot point into.
    st.run("workshop/place", "place-neovim", {"panes": [{"pane": "Neovim", "x": 0, "y": 2,
                                                         "width": 128, "height": 56}]})
    return root, st


def draft(st, label, path, keys):
    st.act(label, [{"into": EDITOR}, {"press": "escape"}, {"type": ":e %s\n" % Path(path).as_posix()},
                   {"wait": 0.5}, {"type": keys}, {"press": "escape"},
                   {"expect": EDITOR + ["UNSAVED"], "seconds": 10}])


def edits(rig):
    story = rig.story
    root, st = prepare(rig, "edits")
    game = Path(rig.record(root)["game"])
    posix = lambda p: Path(os.path.normpath(str(p))).as_posix()

    # ---- a genuinely new, empty document is created --------------------------------------------
    new = game / "notes" / "new.txt"
    verdict, said = edit(story, st, "new", new, [{"create": "first line\nsecond line\n"}])
    shown = rows(st, "new-rows")
    check("N1 a new document is created: saved in Neovim and on disk exactly as planned",
          verdict == "passed" and new.read_bytes() == b"first line\nsecond line\n" and
          shown[0].startswith("saved ") and shown[1:3] == ["first line", "second line"], (said, shown[:3]))

    # ---- a draft never saved is refused and kept ------------------------------------------------
    never = game / "drafts" / "never-saved.txt"
    draft(st, "never-draft", never, "iMY DRAFT, NEVER SAVED")
    verdict, said = edit(story, st, "never", never, [{"create": "replacement\n"}])
    shown = rows(st, "never-rows")
    check("N2 a draft never saved to disk is refused, and the draft is still Neovim's, unsaved",
          verdict == "refused" and "a document never saved" in said and shown[0].startswith("UNSAVED ") and
          names(shown[0], posix(never)) and shown[1] == "MY DRAFT, NEVER SAVED" and not never.exists(),
          (said, shown[:2]))

    # ---- an unsaved change to a file on disk is refused ------------------------------------------
    draft(st, "change-draft", new, "GoAN UNSAVED LINE")
    verdict, said = edit(story, st, "change", new, [{"append": "planned line\n"}])
    shown = rows(st, "change-rows")
    check("N3 an unsaved change to a file on disk is refused; the buffer keeps it, the file is as saved",
          verdict == "refused" and "unsaved changes" in said and shown[0].startswith("UNSAVED ") and
          "AN UNSAVED LINE" in shown and new.read_bytes() == b"first line\nsecond line\n", (said, shown[:4]))
    st.act("change-discard", [{"into": EDITOR}, {"press": "escape"}, {"type": ":e!\n"},
                              {"expect": EDITOR + ["saved "], "seconds": 10}])

    # ---- an interrupted creation, its leftover refused, then discarded and retried ---------------
    cut = game / "drafts" / "interrupted.txt"
    text = "".join("line %03d of a creation interrupted part way\n" % i for i in range(150))
    name = "01-interrupted"
    with st.Session.attach(st.record["session"]) as s:
        s.start("workshop/nvim-edit", name, {"link": "workshop", "path": cut.as_posix(),
                                             "edits": json.dumps([{"create": text}]), "chunk": 200,
                                             "pace_ms": 400})
        deadline = time.monotonic() + 60
        while "type" not in s.run(name).get("step", "") and time.monotonic() < deadline:
            time.sleep(0.2)
        time.sleep(2.0)
        s.cancel(name, "the journey interrupts a creation part way")
        done = s.wait(name, timeout=90)
    shown = rows(st, "cut-rows")
    check("N4 a creation cancelled while it types ends cancelled with its input given back; the "
          "leftover is Neovim's, unsaved, and nothing is on disk", done["state"] == "cancelled" and
          any(n.startswith("cleanup") and n.endswith("done") for n in done["notes"]) and
          shown[0].startswith("UNSAVED ") and shown[1].startswith("line 000") and not cut.exists(),
          (done["state"], shown[:2]))
    verdict, said = edit(story, st, "retry", cut, [{"create": text}])
    shown = rows(st, "retry-rows")
    check("N5 the retry is refused like any unsaved work; the leftover is kept",
          verdict == "refused" and "unsaved changes" in said and shown[0].startswith("UNSAVED ") and
          shown[1].startswith("line 000") and not cut.exists(), said)
    st.act("cut-discard", [{"into": EDITOR}, {"press": "escape"}, {"type": ":e!\n"},
                           {"expect": EDITOR + ["saved "], "seconds": 10}])
    verdict, said = edit(story, st, "retry-2", cut, [{"create": text}])
    check("N6 after the leftover is discarded, the retry creates the file exactly as planned",
          verdict == "passed" and cut.read_bytes() == text.encode(), said)

    # ---- a buffer Neovim will not leave: the other file is never typed into ---------------------
    a, b = game / "a1" / "abcdefghijklmnopqrstuvwxyz.txt", game / "a2" / "abcdefghijklmnopqrstuvwxyz.txt"
    st.act("hidden-off", [{"into": EDITOR}, {"press": "escape"}, {"type": ":set nohidden\n"}])
    draft(st, "a-draft", a, "iDRAFT IN FILE A")
    verdict, said = edit(story, st, "b", b, [{"create": "text meant for B\n"}])
    shown = rows(st, "a-rows")
    check("N7 when :e is refused (another buffer unsaved, 'hidden' off) nothing is typed into that "
          "buffer and neither file is written", verdict == "refused" and "did not open" in said and
          shown[0].startswith("UNSAVED ") and names(shown[0], posix(a)) and shown[1] == "DRAFT IN FILE A" and
          not a.exists() and not b.exists(), (said, shown[:2]))
    return root


def stops(rig, root):
    story = rig.story
    st = story.Story(root)
    st.index = 5
    kept = rig.record(root)["workshop_process"]
    code, out = rig.cli(root, "stop")
    check("S1 `stop` does not quit over Neovim's unsaved work: not stopped, Workshop and the ELH run",
          code == 1 and "did not quit" in out and rig.alive(kept["pid"]) and
          rig.alive(rig.record(root)["host_process"]["pid"]), out)
    code, out = rig.cli(root, "stop", "--discard-unsaved")
    r = rig.record(root)
    check("S2 `stop --discard-unsaved` discards it, Workshop quits and both processes are seen ended",
          code == 0 and "Workshop: quit" in out and not rig.alive(kept["pid"]) and
          not rig.alive(r["host_process"]["pid"]), out)

    root2, st2 = prepare(rig, "force")
    game = Path(rig.record(root2)["game"])
    draft(st2, "force-draft", game / "draft.txt", "iUNSAVED WORK")
    kept = rig.record(root2)["workshop_process"]
    code, out = rig.cli(root2, "stop", "--force")
    check("S3 `stop --force` over unsaved work: the quit is refused, then Workshop is ended -- its "
          "identity confirmed, its ending seen", code == 0 and "Workshop: killed" in out and
          not rig.alive(kept["pid"]), out)


def main():
    parser = arguments(argparse.ArgumentParser(description=__doc__))
    parser.add_argument("--neovim", required=True)
    args = parser.parse_args()
    rig = Rig(args, env={"ZENGINE_NEOVIM": str(Path(args.neovim).resolve()),
                         "ZENGINE_NEOVIM_PROFILE": "clean"})
    try:
        stops(rig, edits(rig))
    finally:
        left = rig.finish()
        check("E1 nothing this driver launched was left for it to end", not left, left)
    return 0


if __name__ == "__main__":
    code = 1
    evidence = sys.argv[sys.argv.index("--evidence") + 1] if "--evidence" in sys.argv else ""
    try:
        code = main()
    except BaseException as err:
        import traceback
        traceback.print_exc()
        check("the journey driver ran to its end", False, "%s: %s" % (type(err).__name__, err))
    sys.exit(max(code, write_evidence(evidence)))
