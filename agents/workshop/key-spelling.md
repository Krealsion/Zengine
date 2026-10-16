# Workshop law — how a key is spelled

Register `WL-KEY`, the spelling half: how a gesture is written and shown, a row with no key,
and the spelling each surface wears. The keymap's own laws are in [`keyboard.md`](keyboard.md)
and editing a binding in [`keymap-edit.md`](keymap-edit.md); the ids are one series. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-KEY-13 — A row may answer to no key at all

LAW — A row may declare no default gesture — rename, duplicate, move left and move right of a layout do — and one bound-or-not test guards dispatch, admission and every spelling.

MEANS
- `gesture_text` answers `unbound`, so no surface teaches a key that does not exist;
- a weaver may still bind any of them, and then every surface spells it.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-14 — A written gesture is modifier words joined to one key name with `+`

LAW — A written gesture is modifier words `ctrl`, `shift`, `alt`, `super` joined to a named key by `+`; the writer spells them in that order, the parser takes any order and refuses a duplicate.

MEANS
- key names are the named scan set and nothing else; punctuation is named by its own character;
- an unknown modifier or key is refused by name, never guessed.

WHY — `agents/decisions/one-binding-truth.md`

## WL-KEY-16 — The band and the Hotkeys view each spell a key as the weaver chose, apart

LAW — How the band spells a key and how the Hotkeys view spells one are two preferences, short and full by default, each toggled and saved alone; a pane's manual spells as the Hotkeys view does.

MEANS
- short is `B`, `^p`, `enter`; full is a keymap file's `shift+b`, `ctrl+p`, `return`;
- the band's is the screen's compact voice throughout: the band, notices, hints, mode headings;
- `KeymapShown` carries each key in a file's spelling, for identity, and as the view shows it.

DOES NOT MEAN
- that a spelling changes a binding, a keymap file, or what a weaver writes in one.

WHY — `agents/decisions/each-surface-spells-a-key-as-the-weaver-chose.md`

## Proven by

### WL-KEY-13
`workshop/keymap.hpp` layout.duplicate layout.move-left layout.move-right kNoGesture scan::kUnknown
is_bound gesture_text layout.rename
`tests/test_workshop_document.cpp` "an action with no default gesture answers to no key, and says
so"
`tests/test_workshop_screen.cpp` "shortcut annotations teach only truthful surrounding bindings"

### WL-KEY-14
`workshop/keymap.hpp` gesture_word parse_gesture key_name_of scancode_of_name
`tests/test_workshop_document.cpp` "a written gesture is modifier words in one order out, any order
in, and never twice" "a known backend gap is accepted and said, never silently rewritten" "a
shift+letter binding swallows the capital its keystroke produced"

### WL-KEY-16
`workshop/keymap.hpp` KeySpelling spell_gesture
`workshop/screen.hpp` Session::band_keys
`workshop/prefs_persist.hpp` kKeysDefault
`workshop/screen_hotkeys.cpp` shown_as
`workshop/weave_input.cpp` WorkshopWeave::state_presentation
`workshop/desktop_seam_vocabulary.hpp` ShownBinding
`tests/test_workshop_panes_builder.cpp` "WL-KEY-16: the band's spelling and the Hotkeys view's are
set apart from a pane's gear, each changing only its own surface, the `?` spelling as the Hotkeys
view does"
`tests/test_workshop_persistence.cpp` "prefs round-trip, and the words are a closed set" "a prefs
file of the version that held the titles alone is read, its spellings the defaults"
