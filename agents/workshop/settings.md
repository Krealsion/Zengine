# Workshop law — a pane's settings

Register `WL-SETTING`: a setting's value and its kinds, the settings a layout's row keeps, a
pane's declaration and the hand-off of its row's settings, the row read and written by message, an
office's office-wide settings, Workshop's settings file, and a pane's pin list. One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-SETTING-01 — A setting is a key and one value of three kinds

LAW — A setting is a key and exactly one value, a flag, a number or a text, each in its own field; one holding no value or two is refused by its key wherever it is read, and nothing converts it.

MEANS
- the three kinds are settled: a fourth moves every shape that carries a setting, files included;
- a key is `a`-`z`, `0`-`9`, `.` and `-`; a text is printable ASCII, which every pane can draw;
- a number is never spelled as text, nor a flag: each kind travels and is saved in its own field.

WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-02 — A layout's row keeps its pane's settings, judged by form alone

LAW — A setup row keeps its pane's settings in key order, once each, at most `kMaxPaneSettingsPerRow` of them and a desk `kMaxPaneSettingsPerDesk`; a key under `workshop.` is refused by name.

MEANS
- form alone: an unresolved reference keeps its settings byte for byte, whatever they mean;
- the desk's bound keeps a session at every bound inside Loom's decode budget (`kMaxDecodedCells`);
- `workshop.` keys are Workshop's own office-wide settings (WL-SETTING-10), never a row's, so a row holding one is refused.

DOES NOT MEAN
- that a refused file is half read: it is refused whole, and `r` leaves the file and the desk.

WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-03 — A pane's declaration stands alone and counts while the incarnation that sent it holds the office

LAW — A settings declaration, v1 or v2, is judged whole under the office stamp, refused aloud with the settings in force standing, replaced by a later one, and counted while its incarnation holds the office.

MEANS
- a pane this office never offered, a default a setting does not take, or a tag, pin or summary past its bound is refused by name;
- a reloaded image keeps the weave id but not the incarnation: one declaring nothing is shown none, and its re-offer drops the rows;
- nothing waits on a declaration: it seats no pane, and the hand-off is not held for it.

DOES NOT MEAN
- that a declaration is answered: the band says a refusal, and the declarer is told nothing;
- that Workshop acts on a tag, a pin or a summary: they say how a weaver finds a setting.

WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-04 — A seated pane is handed its row's settings before any room

LAW — A seated pane whose holder takes `PaneSettings`, declared or not, is handed its row's settings in the newest version it takes, before any room, when they or its revision differ from what it last heard.

MEANS
- a seat, a switch, a re-offer, a re-seat, a managed opening and a new holder each hand them when what is kept moved;
- never handed is not handed none: a reopened row's empty list is handed too;
- a holder without the door is handed nothing; equal settings across a switch hand a v1 holder nothing and a v2 holder its new revision.

DOES NOT MEAN
- that a managed opening's shown picture has them: it was prepared before the seat (WL-OPEN-10).

WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-05 — A setting is edited as a row of its pane's subject, against what it takes

LAW — A pane's subject has a row per declared setting and per kept key; a value its setting does not take is refused naming what it takes, its default is stored as absence, and `-` clears it.

MEANS
- a text is stored as typed, spaces and all; a row reads a value as typed, and one its setting does not take beside the default in effect;
- a kept key takes only `-`; the subject is named anew when its settings rows change;
- the band says the value was stored and handed, or why it was not handed.

DOES NOT MEAN
- that the desk acts on a value: it judges and keeps it, and what it means is the pane's.

WHY — `agents/decisions/settings-are-rows-of-a-panes-subject.md`

## WL-SETTING-06 — A close takes a row's settings and says so; an opening's publication keeps them

LAW — A close takes the row's settings with its place and size, and each close door names them; a managed opening's publication keeps the live desk's name and settings, which its digest leaves out.

