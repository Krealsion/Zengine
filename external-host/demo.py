# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""List, describe, prepare, inspect, walk, reset, stop or export a described Workshop setup.

`start` makes one dedicated Workshop and Loom session per root, prepares the setup through
Workshop's own owners and waits until it is usable; run again on the same root it returns to the
running desk without resetting it. `walk` replays one of the setup's named walks against it. Uses an existing Zengine build and installed Loom. Python owns
launch policy; every Workshop mutation crosses the admitted link. docs/workshop/demo-setups.md."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time
import uuid

HERE = Path(__file__).resolve().parent
PACKAGE = HERE / "tools" / "workshop"
EXAMPLES = HERE.parent / "examples"
sys.path.insert(0, str(PACKAGE))
import setups as described  # noqa: E402
from demo_setup import save_json  # noqa: E402

EXE, LIB = (".exe", ".dll") if os.name == "nt" else ("", ".so")


def find(where):
    return described.load(where, extra=[EXAMPLES])


def runtime(prefix):
    path = Path(prefix) / "lib" / "loom" / "python"
    if not (path / "loom_session").is_dir():
        raise ValueError("Loom prefix has no installed Python session runtime: " + str(path))
    sys.path.insert(0, str(path))
    import loom_session.session as module
    return module


def wait_for(fn, seconds, what):
    end, last = time.monotonic() + seconds, None
    while time.monotonic() < end:
        try:
            result = fn()
            if result:
                return result
        except Exception as error:
            last = error
        time.sleep(.1)
    raise RuntimeError("%s did not happen within %gs%s" % (what, seconds, "; last error: %s" % last if last else ""))


def tool(session, name, timeout=70, **inputs):
    run_name = name + "-" + uuid.uuid4().hex[:10]
    session.start("workshop/" + name, run_name, inputs)
    record = session.wait(run_name, timeout=timeout)
    if record["state"] != "passed":
        raise RuntimeError(record.get("failure") or record.get("summary") or record["state"])
    return record


def artifact(record, name):
    path = next(a["path"] for a in record["artifacts"] if a["name"] == name)
    return json.loads(Path(path).read_text(encoding="utf-8"))


def ready(module, session, config, seconds):
    """Wait for the service's answer, or return `pending` with the run that still waits."""
    run_name = "demo-status-" + uuid.uuid4().hex[:10]
    session.start("workshop/demo-status", run_name, {"wait": True, "seconds": float(seconds)})
    pending = {"state": "pending", "note": "preparation has not finished within %gs; it goes on" % seconds,
               "waiting_run": run_name, "service": config["service"],
               "recover": "`demo.py status --root <root> --wait SECONDS` waits again; `loom-session "
                          "show %s demo-service` shows the step it is on" % config["session"]}
    try:
        record = session.wait(run_name, timeout=seconds + 15)
    except module.NotAnswered:
        return pending
    if record["state"] == "passed":
        return artifact(record, "status.json")
    result = artifact(tool(session, "demo-status"), "status.json")
    if result["state"] == "working":
        return dict(pending, generation=result["generation"])
    return result


def preparation(session, config):
    """The service's latest preparation sample: its owner request counts and elapsed time."""
    try:
        record = session.run(config["service"])
        state = artifact(record, "setup.json")
        return state["samples"][-1] if state.get("samples") else None
    except Exception:
        return None


def guidance(setup, root):
    d = setup.data
    out = {"setup": setup.name, "title": d["title"], "first_task": d["first_task"],
           "guide": setup.summary()["guide_path"] + ("#" + d["guide"].split("#", 1)[1] if "#" in d.get("guide", "") else ""),
           "reset": "python external-host/demo.py reset --root %s  (or press Reset demo)" % root,
           "reset_scope": (d.get("reset") or {}).get("scope", "")}
    if setup.hotkeys():
        out["hotkeys"] = ["%s: %s -> %s (%s)" % (h["key"], h["entry"], h["target"], h["means"]) for h in setup.hotkeys()]
    return out


