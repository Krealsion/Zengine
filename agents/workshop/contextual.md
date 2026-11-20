# Workshop law — the contextual surface

Register `WL-CTX`: what can I do with this? One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

Retired: WL-CTX-03, WL-CTX-04, WL-CTX-05, WL-CTX-06, WL-CTX-08

## WL-CTX-01 — Pointing names a subject for one request; selection is a state a weaver entered

LAW — Opening captures a subject — a `PaneRef`, a layout position, or nothing; never a rectangle, row or handle — and changes no selection, candidate or focus; spend re-asks the owner.

MEANS
- right press: the pointed subject; `workshop.context` (`a`): the room, all command mode names;
- a ref outside the setup gets one truthful absence sentence, not a geometry refusal;
- ⭐ an object id was a subject, and its load the aliasing door, until the object canvas retired.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-02 — Arrange is the one exception

LAW — Arrange binds the scope and selects the subject, and only after its explicit target passes admission (`enter_arrange_pane` → the target-taking `arrange_geometry_ready`).

DOES NOT MEAN
- that any other contextual action selects, binds or focuses — Arrange is the one exception.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-07 — Spending is one seam per subject kind, and paint is not policy

LAW — One seam per subject kind — `spend_pane_action`, the position-taking layout doors, the room's zero-target owners — and the owner refuses at spend; paint is not policy.

MEANS
- no owner predicate runs on the paint path: the menu renders an identity, not an existence;
- ⭐ an object's seam, `delete_object_at`, retired with the object canvas and its draft guard.

WHY — `agents/decisions/pointing-is-not-selection.md`

## Proven by

### WL-CTX-01
`workshop/context.hpp` ContextMenu context_subject
`workshop/keymap.hpp` workshop.context
`workshop/weave_pointer.cpp` spend_context_choice open_context_at open_context_ambient
`workshop/panes.hpp` Panes::selected
`tests/test_workshop_host.cpp` "a right press captures a subject and selects nothing" "a captured
pane that left the setup is refused truthfully" "the keyboard door opens on what command mode can
name: the room"

### WL-CTX-02
`workshop/weave_arrange.cpp` enter_arrange_pane arrange_geometry_ready
`workshop/weave_pointer.cpp` spend_context_choice
`tests/test_workshop_host.cpp` "contextual Arrange admission precedes binding"
`tests/test_workshop_screen.cpp` "contextual Arrange lifts the pane it addressed, not the one in
front" "every pane a weaver can point at can be arranged, and the refusals are blind"

### WL-CTX-07
`workshop/weave_arrange.cpp` spend_pane_action
`workshop/weave_session.cpp` open_layout_rename duplicate_layout shift_layout drop_layout
`workshop/weave_pointer.cpp` spend_context_choice
`tests/test_workshop_host.cpp` "a contextual action acts on the pointed pane, not the selection" "a
contextual remove removes the pointed pane" "Move Left and Move Right reorder from the tab that was
pointed at"
