# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
"""The tip judge, judged on fixed cases (`.github/workflows/package.yml`'s `tip-cases` job, in every
pull request that touches how the package is made). It reads no GitHub: each case's answer is here.

    python3 -I cases.py [--judge <judge.py>]     (`--judge`: a changed copy, to see a case catch it)

In a repository of its own -- commits A tagged v0.9.0, B tagged v0.10.0 and C untagged -- each case
checks one commit out and runs the judge as the night does, with `gh` on PATH a stand-in that
answers only a query for the newest CI push runs on main of the commit checked out, with the case's
runs, newest first, and refuses any other:

  passed         C, its newest run passed (an older one failed)      green, says it passed
  failed         C, its newest run failed (an older one passed)      red, names the run
  cancelled      C, its newest run cancelled                         red, names the run
  still going    C, its newest run in progress (an older one failed) green, says still in CI
  no run         C, no run                                           green, says no run yet
  last tag's     B, the last tag by version; its run failed          green, packaged already
  older tag's    A, a tag but not the last; its run failed           red, names the run
  unreadable     C, an answer that is not JSON                       not green: cannot judge, exit 2

A verdict is the exit, the words and the commit said, the run named, the step summary and the
`::error::` line. Exit 0 when every case gives the verdict expected, 1 otherwise, each wrong
verdict named.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = "owner/zengine"

STAND_IN = r'''#!/usr/bin/env python3
"""`gh` for the tip judge's cases: answers `gh api <the runs query>` with CASE_RUNS, and only when
the query asks for CASE_REPO's CI push runs on main of CASE_TIP."""
import os, sys, urllib.parse
args = sys.argv[1:]
path, _, query = (args[1] if len(args) == 2 and args[0] == "api" else "").partition("?")
q = urllib.parse.parse_qs(query)
want = "repos/%s/actions/workflows/ci.yml/runs" % os.environ["CASE_REPO"]
if (path != want or q.get("branch") != ["main"] or q.get("event") != ["push"]
        or q.get("head_sha") != [os.environ["CASE_TIP"]]):
    sys.stderr.write("gh stand-in: asked %r, which is not the CI push runs on main of %s\n"
                     % (args, os.environ["CASE_TIP"]))
    sys.exit(4)
with open(os.environ["CASE_RUNS"], encoding="utf-8") as f:
    sys.stdout.write(f.read())
'''


def git(cwd, *args):
    env = dict(os.environ, GIT_AUTHOR_NAME="case", GIT_AUTHOR_EMAIL="case@example.invalid",
               GIT_COMMITTER_NAME="case", GIT_COMMITTER_EMAIL="case@example.invalid",
               GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
    return subprocess.run(["git", "-c", "init.defaultBranch=main", *args], cwd=cwd, env=env,
                          check=True, capture_output=True, text=True).stdout.strip()


def run_of(n, status, conclusion):
    return {"id": n, "status": status, "conclusion": conclusion,
            "html_url": "https://github.com/%s/actions/runs/%d" % (REPO, n)}


def main():
    judge = os.path.join(HERE, "judge.py")
    if sys.argv[1:2] == ["--judge"] and len(sys.argv) == 3:
        judge = os.path.abspath(sys.argv[2])
    elif len(sys.argv) != 1:
        print(__doc__)
        return 2

    work = tempfile.mkdtemp(prefix="tip-cases-")
    try:
        repo = os.path.join(work, "repo")
        os.makedirs(repo)
        git(repo, "init", "-q")
        commits = {}
        for name, tag in (("A", "v0.9.0"), ("B", "v0.10.0"), ("C", None)):
            git(repo, "commit", "-q", "--allow-empty", "-m", name)
            commits[name] = git(repo, "rev-parse", "HEAD")
            if tag:
                git(repo, "tag", "-a", tag, "-m", tag)

        bin_dir = os.path.join(work, "bin")
        os.makedirs(bin_dir)
        with open(os.path.join(bin_dir, "gh"), "w", encoding="utf-8", newline="\n") as f:
            f.write(STAND_IN)
        os.chmod(os.path.join(bin_dir, "gh"), 0o755)

        passed = run_of(11, "completed", "success")
        failed = run_of(12, "completed", "failure")
        cancelled = run_of(13, "completed", "cancelled")
        going = run_of(14, "in_progress", None)
        # (name, commit, runs newest first, exit, words said, run the verdict names)
        cases = [
            ("passed", "C", [passed, failed], 0, "passed CI", passed),
            ("failed", "C", [failed, passed], 1, "did not pass CI", failed),
            ("cancelled", "C", [cancelled], 1, "concluded cancelled", cancelled),
            ("still going", "C", [going, failed], 0, "is still in CI (in_progress", going),
            ("no run", "C", [], 0, "has no CI push run yet", None),
            ("last tag's", "B", [failed], 0, "is v0.10.0's: packaged already", None),
            ("older tag's", "A", [failed], 1, "did not pass CI", failed),
            ("unreadable", "C", "<html>busy</html>", 2, "could not be read", None),
        ]
        wrong = []
        for name, commit, runs, want_exit, words, named in cases:
            sha = commits[commit]
            git(repo, "checkout", "-q", "--detach", sha)
            answer = os.path.join(work, "runs.json")
            with open(answer, "w", encoding="utf-8") as f:
                if isinstance(runs, str):
                    f.write(runs)
                else:
                    json.dump({"total_count": len(runs), "workflow_runs": runs}, f)
            summary = os.path.join(work, "summary.md")
            open(summary, "w").close()
            unset = ("TIP", "RUNS_OF", "GH_TOKEN", "GITHUB_TOKEN", "GITHUB_STEP_SUMMARY")
            env = {k: v for k, v in os.environ.items() if k not in unset}
            env.update(PATH=bin_dir + os.pathsep + os.environ.get("PATH", ""), REPO=REPO,
                       CASE_REPO=REPO, CASE_TIP=sha, CASE_RUNS=answer, GITHUB_STEP_SUMMARY=summary)
            done = subprocess.run([sys.executable, "-I", judge], cwd=repo, env=env,
                                  capture_output=True, text=True, encoding="utf-8",
                                  errors="replace")
            said = done.stdout + done.stderr
            with open(summary, encoding="utf-8") as f:
                summarized = f.read()
            faults = []
            if done.returncode != want_exit:
                faults.append("exit %d, expected %d" % (done.returncode, want_exit))
            if want_exit == 2:
                if words not in done.stderr:
                    faults.append("did not say %r" % words)
            elif words not in done.stdout or sha not in done.stdout:
                faults.append("did not say %r of %s" % (words, sha))
            if want_exit != 2 and words not in summarized:
                faults.append("the step summary does not say %r" % words)
            if named is not None and named["html_url"] not in done.stdout:
                faults.append("did not name the run %s" % named["html_url"])
            error = "::error::" in done.stdout
            if error != (want_exit == 1):
                faults.append("an ::error:: line %s" % ("written" if error else "missing"))
            print("%s  %s" % ("FAIL" if faults else "ok  ", name))
            if faults:
                wrong.append(name)
                for fault in faults:
                    print("    " + fault)
                print("    " + said.strip().replace("\n", "\n    "))
        print("%d of %d cases gave the verdict expected" % (len(cases) - len(wrong), len(cases)))
        return 1 if wrong or not cases else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
