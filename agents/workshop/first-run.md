# Workshop law — the quick setup

Register `WL-FIRST`: the quick setup a weaver's first start shows, what it keeps and where each of
its answers goes. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).
Office-wide settings are [`settings.md`](settings.md); the editor switch is
[`editor-switch.md`](editor-switch.md).

## WL-FIRST-01 — The quick setup shows once, at a start that keeps a settings file

LAW — `zengine.first-run` keeps one office-wide setting, `answered`, in Workshop's settings file, and asks for its `setup` pane at a start whose settings are kept and not yet answered, once a run.

MEANS
- a run with no settings file (`--isolated` without `--prefs`), a file refused at launch and settings withheld show nothing by itself;
- clearing `answered` shows it at the next start.

DOES NOT MEAN
- that the weave keeps a file of its own: what it remembers is Workshop's to keep.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-FIRST-02 — Done and Skip write the answer, and the pane is put down either way

LAW — Done and Skip write `answered` through Workshop's settings, and the pane is put down when Workshop has answered the write, whether it took it or said why not.

MEANS
- a refused write is said in Workshop's words, and the setup still leaves the desk;
- the pane's lines and the item a press on each chooses are one pure reading (`lines_of`).

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-FIRST-03 — Edit text with Neovim is the editor switch's to perform

LAW — The box asks the editor switch for the plan's `neovim` or `standard` choice and shows the choice the switch says holds the office; the office's grant names the switch's asks to the switch alone.

MEANS
- a consent the switch asked is confirmed only when the box is chosen again;
- the box is read from the switch's status at every activation and every opening;
- the switch's refusal is shown in its words, and the box follows the switch, never the ask.

DOES NOT MEAN
- that the quick setup keeps a second record of the editor: the session remembers it (WL-SWITCH-08).

WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## Proven by

### WL-FIRST-01
`workshop/first-run/first_run.cpp` FirstRun keeps_settings on(OfficeSettingsShown)
`workshop/first-run/setup.hpp` kAnsweredKey
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-01: the quick setup shows itself once at a start
that keeps settings and has not answered, and never where none are kept"

### WL-FIRST-02
`workshop/first-run/first_run.cpp` choose on(PaneSettingActed)
`workshop/first-run/setup.hpp` lines_of stepped
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-02: Done writes answered and puts the pane down,
and the next start shows nothing" "WL-FIRST-02: the quick setup's lines name the box, the pages and
Done and Skip, each press choosing its own item"

### WL-FIRST-03
`workshop/first-run/first_run.cpp` on(EditorSwitchAnswered) send_switch
`workshop/admission.hpp` first_run_office_grant
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-03: the Neovim box asks the editor switch, shows
its answer in its words, and confirms a consent only when chosen again"
