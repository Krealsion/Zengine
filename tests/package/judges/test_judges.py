# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The package judges, judged (CTest `package_judges`): each is red on an empty population, red on
its canary and green on a clean sample, in a folder of its own.

    python -I test_judges.py <work folder>

  private_scan  a needle planted as UTF-8 and as UTF-16LE is found; a folder without it is clean
  pe_imports    a PE32+ image made here importing libstdc++-6.dll is FOREIGN; one importing only
                KERNEL32.dll, beside a system folder holding it, is clean
  elf_judge     (Linux) a copy of the system's `ls`, which needs more than glibc, is FOREIGN; a copy
                of `true` is clean
  first_start   a program that ends at once is red, and one that does not say what is expected
                of it; one that stays up, loading only the system's libraries, is clean
Exit 0 when every judgement is the one expected, 1 otherwise.
"""
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NT = os.name == "nt"
failures = []


def judge(script, *args):
    done = subprocess.run([sys.executable, "-I", os.path.join(HERE, script), *args],
                          capture_output=True, text=True, encoding="utf-8", errors="replace")
    return done.returncode, done.stdout + done.stderr


def expect(label, got, want, said):
    ok = got == want
    print("%s  %s (exit %s, expected %s)" % ("ok  " if ok else "FAIL", label, got, want))
    if not ok:
        failures.append(label)
        print("    " + said.strip().replace("\n", "\n    "))


def fresh(work, name):
    d = os.path.join(work, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    return d


def pe_image(dll):
    """A minimal PE32+ image whose import table names `dll`: headers, one section holding the import
    descriptor, its name and an empty thunk list."""
    pe_off, sec_rva, sec_raw = 0x40, 0x1000, 0x200
    dos = b"MZ" + b"\0" * 0x3A + struct.pack("<I", pe_off)
    coff = b"PE\0\0" + struct.pack("<HHIIIHH", 0x8664, 1, 0, 0, 0, 240, 0x22)
    data_dirs = [(0, 0)] * 16
    data_dirs[1] = (sec_rva, 40)
    opt = struct.pack("<HBBIIIIIQIIHHHHHHIIIIHHQQQQII", 0x20B, 0, 0, 0x200, 0, 0, 0, sec_rva,
                      0x140000000, 0x1000, 0x200, 6, 0, 0, 0, 6, 0, 0, 0x2000, 0x200, 0, 3, 0,
                      0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
    opt += b"".join(struct.pack("<II", *d) for d in data_dirs)
    section = struct.pack("<8sIIIIIIHHI", b".idata", 0x200, sec_rva, 0x200, sec_raw, 0, 0, 0, 0, 0xC0000040)
    head = (dos + coff + opt + section).ljust(sec_raw, b"\0")
    name_rva, thunk_rva = sec_rva + 40, sec_rva + 0x100
    body = struct.pack("<IIIII", thunk_rva, 0, 0, name_rva, thunk_rva) + b"\0" * 20
    body += dll.encode("ascii") + b"\0"
    body = body.ljust(0x100, b"\0") + b"\0" * 8
    return head + body.ljust(0x200, b"\0")


def main(argv):
    work = os.path.abspath(argv[0]) if argv else os.path.join(HERE, "judges-work")
    os.makedirs(work, exist_ok=True)

    # ---- private_scan --------------------------------------------------------------------------
    empty = fresh(work, "empty")
    expect("private_scan: an empty population is red", judge("private_scan.py", empty, "x")[0], 2, "")
    planted = fresh(work, "planted")
    with open(os.path.join(planted, "a.bin"), "wb") as fh:
        fh.write(b"..C:/builder/work/zengine.." + "/home/builder/work".encode("utf-16-le"))
    code, said = judge("private_scan.py", planted, "/home/builder/work")
    expect("private_scan: a needle planted in UTF-16LE is found", code, 1, said)
    code, said = judge("private_scan.py", planted, "c:/BUILDER/work")
    expect("private_scan: a needle planted in UTF-8 is found, whatever its case", code, 1, said)
    code, said = judge("private_scan.py", planted, "/srv/elsewhere")
    expect("private_scan: a folder without the needle is clean", code, 0, said)

    # ---- pe_imports ----------------------------------------------------------------------------
    expect("pe_imports: an empty population is red", judge("pe_imports.py", empty)[0], 2, "")
    system = fresh(work, "system")
    open(os.path.join(system, "kernel32.dll"), "wb").close()
    canary = fresh(work, "pe-canary")
    with open(os.path.join(canary, "a.exe"), "wb") as fh:
        fh.write(pe_image("libstdc++-6.dll"))
    code, said = judge("pe_imports.py", canary, "--system", system)
    expect("pe_imports: an image importing the toolchain's runtime is FOREIGN", code, 1, said)
    clean = fresh(work, "pe-clean")
    with open(os.path.join(clean, "a.exe"), "wb") as fh:
        fh.write(pe_image("KERNEL32.dll"))
    code, said = judge("pe_imports.py", clean, "--system", system)
    expect("pe_imports: an image importing only the system is clean", code, 0, said)

    # ---- elf_judge -----------------------------------------------------------------------------
    if not NT and shutil.which("readelf") and shutil.which("ls") and shutil.which("true"):
        expect("elf_judge: an empty population is red", judge("elf_judge.py", empty)[0], 2, "")
        canary = fresh(work, "elf-canary")
        shutil.copy(shutil.which("ls"), os.path.join(canary, "ls"))
        code, said = judge("elf_judge.py", canary)
        expect("elf_judge: an image needing more than glibc and the package is FOREIGN", code, 1, said)
        clean = fresh(work, "elf-clean")
        shutil.copy(shutil.which("true"), os.path.join(clean, "true"))
        code, said = judge("elf_judge.py", clean)
        expect("elf_judge: an image needing only glibc is clean", code, 0, said)
    else:
        print("skip  elf_judge: no readelf, or not Linux")

    # ---- first_start ---------------------------------------------------------------------------
    pkg = fresh(work, "start-package")
    os.makedirs(os.path.join(pkg, "bin"))
    if NT:
        root = os.environ.get("SystemRoot", r"C:\Windows")
        ends, stays, stay_args = os.path.join(root, "System32", "whoami.exe"), \
            os.path.join(root, "System32", "PING.EXE"), ["-n", "30", "127.0.0.1"]
    else:
        ends, stays, stay_args = shutil.which("true"), shutil.which("sleep"), ["30"]
    shutil.copy(ends, os.path.join(pkg, "bin", os.path.basename(ends)))
    shutil.copy(stays, os.path.join(pkg, "bin", os.path.basename(stays)))
    code, said = judge("first_start.py", pkg, os.path.join(empty, "nothing"))
    expect("first_start: a program not in the package is red", code, 2, said)
    code, said = judge("first_start.py", pkg, os.path.join(pkg, "bin", os.path.basename(ends)),
                       "--seconds", "2", "--home", fresh(work, "home-ends"))
    expect("first_start: a program that ends at once is red", code, 3, said)
    code, said = judge("first_start.py", pkg, os.path.join(pkg, "bin", os.path.basename(stays)),
                       "--seconds", "2", "--home", fresh(work, "home-stays"), "--", *stay_args)
    expect("first_start: a program that stays up on the system's libraries alone is clean", code, 0, said)
    code, said = judge("first_start.py", pkg, os.path.join(pkg, "bin", os.path.basename(stays)),
                       "--seconds", "2", "--home", fresh(work, "home-silent"),
                       "--expect", "loaded: zengine-window-sdl", "--", *stay_args)
    expect("first_start: a program that does not say what is expected of it is red", code, 1, said)

    print("package judges: %d judgement(s) not as expected" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
