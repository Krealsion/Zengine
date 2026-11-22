# The bus tally

**Where a running bus spends its time: how long each weave's handlers took, and how long each
input took to reach the screen, read off the times Loom stamps on every delivery.**

A host makes one beside its bus and prints what it found as it ends. The tally is an observer the
host adds to its bus: it changes nothing it observes, and no weave reads it.

It measures two things:

- **Handler time.** For each weave, how many deliveries its handlers took, the time they took in
  all, and its slowest delivery with the shape it was. A weave is named by the roles it held when
  the tally first saw it, or by its weave id when it held none.
- **An input's way to its paint.** A key, typed text, a press or a turn of the wheel, whether a
  window gave it to an area or the Input role published it, marks every weave the messages it
  caused reach, however many turns later. The next drawing such a weave sends that the window
  paints is the input's paint, and the tally keeps the time from the start of the input's delivery
  to the end of that drawing's, by kind of input and by the weave that drew. An input whose weaves
  paint nothing within a second is counted unpainted instead.

A program includes `tally/bus_tally.hpp` and links `zengine-tally`.

## Parts

| file | what it holds |
|---|---|
| `bus_tally.hpp` | `zengine::tally::BusTally`, the tally, and the rows it reports, `HandlerRow` and `PaintRow`; installed |
| `CMakeLists.txt` | `zengine-tally`, a header-only target over `zengine::graphics`, `zengine::input` and `loom::switchboard`, whose message names it reads |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`tally/` adds to it.

**Law.** The folder has no law of its own: each rule it keeps is about one declaration, in that
declaration's comment.

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `bus_tally` | a slow handler shows in the handler times, named by its role; a key reaches its paint when the weave it reached draws, turns after the key | `tests/test_bus_tally.cpp` |

**The host's side.** Workshop (`workshop/workshop.cpp`), the app (`app/main.cpp`) and the snake
example (`examples/snake/play.cpp`) each make a tally beside their bus and print its `lines()` as
they end. `cmake/ZengineInstall.cmake` installs the header.
