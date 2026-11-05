#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# THE LANE'S MEMORY -- runs a command and says the most memory its processes held at once, and
# warns when that peak passes a limit. CI's sanitizer job runs its lane under it (VM-LANE-27 in
# agents/verification/lanes.md says why and what the limit is); any lane can be run under it.
#
#   python3 tests/lane_memory.py [--limit-gb <gb>] [--every <s>] [--samples <file>]
#                                [--title <title>] -- <command> [<args>...]
#   python3 tests/lane_memory.py --self-test
#
# What it counts: every process the command starts, however deep, as long as it runs -- the tool
# makes itself the child subreaper (Linux), so a process whose parent ended is still counted --
# each by its anonymous and shared memory (`RssAnon` + `RssShmem` in /proc/<pid>/status), which
# is what the machine cannot drop to make room; files read are left out. A child that holds its
# parent's memory (CLONE_VM: a vfork, LeakSanitizer's tracer as a process exits) is counted once,
# in its parent. Beside it, the machine's used memory (`MemTotal` - `MemAvailable`). Sampled
# every `--every` seconds (1).
#
# What it says, on standard output, once the command has ended:
#   lane-memory: peak <x> GB in <n> process(es), at <m:ss> of <m:ss>; the machine's used memory
#                peaked at <y> GB (from <z> GB)
#   lane-memory: <n> process(es) still running after the command ended: ...   (each one named,
#                then stopped: TERM, and KILL five seconds later)
#   a warning past `--limit-gb`: `::warning title=...::...` under GitHub Actions, where it marks
#                the run, else a `lane-memory: warning:` line
# `--title` names the warning ("Memory"). `--samples` writes each sample as a row: seconds, the
# processes' kB, the machine's used kB, the process count.
# Its exit is the command's own; 2 when it cannot start the command. `--self-test` runs its
# fixed cases, each a real process, and exits 1 when one is bad.

import ctypes
import os
import signal
import subprocess
import sys
import tempfile
import time

PR_SET_CHILD_SUBREAPER = 36
KB_PER_GB = 1024 * 1024


def gb(kb):
    return f"{kb / KB_PER_GB:.1f}"


def clock(seconds):
    seconds = int(round(seconds))
    return f"{seconds // 60}:{seconds % 60:02d}"


def read_meminfo():
    fields = {}
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                key, _, rest = line.partition(":")
                fields[key] = int(rest.split()[0])
        return fields["MemTotal"] - fields["MemAvailable"]
    except (OSError, KeyError, ValueError, IndexError):
        return None