MEANS
- a reopened row keeps none, and is handed that;
- a setting written or a layout renamed while an opening prepares neither aborts it nor is lost to it.

WHY — `agents/decisions/a-setting-is-a-typed-value-its-row-keeps.md`

## WL-SETTING-07 — A pane's row is read and written by message at its revision

LAW — `PaneSettingsRequested` reads a pane's row of the live layout and its revision; `PaneSettingCommitRequested` writes one setting only at that revision, through Info's setter, answered to the asker alone.

MEANS
- a revision is minted afresh when the desk put live, the row's settings, its counted declaration or the office's holder moved, and never again;
- refused in order with nothing written: the asker, the row, the revision, a lock, a clear given a value, a key no declaration takes, a value its setting does not take;
- a clear takes back a kept key, declared or not; clearing an absent key, or writing the value kept, mints nothing.

DOES NOT MEAN
- that `provider` and `pane` are authority: they address a row, and the asker's own grant decides.

WHY — `agents/decisions/a-settings-write-names-the-revision-it-read.md`

## WL-SETTING-08 — A pane says a setting it was handed and cannot use, against that revision

LAW — A holder's `PaneSettingUnused` is kept only for the revision it holds, one a key and at most `kMaxPaneSettingsPerRow`, said with the row and on Attention, until a later revision or a re-offer.

MEANS
- one naming another revision, a desk no longer live included, or said personally, is dropped;
- a key or a reason it cannot have meant is refused aloud, and nothing is kept.

WHY — `agents/decisions/a-settings-write-names-the-revision-it-read.md`

## WL-SETTING-09 — An office, the host's Terminal and a guest with `settings` read and write a row

LAW — The settings doors serve an office, the host's Terminal by `let_terminal_edit_settings`, and a guest whose row has `settings`; any other asker is answered withheld, with nothing read or written.

MEANS
- the host widens its Terminal to the asks, to Workshop's office alone, and to their answers: a row's read and commit, an office's read and commit, and a pin act;
- a guest's `settings` is a word of the guests file's version 3, granted the same five asks to Workshop.

DOES NOT MEAN
- that Loom's grant is the only wall: an ask a grant let through is judged again by the row.

WHY — `agents/decisions/a-settings-write-names-the-revision-it-read.md`

## WL-SETTING-10 — An office's office-wide settings are declared, read and written by message, Workshop's own in code

LAW — An office's `OfficeSettingsDeclared` is judged by a pane declaration's rules and counts while its incarnation holds the office; one office's settings are read, and written at the revision read.

MEANS
- Workshop's five `workshop.` settings are declared in code; an office declaring as Workshop is refused; at most `kMaxSettingOffices` offices, and `kMaxOfficeSettingsKept` settings;
- refused in order, nothing written: the asker, the office, the revision, an unreadable file, a clear given a value, a key not declared, a lock, a value not taken;
- a default is kept as its absence; Workshop applies its own, another office's holder is handed `OfficeSettings`; the answer says applied or handed, then written or why not.

DOES NOT MEAN
- that Workshop acts on another office's value: it judges and keeps it, and what it means is the office's.

WHY — `agents/decisions/office-wide-settings-live-in-workshops-settings-file.md`

## WL-SETTING-11 — Workshop's settings file is read once and written only while its bytes are this host's

LAW — Workshop's settings file, version 3, keeps offices' settings, pin lists and everywhere locks; read at start, it is written by its edit doors only while its bytes are the ones this host read or wrote.

MEANS
- versions 1 and 2 are read as Workshop's own settings; version 3 is judged whole, a key of Workshop's this build does not declare kept;
- a file this run could not read refuses every office commit and pin act and is never written; a toggle stands for the run;
- an everywhere lock refuses every write door naming its key: a row's commit, Info's, an office's.

DOES NOT MEAN
- that a lock is made or removed here: the file is written by hand, and the doors that lock are later.

WHY — `agents/decisions/office-wide-settings-live-in-workshops-settings-file.md`

