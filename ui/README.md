# UI

**UI keeps where a person put something on screen apart from where it lands on a particular
screen. A program stores each part's place and size as they were authored, in words such as "half
the width", "its own size", "what is left" or "the bottom-right corner, 10 pixels in", then solves
them for a screen's size to get the rectangles to draw, in whole pixels, and to learn which part
is under a given pixel.**

A part can stand in another part instead of in the whole screen, so moving one moves everything
standing in it, and a part can lay the parts standing in it down a column. Solving gives an answer
for any values, a terminal solving the same words at the size of its cells; its result is never
stored back on what was authored, and what no screen could solve -- a size that depends on itself
-- is found from the words alone. A compile-time fence keeps a solved number out of an authored
type. It is header-only: a program links `zengine::ui` and writes `#include "ui/vocabulary.hpp"`
for the authored side and `#include "ui/layout.hpp"` for the solved side.

## Pages

- [The UI package — authored and solved](docs/ui.md): the reference — parts, their sizes and
  places in words, frames and columns, how they solve, what is under a pixel, what is wrong with
  the words alone, and the compile-time check that keeps a solved number out of an authored type

## Parts

| file | what it holds |
|---|---|
| `vocabulary.hpp` | what a person authors: the words (`Size`, `Place`, `size_word`, `place_word`), a part (`Element`, version 3), the root context `kRootContext`, finding and following a part's frame (`ById`, `walk_context`), what is wrong with the intent alone (`problem`), and the fence (`authored_only_v`); installed |
| `layout.hpp` | what solving makes of it: the bounds (`Room`), a part's own measure (`Measure`), the solved rectangles (`Rect`, `Placed`, `Scene`), the solve (`arrange`, `frame_in`) and what is under a pixel (`hit`, `placed_for`); installed |
| `CMakeLists.txt` | `zengine-ui-vocabulary`, a header-only target over `loom::core`, aliased and exported as `zengine::ui`; no weave |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`ui/` adds to it.

**Law.** The folder has no law page of its own: its contract is [the reference](docs/ui.md), and a
rule about one declaration is in that declaration's comment.

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `ui` | the authored and solved contract: the shapes, the words, the fence's traits, solving total over every value, frames, columns, rounding to the step, chains, `problem` and hit testing | `tests/test_ui.cpp`, its own program over `loom::core` |
| `ui_authored_element_compiles` | the fence's control: a well-formed authored type compiles | `tests/compile_negative/ui_fence.cpp`, case 3 |
| `ui_authored_extent_required` | the type half refuses a bare number where a `Size` belongs | the same file, case 1 |
| `ui_resolved_geometry_refused` | the name half refuses a cached `w` or `h` beside honest sizes | the same file, case 2 |

The three compile cases' targets are `zengine-cn-ui-*` in `tests/compile_negative/CMakeLists.txt`.
A shared suite counts the folder's shapes too: `workshop_shapes` (`tests/test_shapes_census.cpp`,
through `tests/census_headers.hpp`) counts `Element`, `Place` and `Size`. Outside CTest,
`tests/package/run.cmake` builds `tests/package/public_surface.cpp` against the installed headers.
Solving's arithmetic runs the sanitizer lane too.

**The host's side.** Workshop solves its desk with it: `workshop/screen_chrome.cpp` calls `arrange`,
and `bounds_of` stands each open pane in its place's frame. `workshop/CMakeLists.txt` links
`zengine-ui-vocabulary` into `zengine-workshop-vocabulary` and the `zengine-workshop` host, and
`cmake/ZengineInstall.cmake` installs both headers and exports `zengine::ui`.

**Practice.** [UI](../docs/contributing/best-practices.md#ui-ui).
