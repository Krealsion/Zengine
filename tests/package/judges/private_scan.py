# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A package judge: no file of a package names the machine that built it.

    python -I private_scan.py <package folder> <needle> [<needle>...]

Reads every file's bytes for each needle -- a build machine's folder, its user, a tool's location
-- as UTF-8 and as UTF-16LE (a Windows wide string), ignoring case. Prints each hit as
`<file>: <needle> x<count> ... <context>`. Exit 0 when nothing matched, 1 on a hit, 2 on an empty
population (no file or no needle) or when its own samples misread, which it reads first.
"""
import os
import re
import sys


def finder(needles):
    return [(n, re.compile(re.escape(n.encode("utf-8")), re.I),
             re.compile(re.escape(n.encode("utf-16-le")), re.I)) for n in needles]


def hits(data, pats):
    out = []
    for needle, narrow, wide in pats:
        found = list(narrow.finditer(data)) + list(wide.finditer(data))
        if found:
            m = found[0]
            ctx = data[max(0, m.start() - 40):m.end() + 60].replace(b"\0", b"")
            out.append((needle, len(found), ctx.decode("utf-8", "replace").replace("\n", " ")))
    return out


def selftest():
    pats = finder(["Sample-Machine/path"])
    planted = b"xx sample-machine/PATH yy" + "Sample-Machine/path".encode("utf-16-le")
    got = hits(planted, pats)
    if len(got) != 1 or got[0][1] != 2 or hits(b"nothing here", pats):
        print("private_scan: its own samples misread")
        sys.exit(2)


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    selftest()
    if len(argv) < 2:
        print(__doc__)
        return 2
    folder, needles = argv[0], [n for n in argv[1:] if n]
    files = sorted(os.path.join(d, f) for d, _, fs in os.walk(folder) for f in fs)
    if not files or not needles:
        print("private_scan: nothing to scan (%d file(s), %d needle(s)): an empty population is red"
              % (len(files), len(needles)))
        return 2
    pats = finder(needles)
    bad = 0
    for path in files:
        if os.path.islink(path):
            continue
        with open(path, "rb") as fh:
            data = fh.read()
        for needle, count, ctx in hits(data, pats):
            bad += 1
            print("%s: %s x%d ... %s" % (os.path.relpath(path, folder), needle, count, ctx[:160]))
    print("private_scan: %d file(s), %d needle(s), %d hit(s)" % (len(files), len(needles), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
