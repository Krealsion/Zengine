#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""THE MUTANTS' RUNNER -- each mutant of the corpus beside this file put into the tree, built, and
the cases it names run, by the method of agents/verification/mutation.md (VM-MUT).

    python3 tests/mutants/run.py --build <build dir> [--shard <i>/<n>] [--only <file.md>#<id>]...
                                 [--out <dir>] [--jobs <n>] [--build-timeout <s>] [--case-timeout <s>]
    python3 tests/mutants/run.py --list [--shard <i>/<n>] [--only ...]
    python3 tests/mutants/run.py --gather <out dir>... [--summary <file.md>]

A mutant is CAUGHT when every case it names goes red on it (a failed case, a timeout or a crash),
or when a `static_assert` refuses it; SURVIVED when a case it names stays green; STALE when it no
longer applies: its text is not in its file exactly once, it does not build for another reason, it
compiles to the objects the tree already held, a case it names selects nothing, or no comment under
tests/ points at it. The tree is built and every case the run names is run green first; each
mutant's bytes are written over the file's own and the snapshot written back after its cases ran;
at the end the tree is built again, every case is run green again and every object a mutant
changed is checked back to its first bytes. Exit 0 when every mutant was caught; 1 when one
survived or is stale; 2 when the run itself cannot be trusted (a form error, a restore that did not
hold, a case red on the tree as it is, an empty selection, a stop). It edits the source tree it is
given: never run it beside another lane on that tree (VM-MUT-22); it refuses a second runner on one
build tree. The corpus's form is its README.md.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import signal
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
ID_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
IN_RE = re.compile(r"^In `([^`]+)`:$")
RED_RE = re.compile(r"^Red in `([^`]+)`:$")
READ_RE = re.compile(r"^The cases read `([^`]+)` as text\.$")
ENTRY_RE = re.compile(r"^- (.*)$")
POINTER_RE = re.compile(r"tests/mutants/([a-z0-9_]+\.md)#([a-z0-9-]+)((?:, #[a-z0-9-]+)*)")
CXX = (".h", ".hpp", ".ipp", ".inl", ".c", ".cc", ".cpp", ".cxx")
SUMMARY_RE = re.compile(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed")
LISTED_RE = re.compile(r"unskipped test cases passing the current filters:\s*(\d+)")


class FormError(Exception):
    pass


class Stopped(Exception):
    pass


class NotRestored(Exception):
    pass


# ---- the corpus ----------------------------------------------------------------------------------

def parse_corpus_text(name, text):
    """The entries of one corpus file, in order: id, the hunks (file, old, new) and the runs, each
    (binary, case name) or ("ctest", entry). A form error names its line."""
    entries, entry, current_file, red_binary = [], None, None, None
    lines = text.split("\n")
    i = 0

    def fail(why):
        raise FormError(f"{name}:{i + 1}: {why}")

    while i < len(lines):
        line = lines[i]
        if line.startswith("## "):
            ident = line[3:].strip()
            if not ID_RE.match(ident):
                fail(f"an entry's heading is its id, lower-case words joined by dashes: `{ident}`")
            if any(e["id"] == ident for e in entries):
                fail(f"a second entry `{ident}`")
            entry = dict(id=ident, line=i + 1, hunks=[], runs=[], read=[])
            entries.append(entry)
            current_file, red_binary = None, None
        elif entry is not None and READ_RE.match(line):
            entry["read"].append(READ_RE.match(line).group(1))
        elif entry is not None and IN_RE.match(line):
            current_file, red_binary = IN_RE.match(line).group(1), None
        elif entry is not None and RED_RE.match(line):
            red_binary, current_file = RED_RE.match(line).group(1), None
        elif entry is not None and line.startswith("```diff"):
            if current_file is None:
                fail("a hunk with no `In `<file>`:` line before it")
            old, new, i = [], [], i + 1
            while i < len(lines) and not lines[i].startswith("```"):
                body = lines[i]
                mark, rest = (body[:1], body[1:]) if body else (" ", "")
                if mark == " ":
                    old.append(rest)
                    new.append(rest)
                elif mark == "-":
                    old.append(rest)
                elif mark == "+":
                    new.append(rest)
                else:
                    fail(f"a hunk line opens with `{mark}`, not a space, `-` or `+`")
                i += 1
            if i == len(lines):
                fail("a hunk that never closes")
            if old == new:
                fail("a hunk that changes nothing")
            if not old:
                fail("a hunk with no line it replaces or stands beside")
            entry["hunks"].append(dict(file=current_file, old="\n".join(old) + "\n",
                                       new="\n".join(new) + "\n" if new else "",
                                       changed=[l for l in body_changes(lines, i)]))
        elif entry is not None and red_binary is not None and ENTRY_RE.match(line):
            words = [ENTRY_RE.match(line).group(1).strip()]
            while i + 1 < len(lines) and lines[i + 1].startswith("  ") and lines[i + 1].strip():
                i += 1
                words.append(lines[i].strip())
            case = " ".join(words)
            if red_binary == "CTest":
                if not (case.startswith("`") and case.endswith("`")):
                    fail("a CTest entry is named in backticks")
                entry["runs"].append(("ctest", case[1:-1]))
            else:
                entry["runs"].append((red_binary, case))
        i += 1
    for e in entries:
        if not e["hunks"]:
            raise FormError(f"{name}:{e['line']}: `{e['id']}` changes no file")
        if not e["runs"]:
            raise FormError(f"{name}:{e['line']}: `{e['id']}` names no case that must go red")
    return entries


def body_changes(lines, close):
    """The changed lines of the hunk that closes at `close`, for the anchor check."""
    out, k = [], close - 1
    while k >= 0 and not lines[k].startswith("```"):
        if lines[k][:1] in "-+":
            out.append(lines[k][1:])
        k -= 1
    return out


def load_corpus(root):
    entries = []
    files = sorted(p for p in (root / "tests" / "mutants").glob("*.md") if p.name != "README.md")
    for p in files:
        for e in parse_corpus_text(p.name, p.read_text(encoding="utf-8")):
            e["corpus"] = p.name
            e["key"] = f"{p.name}#{e['id']}"
            entries.append(e)
    return entries


def pointed(text):
    """The entries one text's pointers name: `tests/mutants/<file>.md#<id>`, and after it `, #<id>`
    for each further entry of that file."""
    out = []
    for m in POINTER_RE.finditer(text):
        out.append(f"{m.group(1)}#{m.group(2)}")
        out += [f"{m.group(1)}#{more}" for more in re.findall(r"#([a-z0-9-]+)", m.group(3))]
    return out


def comment_runs(text):
    """Each run of `//` lines as one text, its lines joined by a space, with its first line's number."""
    runs, cur, first = [], [], 0
    for n, line in enumerate(text.split("\n"), 1):
        k = line.find("//")
        if k >= 0 and not line[:k].strip():
            if not cur:
                first = n
            cur.append(line[k + 2:].strip())
            continue
        if cur:
            runs.append((first, " ".join(cur)))
            cur = []
        if k >= 0:
            runs.append((n, line[k + 2:]))
    if cur:
        runs.append((first, " ".join(cur)))
    return runs


def claimed(root):
    """Every corpus entry a comment under tests/ points at, with where."""
    seen = {}
    for p in sorted((root / "tests").rglob("*")):
        if p.suffix in CXX and p.is_file() and "third_party" not in p.parts:
            for n, text in comment_runs(p.read_text(encoding="utf-8", errors="replace")):
                for key in pointed(text):
                    seen.setdefault(key, []).append(f"{p.relative_to(root).as_posix()}:{n}")
    return seen


def only_comments(changed):
    """True when every changed line of a hunk is a comment or blank: an anchor on prose."""
    return all((not l.strip()) or l.strip().startswith(("//", "/*", "*")) for l in changed)


def shard_of(entries, shard):
    if not shard:
        return entries
    k, n = shard
    count = len(entries)
    return entries[(k - 1) * count // n:k * count // n]


# ---- the tree ------------------------------------------------------------------------------------

def run(cmd, log, timeout, cwd=None, env=None):
    """Exit, timed out, seconds; the command's whole output in `log`."""
    started = time.monotonic()
    with open(log, "w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(str(c) for c in cmd) + "\n")
        out.flush()
        proc = subprocess.Popen([str(c) for c in cmd], cwd=cwd, env=env, stdout=out,
                                stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = proc.wait(timeout=timeout)
            return code, False, time.monotonic() - started
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
            out.write(f"\n[mutants] timed out after {timeout} s\n")
            return 124, True, time.monotonic() - started
        except BaseException:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
            raise


def write_in_place(path, data):
    """The file's own inode written and truncated (VM-MUT-26), read back."""
    with open(path, "r+b") as f:
        f.seek(0)
        f.write(data)
        f.truncate()
        f.flush()
        os.fsync(f.fileno())
    return path.read_bytes() == data


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def make_program(build):
    """The Ninja the tree was configured with; the runner reads Ninja's own record of what compiled."""
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    if not re.search(r"^CMAKE_GENERATOR:INTERNAL=Ninja$", cache, re.M):
        raise SystemExit(f"[mutants] {build} is not a Ninja tree; the runner reads Ninja's deps")
    m = re.search(r"^CMAKE_MAKE_PROGRAM:[A-Z]+=(.*)$", cache, re.M)
    return m.group(1).strip() if m else "ninja"


def objects_by_source(build, root):
    """Each source or header under the tree, mapped to the objects Ninja recorded it compiled into."""
    out = subprocess.run([make_program(build), "-C", str(build), "-t", "deps"], capture_output=True,
                         text=True, check=True).stdout
    found, obj = {}, None
    for line in out.split("\n"):
        if line and not line.startswith(" "):
            obj = line.split(":", 1)[0]
        elif line.strip() and obj:
            dep = os.path.realpath(os.path.join(str(build), line.strip()))
            found.setdefault(dep, []).append(os.path.realpath(os.path.join(str(build), obj)))
    return found


def ctest_programs(build):
    """Each program a CTest entry runs, with the entry's working directory and environment."""
    out = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                         capture_output=True, text=True, check=True).stdout
    programs = {}
    for t in json.loads(out).get("tests", []):
        cmd = t.get("command") or []
        if not cmd:
            continue
        props = {p["name"]: p["value"] for p in t.get("properties", [])}
        env = dict(os.environ)
        for pair in props.get("ENVIRONMENT", []):
            k, _, v = pair.partition("=")
            env[k] = v
        programs.setdefault(os.path.basename(cmd[0]), dict(path=cmd[0], entry=t["name"],
                                                           cwd=props.get("WORKING_DIRECTORY"), env=env))
    return programs


def case_filter(name):
    return "-tc=" + name.replace("\\", "\\\\").replace(",", "\\,")


class Tree:
    def __init__(self, build, root, out, jobs, build_timeout, case_timeout):
        self.build, self.root, self.out = build, root, out
        self.jobs, self.build_timeout, self.case_timeout = jobs, build_timeout, case_timeout
        self.programs, self.deps = {}, {}
        self.first_bytes, self.changed = {}, set()

    def make(self, log, runs=None):
        """The whole tree, or for a mutant's runs: the programs they name, every library and every
        program no CTest entry runs as a suite -- what a case loads or starts -- and no other
        suite's program, whose objects a mutant's case never reads."""
        cmd = ["cmake", "--build", self.build, "--parallel", self.jobs]
        if runs is not None:
            named = {os.path.relpath(self.programs[k]["path"], str(self.build))
                     for k, _ in runs if k != "ctest" and k in self.programs}
            whole = any(k == "ctest" for k, _ in runs)
            if not whole:
                cmd += ["--target"] + sorted(set(self.always) | named)
        code, timed, secs = run(cmd, log, self.build_timeout)
        return code, timed, secs

    def learn(self):
        self.programs = ctest_programs(self.build)
        self.deps = objects_by_source(self.build, self.root)
        suites = {p["path"] for p in self.programs.values()}
        out = subprocess.run([make_program(self.build), "-C", str(self.build), "-t", "targets", "all"],
                             capture_output=True, text=True, check=True).stdout
        self.always = []
        for line in out.split("\n"):
            target, _, rule = line.partition(": ")
            if re.match(r"^(CXX|C)_(SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE)_LINKER", rule):
                path = os.path.join(str(self.build), target)
                if not (rule.startswith(("CXX_EXECUTABLE", "C_EXECUTABLE")) and path in suites):
                    self.always.append(target)

    def objects_of(self, rel):
        return self.deps.get(os.path.realpath(str(self.root / rel)), [])

    def count(self, binary, case):
        """How many cases a doctest filter selects, from the program's own listing."""
        prog = self.programs[binary]
        done = subprocess.run([prog["path"], "--list-test-cases", case_filter(case)], cwd=prog["cwd"],
                              env=prog["env"], capture_output=True, text=True, timeout=self.case_timeout)
        m = LISTED_RE.search(done.stdout)
        return int(m.group(1)) if m else 0

    def check(self, run_, log):
        """(red, how): a doctest case or a CTest entry run as CTest runs it, with a timeout."""
        kind, name = run_
        if kind == "ctest":
            code, timed, _ = run(["ctest", "--test-dir", self.build, "--output-on-failure",
                                  "--no-tests=error", "--timeout", self.case_timeout,
                                  "-R", "^" + re.escape(name) + "$"], log, self.case_timeout + 60)
            text = pathlib.Path(log).read_text(encoding="utf-8", errors="replace")
            if timed or "***Timeout" in text:
                return True, "timed out"
            if re.search(r"100% tests passed, 0 tests failed out of 1\b", text):
                return False, "passed"
            if re.search(r"tests failed out of 1\b", text):
                return True, "failed"
            return True, f"exit {code} with no CTest summary"
        prog = self.programs[kind]
        code, timed, _ = run([prog["path"], case_filter(name)], log, self.case_timeout, cwd=prog["cwd"],
                             env=prog["env"])
        text = pathlib.Path(log).read_text(encoding="utf-8", errors="replace")
        m = SUMMARY_RE.search(text)
        if timed:
            return True, "timed out"
        if code == 70:
            return True, "selected nothing"
        if m is None:
            return (code != 0), (f"crashed, exit {code}" if code != 0 else "no summary")
        if int(m.group(3)) >= 1 and code != 0:
            return True, "failed"
        return False, "passed" if code == 0 else f"exit {code} with no case failed"


# ---- one mutant ----------------------------------------------------------------------------------

def first_error(log):
    for line in pathlib.Path(log).read_text(encoding="utf-8", errors="replace").split("\n"):
        if "error:" in line:
            return line.split("error:", 1)[1].strip()[:240]
    return "no error line in the build's log"


def occurrences(text, old):
    """Where `old`, whole lines, stands in `text`."""
    at, found = 0, []
    while True:
        k = text.find(old, at)
        if k < 0:
            return found
        if k == 0 or text[k - 1] == "\n":
            found.append(k)
        at = k + 1


def mutate_one(tree, e, claims, logs):
    """The verdict of one mutant: (verdict, why, per-run notes)."""
    if e["key"] not in claims:
        return "stale", "no comment under tests/ points at it", []
    for h in e["hunks"]:
        if only_comments(h["changed"]):
            return "stale", f"its hunk in `{h['file']}` changes only comments (VM-MUT-20)", []
    for kind, name in e["runs"]:
        if kind != "ctest" and kind not in tree.programs:
            return "stale", f"no CTest entry runs `{kind}`", []
        if kind != "ctest" and tree.count(kind, name) != 1:
            return "stale", f"`{name}` selects {tree.count(kind, name)} cases in `{kind}`, not one", []
    # the edit, every hunk of it, judged before any byte is written
    edits = {}
    for h in e["hunks"]:
        path = tree.root / h["file"]
        if not path.is_file():
            return "stale", f"`{h['file']}` is not in the tree", []
        if path not in edits:
            raw = path.read_bytes()
            edits[path] = dict(rel=h["file"], first=raw, text=raw.decode("utf-8"))
        where = occurrences(edits[path]["text"], h["old"])
        if len(where) != 1:
            return "stale", f"its text stands {len(where)} times in `{h['file']}`, not once", []
        k = where[0]
        edits[path]["text"] = edits[path]["text"][:k] + h["new"] + edits[path]["text"][k + len(h["old"]):]
    objects = {}
    for path, ed in edits.items():
        # A FILE ITS CASES READ AS TEXT IS THE ARTIFACT (VM-MUT-32): its bytes, written and read back.
        if path.suffix in CXX and ed["rel"] not in e["read"]:
            objs = tree.objects_of(ed["rel"])
            if not objs:
                return "stale", f"no object of the tree is built from `{ed['rel']}`", []
            for o in objs:
                if o not in tree.changed and os.path.exists(o):
                    tree.first_bytes.setdefault(o, digest(o))
                objects[o] = os.path.getmtime(o) if os.path.exists(o) else None
    notes = []
    try:
        for path, ed in edits.items():
            want = ed["text"].encode("utf-8")
            if not write_in_place(path, want) or digest(path) == hashlib.sha256(ed["first"]).hexdigest():
                return "error", f"the mutant could not be written into `{ed['rel']}`", []
        code, timed, secs = tree.make(logs / f"{e['id']}-build.log", e["runs"])
        if timed:
            return "stale", f"its build ran past {tree.build_timeout} s", []
        if code != 0:
            why = first_error(logs / f"{e['id']}-build.log")
            if "static assertion failed" in why or "static_assert" in why:
                return "caught", f"refused at compile time: {why}", []
            return "stale", f"it does not build: {why}", []
        # THE ARTIFACT THE MUTANT LANDS IN (VM-MUT-09): an object compiled again from its bytes. An
        # object no build of this mutant compiled feeds only another suite's program, and says
        # nothing either way; one compiled again to the bytes it had is a mutant that changed nothing.
        rebuilt = [o for o, before in objects.items()
                   if os.path.exists(o) and os.path.getmtime(o) != before]
        moved = [o for o in rebuilt if digest(o) != tree.first_bytes.get(o)]
        tree.changed.update(rebuilt)
        if objects and not rebuilt:
            return "stale", "nothing its cases run is compiled from the file it changes", []
        if objects and not moved:
            return "stale", "it compiles to the objects the tree already held (VM-MUT-09)", []
        green = []
        for n, r in enumerate(e["runs"]):
            red, how = tree.check(r, logs / f"{e['id']}-run{n + 1}.log")
            notes.append(f"{r[1]}: {how}")
            if not red:
                green.append(r[1])
        if green:
            return "survived", "stayed green: " + "; ".join(green), notes
        return "caught", "every case it names went red", notes
    finally:
        for path, ed in edits.items():
            if not write_in_place(path, ed["first"]):
                raise NotRestored(f"`{ed['rel']}` could not be written back -- the tree holds a "
                                  f"mutant; restore it by hand before anything builds")


# ---- the run -------------------------------------------------------------------------------------

def stop_on_signal(*_):
    raise Stopped()


def self_test():
    """The parser reads a sound sample and refuses each broken one, or nothing is believed."""
    sound = ("# t\n\n## one-thing\n\nWords.\n\nIn `a/b.cpp`:\n\n```diff\n x;\n-    y();\n+    z();\n```\n\n"
             "The cases read `a/b.cpp` as text.\n\n"
             "Red in `p-tests`:\n\n- a case, with a comma\n  wrapped\n\nRed in `CTest`:\n\n- `an_entry`\n")
    got = parse_corpus_text("t.md", sound)
    want = [dict(id="one-thing", line=3, hunks=[dict(file="a/b.cpp", old=" x;\n    y();\n"[1:],
                                                     new="x;\n    z();\n", changed=["    z();", "    y();"])],
                 runs=[("p-tests", "a case, with a comma wrapped"), ("ctest", "an_entry")], read=["a/b.cpp"])]
    if got != want:
        raise SystemExit(f"[mutants] self-test: the sound sample read as {got}")
    for broken in ("## One\n", "## a\n\n```diff\n-x\n```\n", "## a\n\nIn `f`:\n\n```diff\n-x\n",
                   "## a\n\nIn `f`:\n\n```diff\n x\n```\n", "## a\n\nIn `f`:\n\n```diff\n-x\n```\n",
                   "## a\n\nRed in `p`:\n\n- c\n", "## a\n\nIn `f`:\n\n```diff\n+x\n```\n\nRed in `p`:\n\n- c\n"):
        try:
            parse_corpus_text("t.md", broken)
        except FormError:
            continue
        raise SystemExit(f"[mutants] self-test: a broken sample was read: {broken!r}")
    if occurrences("ab\nb\n", "b\n") != [3] or not only_comments(["  // x", ""]) or only_comments(["x;"]):
        raise SystemExit("[mutants] self-test: the line search or the anchor check misreads")
    if case_filter("a, b\\c") != "-tc=a\\, b\\\\c":
        raise SystemExit("[mutants] self-test: a case's filter is spelled wrong")
    if pointed("// x (tests/mutants/a_b.md#one, #two-2): y, #z") != ["a_b.md#one", "a_b.md#two-2"]:
        raise SystemExit("[mutants] self-test: a comment's pointers are misread")
    runs = comment_runs("x;\n    // a (tests/mutants/f.md#one,\n    // #two): b\ny; // c\n")
    if runs != [(2, "a (tests/mutants/f.md#one, #two): b"), (4, " c")] or pointed(runs[0][1]) != ["f.md#one", "f.md#two"]:
        raise SystemExit(f"[mutants] self-test: a comment's run is misread: {runs}")


def gather(dirs, summary):
    rows = []
    for d in dirs:
        p = pathlib.Path(d) / "results.json"
        if not p.is_file():
            rows.append(dict(key=str(d), verdict="error", why="no results.json: the piece did not finish"))
            continue
        rows += json.loads(p.read_text(encoding="utf-8"))["mutants"]
    counts = {v: sum(1 for r in rows if r["verdict"] == v) for v in ("caught", "survived", "stale", "error")}
    text = [f"{len(rows)} mutants: {counts['caught']} caught, {counts['survived']} survived, "
            f"{counts['stale']} stale" + (f", {counts['error']} not run" if counts["error"] else "") + ".", ""]
    bad = [r for r in rows if r["verdict"] != "caught"]
    if bad:
        text += ["| mutant | verdict | why |", "|---|---|---|"]
        text += [f"| `tests/mutants/{r['key']}` | {r['verdict']} | {r['why'].replace('|', '/')} |" for r in bad]
    out = "\n".join(text) + "\n"
    if summary:
        pathlib.Path(summary).write_text(out, encoding="utf-8")
    print(out, end="")
    return 0 if not bad else (2 if counts["error"] else 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--build")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--gather", nargs="+")
    ap.add_argument("--summary")
    ap.add_argument("--shard")
    ap.add_argument("--corpus", action="append", default=[])
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--out")
    ap.add_argument("--jobs", default=str(os.cpu_count() or 4))
    ap.add_argument("--build-timeout", type=int, default=1800)
    ap.add_argument("--case-timeout", type=int, default=600)
    a = ap.parse_args()
    self_test()
    if a.gather:
        return gather(a.gather, a.summary)
    shard = None
    if a.shard:
        m = re.match(r"^(\d+)/(\d+)$", a.shard)
        if not m or not 1 <= int(m.group(1)) <= int(m.group(2)):
            print(f"[mutants] --shard is <i>/<n> with 1 <= i <= n, not `{a.shard}`")
            return 2
        shard = (int(m.group(1)), int(m.group(2)))
    try:
        entries = load_corpus(ROOT)
    except FormError as err:
        print(f"[mutants] the corpus does not read: {err}")
        return 2
    if a.corpus:
        unknown = [c for c in a.corpus if c not in {e["corpus"] for e in entries}]
        if unknown:
            print(f"[mutants] no corpus file {', '.join(unknown)} holds a mutant")
            return 2
        entries = [e for e in entries if e["corpus"] in a.corpus]
    if a.only:
        unknown = [k for k in a.only if k not in {e["key"] for e in entries}]
        if unknown:
            print(f"[mutants] no mutant {', '.join(unknown)}")
            return 2
        entries = [e for e in entries if e["key"] in a.only]
    chosen = shard_of(entries, shard)
    if not chosen:
        print("[mutants] the selection holds no mutant; an empty run proves nothing")
        return 2
    claims = claimed(ROOT)
    if a.list:
        for e in chosen:
            files = ", ".join(sorted({h["file"] for h in e["hunks"]}))
            print(f"{e['key']}\t{files}\t{len(e['runs'])} case(s)\t"
                  f"{'claimed by ' + ', '.join(claims[e['key']]) if e['key'] in claims else 'NO COMMENT POINTS AT IT'}")
        print(f"[mutants] {len(chosen)} of {len(entries)} mutants")
        return 0
    if not a.build:
        print("[mutants] --build names the build tree the mutants are built in")
        return 2
    build = pathlib.Path(a.build).resolve()
    out = pathlib.Path(a.out or (build / "mutants")).resolve()
    logs = out / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    lock = open(build / "mutants.lock", "w")
    try:
        import fcntl
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print(f"[mutants] another runner holds {build}; two never share a tree (VM-MUT-22)")
        return 2
    signal.signal(signal.SIGTERM, stop_on_signal)
    signal.signal(signal.SIGINT, stop_on_signal)
    tree = Tree(build, ROOT, out, a.jobs, a.build_timeout, a.case_timeout)
    results, started = [], time.monotonic()
    head = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True, text=True).stdout
    print(f"[mutants] {len(chosen)} of {len(entries)} mutants at {head or 'an unknown commit'}, over {build}", flush=True)

    def finish(code, why):
        doc = dict(commit=head, build=str(build), mutants=results, verdict=why,
                   seconds=round(time.monotonic() - started))
        (out / "results.json").write_text(json.dumps(doc, indent=1), encoding="utf-8")
        table = ["| mutant | verdict | why | seconds |", "|---|---|---|---|"]
        table += [f"| `{r['key']}` | {r['verdict']} | {r['why'].replace('|', '/')} | {r['seconds']} |" for r in results]
        (out / "table.md").write_text("\n".join(table) + "\n\n" + why + "\n", encoding="utf-8")
        print(f"[mutants] {why}", flush=True)
        return code

    try:
        code, timed, _ = tree.make(logs / "first-build.log")
        if code != 0:
            return finish(2, f"the tree as it is does not build (logs/first-build.log): no verdict means anything")
        tree.learn()
        # every case the selection names that selects one case here; a mutant naming any other is
        # stale, which its own turn says
        runs = []
        for e in chosen:
            for r in e["runs"]:
                if r not in runs and (r[0] == "ctest" or (r[0] in tree.programs and tree.count(*r) == 1)):
                    runs.append(r)
        red_first = []
        for n, r in enumerate(runs):
            red, how = tree.check(r, logs / f"first-run{n + 1}.log")
            if red:
                red_first.append(f"{r[1]} ({how}, logs/first-run{n + 1}.log)")
        if red_first:
            return finish(2, "red on the tree as it is, before any mutant: " + "; ".join(red_first))
        # A COMMENT POINTING AT NO ENTRY claims a mutant nobody can run: said once a whole run, by
        # its first piece.
        if not a.only and not a.corpus and (shard is None or shard[0] == 1):
            known = {e["key"] for e in entries}
            for key, where in sorted(claims.items()):
                if key not in known:
                    results.append(dict(key=key, verdict="stale", runs=[], seconds=0,
                                        why="a comment points at it and no entry is it: " + ", ".join(where)))
                    print(f"[mutants] {key}: stale -- {results[-1]['why']}", flush=True)
        for e in chosen:
            t0 = time.monotonic()
            verdict, why, notes = mutate_one(tree, e, claims, logs)
            results.append(dict(key=e["key"], verdict=verdict, why=why, runs=notes,
                                seconds=round(time.monotonic() - t0)))
            print(f"[mutants] {e['key']}: {verdict} -- {why} ({results[-1]['seconds']} s)", flush=True)
            if verdict == "error":
                return finish(2, f"{e['key']}: {why}")
        # the tree as it was: built again, every case green, every object a mutant moved back
        code, timed, _ = tree.make(logs / "restored-build.log")
        if code != 0:
            return finish(2, "the restored tree does not build (logs/restored-build.log)")
        back = [o for o in tree.changed if o in tree.first_bytes and os.path.exists(o)
                and digest(o) != tree.first_bytes[o]]
        if back:
            return finish(2, f"{len(back)} objects did not come back to their first bytes, as "
                             f"{os.path.relpath(back[0], build)}")
        red_after = []
        for n, r in enumerate(runs):
            red, how = tree.check(r, logs / f"restored-run{n + 1}.log")
            if red:
                red_after.append(f"{r[1]} ({how})")
        if red_after:
            return finish(2, "red on the restored tree: " + "; ".join(red_after))
        now = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True,
                             text=True).stdout
        if now != dirty:
            return finish(2, "the source tree's status moved during the run:\n" + now)
        counts = {v: sum(1 for r in results if r["verdict"] == v) for v in ("caught", "survived", "stale")}
        why = (f"{len(results)} mutants: {counts['caught']} caught, {counts['survived']} survived, "
               f"{counts['stale']} stale; the restored tree built, its {len(runs)} runs green")
        return finish(0 if counts["caught"] == len(results) else 1, why)
    except Stopped:
        return finish(2, "stopped before the end; every mutant written was written back")
    except NotRestored as err:
        return finish(2, str(err))
    except Exception as err:  # the runner's own failure is no verdict on any mutant
        return finish(2, f"the runner failed: {type(err).__name__}: {err}")


if __name__ == "__main__":
    sys.exit(main())
