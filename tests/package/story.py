# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The story as the package's witness: the tally panel exported from a package's Workshop as an
app, which runs in the package's own `zengine-app` and outlives Workshop.

    python story.py <package folder> <work folder> [--env NAME=VALUE]... [--seconds N]

With the package's own demo service, `share/zengine/loom-tools/demo.py`, the package in place of a
build tree and a Loom prefix (`demo.py start --package`):
  1 the tally setup (`examples/tally-app/` of the checkout this script stands in) starts, ready, in
    the OS window: Workshop says it loaded `zengine-window-sdl`, and nothing stood in its place;
  2 its walk `export` passes: Flow and the View Builder opened, `tally.view` opened, Export app,
    each step's picture kept in <work folder>/pictures;
  3 the app's launcher (`run.cmd` on Windows, `run.sh` elsewhere) names the package's own
    `zengine-app`;
  4 that program, started on the app's folder, is answered at its console line by line, each
    `authority trust` line it asks for, until it says every row of its plan runs;
  5 Workshop is stopped, and its process has ended;
  6 the app still runs, and ends at `quit`, exit 0.
--env sets a variable for Workshop and the app (SDL_VIDEO_DRIVER=offscreen, say); --seconds bounds
each wait (300). Each step is said on its own line. Exit 0 when every step held; 1 at the first that
did not, which it names; 2 on a usage error.
"""
import json
import os
import queue
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2]
#: The demo service a package carries, beside the tool packages it approves.
PACKAGED_DEMO = Path("share") / "zengine" / "loom-tools" / "demo.py"
DEMO = None  # the package's, once it is known
EXE = ".exe" if os.name == "nt" else ""
#: The console's ask, in the app's own words: `approve it at the console with:  <line>`.
ASKED = re.compile(r"approve it at the console with:\s+(authority trust \S+)")
#: ...and its word once every row runs: `running: <n> of <n> artifacts`.
RUNNING = re.compile(r"running: (\d+) of (\d+) artifacts")


class Failed(Exception):
    pass


def say(step, text):
    print("story %s: %s" % (step, text), flush=True)


def demo(env, seconds, *args):
    """One `demo.py` action; its answer is the JSON it prints."""
    done = subprocess.run([sys.executable, "-X", "utf8", str(DEMO)] + list(args), env=env,
                          capture_output=True, text=True, encoding="utf-8", timeout=seconds + 60)
    try:
        return json.loads(done.stdout)
    except ValueError:
        raise Failed("demo.py %s answered no JSON (exit %d): %s" % (args[0], done.returncode,
                                                                   (done.stderr or done.stdout)[-1200:]))


def named_program(launcher):
    """The program an exported app's launcher starts, as it writes it."""
    text = launcher.read_text(encoding="utf-8")
    found = re.search(r'set "app=([^"]+)"', text) or re.search(r"^app='([^']+)'", text, re.M)
    if not found:
        raise Failed("%s names no program" % launcher)
    return found.group(1)


