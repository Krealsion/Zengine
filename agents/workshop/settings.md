# Workshop law — a pane's settings

Register `WL-SETTING`: a setting's value and its kinds, the settings a layout's row keeps, a
pane's declaration and the hand-off of its row's settings.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-SETTING-01 — A setting is a key and one value of three kinds

LAW — A setting is a key and exactly one value, a flag, a number or a text, each in its own field; one holding no value or two is refused by its key wherever it is read, and nothing converts it.

MEANS
- the three kinds are settled: a fourth moves every shape that carries a setting, files included;
- a key is `a`-`z`, `0`-`9`, `.` and `-`; a text is printable ASCII, which every pane can draw;
- a number is never spelled as text, nor a flag: each kind travels and is saved in its own field.

PROVEN BY — `workshop/pane_settings.hpp` `PaneSetting`, `setting_kind`, `pane_setting_problem`,
`pane_setting_key_problem`, `pane_setting_text_ok`, `kMaxPaneSettingKeyLen`,
`kMaxPaneSettingTextLen`; `workshop/setup_persist.hpp` `setup_in`;
`tests/test_workshop_persistence.cpp` case `"a setting rides its row through the setup file in
its own kind's field"`, case `"a setting holding no value, or two, is refused by every reader,
naming its key"`; `tests/test_workshop_demo.cpp` case `"an agent's desk holding a setting with no
value is refused by its key, and changes nothing"`.
WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-02 — A layout's row keeps its pane's settings, judged by form alone

LAW — A setup row keeps its pane's settings in key order, once each, at most `kMaxPaneSettingsPerRow` of them and a desk `kMaxPaneSettingsPerDesk`; a key under `workshop.` is refused by name.

MEANS
- form alone: an unresolved reference keeps its settings byte for byte, whatever they mean;
- the desk's bound keeps a session at every bound inside Loom's decode budget (`kMaxDecodedCells`);
- `workshop.` keys are Workshop's own and this Workshop knows none, so a newer file is refused.

DOES NOT MEAN
- that a refused file is half read: it is refused whole, and `r` leaves the file and the desk.

PROVEN BY — `workshop/setup.hpp` `check_pane_settings`, `kMaxPaneSettingsPerDesk`,
`desk_setting_count`, `check_setup`, `SetupPane::settings`; `workshop/pane_settings.hpp`
`kMaxPaneSettingsPerRow`, `kDeskSettingPrefix`, `is_desk_setting_key`;
`tests/test_workshop_persistence.cpp` case `"a setting holding no value, or two, is refused by
every reader, naming its key"`, case `"a setup holding a Workshop setting is refused by name, and
`r` leaves file and desk"`, case `"a desk at its pane bound, every key at its own bound, is a
setup file a launch reads"`, case `"a maximal legal session is still one this build can read
back"`.
WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-03 — A pane's declaration stands alone and counts while its sender holds the office

LAW — A pane's settings declaration is judged whole under the office stamp, refused aloud with the settings in force left standing, replaced by a later one, and counted only while its sender holds the office.

MEANS
- a pane this office never offered, or a setting whose default it does not take, is refused by name;
- nothing waits on a declaration: it seats no pane, and the hand-off is not held for it.

DOES NOT MEAN
- that a declaration is answered: the band says a refusal, and the declarer is told nothing.

PROVEN BY — `workshop/setup.hpp` `admit_pane_settings`; `workshop/weave_seam.cpp`
`on(PaneSettingsDeclared)`, `counted_settings`; `workshop/panes.hpp` `RuntimePane::settings_rows`;
`workshop/pane_settings.hpp` `PaneSettingsDeclared`, `PaneSettingRow`,
`pane_settings_declared_problem`, `pane_setting_row_problem`, `pane_setting_refused_by`;
`tests/test_workshop_panes_settings.cpp` case `"a declaration is judged whole under the office
stamp, refused aloud, and replaced"`, case `"a declaration counts only while the weave that sent it
holds the office"`, case `"what a declared setting takes is said in its own kind's words"`.
WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-04 — A seated pane is handed its row's settings before any room

LAW — A seated pane whose holder accepts `PaneSettings` is handed its row's settings, declared or not, at the top of the repaint before any room, whenever they differ from what that holder last heard.

MEANS
- a seat, a switch, a re-offer, a re-seat and a new holder each hand them when what is kept moved;
- never handed is not handed none: a reopened row's empty list is handed too;
- a holder without the door is handed nothing, and equal settings across a switch hand nothing.

PROVEN BY — `workshop/weave_external.cpp` `hand_settings`, `refresh_external_rooms`;
`workshop/panes.hpp` `ExternalPane::settings_heard`; `workshop/weave_seam.cpp`
`accept_pane_offer`; `workshop/grant.cpp` `workshop_grant`; `workshop/pane_settings.hpp`
`PaneSettings`; `tests/test_workshop_panes_settings.cpp` case `"a pane's settings are handed
before its first room, whether or not it declared"`, case `"a holder that takes no settings is
handed none"`, case `"a layout switch hands a pane its new layout's settings once, and only when
they differ"`, case `"a re-offer and a reopened row are handed their settings again, before their
room"`.
WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`
