# Workshop law — the text box

Register `WL-TEXT`: the editable line as a window onto its text, and editing as a component
that belongs to no consumer. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

Retired: WL-TEXT-14, WL-TEXT-03, WL-TEXT-04, WL-TEXT-05, WL-TEXT-06, WL-TEXT-07, WL-TEXT-10, WL-TEXT-12, WL-TEXT-13

## WL-TEXT-01 — Editing text is a component, and it belongs to no consumer

LAW — `component::TextBox` owns text, caret, anchor and window as one state, the operations its only door; the name editor, a Composer field and a pane's line are instances.

DOES NOT MEAN
- that it has a focus flag, a filter, a max length, a multiline mode or a blink — it has none.

WHY — `agents/decisions/editing-text-is-a-component.md`

## WL-TEXT-02 — A consumer owns the capacity, the clipboard's custody and what the text means

LAW — The capacity is an argument; where the prose begins, the `Clipboard` and its custody beyond the process, and what the text means are the consumer's; Return, Escape and Tab are never the component's.

WHY — `agents/decisions/editing-text-is-a-component.md`

## WL-TEXT-08 — `Session::clipboard` is one in-process mirror

LAW — The mirror is written on copy and cut, said to the process once around the chain, and filled by copies heard from elsewhere, not counted as writes; never persisted, watching no system clipboard.

WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-TEXT-09 — A paste is a conversation, and the answer belongs to the draft that asked

LAW — A paste is a request: the component counts it, whoever owns the line names the asking draft and asks the medium's role, and the answer lands only if that same draft is still standing.

MEANS
- a paste means the platform clipboard's current value, which only the owner can obtain;
- anything else discards the payload whole; the book holds four asks and refuses a fifth.

DOES NOT MEAN — that the check belongs to this host. `TextBox::draft_epoch` is the boundary made
comparable, and a line that left for a pane's own image asks and checks for itself.

DOES NOT MEAN — that any insertion door will do. `TextBox::paste` is the one that applies a
payload: one structural undo entry, and `pasteable_line` before it. `type` is a keystroke.

WHY — `agents/decisions/a-paste-is-a-conversation.md`

## WL-TEXT-11 — `zengine-component` links nothing

LAW — A text box has no wire form and nothing hosts it: the component library links nothing, and its key identities are spelled locally and pinned against the input wire's spellings in the input suite.

WHY — `agents/decisions/editing-text-is-a-component.md`

## Proven by

### WL-TEXT-01
`component/text_box.hpp` TextBox TextBox::first_visible TextBox::caret
`workshop/live_desk.hpp` LayoutNaming
`tests/test_component.cpp` "component: a TextBox is a value with no identity and no policy"
`tests/test_workshop_document.cpp` "the name editor selects with the same keys and says it in
characters" "the real Composer's fields speak the vocabulary across the seam"

### WL-TEXT-02
`component/text_box.hpp` TextBox::consume kEditingVocabulary
`workshop/weave_session.cpp` naming_key
`tests/test_component.cpp` "component: the capacity is an argument, so one box serves two different
widths" "component: consume owns exactly the editing vocabulary and declines the rest"

### WL-TEXT-08
`workshop/screen.hpp` Session::clipboard
`component/text.hpp` Clipboard Clipboard::writes copy_selection
`component/text_box.hpp` TextBox::copy
`graphics/vocabulary.hpp` ClipboardCopy
`workshop/weave_pointer.cpp` on(ClipboardCopy)
`tests/test_component.cpp` "component: copy, cut and paste move text through the owner's clipboard"
"component: copy with nothing selected leaves the clipboard alone"

### WL-TEXT-09
`component/text.hpp` Clipboard::paste_requests
`component/text_box.hpp` TextBox::paste TextBox::draft_epoch
`workshop/weave_seam.cpp` paste_owner_now naming_line begin_clipboard_paste
`workshop/weave.hpp` WorkshopWeave::paste_asks_ AskBook PasteOwner PendingPaste
`workshop/weave_pointer.cpp` answers_ask on(ClipboardText)
`terminal/pane.cpp` begin_paste on(ClipboardText) Paste
`files/files.cpp` paste_epoch_
`info/pane.cpp` begin_paste on(ClipboardText) Paste::epoch
`graphics/vocabulary.hpp` ClipboardTextRequested
`tests/test_workshop_document.cpp` "paste reads the platform current, not the mirror stale" "an
unsolicited ClipboardText enters no box and no mirror"
`tests/test_component.cpp` "paste is a request the owner applies, and set/clear name the draft"
"component: a paste is its own undo entry, however much typing preceded it"
`tests/test_workshop_panes_terminal.cpp` "clipboard text lands in the draft that asked for it, or
nowhere" "an edit is not a new draft, and a submit is" "a paste is one gesture, and undo gives back
the line it landed in" "what the clipboard holds is normalized to fit a line, or refused aloud"
`tests/test_workshop_panes_info.cpp` "a clipboard answer asked for by an Info draft that has closed
lands in no later draft, and one asked for by the draft still standing lands in it"

### WL-TEXT-11
`component/CMakeLists.txt` zengine-component
`component/text.hpp` key mod
`tests/test_input.cpp` "the component's key spellings ARE the wire's"
