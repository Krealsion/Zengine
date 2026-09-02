# Workshop law — the text box

Register `WL-TEXT`: the editable line as a window onto its text, and editing as a component
that belongs to no consumer. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-TEXT-01 — Editing text is a component, and it belongs to no consumer

LAW — `component::TextBox` owns text, caret, anchor and window as one state, the operations its only door; the Terminal line, a property draft, the name editor and the Composer's fields are four instances.

DOES NOT MEAN
- that it has a focus flag, a filter, a max length, a multiline mode or a blink — it has none;
- that a fifth absence a competent user trips on is a reflex extraction — it gets the same test.

PROVEN BY — `component/text_box.hpp` `TextBox`, `first_visible`, `caret`;
`workshop/property.hpp` `Row`; `workshop/screen.hpp` `TerminalPane`; `tests/test_component.cpp`
case `"component: a
TextBox is a value with no identity and no policy"`; `tests/test_workshop_document.cpp` case
`"TEXT-0: the property draft speaks the same vocabulary and keeps its policy keys"`, case
`"TEXT-0: the name editor selects with the same keys and says it in characters"`, case `"TEXT-0:
the real Composer's fields speak the vocabulary across the seam"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-TEXT-02 — A consumer owns the capacity, the clipboard's custody and what the text means

LAW — The capacity is an argument; where the prose begins, the `Clipboard` and its custody beyond the process, and what the text means are the consumer's; Return, Escape and Tab are never the component's.

PROVEN BY — `component/text_box.hpp` `consume`, `kEditingVocabulary`; `workshop/weave.hpp`
`terminal_key`; `tests/test_component.cpp` case `"component: the capacity is an argument, so one
box serves two different widths"`, case `"component: consume owns exactly the editing vocabulary
and declines the rest"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-TEXT-03 — The editable line is a window onto its text

LAW — Always, `0 ≤ first_visible ≤ caret ≤ size()` with `first_visible` on a character boundary; after `keep_caret_visible(N)`, `caret - first_visible ≤ N` and `first_visible ≤ max(0, size() - N)`.

MEANS
- the window moves as little as it must and never recentres; only the caret moves it;
- no blank room on the right while text is hidden on the left;
- no hidden-content marker, no scrollbar, no wheel or drag scrolling, no scroll command.

PROVEN BY — `component/text_box.hpp` `first_visible`, `keep_caret_visible`,
`character_boundary_at_or_after`; `tests/test_component.cpp` case `"HD-4: the window is state,
and every operation leaves the caret inside it"`, case `"HD-4: the window never begins inside a
character"`, case `"component: the window moves as little as it must, and never recentres"`,
case `"component: no blank room on the right while text is hidden on the left"`.
WHY — `agents/decisions/the-line-is-a-window.md`

## WL-TEXT-04 — The Terminal's capacity is never guessed

LAW — The Terminal's capacity is `terminal_input_place(sc).columns`, what the painter cuts by and a press is answered against; `refresh_terminal` reconciles the window once per repaint, above `attached`.

MEANS
- a resize needs no path of its own, because a new extent causes a repaint;
- the reconcile stays above the return: a maker can type into a pane with no participant mounted.

PROVEN BY — `workshop/screen.hpp` `terminal_input_place`; `workshop/weave.hpp`
`refresh_terminal`, `attached`, `terminal_key`; `tests/test_workshop_screen.cpp` case `"HD-4: a
long line is shown as a slice, and both media draw the caret against it"`, case `"HD-4: the
window follows the caret across a resize"`, case `"HD-4: a press on a SCROLLED line lands in the
full authored string"`.
WHY — `agents/decisions/the-line-is-a-window.md`

## WL-TEXT-05 — The left edge snaps forwards, the right cut is a byte cut

LAW — The left edge snaps forwards (`character_boundary_at_or_after`), the right-hand cut is a byte cut as `detail::fit`'s is, and `kTerminalCaretCols` reserves one column the line may not use, on both media.

MEANS
- snapping backwards would carry the right edge back and push the caret off its row;
- a caret is between characters, so the one after a full row needs somewhere to be.

