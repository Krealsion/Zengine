# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""THE WORKSHOP JOURNEY FROM A LOOM SESSION -- three kinds of real process.

A real `zengine-workshop` (isolated: it reads and writes none of the user's profile), started with
a guests file; a real session host from an INSTALLED Loom (`loom-host --serve`, its run manager
`loom-runs`, the `loom_session` runtime) linked to it twice under two guest rows; this
repository's guest-vocabulary weave booted in that host; and the Workshop tool package, written in
Python, run as named runs by workers the session starts. Clients attach, act and leave. Every
claim is read from its owner: Workshop's answers through the run's own asks, the run manager's
records, the session door, the host's history through its scoped reader. Waiting is this driver's
own decision and is never reported as anybody's outcome.

    workshop_journey.py --workshop <zengine-workshop> --plan <load plan>
                        --loom-host <loom-host> --loom-runs <loom-runs artifact>
                        --loom-runtime <dir holding loom_session>
                        --vocabulary <zengine-guest-vocabulary artifact>
                        --tools <external-host/tools/workshop> --work <dir>
                        [--graphical] [--evidence <file.json>]

The three Loom paths are what an installed Loom's package names (`LOOM_HOST_PROGRAM`,
`LOOM_RUNS_ARTIFACT`, `LOOM_SESSION_RUNTIME`); tests/CMakeLists.txt passes them.

With a headless plan the Skin's pictures are its cells (`text/cells`); with `--graphical` and the
graphical plan they are the window's pixels (`image/bmp`), and the pictures are kept for a person
to look at. Exit 0 only when every check held.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

CHECKS = []
PROCS = []
EVIDENCE = {"path": "", "notes": {}}
PANE_MANAGER = b"Pane Manager @zengine.desktop"
# THE EDIT the journey makes to its copy of inspect_capture.py: put Workshop back after the chord.
EDIT_ANCHOR = '''    ctx.step("close input")
    close()'''
EDIT_ADDS = '''    back = {"down": "up", "up": "down", "left": "right", "right": "left"}.get(ctx.inputs["chord"])
    if back:
        ctx.step("put back")
        ctx.ask("zengine.input", "InjectInput", {"session": session,
                                                 "events": chord_moments(ctx, back)},
                via=link, settle=True)
        restored, restored_bytes = picture(ctx, link, "restored")
        ctx.check(restored_bytes != after_bytes, "%s changed nothing Workshop presents" % back)
        ctx.note("edited: %s pressed after the picture, and Workshop pictured again" % back)

'''


def check(name, condition, detail=""):
    CHECKS.append({"check": name, "ok": bool(condition), "detail": str(detail)})
    shown = str(detail).splitlines()[0] if detail else ""
    print("%s  %s%s" % ("ok  " if condition else "FAIL", name, ("  -- " + shown) if shown else ""),
          flush=True)
    return bool(condition)


def note(key, value):
    EVIDENCE["notes"][key] = value


def until(predicate, seconds, what):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            if predicate():
                return True
        except Exception:
            pass
        time.sleep(0.05)
    print("     (the driver stopped waiting for %s after %.0fs)" % (what, seconds), flush=True)
    return False


def read_record(run_dir):
    with open(os.path.join(run_dir, "run.json"), "r", encoding="utf-8") as f:
        return json.load(f)["fields"]


def release(run, gate):
    os.makedirs(os.path.join(run["directory"], "release"), exist_ok=True)
    open(os.path.join(run["directory"], "release", gate), "w").close()


def artifacts(run):
    return dict((a["name"], a) for a in run["artifacts"])


def picture_name(arts, stem):
    for ext in ("bmp", "cells.txt"):
        if "%s.%s" % (stem, ext) in arts:
            return "%s.%s" % (stem, ext)
    return None


