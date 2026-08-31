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
                         word_before / word_after
                                          what a WORD is: space-delimited runs, nothing more
                         pasteable_line   what foreign bytes become in a one-line box
                         Clipboard        text a maker copied + a writes counter + a
                                          paste_requests counter; the OWNER holds it, the
                                          component only operates on it
                         key:: / mod::    the editing vocabulary's identities -- the same
                                          numbers input::scan::/mod:: name, spelled locally
                                          because this package includes nothing (pinned
                                          against the wire in the input suite)

                         TextBox          text + caret + anchor + first_visible, as one state
                           text/caret/anchor/first_visible/size/empty/at_end/caret_column
                           draft_epoch      which draft this box holds (set/clear bump it)
                           has_selection/selection_begin/selection_end/selected_text
                           visible(columns) / visible_selection(columns)
                           position_at_column(column) / keep_caret_visible(columns)
                           type/backspace/erase_forward/left/right/home/end/place/clear/set
                           select_left/right/home/end / select_all / word + select_word moves
                           word_at(at) -> WordSpan / select_word_at(at) -> bool
                           erase_word_before/after / drag_to_column(column)
                           copy/cut/paste(Clipboard) / undo/redo/can_undo/can_redo
                           consume(scancode, modifiers, Clipboard) -> bool
```

**A word has one definition here, and every gesture composes it from the same scan.** A word
is a maximal run of non-space bytes -- a shell's word, not an editor's: no identifier class, no
punctuation class, no locale, no Unicode category table. `word_run_begin` / `word_run_end` are
that run, read backwards and forwards from a position; `word_before` / `word_after` add the
separator walk a keyboard gesture means, and `word_at` is the two scans meeting at one position,
which is what a pointer means. So `Ctrl`+`Shift`+`Left`, `Ctrl`+`Backspace` and a consumer's
double-click select and delete exactly the same bytes -- not because three implementations were
kept in agreement, but because there is only one. A position with a separator on both sides is
in **no** word and `WordSpan::present()` says so; nothing invents the nearest one.

`select_word_at` is `place`'s other answer to one press: the anchor goes to the word's start and
the caret to its end (`select_all`'s choice of active end), and a position in no word places the
caret and selects nothing. **The component knows nothing about clicks.** Whether two presses are
one gesture -- the interval, the control's identity, the modifiers -- belongs to the consumer
holding the presses; this class supplies the span and the operation and no timing at all.

**Since TEXT-0 the ordinary expectations are mechanics, not omissions.** A selection is the
anchor and the caret (`anchor == caret` *is* "no selection"); typing replaces it, Shift-movement
extends it, plain movement collapses it, and both ends stay on character boundaries always. The
clipboard operations move text through a `Clipboard` the **owner** holds — where its text goes
beyond this process (a platform clipboard through a Skin, a bus publication, nowhere) is the
owner's custody. Since QR-11 that custody includes the paste's *value*: `consume`'s Ctrl+V
records a request (`Clipboard::paste_requests`) rather than pasting, because the value a paste
means is the clipboard's **current** one and only the owner can obtain it — read on the
maker's intent, never mirrored from watching — and the owner applies it through `paste` once
it holds the text. `draft_epoch()` is the companion counter, bumped by `set`/`clear` (the two
draft doors), so an owner whose acquisition crosses a turn can tell the draft that asked from
whatever draft is standing when the answer arrives. Undo is a bounded local snapshot history
that dies with the draft: `set` and `clear` wipe it, so a new draft can never resurrect an old
one's text; contiguous same-kind keystrokes coalesce into one entry. And
`consume(scancode, modifiers, clip)` is the one owner of the editing-key vocabulary, under the
press chain's own bool: *true* = mine, stop routing; *false* = not my vocabulary, yours.
Declining is `default:`, not knowledge — Return, Escape, Tab, `^s` and every application chord
ever invented come back `false` from a switch that was never edited, which is what four
consumers used to spell as four copies of the same mapping.

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
tab order, multiline mode or theme, and none of them will arrive because a toolkit is expected
to have one — the rule this package is built on is *extract from repeated working behaviour,
never from a list of widgets*. (Selection, the clipboard operations and a local undo *did*
arrive, in TEXT-0, and the reason is the same rule read forward: with four consumers carrying
one editing surface, "a text box that cannot select, copy or undo" had stopped being a smaller
component and become a surprising one.) The pre-Zen `Zen::TextBox` (`reference/`, archaeology
only) is not its ancestor in anything but the name: it carried a filter, a focus flag, a blink
timer, two signals and a child `Text` entity, and it could not move its caret, could not
scroll, and erased one **byte** at a time.
