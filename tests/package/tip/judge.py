# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
"""Whether main's tip passed CI: the judge `.github/workflows/package.yml`'s `tip` job runs at night
and on a dispatch, in a checkout of `main` (agents/packaging.md, "How the package is made").

    python3 -I judge.py        (from a checkout, with `gh` signed in or GH_TOKEN set)

A night packages only a commit whose CI push run passed, so main's tip whose newest CI push run
finished without passing -- failed, or cancelled, as a job stopped at its time limit leaves it -- is
packaged by no night until a later commit passes. The judge says so, naming the commit and the run,
and exits 1. It says and exits 0 when that run passed, when it is still going, when the commit has
no run yet, and when the commit is the last tag's own: packaged already. It exits 2 when it cannot
judge: `git` or `gh` failed, or GitHub's answer could not be read.

Read from the environment:
  REPO                 owner/name of the repository whose CI runs are read (required)
  TIP                  the commit judged; the checkout's HEAD when unset
  RUNS_OF              the runs' filter, `branch=main&event=push` when unset
  GITHUB_STEP_SUMMARY  a file each verdict is also written to, when set
`TIP` and `RUNS_OF` name another commit and its runs, for a check by hand. Its cases, run in every
pull request that changes it, are `cases.py` beside it.
"""
import json
import os
import subprocess
import sys


def say(line):
    print(line, flush=True)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write(line + "\n")


def cannot(why):
    print("cannot judge main's tip: " + why, file=sys.stderr, flush=True)
    sys.exit(2)


def output(*cmd):
    done = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if done.returncode != 0:
        cannot("`%s` exited %d: %s" % (" ".join(cmd), done.returncode, done.stderr.strip()))
    return done.stdout


def main():
    repo = os.environ.get("REPO", "")
    if not repo:
        cannot("REPO names no repository")
    tip = os.environ.get("TIP") or output("git", "rev-parse", "HEAD").strip()
    runs_of = os.environ.get("RUNS_OF") or "branch=main&event=push"

    tags = output("git", "tag", "--list", "v*", "--sort=-version:refname").split()
    if tags:
        last = tags[0]
        if output("git", "rev-list", "-n", "1", last).strip() == tip:
            say("main's tip %s is %s's: packaged already" % (tip, last))
            return 0

    answer = output("gh", "api", "repos/%s/actions/workflows/ci.yml/runs?%s&head_sha=%s&per_page=1"
                    % (repo, runs_of, tip))
    try:
        runs = json.loads(answer)["workflow_runs"]
        run = runs[0] if runs else None
        if run is not None:
            status, url = run["status"], run["html_url"]
            conclusion = run["conclusion"] or "none"
    except (ValueError, KeyError, TypeError, IndexError) as e:
        cannot("GitHub's answer could not be read (%s: %s): %s"
               % (type(e).__name__, e, answer[:400]))

    if run is None:
        say("main's tip %s has no CI push run yet: a later night judges it" % tip)
        return 0
    if status != "completed":
        say("main's tip %s is still in CI (%s, %s): a later night judges it" % (tip, status, url))
        return 0
    if conclusion == "success":
        say("main's tip %s passed CI (%s)" % (tip, url))
        return 0
    say("main's tip %s did not pass CI: its push run %s concluded %s, so no night packages it until"
        " a later commit passes" % (tip, url, conclusion))
    print("::error::main's tip %s did not pass CI: %s concluded %s" % (tip, url, conclusion),
          flush=True)
    return 1


if __name__ == "__main__":
    sys.exit(main())
