# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# A first draft of the case map: every case name with a plan code, and the name it has once its
# leading labels are taken off. A name whose code sits inside it, or whose stripped name would
# repeat another case's, is drafted with an empty new name for a person to write. The draft is
# the start of map.tsv, never a replacement for it.
#
#   python tools/phase-codes/draft.py [--repo DIR] > draft.tsv

import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cases  # noqa: E402
import codes  # noqa: E402

CODE = r"[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+[a-z]?"
# One leading label: codes joined by `/` or `+`, a subcase letter, an optional `law`, then `:`
# or ` --`. `WUX-0 D/MIG-0:`, `QR-10 + BOOT-0:`, `BLD-1 law:`, `BLD-WEAVE: PANE-MIG --`.
LABEL = re.compile(rf"^(?:{CODE}|[A-Z])(?:\s*[/+]\s*(?:{CODE}))*(?: [A-Z](?:/{CODE})?)?"
                   rf"(?: law)?(?::| --) +")


def strip(name):
    out = name
    while True:
        m = LABEL.match(out)
        if not m or codes.is_law_id(out.split(":", 1)[0].split(" ", 1)[0]):
            return out
        out = out[m.end():]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    a = ap.parse_args()
    every = cases.all_cases(a.repo)
    rows = []
    for c in every:
        if not codes.case_codes(c.name):
            continue
        new = strip(c.name)
        kind = "strip"
        if codes.case_codes(new) or new == c.name:
            new, kind = "", "hand"
        rows.append([c.path, c.macro, c.name, new, kind])
    # A drafted name must not repeat a name its file already holds, before or after the map.
    held = collections.Counter()
    for c in every:
        if not codes.case_codes(c.name):
            held[(c.path, c.name)] += 1
    for r in rows:
        if r[3]:
            held[(r[0], r[3])] += 1
    for r in rows:
        if r[3] and held[(r[0], r[3])] > 1:
            r[4] = "collides"
    for r in rows:
        print("\t".join([r[0], r[2], r[3], r[4]]))


if __name__ == "__main__":
    main()