PROVEN BY — `component/text_box.hpp` `character_boundary_at_or_after`; `workshop/screen.hpp`
`kTerminalCaretCols`, `terminal_input_place`; `surface/region.hpp` `project_text_regions`;
`tests/test_workshop_screen.cpp` case `"HD-4: clicking a SCROLLED multibyte line snaps exactly as
HD-3's did"`, case `"HD-4: a column and a byte index are inverses THROUGH the window"`;
`tests/test_component.cpp` case `"component: the window never begins inside a character, at any
capacity"`.
WHY — `agents/decisions/the-line-is-a-window.md`

## WL-TEXT-06 — `consume()` is the routing bool at the component boundary

LAW — True is the box's own vocabulary and false is not mine; declining is `default:`, never knowledge of an application chord, and `kEditingVocabulary`'s rows are exactly what `consume` answers true to.

MEANS
- a consumed gesture need not change anything — a copy with nothing selected is consumed;
- a chord carrying Alt or Super is never the box's.

PROVEN BY — `component/text_box.hpp` `consume`, `kEditingVocabulary`, `key`, `mod`;
`tests/test_component.cpp` case `"component: consume owns exactly the editing vocabulary and
declines the rest"`, case `"component: a consumed gesture that changes nothing is still
consumed"`, case `"KEY-0: the editing vocabulary's declaration rows and consume() agree, both
ways"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-TEXT-07 — The history is the draft's and dies with it

LAW — `set`/`clear` wipe the history and bump `draft_epoch()`; same-kind keystrokes coalesce, paste, cut and a replacement are one entry each, the depth is bounded, and no app-wide undo grows from this.

PROVEN BY — `component/text_box.hpp` `set`, `clear`, `draft_epoch`, `kUndoDepth`;
`tests/test_component.cpp` case `"component: set and clear open a fresh draft with no inherited
history"`, case `"component: contiguous typing coalesces into one undo entry"`, case
`"component: the history is bounded and forgets its far past first"`, case `"component: undo
restores text, caret and selection; redo replays it"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-TEXT-08 — `Session::clipboard` is one in-process mirror

LAW — The component writes it on copy/cut, `on(KeyPressed)` notices `writes` once around the chain and publishes `ClipboardCopy`; copies from elsewhere land in it without bumping `writes`; never persisted.

PROVEN BY — `workshop/screen.hpp` `clipboard`; `component/text_box.hpp` `Clipboard`, `writes`;
`surface/vocabulary.hpp` `ClipboardCopy`; `tests/test_workshop_document.cpp` case `"TEXT-0: a
copy is said to the process once, and a heard copy fills the mirror"`; `tests/test_component.cpp`
case `"component: copy, cut and paste move text through the owner's clipboard"`, case
`"component: copy with nothing selected leaves the clipboard alone"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-TEXT-09 — A paste is a conversation, and the answer belongs to the draft that asked

LAW — Ctrl+V bumps `paste_requests`, `paste_owner_now()` names the asker, Workshop asks `kSkinRole` through its own `AskBook`, and the answer is applied only if the same owner holds the same `draft_epoch`.

MEANS
- a paste means the platform clipboard's current value, which only the owner can obtain;
- a property row also needs the same object and label; a `Row::resume` draft keeps its epoch;
- anything else discards the payload whole; the book holds four asks and refuses a fifth.

PROVEN BY — `component/text_box.hpp` `paste_requests`, `paste`, `draft_epoch`;
`workshop/weave.hpp` `paste_owner_now`, `paste_asks_`, `answers_ask`; `workshop/property.hpp`
`paste`, `resume`; `surface/vocabulary.hpp` `ClipboardTextRequested`, `kSkinRole`;
`tests/test_workshop_document.cpp` case `"QR-11: paste reads the platform current, not the mirror
stale"`, case `"QR-11: an answer crossing a draft boundary lands nowhere, and the payload
dies"`, case `"QR-11: the draft that asked keeps its paste across a rebuild in flight"`, case
`"QR-11: an unsolicited ClipboardText enters no box and no mirror"`; `tests/test_component.cpp`
case `"QR-11: paste is a request the owner applies, and set/clear name the draft"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-TEXT-10 — A medium that cannot be read falls back to the mirror

LAW — On a medium that answers `readable=false` the paste falls back to the in-process mirror; an ask with nobody at the Skin's role stays open, bounded by the book, and inserts nothing.

PROVEN BY — `workshop/weave.hpp` `paste_asks_`, `answers_ask`; `surface/vocabulary.hpp`
`readable`;
`tests/test_workshop_document.cpp` case `"QR-11: with nobody at the skin role, paste inserts
nothing and breaks nothing"`; `tests/test_surface.cpp` case `"QR-11: the terminal medium answers
a clipboard read with its standing truth"`.
WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-TEXT-11 — `zengine-component` links nothing