def parent_of(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            stat = f.read()
        return int(stat[stat.rindex(")") + 2:].split()[1])
    except (OSError, ValueError, IndexError):
        return None


def held_kb(pid):
    total = 0
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if line.startswith(("RssAnon:", "RssShmem:")):
                    total += int(line.split()[1])
    except (OSError, ValueError, IndexError):
        return 0
    return total


def command_line(pid):
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            words = f.read().split(b"\0")
        return " ".join(w.decode(errors="replace") for w in words if w)[:200]
    except OSError:
        return "?"


def descendants(root):
    """Every live process beneath root, with its parent, read from /proc in one pass."""
    children = {}
    for name in os.listdir("/proc"):
        if name.isdigit():
            pid = int(name)
            ppid = parent_of(pid)
            if ppid is not None:
                children.setdefault(ppid, []).append(pid)
    found, todo = [], [root]
    while todo:
        parent = todo.pop()
        for child in children.get(parent, []):
            found.append((child, parent))
            todo.append(child)
    return found


KCMP_VM = 1
KCMP_CALL = {"x86_64": 312, "aarch64": 272}.get(os.uname().machine)


def same_memory(libc, a, b):
    """A child made with CLONE_VM (a vfork, LeakSanitizer's tracer) holds its parent's memory."""
    return libc is not None and KCMP_CALL is not None and \
        libc.syscall(KCMP_CALL, a, b, KCMP_VM, 0, 0) == 0


class Watch:
    def __init__(self):
        self.main = None
        self.status = None

    def reap(self, *_):
        while True:
            try:
                pid, status = os.waitpid(-1, os.WNOHANG)
            except ChildProcessError:
                return
            if pid == 0:
                return
            if pid == self.main:
                self.status = status


def run(args):
    if not args.command:
        print("lane-memory: no command to run (put it after --)", file=sys.stderr)
        return 2
    me = os.getpid()
    watching = os.path.isdir("/proc") and read_meminfo() is not None
    libc = None
    if watching:
        libc = ctypes.CDLL(None, use_errno=True)
        libc.prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0)
    watch = Watch()
    signal.signal(signal.SIGCHLD, watch.reap)
    signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGCHLD})   # its pid is known before it ends
    try:
        watch.main = os.posix_spawnp(args.command[0], args.command, dict(os.environ),
                                     setsigmask=())
    except OSError as e:
        print(f"lane-memory: cannot start {args.command[0]}: {e}", file=sys.stderr)
        return 2
    finally:
        signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGCHLD})
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda s, _f: os.kill(watch.main, s))
    watch.reap()

    began = time.monotonic()
    machine_from = read_meminfo() if watching else None
    peak = (0, 0.0, 0)
    machine_peak = machine_from or 0
    rows = []
    while watch.status is None:
        if watching:
            pids = [p for p, parent in descendants(me) if not same_memory(libc, p, parent)]
            held = sum(held_kb(p) for p in pids)
            used = read_meminfo() or 0
            at = time.monotonic() - began
            rows.append((at, held, used, len(pids)))
            if held > peak[0]:
                peak = (held, at, len(pids))
            machine_peak = max(machine_peak, used)
        time.sleep(args.every)
        watch.reap()
    took = time.monotonic() - began
    code = os.waitstatus_to_exitcode(watch.status)

    lines, warning = [], None
    if not watching:
        lines.append("lane-memory: cannot read /proc here; the peak is unknown")
    else:
        lines.append(f"lane-memory: peak {gb(peak[0])} GB in {peak[2]} process(es), at "
                     f"{clock(peak[1])} of {clock(took)}; the machine's used memory peaked at "
                     f"{gb(machine_peak)} GB (from {gb(machine_from)} GB)")
        watch.reap()
        left = [p for p, _ in descendants(me) if not zombie(p)]
        if left:
            named = "; ".join(f"pid {p}: {command_line(p)}" for p in left)
            lines.append(f"lane-memory: {len(left)} process(es) still running after the command "
                         f"ended, stopped now: {named}")
            stop(left, watch)
        if args.limit_gb is not None and peak[0] > args.limit_gb * KB_PER_GB:
            warning = (f"The peak, {gb(peak[0])} GB at {clock(peak[1])}, is past "
                       f"{args.limit_gb:g} GB")
    for line in lines:
        print(line, flush=True)
    actions = os.environ.get("GITHUB_ACTIONS") == "true"
    if warning:
        if actions:
            print(f"::warning title={args.title}: past {args.limit_gb:g} GB::"
                  f"{warning}", flush=True)
        else:
            print(f"lane-memory: warning: {warning}", flush=True)
    if args.samples:
        with open(args.samples, "w") as f:
            f.write("seconds\tprocesses_kb\tmachine_used_kb\tprocesses\n")
            for at, held, used, n in rows:
                f.write(f"{at:.1f}\t{held}\t{used}\t{n}\n")
    return code


def stop(pids, watch):
    for sig, wait in ((signal.SIGTERM, 5.0), (signal.SIGKILL, 1.0)):
        for p in pids:
            try:
                os.kill(p, sig)
            except ProcessLookupError:
                pass
        until = time.monotonic() + wait
        while time.monotonic() < until:
            watch.reap()
            pids = [p for p in pids if os.path.exists(f"/proc/{p}")
                    and not zombie(p)]
            if not pids:
                return
            time.sleep(0.1)