def same_file(a, b):
    a, b = os.path.normpath(a), os.path.normpath(b)
    return a.lower() == b.lower() if os.name == "nt" else a == b


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    package, work = Path(argv[1]).resolve(), Path(argv[2]).resolve()
    env, seconds, rest = dict(os.environ), 300.0, argv[3:]
    while rest:
        if rest[0] == "--env" and len(rest) > 1 and "=" in rest[1]:
            name, _, value = rest[1].partition("=")
            env[name] = value
        elif rest[0] == "--seconds" and len(rest) > 1:
            seconds = float(rest[1])
        else:
            print("story: %r is not an option this script takes" % rest[0], file=sys.stderr)
            return 2
        rest = rest[2:]
    if not (package / "bin" / ("zengine-workshop" + EXE)).is_file():
        print("story: %s holds no bin/zengine-workshop%s" % (package, EXE), file=sys.stderr)
        return 2
    global DEMO
    DEMO = package / PACKAGED_DEMO
    work.mkdir(parents=True, exist_ok=True)
    root = work / "root"
    if not DEMO.is_file():
        raise Failed("1 the package carries no demo service: %s is not there"
                     % PACKAGED_DEMO.as_posix())
    started = demo(env, seconds, "start", "--setup", str(SOURCE / "examples" / "tally-app"),
                   "--package", str(package),
                   "--root", str(root), "--wait", str(seconds))
    if started.get("state") != "ready":
        raise Failed("1 the tally setup did not come up ready: %s" % started.get("note", started))
    # IN THE WINDOW IT MEANS: a fallback to the terminal window never passes for the OS window.
    log = (root / "workshop" / "process.log").read_text(encoding="utf-8", errors="replace")
    if "loaded: zengine-window-sdl" not in log or "did not load, so" in log:
        raise Failed("1 Workshop did not start in the OS window; it said:\n%s" % log[-2000:])
    say(1, "ready in the OS window: %s" % started.get("note", ""))
    app = None
    try:
        walked = demo(env, seconds, "walk", "export", "--root", str(root), "--pictures",
                      str(work / "pictures"), "--wait", str(seconds))
        if walked.get("state") != "passed":
            raise Failed("2 the walk export did not pass: %s" % walked.get("failure", walked))
        say(2, "the walk export passed, %d steps; pictures in %s" % (walked["steps"], walked["folder"]))

        folder = root / "project" / "apps" / "tally"
        launcher = folder / ("run.cmd" if os.name == "nt" else "run.sh")
        program = package / "bin" / ("zengine-app" + EXE)
        named = named_program(launcher)
        if not same_file(named, program):
            raise Failed("3 %s names %s, not the package's %s" % (launcher.name, named, program))
        say(3, "%s names the package's %s" % (launcher.name, program))

        app = subprocess.Popen([str(program), str(folder)], cwd=str(folder), env=env,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
        lines = queue.Queue()
        threading.Thread(target=lambda: [lines.put(raw.decode("utf-8", "replace").rstrip())
                                         for raw in app.stdout], daemon=True).start()
        said, answered, deadline = [], 0, time.monotonic() + seconds
        while True:
            try:
                line = lines.get(timeout=max(0.1, deadline - time.monotonic()))
            except queue.Empty:
                raise Failed("4 the app did not say every row runs within %gs; it said:\n%s"
                             % (seconds, "\n".join(said[-30:])))
            said.append(line)
            asked = ASKED.search(line)
            if asked:
                app.stdin.write((asked.group(1) + "\n").encode())
                app.stdin.flush()
                answered += 1
            runs = RUNNING.search(line)
            if runs and runs.group(1) == runs.group(2):
                break
            if app.poll() is not None and lines.empty():
                raise Failed("4 the app ended (exit %s) before every row ran; it said:\n%s"
                             % (app.returncode, "\n".join(said[-30:])))
        (work / "app-console.txt").write_text("\n".join(said) + "\n", encoding="utf-8")
        say(4, "%s after %d answer(s)" % (runs.group(0), answered))

        record = json.loads((root / "demo.json").read_text(encoding="utf-8"))
        stopped = demo(env, seconds, "stop", "--root", str(root))
        root = None
        if stopped.get("state") != "stopped":
            raise Failed("5 Workshop did not stop: %s" % stopped)
        ended = time.monotonic() + 30
        while alive(record["workshop_pid"]) and time.monotonic() < ended:
            time.sleep(0.2)
        if alive(record["workshop_pid"]):
            raise Failed("5 Workshop (pid %s) still runs after its stop" % record["workshop_pid"])
        say(5, "Workshop stopped, its process (pid %s) ended" % record["workshop_pid"])

        time.sleep(3)
        if app.poll() is not None:
            raise Failed("6 the app ended (exit %s) with Workshop" % app.returncode)
        app.stdin.write(b"quit\n")
        app.stdin.flush()
        code = app.wait(timeout=30)
        if code != 0:
            raise Failed("6 the app outlived Workshop, and ended at quit with exit %d, not 0" % code)
        say(6, "the app outlived Workshop and ended at quit, exit 0")
        return 0
    finally:
        if app is not None and app.poll() is None:
            app.kill()
        if root is not None:
            demo(env, seconds, "stop", "--root", str(root))


def alive(pid):
    """Whether a process of this id still runs."""
    if os.name == "nt":
        listed = subprocess.run(["tasklist", "/FI", "PID eq %d" % int(pid), "/NH"],
                                capture_output=True, text=True)
        return str(int(pid)) in listed.stdout
    try:
        os.kill(int(pid), 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    try:
        sys.exit(main(sys.argv))
    except Failed as failed:
        print("story FAILED at %s" % failed, flush=True)
        sys.exit(1)
