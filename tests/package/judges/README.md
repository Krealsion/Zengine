# The package's judges

Four judges of a package folder, the archive a stranger downloads once it is extracted. Each is a
Python script with the standard library alone. Each reads its own samples before it answers,
and is red on an empty population.

| judge | red when | run |
|---|---|---|
| [`pe_imports.py`](pe_imports.py) | a Windows image imports a DLL that is neither in the package, an API set, nor Windows' own | `python -I pe_imports.py <folder>` |
| [`elf_judge.py`](elf_judge.py) | a Linux image needs a library outside the package, glibc and the system's C++ runtime, or its RUNPATH is not `$ORIGIN` | `python3 -I elf_judge.py <folder> --allow libstdc++.so.6 --allow libgcc_s.so.1` |
| [`private_scan.py`](private_scan.py) | a file names the machine that built it, in UTF-8 or UTF-16 | `python -I private_scan.py <folder> <needle>...` |
| [`first_start.py`](first_start.py) | a program started with nothing installed beside it ends at once, loads a module from outside the package and the system, prints a needle, or does not say a text expected of it | `python -I first_start.py <folder> <program> --env SDL_VIDEO_DRIVER=offscreen [--needle <text>]... [--expect <text>]... -- <arguments>` |

A needle names the build machine: its build and source folders, its toolchain, its user. It never
names the folder the package was extracted into. Workshop's banner names its own project, so that
folder would always be found.

`test_judges.py`, the CTest entry `package_judges` (the `session` gate, where a Python is found),
holds each judge to its word. Each judge must be red on an empty population, red on its canary
and green on a clean sample:
- `private_scan`: a planted needle, UTF-8 and UTF-16;
- `pe_imports`: a PE32+ image made there that imports `libstdc++-6.dll`;
- `elf_judge`: a copy of `ls`;
- `first_start`: a program that ends at once, and one that does not say what is expected of it.