def zombie(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            stat = f.read()
        return stat[stat.rindex(")") + 2] == "Z"
    except (OSError, ValueError, IndexError):
        return True


# ---- its fixed cases ----------------------------------------------------------------------------

HOLD = ("import sys, time; b = bytearray(int(sys.argv[1]) * 1024 * 1024); "
        "b[::4096] = b'x' * len(b[::4096]); time.sleep(1.5)")
NEST = "import subprocess, sys; raise SystemExit(subprocess.call([sys.executable, *sys.argv[1:]]))"
# A CLONE_VM child that runs only libc's sleep(2) on a stack of its own, beside 256 MB touched.
SHARE = ("import ctypes, os, time; libc = ctypes.CDLL(None, use_errno=True); "
         "b = bytearray(256 * 1024 * 1024); b[::4096] = b'x' * len(b[::4096]); "
         "stack = ctypes.create_string_buffer(1 << 16); "
         "top = ctypes.c_void_p(ctypes.addressof(stack) + (1 << 16) - 64); "
         "pid = libc.clone(ctypes.cast(libc.sleep, ctypes.c_void_p), top, 0x100 | 17, "
         "ctypes.c_void_p(2)); "
         "assert pid > 0, os.strerror(ctypes.get_errno()); time.sleep(1.5); os.waitpid(pid, 0)")
LEAVE = ("import subprocess, sys; subprocess.Popen([sys.executable, '-c', "
         "'import time; time.sleep(60)', sys.argv[1]], start_new_session=True)")


def self_test():
    me = os.path.abspath(__file__)
    cases = []

    def case(name, tool_args, child, expect):
        env = dict(os.environ, GITHUB_ACTIONS="true")
        with tempfile.TemporaryFile("w+") as out:   # a pipe would wait for a process left running
            done = subprocess.run([sys.executable, me, *tool_args, "--", *child], stdout=out,
                                  stderr=subprocess.STDOUT, text=True, env=env, timeout=60)
            out.seek(0)
            done.stdout = out.read()
        why = expect(done)
        cases.append((name, why))
        shown = done.stdout.replace("::warning", "[warning]").strip().replace("\n", "\n    ")
        print(f"lane-memory: case '{name}': {'ok' if why is None else 'BAD -- ' + why}\n    {shown}")

    def peak_of(out):
        for line in out.splitlines():
            if line.startswith("lane-memory: peak "):
                return float(line.split()[2])
        return None

    def machine_of(out):
        for line in out.splitlines():
            if line.startswith("lane-memory: peak ") and "used memory peaked at " in line:
                return float(line.split("used memory peaked at ")[1].split()[0])
        return None

    def past(done):
        if done.returncode != 0:
            return f"exit {done.returncode}, not the command's 0"
        if (peak_of(done.stdout) or 0) < 0.2:
            return "the peak does not hold the 256 MB the command touched"
        if "::warning title=Lane: past 0.1 GB::The peak, " not in done.stdout:
            return "no warning past the limit"
        return None

    def under(done):
        if (peak_of(done.stdout) or 0) < 0.2:
            return "the peak does not hold the 256 MB the command touched"
        if "warning" in done.stdout:
            return "a warning under the limit"
        if (machine_of(done.stdout) or 0) < peak_of(done.stdout):
            return "the machine's used memory is less than the command's own"
        return None

    def shared_once(done):
        if done.returncode != 0:
            return f"exit {done.returncode}: the command could not make its CLONE_VM child"
        peak = peak_of(done.stdout) or 0
        if peak < 0.2 or peak > 0.4:
            return f"{peak} GB, not the 256 MB the two processes share, counted once"
        return None

    def exit_kept(done):
        return None if done.returncode == 3 else f"exit {done.returncode}, not the command's 3"

    marker = f"lane-memory-left-behind-{os.getpid()}"

    def left(done):
        running = [int(n) for n in os.listdir("/proc") if n.isdigit()
                   and marker in command_line(int(n)) and not zombie(int(n))]
        for pid in running:
            os.kill(pid, signal.SIGKILL)
        if "still running after the command ended" not in done.stdout \
                or marker not in done.stdout:
            return "the process left running is not named"
        if running:
            return f"pid {running[0]}, left running, was not stopped"
        return None

    py = sys.executable
    case("past the limit, a warning, its holder a grandchild",
         ["--limit-gb", "0.1", "--every", "0.1", "--title", "Lane"],
         [py, "-c", NEST, "-c", HOLD, "256"], past)
    case("under the limit, none", ["--limit-gb", "1", "--every", "0.1", "--title", "Lane"],
         [py, "-c", HOLD, "256"], under)
    case("memory two processes share, counted once", ["--every", "0.1"], [py, "-c", SHARE],
         shared_once)
    case("the command's exit", ["--every", "0.1"], [py, "-c", "raise SystemExit(3)"], exit_kept)
    case("a process left running, named and stopped", ["--every", "0.1"],
         [py, "-c", LEAVE, marker], left)
    bad = sum(1 for _, why in cases if why is not None)
    print(f"lane-memory: self-test: {len(cases)} case(s), {bad} bad")
    return 1 if bad else 0


def main():
    import argparse
    p = argparse.ArgumentParser(description="Run a command; say its processes' peak memory.")
    p.add_argument("--limit-gb", type=float)
    p.add_argument("--every", type=float, default=1.0)
    p.add_argument("--samples")
    p.add_argument("--title", default="Memory")
    p.add_argument("--self-test", action="store_true")
    p.add_argument("command", nargs=argparse.REMAINDER)
    args = p.parse_args()
    if args.self_test:
        return self_test()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
