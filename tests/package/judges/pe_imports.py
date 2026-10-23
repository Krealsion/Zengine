# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A package judge: every Windows image imports only the package and Windows itself.

    python -I pe_imports.py <package folder> [--system <dir>]...

Reads every .exe and .dll under the folder (PE32+), its import and delay-import tables, and names
each DLL it imports:
  package   a file of that name is in the package, anywhere under the folder
  apiset    an API set (api-ms-win-*, ext-ms-win-*), which Windows resolves itself
  system    in a --system folder (%SystemRoot%\\System32 by default) and not a redistributable a
            clean Windows does not carry (REDISTRIBUTABLE)
  FOREIGN   anything else: a DLL the package needs and does not carry
Exit 0 when nothing is FOREIGN, 1 otherwise, 2 on an empty population, a usage error, or its own
samples misread, which it reads first. Standard library only; it reads the tables, it loads nothing.
"""
import os
import struct
import sys

# What a developer's System32 may hold and a clean Windows does not: the Visual C++ runtime, the
# debug CRT, other toolchains' runtimes, libraries a package must carry, and the DirectX
# redistributable's shader compilers (43 and before; Windows itself carries d3dcompiler_47).
REDISTRIBUTABLE = ("vcruntime", "msvcp1", "msvcp2", "concrt", "vccorlib", "vcomp", "mfc", "ucrtbased",
                   "libstdc++", "libgcc", "libwinpthread", "libc++", "libunwind", "sdl", "freetype",
                   "zlib", "python", "d3dcompiler_3", "d3dcompiler_40", "d3dcompiler_41",
                   "d3dcompiler_42", "d3dcompiler_43")


def rva_to_offset(sections, rva):
    for va, vsize, raw, rsize in sections:
        if va <= rva < va + max(vsize, rsize):
            return rva - va + raw
    return None


def read_cstr(data, off):
    return data[off:data.index(b"\0", off)].decode("ascii", "replace")


def imports(data):
    """The DLL names a PE32+ image imports, normal and delay-loaded, in table order."""
    if data[:2] != b"MZ":
        raise ValueError("not an MZ image")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("no PE signature")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x20B:
        raise ValueError("not PE32+")
    dirs = opt + 112
    sections = []
    for i in range(nsec):
        s = opt + optsize + 40 * i
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, s + 8)
        sections.append((va, vsize, raw, rsize))
    names = []
    imp_rva = struct.unpack_from("<I", data, dirs + 8 * 1)[0]
    off = rva_to_offset(sections, imp_rva) if imp_rva else None
    while off is not None:
        desc = struct.unpack_from("<IIIII", data, off)
        if desc == (0, 0, 0, 0, 0):
            break
        names.append(read_cstr(data, rva_to_offset(sections, desc[3])))
        off += 20
    dly_rva = struct.unpack_from("<I", data, dirs + 8 * 13)[0]
    off = rva_to_offset(sections, dly_rva) if dly_rva else None
    while off is not None:
        name_rva = struct.unpack_from("<II", data, off)[1]
        if name_rva == 0:
            break
        names.append(read_cstr(data, rva_to_offset(sections, name_rva)) + " (delay)")
        off += 32
    return names


def classify(name, present, system_dirs):
    n = name.replace(" (delay)", "").lower()
    if n in present:
        return "package"
    if n.startswith(("api-ms-win-", "ext-ms-win-")):
        return "apiset"
    if not n.startswith(REDISTRIBUTABLE):
        for d in system_dirs:
            if os.path.isfile(os.path.join(d, n)):
                return "system"
    return "FOREIGN"


def selftest(system_dirs):
    present = {"zengine-workshop.exe", "sdl3.dll"}
    cases = [("SDL3.dll", "package"), ("api-ms-win-crt-runtime-l1-1-0.dll", "apiset"),
             ("libstdc++-6.dll", "FOREIGN"), ("VCRUNTIME140.dll", "FOREIGN"),
             ("no-such-library-zz.dll", "FOREIGN"), ("d3dcompiler_43.dll", "FOREIGN")]
    for name in ("KERNEL32.dll", "d3dcompiler_47.dll"):
        if any(os.path.isfile(os.path.join(d, name.lower())) for d in system_dirs):
            cases.append((name, "system"))
    for name, want in cases:
        if classify(name, present, system_dirs) != want:
            print("pe_imports: its own samples misread %s" % name)
            sys.exit(2)


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if not argv:
        print(__doc__)
        return 2
    folder = argv[0]
    system_dirs = [argv[i + 1] for i, a in enumerate(argv) if a == "--system" and i + 1 < len(argv)] or \
        [os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")]
    selftest(system_dirs)
    images, present = [], set()
    for d, _, files in os.walk(folder):
        for f in files:
            present.add(f.lower())
            if f.lower().endswith((".exe", ".dll")):
                images.append(os.path.join(d, f))
    if not images:
        print("pe_imports: no .exe or .dll under %s: an empty population is red" % folder)
        return 2
    foreign, tally = 0, {}
    for path in sorted(images):
        with open(path, "rb") as fh:
            data = fh.read()
        try:
            names = imports(data)
        except (ValueError, struct.error) as e:
            print("UNREAD   %s: %s" % (os.path.relpath(path, folder), e))
            foreign += 1
            continue
        verdicts = [(n, classify(n, present, system_dirs)) for n in names]
        bad = [n for n, v in verdicts if v == "FOREIGN"]
        foreign += len(bad)
        for _, v in verdicts:
            tally[v] = tally.get(v, 0) + 1
        print("%-8s %s: %s" % ("FOREIGN" if bad else "ok", os.path.relpath(path, folder),
                               ", ".join("%s[%s]" % nv for nv in verdicts)))
    print("pe_imports: %d image(s), imports by kind %s, %d FOREIGN" % (len(images), tally, foreign))
    return 1 if foreign else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