def prerequisites(args, setup):
    """Every missing input, named with the flag or build step that supplies it."""
    missing, build, prefix = [], Path(args.build or ""), Path(args.loom_prefix or "")
    if not args.build:
        missing.append("--build: a configured, built Zengine tree")
    if not args.loom_prefix:
        missing.append("--loom-prefix: an installed Loom with loom-host and its Python session runtime")
    project = setup.get("project") or {}
    medium = "tui" if args.tui else "sdl"
    if medium not in setup.get("medium")["supports"]:
        missing.append("setup %s does not support the %s medium (it supports %s)"
                       % (setup.name, medium, ", ".join(setup.get("medium")["supports"])))
    if args.build:
        wanted = [build / "workshop" / ("zengine-workshop" + EXE),
                  build / "external-host" / ("zengine-guest-vocabulary" + LIB),
                  build / "workshop" / ("zengine-demo-control" + LIB),
                  build / "workshop" / ("default-load-plan.json" if args.tui else "graphical-load-plan.json")]
        wanted += [build / "workshop" / (a + LIB) for a in (setup.get("requires") or {}).get("artifacts", [])]
        if project.get("runtime") == "development":
            wanted.append(build / "workshop" / "development-runtime.cmake")
        missing += ["build artifact %s (build the tree)" % p for p in wanted if not p.is_file()]
    if args.loom_prefix:
        for p in (prefix / "bin" / ("loom-host" + EXE), prefix / "lib" / "loom" / ("loom-runs" + LIB),
                  prefix / "lib" / "loom" / "python" / "loom_session"):
            if not p.exists():
                missing.append("Loom install %s (install Loom with its session tools)" % p)
    for name, why in sorted(((setup.get("requires") or {}).get("inputs") or {}).items()):
        value = getattr(args, name, None)
        if not value:
            missing.append("--%s: %s" % (name.replace("_", "-"), why))
        elif name.endswith("_prefix") and not (Path(value) / "lib" / "cmake").is_dir():
            missing.append("--%s %s holds no installed package (lib/cmake)" % (name.replace("_", "-"), value))
    if args.toolchain_bin and not Path(args.toolchain_bin).is_dir():
        missing.append("--toolchain-bin %s is not a directory" % args.toolchain_bin)
    if args.neovim and not Path(args.neovim).is_file():
        missing.append("--neovim %s is not a program" % args.neovim)
    return missing


def make_project(args, setup, root, build, prefix):
    """The setup's project files at their targets inside `root/project` -- nested targets get
    their directories; the description already refused a target outside it -- with its recipe
    template's inputs spelled for this instance."""
    project, directory = setup.get("project"), root / "project"
    directory.mkdir()
    for target, source in project.get("files", {}).items():
        path = directory / target
        if directory.resolve() not in path.resolve().parents:
            raise RuntimeError("project file %r would be written outside %s" % (target, directory))
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(setup.asset(source), path)
    text = setup.asset(project["recipes"]).read_text(encoding="utf-8")
    values = {"zengine_prefix": Path(args.zengine_prefix or "").resolve().as_posix(),
              "loom_prefix": prefix.as_posix(), "build": build.as_posix(), "root": root.as_posix()}
    for key, value in values.items():
        text = text.replace("${%s}" % key, value)
    (directory / "build-recipes.json").write_text(text, encoding="utf-8")
    return directory


