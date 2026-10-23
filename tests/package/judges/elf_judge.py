# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A package judge: every Linux image needs only the package, glibc and what it is allowed, and
finds the package beside itself.

    python3 -I elf_judge.py <package folder> [--allow <soname>]...

For every ELF file under the folder, read with `readelf`: each NEEDED soname is
  package   a file of that name is in the package
  glibc     glibc's own family (libc, libm, libdl, libpthread, librt, libresolv, libutil, ld-linux)
  allowed   named by --allow (libstdc++.so.6 and libgcc_s.so.1: the system's C++ runtime)
  FOREIGN   anything else
and a RUNPATH or RPATH holding anything but $ORIGIN-relative entries is BUILD-PATH. Prints the
highest GLIBC_, GLIBCXX_ and CXXABI_ versions any image requires. Exit 0 when nothing is FOREIGN
or BUILD-PATH, 1 otherwise, 2 on an empty population or its own samples misread, read first.
"""
import os
import re
import subprocess
import sys

GLIBC = re.compile(r"^(libc|libm|libdl|libpthread|librt|libresolv|libutil)\.so\.\d+$|^ld-linux-x86-64\.so\.2$")


def readelf(*args):
    return subprocess.run(["readelf", *args], capture_output=True, text=True).stdout


def vkey(v):
    return tuple(int(x) for x in re.findall(r"\d+", v))


def kind_of(needed, present, allowed):
    return ("package" if needed in present else "glibc" if GLIBC.match(needed)
            else "allowed" if needed in allowed else "FOREIGN")


def runpath_bad(entries):
    """A RUNPATH entry that is not $ORIGIN-relative, an empty one included: the loader reads that
    as the current folder."""
    return [e for e in entries.split(":") if not e.startswith("$ORIGIN")]


def selftest():
    present = {"libSDL3.so.0"}
    cases = [(kind_of("libSDL3.so.0", present, set()), "package"),
             (kind_of("libc.so.6", present, set()), "glibc"),
             (kind_of("libstdc++.so.6", present, {"libstdc++.so.6"}), "allowed"),
             (kind_of("libX11.so.6", present, set()), "FOREIGN"),
             (bool(runpath_bad("$ORIGIN:$ORIGIN/../lib")), False),
             (bool(runpath_bad("/home/someone/build/lib")), True),
             (bool(runpath_bad("$ORIGIN:")), True)]
    if any(got != want for got, want in cases):
        print("elf_judge: its own samples misread")
        sys.exit(2)


def main(argv):
    selftest()
    if not argv:
        print(__doc__)
        return 2
    folder = argv[0]
    allowed = {argv[i + 1] for i, a in enumerate(argv) if a == "--allow" and i + 1 < len(argv)}
    images, present = [], set()
    for d, _, files in os.walk(folder):
        for f in files:
            p = os.path.join(d, f)
            present.add(f)
            if os.path.islink(p):
                continue
            with open(p, "rb") as fh:
                if fh.read(4) == b"\x7fELF":
                    images.append(p)
    if not images:
        print("elf_judge: no ELF file under %s: an empty population is red" % folder)
        return 2
    bad, top = 0, {}
    for p in sorted(images):
        dyn = readelf("-dW", p)
        needed = re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", dyn)
        runpath = re.findall(r"\((?:RUNPATH|RPATH)\)\s+Library r(?:un)?path: \[([^\]]*)\]", dyn)
        verdicts = [(n, kind_of(n, present, allowed)) for n in needed]
        rp_bad = [r for r in runpath if runpath_bad(r)]
        for v in re.findall(r"Name: ((?:GLIBC|GLIBCXX|CXXABI)_[\d.]+)", readelf("-VW", p)):
            fam, ver = v.split("_", 1)
            if fam not in top or vkey(ver) > vkey(top[fam]):
                top[fam] = ver
        foreign = [n for n, k in verdicts if k == "FOREIGN"]
        bad += len(foreign) + len(rp_bad)
        mark = "FOREIGN" if foreign else "BUILD-PATH" if rp_bad else "ok"
        print("%-10s %s: %s%s" % (mark, os.path.relpath(p, folder),
                                  ", ".join("%s[%s]" % nv for nv in verdicts),
                                  ("  RUNPATH %s" % runpath) if runpath else ""))
    print("elf_judge: %d image(s); highest required %s; %d problem(s)" % (
        len(images), ", ".join("%s_%s" % kv for kv in sorted(top.items())), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
