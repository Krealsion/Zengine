#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# THE COMPILER CACHE'S STORES -- what CI's build jobs and its hourly check ask of a store
# (VM-LANE-23 in agents/verification/lanes.md says why a store must be level and what keeps it so).
# A store's key names what it was built against and who saved it:
#
#   <family><compiler>-<loom>-<run id>-<attempt>
#
# the family the job's (`ccache-linux-debug-`), the compiler a digest of the compilers' bytes (16
# hexadecimal digits), Loom the commit the run built against (40) and the run that saved it.
#
#   python3 tests/ci_store.py say --title <job> --family <family> --loom <commit> [--key <key>]
#   python3 tests/ci_store.py ask --store <family>=<key> [--store <family>=<key> ...]
#   python3 tests/ci_store.py newest --zengine <commit> --loom <commit>
#   python3 tests/ci_store.py --self-test
#
# `say` writes the job's summary lines, in Markdown: which run saved the store the job restored
# (the key `actions/cache/restore` matched, empty when it restored none) and whether that store
# was built against this run's Loom.
# `ask` resolves Loom's `main` and says, a line a family, whether the family's newest store (the
# key a lookup matched, empty when there is none) was built against it; it writes `level=true` to
# GITHUB_OUTPUT when every one was, else `level=false`.
# `newest` says whether a run's store would still be the newest: its commit still `main`'s tip and
# its Loom still Loom's `main`. It writes `save=true` to GITHUB_OUTPUT when both are, or when either
# cannot be resolved, so a lost answer keeps the save; else `save=false`, and the newer run saves.
# Its exit is 0, 1 when a self-test case fails and 2 on arguments it cannot read.

import argparse
import os
import re
import subprocess
import sys

LOOM_URL = "https://github.com/Krealsion/Loom.git"
COMMIT = re.compile(r"^[0-9a-f]{40}$")


def read_key(family, key):
    """(loom, run id) of a store's key, loom None when the key names none; None for no key or one
    of another family."""
    if not key or not key.startswith(family):
        return None
    rest = key[len(family):]
    m = re.fullmatch(r"[0-9a-f]{16}-([0-9a-f]{40})-([0-9]+)-[0-9]+", rest)
    if m:
        return m.group(1), m.group(2)
    m = re.fullmatch(r"[0-9a-f]{16}-([0-9]+)-[0-9]+", rest)
    if m:
        return None, m.group(1)
    return None


def run_link(run, server, repository):
    if server and repository:
        return "[%s](%s/%s/actions/runs/%s)" % (run, server, repository, run)
    return run


def say_lines(title, family, loom, key, server="", repository=""):
    read = read_key(family, key)
    head = "## %s: the store it restored\n\n" % title
    if not key:
        return head + "No store was restored: every compile is a miss, and this build is cold.\n"
    if read is None:
        return head + "The store `%s` was restored, a key of no family this job reads.\n" % key
    store_loom, run = read
    saved = "The store run %s saved was restored" % run_link(run, server, repository)
    if store_loom is None:
        return head + saved + ": its key names no Loom, so whether it was built against this run's is not known.\n"
    if store_loom == loom:
        return head + saved + ", built against Loom `%s`, this run's Loom.\n" % loom[:7]
    return head + saved + (", built against Loom `%s`, not this run's `%s`: what Loom changed "
                           "between them is compiled again.\n" % (store_loom[:7], loom[:7]))


def ask_verdict(stores, loom):
    """(level, lines): every family's newest store against `loom`, a line each."""
    lines = []
    level = True
    for family, key in stores:
        read = read_key(family, key)
        if not key:
            lines.append("%s: no store on main, so it is rebuilt" % family)
            level = False
        elif read is None or read[0] is None:
            lines.append("%s: the newest store, %s, names no Loom, so it is rebuilt" % (family, key))
            level = False
        elif read[0] != loom:
            lines.append("%s: the newest store, run %s's, was built against Loom %s, behind Loom's "
                         "main %s, so it is rebuilt" % (family, read[1], read[0][:7], loom[:7]))
            level = False
        else:
            lines.append("%s: the newest store, run %s's, was built against Loom's main %s: level"
                         % (family, read[1], loom[:7]))
    return level, lines


def newest_verdict(zengine, loom, zengine_main, loom_main):
    """(save, words): whether a run at `zengine` against `loom` saves its store, given each
    `main` as resolved now (None when it could not be)."""
    if zengine_main is None or loom_main is None:
        return True, "a main could not be resolved, so the store is saved as before"
    if zengine_main != zengine:
        return False, ("main is at %s now, past this run's %s: the newer commit's run saves the store"
                       % (zengine_main[:7], zengine[:7]))
    if loom_main != loom:
        return False, ("Loom's main is at %s now, past this run's %s: a run against it saves the store"
                       % (loom_main[:7], loom[:7]))
    return True, "this run's commit is main's tip and its Loom Loom's main: the store is saved"


def resolve_main(url):
    try:
        out = subprocess.run(["git", "ls-remote", url, "refs/heads/main"], capture_output=True,
                             text=True, timeout=60).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    sha = out.split()[0] if out.split() else ""
    return sha if COMMIT.match(sha) else None


def write_output(name, value):
    path = os.environ.get("GITHUB_OUTPUT")
    if path:
        with open(path, "a", encoding="utf-8") as f:
            f.write("%s=%s\n" % (name, value))


