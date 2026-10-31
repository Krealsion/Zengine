# The Component package

**Reference.** Reusable pieces of a weaver-facing tool that own their own semantic state and
know nothing about the medium showing them. The stateful pieces are the text pieces -- a core
of characters, words, places and the copy, a `TextBox` that edits one line over it and a
`TextArea` that shows long text, read only, over it; four list
mechanics — a window onto a list, a composition read backwards, a choice held by identity, and
a table's columns; a strip of labelled controls; a strip of tabs over a settings list; and an
accordion, one section shut, open or peeking, with the independent group over accordions and the
rows a tree shows, an accordion at every level. A pure `motion::Path` also supplies
linear/cubic Bezier sampling and elapsed-time progress.

The rule this package is built on: **a piece owns an invariant and knows no medium, and it is
built whole, with care, for the uses Zengine sets out to serve.** A piece belongs here when it
holds state that every tool showing it would otherwise keep true for itself. It does not wait for
tools to repeat it first, and it is never made from a list of familiar widget names: a use nobody
has named is still a guess, so a piece says what it is for before it is built.

Sources: [`component/text.hpp`](../text.hpp),
[`component/text_box.hpp`](../text_box.hpp),
[`component/text_area.hpp`](../text_area.hpp),
[`component/list_window.hpp`](../list_window.hpp),
[`component/row_map.hpp`](../row_map.hpp),
[`component/held_choice.hpp`](../held_choice.hpp),
[`component/control_strip.hpp`](../control_strip.hpp),
[`component/columns.hpp`](../columns.hpp),
[`component/tab_strip.hpp`](../tab_strip.hpp),
[`component/accordion.hpp`](../accordion.hpp),
[`component/motion.hpp`](../motion.hpp).

```text
component/list_window.hpp   ListWindow{first, count, before, after, markers}
                            cursor_window / centred_window / scroll_window / scroll_by
                                          what a bounded place shows of a list, what it
                                          omitted (always conserved), and how many rows it
                                          reserved to say so; three anchorings, one accounting
                            earlier_marker / more_marker
                                          how many a side's marker row says were left out,
                                          where the window reserved that side a row
                            take_wheel_notches
                                          a wheel's whole notches, the fraction carried
component/row_map.hpp       RowMap<Meaning>: row / span / at / at_row / row_of / cover
                            begin / settle / picture / current / press_order
                                          what each published row and control means, recorded
                                          by the pass that wrote it; the composition's number,
                                          moved only when the map moves; the spans in the order
                                          a press reads them, the one `at` answers last; rows
                                          something is drawn over meaning nothing beneath it;
                                          solid_columns
component/held_choice.hpp   HeldChoice<Key>: find / hold / step / actionable
                                          a choice kept by identity across a list that moves,
                                          and still a choice while its row is gone
component/control_strip.hpp Control{label, available}, pack_controls, control_face
                            ControlStrip{rows, placed, dropped}
                            strip_budget / kMaxStripRows / strip_rows_for
                                          labelled controls packed into the width a pane has,
                                          one space apart, wrapped and capped; `[label]` when
                                          the pane believes the operation applies, `(label)`
                                          when it does not -- both PLACED, because a control
                                          a pane will refuse is still one it must answer for.
                                          What did not fit is counted, never cut in half; the
                                          rows a strip may spend grow with the room, to a cap
component/columns.hpp       Column, layout_columns, column_offsets, table_line, widest
                                          a table laid out once for every row, cut from the
                                          last column to the first, never silently
component/tab_strip.hpp     setting_tabs, window_tab_strip, tab_face
                            TabStrip{text, tabs, before, after}, PlacedTab
                                          the tabs a settings list derives -- pinned, all,
                                          each tag in byte order, kept -- and what a width
                                          shows of them: a window grown outward from the
                                          active tab, `*word*`, counting `<N` and `N>` on
                                          each side; derived each time, so no offset can go
                                          stale when the tags change
component/accordion.hpp     Fold{kShut, kOpen, kPeek}, fold_mark
                            Accordion{open, resting, spent}: fold / rest / press / spend
                                          one section: a header that always shows and a body
                                          that shows while it is open, or over what follows
                                          while the pointer rests on a shut one's header; a
                                          press opens a peek, and a section shut under the
                                          pointer peeks again only once it has left; `+` on
                                          a header a press opens, `-` on one it shuts
                            SectionSpan, section_span
                                          what a section spends of a room in the owner's unit:
                                          the header, then as much of the body as fits -- taken
                                          from the room while open, over what follows while it
                                          peeks -- and what did not fit, counted
                            IndependentAccordions<Key>: section / fold / press / open / shut
                            shut_all / rest / rest_none / keep
                                          the independent group over the open sections its
                                          owner keeps: sections by identity, each opened and
                                          shut alone, every one shut by one act, one at most
                                          peeking; kept across a population that moves until
                                          it no longer holds them
                            TreeRow{row, depth, branch}, shown_tree, tree_parent
                                          a tree's shown rows, from each row's depth in
                                          depth-first order: a row shows while every branch
                                          above it is open; a branch is a row whose next row
                                          stands deeper; a row's parent is the nearest
                                          shallower row before it
```

