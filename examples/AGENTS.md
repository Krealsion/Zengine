# `examples/` — Examples: working on it

The router for `examples/`, the worked examples: where their law, their suites, the host's side of
them and their practices are. It states no law. The repository's own rules are the root
[AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `snake/` | the snake game, built only where `loom::kernel` exists |
| `snake/world.cpp` | loadable weaves `snake-world-v1` and `snake-world-v2` (`SNAKE_WORLD_V2`) |
| `snake/score.cpp` | loadable weave `snake-score`; `controls.cpp`: `snake-controls` |
| `snake/clock.cpp` | loadable weave `snake-clock` |
| `snake/play.cpp` | executable `zengine-snake`, the playable host; state: `play_state.hpp` |
| `snake/vocabulary.hpp` | snake's shapes and role `snake.world`; `logic.hpp`: the pure simulation |
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
  swaps through the real kernel, zero stdout bytes without a Skin
  (`tests/test_snake.cpp`; it reads `play.cpp` and `clock.cpp` as text)
- `story_journey` — `tower-defense/story.py`'s custody of the Workshop and Loom-host
  processes it starts (`tests/session/story_journey.py`; the `session` gate)
- `workshop_probe` — the probe weave on one bus behind a scripted link: when it may say
  PASS, what moves it, what it closes (`tests/test_workshop_probe.cpp`)

Its cases in shared suites: `workshop_panes` loads Tally, Guard and the numbered presenter
(`tests/test_workshop_panes_button.cpp`, `_code.cpp`, `_desktop.cpp`, `_introspection.cpp`);
snake's weaves ride `input` (`tests/test_input.cpp`), `timer` (`tests/test_timer.cpp`),
`audit_probes` (`tests/test_audit_probes.cpp`) and `surface` (`tests/test_surface.cpp`);
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
- `surface/skin.hpp` and `surface/skin_sdl_plan.hpp` include `examples/snake/vocabulary.hpp`
  (the Skins accept `SnakeVisual`); `cmake/ZengineInstall.cmake` installs nothing from here.

## Pages

- [snake — a worked example](snake/README.md)
- [Tower Defense, made from inside Workshop](tower-defense/README.md)
- Walkthroughs: [Tally](../builder/docs/edit-a-running-pane.md),
  [Guard and the numbered presenter](../workshop/docs/panes.md#the-second-button--the-panes-first),
  [the probe](../external-host/docs/external-host.md),
  [the recipe fixture](../external-host/docs/elh-recipe-journey.md)

## Practices

- [Snake and smoke](../docs/contributing/best-practices.md#snake-and-smoke-examplessnake-testssmoke)
- [Examples](../docs/contributing/best-practices.md#examples-examples)
