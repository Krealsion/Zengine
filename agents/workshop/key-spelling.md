# Workshop law — how a key is spelled

Register `WL-KEY`, the spelling half: how a gesture is written and shown, a row with no key,
and the spelling each surface wears. The keymap's own laws are in [`keyboard.md`](keyboard.md)
and editing a binding in [`keymap-edit.md`](keymap-edit.md); the ids are one series. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-KEY-13, WL-KEY-16

## WL-KEY-14 — A written gesture is modifier words joined to one key name with `+`

LAW — A written gesture is modifier words `ctrl`, `shift`, `alt`, `super` joined to a named key by `+`; the writer spells them in that order, the parser takes any order and refuses a duplicate.

MEANS
- key names are the named scan set and nothing else; punctuation is named by its own character;
- an unknown modifier or key is refused by name, never guessed.

WHY — `agents/decisions/one-binding-truth.md`

## Proven by

### WL-KEY-14
`input/gesture_grammar.hpp` gesture_word parse_gesture key_name_of scancode_of_name
`tests/test_workshop_document.cpp` "a written gesture is modifier words in one order out, any order
in, and never twice" "a known backend gap is accepted and said, never silently rewritten" "a
shift+letter binding swallows the capital its keystroke produced"
