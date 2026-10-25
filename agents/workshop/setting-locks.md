# Workshop law — locks, and every layout's settings at once

Register `WL-LOCK`: a lock on a setting, in one layout or everywhere, the doors it holds against and
the hand that unlocks it; every layout's rows read at one revision and written in one batch; and
what a layout keeps for itself in its desk. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). A row's settings and the other settings doors are
[`settings.md`](settings.md) (`WL-SETTING`).

Retired: WL-LOCK-04.

## WL-LOCK-01 — A lock holds a setting in one layout or everywhere, and every write door refuses or passes over it

LAW — `PaneSettingLockRequested` locks a key of a pane's row in one layout, or everywhere, pinning the value its layout holds into every row seating the pane; an office-wide setting locks everywhere.

MEANS
- an everywhere lock pins the row's value, or its default when the row keeps none, writes it into every layout through the setter, all or none, and is given to a row seated later;
- it replaces each layout's own lock on the key; refused: a layout's lock on a key locked everywhere, a lock twice, a key nothing declares or keeps, a pane not seated, a file not written;
- a row's commit, Info's and an office's refuse a locked key, a clear included; every reading says each lock and where it holds.

DOES NOT MEAN
- that a lock binds a loaded weave: code in Workshop's process holds `allow_any`; a lock restrains guests, the Terminal and honest writers.

WHY — `agents/decisions/a-lock-holds-a-setting-in-a-layout-or-everywhere.md`

## WL-LOCK-02 — Unlocking is the weaver's hand

LAW — An unlock is accepted from the Terminal this host mounted, or from an office naming the correlation of the weaver's current act sent to its pane; any other unlock is refused, and the lock stands.

MEANS
- a guest's act is its own hand's, never the weaver's: no guest power and no number a guest names unlocks;
- an act the weaver's hand has since moved on from is stale, as is a number it never sent and one sent to another office's pane.

WHY — `agents/decisions/a-lock-holds-a-setting-in-a-layout-or-everywhere.md`

## WL-LOCK-03 — Every layout's rows are read at one revision and written in one batch judged whole

LAW — `LayoutSettingsRequested` reads every layout's rows, locks and baseline at one revision; `SettingsWriteRequested` at that revision is judged whole, then each write is written or passed over and said.

MEANS
- the revision moves when anything the reading says moves: a row, a layout added, removed, moved or renamed, the live one, a declaration, a lock, a baseline;
- refused whole, nothing written: the asker, a stale revision, the bounds, a write's form, target, layout or value, a baseline's name, a row's or the file's bound, a file not written;
- passed over and said: `locked`, `not-taken` (no counted declaration takes the key), `not-in-layout`; the live rows moved are handed, Workshop's own applied.

WHY — `agents/decisions/a-lock-holds-a-setting-in-a-layout-or-everywhere.md`

## WL-LOCK-05 — A layout's locks, pins and baseline are its desk's, and a replaced desk keeps its locks

LAW — A layout keeps its locks, its pin lists and its baseline in its desk; `r` and `SetupApplyRequested` never take a lock away, and are refused whole when they would change a locked value, naming each.

MEANS
- a rename keeps them, a copy takes its own, a removal takes them away; a layout's lock holds no value, and a baseline keeps a theme's name and its fingerprint alone;
- a replacing desk keeps every lock the live one holds, a lock it brings joining them; a row it drops that a lock in this layout holds is a change.

DOES NOT MEAN
- that a lock is taken away by a replacement that leaves its value: the lock stays, and its value with it.

WHY — `agents/decisions/a-lock-holds-a-setting-in-a-layout-or-everywhere.md`

## Proven by

### WL-LOCK-01
`workshop/settings_seam_vocabulary.hpp` PaneSettingLockRequested PaneSettingLockAnswered SettingLock
kLockScopeLayout kLockScopeEverywhere
`workshop/prefs_persist.hpp` put_lock drop_lock layout_target
`workshop/layouts/owner.cpp` on(PaneSettingLockRequested)
`workshop/layouts/store.cpp` locks_on pin_everywhere_locks lock
`workshop/setup.hpp` SetupLock
`workshop/setup_persist.hpp` WorkshopSetupLock
`tests/test_workshop_panes_settings.cpp` "a lock in one layout freezes its row there: both commit
doors refuse it, every reading says it, and another layout still writes it" "an everywhere lock pins
the value its layout holds into every layout that seats the pane, and a pane seated later wears it"
"a lock act is refused in words with nothing changed: an act or a scope not known, a target naming
its layout, no such layout, a key nothing takes, a pane its layout does not seat" "an office-wide
setting is locked everywhere alone, and its lock freezes the office's commit"

### WL-LOCK-02
`workshop/weave_settings.cpp` weavers_act on(WeaversActRequested)
`workshop/layouts/owner.cpp` on(WeaversActAnswered) on(PaneSettingLockRequested)
`workshop/layouts/store.cpp` lock
`tests/test_workshop_panes_settings.cpp` "only the weaver unlocks: a pane on the weaver's current
act does, and an office's ask, a forged number and a stale act are refused"
`tests/test_workshop_panes_guests.cpp` "a guest whose row has `settings` locks a setting, reads every
layout and writes a batch, and never unlocks; without it Loom's grant refuses all three"
`tests/test_workshop_panes_terminal.cpp` "a Terminal line locks a setting and, as the weaver's hand,
unlocks it, and reads every layout"

### WL-LOCK-03
`workshop/settings_seam_vocabulary.hpp` LayoutSettingsRequested LayoutSettingsAnswered LayoutSettings
LayoutPaneSettings DeclaredSettings LayoutTheme SettingsWriteRequested SettingWrite LayoutThemeWrite
SettingsWriteAnswered SettingWriteResult
`workshop/layouts/owner.cpp` on(LayoutSettingsRequested) on(SettingsWriteRequested)
`workshop/layouts/store.cpp` layout_settings_reading layout_settings_revision read_layouts write
`workshop/setup.hpp` layout_theme_name_problem
`tests/test_workshop_panes_settings.cpp` "every layout is read at once: each row with its locks, what
each pane and office takes, the everywhere locks, the live one and one revision" "a batch is judged
whole at its revision: a stale one, a value not taken, a layout not held or a bad baseline writes
nothing" "a batch writes what it can across layouts, says each write's outcome, keeps baselines, and
hands the live rows and offices it moved"

### WL-LOCK-05
`workshop/setup.hpp` SetupLock SetupPins Setup::locks Setup::pins Setup::theme
`workshop/layouts/store.cpp` replacement_refused kept_locks
`tests/test_workshop_panes_settings.cpp` "what a layout keeps for itself is its desk's: a rename keeps
it, a copy takes its own, a removal takes it away" "a layout is named by its id: a lock by id lands
on it after it moved, and two layouts of one name keep their own"
`tests/test_workshop_demo.cpp` "a desk put live keeps every lock: `r` and an agent's desk are
refused whole naming each locked value they would change, and a lock the desk brings joins"
