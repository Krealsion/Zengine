# Workshop law — arrangement, the snap

Register `WL-ARR`, its second file: the snap a hand's place and size meet. One law per heading;
cite by ID. The register's first file is [`arrangement.md`](arrangement.md). Router:
[`../workshop.md`](../workshop.md).

## WL-ARR-17 — A hand's place and size come to the edges near them; a key places exactly

LAW — An edge a hand moves comes to the nearest legal line within `kPaneSnapReachPx`, the room's or another pane's on the screen, before the gesture door, unless Alt is held; keys and typed values never snap.

MEANS
- the line met is marked across the room on the affordance plane, a device unit wide, while held;
- every motion snaps afresh from the press, so a snap leaves a place and a size and nothing else;
- a hand within reach of the room's edge comes to it; one beyond is refused as WL-ARR-06 says.

DOES NOT MEAN
- collision avoidance or an anchor: a snapped pane may stand over another, and nothing follows it.

PROVEN BY — `workshop/screen.hpp` `kPaneSnapReachPx`, `PaneSnapLines`, `SnappedWindow`,
`PaneGesture::met_x`, `PaneGesture::met_y`; `workshop/screen_gestures.cpp` `pane_snap_lines`,
`snap_pane_window`, `snap_axis`; `workshop/weave_arrange.cpp` `arrange_motion`, `arrange_status`;
`workshop/weave_pointer.cpp` `on(PointerMoved)`; `workshop/screen_reveal.cpp`
`paint_pane_affordances`; `tests/test_workshop_screen.cpp` case `"a pane dragged near another
pane's edge or the room's comes to it, and the edge met is marked across the room while held"`,
case `"Alt held while dragging sets every snap aside, and the hand places to the pixel"`, case
`"an edge pulled near another pane's edge or the room's comes to it, its opposite edge held"`,
case `"the arrow keys and a typed value place exactly, near an edge or not"`, case `"in a
terminal a hand moving by cells meets an edge between them, and the mark is a cell across the
room"`.
WHY — `agents/decisions/a-hand-snaps-a-key-places.md`

## Do not assume

- That a snap is saved, anchors a pane or keeps two panes apart — it leaves a place and a size,
  and a pane may stand over another (WL-ARR-17).
