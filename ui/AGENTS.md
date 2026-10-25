# `ui/` — UI: working on it

The router for `ui/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | authored intent: the words, `Size`, `Place`, `Element` (v3), `kRootContext`, `ById` |
| `vocabulary.hpp` | following a context: `walk_context`; the intent's problems: `problem`; the fence: `authored_only_v` |
| `layout.hpp` | solved geometry: `Room`, `Measure`, `Rect`, `Placed`, `Scene` |
| `layout.hpp` | solving: `arrange`, `frame_in`; asking: `hit`, `placed_for` |
| `CMakeLists.txt` | `zengine-ui-vocabulary` (`zengine::ui`): headers over `loom::core`; no weave |
| `docs/ui.md` | the reference page |

## Law

No register of its own. Its law is its reference page,
[The UI package — authored and solved](docs/ui.md). Workshop's desk is solved by it: WL-PANE-03 in
`agents/workshop/panes-and-windows.md` (which `workshop/` routes) names `ui/layout.hpp`'s `arrange`.

## Suites

- `ui` — the authored/solved contract (shapes, the words, the fence traits, total solving, frames,
  columns, rounding to the step, chains, `problem`, hit testing): `tests/test_ui.cpp`, its own
  executable over `loom::core`
- `ui_authored_element_compiles` — the fence's positive control, a well-formed authored type
  builds: `tests/compile_negative/ui_fence.cpp` case 3
- `ui_authored_extent_required` — the type half refuses a bare-number `width` where a `Size` belongs:
  `tests/compile_negative/ui_fence.cpp` case 1
- `ui_resolved_geometry_refused` — the name half refuses a cached `w`/`h` beside honest extents:
  `tests/compile_negative/ui_fence.cpp` case 2

The three fixtures are `zengine-cn-ui-*` in `tests/compile_negative/CMakeLists.txt`. In a shared
suite: `workshop_shapes` (`tests/test_shapes_census.cpp`, through `tests/census_headers.hpp`)
counts `Element`, `Place` and `Size`. Outside CTest, `tests/package/run.cmake` builds
`tests/package/public_surface.cpp` against the installed headers.

## Host side

- `workshop/CMakeLists.txt` links `zengine-ui-vocabulary` into `zengine-workshop-vocabulary`
  and the `zengine-workshop` host.
- `workshop/screen_chrome.cpp` solves the desk with `arrange`: `bounds_of` stands each open pane in
  its place's frame.
- `cmake/ZengineInstall.cmake` installs `vocabulary.hpp` and `layout.hpp` and exports
  `zengine::ui`.

## Pages

- [The UI package — authored and resolved](docs/ui.md)

## Practices

- [UI](../docs/contributing/best-practices.md#ui-ui)
