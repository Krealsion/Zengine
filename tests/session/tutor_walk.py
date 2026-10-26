# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""THE TUTOR'S WALK, LIVE -- a guest teaches on a weaver's desk in a terminal, and the weaver presses.

A real `zengine-workshop` in a terminal of its own -- a pseudo-terminal this driver holds, so every
key and press it writes there is the weaver's own hand, the local device's -- with a guests file of
version 3 on a weaver's host: the `tutor` row (role `tutor`, reading and pointing at the Builder and
the Pane Manager), a row reading every pane, a row reading the Builder alone, and a guest's hand (input
and capture, following the desk as a launcher writes its row). A real session host from an INSTALLED
Loom (`loom-host --serve`) links to it under each row and runs the `desk` package's `desk/resolve` and
`desk/guide`, and the hand's `workshop/act`. The walk: the plan at rest before the first ask; the
hand's `s` refused in Workshop's words, since a guest writes no file here; the Builder hidden,
so `BuildRequested` resolves to the route through the Pane Manager and the desktop's own key, and
nothing is marked; the weaver's Ctrl+P; the Pane Manager's row for the Builder marked as the nearest
step; the weaver's press there opens the Builder and ends the mark; `builder.build` shown and marked
by the address a press names; the weaver's press inside it ends that mark; the Builder's other mode
reached through the row that enters it, never called a case for a tool; a message no control causes
drafted as the case for a tool by the row that reads every pane; and the Pane Manager named outside
the read of the row that reads the Builder alone. Every claim is read from its owner's answer: the
tools' results, which quote Workshop's.

POSIX only: the weaver's hand is the pseudo-terminal's.

    tutor_walk.py --workshop <zengine-workshop> --plan <load plan>
                  --loom-host <loom-host> --loom-runs <loom-runs artifact>
                  --loom-runtime <dir holding loom_session>
                  --vocabulary <zengine-guest-vocabulary artifact>
                  --tools <external-host/tools> --work <dir> [--evidence <file.json>]

Exit 0 only when every check held.
"""

import argparse
import fcntl
import json
import os
import pty
import re
import shutil
import struct
import subprocess
import sys
import termios
import threading
import time

CHECKS = []
PROCS = []
EVIDENCE = {"path": "", "notes": {}}
COLUMNS, ROWS = 160, 48
ROWS_FILE = {
    "tutor": {"credential": "tutor-key", "role": "tutor",
              "capture": ["zengine.builder-pane/*", "zengine.desktop/launcher"],
              "guide": ["zengine.builder-pane/*", "zengine.desktop/launcher"]},
    "whole": {"credential": "whole-key", "may": ["capture", "guide", "inspect"], "capture": ["*"],
              "guide": ["*"]},
    "narrow": {"credential": "narrow-key", "role": "tutor", "capture": ["zengine.builder-pane/*"],
               "guide": ["zengine.builder-pane/*"]},
}


def check(name, condition, detail=""):
    CHECKS.append({"check": name, "ok": bool(condition), "detail": str(detail)})
    shown = str(detail).splitlines()[0] if detail else ""
    print("%s  %s%s" % ("ok  " if condition else "FAIL", name, ("  -- " + shown) if shown else ""),
          flush=True)
    if not condition and detail:
        print("      " + str(detail).replace("\n", "\n      "), flush=True)
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


class Terminal(object):
    """The weaver's terminal: what Workshop draws is drained and kept; what is written is the
    weaver's own hand."""

    def __init__(self):
        self.master, self.slave = pty.openpty()
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, COLUMNS, 0, 0))
        self.drawn = bytearray()
        self.lock = threading.Lock()
        self.reader = threading.Thread(target=self._drain, daemon=True)

    def start(self):
        self.reader.start()

    def _drain(self):
        while True:
            try:
                chunk = os.read(self.master, 65536)
            except OSError:
                return
            if not chunk:
                return
            with self.lock:
                self.drawn += chunk
                if len(self.drawn) > (8 << 20):
                    del self.drawn[: len(self.drawn) - (4 << 20)]

    def write(self, data):
        os.write(self.master, data)

    def key(self, data):
        self.write(data)
        time.sleep(0.3)

    def press(self, x, y):
        """A primary press and release at a cell, 0-based, as an SGR mouse report says it."""
        self.write(b"\x1b[<0;%d;%dM" % (x + 1, y + 1))
        time.sleep(0.05)
        self.write(b"\x1b[<0;%d;%dm" % (x + 1, y + 1))
        time.sleep(0.3)


