# How to install these dependencies

Workshop's package carries everything it runs but what this page lists. Each says what needs it,
how to see whether you have it, and how to install it. The package's `README.txt` links each one
here.

## Python 3.8 or newer

**Needed for the ready-to-use setups.** The quick setup at the first start, and the palette
(`ctrl+space`), list setups that each start in a Workshop of their own; starting one runs Python.
Without it the setups are still listed, and a start says what is missing. Nothing else in the
package needs Python.

- **Windows**: install it from [python.org](https://www.python.org/downloads/windows/). Its `py`
  launcher is enough; adding Python to `PATH` is not needed. Check it in a new console with
  `py -3 --version`.
- **Linux**: the system's `python3`. Most desktops have it; check with `python3 --version`. On
  Debian or Ubuntu `sudo apt install python3`, on Fedora `sudo dnf install python3`.

## Linux: the C++ runtime and glibc 2.35 or newer

**Needed by every program in the Linux package**, which is built on Ubuntu 22.04. Ubuntu 22.04,
Debian 12, Fedora 36 and their later releases have both.

- Check glibc with `ldd --version`: its first line ends in the version.
- The C++ runtime is `libstdc++`, part of nearly every desktop. On Debian or Ubuntu
  `sudo apt install libstdc++6`, on Fedora `sudo dnf install libstdc++`.

An older glibc cannot be installed beside the system's own: a system older than these needs a newer
release.

## Linux: a desktop with X11 or Wayland, for the window

**Needed by `./workshop.sh`**, which opens Workshop in a window. A desktop session gives you one.
Where none is running (a server, a console login, a container), `./workshop.sh` goes on in the
terminal it was started from and says why once there; `./workshop-terminal.sh` needs no display at
all.

Windows 10 and 11 need nothing more: the package's window and console launchers run as they are.
