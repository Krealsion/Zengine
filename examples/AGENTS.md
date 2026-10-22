# `examples/` — Examples: working on it

The router for `examples/`, the worked examples: where their law, their suites, the host's side of
them and their practices are. It states no law. The repository's own rules are the root
[AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `snake/` | the snake game, built only where `loom::kernel` exists |
| `snake/world.cpp` | loadable weaves `snake-world-v1` and `snake-world-v2` (`SNAKE_WORLD_V2`) |
| `snake/board.cpp` | element code `snake-board` (a provider library), presenting `board.hpp`'s `SnakeBoardData` |
| `snake/score.cpp` | loadable weave `snake-score`, role `snake.score` |
| `snake/play.cpp` | executable `zengine-snake`, the playable host; state: `play_state.hpp` |
| `snake/vocabulary.hpp` | snake's shapes and role `snake.world`; `logic.hpp`: the pure simulation |
| `tally-app/` | an app's folder `zengine-app` runs; `fixture.cpp`, `zengine-tally-app-fixture`, writes its view and flow from `tally.hpp` |
| `tally-pane/tally.cpp` | loadable weave `zengine-example-tally`, role `example.tally` |
| `guard-pane/guard.cpp` | loadable weave `zengine-example-guard`, role `example.guard` |
| `numbered-presenter/presenter.cpp` | loadable weave `zengine-example-numbered-presenter` |
| `tower-defense/` | `td.cpp` (role `td.game`), `story.py`, `story/`, setups, `monitor/`, toolbox |
| `workshop-probe/` | own CMake project: loadable weave `zengine-workshop-probe` (`probe.hpp`) |
| `workshop-recipe-fixture/` | own CMake project declaring `demo_target`, for the recipe journey |

## Law

No register of its own. The pane examples keep the pane seam's law, `agents/panes.md`, which the
root routes; the presenter's half of a pane's menu is `agents/workshop/pane-menu.md`, which
[Workshop's router](../workshop/AGENTS.md) routes.

## Suites

- `snake` — the game headless: the content-id contract, simulation and migration as math, live
  swaps through the real kernel, a skin chosen mid-game, the world's doors beside its own keys
  and beat, zero stdout bytes with no terminal (`tests/test_snake.cpp`; it reads `play.cpp` and
  `world.cpp` as text)
- `story_journey` — `tower-defense/story.py`'s custody of the Workshop and Loom-host
  processes it starts (`tests/session/story_journey.py`; the `session` gate)
- `workshop_probe` — the probe weave on one bus behind a scripted link: when it may say
  PASS, what moves it, what it closes (`tests/test_workshop_probe.cpp`)

Its cases in shared suites: `workshop_panes_seam`, `workshop_panes_project`, `workshop_panes_hand`
and `workshop_panes_inspect` load Tally, Guard and the numbered presenter
(`tests/test_workshop_panes_button.cpp`, `_code.cpp`, `_desktop.cpp`, `_introspection.cpp`);
snake's weaves ride `graphics` (`tests/test_graphics.cpp`), `input` (`tests/test_input.cpp`),
`timer` (`tests/test_timer.cpp`), `audit_probes` (`tests/test_audit_probes.cpp`) and `surface`
(`tests/test_surface.cpp`);
`neovim_journey` (`tests/session/neovim_journey.py`) also runs `story.py`. The installed-package
witness, `tests/package/run.cmake` (not a CTest entry), builds `workshop-probe/` as a stranger.

## Host side

- Built: `snake/` by the root `CMakeLists.txt`; Tally, Guard and the numbered presenter by
  `tests/CMakeLists.txt`.
- The pane seam: `workshop/pane_vocabulary.hpp`; Guard also `workshop/pane_menu.hpp`; the
  numbered presenter `workshop/presenter_vocabulary.hpp`, loaded under `zengine.presenter` in
  place of `workshop/menu-presenter/`.
- The probe's seam vocabulary: `workshop/guest_seam_vocabulary.hpp`; the guest door:
  `workshop/guest_door.hpp`.
- `external-host/demo.py` reads `tower-defense/setup.json` (`workshop/docs/demo-setups.md`).
- `tally-app/` is built by the root `CMakeLists.txt`, after `app/`, and staged beside `zengine-app`
  with its launchers; the `app_host` suite (`tests/test_app_host.cpp`, `app/`'s router) reads it.
- The snake draws through `graphics/`: `snake/play.cpp` loads the directory and the terminal
  window and arranges them; `cmake/ZengineInstall.cmake` installs nothing from here.

## Pages

- [snake — a worked example](snake/README.md)
- [Tower Defense, made from inside Workshop](tower-defense/README.md)
- [The tally app](tally-app/README.md)
- Walkthroughs: [Tally](../builder/docs/edit-a-running-pane.md),
  [Guard and the numbered presenter](../workshop/docs/panes.md#the-second-button--the-panes-first),
  [the probe](../external-host/docs/external-host.md),
  [the recipe fixture](../external-host/docs/elh-recipe-journey.md)

## Practices

- [Snake and smoke](../docs/contributing/best-practices.md#snake-and-smoke-examplessnake-testssmoke)
- [Examples](../docs/contributing/best-practices.md#examples-examples)
