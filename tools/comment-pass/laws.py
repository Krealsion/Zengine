# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Print the law a source file's pointers name -- each id's LAW, MEANS and DOES NOT MEAN, and
# the decision record its WHY names -- so a comment can be read against its owner before it is
# kept, moved or cut. Read-only.
#
#   python tools/comment-pass/laws.py workshop/panel.hpp [--why]

import os
import re
import sys

POINTER = re.compile(r"^\s*// ((?:WL|MW)-[A-Z]+-\d+.*)$")
ID = re.compile(r"\b((?:WL|MW)-[A-Z]+-\d+)\b")
SEGMENT = re.compile(r"((?:(?:WL|MW)-[A-Z]+-\d+\s*,?\s*)+)--\s*([A-Za-z0-9_./-]+\.md)")


def entries(register):
    """{id: [lines]} for every `## <id>` entry of a register file."""
    out = {}
    cur = None
    with open(register, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^## ((?:WL|MW|VM)-[A-Z]+-\d+)", line)
            if m:
                cur = m.group(1)
                out[cur] = [line.rstrip()]
            elif line.startswith("## "):
                cur = None
            elif cur:
                out[cur].append(line.rstrip())
    return out


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    path = sys.argv[1]
    show_why = "--why" in sys.argv
    wanted = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = POINTER.match(line)
            if not m:
                continue
            for ids, reg in SEGMENT.findall(m.group(1)):
                for i in ID.findall(ids):
                    wanted.setdefault(reg, set()).add(i)
    records = set()
    for reg in sorted(wanted):
        if not os.path.exists(reg):
            print("!! missing register %s" % reg)
            continue
        es = entries(reg)
        for i in sorted(wanted[reg], key=lambda s: int(s.rsplit("-", 1)[1])):
            body = es.get(i)
            if body is None:
                print("!! %s not in %s" % (i, reg))
                continue
            keep = []
            section = None
            for line in body:
                if line.startswith("## "):
                    keep.append(line[3:])
                    continue
                head = line.split(" ", 1)[0]
                if head in ("LAW", "MEANS", "DOES", "PROVEN", "WHY", "UNWITNESSED"):
                    section = head
                if line.startswith("WHY"):
                    records.add(line.split("`")[1] if "`" in line else line)
                if section in ("LAW", "MEANS", "DOES") and line.strip():
                    keep.append("  " + line)
            print("\n".join(keep))
            print()
    if show_why:
        for r in sorted(records):
            print("== %s" % r)
            if os.path.exists(r):
                with open(r, encoding="utf-8") as f:
                    print(f.read())


if __name__ == "__main__":
    main()
