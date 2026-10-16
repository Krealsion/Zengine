# Workshop law — keyboard

Register `WL-KEY`: one executable binding truth. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-KEY-01 — Action, binding and execution are three things

LAW — An ACTION is a stable dotted id, label, context and default gesture in `kActionCatalog`; a BINDING is the gesture that requests it; EXECUTION stays with each dispatch site.

MEANS
- every dispatch site switches on the action id and calls its own function;
- there is no callback, `std::function`, command bus or registry object in the keymap.

DOES NOT MEAN
- that execution moves into the keymap: a pane row's dispatch site is the pane (WL-KEY-15).

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-02 — `Session::keymap` is the effective truth, and no surface spells a literal

LAW — Dispatch, every help surface and persistence read the one effective keymap on the session; every hint is a projection through the keymap's spelling functions, never a string literal.

MEANS
- the band rows, title hints, mode headings, the terminal header and prompt, the boot line;
- adding a gesture claim as a literal reintroduces the drift once measured in six places.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-03 — `keyboard_context` is the routing chain, spelled once

LAW — The routing chain is spelled once — the terminal, the arrangement scope, the contextual surface, then what lies beneath the menu — and the key, text and paste-owner handlers all switch on it.

MEANS
- `context_takes_text(ctx)` replaced the old hand-kept mirror;
- it is resolved fresh and stored nowhere: no context stack, no registration, no focus framework;
- a pane's rows are keyed by its runtime handle, the value `kPane` stands for (WL-KEY-15).

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-04 — Matching is exact

LAW — A binding matches the observed modifier bits exactly, one family spelled two ways is two declared actions, and `shift+space` is gone rather than aliased.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-05 — Three declaration-only activity classes

LAW — Three declaration-only classes answer above a mode: global rows everywhere, no-text rows where no editable text has the keys, unless-owned rows unless the keyboard's pane declared it owns them.

MEANS
- the host's one row above a mode is `workshop.quit` (`^c`, `kNoText`); the launches are an app's;
- ⭐ `kGlobal` and `kUnlessOwned` hold no host row since `^s`/`^o` retired; the classes stand;
- `Keymap::row_active` is the one answer for all three; every view spends it.

DOES NOT MEAN
- that a pane's row is a fourth class: it meets these, never another mode's (WL-KEY-15).
- ⚠ that `kNoEditor` survived: `kUnlessOwned` is it, with the exception declared.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-06 — An action may own several rows, and an override moves all of them

LAW — An action's identity is kept across migrations; a retired id's authored row is preserved byte-for-byte, and applied when something declares it or, for a renamed id, its successor.

MEANS
- a renamed id is read as its successor; a retired one is kept, said, and unanswered;
- a row that left this host for a PANE keeps its id, so the weaver's override moves with it;
- reusing one gesture across mutually exclusive contexts is legal.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-07 — The keymap file is a durable artifact of authored differences

LAW — `zengine-workshop-keymap` version <!-- value keymap_persist::kFormatVersion -->2<!-- /value --> (`--keymap`, default `workshop-keymap.json`): defaults in code, authored differences only, absent ≡ defaults; read once, written by the edit door alone.

MEANS
- read once at the first `SurfaceReady`, bytes kept; a version-1 file is imported, written as 2;
- `Keymap::authored` is what a write writes back: rows as values, in order; layout not promised.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-08 — Admission refuses, naming what a weaver can fix

LAW — Refused: a gesture outside the grammar, one gesture twice for an action, `none` beside a key, a same-context collision over every key in force, a bare printable or component chord above every mode.

MEANS
- several rows for one id are several keys: dispatch answers to any, the legend spells the first;
- an unknown action's row is preserved unjudged;
- a known POSIX-gap gesture is accepted, bound as written, and its gap said once (`posix_gap`): a file's at load, a pane row's when it comes into force.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-09 — The legend preference governs the band's legend rows and nothing else

