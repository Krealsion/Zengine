# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Prepare, inspect, reset or stop an owned Workshop demonstration.

Uses an existing Zengine build and installed Loom. Python owns launch/recipe policy; all
Workshop mutations cross the admitted link to ordinary owners. See docs/workshop/demo-setups.md.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time
import uuid

PACKAGE = Path(__file__).resolve().parent / "tools" / "workshop"
sys.path.insert(0, str(PACKAGE))
from demo_setup import layout, NAMES, save_json


def runtime(prefix):
    path = Path(prefix) / "lib" / "loom" / "python"
    if not (path / "loom_session").is_dir():
        raise ValueError("Loom prefix has no installed Python session runtime: " + str(path))
    sys.path.insert(0, str(path))
    from loom_session.session import Session
    return Session


def wait_for(fn, seconds=40):
    end, last = time.monotonic() + seconds, None
    while time.monotonic() < end:
        try:
            result = fn()
            if result:
                return result
        except Exception as error:
            last = error
        time.sleep(.1)
    raise RuntimeError("readiness deadline expired: " + str(last))


def tool(session, name, **inputs):
    run_name = name + "-" + uuid.uuid4().hex[:10]
    session.start("workshop/" + name, run_name, inputs)
    record = session.wait(run_name, timeout=70)
    if record["state"] != "passed":
        raise RuntimeError(record.get("failure") or record.get("summary") or record["state"])
    return record


def artifact(record, name):
    path = next(a["path"] for a in record["artifacts"] if a["name"] == name)
    return json.loads(Path(path).read_text(encoding="utf-8"))


def status(session, wait=False):
    return artifact(tool(session, "demo-status", wait=wait), "status.json")


