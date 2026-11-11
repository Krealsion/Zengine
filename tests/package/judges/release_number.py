# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
"""One release number in everything a package says: red when any of it says another.

    python -I release_number.py <folder> <X.Y.N>

Each is read where the package puts it:
  bin/zengine-workshop --version   its first line, `zengine X.Y.N`
  bin/loom-host --version          its first line, `loom-host X.Y.N  (weave ABI v...)`
  lib/cmake/zengine/zengineConfigVersion.cmake and lib/cmake/loom/loomConfigVersion.cmake
                                   `set(PACKAGE_VERSION "X.Y.N")`
  BUILD-INFO.txt                   its first line, `zengine        X.Y.N`
  README.txt                       its first line, `Zengine X.Y.N -- ...`
Exit 0 when each says the number, 1 when one is missing, says another or cannot be read, 2 when
the folder holds none of them (an empty population) or the number given is no X.Y.N.
"""
import os
import re
import subprocess
import sys

EXE = ".exe" if os.name == "nt" else ""


def first_line(text):
    lines = text.strip().splitlines()
    return lines[0].strip() if lines else ""


def program(folder, name):
    path = os.path.join(folder, "bin", name + EXE)
    if not os.path.isfile(path):
        return None, "the package has no bin/%s%s" % (name, EXE)
    try:
        done = subprocess.run([path, "--version"], capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=60)
    except (OSError, subprocess.TimeoutExpired) as e:
        return "", "bin/%s%s --version did not answer: %s" % (name, EXE, e)
    if done.returncode != 0:
        return "", "bin/%s%s --version exits %d" % (name, EXE, done.returncode)
    return first_line(done.stdout), None


def text_file(folder, rel):
    path = os.path.join(folder, *rel.split("/"))
    if not os.path.isfile(path):
        return None, "the package has no %s" % rel
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read(), None


def config_version(text):
    m = re.search(r'^set\(PACKAGE_VERSION "([^"]*)"\)', text, re.M)
    return m.group(1) if m else ""


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip())
        return 2
    folder, number = os.path.abspath(argv[0]), argv[1]
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", number):
        print("release number: '%s' is no release number X.Y.N" % number)
        return 2
    # Each: what it is, what it says (None when the package does not hold it), and whether that
    # is the number.
    said = []
    line, why = program(folder, "zengine-workshop")
    said.append(("zengine-workshop --version", line, why, line == "zengine " + number))
    line, why = program(folder, "loom-host")
    said.append(("loom-host --version", line, why,
                 line is not None and line.startswith("loom-host %s  (weave ABI v" % number)))
    for rel in ("lib/cmake/zengine/zengineConfigVersion.cmake",
                "lib/cmake/loom/loomConfigVersion.cmake"):
        text, why = text_file(folder, rel)
        version = config_version(text) if text is not None else None
        said.append((rel, version, why, version == number))
    text, why = text_file(folder, "BUILD-INFO.txt")
    line = first_line(text) if text is not None else None
    said.append(("BUILD-INFO.txt", line, why, line is not None and line.split() == ["zengine", number]))
    text, why = text_file(folder, "README.txt")
    line = first_line(text) if text is not None else None
    said.append(("README.txt", line, why,
                 line is not None and line.startswith("Zengine %s -- " % number)))

    if all(s[1] is None for s in said):
        print("release number: %s holds nothing that says a release number (an empty population)"
              % folder)
        return 2
    wrong = 0
    for what, line, why, ok in said:
        if ok:
            print("ok     %s: %s" % (what, line))
        else:
            wrong += 1
            print("WRONG  %s: %s" % (what, why or ("says '%s', not %s" % (line, number))))
    print("release number: %d of %d say %s" % (len(said) - wrong, len(said), number))
    return 1 if wrong else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