def self_test():
    failures = []
    loom = "6314498" + "1" * 33
    older = "9c9ee9a" + "2" * 33
    family = "ccache-linux-debug-"
    compiler = "3dd0a4ce9331d7bf"
    now_key = "%s%s-%s-45508015165-1" % (family, compiler, loom)
    old_key = "%s%s-%s-45480059320-2" % (family, compiler, older)
    bare_key = "%s%s-45442148419-1" % (family, compiler)

    def expect(case, got, want):
        if got != want:
            failures.append("%s: wanted %r, got %r" % (case, want, got))

    expect("a key that names its Loom", read_key(family, now_key), (loom, "45508015165"))
    expect("a key that names none", read_key(family, bare_key), (None, "45442148419"))
    expect("no key", read_key(family, ""), None)
    expect("another family's key", read_key("ccache-linux-boxed-", now_key), None)
    expect("a family that only begins this one's", read_key("ccache-linux-asan-",
           "ccache-linux-asan-panes-%s-%s-1-1" % (compiler, loom)), None)

    said = say_lines("J", family, loom, now_key, "https://github.com", "o/r")
    expect("said, this run's Loom", "this run's Loom" in said and
           "(https://github.com/o/r/actions/runs/45508015165)" in said, True)
    said = say_lines("J", family, loom, old_key)
    expect("said, an older Loom", "not this run's `6314498`" in said and "`9c9ee9a`" in said, True)
    expect("said, no Loom named", "names no Loom" in say_lines("J", family, loom, bare_key), True)
    expect("said, nothing restored", "No store was restored" in say_lines("J", family, loom, ""), True)

    level, lines = ask_verdict([(family, now_key), ("ccache-linux-boxed-", "")], loom)
    expect("asked, one family with no store", (level, "no store" in lines[1]), (False, True))
    level, lines = ask_verdict([(family, old_key)], loom)
    expect("asked, a store behind", (level, "behind Loom's main 6314498" in lines[0]), (False, True))
    level, _ = ask_verdict([(family, bare_key)], loom)
    expect("asked, a store that names no Loom", level, False)
    level, _ = ask_verdict([(family, now_key), (family, now_key)], loom)
    expect("asked, every store level", level, True)

    z = "2728c4d6" + "3" * 32
    z2 = "f9193b0a" + "4" * 32
    expect("newest, both mains still this run's", newest_verdict(z, loom, z, loom)[0], True)
    expect("newest, main moved past", newest_verdict(z, loom, z2, loom)[0], False)
    expect("newest, Loom moved past", newest_verdict(z, loom, z, older)[0], False)
    expect("newest, a main unresolved", newest_verdict(z, loom, None, older)[0], True)

    for f in failures:
        print("ci-store: self-test FAILED: %s" % f)
    if not failures:
        print("ci-store: self-test passed")
    return 1 if failures else 0


def main(argv):
    parser = argparse.ArgumentParser(prog="ci_store.py")
    parser.add_argument("--self-test", action="store_true")
    sub = parser.add_subparsers(dest="command")
    say = sub.add_parser("say")
    say.add_argument("--title", required=True)
    say.add_argument("--family", required=True)
    say.add_argument("--loom", required=True)
    say.add_argument("--key", default="")
    ask = sub.add_parser("ask")
    ask.add_argument("--store", action="append", required=True)
    newest = sub.add_parser("newest")
    newest.add_argument("--zengine", required=True)
    newest.add_argument("--loom", required=True)
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()
    if args.command == "say":
        if not COMMIT.match(args.loom):
            parser.error("a commit is 40 hexadecimal digits, not '%s'" % args.loom)
        sys.stdout.write(say_lines(args.title, args.family, args.loom, args.key,
                                   os.environ.get("GITHUB_SERVER_URL", ""),
                                   os.environ.get("GITHUB_REPOSITORY", "")))
        return 0
    if args.command == "ask":
        stores = []
        for one in args.store:
            family, _, key = one.partition("=")
            if not family:
                parser.error("--store is <family>=<key>, not '%s'" % one)
            stores.append((family, key))
        loom = resolve_main(LOOM_URL)
        if loom is None:
            print("ci-store: Loom's main could not be resolved, so every store is rebuilt")
            write_output("level", "false")
            return 0
        level, lines = ask_verdict(stores, loom)
        for line in lines:
            print("ci-store: " + line)
        print("ci-store: %s" % ("every store is level with Loom's main" if level else
                               "a store is behind Loom's main, so the build jobs run on main"))
        write_output("level", "true" if level else "false")
        return 0
    if args.command == "newest":
        for commit in (args.zengine, args.loom):
            if not COMMIT.match(commit):
                parser.error("a commit is 40 hexadecimal digits, not '%s'" % commit)
        server = os.environ.get("GITHUB_SERVER_URL", "https://github.com")
        repository = os.environ.get("GITHUB_REPOSITORY", "Krealsion/Zengine")
        save, words = newest_verdict(args.zengine, args.loom,
                                     resolve_main("%s/%s.git" % (server, repository)),
                                     resolve_main(LOOM_URL))
        print("ci-store: " + words)
        write_output("save", "true" if save else "false")
        return 0
    parser.error("say, ask or newest, or --self-test")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