## WL-SETTING-12 — A pane's pin list is edited by message in the list its scope reads

LAW — `PaneSettingPinRequested` pins, unpins, moves or resets a pane's pin list, at most `kMaxPinsPerPane`, in the list `workshop.pinned-scope` reads; it is answered the list, and never handed to the pane.

MEANS
- Workshop-wide, the list under `<office>/<pane>`; per layout, `<office>/<pane>@<layout>`, falling back to the Workshop-wide one, then the declared pins;
- the first act copies the list it read into the scope's own, and `reset` drops the scope's own;
- refused, nothing written: the asker, the layout, no declaration, an unreadable file, a key not declared, pinned already, not pinned, an unknown act, the bound.

DOES NOT MEAN
- that a per-layout list follows a renamed layout: it is keyed by the name it was made under.

WHY — `agents/decisions/a-setting-is-found-by-its-tags-and-pins.md`

## Proven by

### WL-SETTING-01
`workshop/pane_settings.hpp` PaneSetting setting_kind pane_setting_problem pane_setting_key_problem
pane_setting_text_ok kMaxPaneSettingKeyLen kMaxPaneSettingTextLen
`workshop/setup_persist.hpp` setup_in
`tests/test_workshop_persistence.cpp` "a setting rides its row through the setup file in its own
kind's field" "a setting holding no value, or two, is refused by every reader, naming its key"
`tests/test_workshop_demo.cpp` "an agent's desk holding a setting with no value is refused by its
key, and changes nothing"

### WL-SETTING-02
`workshop/setup.hpp` check_pane_settings kMaxPaneSettingsPerDesk desk_setting_count check_setup
SetupPane::settings
`workshop/pane_settings.hpp` kMaxPaneSettingsPerRow kDeskSettingPrefix is_desk_setting_key
`tests/test_workshop_persistence.cpp` "a setting holding no value, or two, is refused by every
reader, naming its key" "a setup holding a Workshop setting is refused by name, and `r` leaves file
and desk" "a desk at its pane bound, every key at its own bound, is a setup file a launch reads" "a
maximal legal session is still one this build can read back" "the settings door keeps a desk inside
its bound, and a value moved on a full desk is kept"

### WL-SETTING-03
`workshop/setup_reconcile.hpp` admit_pane_settings
`workshop/weave_seam.cpp` on(PaneSettingsDeclared) on(v2::PaneSettingsDeclared) keep_settings_rows
counted_settings counted_settings_v2 accept_pane_offer
`workshop/panes.hpp` RuntimePane::settings_rows RuntimePane::settings_rows_v2
RuntimePane::settings_incarnation
`workshop/pane_settings.hpp` PaneSettingsDeclared PaneSettingRow pane_settings_declared_problem
pane_setting_row_problem pane_setting_refused_by pane_setting_tag_problem widen_setting_row
narrow_setting_row kMaxTagLen kMaxSettingTags kMaxDeclaredTags kMaxDefaultPins kReservedTags
`tests/test_workshop_panes_settings.cpp` "a declaration is judged whole under the office stamp,
refused aloud, and replaced" "a declaration counts only while the weave that sent it holds the
office" "what a declared setting takes is said in its own kind's words" "a v2 declaration says how
each setting is found, and each of its bounds is refused by name" "a v1 declaration is read as v2
rows, untagged, unpinned and unsummarised" "a reloaded image that declares nothing shows none of its
predecessor's settings"
`tests/test_workshop_panes_legend.cpp` "the Terminal's settings rows appear the moment its image
declares, and an identical declaration keeps a draft"

