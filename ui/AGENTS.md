# `ui/` — UI: working on it

The router for `ui/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `vocabulary.hpp` | authored intent: `Extent`, `Element` (v2), `kRootContext`, `ById` |
| `vocabulary.hpp` | following a context: `walk_context`; the fence: `authored_only_v` |
| `layout.hpp` | resolved geometry: `Viewport`, `Rect`, `Placed`, `Scene`, `kMinCells` |
| `layout.hpp` | resolving: `resolve_extent`, `resolve`, `frame_in`; asking: `hit`, `placed_for` |
| `CMakeLists.txt` | `zengine-ui-vocabulary` (`zengine::ui`): headers over `loom::core`; no weave |
| `docs/ui.md` | the reference page |

## Law

No register of its own. Its law is its reference page,
[The UI package — authored and resolved](docs/ui.md). Workshop's whole-pixel law, WL-GEO-06 in
`agents/workshop/geometry.md` (which `workshop/` routes), names `ui/layout.hpp`'s `Rect`.

## Suites

- `ui` — the authored/resolved contract (shapes, the fence traits, total resolution, frames,
  chains, hit testing): `tests/test_ui.cpp`, its own executable over `loom::core`
- `ui_authored_element_compiles` — the fence's positive control, a well-formed authored type
  builds: `tests/compile_negative/ui_fence.cpp` case 3
- `ui_authored_extent_required` — the type half refuses a bare-number `width`:
  `tests/compile_negative/ui_fence.cpp` case 1
- `ui_resolved_geometry_refused` — the name half refuses a cached `w`/`h` beside honest extents:
  `tests/compile_negative/ui_fence.cpp` case 2

The three fixtures are `zengine-cn-ui-*` in `tests/compile_negative/CMakeLists.txt`. In a shared
suite: `workshop_shapes` (`tests/test_shapes_census.cpp`, through `tests/census_headers.hpp`)
counts `Element` and `Extent`. Workshop's suites read cell rectangles as `ui::Rect`
(`tests/workshop_support.hpp`). Outside CTest, `tests/package/run.cmake` builds
`tests/package/public_surface.cpp` against the installed headers.

## Host side

- `workshop/CMakeLists.txt` links `zengine-ui-vocabulary` into `zengine-workshop-vocabulary`
  and the `zengine-workshop` host.
- `workshop/screen.hpp` turns a cell `ui::Rect` into a `PixelRect` and back (`pixels_of_cells`,
  `cells_covered`); `workshop/setup.hpp` builds `kPanePxMin` from `ui::kMinCells`.
- `cmake/ZengineInstall.cmake` installs `vocabulary.hpp` and `layout.hpp` and exports
  `zengine::ui`.

## Pages

- [The UI package — authored and resolved](docs/ui.md)

## Practices

- [UI](../docs/contributing/best-practices.md#ui-ui)
