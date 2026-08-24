# The Component package

**Reference.** Reusable pieces of a maker-facing tool that own their own semantic state and
know nothing about the medium showing them. There is currently exactly one: `TextBox`.

Source: [`component/text_box.hpp`](../../component/text_box.hpp).

This package exists because of a **measurement** rather than a roadmap: two working Workshop
tools reached the same editing machinery from opposite ends. The Terminal's command
line had text, a movable caret, character-safe edits, a horizontal window and a pointer that
places the caret. An Inspector property draft had the text and the character-safe
edits — and **no** caret, **no** window, and no way to reach a value longer than its row.

The second consumer was traced on all nine axes and extraction *declined*, because at that
point the two shared only the character walk they were already sharing as free functions: a
`TextBox` would have renamed `TerminalInput` and deleted nothing. It was extracted on the day
the property editor genuinely needed the caret, the window and the pointer arithmetic — the
day extracting became the **smaller** repair. That is the rule this package is built on:
**extract from repeated working behaviour, never from a list of widgets.**

```text
component/text_box.hpp   is_continuation_byte / character_before / character_after
                         character_boundary / character_boundary_at_or_after
                                          what a CHARACTER is, in this application

                         TextBox          text + caret + first_visible, as one state
                           text/caret/first_visible/size/empty/at_end/caret_column
                           visible(columns) / position_at_column(column)
                           keep_caret_visible(columns)
                           type/backspace/erase_forward/left/right/home/end/place/clear/set
```

Four things are structural rather than promised:

- **The operations are the only door.** `0 <= first_visible <= caret <= size()`, both indices on
  a character boundary, holds after every mutator because `settle()` runs at the end of each
  one. There is no `fix_it()` to call and no way to reach a state that would need it. The other
  half — `caret - first_visible <= N` and `first_visible <= max(0, size() - N)` — needs to know
  how much room there is, so it is `keep_caret_visible(N)`, which a consumer calls once per
  repaint with the capacity it resolved.
- **The capacity is an argument and never a member.** The Terminal's row and an Inspector row
  are different widths in the same running application, so a component that remembered one of
  them would be remembering the wrong one for the other.
- **It owns no policy and no medium.** No SDL, no terminal, no cell, no pixel, no font metric,
  no commit, no validation, no refusal, no parse, no completion, no submission, no focus, no
  blink and no drawing. What a draft *means* is the consumer's, which is exactly what lets one
  implementation serve two tools whose commit models have nothing in common: the Terminal
  submits a line to a participant, and a property row parses it, writes it, and may be refused
  with a reason.
- **It is not an entity.** No identity, no registry, no persistence, nothing to clean up. A
  `TerminalPane` owns one and a `workshop::Row` owns one; destroying the owner destroys it.

`zengine-component` links **nothing** — not even `loom::core`, which every other package here
needs for `zen/weave/shape.hpp`. A TextBox has no wire form, nothing serializes it and nothing
hosts it, and the absence of that link is the enforcement of "a component is not content".

What it is **not**: a widget set. There is no Button, List, Dropdown, ScrollView, focus tree,
tab order, selection range, clipboard, undo stack, multiline mode or theme, and none of them
will arrive because a toolkit is expected to have one — the rule this package is built on is
*extract from repeated working behaviour, never from a list of widgets*. The pre-Zen
`Zen::TextBox` (`reference/`, archaeology only) is not its ancestor in anything but the name: it
carried a filter, a focus flag, a blink timer, two signals and a child `Text` entity, and it
could not move its caret, could not scroll, and erased one **byte** at a time.