### WL-SETTING-04
`workshop/weave_external.cpp` hand_settings refresh_external_rooms
`workshop/weave_managed.cpp` after_delivery
`workshop/panes.hpp` ExternalPane::settings_heard ExternalPane::settings_revision
`workshop/weave_seam.cpp` accept_pane_offer
`workshop/grant.cpp` workshop_grant
`workshop/pane_settings.hpp` PaneSettings
`tests/test_workshop_panes_settings.cpp` "a pane's settings are handed before its first room,
whether or not it declared" "a holder that takes no settings is handed none" "a layout switch hands
a pane its new layout's settings once, and only when they differ" "a re-offer and a reopened row are
handed their settings again, before their room" "a declaration counts only while the weave that sent
it holds the office" "a holder that takes v2 is handed its row's revision, moved by a write and by a
desk put live"
`tests/test_workshop_panes_opening.cpp` "a managed opening hands its pane's settings before the room
it seats it in" "a managed opening's owed rooms follow its pane's settings when the next delivery
repaints nothing"

### WL-SETTING-05
`workshop/setup.hpp` author_pane_setting
`workshop/screen.hpp` SettingsSubject
`workshop/screen_pane_subject.cpp` settings_subject write_pane_setting write_kept_setting
pane_setting_text kept_setting_text
`workshop/weave_inspection.cpp` settings_handed refresh_inspected
`tests/test_workshop_panes_settings.cpp` "a pane's settings are rows of its subject, named by their
keys, after RESOLVED" "a value a setting does not take is refused at the edit, naming what it takes"
"a setting typed at its default is stored as its absence, and `-` clears one" "a settings write says
it was stored and handed, or why it was not handed" "a text setting is stored as it is typed, spaces
included, and `-` clears it" "a kept setting takes only `-`, and the rows are named anew when it
goes" "the subject is named anew when the settings its rows are built from change"

### WL-SETTING-06
`workshop/setup_reconcile.hpp` discarded_settings settings_said
`workshop/weave_desktop.cpp` close_pane
`workshop/weave_managed.cpp` show_presentation digest_of
`tests/test_workshop_panes_settings.cpp` "a close discards a pane's settings with its row, and every
close door says which" "arrangement's remove says the settings it discarded too"
`tests/test_workshop_panes_opening.cpp` "a setting written while an opening prepares survives its
publication" "a layout renamed while an opening prepares keeps its name through the publication"

### WL-SETTING-07
`workshop/settings_seam_vocabulary.hpp` PaneSettingsRequested PaneSettingsShown
PaneSettingCommitRequested PaneSettingActed SettingLockShown
`workshop/weave_inspection.cpp` on(PaneSettingsRequested) on(PaneSettingCommitRequested)
settings_handed
`workshop/weave_seam.cpp` settings_revision
`tests/test_workshop_panes_settings.cpp` "a pane's row is read by message: its revision, what its
declaration takes, its pins and what it keeps" "a commit writes one setting at the revision it
read, hands it, and says how far it got" "a commit is refused in words with nothing written: a stale
revision, a value its setting does not take, a clear with a value, a key no declaration takes" "a
clear takes back what the row keeps, declared or not, and clearing nothing changes nothing"
`tests/test_workshop_panes_terminal.cpp` "a Terminal line reads the Terminal's settings, and writes
them: the wheel reads its rows a notch and the line wears its prompt"

### WL-SETTING-08
`workshop/settings_seam_vocabulary.hpp` PaneSettingUnused
`workshop/weave_inspection.cpp` on(PaneSettingUnused) settings_handed
`workshop/panes.hpp` ExternalPane::settings_unused
`workshop/weave_external.cpp` hand_settings
`workshop/screen_attention.cpp` attention_conditions pane_setting_unused_key
`tests/test_workshop_panes_settings.cpp` "a pane's word that it cannot use a setting is kept against
its revision, said, and goes when a newer one is handed"