**A peek is the open seen early, and nothing keeps it.** While the pointer rests on a shut
section's header its body shows over what follows, taking none of the room, so nothing moves
under the hand; a press then opens what was already showing. A press that shuts a section under
the resting pointer does not bring the peek straight back: it waits for the pointer to leave and
come again. Where the fold is kept is the owner's -- a word in its layout's row, its own state, or
nothing. The group keeps none of it: it is made over the owner's list of open sections, reads it
there and writes it there, so an edit to the owner's state is what the next fold reads, and a key
written twice or out of order still reads as open. Where the pointer is is the owner's to tell
it: the component holds what follows from both and no medium, so the same section serves rows of
characters and a drawn picture. The group here is the independent one: opening a section shuts
none of the others. A tree is the same fold at every level: its branches are sections, a level
open beside its siblings, and the owner says which rows are open; `shown_tree` says which rows
show, each with its depth and whether it holds rows of its own.

Each owns arithmetic and a value, and nothing else: no Loom, no medium, no policy.

**Availability drawn on a face is a hint, never permission.** `pack_controls` places an
unavailable control as readily as an available one, and the operation asks its own question
again when the press arrives: a weaver who aims at a control is owed the reason it will not run,
and the pane that owns the operation is the only party that can give one.

```text
component/text.hpp       is_continuation_byte / character_before / character_after
                         character_boundary / character_boundary_at_or_after
                                          what a CHARACTER is, in this application
                         word_before / word_after
                                          what a WORD is: space-delimited runs, nothing more
                         pasteable_line   what foreign bytes become in a one-line box
                         Clipboard        text a weaver copied + a writes counter + a
                                          paste_requests counter; the OWNER holds it, the
                                          component only operates on it
                         key:: / mod::    the editing vocabulary's identities -- the same
                                          numbers input::scan::/mod:: name, spelled locally
                                          because this package includes nothing (pinned
                                          against the wire in the input suite)
                         TextPlaces{anchor, caret}: begin / end / has_selection / settle
                                          a selection as two places in a text
                         selected_text / copy_selection
                                          the text between them as written, and its copy

component/text_box.hpp   TextBox          text + caret + anchor + first_visible, as one state
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
                         TextBoxView, text_box_view(box, columns)
                                          what `columns` characters show of the box: its slice,
                                          the caret's column and the selected columns, from one
                                          window
                         text_box_press(box, column, extend)
                                          a press on any column of the box, its blank room
                                          included, placing or extending the caret

component/text_area.hpp  TextLook{role, hang}
                                          how a paragraph reads: the owner's number for its
                                          look, the columns its further lines stand in
                         TextArea         text + looks + TextPlaces + the scroll's place
                           set / clear / text / places / top / within / look
                           lines(columns) -> Line{begin, end, paragraph, indent}
                           line_of / place_in_line
                           last_first / first_line / scroll_lines / scroll_home
                                                             by whole lines
                           offset / scroll_by / scroll_to / furthest
                                                             in a window, in kUnitsPerLine
                           press / drag_to / select_word_at / select_all / clear_selection
                           copy(Clipboard) / consume(scancode, modifiers, Clipboard) -> bool
                           shown_line(line, index, columns) -> ShownLine{text, look, line,
                                                             selection_begin, selection_end}
```

