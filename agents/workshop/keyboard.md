# Workshop law — keyboard

Register `WL-KEY`: one executable binding truth. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-KEY-01 — Action, binding and execution are three things

LAW — An ACTION is a stable dotted id, label, context and default gesture in `kActionCatalog`; a BINDING is the gesture that requests it; EXECUTION stays with each dispatch site.

MEANS
- every dispatch site switches on the action id and calls its own function;
- there is no callback, `std::function`, command bus or registry object in the keymap.

DOES NOT MEAN
- that a provider contributes declarations — the pane seam has no shape for wanted keys.

PROVEN BY — `workshop/keymap.hpp` `kActionCatalog`, `Act`, `Keymap`;
`tests/test_workshop_document.cpp` case `"KEY-0: an authored override changes dispatch AND every
displayed spelling"`, case `"KEY-0: an override survives restart, and deleting the file restores
defaults"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-02 — `Session::keymap` is the effective truth, and no surface spells a literal

LAW — Dispatch, every help surface and persistence read the one effective keymap on the session; every hint is a projection through the keymap's spelling functions, never a string literal.

MEANS
- the band rows, title hints, mode headings, the terminal header and prompt, the boot line;
- adding a gesture claim as a literal reintroduces the drift once measured in six places.

PROVEN BY — `workshop/screen.hpp` `keymap`, `hotkey_text`, `setup_hints`; `workshop/keymap.hpp`
`gesture_text`; `tests/test_workshop_document.cpp` case `"KEY-0: an authored override changes
dispatch AND every displayed spelling"`, case `"KEY-0: the terminal header and hints spell the
effective toggle"`; `tests/test_workshop_panes_window.cpp` case `"WUX-6/SC-7: the coarse step is
ordinary action vocabulary, not pane chrome"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-03 — `keyboard_context` is the routing chain, spelled once

LAW — The routing chain is spelled once — the terminal, the arrangement scope, the contextual surface, then what lies beneath the menu — and the key, text and paste-owner handlers all switch on it.

MEANS
- `context_takes_text(ctx)` replaced the old hand-kept mirror;
- it is resolved fresh and stored nowhere: no context stack, no registration, no focus framework.

PROVEN BY — `workshop/screen.hpp` `keyboard_context`, `keyboard_context_beneath_menu`;
`workshop/keymap.hpp` `context_takes_text`, `KeyContext`; `workshop/weave.hpp` `paste_owner_now`,
`on`; `tests/test_workshop_panes_input.cpp` case `"MSG-0: every Workshop mode owns the keyboard
above a focused pane"`; `tests/test_workshop_editor.cpp` case `"EDIT-0: the editor context takes
text, and its class algebra is exact"`; `tests/test_workshop_document.cpp` case `"KEY-0: the view
lists the context beneath it, and three contexts differ"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-04 — Matching is exact

LAW — A binding matches the observed modifier bits exactly, one family spelled two ways is two declared actions, and `shift+space` is gone rather than aliased.

PROVEN BY — `workshop/keymap.hpp` `Keymap`, `Gesture`, `action_for`;
`tests/test_workshop_document.cpp` case `"KEY-0: exact modifier matching -- the accidental subset
aliases no longer fire"`, case `"KEY-0: shift+space is gone -- not a binding, not an invisible
alias"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-05 — Three declaration-only activity classes

LAW — Three declaration-only classes: global rows above every mode, no-text rows exactly where no editable text has the keys, no-editor rows everywhere but the editor; nothing else answers above a mode.

MEANS
- `document.open`, `workshop.terminal`, `workshop.hotkeys` are global;
- `workshop.quit` (`^c`) and `workshop.attention` (`^a`) are `kNoText`;
- `document.save` (`^s`) is `kNoEditor`; the editor's row is `editor.save`; they never meet.

PROVEN BY — `workshop/keymap.hpp` `kGlobal`, `kNoText`, `kNoEditor`, `above_mode_action`,
`workshop.quit`, `workshop.attention`, `document.save`, `editor.save`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: one physical ^s resolves to the document's save
or the editor's, by context"`; `tests/test_workshop_document.cpp` case `"TEXT-0: ^c still quits
exactly where nothing takes text"`; `tests/test_workshop_panes_input.cpp` case `"MSG-0: the keys
that mean the same thing in every mode still outrank a pane"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-06 — An action may own several rows, and an override moves all of them

LAW — An action's identity is kept across migrations, however its rows move; a retired id's authored row is preserved byte-for-byte as unknown.

MEANS
- `manage.arrange` was added; `manage.move`, `manage.size` and `manage.edge` are retired;
- reusing one gesture across mutually exclusive contexts is legal.

PROVEN BY — `workshop/keymap.hpp` `kActionCatalog`, `manage.arrange`, `manage.next`,
`manage.previous`, `workshop.manage`, `AuthoredOverride`; `tests/test_workshop_document.cpp` case
`"KEY-0: an override for an unknown action survives with its intent whole"`, case `"KEY-0: reusing
one gesture across mutually exclusive contexts is legal"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-07 — The keymap file is a durable artifact of authored differences

LAW — `zengine-workshop-keymap` version 1 (`--keymap`, default `workshop-keymap.json`): defaults in code, authored differences only, absent ≡ defaults, hand-edited, never rewritten.

MEANS
- loaded once on the first `SurfaceReady`, the session restore's own moment;
- `Keymap::authored` is what a save writes back, so a round trip edits nothing.

PROVEN BY — `workshop/keymap_persist.hpp` `zengine-workshop-keymap`, `kFormatVersion`,
`authored`, `to_keymap`, `load_file`; `workshop/weave.hpp` `load_keymap`, `keymap_path`;
`workshop/workshop.cpp` `keymap`; `tests/test_workshop_document.cpp` case `"KEY-0: an override
survives restart, and deleting the file restores defaults"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-08 — Admission refuses, naming what a maker can fix