def launch(args, source, root):
    build, prefix = Path(args.build).resolve(), Path(args.loom_prefix).resolve()
    module = runtime(prefix)
    root.mkdir(parents=True, exist_ok=False)
    # The revision this root prepares, kept with it: everything below, and every Reset, reads this
    # copy, whatever later happens to the setup's own directory.
    setup = source.export(root / "prepared" / source.name)
    wdir, sdir = root / "workshop", root / "session"
    wdir.mkdir(); sdir.mkdir()
    environment = dict(os.environ)
    if args.toolchain_bin:
        environment["PATH"] = str(Path(args.toolchain_bin).resolve()) + os.pathsep + environment.get("PATH", "")
    if args.neovim:
        environment["ZENGINE_NEOVIM"] = str(Path(args.neovim).resolve())
    project = setup.get("project") or {}
    host_dir = build / "workshop"
    if project.get("runtime") == "development":
        # A copy of this build no build rule writes, so a built game never lands in the build tree.
        with (root / "runtime-make.log").open("wb") as out:
            made = subprocess.run(["cmake", "-DZEN_RUNTIME=" + (root / "runtime").as_posix(), "-P",
                                   str(build / "workshop" / "development-runtime.cmake")],
                                  stdout=out, stderr=subprocess.STDOUT, env=environment)
        if made.returncode:
            raise RuntimeError("the development runtime was not made (exit %d): %s"
                               % (made.returncode, root / "runtime-make.log"))
        host_dir = root / "runtime"
    cwd = make_project(args, setup, root, build, prefix) if project.get("recipes") else wdir
    providers = [p["role"] for p in setup.providers()]
    first = setup.without(setup.desk(), providers)
    save_json(wdir / "setup.json", first)
    columns, rows = setup.get("medium")["viewport"]
    save_json(wdir / "session.json", {"zen": 1, "schema": "WorkshopSession", "version": 7, "fields": {
        "format": "zengine-workshop-session", "format_version": "7",
        "viewport": {"width": str(columns * 12), "height": str(rows * 12)}, "active": "0",
        "placement": {"mode": "none", "x": "0", "y": "0", "window": "normal"},
        "layouts": [{"desk": first["fields"], "link": {"path": str(wdir / "setup.json"), "known": first["fields"]}}]}})
    plan = json.loads((host_dir / ("default-load-plan.json" if args.tui else "graphical-load-plan.json")).read_text(encoding="utf-8"))
    plan["fields"]["artifacts"].append({"artifact": "zengine-demo-control", "provider": [],
                                        "weave": [{"role": "zengine.demo"}], "optional": False})
    if args.neovim:
        # THE PLAN'S OWN SECOND CHOICE for the Editor's office holds it from the start.
        for row in plan["fields"]["artifacts"]:
            if row["artifact"] == "zengine-editor-pane":
                row["artifact"] = "zengine-neovim-editor"
    # A row the setup builds comes last: until it is built the project waits there (its frontier).
    for row in project.get("plan", []):
        plan["fields"]["artifacts"].append({"artifact": row["artifact"], "provider": [],
                                            "weave": [{"role": row["role"]}], "optional": False})
    save_json(wdir / "load-plan.json", plan)
    authority = setup.get("authority")
    credential = secrets.token_urlsafe(32)
    guest = {"name": "workshop-demo", "credential": credential, "may": authority["may"]}
    if authority.get("observe"):
        guest["observe"] = authority["observe"]
    save_json(wdir / "guests.json", {"listen": "127.0.0.1:0", "port_file": str(wdir / "guests.port"),
                                     "guests": [guest]})
    packages = [{"path": str(PACKAGE), "approve": "any-revision"}]
    packages += [{"path": str(setup.asset(t)), "approve": "any-revision"} for t in setup.get("tools", [])]
    save_json(sdir / "loom-tools.json", {"python": sys.executable,
        "runtime": str(prefix / "lib" / "loom" / "python"), "packages": packages})
    children, custody = [], {"root": str(root), "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}

    def spawn(label, argv, cwd, log, approvals=None):
        with log.open("wb") as output:
            child = subprocess.Popen([str(a) for a in argv], cwd=cwd, stdout=output, env=environment,
                stderr=subprocess.STDOUT, stdin=subprocess.PIPE if approvals else subprocess.DEVNULL,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        children.append(child)
        # WRITTEN BEFORE ANYTHING WAITS: an interrupted start leaves which processes it began.
        custody[label + "_pid"] = child.pid
        save_json(root / "launch.json", custody)
        if approvals:
            child.stdin.write(("\n".join(approvals) + "\n").encode()); child.stdin.close()
        return child

    try:
        workshop = spawn("workshop", [host_dir / ("zengine-workshop" + EXE), "--isolated", "--setup", wdir / "setup.json",
                          "--session", wdir / "session.json", "--load-plan", wdir / "load-plan.json",
                          "--guests", wdir / "guests.json", "--log", wdir / "workshop.log",
                          "--log-refusals", "--demo-history", "--dump", wdir / "history.txt"]
                         , cwd, wdir / "process.log")
        port = wait_for(lambda: (wdir / "guests.port").read_text().strip(), 90, "Workshop writing its guest port")
        runs = prefix / "lib" / "loom" / ("loom-runs" + LIB)
        vocab = build / "external-host" / ("zengine-guest-vocabulary" + LIB)
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
        host = spawn("host", [prefix / "bin" / ("loom-host" + EXE), "--serve", sdir], sdir, sdir / "process.log", approvals)

        def admitted():
            with module.Session.attach(str(sdir)) as session:
                session.tools()
                return any(r["name"] == "workshop" and r["state"] == "admitted" for r in session.describe()["links"])

        wait_for(admitted, 90, "the Loom session admitting its link to Workshop")
        with module.Session.attach(str(sdir)) as session:
            service = session.start("workshop/demo-serve", "demo-service", {"setup": str(setup.root)})
            config = {"setup": setup.name, "directory": str(source.root), "digest": source.digest(),
                      "prepared": str(setup.root),
                      "medium": "tui" if args.tui else "sdl", "build": str(build), "loom_prefix": str(prefix),
                      "workshop_pid": workshop.pid, "host_pid": host.pid, "lifetime": session.lifetime,
                      "endpoint": "127.0.0.1:" + port, "project": str(cwd) if cwd != wdir else "",
                      "session": str(sdir), "service": "demo-service", "service_start": service}
            save_json(root / "demo.json", config)
            result = ready(module, session, config, args.wait)
            result.update(session=str(sdir), root=str(root), reused=False, lifetime=session.lifetime,
                          endpoint=config["endpoint"], preparation=preparation(session, config),
                          guidance=guidance(setup, root))
            return result
    except BaseException:
        if not (root / "demo.json").exists():
            # Nothing is recorded as running: what this start began, it ends, and says so.
            for child in reversed(children):
                if child.poll() is None:
                    child.terminate(); child.wait(timeout=10)
            custody["ended_by_failed_start"] = True
            save_json(root / "launch.json", custody)
        raise


def walk(module, session, config, name, folder, seconds):
    """Replay the setup's walk `name` through `workshop/act` against this root's Workshop, and copy
    what the run kept -- its pictures, any rows it kept and steps.json -- into `folder`, which must
    be new or empty. The walk is read from the setup's directory as it is now, so an edited walk
    replays without a new root; the desk it walks is the one this root prepared."""
    setup = find(config["directory"])
    if name not in setup.walks():
        raise RuntimeError("setup %s carries no walk %r; it carries: %s"
                           % (setup.name, name, ", ".join(sorted(setup.walks())) or "none"))
    folder = folder.resolve()
    if folder.exists() and any(folder.iterdir()):
        raise RuntimeError("%s already holds files; name a new or empty folder, so one replay's "
                           "pictures never mix with another's" % folder)
    # A window's picture is kept as a PNG too; a terminal's is its cells, whatever is asked.
    steps = [dict(s, png=True) if "picture" in s else dict(s) for s in setup.walks()[name]["steps"]]
    run_name = "walk-%s-%s" % (name, uuid.uuid4().hex[:10])
    session.start("workshop/act", run_name, {"steps": json.dumps(steps)})
    try:
        record = session.wait(run_name, timeout=seconds)
    except module.NotAnswered:
        raise RuntimeError("the walk had not finished within %gs and goes on; `loom-session show %s %s` "
                           "shows the step it is on" % (seconds, config["session"], run_name))
    folder.mkdir(parents=True, exist_ok=True)
    kept = []
    for made in record.get("artifacts", []):
        if made["name"].endswith(".bmp"):
            continue  # the PNG beside it is the same picture
        shutil.copyfile(made["path"], folder / made["name"])
        kept.append(made["name"])
    result = {"walk": name, "state": record["state"], "medium": config["medium"], "run": run_name,
              "steps": len(steps), "folder": str(folder), "kept": kept}
    if record["state"] != "passed":
        result["failure"] = record.get("failure") or record.get("summary")
    if setup.digest() != config.get("digest"):
        result["description_changed"] = ("the walk was read from the setup's directory as it is now; "
                                         "this root prepared an earlier revision (%s)" % config.get("prepared"))
    return result


def existing(args, root):
    """The root's recorded instance, or an explanation of why it cannot be returned to."""
    config_path = root / "demo.json"
    if not config_path.exists():
        launch_record = root / "launch.json"
        if launch_record.exists():
            raise RuntimeError("an earlier start in %s was interrupted before it recorded its demo: "
                               "launch.json names what it started (%s). Nothing is stopped, removed or "
                               "reused; start a new root." % (root, launch_record.read_text(encoding="utf-8").strip()))
        raise RuntimeError("%s holds no demo record; start makes a new root and never adopts a directory" % root)
    config = json.loads(config_path.read_text(encoding="utf-8"))
    if config.get("stopped_utc"):
        raise RuntimeError("the demo in %s was stopped at %s; its evidence stays there. Start a new root."
                           % (root, config["stopped_utc"]))
    return config


def attach(config):
    module = runtime(config["loom_prefix"])
    try:
        session = module.Session.attach(config["session"])
    except Exception as error:
        raise RuntimeError("lost: the recorded Loom session in %s does not answer (%s). Nothing was stopped "
                           "or removed: Workshop (pid %s) and the Loom host (pid %s) are left as they are; "
                           "close the Workshop window yourself if it is still open, and start a new root."
                           % (config["session"], error, config.get("workshop_pid"), config.get("host_pid")))
    if session.lifetime != config["lifetime"]:
        session.close()
        raise RuntimeError("this directory now names a different ELH lifetime; nothing was changed")
    return module, session


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["list", "describe", "start", "status", "walk", "reset", "stop", "export"])
    parser.add_argument("name", nargs="?", help="describe: the setup's name or directory; walk: the walk's name")
    parser.add_argument("--root", type=Path, help="dedicated instance directory; never a source or weaver state directory")
    parser.add_argument("--setup", help="a setup's name (see list) or a directory holding setup.json; a first start defaults to values")
    parser.add_argument("--build", help="configured, built Zengine tree")
    parser.add_argument("--loom-prefix", help="installed Loom prefix, including loom-host and Python runtime")
    parser.add_argument("--zengine-prefix", help="installed Zengine prefix, for a setup whose project builds against it")
    parser.add_argument("--toolchain-bin", help="a directory put first on PATH for Workshop and the Loom host")
    parser.add_argument("--tui", action="store_true", help="use the classic terminal medium")
    parser.add_argument("--neovim", help="a Neovim program: the Neovim-backed Editor holds the Editor's office from the start")
    parser.add_argument("--wait", type=float, default=900, help="seconds start/status wait for readiness, or walk for its run, before answering")
    parser.add_argument("--pictures", type=Path, help="walk: a new or empty folder for the walk's pictures, rows and steps.json")
    parser.add_argument("--json", action="store_true", help="list/describe: the machine-readable description")
    parser.add_argument("--to", type=Path, help="export: a new directory to copy the setup and its assets into")
    args = parser.parse_args()
    started = time.monotonic()
    if args.action == "list":
        rows = []
        for name, directory in described.collection([EXAMPLES]).items():
            try:
                setup = described.Setup(directory)
                rows.append(setup.summary() if args.json else {"name": name, "title": setup.get("title"),
                                                               "summary": setup.get("summary")})
            except described.SetupError as error:
                rows.append({"name": name, "unusable": str(error)})
        if args.json:
            print(json.dumps(rows, indent=1))
        else:
            for r in rows:
                print("%-18s %s" % (r["name"], r.get("title") or "UNUSABLE: " + r["unusable"]))
                if r.get("summary"):
                    print("%-18s %s" % ("", r["summary"]))
        return
    if args.action in ("describe", "export"):
        setup = find(args.name or args.setup or parser.error("name a setup"))
        if args.action == "export":
            if not args.to:
                parser.error("export needs --to")
            copy = setup.export(args.to)
            print(json.dumps({"exported": setup.name, "to": str(copy.root), "placed": setup.placements(),
                              "assets": [n for n, _ in copy.assets()], "digest": copy.digest()}, indent=1))
        else:
            print(json.dumps(setup.summary(), indent=1) if args.json else setup.describe())
        return
    if args.root is None:
        parser.error("--root is required")
    root = args.root.resolve()
    if args.action == "start" and not root.exists():
        setup = find(args.setup or "values")
        missing = prerequisites(args, setup)
        if missing:
            raise RuntimeError("setup %s cannot start here; nothing was started:\n  - %s"
                               % (setup.name, "\n  - ".join(missing)))
        result = launch(args, setup, root)
    else:
        config = existing(args, root)
        module, session = attach(config)
        with session:
            if args.action == "walk":
                if not args.name or not args.pictures:
                    parser.error("walk needs the walk's name and --pictures")
                result = walk(module, session, config, args.name, args.pictures, args.wait)
            elif args.action == "reset":
                result = artifact(tool(session, "demo-reset", timeout=args.wait + 15, seconds=args.wait), "reset.json")
                result["preparation"] = preparation(session, config)
            elif args.action == "stop":
                # The quit first: refused, it leaves the demo exactly as it was, service and all.
                try:
                    tool(session, "demo-stop")
                except RuntimeError as refused:
                    said = (str(refused).splitlines() or [""])[0].rstrip(". ")  # not its cleanup notes
                    raise RuntimeError("stop: %s. Nothing was stopped: the demo is still running. "
                                       "Save or discard what is named, then stop again." % said)
                try:
                    session.cancel(config["service"], "demo stopped")
                    session.wait(config["service"], timeout=15)
                except Exception:
                    pass  # the service ends with the link Workshop closed
                wait_for(lambda: all(r["name"] != "workshop" or r["state"] != "admitted"
                                     for r in session.describe()["links"]), 20,
                         "Workshop closing its link after the quit it accepted")
                session.shutdown("demo stopped")
                config["stopped_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                save_json(root / "demo.json", config)
                result = {"state": "stopped", "history": str(root / "workshop" / "history.txt")}
            else:
                if args.action == "start" and args.setup is not None:
                    asked = find(args.setup)
                    if str(asked.root) != config.get("directory", ""):
                        raise RuntimeError("this root prepared setup %s (%s); a different setup needs a new root"
                                           % (config["setup"], config.get("directory")))
                wait = args.action == "start" or "--wait" in sys.argv
                result = ready(module, session, config, args.wait) if wait else artifact(tool(session, "demo-status"), "status.json")
                result.update(session=config["session"], root=str(root), reused=True, lifetime=session.lifetime,
                              endpoint=config.get("endpoint", "see workshop/guests.port"))
                try:
                    now = described.Setup(Path(config["directory"])).digest()
                except described.SetupError as error:
                    now = "unreadable: %s" % error
                if now != config.get("digest"):
                    result["description_changed"] = ("the setup's description or assets changed since this root "
                        "prepared it (%s -> %s). This root keeps the revision it prepared (%s) and Reset restores "
                        "that revision; start a new root to use the change."
                        % (config.get("digest"), now, config.get("prepared", "its service's loaded copy")))
                if args.action == "start":
                    result["guidance"] = guidance(find(config.get("prepared") or config["directory"]), root)
    result["elapsed_ms"] = (time.monotonic() - started) * 1000
    print(json.dumps(result, indent=2, default=str))
    if result.get("state") not in ("ready", "stopped", "passed", None):
        sys.exit(1 if result.get("state") == "failed" else 3)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
