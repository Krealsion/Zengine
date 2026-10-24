# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A package judge: a program started from a package with an environment naming nothing installed
stays up, loads nothing from outside the package and the system, and says nothing of the machine
that built it.

    python -I first_start.py <package folder> <program> [--seconds N] [--home <empty folder>]
                             [--cwd <folder>] [--env NAME=VALUE]... [--needle <text>]...
                             [--expect <text>]... [-- <program arguments>...]

The program (a path under the package folder) runs with PATH naming only the system and every
per-user folder under --home; --env adds a variable (SDL_VIDEO_DRIVER=offscreen, say). After
--seconds (8) it must still run; every file it has mapped as a module is then named:
  package   under the package folder
  system    Windows: under %SystemRoot%, not a redistributable (pe_imports.REDISTRIBUTABLE);
            elsewhere: under /usr, /lib or /lib64
  FOREIGN   anything else
and what it printed is read for each --needle, and for each --expect it must say (Workshop's
`loaded: zengine-window-sdl`, say: the window a start means). It is then ended. Exit 0 when it
stayed up, nothing was FOREIGN or named and everything expected was said; 1 on a FOREIGN module,
a needle or a text it did not say; 3 when it ended before the look; 2 on a usage error, an empty
population or its own samples misread, which it reads first.
"""
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_imports import REDISTRIBUTABLE  # noqa: E402

NT = os.name == "nt"


def norm(p):
    return os.path.normcase(os.path.normpath(p))


def classify(path, package, sysroot):
    p = norm(path)
    if p.startswith(norm(package) + os.sep):
        return "package"
    if NT:
        if p.startswith(norm(sysroot) + os.sep) and not os.path.basename(p).startswith(REDISTRIBUTABLE):
            return "system"
        return "FOREIGN"
    if p.startswith(("/usr/", "/lib/", "/lib64/")):
        return "system"
    return "FOREIGN"


def modules_nt(pid):
    import ctypes
    from ctypes import wintypes
    k32, psapi = ctypes.WinDLL("kernel32", use_last_error=True), ctypes.WinDLL("psapi", use_last_error=True)
    handle = k32.OpenProcess(0x0400 | 0x0010, False, pid)  # QUERY_INFORMATION | VM_READ
    if not handle:
        return None
    try:
        mods = (wintypes.HMODULE * 2048)()
        need = wintypes.DWORD()
        if not psapi.EnumProcessModulesEx(handle, mods, ctypes.sizeof(mods), ctypes.byref(need), 3):
            return None
        out = []
        for i in range(min(need.value // ctypes.sizeof(wintypes.HMODULE), 2048)):
            buf = ctypes.create_unicode_buffer(32768)
            if psapi.GetModuleFileNameExW(handle, ctypes.c_void_p(mods[i]), buf, 32768):
                out.append(buf.value)
        return out
    finally:
        k32.CloseHandle(handle)


def modules_posix(pid):
    """The files a process has mapped that are modules: its program and every shared object."""
    try:
        exe = os.readlink("/proc/%d/exe" % pid)
        with open("/proc/%d/maps" % pid, encoding="utf-8", errors="replace") as fh:
            maps = fh.read().splitlines()
    except OSError:
        return None
    out = {exe}
    for line in maps:
        parts = line.split(None, 5)
        if len(parts) == 6 and parts[5].startswith("/") and ".so" in os.path.basename(parts[5]):
            out.add(parts[5].replace(" (deleted)", ""))
    return sorted(out)


def bare_env(home, extra):
    os.makedirs(home, exist_ok=True)
    if NT:
        root = os.environ.get("SystemRoot", r"C:\Windows")
        env = {"SystemRoot": root, "windir": root, "SystemDrive": os.environ.get("SystemDrive", "C:"),
               "ComSpec": os.path.join(root, "System32", "cmd.exe"), "PATHEXT": ".COM;.EXE;.BAT;.CMD",
               "OS": "Windows_NT", "PATH": os.path.join(root, "System32") + ";" + root,
               "USERPROFILE": home, "USERNAME": "stranger",
               "APPDATA": os.path.join(home, "AppData", "Roaming"),
               "LOCALAPPDATA": os.path.join(home, "AppData", "Local"),
               "TEMP": os.path.join(home, "AppData", "Local", "Temp"),
               "TMP": os.path.join(home, "AppData", "Local", "Temp")}
        for k in ("PROCESSOR_ARCHITECTURE", "NUMBER_OF_PROCESSORS"):
            if k in os.environ:
                env[k] = os.environ[k]
        for d in ("APPDATA", "LOCALAPPDATA", "TEMP"):
            os.makedirs(env[d], exist_ok=True)
    else:
        env = {"PATH": "/usr/bin:/bin", "HOME": home, "LANG": "C.UTF-8",
               "XDG_CONFIG_HOME": os.path.join(home, ".config"),
               "XDG_STATE_HOME": os.path.join(home, ".local", "state"),
               "XDG_DATA_HOME": os.path.join(home, ".local", "share"),
               "XDG_CACHE_HOME": os.path.join(home, ".cache")}
    for kv in extra:
        name, _, value = kv.partition("=")
        env[name] = value
    return env


def selftest():
    sysroot = r"C:\Windows" if NT else ""
    pkg = os.path.join(tempfile.gettempdir(), "pkg")
    cases = [(os.path.join(pkg, "bin", "a"), "package"), (os.path.join(tempfile.gettempdir(), "x", "a"), "FOREIGN")]
    if NT:
        cases += [(r"C:\Windows\System32\kernel32.dll", "system"), (r"C:\Windows\System32\vcruntime140.dll", "FOREIGN")]
    else:
        cases += [("/usr/lib/x86_64-linux-gnu/libc.so.6", "system"), ("/home/b/build/libx.so", "FOREIGN")]
    for path, want in cases:
        if classify(path, pkg, sysroot) != want:
            print("first_start: its own samples misread %s" % path)
            sys.exit(2)


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    selftest()
    rest = []
    if "--" in argv:
        rest = argv[argv.index("--") + 1:]
        argv = argv[:argv.index("--")]
    if len(argv) < 2:
        print(__doc__)
        return 2
    package, program = os.path.abspath(argv[0]), os.path.abspath(argv[1])

    def opts(flag):
        return [argv[i + 1] for i, a in enumerate(argv) if a == flag and i + 1 < len(argv)]
    seconds = float((opts("--seconds") or ["8"])[-1])
    home = os.path.abspath((opts("--home") or [tempfile.mkdtemp(prefix="first-start-home-")])[-1])
    cwd = os.path.abspath((opts("--cwd") or [home])[-1])
    needles = [n for n in opts("--needle") if n]
    expected = [e for e in opts("--expect") if e]
    if not os.path.isfile(program) or classify(program, package, "") != "package":
        print("first_start: %s is no program in the package %s: an empty population is red" % (program, package))
        return 2
    os.makedirs(cwd, exist_ok=True)
    env = bare_env(home, opts("--env"))
    out_path = os.path.join(home, "first-start-output.txt")
    with open(out_path, "wb") as out:
        proc = subprocess.Popen([program] + rest, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                stdout=out, stderr=subprocess.STDOUT)
        time.sleep(seconds)
        alive = proc.poll() is None
        mods = (modules_nt(proc.pid) if NT else modules_posix(proc.pid)) if alive else None
        if alive:
            proc.kill()
        proc.wait(timeout=30)
    with open(out_path, "rb") as fh:
        said = fh.read().decode("utf-8", "replace")
    if not alive:
        print("first_start: %s ended before %gs (exit %s); it said:\n%s"
              % (os.path.relpath(program, package), seconds, proc.returncode, said[-2000:]))
        return 3
    if not mods:
        print("first_start: the modules of %s could not be read" % os.path.relpath(program, package))
        return 2
    sysroot = os.environ.get("SystemRoot", r"C:\Windows")
    tally, bad = {}, 0
    for m in mods:
        v = classify(m, package, sysroot)
        tally[v] = tally.get(v, 0) + 1
        if v == "FOREIGN":
            bad += 1
            print("FOREIGN  %s" % m)
    for n in needles:
        if n.lower() in said.lower():
            bad += 1
            print("NAMED    its output names %s" % n)
    for e in expected:
        if e not in said:
            bad += 1
            print("MISSING  its output does not say %s" % e)
    print("first_start: %s stayed up %gs; %d module(s) by kind %s; %d problem(s)"
          % (os.path.relpath(program, package), seconds, len(mods), tally, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