**A text area keeps places, never rows or pixels.** Its text is long and read only: a paragraph
is a line of the text as written, up to its `\n`, and a width wraps it at its last space that fits,
a further line standing in the paragraph's hang. The scroll is the place of the first line shown
and how far into that line a window has scrolled, and the selection is two places, so a text
wrapped again at another width -- a pane made narrower, the same text in a terminal and a window
-- keeps both. A copy is the text between the two places as written: the wrap's breaks and
indents, and the markers a row of cells shows, were never in the text. A row of cells shows the
lines from the scroll's between markers for those hidden above and below, and a window draws the
same value scrolled freely with a bar: both are faces (`graphics/elements/text_face.hpp`). The area answers Ctrl+C and Ctrl+A
itself; the scroll's keys are the owner's, which names its own actions.

**This package knows no medium, and each look is a face.** A text piece is measured here in
characters, as both media lay a row out: what a width shows (`text_box_view`, `TextArea::lines`,
`shown_line`), where its caret and selection stand, which place a press means (`text_box_press`,
`place_in_line`). How it looks -- a box's `[text___]` in a row of cells or its outline where the
window sets type, an area's markers or its bar -- is a face beside the others in
`graphics/elements/text_face.hpp`, each with the geometry its owner reads a press with. No face
keeps anything: the caret, the selection and the scroll are the value's.

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

**The ordinary expectations are mechanics, not omissions.** A selection is the
anchor and the caret (`anchor == caret` *is* "no selection"); typing replaces it, Shift-movement
extends it, plain movement collapses it, and both ends stay on character boundaries always. The
clipboard operations move text through a `Clipboard` the **owner** holds — where its text goes
beyond this process (a platform clipboard through a window, a bus publication, nowhere) is the
owner's custody. That custody includes the paste's *value*: `consume`'s Ctrl+V
records a request (`Clipboard::paste_requests`) rather than pasting, because the value a paste
means is the clipboard's **current** one and only the owner can obtain it — read on the
weaver's intent, never mirrored from watching — and the owner applies it through `paste` once
it holds the text. `draft_epoch()` is the companion counter, bumped by `set`/`clear` (the two
draft doors), so an owner whose acquisition crosses a turn can tell the draft that asked from
whatever draft is standing when the answer arrives. Undo is a bounded local snapshot history
that dies with the draft: `set` and `clear` wipe it, so a new draft can never resurrect an old
one's text; contiguous same-kind keystrokes coalesce into one entry. And
`consume(scancode, modifiers, clip)` is the one owner of the editing-key vocabulary, under the
press chain's own bool: *true* = mine, stop routing; *false* = not my vocabulary, yours.
Declining is `default:`, not knowledge — Return, Escape, Tab, `^s` and every application chord
ever invented come back `false` from a switch that was never edited, so no consumer keeps its
own copy of the mapping.

Four things are structural rather than promised:

- **The operations are the only door.** `0 <= first_visible <= caret <= size()`, both indices on
  a character boundary, holds after every mutator because `settle()` runs at the end of each
  one. There is no `fix_it()` to call and no way to reach a state that would need it. The other
  half — `caret - first_visible <= N` and `first_visible <= max(0, size() - N)` — needs to know
  how much room there is, so it is `keep_caret_visible(N)`, which a consumer calls once per
  repaint with the capacity it resolved.
- **The capacity is an argument and never a member.** Two lines in the same running
  application are different widths, so a component that remembered one of them would be
  remembering the wrong one for the other.
- **It owns no policy and no medium.** No SDL, no terminal, no cell, no pixel, no font metric,
  no commit, no validation, no refusal, no parse, no completion, no submission, no focus, no
  blink and no drawing. What a draft *means* is the consumer's, which is exactly what lets one
  implementation serve tools whose commit models have nothing in common: one submits a line to
  a participant, another parses it, writes it, and may be refused with a reason.
- **It is not an entity.** No identity, no registry, no persistence, nothing to clean up. A
  pane's command line or a draft's field holds one as a member; destroying the owner destroys
  it.

`zengine-component` links **nothing** — not even `loom::core`, which every other package here
needs for `zen/weave/shape.hpp`. A TextBox has no wire form, nothing serializes it and nothing
hosts it, and the absence of that link is the enforcement of "a component is not content".

What it is **not**: a widget set. There is no Button, List, Dropdown, ScrollView, focus tree,
tab order, multiline editing or theme, and none of them will arrive because a toolkit is expected
to have one. Selection, the clipboard operations and a local undo are here under the same rule:
an editing surface several tools share is one its weavers expect to select, copy and undo, and a
text box that could not would be a surprising component, not a smaller one.
