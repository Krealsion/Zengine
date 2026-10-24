# The tally app

An app's folder: the tally panel -- three number fields, a Count button and the total -- and the
tally flow that counts, run by `zengine-app` with no Workshop running
([running an app on its own](https://github.com/Krealsion/Zengine/blob/main/app/docs/app.md)).
Its plan is the one the View Builder's Export app writes, byte for byte; its `app.json` is written
by hand.

| file | what it is |
|---|---|
| `app-plan.json` | the plan: the basic operators, the app's weave, the graphics directory, the OS window and the Timer |
| `app.json` | the app: its name, the `day` skin, the panel at (8, 8) and the flow |
| `tally.view` | the panel, as the View Builder saves it |
| `tally.flow` | the flow `tally`, folding `math.add`, its total starting at 0, as Flow's Export writes it |
| `tally.hpp` | the panel and the flow as values: the view suite's tally panel and the maker suites' tally |
| `fixture.cpp` | `zengine-tally-app-fixture <folder>`: writes `tally.view` and `tally.flow` from them |

## Run it

A build stages a copy of this folder beside `zengine-app`, in `app/apps/tally/` of the build tree,
with two launchers: `run.cmd` starts the app in a console of its own on Windows, and `run.sh` in
the terminal it is run from. Its plan's window is the OS window, which every build but a
`ZENGINE_TUI_ONLY` one builds; without it, the console says the window's artifact cannot be read.

On its first start the console asks about each of the five artifacts in turn; answer each with the
line it gives, `authority trust <artifact>`. Then the window shows the panel:

- **Count** as it starts says `Total: 45`: 0 to 10 by 1.
- **Tab** to a field, **Backspace** and type: `10`, `0` and `-2`, then **Count**, says `Total: 30`.
- `0`, `10` and `0` says the fold's refusal on the panel's notice rows: a step of 0 never moves.
- Closing the window ends the app. A second start asks nothing while the builds stand.

## Export it from Workshop

The folder's `setup.json` is a setup
([demo setups](https://github.com/Krealsion/Zengine/blob/main/workshop/docs/demo-setups.md)): a
project holding `tally.view` and `tally.flow`, and a walk, `export`, that opens Flow and the View Builder,
opens `tally.view` and presses **Export app**. Started from a package with `--package`, it is the
package's story, walked each night: `tests/package/story.py` checks the exported launcher names the
package's `zengine-app`, answers the app's console, and quits Workshop with the app still running.

## Write the view and the flow again

```sh
zengine-tally-app-fixture examples/tally-app
```

The `app_host` suite reads the committed files against what the tool writes, byte for byte, so a
change to the panel, the flow or their formats is written here again in the same change.