def launch(args, root):
    build, prefix = Path(args.build).resolve(), Path(args.loom_prefix).resolve()
    Session = runtime(prefix)
    binary = ".exe" if os.name == "nt" else ""
    library = ".dll" if os.name == "nt" else ".so"
    host_path = prefix / "bin" / ("loom-host" + binary)
    workshop_path = build / "workshop" / ("zengine-workshop" + binary)
    vocab = build / "external-host" / ("zengine-guest-vocabulary" + library)
    for path in (host_path, workshop_path, vocab, build / "workshop" / ("zengine-demo-control" + library)):
        if not path.is_file():
            raise ValueError("Build prerequisite missing: " + str(path))
    root.mkdir(parents=True, exist_ok=False)
    wdir, sdir = root / "workshop", root / "session"
    wdir.mkdir(); sdir.mkdir()
    desk = layout(args.setup)
    save_json(wdir / "setup.json", desk)
    save_json(wdir / "session.json", {"zen": 1, "schema": "WorkshopSession", "version": 6, "fields": {
        "format": "zengine-workshop-session", "format_version": "6",
        "viewport": {"width": "120", "height": "56"}, "active": "0",
        "placement": {"mode": "none", "x": "0", "y": "0", "window": "normal"},
        "layouts": [{"desk": desk["fields"], "link": {"path": str(wdir / "setup.json"), "known": desk["fields"]}}]}})
    plan_path = build / "workshop" / ("default-load-plan.json" if args.tui else "graphical-load-plan.json")
    plan = json.loads(plan_path.read_text(encoding="utf-8"))
    plan["fields"]["artifacts"].append({"artifact": "zengine-demo-control", "provider": [],
                                         "weave": [{"role": "zengine.demo"}], "optional": False})
    save_json(wdir / "load-plan.json", plan)
    credential = secrets.token_urlsafe(32)
    save_json(wdir / "guests.json", {"listen": "127.0.0.1:0", "port_file": "guests.port",
        "guests": [{"name": "workshop-demo", "credential": credential,
                    "may": ["input", "capture", "inspect", "inventory", "toolbox", "demo"]}]})
    save_json(sdir / "loom-tools.json", {"python": sys.executable,
        "runtime": str(prefix / "lib" / "loom" / "python"),
        "packages": [{"path": str(PACKAGE), "approve": "any-revision"}]})
    children = []

    def spawn(argv, cwd, approvals=None):
        with (cwd / "process.log").open("wb") as output:
            child = subprocess.Popen([str(a) for a in argv], cwd=cwd, stdout=output,
                stderr=subprocess.STDOUT, stdin=subprocess.PIPE if approvals else subprocess.DEVNULL,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        children.append(child)
        if approvals:
            child.stdin.write(("\n".join(approvals) + "\n").encode()); child.stdin.close()
        return child

    try:
        workshop = spawn([workshop_path, "--isolated", "--setup", "setup.json", "--session", "session.json",
                          "--load-plan", "load-plan.json", "--guests", "guests.json", "--log", "workshop.log",
                          "--log-refusals", "--demo-history", "--dump", "history.txt"], wdir)
        port = wait_for(lambda: (wdir / "guests.port").read_text().strip())
        runs = prefix / "lib" / "loom" / ("loom-runs" + library)
        save_json(sdir / "loom-boot.json", {"boot": [{"name": "runs", "path": str(runs), "role": "loom.runs"},
                  {"name": "vocab", "path": str(vocab)}], "links": [{"name": "workshop",
                  "connect": "127.0.0.1:" + port, "identity": "workshop-demo", "credential": credential}],
                  "history": {"log": "session.log", "recent": "8192", "payload_budget": "134217728"}})
        approvals = ["authority trust runs --rebuilds", "authority trust vocab --rebuilds"]
        approvals += ["authority allow runs %s v1 -> role loom.session" % s
                      for s in ("loom.session.ExpectRun", "loom.session.ForgetRun")]
        approvals += ["authority allow runs loom.runs.%s v1 -> any target" % s
                      for s in ("Tools", "ToolDescription", "Run", "RunList", "Directive")]
        approvals += ["authority allow runs loom.link.%s v1 -> role loom.link.workshop" % s
                      for s in ("Ask", "StatusRequested")]
        approvals += ["start vocab %s" % vocab, "start runs %s loom.runs" % runs]
        host = spawn([host_path, "--serve", sdir], sdir, approvals)

        def ready():
            with Session.attach(str(sdir)) as session:
                session.tools()
                return any(r["name"] == "workshop" and r["state"] == "admitted" for r in session.describe()["links"])

        wait_for(ready)
        with Session.attach(str(sdir)) as session:
            service = session.start("workshop/demo-serve", "demo-service", {"setup": args.setup})
            config = {"setup": args.setup, "build": str(build), "loom_prefix": str(prefix),
                      "workshop_pid": workshop.pid, "host_pid": host.pid, "lifetime": session.lifetime,
                      "endpoint": "127.0.0.1:" + port,
                      "session": str(sdir), "service": "demo-service", "service_start": service}
            save_json(root / "demo.json", config)
            result = status(session, wait=True)
            result.update(session=str(sdir), root=str(root), reused=False,
                          lifetime=session.lifetime, endpoint=config["endpoint"])
            return result
    except BaseException:
        for child in reversed(children):
            if child.poll() is None:
                child.terminate(); child.wait(timeout=10)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["list", "start", "status", "reset", "stop"])
    parser.add_argument("--root", type=Path, help="dedicated demo directory; never a source or maker state directory")
    parser.add_argument("--setup", choices=list(NAMES), help="first start defaults to values; reuse keeps its setup")
    parser.add_argument("--build", help="configured, built Zengine tree")
    parser.add_argument("--loom-prefix", help="installed Loom prefix, including loom-host and Python runtime")
    parser.add_argument("--tui", action="store_true", help="use the classic terminal medium")
    args = parser.parse_args()
    if args.action == "list":
        print(json.dumps(NAMES, indent=2)); return
    if args.root is None:
        parser.error("--root is required")
    started = time.monotonic()
    root = args.root.resolve()
    config_path = root / "demo.json"
    if args.action == "start" and not config_path.exists():
        if not args.build or not args.loom_prefix:
            parser.error("a first start needs --build and --loom-prefix")
        args.setup = args.setup or "values"
        result = launch(args, root)
    else:
        config = json.loads(config_path.read_text(encoding="utf-8"))
        Session = runtime(config["loom_prefix"])
        with Session.attach(config["session"]) as session:
            if session.lifetime != config["lifetime"]:
                raise RuntimeError("this directory now names a different ELH lifetime")
            if args.action == "reset":
                result = artifact(tool(session, "demo-reset"), "reset.json")
            elif args.action == "stop":
                session.cancel(config["service"], "demo stopped")
                session.wait(config["service"], timeout=15)
                tool(session, "demo-stop")
                wait_for(lambda: all(r["name"] != "workshop" or r["state"] != "admitted"
                                     for r in session.describe()["links"]), 20)
                session.shutdown("demo stopped")
                result = {"state": "stopped", "history": str(root / "workshop" / "history.txt")}
            else:
                if args.action == "start" and args.setup is not None and args.setup != config["setup"]:
                    raise RuntimeError("this demo already has another setup; use a new root")
                result = status(session, wait=args.action == "start")
                result.update(session=config["session"], root=str(root), reused=True,
                              lifetime=session.lifetime, endpoint=config.get("endpoint", "see workshop/guests.port"))
    result["elapsed_ms"] = (time.monotonic() - started) * 1000
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