def package_for(src, dest, package, link):
    """A copy of the `desk` package whose tools ask through `link`, as a second guest row's are."""
    shutil.copytree(src, dest, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    if link == "workshop":
        return
    with open(os.path.join(dest, "loom-tool.json"), "r", encoding="utf-8") as f:
        m = json.load(f)
    m["package"] = package
    for t in m["tools"]:
        t["asks"] = [a.replace("loom.link.workshop", "loom.link." + link) for a in t["asks"]]
        for i in t["inputs"]:
            if i["name"] == "link":
                i["default"] = link
    with open(os.path.join(dest, "loom-tool.json"), "w", encoding="utf-8") as f:
        json.dump(m, f, indent=1)


def main():
    ap = argparse.ArgumentParser()
    names = ("workshop", "plan", "loom_host", "loom_runs", "loom_runtime", "vocabulary", "tools", "work")
    for name in names:
        ap.add_argument("--" + name.replace("_", "-"), required=True)
    ap.add_argument("--evidence", default="")
    args = ap.parse_args()
    EVIDENCE["path"] = args.evidence
    for name in names:
        setattr(args, name, os.path.abspath(getattr(args, name)))
    sys.path.insert(0, args.loom_runtime)
    from loom_session.session import Session, read_session_file

    work = args.work
    shutil.rmtree(work, ignore_errors=True)
    wdir = os.path.join(work, "workshop")
    sdir = os.path.join(work, "session")
    pkgs = os.path.join(work, "packages")
    os.makedirs(wdir)
    os.makedirs(sdir)

    # ---- the Workshop: a weaver's host, a guests file of version 3, in a terminal of its own ------
    # THE HAND'S ROW follows the desk as a launcher writes it (`following_the_desk`), and so hears
    # what Workshop says.
    sys.path.insert(0, os.path.join(args.tools, "workshop"))
    import workshop_formats as formats
    hand_row = formats.following_the_desk({"name": "hand", "credential": "hand-key",
                                           "may": ["input", "capture"], "capture": ["*"]})
    with open(os.path.join(wdir, "guests.json"), "w", encoding="utf-8") as f:
        json.dump({"version": "3", "host": "weaver", "listen": "127.0.0.1:0", "port_file": "guests.port",
                   "guests": [dict(name=n, **r) for n, r in ROWS_FILE.items()] + [hand_row]}, f,
                  indent=1)
    env = dict(os.environ)
    for var in ("APPDATA", "LOCALAPPDATA", "XDG_CONFIG_HOME", "XDG_STATE_HOME", "XDG_DATA_HOME"):
        env[var] = os.path.join(wdir, "profile", var.lower())
        os.makedirs(env[var], exist_ok=True)
    env["TERM"] = "xterm-256color"
    term = Terminal()
    err = open(os.path.join(wdir, "workshop.err"), "wb")
    workshop = subprocess.Popen([args.workshop, "--isolated", "--load-plan", args.plan, "--tui",
                                 "--guests", "guests.json", "--log", "workshop.log"],
                                cwd=wdir, env=env, stdin=term.slave, stdout=term.slave, stderr=err,
                                start_new_session=True)
    os.close(term.slave)
    term.start()
    PROCS.append(workshop)
    port_file = os.path.join(wdir, "guests.port")
    if not check("W0 Workshop listens for its guests, in a terminal of its own", until(
            lambda: os.path.exists(port_file) and open(port_file).read().strip(), 60, "Workshop's port")):
        return 1
    port = int(open(port_file).read().strip())
    note("workshop_pid", workshop.pid)

    # ---- the session: the desk package under three rows, and the Workshop package's plan wait -----
    desk_src = os.path.join(args.tools, "desk")
    package_for(desk_src, os.path.join(pkgs, "desk"), "desk", "workshop")
    package_for(desk_src, os.path.join(pkgs, "desk-whole"), "desk-whole", "whole")
    package_for(desk_src, os.path.join(pkgs, "desk-narrow"), "desk-narrow", "narrow")
    # The reader that writes the desk as files, run only to show a failure, builds on `desk`.
    package_for(os.path.join(args.tools, "desk-machine"), os.path.join(pkgs, "desk-machine"),
                "desk-machine", "workshop")
    # THE PLAN WAIT goes through the row that reads every pane and the door's inventory.
    package_for(os.path.join(args.tools, "workshop"), os.path.join(pkgs, "workshop-whole"),
                "workshop-whole", "whole")
    # A GUEST'S HAND, pressing through `workshop/act` under the row that may press.
    package_for(os.path.join(args.tools, "workshop"), os.path.join(pkgs, "workshop-hand"),
                "workshop-hand", "hand")
    with open(os.path.join(sdir, "loom-tools.json"), "w", encoding="utf-8") as f:
        json.dump({"python": sys.executable, "runtime": args.loom_runtime, "packages": [
            {"path": os.path.join(pkgs, p), "approve": "any-revision"}
            for p in ("desk", "desk-machine", "desk-whole", "desk-narrow", "workshop-whole",
                      "workshop-hand")]}, f, indent=1)
    links = [("workshop", "tutor-key"), ("whole", "whole-key"), ("narrow", "narrow-key"),
             ("hand", "hand-key")]
    with open(os.path.join(sdir, "loom-boot.json"), "w", encoding="utf-8") as f:
        json.dump({"boot": [{"name": "runs", "path": args.loom_runs, "role": "loom.runs"},
                            {"name": "vocab", "path": args.vocabulary}],
                   "links": [{"name": n, "connect": "127.0.0.1:%d" % port, "identity": "tutor-" + n,
                              "credential": c} for n, c in links],
                   "history": {"log": "session.log", "recent": "4096"}}, f, indent=1)
    approvals = ["authority trust runs --rebuilds", "authority trust vocab --rebuilds"]
    for shape in ("loom.session.ExpectRun", "loom.session.ForgetRun"):
        approvals.append("authority allow runs %s v1 -> role loom.session" % shape)
    for shape in ("loom.runs.Tools", "loom.runs.ToolDescription", "loom.runs.Run", "loom.runs.RunList",
                  "loom.runs.Directive"):
        approvals.append("authority allow runs %s v1 -> any target" % shape)
    for n, _ in links:
        approvals.append("authority allow runs loom.link.Ask v1 -> role loom.link.%s" % n)
        approvals.append("authority allow runs loom.link.StatusRequested v1 -> role loom.link.%s" % n)
    approvals += ["start vocab %s" % args.vocabulary, "start runs %s loom.runs" % args.loom_runs]
    log = open(os.path.join(sdir, "host.log"), "wb")
    host = subprocess.Popen([args.loom_host, "--serve", sdir], cwd=sdir, stdin=subprocess.PIPE,
                            stdout=log, stderr=subprocess.STDOUT)
    host.stdin.write(("\n".join(approvals) + "\n").encode("utf-8"))
    host.stdin.close()
    PROCS.append(host)

    def serving():
        if read_session_file(sdir).get("pid") != host.pid:
            return False
        with Session.attach(sdir) as s:
            s.tools()
            said = dict((l["name"], l) for l in s.describe()["links"])
            return all(said.get(n, {}).get("state") == "admitted" for n, _ in links)

    if not check("S1 the session serves, and Workshop admitted each link as its own row",
                 until(serving, 60, "the session")):
        return 1

    with Session.attach(sdir) as s:
        counter = {"n": 0}

        def run(tool, inputs, seconds=60):
            counter["n"] += 1
            name = "%s-%d" % (tool.replace("/", "-"), counter["n"])
            s.start(tool, name, inputs)
            done = s.wait(name, timeout=seconds)
            text = done.get("summary") or done.get("failure") or ""
            note(name, {"inputs": inputs, "state": done["state"], "said": text})
            return done["state"], text

        state, said = run("workshop-whole/connections", {"wait_plan": True})
        if not check("S2 Workshop says its plan came to rest, complete, before the first ask",
                     state == "passed" and said.startswith("Workshop's plan is complete"), said):
            return 1

        # ---- a guest's press Workshop refuses ------------------------------------------------
        state, said = run("workshop-hand/act", {"link": "hand", "steps": json.dumps([{"press": "s"}])})
        check("A1 a guest's `s` through workshop/act fails in Workshop's words: on a weaver's host the "
              "setup is not saved",
              state != "passed" and "step 0 (press): Workshop refused it: the setup was not saved -- "
              "this is a weaver's host: only the weaver's hand writes a file here" in said, said)

        # ---- a step whose control is not on the desk ---------------------------------------
        state, said = run("desk/resolve", {"shape": "BuildRequested", "mark": "yes", "words": "build"})
        check("R1 the Builder hidden: BuildRequested resolves to the route through the Pane Manager and "
              "the desktop's own key, and nothing is marked",
              state == "passed" and "zengine.builder-pane/builder:builder.build [hidden" in said and
              "zengine.desktop:desktop.panes" in said and "not marked" in said, said)
        term.key(b"\x10")  # the weaver's Ctrl+P
        # THE ROW MAY STAND BELOW THE LIST'S FOOT: the resolution says its part is not shown, and the
        # weaver scrolls down, a row at a time, until it shows and is marked.
        for _ in range(24):
            state, said = run("desk/resolve", {"shape": "BuildRequested", "mark": "yes",
                                               "words": "open the Builder here"})
            marked = re.search(r"marked zengine\.desktop/launcher:launcher\.open \(the nearest step", said)
            row = re.search(r"part pane:zengine\.builder-pane/builder@(\d+),(\d+) shown", said)
            if marked and row:
                break
            if "part pane:zengine.builder-pane/builder not shown" in said:
                term.key(b"\x1b[B")  # the weaver's Down
            else:
                time.sleep(0.5)
        if not check("R2 after the weaver's Ctrl+P the Pane Manager's row for the Builder is marked as the "
                     "nearest step", state == "passed" and marked and row, said):
            out = os.path.join(work, "desk-read")
            run("desk-machine/read", {"out": out})
            panes = os.path.join(out, "panes")
            for name in sorted(os.listdir(panes)) if os.path.isdir(panes) else []:
                if "launcher" in name:
                    print(open(os.path.join(panes, name), encoding="utf-8").read()[:3000])
            return 1
        term.press(int(row.group(1)), int(row.group(2)))  # the weaver presses the marked row

        # ---- a step whose control is on the desk ------------------------------------------
        state, said = run("desk/resolve", {"shape": "BuildRequested"})
        build = re.search(r"zengine\.builder-pane/builder:builder\.build \[open; part "
                          r"control:builder\.build@(\d+),(\d+) shown", said)
        if not check("R3 the weaver's press opened the Builder, and builder.build is shown there", state == "passed" and build, said):
            return 1
        state, said = run("desk/guide", {"pane": "zengine.builder-pane/builder", "control": "builder.build",
                                         "words": "build this one"})
        check("G1 builder.build is marked by the address a press names, the guest named",
              state == "passed" and said.startswith("desk/guide: shown") and "as guest 'tutor'" in said, said)
        state, said = run("desk-whole/guide", {"pane": "zengine.builder-pane/builder",
                                               "control": "builder.build"})
        check("G2 while the tutor's mark stands, another guest's mark on the Builder is refused",
              "marks that pane already" in said, said)
        term.press(int(build.group(1)), int(build.group(2)))  # the weaver presses inside the mark
        # THE PRESS STARTS A BUILD, AND THE BUILDER DRAWS AGAIN: a mark waits for its picture to
        # settle, so the ask is made again, a little later, while Workshop says it has not.
        for _ in range(24):
            state, said = run("desk-whole/guide", {"pane": "zengine.builder-pane/builder",
                                                   "control": "builder.build", "words": "here"})
            if "is not on the desk, settled and uncovered, now" not in said:
                break
            time.sleep(0.5)
        check("G3 the weaver's press inside the mark ended it: another guest may mark the Builder now",
              said.startswith("desk/guide: shown"), said)
        run("desk-whole/guide", {"pane": "zengine.builder-pane/builder", "control": ""})

        # ---- another mode ---------------------------------------------------------------------
        term.key(b"l")  # the weaver opens the Builder's output reader
        state, said = run("desk/resolve", {"shape": "BuildRequested", "mark": "yes", "words": "back"})
        check("R4 in the Builder's other mode, BuildRequested is reached through the row that enters it, "
              "and is never a case for a tool",
              state == "passed" and "in its mode browse" in said and "builder.output-close" in said and
              "case for a tool" not in said, said)
        term.key(b"\x1b")

        # ---- a message no control causes, and the scope ---------------------------------------
        state, said = run("desk-whole/resolve", {"shape": "BuildStatusRequested",
                                                 "office": "zengine.builder",
                                                 "why": "the lesson asks where the Builder stands"})
        check("R5 a message no control causes is drafted as the case for a tool, four claims, filed nowhere",
              state == "passed" and "a case for a tool" in said and all(
                  ("%d. " % i) in said for i in (1, 2, 3, 4)) and "filed nowhere" in said, said)
        state, said = run("desk-narrow/resolve", {"shape": "PaneLaunchRequested",
                                                  "subject": "zengine.builder-pane/builder"})
        check("R6 a row reading the Builder alone resolves showing it to nothing it may read, and is told "
              "the Pane Manager is outside its read",
              state == "passed" and "outside its read" in said and "zengine.desktop/launcher" in said and
              "case for a tool" not in said, said)
    return 0


def finish(code):
    for p in reversed(PROCS):
        try:
            os.killpg(p.pid, 15) if p.pid else None
        except Exception:
            try:
                p.terminate()
            except Exception:
                pass
    for p in PROCS:
        try:
            p.wait(timeout=10)
        except Exception:
            p.kill()
    ok = code == 0 and CHECKS and all(c["ok"] for c in CHECKS)
    if EVIDENCE["path"]:
        with open(EVIDENCE["path"], "w", encoding="utf-8") as f:
            json.dump({"checks": CHECKS, "notes": EVIDENCE["notes"], "passed": bool(ok)}, f, indent=1)
    print("tutor walk: %s, %d of %d checks held" % ("PASSED" if ok else "FAILED",
                                                   sum(1 for c in CHECKS if c["ok"]), len(CHECKS)))
    return 0 if ok else 1


if __name__ == "__main__":
    try:
        result = main()
    except Exception as e:
        check("the driver ran to its end", False, repr(e))
        result = 1
    sys.exit(finish(result))
