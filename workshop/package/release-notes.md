<!-- The notes a night's release is published with: each @NAME@ is that night's own value. -->
Zengine @VERSION@ -- Workshop, ready to run. An alpha: interfaces change without deprecation
cycles. Made from `main` at @COMMIT@, with the Loom at @LOOM@, the night of @DATE@; it replaces
the package before it, whose tag stays.

## Download and run

| platform | file | start |
|---|---|---|
| Windows 10 or 11, x64 | `zengine-@VERSION@-windows-x64.zip` | extract it anywhere; double-click `Workshop.cmd` (a window) or `Workshop-terminal.cmd` (this console) |
| Linux x86-64, glibc 2.35 or newer | `zengine-@VERSION@-linux-x86_64.tar.gz` | `tar -xzf` it anywhere; `./workshop.sh` (a window) or `./workshop-terminal.sh` (this terminal) |

What it needs that it does not carry -- Python 3.8 or newer for the ready-to-use setups, and on
Linux its C++ runtime and a desktop for the window -- is on [how to install these
dependencies](https://github.com/Krealsion/Zengine/blob/main/workshop/docs/dependencies.md).
Your project is a folder of your own, made at the first start and kept across packages:
`%USERPROFILE%\Zengine` on Windows, `~/Zengine` on Linux. Both launchers share it; a `terminal`
folder inside it, which earlier packages' terminal launchers used as their project, is left as it
is, and no launcher reads it. Where no window can open, the window's launcher goes on
in the terminal it was started from and says why. `README.txt` in the package says where
everything goes; `bin/zengine-workshop --version` names this package's commits.

**Windows says the file is from an unknown publisher.** The package is not signed. SmartScreen may
stop `Workshop.cmd` the first time: choose **More info**, then **Run anyway**, once you have
checked the download as below.

## Check what you downloaded

The sums of both archives are in `SHA256SUMS`, beside them:

```sh
sha256sum --check --ignore-missing SHA256SUMS
```

```powershell
(Get-FileHash zengine-@VERSION@-windows-x64.zip -Algorithm SHA256).Hash   # compare with SHA256SUMS
```

Each archive and `SHA256SUMS` carry a build provenance attestation: GitHub's record that this
repository's workflow made them from @COMMIT@. With the GitHub CLI:

```sh
gh attestation verify zengine-@VERSION@-windows-x64.zip --repo Krealsion/Zengine
```

## How it was made

- Windows: WinLibs GCC @WINDOWS_GCC@ (UCRT, posix threads, SEH), Release, every image linking its C
  and C++ runtime statically, stripped.
- Linux: GCC 11.4 in Ubuntu 22.04, its packages from Ubuntu's archive as it stood on
  @APT_SNAPSHOT@; Release, stripped. It needs the system's C++ runtime (`libstdc++.so.6`).
- Both: SDL 3, SDL_ttf 3 and FreeType built from the archives Zengine's CMake pins; judged before
  publishing: imports, the libraries each image needs, no path of the machine that built it, and
  Workshop started from the extracted folder with nothing installed beside it.

What does not work yet: `workshop/docs/limitations.md` in the source at @COMMIT@.
