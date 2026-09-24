# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A test rig for the tower-defense story's own custody (examples/tower-defense/story.py): story
roots launched by the story's own `launch` from this build tree -- a real Workshop on the terminal
plan and a real session host from the installed Loom, linked as the story links them -- and the
story's commands run as a maker runs them, one process each.

What the rig adds is only what a test needs and a story root does not keep: the checks and their
evidence, and the Popen handles of the processes it launched, so whether one of them is still
running is asked of the operating system by their parent, independently of the story's own
custody reading."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import types

CHECKS = []
NOTES = {}


def check(name, condition, detail=""):
    CHECKS.append({"check": name, "ok": bool(condition), "detail": str(detail)})
    shown = str(detail).splitlines()[0] if detail else ""
    print("%s  %s%s" % ("ok  " if condition else "FAIL", name, ("  -- " + shown[:300]) if shown else ""),
          flush=True)
    return bool(condition)


def write_evidence(path):
    failed = [c for c in CHECKS if not c["ok"]]
    if path:
        with open(path, "w", encoding="utf-8") as f:
            json.dump({"checks": CHECKS, "failed": len(failed), "notes": NOTES}, f, indent=1, default=str)
    print("%d of %d checks held" % (len(CHECKS) - len(failed), len(CHECKS)), flush=True)
    return 0 if CHECKS and not failed else 1


def arguments(parser):
    for name in ("story", "workshop", "loom-host", "loom-runs", "loom-runtime", "vocabulary", "work"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--evidence", default="")
    return parser


class Rig:
    def __init__(self, args, env=None):
        self.args = args
        self.story_path = Path(args.story).resolve()
        spec = importlib.util.spec_from_file_location("td_story", self.story_path)
        self.story = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.story)
        self.runtime = Path(args.workshop).resolve().parent
        self.tools = {"loom_host": str(Path(args.loom_host).resolve()),
                      "loom_runs": str(Path(args.loom_runs).resolve()),
                      "loom_python": str(Path(args.loom_runtime).resolve()),
                      "vocabulary": str(Path(args.vocabulary).resolve())}
        self.work = Path(args.work).resolve()
        shutil.rmtree(self.work, ignore_errors=True)
        if self.work.exists():
            raise SystemExit("the work directory %s could not be cleared (a process still uses "
                             "something in it); nothing was launched" % self.work)
        self.work.mkdir(parents=True)
        self.env = dict(os.environ, **(env or {}))
        # Workshop is isolated from the user's profile; so are the Neovim swap files and logs.
        for var in ("APPDATA", "LOCALAPPDATA", "XDG_CONFIG_HOME", "XDG_STATE_HOME", "XDG_DATA_HOME",
                    "XDG_CACHE_HOME"):
            self.env[var] = str(self.work / "profile" / var.lower())
            os.makedirs(self.env[var], exist_ok=True)
        self.launched = {}
        rig = self

        class Recorded(subprocess.Popen):
            def __init__(self, *a, **k):
                subprocess.Popen.__init__(self, *a, **k)
                rig.launched[self.pid] = self

        self.story.subprocess = types.SimpleNamespace(
            **dict((n, getattr(subprocess, n)) for n in dir(subprocess) if not n.startswith("_")))
        self.story.subprocess.Popen = Recorded

    def root(self, name, plan="default-load-plan.json"):
        """A story root, launched by the story's own `launch` and recorded as `start` records one."""
        root = self.work / name
        game, wdir, sdir = root / "game", root / "workshop", root / "elh"
        for d in (game, wdir, sdir):
            d.mkdir(parents=True)
        record = {"root": str(root), "runtime": str(self.runtime), "game": str(game), "session": str(sdir),
                  "build": "", "loom_prefix": "", "zengine_prefix": "", "loom_python": self.tools["loom_python"],
                  "toolchain_bin": ""}
        record.update(self.story.launch(self.runtime, game, wdir, sdir, self.tools, self.env, "180x80", plan))
        self.story.save(root / "story.json", record)
        NOTES[name] = dict((k, record[k]) for k in ("workshop_process", "host_process", "lifetime"))
        return root

    def cli(self, root, *argv, timeout=300):
        """One story.py command, as its own process: (exit code, output)."""
        done = subprocess.run([sys.executable, str(self.story_path)] + list(argv) + ["--root", str(root)],
                              capture_output=True, text=True, timeout=timeout, env=self.env)
        return done.returncode, (done.stdout + done.stderr).strip()

    def alive(self, pid):
        """Whether a process this rig launched still runs, asked of the operating system by its
        parent through the handle it kept -- not through the story's custody reading."""
        p = self.launched.get(pid)
        if p is None:
            raise AssertionError("pid %s was not launched by this rig" % pid)
        return p.poll() is None

    def record(self, root):
        return json.loads((Path(root) / "story.json").read_text(encoding="utf-8"))

    def finish(self):
        """Whatever this rig launched and is still running is ended, and said."""
        left = []
        for pid, p in self.launched.items():
            if p.poll() is None:
                p.kill()
                try:
                    p.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    pass
                left.append(pid)
        NOTES["ended_by_the_rig_at_exit"] = left
        return left


def wait_until(predicate, seconds, step=0.2):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return True
        time.sleep(step)
    return predicate()
