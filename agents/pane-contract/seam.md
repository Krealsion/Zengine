# Pane contract law — the seam

Register `PANE-SEAM`: what a pane and its host owe each other at the seam -- the offer, the area,
the keys, a declared row's id, a menu's answer and the quit's answer. One law per heading; cite by
ID. Router: [`../pane-contract.md`](../pane-contract.md).

## PANE-SEAM-01 — A pane is offered by its role, and judged whole under it

LAW — A pane offers itself as the role it is held under, on the host's `PaneCatalogRequested` and on its arrival; the host judges the offer whole under that stamped role, keyed by role and pane.

MEANS
- an offer with no stamped role is refused, and retains nothing;
- two roles offering one pane key are two panes, and neither can move the other.

DOES NOT MEAN
- that a pane names its host's code: it names the host's role, and Loom delivers to its holder.

WHY — `agents/decisions/the-catalog-is-one-list.md`

## PANE-SEAM-02 — A pane draws only in its own area

LAW — A pane draws its picture in its own area of the window, `<role>.<pane>`, in the room the window gives it; the host places the area and draws none of it, and the window cuts it at the area's edge.

MEANS
- the area's name is the pane's to know before any host says where it stands (`pane_area`);
- a room is the window's to give, and a new room starts the drawing afresh.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## PANE-SEAM-03 — The keys cross as two shapes, and the host says where they are

LAW — The pane holding the keys is sent `PaneKey` and `PaneTextInput`, and `PaneKeys` whenever the keys come or go; it activates a row on a press only when it was told the keys were already its own.

MEANS
- the host never asks a pane whether it wants keys: Loom's gate refuses them to a pane that takes none;
- a mode that holds the keys is "not here".

WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## PANE-SEAM-04 — A declared row arrives as its id, never as its key

LAW — While a pane holds the keys, a gesture bound to one of the rows it declared reaches it as `PaneActionRequested` naming the row's id, in place of the key; anything else crosses as the key.

MEANS
- a pane acts on the id and never re-derives a binding it cannot see;
- a pane built against the first published version still declares and is dispatched.

DOES NOT MEAN
- that declaring rows points the keyboard at a pane: a press is still the only way it gets them.

WHY — `agents/decisions/a-pane-declares-its-actions.md`

## PANE-SEAM-05 — A menu's answer is read through the requester's own record

LAW — A pane reads a menu's answer through its record of the ask (`pane_menu::Asked`): a choice counts from the presenter's role, a refusal from it or the host's, under its own number, once.

MEANS
- the record lives in the image, never in reload-kept state: a reloaded requester cancels its predecessor's menus;
- the subject is the pane's to judge against what it holds when the answer comes.

WHY — `agents/decisions/a-menu-is-presented-by-a-participant.md`

## PANE-SEAM-06 — A pane that takes the quit question answers it

LAW — The host publishes `PaneQuitRequested` before it ends; a pane that accepts it answers `PaneQuitAnswered` about the instant it answers, refusing in words while its work is unsaved.

MEANS
- the host counts the accepters Loom delivered to and waits for their answers;
- a pane permits the quit once nothing it holds would be lost.

WHY — `agents/decisions/a-quit-that-cannot-be-asked-is-refused.md`

## Do not assume

- That a law here is Workshop's: what Workshop does as the host -- its catalog's bound, its keymap,
  its presenter's grant, its desk -- is in its own registers (WL-CAT-03, WL-FOCUS-04, WL-KEY-15,
  WL-CTX-10, WL-SESSION-19), and a second host owes none of it.
- That the settings, approval and carry halves are not the contract's: they are, and their prose
  is [`../panes.md`](../panes.md) until they are laws here.

## Proven by

### PANE-SEAM-01
`pane/vocabulary.hpp` PaneCatalogRequested PaneOffered
`workshop/setup_reconcile.hpp` admit_pane_offer
`tests/test_workshop_panes_seam.cpp` "a valid offer is admitted under the role that stamped it"
"an offer with no stamped role is refused, and retains nothing" "two roles offering one pane key
stay two panes, and neither can move the other"

### PANE-SEAM-02
`pane/drawing.hpp` pane_area WindowDrawing
`pane/canvas_drawing.hpp` canvas_sketch canvas_window_room
`tests/test_workshop_panes_canvas.cpp` "pane canvas text clipping preserves surviving positions
through both edges"
`tests/test_workshop_desk.cpp` "a picture of as many parts as it may name gives each a point a
press there gives it, or none"

### PANE-SEAM-03
`pane/vocabulary.hpp` PaneKey PaneTextInput PaneKeys
`pane/facets.hpp` RowPress::keys_went_here PaneRowsFacet
`tests/test_workshop_panes_files.cpp` "a press on Files' selected row opens it when the keys were
already Files', whether Workshop's title, Files' own header or an arrow left them there" "a press
selects, and a second press on the same row activates"

### PANE-SEAM-04
`pane/vocabulary.hpp` PaneActionRow PaneActions PaneActionRequested
`tests/test_workshop_panes_actions.cpp` "a declared gesture arrives as the resolved id and an
undeclared one as the key; typing still crosses raw" "a pane built against the published version
one still registers, declares and dispatches"

### PANE-SEAM-05
`pane/menu.hpp` Asked Asked::take Offer
`tests/test_workshop_panes_button.cpp` "WL-CTX-10: a requester's record of its ask settles on the
presenter's answer or the host's refusal -- once, and on nothing another role says under its
number"
`tests/test_workshop_panes_desktop.cpp` "WL-CTX-10: the desktop acts only on the presenter's answer
to an ask of this image's own -- not the host's choice, not another subject, not another number,
not a predecessor's, and not twice"

### PANE-SEAM-06
`pane/vocabulary.hpp` PaneQuitRequested PaneQuitAnswered
`tests/test_workshop_panes_editor.cpp` "an orderly quit refuses while source is unsaved, and
proceeds once it is not"
`tests/test_workshop_neovim.cpp` "the orderly quit is refused while Neovim holds unsaved changes,
naming the file, and permitted once written"
`tests/test_flow_pane.cpp` "loaded Flow pane retains unconfirmed field text and selection on reload
and refuses quit"