LAW — A TextBox has no wire form and nothing hosts it; its key identities are spelled locally in `component::key`/`mod` and pinned against `input::scan`/`mod` in the input suite.

PROVEN BY — `component/CMakeLists.txt` `zengine-component`; `component/text_box.hpp` `key`,
`mod`;
`tests/test_input.cpp` case `"TEXT-0: the component's key spellings ARE the wire's"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-TEXT-12 — A word has one definition and three compositions

LAW — `word_run_begin`/`word_run_end` are the maximal non-space run from a position; `word_before`/`word_after` add the separator walk a keyboard means; `word_at` is both scans meeting, a pointer's meaning.

MEANS
- a position with separators on both sides is in no word, and nothing invents the nearest one;
- `select_word_at` is `place`'s other answer to one press: anchor at the start, caret at the end.

PROVEN BY — `component/text_box.hpp` `word_run_begin`, `word_run_end`, `word_before`,
`word_after`, `word_at`, `WordSpan`, `select_word_at`; `tests/test_component.cpp` case `"WUX-7:
one run definition, and the keyboard's two answers are composed from it"`, case `"WUX-7: the
word at a position, including at both of its edges"`, case `"WUX-7: select_word_at opens the
selection across the word a press landed in"`, case `"WUX-7: pointer and keyboard agree about
which bytes are one word"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-TEXT-13 — The one-measurer family takes the box

LAW — `terminal_caret_column`/`terminal_caret_of_column` take the `TextBox`, `visible_selection(columns)` is the only span arithmetic, and the selection helpers add exactly the caret helpers' prose offset.

MEANS
- `pasteable_line` flattens foreign bytes into one line, for the byte=column grid's sake.

PROVEN BY — `workshop/screen.hpp` `terminal_caret_column`, `terminal_caret_of_column`,
`terminal_selection_columns`, `property_selection_columns`; `component/text_box.hpp`
`visible_selection`, `pasteable_line`; `tests/test_workshop_screen.cpp` case `"caret geometry: a
byte index and a prose column are one number, both ways"`; `tests/test_component.cpp` case
`"component: the visible selection is the span both media may spend"`, case `"component: paste
flattens foreign bytes into one line"`.
WHY — `agents/decisions/the-line-is-a-window.md`

## WL-TEXT-14 — A text-selection drag is a gesture record of its own

LAW — `Session::text_drag` holds which editable line a press began sweeping and nothing else; every motion re-resolves the current geometry through the press's own functions and hands the component a column.

MEANS
- the row is not re-tested mid-drag, so a hand that wanders off the line keeps sweeping it;
- a press begins it only on the paths that consume the press; release keeps the selection.

PROVEN BY — `workshop/screen.hpp` `text_drag`, `TextDrag`, `terminal_value_column`,
`property_value_column`; `component/text_box.hpp` `drag_to_column`;
`tests/test_workshop_document.cpp` case `"TEXT-0: a drag sweeps a selection on the terminal line,
and release keeps it"`, case `"TEXT-0: a drag sweeps a selection on the property draft through
its own row"`; `tests/test_component.cpp` case `"component: drag_to_column extends from the
pressed anchor and can leave the slice"`.
WHY — `agents/decisions/one-press-one-gesture.md`