LAW — Refused: a gesture outside the grammar on a known action, an action authored twice, a same-context collision over the effective map, a bare printable or a component chord on a global.

MEANS
- an unknown action's row is preserved unjudged;
- a known POSIX-gap gesture is accepted and the gap said once (`posix_gap`).

PROVEN BY — `workshop/keymap.hpp` `posix_gap`, `contexts_intersect`, `component_owns_gesture`,
`apply_overrides`; `workshop/keymap_persist.hpp` `from_text`; `tests/test_workshop_document.cpp`
case `"KEY-0: a same-context collision is refused naming both actions and the gesture"`, case
`"KEY-0: a gesture outside the grammar on a KNOWN action is refused in words"`, case `"KEY-0: a
global action cannot take a bare printable or the editing vocabulary"`, case `"KEY-0: a known
backend gap is accepted and said, never silently rewritten"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-09 — The legend preference governs the band's legend rows and nothing else

LAW — `full`/`compact`/`hidden` (`default` is the code's answer) govern every band row after the notice and nothing else; hidden blanks them, reclaims no geometry and unbinds nothing.

MEANS
- the full rows fold four families exactly while every member sits on its default (`help_pairs`);
- the hotkey view remains the complete list in every mode.

PROVEN BY — `workshop/screen.hpp` `band_region`, `help_rows`, `help_pairs`;
`workshop/keymap.hpp` `legend_mode`; `workshop/keymap_persist.hpp` `kLegendDefault`;
`tests/test_workshop_document.cpp` case `"KEY-0: the legend's three modes project the band, and
hidden unbinds nothing"`, case `"WUX-1/SC-3: the legend modes move only the legend rows, in both
budgets"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-10 — The hotkey view opens beside the selected pane and fits what it says

LAW — The hotkey view anchors at the selected pane's visible outer top-left and is sized from its own rows through the one popup measurer; with no anchor it opens at the overlay column's corner.

MEANS
- it is derived at every paint and press and stored nowhere;
- a list taller than the band keeps the band's height while the painter says what it cut.

DOES NOT MEAN
- that `attention_bounds` follows the selection — a condition is about the application.

PROVEN BY — `workshop/screen.hpp` `hotkeys_bounds`, `hotkeys_rows`, `popup_bounds_at`,
`overlay_column`, `paint_hotkeys`, `HotkeysView`, `attention_bounds`;
`tests/test_workshop_screen.cpp` case `"WUX-5: contextual help opens at the selected pane, and
follows it"`, case `"QR-17/SC-1..3: the hotkey view is as tall as its rows and as wide as its
longest"`, case `"QR-17/SC-4: a list the room cannot hold keeps the room and counts the cut"`.
WHY — `agents/decisions/content-sized-popups.md`

## WL-KEY-11 — The hotkey view is a projection, not an owner

LAW — It lists the context beneath it, shows the component's vocabulary from `kEditingVocabulary` marked not remappable, is keys-modal while open, and owns no pointer space.

MEANS
- its toggle and bare Escape close it, and Escape is not a keymap action;
- a focused pane is described only as ownership — Workshop is never told a provider's bindings.

PROVEN BY — `workshop/screen.hpp` `paint_hotkeys`, `hotkeys_rows`, `keyboard_context_name`;
`component/text_box.hpp` `kEditingVocabulary`; `workshop/weave.hpp` `hotkeys_key`;
`tests/test_workshop_document.cpp` case `"KEY-0: ctrl+k opens the hotkey view, esc and ctrl+k
close it"`, case `"KEY-0: the view is keys-modal -- a maker reading a binding is not executing
it"`; `tests/test_workshop_screen.cpp` case `"QR-17/SC-6,7: the compact view owns no pointer space
and moves no reservation"`; `tests/test_workshop_editor.cpp` case `"EDIT-0: the hotkey view
answers for the editor with its own unremappable keys"`.
WHY — `agents/decisions/content-sized-popups.md`

## WL-KEY-12 — The printable-trigger swallow is derived from the binding

LAW — `expected_text_of` arms the swallow centrally in `on(KeyPressed)` when the keymap consumed a text-faced gesture, and the very next key or text clears it; no site hard-codes a character.

PROVEN BY — `workshop/keymap.hpp` `expected_text_of`; `workshop/weave.hpp` `swallow_text_`,
`same_keystroke`; `tests/test_workshop_document.cpp` case `"KEY-0: a printable trigger's own
character is swallowed, wherever it is authored"`, case `"KEY-0: the swallow eats only the
trigger's own character, never a different one"`, case `"KEY-0: a shift+letter binding swallows
the capital its keystroke produced"`.
WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-13 — A row may answer to no key at all

LAW — A row may declare no default gesture — rename, duplicate, move left and move right of a layout do — and one bound-or-not test guards dispatch, admission and every spelling.

MEANS
- `gesture_text` answers `unbound`, so no surface teaches a key that does not exist;
- a maker may still bind any of them, and then every surface spells it.

PROVEN BY — `workshop/keymap.hpp` `layout.duplicate`, `layout.move-left`, `layout.move-right`,
`kNoGesture`, `scan::kUnknown`, `is_bound`, `gesture_text`, `layout.rename`;
`tests/test_workshop_document.cpp` case `"WUX-11: an action with no default gesture answers to
no key, and says so"`; `tests/test_workshop_screen.cpp` case `"ARR-0: shortcut annotations teach
only truthful surrounding bindings"`.
WHY — `agents/decisions/one-binding-truth.md`

## Do not assume

- That the legend has a fixed row count per medium — it is every band row after the notice,
  however many the band's budget grants (WL-KEY-09).
