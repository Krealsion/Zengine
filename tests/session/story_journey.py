# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""THE TOWER-DEFENSE STORY'S CUSTODY, ON REAL PROCESSES: what `story.py` keeps of a run it stopped
waiting for, and what it believes about the Workshop and Loom host it started.

    story_journey.py --story <examples/tower-defense/story.py> --workshop <zengine-workshop>
                     --loom-host <loom-host> --loom-runs <loom-runs> --loom-runtime <loom_session dir>
                     --vocabulary <zengine-guest-vocabulary> --work <dir> [--evidence <file.json>]

Each root is launched by the story's own `launch` (this build tree's Workshop on the terminal plan,
the installed Loom's session host) and its commands run as a maker runs them, one process each.
A run whose wait ran out keeps its handle until it is seen to settle: through `status` when its
result arrives late, through `cancel` when it is cancelled -- and after cancellation the SAME link
gets Workshop's input session again -- and not at all while the session does not answer. `stop`
quits Workshop through the ELH and sees it end; with the ELH gone it refuses, `reset` retires
nothing, and `--force` ends Workshop only once its identity is confirmed; a record whose start
time names another process is not touched; a force that cannot end the process leaves the root
unstopped; a launch that fails ends what it started. Whether a process still runs is asked of the
operating system by this driver, the processes' parent, and never taken from the story's word.
Exit 0 only when every check held.
"""

import argparse
import json
from pathlib import Path
import secrets
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from story_rig import NOTES, Rig, arguments, check, wait_until, write_evidence  # noqa: E402


def status_of(rig, root):
    code, out = rig.cli(root, "status")
    try:
        return json.loads(out)
    except ValueError:
        return {"unreadable": out, "exit": code}


def held(root):
    return (json.loads((Path(root) / "story-status.json").read_text(encoding="utf-8")) or {}).get("current_run")


def unresolved_runs(rig):
    story = rig.story
    root = rig.root("runs")
    st = story.Story(root)
    st.index = 1
    info = ["zengine.info", "info"]
    never = "never painted %s" % secrets.token_hex(4)

    # ---- a wait that runs out keeps the run's handle, and nothing new starts over it ------------
    try:
        st.run("workshop/act", "held", {"steps": [{"expect": info + [never], "seconds": 120}]}, wait=1.0)
        raised = "returned"
    except story.Unresolved as why:
        raised = str(why)
    h = held(root)
    check("U1 a run whose story wait ran out is UNRESOLVED, and its handle (name, lifetime) is kept",
          raised.startswith("run 01-held") and h and h["name"] == "01-held" and
          h["lifetime"] == rig.record(root)["lifetime"], raised)
    s = status_of(rig, root)
    check("U2 `status`, a separate process, finds the same run through the manager, still running",
          "run 01-held is running" in s.get("current_run_now", ""), s.get("current_run_now"))
    try:
        st.run("workshop/connections", "blocked", {})
        blocked = "started"
    except story.StepFailed as why:
        blocked = str(why)
    check("U3 nothing new starts while a run is unresolved", "still unresolved" in blocked, blocked)

    # ---- cancellation is followed to the ending the manager reports -----------------------------
    code, out = rig.cli(root, "cancel")
    check("U4 `cancel` reaches the held run and reports the ending it saw: cancelled, its cleanup "
          "done", code == 0 and "run 01-held ended cancelled" in out and
          "release the demo input session: done" in out, out)
    check("U5 once the ending is seen the handle is let go", held(root) is None, held(root))
    rec = st.run("workshop/act", "after-cancel", {"steps": [{"rows": info, "as": "info"}]})
    check("U6 the SAME link gets Workshop's input session again: the cancelled run gave it back",
          rec["state"] == "passed", rec.get("failure"))

    # ---- a result that arrives after the wait is read and written down --------------------------
    try:
        st.run("workshop/act", "late", {"steps": [{"wait": 3}]}, wait=0.5)
        late = "returned"
    except story.Unresolved as why:
        late = str(why)
    time.sleep(4.0)
    s = status_of(rig, root)
    entry = [r for r in json.loads((Path(root) / "story-status.json").read_text())["runs"]
             if r["name"] == "01-late"]
    check("U7 a late result is read by `status` and written down; the handle is let go",
          late.startswith("run 01-late") and "settled" in s.get("current_run_now", "") and
          held(root) is None and entry and entry[0]["state"] == "passed", (late, s.get("current_run_now")))

    # ---- a session that does not answer is not an ending ----------------------------------------
    try:
        st.run("workshop/act", "orphan", {"steps": [{"expect": info + [never], "seconds": 120}]}, wait=1.0)
    except story.Unresolved:
        pass
    before = held(root)
    host = rig.record(root)["host_process"]
    rig.launched[host["pid"]].kill()
    wait_until(lambda: not rig.alive(host["pid"]), 30)
    check("U8 (arranged) the Loom host is gone and Workshop still runs",
          not rig.alive(host["pid"]) and rig.alive(rig.record(root)["workshop_process"]["pid"]))
    s = status_of(rig, root)
    check("U9 `status` with no session says the session does not answer and keeps the handle",
          "does not answer" in s.get("current_run_now", "") and held(root) == before, s.get("current_run_now"))
    code, out = rig.cli(root, "cancel")
    check("U10 `cancel` with no session refuses to call it stopped and keeps the handle",
          code == 1 and "not known to have stopped" in out and held(root) == before, out)
    code, out = rig.cli(root, "stop")
    check("U11 `stop` refuses while that run is unresolved, and Workshop still runs",
          code == 1 and "unresolved" in out and rig.alive(rig.record(root)["workshop_process"]["pid"]), out)
    code, out = rig.cli(root, "stop", "--force")
    check("U12 `stop --force` ends Workshop -- seen ended -- and says so",
          code == 0 and "Workshop: killed" in out and not rig.alive(rig.record(root)["workshop_process"]["pid"]),
          out)
    return root


def custody(rig):
    story = rig.story

    # ---- a maker's quit, and its ending seen --------------------------------------------------
    root = rig.root("quit")
    st = story.Story(root)
    st.index = 1
    st.act("terminal", [{"press": "ctrl+t"}, {"expect": ["zengine.terminal", "terminal", "TERMINAL"],
                                              "seconds": 10}])
    code, out = rig.cli(root, "stop")
    r = rig.record(root)
    check("P1 `stop` quits Workshop through the ELH and ends the session, both seen ended",
          code == 0 and "Workshop: quit" in out and "ELH: shut down" in out and
          not rig.alive(r["workshop_process"]["pid"]) and not rig.alive(r["host_process"]["pid"]) and
          r.get("stopped", {}).get("workshop", "").startswith("quit"), out)
    code, out = rig.cli(root, "reset")
    check("P2 `reset` retires a root whose processes are seen ended", code == 0 and "retired" in out and
          not root.exists(), out)

    # ---- the ELH gone, Workshop alive: nothing is claimed, nothing retired ----------------------
    root = rig.root("lost")
    r = rig.record(root)
    rig.launched[r["host_process"]["pid"]].kill()
    wait_until(lambda: not rig.alive(r["host_process"]["pid"]), 30)
    code, out = rig.cli(root, "stop")
    check("P3 `stop` with the ELH gone does not call Workshop stopped, and it still runs",
          code == 1 and "does not answer" in out and "NOT stopped" in out and rig.alive(r["workshop_process"]["pid"]) and
          "stopped" not in rig.record(root), out)
    code, out = rig.cli(root, "reset")
    check("P4 `reset` retires nothing while Workshop runs", code != 0 and root.exists() and
          "refusing to retire" in out, out)

    # ---- a record whose start time names another process: not touched --------------------------
    kept = dict(r["workshop_process"])
    story.save(root / "story.json", dict(r, workshop_process=dict(kept, started=str(int(kept["started"]) + 1))))
    code, out = rig.cli(root, "stop", "--force")
    check("P5 a recorded identity that names another process start is taken as ended and never "
          "forced: the live Workshop with that id is not touched", rig.alive(kept["pid"]) and
          "is another process now" in out, out)
    story.save(root / "story.json", r)

    # ---- a force that cannot end the process leaves the root unstopped (controlled branch) -------
    real = story.terminate
    story.terminate = lambda k: (False, "the operating system refused (arranged)")
    seen = story.stop_processes(story.Story(root), force=True)
    story.terminate = real
    check("P6 (controlled) a force the operating system refuses: not stopped, said, Workshop running",
          not seen["stopped"] and any("--force could not end Workshop" in n for n in seen["notes"]) and
          rig.alive(kept["pid"]), seen)
    real_seen = story.seen_ending
    story.terminate = lambda k: (True, "asked to end (arranged)")
    story.seen_ending = lambda k, s: ("running", "still running (arranged)")
    seen = story.stop_processes(story.Story(root), force=True)
    story.terminate, story.seen_ending = real, real_seen
    check("P7 (controlled) a force whose ending is never seen: not stopped, said",
          not seen["stopped"] and any("not seen to end" in n for n in seen["notes"]), seen)
    code, out = rig.cli(root, "stop", "--force")
    check("P8 `stop --force` ends the confirmed Workshop, sees it end, and marks the root stopped",
          code == 0 and "Workshop: killed" in out and not rig.alive(kept["pid"]) and
          rig.record(root).get("stopped", {}).get("workshop", "").startswith("killed"), out)
    code, out = rig.cli(root, "reset")
    check("P9 ...and only then `reset` retires it", code == 0 and not root.exists(), out)

    # ---- a launch that fails ends what it started ------------------------------------------------
    work = rig.work / "failed-launch"
    for d in ("game", "workshop", "elh"):
        (work / d).mkdir(parents=True)
    before = set(rig.launched)
    tools = dict(rig.tools, loom_host=str(work / "no-such-loom-host"))
    try:
        story.launch(rig.runtime, work / "game", work / "workshop", work / "elh", tools, rig.env, "180x80",
                     "default-load-plan.json")
        said = "launched"
    except SystemExit as why:
        said = str(why)
    started = [p for p in rig.launched if p not in before]
    check("P10 a launch that fails ends the Workshop it had started and says so",
          "the launch ended what it had started: Workshop pid" in said and started and
          not any(rig.alive(p) for p in started), said)


def main():
    args = arguments(argparse.ArgumentParser(description=__doc__)).parse_args()
    rig = Rig(args)
    try:
        unresolved_runs(rig)
        custody(rig)
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
