# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""EVERY TUTORIAL, PLAYED AS A WEAVER PLAYS IT.

A tutorial is a setup in its feature's `docs/tutorials/<name>/`, and its `tutorial` walk is the page's
steps. This installs the build into a prefix of its own -- the package a weaver installs, and the
one each tutorial builds against -- then, for every tutorial, starts the copy the install put in the
tool package's `setups/` through `external-host/demo.py` in a terminal Workshop, replays its
`tutorial` walk and stops it. It passes when every walk says `passed` and the `steps.json` its run
kept reads the same by message from the root's Loom session, as a client with no shell reads it; a
step that cannot be done is named with what the pane painted instead.

    tutorial_walks.py --repo <checkout> --build <tree> --cmake <cmake> --config <config>
                      --loom-prefix <installed Loom> --work <dir> [--evidence <file.json>]

Each tutorial runs in a root of its own under `--work`; a tutorial that builds stages what it built
beside its root's own Workshop (the setup's development runtime), never in `--build`, and a file
that appears beside `--build`'s Workshop during a replay is a failure.
workshop/docs/demo-setups.md is the guide.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys


def demo(args, *more):
    """One `demo.py` action: its answer, and its exit."""
    run = subprocess.run([sys.executable, "-B", str(args.repo / "external-host" / "demo.py"), *more],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    said = run.stdout.decode("utf-8", "replace")
    try:
        answer = json.loads(said)
    except ValueError:
        answer = {"said": said}
    return answer, run.returncode


def read_by_message(args, root, run, name):
    """A file run `run` kept, read by message from the root's Loom session page by page, each page
    checked against the digest the run manager verified."""
    runtime = str(args.loom_prefix / "lib" / "loom" / "python")
    if runtime not in sys.path:
        sys.path.insert(0, runtime)
    from loom_session.session import Session
    config = json.loads((root / "demo.json").read_text(encoding="utf-8"))
    with Session.attach(config["session"]) as session:
        return session.read_artifact(run, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "build", "loom-prefix", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--config", default="")
    parser.add_argument("--evidence", type=Path)
    args = parser.parse_args()
    found = sorted(p.parent for p in args.repo.glob("*/docs/tutorials/*/setup.json"))
    evidence = {"tutorials": [p.relative_to(args.repo).as_posix() for p in found], "walks": []}
    if not found:
        print("tutorial_walks: no tutorial under */docs/tutorials/; an empty population is red")
        return 1
    shutil.rmtree(args.work, ignore_errors=True)
    args.work.mkdir(parents=True)
    prefix = args.work / "prefix"
    install = [args.cmake, "--install", str(args.build), "--prefix", str(prefix)]
    if args.config:
        install += ["--config", args.config]
    with open(args.work / "install.log", "wb") as log:
        installed = subprocess.run(install, stdout=log, stderr=subprocess.STDOUT).returncode
    if installed:
        print("tutorial_walks: installing the build into %s failed (exit %d)" % (prefix, installed))
        return 1
    setups = prefix / "share" / "zengine" / "loom-tools" / "workshop" / "setups"
    host = args.build / "workshop"
    failed = 0
    for tutorial in found:
        name, root = tutorial.name, args.work / tutorial.name
        record = {"tutorial": name}
        evidence["walks"].append(record)
        if not (setups / name / "setup.json").is_file():
            record["state"] = "not installed: %s holds no %s" % (setups, name)
            print("tutorial_walks: %s: %s" % (name, record["state"]))
            failed += 1
            continue
        before = sorted(p.name for p in host.iterdir())
        started, code = demo(args, "start", "--setup", str(setups / name), "--root", str(root), "--tui",
                             "--build", str(args.build), "--loom-prefix", str(args.loom_prefix),
                             "--zengine-prefix", str(prefix))
        record["start"] = {k: started.get(k) for k in ("state", "said", "note", "elapsed_ms")}
        try:
            if code or started.get("state") != "ready":
                record["state"] = "not started"
            else:
                walked, code = demo(args, "walk", "tutorial", "--root", str(root),
                                    "--pictures", str(root / "walk"))
                record.update(state=walked.get("state", "no answer"), steps=walked.get("steps"),
                              failure=walked.get("failure") or walked.get("said"))
                done = root / "walk" / "steps.json"
                steps = json.loads((setups / name / "setup.json").read_text(
                    encoding="utf-8"))["walks"]["tutorial"]["steps"]
                # The walk keeps every step it finished, so the one it could not do is the next.
                at = len(json.loads(done.read_text(encoding="utf-8"))) if done.is_file() else -1
                if record["state"] != "passed" and 0 <= at < len(steps):
                    record["failure"] = "step %d of %d, %s: %s" % (
                        at + 1, len(steps), json.dumps(steps[at]), record["failure"])
                # WHAT THE RUN KEPT, READ BY MESSAGE: the same bytes the walk copied from disk.
                if record["state"] == "passed":
                    try:
                        read = read_by_message(args, root, walked["run"], "steps.json")
                        record["read_by_message"] = len(read)
                        if read != done.read_bytes():
                            record.update(state="read by message differs", failure=(
                                "steps.json of run %s: %d bytes by message, %d on disk"
                                % (walked["run"], len(read), done.stat().st_size)))
                    except Exception as error:
                        record.update(state="not read by message", failure=(
                            "steps.json of run %s, read by message from the root's session: %s: %s"
                            % (walked["run"], type(error).__name__, error)))
        finally:
            stopped, _ = demo(args, "stop", "--root", str(root))
            record["stop"] = stopped.get("state") or stopped.get("said")
        staged = sorted(set(p.name for p in host.iterdir()) - set(before))
        if staged and record["state"] == "passed":
            record.update(state="wrote the build tree", failure="%s gained %s" % (host, staged))
        print("tutorial_walks: %s: %s%s" % (name, record["state"],
              "" if record["state"] == "passed" else " -- %s" % (record.get("failure") or record["start"])))
        failed += record["state"] != "passed"
    if args.evidence:
        args.evidence.write_text(json.dumps(evidence, indent=1), encoding="utf-8")
    print("tutorial_walks: %d of %d tutorial(s) replayed `passed`" % (len(found) - failed, len(found)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
