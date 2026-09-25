# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/source -- read files on this machine for a maker working through the external host:
list a directory, read a range of a file's lines, or find the lines matching a pattern under a
root. loom-tool.json describes the inputs.

IT ONLY READS. Nothing is written, run, opened in Workshop or sent to it. What it returns is a
LOCAL OBSERVATION of this machine's files at the moment of the run, kept whole as result.txt so a
later reader can see exactly what was read; it says nothing about what Workshop has loaded.

WHAT IT SKIPS. A search walks the tree below `path` but never enters .git, _deps or a directory
named build* or cmake-build*: build trees are large and hold generated copies of the sources.
The result is capped at LIMIT bytes and says so when it was cut."""
import fnmatch
import os
import re
from pathlib import Path

LIMIT = 400000
SKIP = (".git", "_deps", "__pycache__")


def skipped(name):
    return name in SKIP or name.startswith("build") or name.startswith("cmake-build")


def files(top):
    for here, dirs, names in os.walk(top):
        dirs[:] = sorted(d for d in dirs if not skipped(d))
        for name in sorted(names):
            yield Path(here) / name


def run(ctx):
    root = Path(ctx.inputs["root"]).resolve()
    ctx.check(root.is_absolute() and root.exists(), "root must name an existing absolute path")
    rel = ctx.inputs.get("path", "")
    target = (root / rel).resolve() if rel else root
    ctx.check(target == root or root in target.parents, "path %r leaves the root" % rel)
    ctx.check(target.exists(), "%s does not exist" % target)
    op = ctx.inputs["op"]
    out = []
    if op == "list":
        ctx.check(target.is_dir(), "%s is not a directory" % target)
        for p in sorted(target.iterdir()):
            if p.is_dir():
                out.append("%s/" % p.name)
            else:
                out.append("%s  %d" % (p.name, p.stat().st_size))
    elif op == "read":
        ctx.check(target.is_file(), "%s is not a file" % target)
        lines = target.read_text(encoding="utf-8", errors="replace").split("\n")
        start = max(1, int(ctx.inputs.get("start", 1)))
        last = min(len(lines), start + max(1, int(ctx.inputs.get("count", 200))) - 1)
        out = ["%5d  %s" % (n, lines[n - 1]) for n in range(start, last + 1)]
        out.insert(0, "%s: lines %d-%d of %d" % (target, start, last, len(lines)))
    elif op == "grep":
        pattern = re.compile(ctx.inputs["pattern"])
        glob = ctx.inputs.get("glob", "") or "*"
        most = max(1, int(ctx.inputs.get("max_hits", 200)))
        for f in (files(target) if target.is_dir() else [target]):
            if not fnmatch.fnmatch(f.name, glob):
                continue
            try:
                text = f.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            for n, line in enumerate(text.split("\n"), 1):
                if pattern.search(line) and len(out) < most:
                    out.append("%s:%d: %s" % (f.relative_to(root).as_posix(), n, line))
        if len(out) >= most:
            out.append("(stopped at max_hits=%d)" % most)
    elif op == "compare":
        # Two files, byte for byte: `path` under the root and `other`, an absolute path. It fails
        # when they differ, naming the first differing line, and keeps both sizes and hashes.
        import hashlib
        other = Path(ctx.inputs.get("other", ""))
        ctx.check(target.is_file() and other.is_absolute() and other.is_file(),
                  "compare needs `path` (a file under root) and `other` (an absolute file)")
        a, b = target.read_bytes(), other.read_bytes()
        out = ["%s  %d bytes  sha256 %s" % (target, len(a), hashlib.sha256(a).hexdigest()),
               "%s  %d bytes  sha256 %s" % (other, len(b), hashlib.sha256(b).hexdigest()),
               "identical" if a == b else "DIFFERENT"]
        if a != b:
            la, lb = a.split(b"\n"), b.split(b"\n")
            first = next((n for n in range(max(len(la), len(lb)))
                          if n >= len(la) or n >= len(lb) or la[n] != lb[n]), 0)
            ctx.produce("result.txt", "\n".join(out).encode("utf-8"))
            ctx.fail("the files differ from line %d" % (first + 1))
    else:
        ctx.fail("op must be list, read, grep or compare")
    data = "\n".join(out).encode("utf-8")
    if len(data) > LIMIT:
        data = data[:LIMIT] + b"\n(cut at %d bytes)" % LIMIT
    ctx.produce("result.txt", data)
    return "%s %s: %d line(s)" % (op, target, len(out))
