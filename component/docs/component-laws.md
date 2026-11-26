# The text box's laws (WL-TEXT)

**Laws.** The numbered rules kept about the editable line, `component::TextBox`: by the component,
by every pane and host that holds one, and by Workshop, which mirrors the clipboard and asks a
window for a paste. Each is a rule that more than one file relies on, and a comment or a page that
relies on one names it by its number; a rule about one declaration is in that declaration's
comment. What each piece does is [the Component package](component.md)'s. The controls a pane draws
over `control_strip.hpp`, `list_window.hpp` and `row_map.hpp` keep Workshop's laws, routed by
[Workshop's router](../../workshop/AGENTS.md). The files and cases that prove each law here are
listed in [`tests/laws/component.txt`](../../tests/laws/component.txt), which the `law_register`
check reads.

## WL-TEXT-01 — Editing text is a component, and it belongs to no consumer

`component::TextBox` owns the text, the caret, the anchor and the window onto them as one state,
and its operations are the only way to change it. Workshop's name editor, a Composer field and a
pane's line are each an instance of it.

What it does not mean: that it has a focus flag, a filter, a maximum length, a multiline mode or a
blink. It has none of them.

Why: [editing text is a component](../../agents/decisions/editing-text-is-a-component.md).

## WL-TEXT-02 — A consumer owns the capacity, the clipboard's custody and what the text means

The capacity is an argument. Where the prose begins, the `Clipboard` and its custody beyond the
process, and what the text means are the consumer's. Return, Escape and Tab are never the
component's.

Why: [editing text is a component](../../agents/decisions/editing-text-is-a-component.md).

## WL-TEXT-08 — `Session::clipboard` is one in-process mirror

Workshop's mirror of the clipboard is written on a copy and a cut and said to the process once
around the chain, and filled by copies heard from elsewhere, which do not count as writes. It is
never written to a file, and it watches no system clipboard.

Why: [a paste is a conversation](../../agents/decisions/a-paste-is-a-conversation.md).

## WL-TEXT-09 — A paste is a conversation, and the answer belongs to the draft that asked

A paste is a request. The component counts it; whoever owns the line names the draft that asks
and asks the medium's role; and the answer lands only if that same draft still stands.

What this means:

- A paste means the platform clipboard's current value, which only the owner can obtain.
- Anything else drops the payload whole. The book of asks holds four, and refuses a fifth.

What it does not mean:

- That the check belongs to Workshop. `TextBox::draft_epoch` is the boundary made comparable, and
  a line that moved to a pane's own image asks and checks for itself.
- That any way of inserting text will do. `TextBox::paste` is the one that applies a payload: one
  undo entry of its own, with `pasteable_line` before it. `type` is a keystroke.

Why: [a paste is a conversation](../../agents/decisions/a-paste-is-a-conversation.md).

## WL-TEXT-11 — `zengine-component` links nothing

A text box has no wire form and nothing hosts it: the component library links nothing, and its key
identities are spelled where it stands and pinned against the input wire's spellings in the
`input` suite.

Why: [editing text is a component](../../agents/decisions/editing-text-is-a-component.md).
