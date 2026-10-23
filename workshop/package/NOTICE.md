# What this folder holds, and under which terms

Zengine and the Loom are MPL-2.0 (`LICENSE`, one text for both). Their source is at
https://github.com/Krealsion/Zengine and https://github.com/Krealsion/Loom, at the commits
`BUILD-INFO.txt` names.

| file | holds | license | text |
|---|---|---|---|
| `bin/zengine-*`, `bin/loom-*`, `lib/` | Zengine; the Loom, linked into each | MPL-2.0 | `LICENSE` |
| `bin/zengine-window-sdl*` | the JetBrains Mono typeface, which stays under the SIL Open Font License 1.1 and is not MPL | SIL Open Font License 1.1 | `licenses/jetbrains-mono/OFL.txt` |
| `bin/SDL3*` (not `SDL3_ttf`) | SDL 3 | zlib | `licenses/SDL3/` |
| `bin/SDL3_ttf*` | SDL_ttf 3; FreeType, taken under the FreeType License | zlib, FTL | `licenses/SDL3_ttf/`, `licenses/FreeType/` |
| every Windows image | the mingw-w64 runtime; winpthreads; the GCC runtime libraries (the GNU GPL version 3 with the GCC Runtime Library Exception) | as each text says | `licenses/mingw-w64/`, `licenses/winpthreads/`, `licenses/gcc/` |

Portions of this software are copyright (c) 2026 The FreeType Project (https://freetype.org). All
rights reserved.