### WL-SETTING-09
`workshop/weave.hpp` let_terminal_edit_settings
`workshop/weave_inspection.cpp` settings_asker_refused
`workshop/weave_settings.cpp` on(OfficeSettingsRequested) on(PaneSettingPinRequested)
`workshop/workshop.cpp` let_terminal_edit_settings
`workshop/guests.cpp` grant_for read_guests_file
`workshop/guests.hpp` kPowerSettings
`workshop/actor_scope.hpp` kPowerSettings
`tests/test_workshop_panes_settings.cpp` "an asker the settings doors do not serve is answered that
it was withheld, and nothing is written"
`tests/test_workshop_panes_terminal.cpp` "a Terminal line reads the Terminal's settings, and writes
them: the wheel reads its rows a notch and the line wears its prompt" "a Terminal not widened to
settings cannot ask for them: the line is refused before it is sent"
`tests/test_workshop_panes_guests.cpp` "a guest whose row has `settings` reads and writes a pane's
settings; without it Loom's grant refuses the ask, and Workshop's door the asker"

### WL-SETTING-10
`workshop/settings_seam_vocabulary.hpp` OfficeSettingsDeclared OfficeSettings
OfficeSettingsRequested OfficeSettingsShown OfficeSettingCommitRequested kMaxSettingOffices
kMaxOfficeSettings kMaxOfficeSettingsKept
`workshop/weave_settings.cpp` office_rows on(OfficeSettingsDeclared) office_revision
hand_office_settings apply_workshop_settings on(OfficeSettingsRequested)
on(OfficeSettingCommitRequested) write_prefs
`workshop/prefs_persist.hpp` workshop_office_rows put_office_value put_workshop_value
`workshop/weave_external.cpp` hand_settings
`tests/test_workshop_panes_settings.cpp` "Workshop's own settings are read by message: what each
takes, what is kept, the revision, and whether the file holds it" "a commit of Workshop's own
setting applies it, keeps it in the settings file, and says both" "an office commit is refused in
words with nothing written: a stale revision, an office nothing declares for, a value not taken, a
key not declared, a lock" "an office declares the office-wide settings it takes, is handed what is
kept, and its settings are written by message"

### WL-SETTING-11
`workshop/prefs_persist.hpp` WorkshopPrefs OfficeSettingsKept PinList SettingLock Prefs from_text
kept_problem to_text kFormatVersion kWordsVersion kMaxPinLists kMaxEverywhereLocks
`workshop/weave_settings.cpp` write_prefs
`workshop/weave_handlers.cpp` load_prefs
`workshop/weave_input.cpp` state_presentation
`workshop/screen_pane_subject.cpp` pane_subject_rows
`workshop/weave_inspection.cpp` on(PaneSettingCommitRequested)
`tests/test_workshop_persistence.cpp` "the settings file round-trips what it keeps: Workshop's own
settings, another office's, pin lists and locks" "a version-3 settings file is judged whole: a value
Workshop's settings do not take, a list out of order, a pin twice, a lock with two values" "a
version-2 prefs file is read as Workshop's own three settings, and its words are still a closed set"
"a settings file another hand changed since this Workshop read it is never written over" "a prefs
file version 3 cannot read is refused whole, and its bytes stand"
`tests/test_workshop_panes_settings.cpp` "an office commit is refused in words with nothing written:
a stale revision, an office nothing declares for, a value not taken, a key not declared, a lock" "an
everywhere lock on a pane's setting refuses the message commit and the subject's row, and every
reading says it"

### WL-SETTING-12
`workshop/settings_seam_vocabulary.hpp` PaneSettingPinRequested PaneSettingPinActed kMaxPinsPerPane
kPinsWorkshopWide kPinsPerLayout
`workshop/weave_settings.cpp` pin_target effective_pins on(PaneSettingPinRequested)
`workshop/weave_inspection.cpp` on(PaneSettingsRequested)
`tests/test_workshop_panes_settings.cpp` "a pin act pins, unpins and moves a pane's setting in the
Workshop-wide list, and a reading of the row says the list" "a pin act is refused in words with
nothing written: a key not declared, one pinned already, one not pinned, an act not known, past the
bound" "per-layout pins keep each layout's own list, a layout with none reading the Workshop-wide
one"
