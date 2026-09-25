# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/lane -- build and check a configured Zengine build tree from the external host: build
named targets, run one doctest binary's cases by name, or run the official lane
(tests/verify.cmake). loom-tool.json describes the inputs.

WHAT IT RUNS. CMake and the build tree's own test binaries, on this machine, as this session's
user: scoped local work, not an operation through Workshop, and not a sandbox. Each command's
whole output is kept as a file and its exit code is recorded as the process returned it -- never
read through a pipe. The run passes when every command exits 0; `expect=red` passes only when the
LAST command exits non-zero, which is how a mutation's red is recorded as evidence.

WHAT IT DOES NOT DO. It configures nothing (a tree is configured once, by whoever owns it), it
edits no source, and it does not decide what a green means: the repository's AGENTS.md does, and
the official lane is `op=verify`, never a bare test binary."""
import json
import os
import subprocess
import time
from pathlib import Path


def run(ctx):
    tree = Path(ctx.inputs["tree"])
    ctx.check(tree.is_absolute() and (tree / "CMakeCache.txt").is_file(),
              "tree must be an absolute, configured CMake build tree")
    op = ctx.inputs["op"]
    env = dict(os.environ)
    extra = ctx.inputs.get("path_prefix", "")
    if extra:
        env["PATH"] = extra.replace("/", os.sep) + os.pathsep + env.get("PATH", "")
    commands = []
    if op == "build":
        targets = [t for t in ctx.inputs.get("targets", "").split(",") if t]
        ctx.check(targets, "build names at least one target")
        commands.append(["cmake", "--build", str(tree), "--parallel",
                         str(int(ctx.inputs.get("parallel", 24))), "--target"] + targets)
    elif op == "case":
        binary = tree / "tests" / (ctx.inputs["binary"] + (".exe" if os.name == "nt" else ""))
        ctx.check(binary.is_file(), "no test binary %s" % binary)
        commands.append([str(binary), "--test-case=" + ctx.inputs["cases"]])
    elif op == "verify":
        source = Path(ctx.inputs["source"])
        ctx.check((source / "tests" / "verify.cmake").is_file(), "source must be the Zengine checkout")
        commands.append(["cmake", "-DZEN_BUILD_DIR=" + str(tree), "-P", str(source / "tests" / "verify.cmake")])
    else:
        ctx.fail("op must be build, case or verify")
    done = []
    for i, argv in enumerate(commands):
        ctx.step("%s: %s" % (op, " ".join(argv[:4])))
        log = Path("out-%d.log" % i)
        started = time.monotonic()
        with log.open("wb") as out:
            code = subprocess.run(argv, stdout=out, stderr=subprocess.STDOUT, env=env,
                                  cwd=str(Path(ctx.inputs.get("source") or tree))).returncode
        seconds = round(time.monotonic() - started, 1)
        data = log.read_bytes()
        ctx.produce("%s-%d.log" % (op, i), data)
        done.append({"argv": argv, "exit": code, "seconds": seconds, "bytes": len(data),
                     "tail": data.decode("utf-8", "replace").splitlines()[-6:]})
    ctx.produce("lane.json", json.dumps({"op": op, "tree": str(tree), "commands": done}, indent=1).encode())
    last = done[-1]["exit"]
    if ctx.inputs.get("expect", "pass") == "red":
        ctx.check(last != 0, "expected red, but the last command exited 0")
        return "%s red as expected: exit %d (%.1fs)" % (op, last, done[-1]["seconds"])
    ctx.check(all(d["exit"] == 0 for d in done), "%s exited %s" % (op, [d["exit"] for d in done]))
    return "%s passed: exit 0 (%.1fs)" % (op, sum(d["seconds"] for d in done))
