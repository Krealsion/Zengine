# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/repo -- Git, and a pull request on GitHub, for one named repository, from the external
host: read its state, stage named paths, commit, push the current branch, open or read a pull
request. loom-tool.json describes the inputs.

WHAT IT RUNS. `git` in `root` and, for a pull request, GitHub's `gh`, on this machine, as this
session's user and with that user's own credentials: scoped local work, not an operation through
Workshop, and not a sandbox. Every command's whole output is kept as a file and its exit code is
recorded as the process returned it.

WHAT IT REFUSES. A commit names its author as `Name <email>`, who is also its committer; a message
that carries a trailer is refused before anything is written, and a made commit is read back:
its trailers must be empty, and a repository that carries tests/check_commit_attribution.cmake
must pass it. It never merges, never force-pushes, never pushes the default branch, never
rewrites or deletes history, and never deletes a branch, a file or a pull request."""
import json
import os
import re
import subprocess
import time
from pathlib import Path

READS = ("status", "diff", "log", "show", "fetch")
OPS = READS + ("add", "commit", "push", "pr-create", "pr-edit", "pr-view", "pr-checks", "run-view")


class Commands:
    """Each command run, its output kept as <op>-<n>.log, its exit code as returned."""

    def __init__(self, ctx, root, op):
        self.ctx, self.root, self.op, self.done = ctx, root, op, []

    def run(self, argv, env=None, must=True):
        self.ctx.step("%s: %s" % (self.op, " ".join(argv[:3])))
        started = time.monotonic()
        done = subprocess.run(argv, cwd=str(self.root), capture_output=True, env=env, timeout=900)
        data = done.stdout + done.stderr
        self.ctx.produce("%s-%d.log" % (self.op, len(self.done)), data)
        text = data.decode("utf-8", "replace")
        self.done.append({"argv": argv, "exit": done.returncode, "bytes": len(data),
                          "seconds": round(time.monotonic() - started, 1),
                          "tail": text.splitlines()[-6:]})
        if must:
            self.ctx.check(done.returncode == 0, "`%s` exited %d: %s" % (
                " ".join(argv[:4]), done.returncode, " | ".join(text.splitlines()[-3:])))
        return done.returncode, done.stdout.decode("utf-8", "replace")

    def git(self, *args, **more):
        return self.run(["git", "-C", str(self.root)] + list(args), **more)


def author_of(ctx):
    found = re.fullmatch(r"\s*([^<>]+?)\s*<([^<>\s]+@[^<>\s]+)>\s*", ctx.inputs.get("author", ""))
    ctx.check(found, "author must be `Name <email>` -- the author and the committer of the commit")
    return found.group(1), found.group(2)


def branch_of(ctx, c):
    _, name = c.git("rev-parse", "--abbrev-ref", "HEAD")
    name = name.strip()
    ctx.check(name and name != "HEAD", "the repository is not on a branch (detached HEAD)")
    return name


def default_branch(c):
    code, ref = c.git("symbolic-ref", "--quiet", "refs/remotes/origin/HEAD", must=False)
    return ref.strip().rsplit("/", 1)[-1] if code == 0 and ref.strip() else "main"


def run(ctx):
    root = Path(ctx.inputs["root"])
    ctx.check(root.is_absolute() and (root / ".git").exists(), "root must be an absolute path to a Git checkout")
    op = ctx.inputs["op"]
    ctx.check(op in OPS, "op must be one of %s" % ", ".join(OPS))
    c = Commands(ctx, root, op)
    said = ""
    if op == "status":
        c.git("status", "--short", "--branch")
        _, said = c.git("log", "-1", "--format=%H %an <%ae> / %cn <%ce>%nTRAILERS:[%(trailers)]")
    elif op == "diff":
        spec = [s for s in ctx.inputs.get("range", "").split() if s]
        c.git("diff", "--stat", *spec)
        c.git("diff", *spec)
    elif op == "log":
        _, said = c.git("log", "--format=%h %an <%ae> %s", "-n", str(int(ctx.inputs.get("count", 10))),
                        *[s for s in ctx.inputs.get("range", "").split() if s])
    elif op == "show":
        c.git("show", "--stat", ctx.inputs.get("range", "") or "HEAD")
    elif op == "fetch":
        c.git("fetch", ctx.inputs.get("remote", "origin"))
        _, said = c.git("rev-parse", "HEAD", "%s/%s" % (ctx.inputs.get("remote", "origin"), default_branch(c)))
    elif op == "add":
        paths = json.loads(ctx.inputs.get("paths", "[]") or "[]")
        ctx.check(paths and all(isinstance(p, str) and p and not p.startswith("-") for p in paths),
                  "add names at least one path, none starting with '-'")
        c.git("add", "--", *paths)
        _, said = c.git("status", "--short")
    elif op == "commit":
        name, email = author_of(ctx)
        message = ctx.inputs.get("message", "")
        ctx.check(message.strip(), "commit needs a message")
        probe = subprocess.run(["git", "-C", str(root), "interpret-trailers", "--parse"],
                               input=message.encode("utf-8"), capture_output=True)
        ctx.check(probe.returncode == 0 and not probe.stdout.strip(),
                  "the message carries a trailer (%r): refused before anything was written"
                  % probe.stdout.decode("utf-8", "replace").strip())
        Path("message.txt").write_text(message, encoding="utf-8")
        env = dict(os.environ, GIT_AUTHOR_NAME=name, GIT_AUTHOR_EMAIL=email,
                   GIT_COMMITTER_NAME=name, GIT_COMMITTER_EMAIL=email)
        c.git("-c", "user.name=" + name, "-c", "user.email=" + email, "commit", "--file",
              str(Path("message.txt").resolve()), env=env)
        _, said = c.git("log", "-1", "--format=%H %an <%ae> / %cn <%ce>%nTRAILERS:[%(trailers)]")
        ctx.check(said.strip().endswith("TRAILERS:[]"), "the commit carries trailers: %r" % said)
        ctx.check(("%s <%s> / %s <%s>" % (name, email, name, email)) in said,
                  "the commit's author or committer is not %s <%s>: %r" % (name, email, said))
        if (root / "tests" / "check_commit_attribution.cmake").is_file():
            c.run(["cmake", "-P", "tests/check_commit_attribution.cmake"])
    elif op == "push":
        branch = branch_of(ctx, c)
        ctx.check(branch != default_branch(c), "refusing to push the default branch %r: push a branch "
                  "and open a pull request" % branch)
        ctx.check(ctx.inputs.get("branch", branch) == branch, "the checkout is on %r, not %r"
                  % (branch, ctx.inputs.get("branch")))
        c.git("push", "--set-upstream", ctx.inputs.get("remote", "origin"), "HEAD:refs/heads/" + branch)
        _, said = c.git("rev-parse", "HEAD")
    elif op in ("pr-create", "pr-edit"):
        body = ctx.inputs.get("body", "")
        ctx.check(body.strip(), "%s needs a body" % op)
        Path("body.md").write_text(body, encoding="utf-8")
        if op == "pr-create":
            branch = branch_of(ctx, c)
            ctx.check(ctx.inputs.get("title", "").strip(), "pr-create needs a title")
            _, said = c.run(["gh", "pr", "create", "--base", ctx.inputs.get("base", "") or default_branch(c),
                             "--head", branch, "--title", ctx.inputs["title"],
                             "--body-file", str(Path("body.md").resolve())])
        else:
            pr = str(int(ctx.inputs.get("pr", 0)))
            ctx.check(pr != "0", "pr-edit names the pull request's number")
            _, said = c.run(["gh", "pr", "edit", pr, "--body-file", str(Path("body.md").resolve())])
    elif op in ("pr-view", "pr-checks"):
        pr = str(int(ctx.inputs.get("pr", 0)))
        ctx.check(pr != "0", "%s names the pull request's number" % op)
        if op == "pr-view":
            _, said = c.run(["gh", "pr", "view", pr, "--json",
                             "number,url,state,headRefName,headRefOid,mergeable,title,body"])
        else:
            # `gh pr checks` exits non-zero while a check fails or is pending: that is its answer.
            _, said = c.run(["gh", "pr", "checks", pr], must=False)
    elif op == "run-view":
        run_id = str(int(ctx.inputs.get("run", 0)))
        ctx.check(run_id != "0", "run-view names the workflow run's id")
        _, said = c.run(["gh", "run", "view", run_id] + (["--log-failed"] if ctx.inputs.get("log_failed") else []),
                        must=False)
    ctx.produce("repo.json", json.dumps({"op": op, "root": str(root), "commands": c.done}, indent=1).encode())
    return "%s: %s" % (op, " / ".join(line for line in said.strip().splitlines()[:3]) or "done")
