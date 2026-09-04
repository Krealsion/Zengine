#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE OBJECT CENSUS -- what a translation unit spends on COMDAT groups, measured on an ELF
# object so the Windows ceiling can be estimated on the canonical lane's tree.
#
# A body in a header is emitted in every translation unit that reaches it, as a COMDAT
# group whose signature is the mangled name; on COFF with `-g`, gas mirrors each group into
# four sections (.text$, .pdata$, .xdata$, .debug_frame$) that all carry that name, and
# the section-name string table cannot exceed 9,999,999 bytes (CMakeLists.txt, "MinGW
# objects are allowed to be big"). So for one object this script reports:
#
#   groups       COMDAT groups (`readelf -gW`)
#   name bytes   the sum of the signatures' mangled lengths
#   mean         name bytes / groups
#   coff est.    4 * name bytes + 37 * groups -- the four mirrored sections, each with its
#                prefix and terminator -- against the 9,999,999-byte table
#   buckets      groups by the demangled owner of the signature: zengine::workshop, other
#                zengine::, loom::, std (no argument from any of those), a template
#                instantiated over any of those, and other (lambdas, doctest, the C
#                library)
#
#   python3 tools/workshop-split/census.py <object.o>...
#   python3 tools/workshop-split/census.py --tree <build-dir> --source <repo>
#          every object under <build-dir>/workshop and <build-dir>/tests whose source
#          still exists under <repo> -- a build tree keeps the object of a source a phase
#          removed, and that object is not this tree's; each one skipped is reported.
#          The compile-fixture tree under tests/ is not a suite and is skipped by name.
#
# Needs binutils' readelf and c++filt on PATH. Output is one TSV row per object, then a
# `largest` row per class of object (the host, the suites, the logic library).

import argparse
import os
import re
import subprocess
import sys

CEILING = 9_999_999
GROUP = re.compile(r"^COMDAT group section \[\s*\d+\] `\.group' \[([^\]]+)\] contains (\d+) sections?:")
PREFIXES = ("vtable for ", "typeinfo for ", "typeinfo name for ", "VTT for ", "construction vtable for ",
            "non-virtual thunk to ", "virtual thunk to ", "guard variable for ", "covariant return thunk to ")


def signatures(obj):
    out = subprocess.run(["readelf", "-gW", obj], check=True, capture_output=True).stdout.decode("utf-8", "replace")
    sigs = []
    for line in out.split("\n"):
        m = GROUP.match(line)
        if m:
            sigs.append(m.group(1))
    return sigs


def demangle(names):
    if not names:
        return []
    out = subprocess.run(["c++filt"], input="\n".join(names).encode(), check=True, capture_output=True)
    return out.stdout.decode("utf-8", "replace").split("\n")[:len(names)]


def owner_of(demangled):
    d = demangled
    for p in PREFIXES:
        if d.startswith(p):
            d = d[len(p):]
    m = re.match(r"^((?:[A-Za-z_][A-Za-z0-9_]*::)*)", d)
    prefix = m.group(1) if m else ""
    rest = d[len(prefix):]
    args = d
    if prefix.startswith("zengine::workshop::"):
        return "workshop"
    if prefix.startswith("zengine::"):
        return "zengine"
    if prefix.startswith("loom::"):
        return "loom"
    if prefix.startswith("std::") or prefix.startswith("__gnu_cxx::"):
        if "zengine::" in args or "loom::" in args:
            return "template-over"
        return "std"
    if "zengine::" in args or "loom::" in args:
        return "template-over"
    return "other"


BUCKETS = ("workshop", "zengine", "loom", "std", "template-over", "other")


def census(obj):
    sigs = signatures(obj)
    groups = len(sigs)
    total = sum(len(s) for s in sigs)
    buckets = {b: [0, 0] for b in BUCKETS}
    for s, d in zip(sigs, demangle(sigs)):
        b = owner_of(d)
        buckets[b][0] += 1
        buckets[b][1] += len(s)
    return {"object": obj, "groups": groups, "name_bytes": total,
            "mean": (total / groups) if groups else 0.0, "coff": 4 * total + 37 * groups,
            "buckets": buckets, "size": os.path.getsize(obj)}


def row(c, rel):
    cols = [rel, str(c["groups"]), str(c["name_bytes"]), "%.1f" % c["mean"], str(c["coff"]),
            "%.1f%%" % (100.0 * c["coff"] / CEILING), str(c["size"])]
    for b in BUCKETS:
        cols.append("%d/%d" % tuple(c["buckets"][b]))
    return "\t".join(cols)


HEADER = "\t".join(["object", "groups", "name bytes", "mean", "coff est.", "of ceiling", "file bytes"] +
                   ["%s g/b" % b for b in BUCKETS])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("objects", nargs="*")
    ap.add_argument("--tree")
    ap.add_argument("--source", help="the source tree the build tree was configured from")
    a = ap.parse_args()
    objs = list(a.objects)
    base = None
    if a.tree:
        base = a.tree
        for sub in ("workshop", "tests"):
            for root, _, files in os.walk(os.path.join(a.tree, sub)):
                if "compile-fixtures" in root:
                    continue
                for f in sorted(files):
                    if not f.endswith(".o"):
                        continue
                    obj = os.path.join(root, f)
                    if a.source:
                        rel = os.path.relpath(obj, a.tree)
                        m = re.match(r"^(.*)/CMakeFiles/[^/]+\.dir/(.*)\.o$", rel.replace(os.sep, "/"))
                        src = os.path.join(a.source, m.group(1), m.group(2)) if m else None
                        if src is None or not os.path.exists(src):
                            print("skipped (no source): %s" % rel, file=sys.stderr)
                            continue
                    objs.append(obj)
    if not objs:
        print("census: no objects", file=sys.stderr)
        sys.exit(2)
    print(HEADER)
    results = []
    for o in sorted(objs):
        c = census(o)
        rel = os.path.relpath(o, base) if base else o
        results.append((rel, c))
        print(row(c, rel))
    classes = {"host": lambda r: "zengine-workshop.dir" in r,
               "suite": lambda r: "-tests.dir" in r,
               "logic": lambda r: "zengine-workshop-logic.dir" in r}
    for name, pred in classes.items():
        members = [(rel, c) for rel, c in results if pred(rel)]
        if members:
            rel, c = max(members, key=lambda rc: rc[1]["coff"])
            print("largest %s\t%s" % (name, row(c, rel)))


if __name__ == "__main__":
    main()
