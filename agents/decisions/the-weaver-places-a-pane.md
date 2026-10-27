# The weaver places a pane

**Decision record.** One decision, its alternatives, and why this one. Not a how-to -- the laws it
supports are in [desktop](../workshop/desktop.md) and [arrangement](../workshop/arrangement.md).

**Context.** A pane seated on the desk took the default place, the stack at the room's top left,
so every pane a weaver opened stood on the last one, and a hidden pane came back there whatever
the weaver had made of it. Arranging was a mode a weaver had to know and could leave without
seeing that it ended.

**Decision.** A pane a layout never had opens at the room's centre, or at the hand where
`workshop.open-at` says `mouse`, and an open that brings the keys places it at once: its title
and edges take the hand, so the weaver puts it where it belongs, while its keys and a press in
its body stay its own, the first of either ending the placing. A pane taken off a layout leaves its place and size with the
layouts' owner, by the layout's id (`SetupState::left`), and a seat there gives them back
(`LayoutEditAnswered::returned`), so no second arrangement follows. The palette opens centred
every time and is never arranged. A manual opens over the pane it describes, a margin of it
showing round it, and is not placed by hand. While arranging, the room's top says so in words, the pane it
arranges or `all panes` beneath them, and every other pane fades.

**Alternatives considered.**
- *Seating a pane in free room, beside the one that opened it* -- argued: the desk would guess,
  and a guess that covers what the weaver works in is the pile again; the weaver places it.
- *Keeping the place in the setup file and the session* -- argued: a new format for both and
  their conversions; a run's memory serves the open and close that hurt, and the file can follow.
- *Arranging as `w` arranges, owning the keys until Escape* -- tried: a weaver who opened the
  Terminal and typed `d` hid it; and most of the suite, which opens a pane and types into it,
  went red.
- *Fading by a property of each pane's area* -- argued: the window would gain a field; a veil in
  Workshop's own area, or its overlay, needs none.

**Consequences.** A relaunch opens a pane the last run closed as a first open. A terminal shows
the words and no fade: it draws an image only where it is mostly opaque.

**Laws supported.** [WL-ARR-18](../workshop/arrangement.md), [WL-DESK-19](../workshop/desktop.md),
[WL-DESK-20](../workshop/desktop.md).