def main():
    ap = argparse.ArgumentParser()
    names = ("workshop", "plan", "loom_host", "loom_runs", "loom_runtime", "vocabulary", "tools",
             "work")
    for name in names:
        ap.add_argument("--" + name.replace("_", "-"), required=True)
    ap.add_argument("--graphical", action="store_true")
    ap.add_argument("--evidence", default="")
    args = ap.parse_args()
    EVIDENCE["path"] = args.evidence
    for name in names:
        setattr(args, name, os.path.abspath(getattr(args, name)))
    runtime = args.loom_runtime
    sys.path.insert(0, runtime)
    from loom_session.session import Session, read_session_file
    from loom_session.client import Refused
    host_exe = args.loom_host
    runs_artifact = args.loom_runs
    check("P1 the installed Loom carries the session tooling", os.path.isfile(host_exe) and
          os.path.isfile(runs_artifact) and
          os.path.isfile(os.path.join(runtime, "loom_session", "__init__.py")),
          (host_exe, runs_artifact, runtime))

    from workshop_tool_checks import run_checks
    if not check("T1 tool entry points preserve events, early refusal and cleanup",
                 run_checks(args.tools, runtime)):
        return 1

    work = args.work
    shutil.rmtree(work, ignore_errors=True)
    wdir = os.path.join(work, "workshop")
    sdir = os.path.join(work, "session")
    pkgs = os.path.join(work, "packages")
    os.makedirs(wdir)
    os.makedirs(sdir)

    # ---- the Workshop: isolated, a guests file naming TWO rows, its port written for us -------
    with open(os.path.join(wdir, "guests.json"), "w", encoding="utf-8") as f:
        json.dump({"listen": "127.0.0.1:0", "port_file": "guests.port", "guests": [
            {"name": "agent", "credential": "first-key", "may": ["input", "capture", "inspect"]},
            {"name": "agent-two", "credential": "second-key",
             "may": ["input", "capture", "inspect"]}]}, f)
    env = dict(os.environ)
    for var in ("APPDATA", "LOCALAPPDATA", "XDG_CONFIG_HOME", "XDG_STATE_HOME", "XDG_DATA_HOME"):
        env[var] = os.path.join(wdir, "profile", var.lower())
        os.makedirs(env[var], exist_ok=True)
    wout = open(os.path.join(wdir, "workshop.out"), "wb")
    workshop = subprocess.Popen([args.workshop, "--isolated", "--load-plan", args.plan,
                                 "--guests", "guests.json", "--log", "workshop.log"],
                                cwd=wdir, env=env, stdin=subprocess.DEVNULL, stdout=wout,
                                stderr=subprocess.STDOUT)
    PROCS.append(workshop)
    port_file = os.path.join(wdir, "guests.port")
    if not check("W0 Workshop listens for its guests", until(
            lambda: os.path.exists(port_file) and open(port_file).read().strip(), 30,
            "Workshop's port")):
        return 1
    port = int(open(port_file).read().strip())
    note("workshop_pid", workshop.pid)
    note("workshop_port", port)

    # ---- the tool packages: editable copies; a second one speaks through the second link ------
    shutil.copytree(args.tools, os.path.join(pkgs, "workshop"),
                    ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    second = os.path.join(pkgs, "workshop-b")
    shutil.copytree(args.tools, second, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    with open(os.path.join(second, "loom-tool.json"), "r", encoding="utf-8") as f:
        m = json.load(f)
    m["package"] = "workshop-b"
    for t in m["tools"]:
        t["asks"] = [a.replace("loom.link.workshop", "loom.link.workshop2") for a in t["asks"]]
        for i in t["inputs"]:
            if i["name"] == "link":
                i["default"] = "workshop2"
    with open(os.path.join(second, "loom-tool.json"), "w", encoding="utf-8") as f:
        json.dump(m, f, indent=1)
    with open(os.path.join(sdir, "loom-tools.json"), "w", encoding="utf-8") as f:
        # The workers run on the runtime this driver's own clients use, named rather than found.
        json.dump({"python": sys.executable, "runtime": runtime, "packages": [
            {"path": os.path.join(pkgs, "workshop"), "approve": "any-revision"},
            {"path": second, "approve": "any-revision"}]}, f, indent=1)
    with open(os.path.join(sdir, "loom-boot.json"), "w", encoding="utf-8") as f:
        json.dump({
            "boot": [{"name": "runs", "path": runs_artifact, "role": "loom.runs"},
                     {"name": "vocab", "path": args.vocabulary}],
            "links": [{"name": "workshop", "connect": "127.0.0.1:%d" % port,
                       "identity": "loom-session", "credential": "first-key"},
                      {"name": "workshop2", "connect": "127.0.0.1:%d" % port,
                       "identity": "loom-session-two", "credential": "second-key"}],
            "history": {"log": "session.log", "recent": "8192", "payload_budget": "134217728",
                        "retain": [{"shape": "SurfaceCaptureChunk", "last_n": "1",
                                    "in_recent": False, "retain_payload": False},
                                   {"shape": "loom.link.Crossed", "last_n": "1024",
                                    "in_recent": True, "retain_payload": True}],
                        "keep": [{"shape": "loom.link.Crossed", "cap": "4096"},
                                 {"shape": "loom.link.Outcome", "cap": "256"}]}}, f, indent=1)
    approvals = [
        "authority trust runs --rebuilds", "authority trust vocab --rebuilds",
        "authority allow runs loom.session.ExpectRun v1 -> role loom.session",
        "authority allow runs loom.session.ForgetRun v1 -> role loom.session",
        "authority allow runs loom.runs.Tools v1 -> any target",
        "authority allow runs loom.runs.ToolDescription v1 -> any target",
        "authority allow runs loom.runs.Run v1 -> any target",
        "authority allow runs loom.runs.RunList v1 -> any target",
        "authority allow runs loom.runs.Directive v1 -> any target",
        "authority allow runs loom.link.Ask v1 -> role loom.link.workshop",
        "authority allow runs loom.link.StatusRequested v1 -> role loom.link.workshop",
        "authority allow runs loom.link.Ask v1 -> role loom.link.workshop2",
        "authority allow runs loom.link.StatusRequested v1 -> role loom.link.workshop2",
        "start vocab %s" % args.vocabulary,
        "start runs %s loom.runs" % runs_artifact]

    def start_host(lines):
        log = open(os.path.join(sdir, "host-%d.log" % int(time.time() * 1000)), "wb")
        proc = subprocess.Popen([host_exe, "--serve", sdir], cwd=sdir, stdin=subprocess.PIPE,
                                stdout=log, stderr=subprocess.STDOUT)
        proc.stdin.write(("\n".join(lines) + "\n").encode("utf-8"))
        proc.stdin.close()
        PROCS.append(proc)
        return proc

    host = start_host(approvals)
    state = {"host": host}

    def serving():
        info = read_session_file(sdir)
        if info.get("pid") != state["host"].pid:
            return False
        with Session.attach(sdir) as s:
            s.tools()
            links = dict((l["name"], l) for l in s.describe()["links"])
            return all(links.get(n, {}).get("state") == "admitted"
                       for n in ("workshop", "workshop2"))

    if not check("S1 the session serves, its manager and vocabulary loaded, both links admitted",
                 until(serving, 40, "the session")):
        return 1
    with Session.attach(sdir) as s:
        lifetime = s.lifetime
        d = s.describe()
        links = dict((l["name"], l) for l in d["links"])
        check("S2 Workshop admitted the two links as its own two rows, two far sessions",
              links["workshop"]["established"] == "agent" and
              links["workshop2"]["established"] == "agent-two" and
              links["workshop"]["far_session"] != links["workshop2"]["far_session"],
              (links["workshop"]["far_session"], links["workshop2"]["far_session"]))
        note("far_sessions", [links["workshop"]["far_session"], links["workshop2"]["far_session"]])

        # ---- discovery ------------------------------------------------------------------
        found = [r["id"] for r in s.tools("capture")["rows"]]
        check("D1 the catalog finds the Workshop tools by their words",
              "workshop/inspect-capture" in found, found)
        desc = s.tool("workshop/inspect-capture")
        resolvable = {}
        for v in desc["vocabulary"]:
            name, _, ver = v.rpartition(" v")
            try:
                resolvable[v] = s.shape(name, int(ver))
            except Exception:
                resolvable[v] = None
        check("D2 every shape the tool speaks resolves on this host, from Workshop's own definitions",
              all(v is not None for v in resolvable.values()),
              [k for k, v in resolvable.items() if v is None])
        check("D3 the structural truth is the host's: InjectInput carries its events as nested shapes",
              ("events", "List<InjectedEvent v1>", True) in [tuple(x) for x in
                                                             resolvable.get("InjectInput v1") or []])

        s.start("workshop/connections", "who", {})
        who = s.wait("who", timeout=60)
        check("D4 a first run inspects Workshop and finds this session's own row",
              who["state"] == "passed" and "agent" in who["summary"], who["summary"] or who["failure"])

    # ---- the journey that outlives its client: pending at Workshop, finished with nobody here --
    with Session.attach(sdir) as c1:
        c1_session = c1.connection.session
        c1.start("workshop/inspect-capture", "look", {"arm": True, "hold": "go"})
        until(lambda: c1.run("look")["step"] == "held: go", 60, "the run to hold")
        held = c1.run("look")
        pending = " | ".join(held["pending"])
        check("J1 the run is held with a capture WAITING at Workshop's Skin, and says so",
              held["state"] == "running" and "SurfaceCaptureRequested" in pending and
              "via link workshop" in pending and "release gate 'go'" in pending, pending)
        link_open = dict((l["name"], l) for l in c1.describe()["links"])["workshop"]["open"]
        check("J2 ...and the link itself holds that crossing open", link_open >= 1, link_open)
        manager = c1.connection.call("loom.runs.List", {}, role="loom.runs").sender
        look_dir = held["directory"]
    on_disk = read_record(look_dir)
    check("J3 no client attached: the manager's record says running, and what it waits on",
          on_disk["state"] == "running" and any("SurfaceCaptureRequested" in p
                                                for p in on_disk["pending"]), on_disk["step"])
    release(held, "go")
    check("J4 released with no client attached, the run finishes (its manager's record says so)",
          until(lambda: read_record(look_dir)["state"] in ("passed", "failed", "error"), 90,
                "the run's record to say it finished") and read_record(look_dir)["state"] ==
          "passed", read_record(look_dir)["failure"] or read_record(look_dir)["summary"])
    with Session.attach(sdir) as c2:
        look = c2.run("look")
        arts = artifacts(look)
        stems = [picture_name(arts, x) for x in ("before", "armed", "after")]
        check("J5 a new client finds the same run, passed, every picture verified on disk",
              c2.connection.session != c1_session and look["state"] == "passed" and
              None not in stems and all(arts[n]["state"] == "verified" for n in stems) and
              arts["connections.json"]["state"] == "verified", stems)
        check("J6 the armed capture was answered after the picture before it",
              "armed capture was answered at frame" in look["summary"], look["summary"])
        after = open(arts[stems[2]]["path"], "rb").read() if stems[2] else b""
        before = open(arts[stems[0]]["path"], "rb").read() if stems[0] else b""
        if args.graphical:
            check("J7 the pictures are the window's pixels, and the chord changed them",
                  after[:2] == b"BM" and before[:2] == b"BM" and after != before)
        else:
            # The Info pane LISTS "Pane Manager -- closed" before the chord; the pane's own title
            # row, "Pane Manager @zengine.desktop", is on the desk only once the chord opened it.
            check("J7 the picture after the chord shows the Pane Manager it opened",
                  PANE_MANAGER in after and PANE_MANAGER not in before)
        note("look_pictures", dict((n, arts[n]["path"]) for n in stems if n))
        rec_rows = c2.deliveries_to(look["session"])["rows"]
        by_corr = dict((a["correlation"], a) for a in look["asks"])
        crossing = [r for r in rec_rows if r["crossing"] and r["far_shape"] == "SurfaceCaptured"
                    and r["crossing_payload"] == "retained"]
        joined = by_corr.get(crossing[0]["correlation"]) if crossing else None
        check("J8 the picture's answer is traced to its crossing: far session, far author, attempt",
              crossing and crossing[0]["far_session"] == links["workshop"]["far_session"] and
              crossing[0]["established"] == "agent" and crossing[0]["attempt"] > 0 and
              crossing[0]["far_sender"] > 0 and crossing[0]["kind"] == "answer" and
              joined is not None and joined["shape"] == "SurfaceCaptureRequested",
              crossing[0] if crossing else rec_rows[:2])
        note("look_crossing", crossing[0] if crossing else None)
        c1_rows = c2.deliveries_to(c1_session)["rows"]
        fin = [r for r in c2.deliveries_to(manager, shape="loom.runs.Finished")["rows"]
               if r["sender"] == look["session"]]
        c2_rows = c2.deliveries_to(c2.connection.session)["rows"]
        order = (max(r["seq"] for r in c1_rows) if c1_rows else None,
                 fin[0]["seq"] if fin else None,
                 min(r["seq"] for r in c2_rows) if c2_rows else None)
        check("J9 the bus's own order: client 1 left, the run finished, client 2 arrived",
              None not in order and order[0] < order[1] < order[2], order)

        # ---- edit the tool; run again; no compiler, no restart, the old run kept ------------------
        # The first version leaves Workshop as its chord left it (the Pane Manager now holds the
        # keyboard). The edit teaches it to put Workshop back: an arrow's opposite, pictured.
        script = os.path.join(pkgs, "workshop", "inspect_capture.py")
        src = open(script, "r", encoding="utf-8").read()
        edited = src.replace(EDIT_ANCHOR, EDIT_ADDS + EDIT_ANCHOR)
        check("E0 the edit changes the tool's behaviour", edited != src)
        with open(script, "w", encoding="utf-8") as f:
            f.write(edited)
        c2.start("workshop/inspect-capture", "look-2", {"chord": "down"})
        look2 = c2.wait("look-2", timeout=120)
        arts2 = artifacts(look2)
        check("E1 the edited tool ran as a new revision with a new result",
              look2["state"] == "passed" and look2["revision"] != look["revision"] and
              picture_name(arts2, "restored") is not None,
              look2["failure"] or look2["revision"][:12])
        again = c2.run("look")
        check("E2 the earlier run keeps its revision and its pictures",
              again["revision"] == look["revision"] and
              all(hashlib.sha256(open(a["path"], "rb").read()).hexdigest() == a["sha256"]
                  for a in again["artifacts"]) and picture_name(artifacts(again), "restored") is None)
        d2 = c2.describe()
        check("E3 neither the session host nor its lifetime changed",
              d2["lifetime"] == lifetime and d2["pid"] == state["host"].pid)
        if picture_name(arts2, "restored"):
            digest = dict((stem, arts2[picture_name(arts2, stem)]["sha256"])
                          for stem in ("before", "after", "restored"))
            moved = digest["after"] != digest["before"]
            same = digest["restored"] == digest["before"]
            check("E4 the edited behaviour's pictures: down moved the Pane Manager's row, and up "
                  "put back exactly what Workshop presented before", moved and same)
        else:
            check("E4 the edited behaviour's pictures", False, "no restored picture")

        # ---- overlapping runs, and Workshop's one input holder ------------------------------
        c2.start("workshop/inspect-capture", "hold-a", {"hold": "a", "chord": "down"})
        until(lambda: c2.run("hold-a")["step"] == "held: a", 60, "hold-a to hold")
        c2.start("workshop/inspect-capture", "busy-b", {"chord": "down"})
        b = c2.wait("busy-b", timeout=90)
        b_asks = dict((a["shape"], a) for a in b["asks"])
        check("O1 a second run on the SAME link is refused the input session after it inspected: "
              "one holder, and every run on one link is that holder to Workshop",
              b["state"] == "failed" and "busy" in b["failure"] and "held by you" in b["failure"]
              and artifacts(b).get("connections.json", {}).get("state") == "verified" and
              b_asks.get("InputSessionRequested", {}).get("outcome") == "refused", b["failure"])
        c2.start("workshop-b/inspect-capture", "busy-c", {"chord": "down"})
        c = c2.wait("busy-c", timeout=90)
        check("O2 a run on the SECOND link is a different participant to Workshop, and is refused "
              "as one", c["state"] == "failed" and "held by another participant" in c["failure"],
              c["failure"])
        a = c2.run("hold-a")
        check("O3 meanwhile the first run still holds, untouched", a["state"] == "running" and
              a["step"] == "held: a")
        release(a, "a")
        a = c2.wait("hold-a", timeout=120)
        check("O4 released, it finishes -- its own pictures, its own directory",
              a["state"] == "passed" and a["directory"] != b["directory"] != c["directory"],
              a["failure"] or a["summary"])
        c2.start("workshop/inspect-capture", "after-overlap", {"chord": "down"})
        n = c2.wait("after-overlap", timeout=120)
        check("O5 the input session is free again: the next run passes", n["state"] == "passed",
              n["failure"])

        # ---- a tool bug after the input session opened: cleanup, then a success -------------
        c2.start("workshop/inspect-capture", "bug", {"bug_after": "open", "chord": "down"})
        g = c2.wait("bug", timeout=90)
        check("F1 a tool bug ends the run as an error, attributed, with its cleanup recorded",
              g["state"] == "error" and "deliberate tool bug" in g["failure"] and
              any("cleanup close input session" in x and "done" in x for x in g["notes"]),
              g["failure"].splitlines()[0] if g["failure"] else "")
        c2.start("workshop/inspect-capture", "after-bug", {"chord": "down"})
        n = c2.wait("after-bug", timeout=120)
        check("F2 the cleanup closed the session: the next run opens one and passes",
              n["state"] == "passed", n["failure"])

        # ---- CANCELLED WHILE HOLDING WORKSHOP'S INPUT SESSION --------------------------------
        #
        # The hardest case for a cancellation to get right: the run owns a far resource when it
        # is told to stop. Stopping its work must not stop it giving that resource back, and
        # "it tried" is not the evidence -- the evidence is Workshop handing the SAME link an
        # input session again afterwards, in the same host lifetime, which it refuses to anyone
        # already holding one.
        c2.start("workshop/inspect-capture", "cancel-held",
                 {"hold": "c", "chord": "down", "changed": False})
        held_input = until(lambda: c2.run("cancel-held")["step"] == "held: c", 120,
                           "the run to hold with an input session open")
        holding = c2.run("cancel-held")
        opened = [a for a in holding["asks"]
                  if a["shape"] == "InputSessionRequested" and a["outcome"] == "answer"]
        check("C1 the run owns Workshop's input session and is holding",
              held_input and len(opened) == 1 and holding["state"] == "running",
              (holding["state"], holding["step"]))
        asked = c2.cancel("cancel-held", reason="the journey cancels a run that owns input")
        check("C2 the cancellation is a request, recorded as asked for", asked["cancel_requested"])
        done = c2.wait("cancel-held", timeout=120)
        closed = [a for a in done["asks"] if a["shape"] == "InputSessionClosed"]
        check("C3 the cancelled run's cleanup CLOSED the input session, and the Input owner "
              "answered it", done["state"] == "cancelled" and
              any("cleanup close input session" in x and x.endswith("done") for x in done["notes"]),
              " | ".join(x for x in done["notes"] if "cleanup" in x))
        check("C4 that close is in the run's own account of its asks, answered -- not attempted",
              len(closed) == 1 and closed[0]["outcome"] == "answer" and closed[0]["via"] == "workshop",
              closed)
        # THE OWNER'S OWN WORD, and the whole point: the same link, the same host lifetime.
        c2.start("workshop/inspect-capture", "after-cancel", {"chord": "down"})
        again = c2.wait("after-cancel", timeout=120)
        check("C5 the SAME link opens an input session again and the run passes: Workshop is "
              "not holding one for a guest that went away",
              again["state"] == "passed" and "held by you" not in again["failure"],
              again["failure"][:200] or again["summary"])

        # ---- remote loss after submission: the outcome stays unknown ------------------------
        c2.start("workshop/inspect-capture", "lost",
                 {"arm": True, "hold": "k", "await_repaint": True, "await_seconds": 300,
                  "chord": "down"})
        until(lambda: c2.run("lost")["step"] == "held: k", 60, "the run to hold")
        lost_run = c2.run("lost")
        open_now = dict((x["name"], x) for x in c2.describe()["links"])["workshop"]["open"]
    # The far host goes away with this run's capture submitted and unanswered -- the link's own
    # count of open crossings, read just before, says one was.
    workshop.kill()
    workshop.wait()
    check("L0 Workshop ended while the run's armed capture was open on the link",
          open_now >= 1 and any("SurfaceCaptureRequested" in p for p in lost_run["pending"]),
          (open_now, lost_run["pending"][:1]))
    with Session.attach(sdir) as c3:
        until(lambda: dict((x["name"], x) for x in c3.describe()["links"])["workshop"]["state"]
              == "lost", 30, "the link to see its far host gone")
        release(lost_run, "k")
        l = c3.wait("lost", timeout=120)
        asks = [x for x in l["asks"] if x["shape"] == "SurfaceCaptureRequested"]
        check("L1 the link lost AFTER submission: the run says UNKNOWN, the ask says lost, nothing "
              "was resent", l["state"] == "failed" and "UNKNOWN" in l["failure"] and
              asks and asks[-1]["outcome"] == "lost", l["failure"])
        check("L2 ...and it did not claim to have closed Workshop's input session",
              any("Workshop's to close" in x for x in l["notes"]) or "Workshop's to close" in
              l["failure"], l["notes"][-2:])
        link_state = dict((x["name"], x) for x in c3.describe()["links"])["workshop"]["state"]
        check("L3 the session door reports the link as lost", link_state == "lost", link_state)
        c3.shutdown("the Workshop journey ends this lifetime")
    state["host"].wait(timeout=30)

    # ---- a new lifetime refuses the old handles; the records remain -------------------------
    host = start_host([])
    state["host"] = host
    if check("R1 a new session host serves the directory, booting what was approved",
             until(lambda: read_session_file(sdir).get("pid") == host.pid and
                   Session.attach(sdir).tools() is not None, 40, "the second host")):
        with Session.attach(sdir) as s:
            try:
                s.run("look", lifetime=lifetime)
                why = ""
            except Refused as err:
                why = str(err)
            check("R2 a run handle of the ended lifetime is refused by name", s.lifetime !=
                  lifetime and "another host lifetime" in why, why)
            past = dict((r["name"], r) for r in s.past()["rows"])
            check("R3 its records remain as evidence: passed, failed, error and the unknown one",
                  past.get("look", {}).get("state") == "passed" and
                  past.get("busy-b", {}).get("state") == "failed" and
                  past.get("bug", {}).get("state") == "error" and
                  "UNKNOWN" in past.get("lost", {}).get("failure", ""), sorted(past))
            s.shutdown("the journey is over")
        host.wait(timeout=30)
    return 0


def write_evidence():
    failed = [c for c in CHECKS if not c["ok"]]
    if EVIDENCE["path"]:
        with open(EVIDENCE["path"], "w", encoding="utf-8") as f:
            json.dump({"checks": CHECKS, "failed": len(failed), "notes": EVIDENCE["notes"]}, f,
                      indent=1, default=str)
    print("%d of %d checks held" % (len(CHECKS) - len(failed), len(CHECKS)), flush=True)
    return 0 if CHECKS and not failed else 1


if __name__ == "__main__":
    code = 1
    try:
        code = main()
    except BaseException as err:
        import traceback
        traceback.print_exc()
        check("the journey driver ran to its end", False, "%s: %s" % (type(err).__name__, err))
    finally:
        for proc in PROCS:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
    sys.exit(max(code, write_evidence()))