LAW — `full`/`compact`/`hidden` (`default` is the code's answer) govern every band row after the notice and nothing else; hidden blanks them, reclaims no geometry and unbinds nothing.

MEANS
- the full rows fold four families exactly while every member sits on its default (`help_pairs`);
- compact is the application's rows above every mode; the effective keymap stays the full list;
- where no pane holds the keys, the full rows lead with the application's rows above every mode.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-10 — The key list is the desktop's Hotkeys pane

LAW — The key list is the desktop's Hotkeys pane, seated and arranged like any pane; nothing in this host anchors, sizes or paints a key list.

WHY — `agents/decisions/content-sized-popups.md`

## WL-KEY-11 — The host projects the effective keymap as a value and presents none of it

LAW — The host projects the effective keymap as a value (`keymap_shown`, WL-DESK-11) and presents none of it; the text box's keys are listed there, marked not remappable.

WHY — `agents/decisions/content-sized-popups.md`

## WL-KEY-12 — The printable-trigger swallow is derived from the binding

LAW — `expected_text_of` arms the swallow centrally in `on(KeyPressed)` when the keymap consumed a text-faced gesture, and the very next key or text of that hand clears it; no site hard-codes a character.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-15 — A pane declares its actions, and the host joins them at admission

LAW — A pane declares rows beside its offer, in either published version; Workshop judges them whole under the office stamp, joins them under the collision law, and dispatches the resolved id.

MEANS
- input's OWNER is the pane's handle only while the resolved context is that pane's;
- a v2 row may name an application row it stands in for; a retired id (`^s`) stands for nothing;
- a refusal keeps the previous rows; a file's load re-joins every pane and withdraws the loser.

DOES NOT MEAN
- that a pane says it wants keys: a declaration points no keyboard at it and holds none;
- that rebinding either row moves either meaning, or that a global row can be owned.

WHY — `agents/decisions/a-pane-declares-its-actions.md`

## Do not assume

- That the legend has a fixed row count per medium — it is every band row after the notice,
  however many the band's budget grants (WL-KEY-09).

## Proven by

### WL-KEY-01
`workshop/keymap.hpp` kActionCatalog Act Keymap ActionRow
`workshop/weave_input.cpp` command
`tests/test_workshop_document.cpp` "an authored override changes dispatch AND every displayed
spelling" "an override survives restart, and deleting the file restores defaults"

### WL-KEY-02
`workshop/screen.hpp` Session::keymap
`workshop/screen_bindings.cpp` hotkey_text
`workshop/screen_layouts.cpp` setup_hints
`workshop/keymap.hpp` gesture_text
`tests/test_workshop_document.cpp` "an authored override changes dispatch AND every displayed
spelling"
`tests/test_workshop_panes_window.cpp` "the coarse step is ordinary action vocabulary, not pane
chrome"

### WL-KEY-03
`workshop/screen_arrange.cpp` keyboard_context keyboard_context_beneath_menu
`workshop/keymap.hpp` context_takes_text KeyContext
`workshop/weave_seam.cpp` paste_owner_now
`workshop/weave_handlers.cpp` on(KeyPressed)
`workshop/weave_pointer.cpp` on(TextEntered)
`tests/test_workshop_panes_input.cpp` "every Workshop mode owns the keyboard above a focused pane"
`tests/test_workshop_panes_editor.cpp` "^s is the Editor's save while it holds the keys, and the
host answers ^s nowhere"
`tests/test_workshop_document.cpp` "the effective keymap lists every place a key is answered, and
marks the text box's keys as nobody's to move"

### WL-KEY-04
`workshop/keymap.hpp` Keymap Gesture Keymap::action_for
`tests/test_workshop_document.cpp` "exact modifier matching -- the accidental subset aliases no
longer fire" "a known backend gap is accepted and said, never silently rewritten"

### WL-KEY-05
`workshop/keymap.hpp` KeyContext::kGlobal KeyContext::kNoText KeyContext::kUnlessOwned
Keymap::above_mode_action workshop.quit
`workshop/weave_handlers.cpp` on(KeyPressed)
`tests/test_workshop_panes_editor.cpp` "^s is the Editor's save while it holds the keys, and the
host answers ^s nowhere"
`tests/test_workshop_document.cpp` "^c still quits exactly where nothing takes text"
`tests/test_workshop_panes_input.cpp` "the keys that mean the same thing in every mode still outrank
a pane"

### WL-KEY-06
`workshop/keymap.hpp` kActionCatalog manage.arrange manage.next manage.previous workshop.manage
AuthoredOverride contexts_intersect RenamedAction kRenamedActions renamed_to RetiredAction
kRetiredActions retired_with join_app_rows
`workshop/weave_handlers.cpp` load_keymap
`tests/test_workshop_document.cpp` "an override for an unknown action survives with its intent
whole" "reusing one gesture across mutually exclusive contexts is legal" "a retired id in a weaver's
file is kept and said, and nothing answers it"
`tests/test_workshop_panes_builder.cpp` "a weaver's authored override for a retired Workshop id
keeps working"
`tests/test_workshop_panes_actions.cpp` "a keymap row written for an id whose owner changed is read
as its successor, once, and the load says which rename to make"

### WL-KEY-07
`workshop/keymap_persist.hpp` zengine-workshop-keymap kFormatVersion kFormatVersionOne
keymap_rows_in to_keymap from_text load_file
`workshop/weave_handlers.cpp` load_keymap
`workshop/weave.hpp` HostContext::keymap_path keymap_bytes_
`workshop/workshop.cpp` Arguments::keymap
`workshop/keymap.hpp` Keymap::authored
`tests/test_workshop_document.cpp` "an override survives restart, and deleting the file restores
defaults"

### WL-KEY-08
`workshop/keymap.hpp` posix_gap gap_sentence PaneRows::gaps contexts_intersect component_owns_gesture
apply_overrides AuthoredOverride AuthoredGestures authored_gestures_for Keymap::row_gestures
Keymap::row_answers join_app_rows join_pane_rows
`workshop/keymap_persist.hpp` from_text
`workshop/screen_hotkeys.cpp` keymap_shown
`workshop/weave_handlers.cpp` load_keymap
`workshop/weave.hpp` WorkshopWeave::unsaid_gaps
`workshop/weave_seam.cpp` unsaid_gaps declare_pane_actions rejoin_pane_rows
`tests/test_workshop_document.cpp` "a same-context collision is refused naming both actions and the
gesture" "a gesture outside the grammar on a KNOWN action is refused in words" "a known backend gap
is accepted and said, never silently rewritten"
`tests/test_workshop_panes_actions.cpp` "an application row answered above every mode cannot take a
bare printable or a chord the text box owns, whoever wrote it" "WL-KEY-08: a pane row on a gesture the
terminal cannot produce is joined as it stands, and its gap is kept once" "WL-KEY-08: a pane's row the
terminal cannot produce is admitted, acts, and is said once, whichever path brought it into force"

### WL-KEY-09
`workshop/screen_compose.cpp` band_region
`workshop/screen_gestures.cpp` help_rows
`workshop/screen_bindings.cpp` help_pairs
`workshop/keymap.hpp` legend_mode
`workshop/keymap_persist.hpp` kLegendDefault
`tests/test_workshop_document.cpp` "the legend's three modes project the band, and hidden unbinds
nothing" "the legend modes move only the legend rows, in both budgets"
`tests/test_workshop_panes_actions.cpp` "the band and the Hotkeys pane teach the application's keys
as they are in force: a moved row where it moved, a disabled one as having no key"

### WL-KEY-10
`tests/test_workshop_panes_actions.cpp` "the band and the Hotkeys pane teach the application's keys
as they are in force: a moved row where it moved, a disabled one as having no key"

### WL-KEY-11
`workshop/screen_hotkeys.cpp` keymap_shown
`tests/test_workshop_document.cpp` "the effective keymap lists every place a key is answered, and
marks the text box's keys as nobody's to move"

### WL-KEY-12
`workshop/keymap.hpp` expected_text_of
`workshop/weave.hpp` WorkshopWeave::Hand::swallow_text
`workshop/weave_seam.cpp` same_keystroke
`workshop/weave_handlers.cpp` on(KeyPressed)
`tests/test_workshop_document.cpp` "a printable trigger's own character is swallowed, wherever it is
authored" "the swallow eats only the trigger's own character, never a different one" "a shift+letter
binding swallows the capital its keystroke produced"

### WL-KEY-15
`workshop/pane_vocabulary.hpp` PaneActionRow PaneActions PaneActionRequested v2::PaneActionRow
v2::PaneActions kOwnableDocumentSave
`workshop/keymap.hpp` PaneRow PaneRows kMaxPaneActionRows collision_sentence Keymap::panes
Keymap::pane_action_for check_pane_action_text join_pane_rows drop_pane_rows PaneRow::supersedes
Keymap::pane_supersedes Keymap::row_active Keymap::owner_of superseded_here
`workshop/weave.hpp` declare_pane_actions on(PaneActions) rejoin_pane_rows
`workshop/weave_seam.cpp` declare_pane_actions on(PaneActions) rejoin_pane_rows
`workshop/panes.hpp` RuntimePane::actions
`workshop/setup.hpp` admit_pane_actions
`workshop/weave_external.cpp` external_key
`workshop/screen.hpp` help_pairs
`workshop/screen_bindings.cpp` help_pairs
`workshop/screen_hotkeys.cpp` keymap_shown
`tests/test_workshop_panes_actions.cpp` "the join judges a declaration whole, in order, and a
refusal writes nothing" "a declared gesture arrives as the resolved id and an undeclared one as the
key; typing still crosses raw" "the keymap file wins: a pane whose rows its bindings collide with is
refused in words, in both orders" "a pane built against the published version one still registers,
declares and dispatches"
`tests/test_workshop_panes_editor.cpp` "an application row a pane stands in for belongs to every
context that is not the pane's own" "a pane that owns one action may put its other rows on that
action's key"
