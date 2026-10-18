# Workshop law — a guest that reads and points by door

Register `WL-GUIDE`: what a guests-file row of version 3 says -- a role, `guide`, `keystrokes`, and
the `capture` and `guide` doors Loom holds -- what a row that reads by pane reads, what no guest reads
on any host, and a guest's guide mark: how it is asked, where it never stands, how it is drawn and
how it ends. The file's form is public in [Drive Workshop from another
host](../../external-host/docs/external-host.md). One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-GUIDE-01 — Version 3 adds guide, keystrokes, a role and the doors, and a row's reach is said in full

LAW — A version-3 file may name a row's `role`, the powers `guide`, `keystrokes` and `settings`, and its `capture` and `guide` doors; a role expands to its powers at launch, its reach said beside it.

MEANS
- a door is `<office>/<pane>`, `<office>/*` or `*` alone; a guide door lies within a capture door;
- a field needs its power; a power without its field reaches no pane; no door names Connections or Hotkeys;
- the role, its powers and the doors are said at launch, on Attention, in Connections and to the row.

DOES NOT MEAN
- that a role implies what it does not list: `tutor` is `capture` and `guide`, and nothing else.

WHY — `agents/decisions/a-guest-reads-and-points-by-door.md`

## WL-GUIDE-02 — A row that reads by pane reads only the panes Loom says its doors hold

LAW — A row of version 3 whose `capture` is not `*` reads a pane's words only where Loom says its `capture` doors hold that pane, asked at the read; another pane is its place, catalog name and holder.

MEANS
- the doors are door rules on `DeskReadRequested` v2, so a later grant widens the read with no relaunch;
- it is granted the desk read, the newest pages, the inventory, the keymap and its offices' manuals;
- it hears no band, status, desk number or `DeskStamps`, no older read, no Skin picture or desk publication.

WHY — `agents/decisions/a-guest-reads-and-points-by-door.md`

## WL-GUIDE-03 — No guest reads what only the weaver's hand reads, nor hears the weaver type

LAW — On every host every guest is refused the words of Connections, the Hotkeys pane and an editor showing the guests file, every observation of the connections, and the weaver's input without `keystrokes`.

MEANS
- the weaver's input is `AttributedInput`, `KeyPressed` and `TextEntered`, from any office;
- a refusal names the pane and why, never what it shows.

DOES NOT MEAN
- that the whole picture is withheld: a row that reads every pane still sees their pixels.

WHY — `agents/decisions/a-guest-reads-and-points-by-door.md`

## WL-GUIDE-04 — A mark names what a press names, and is judged by doors before anything shows

LAW — `GuideRequested` names a pane and a control's action id, drawn as `control:<id>`, or a subject `part:<name>`; Workshop judges the row's `guide` doors and its read before it looks at the pane.

MEANS
- a pane outside the doors is refused in the same words whether the part shows or not;
- a control with no part, a row above every pane included, is answered not shown with the weaver's key;
- an empty control clears the session's mark.

WHY — `agents/decisions/a-guest-reads-and-points-by-door.md`

## WL-GUIDE-05 — No mark stands where only the weaver's hand reaches

LAW — A mark is refused on Connections, the Hotkeys pane and any part whose press opens either, on a pane another guest already marks, past 16 marks, and on words past 60 bytes of printable ASCII.

MEANS
- a part opens a pane by its subject (`pane:<office>/<pane>`) or by a cause `to <office>/<pane>`;
- one mark a session: a newer one replaces it.

WHY — `agents/decisions/a-guest-reads-and-points-by-door.md`

## WL-GUIDE-06 — A mark is drawn inside its pane in the guide role, and ends when its part does

LAW — A mark is drawn in `role::kGuide` inside its pane's body, never over another word; it ends at the weaver's press inside it, a settled picture without its part, a new holder, or its session's end.

MEANS
- in a window an outline one grain wide; in a terminal the part's own cells, none beside them;
- its label stands where it covers no word or part, a terminal row wholly free, or is not drawn: said;
- an ended mark is told to its guest, why, and never comes back on whatever later bears the name.

DOES NOT MEAN
- that a guest's press ends one: only the weaver's, and the press still reaches its pane.

WHY — `agents/decisions/a-guest-reads-and-points-by-door.md`

## Do not assume

- That a mark is a cover: a reading of its pane is the same with and without it.
- That a Skin that does not know the guide role refuses a mark: it paints it as plain fill.
- That a Files entry naming the guests file is refused a mark: Workshop sees the entry's name, not its path.

## Proven by

### WL-GUIDE-01
`workshop/guests.cpp` read_guests_file door_problem within scopes_of grant_for
`workshop/guests.hpp` kPowerGuide kPowerKeystrokes kPowerSettings kDoorsVersion RoleBundle role_bundles GuestRow::by_door
GuestRow::capture GuestRow::guide
`workshop/attention.hpp` guest_row_scopes
`workshop/guest_door.hpp` on(GuestRowDescribedRequested) on(GuestConnectionsRequested) inventory_v3
`external-host/connections-pane/pane.cpp` scopes_text
`tests/test_workshop_guests.cpp` "guests file: version 3 adds guide, keystrokes, a role and the capture and
guide doors, and a file of an earlier version saying one is refused" "guests file: a door names a pane, a
guide door lies within a capture door, every pane stands alone, and no door names a pane only the weaver's
hand reads" "guests file: the tutor role is capture and guide, a row adds to it, and its reach is said in
full beside it" "door: through the real door, a session of version 3 hears its own role and doors, and the
inventory says each row's beside it"
`tests/test_workshop_panes_attention.cpp` "the Connections pane says a row's version, its losses and
its role and doors beneath its connection"

### WL-GUIDE-02
`workshop/guests.cpp` grant_for observation_of
`workshop/weave_guest_read.cpp` reads_by_pane read_withheld withheld_stamp withhold_desk
`workshop/weave_desk_read.cpp` answer_page
`workshop/weave_desktop.cpp` on(KeymapRequested)
`tests/test_workshop_guests.cpp` "grant: a row that reads by pane is granted the newest reads, the keymap,
the inventory and its doors' manuals as Loom door rules, and never the older reads or the Skin's picture"
`tests/test_workshop_panes_guide.cpp` "a row that reads by pane reads the Builder's words and the Editor's
place, never the Editor's words, and hears the keymap of its own panes and the rows above every pane" "a
pane's manual is answered to a guest under its capture doors, and another office's manual is refused at the
bus"

### WL-GUIDE-03
`workshop/guests.cpp` carries_the_keys observation_of
`workshop/actor_scope.hpp` weaver_only_pane
`workshop/weave_guest_read.cpp` read_withheld
`tests/test_workshop_guests.cpp` "observation: the weaver's input is heard only with keystrokes, no guest
observes the connections, and a row that reads by pane is refused what carries every pane"
`tests/test_workshop_panes_guide.cpp` "every guest is refused the words of the Hotkeys pane and of an editor
showing the guests file, whatever it reads"

### WL-GUIDE-04
`workshop/guide_vocabulary.hpp` GuideRequested GuideAnswered kGuidePartPrefix
`workshop/weave_guide.cpp` on(GuideRequested)
`tests/test_workshop_panes_guide.cpp` "a mark beyond the row's guide doors is refused in the same words
whether its part shows or not, and a row without guide is refused at the bus" "a control drawn as no part is
answered not shown, with the weaver's key for it"

### WL-GUIDE-05
`workshop/weave_guide.cpp` on(GuideRequested)
`workshop/guide_vocabulary.hpp` kGuideWordsBytes kGuideMarks
`tests/test_workshop_panes_guide.cpp` "a second guest's mark on a marked pane is refused, and no mark stands
on Connections, the Hotkeys pane or a part whose press opens either"

### WL-GUIDE-06
`workshop/weave_guide.cpp` guide_drawing guide_layer guide_pressed end_guide
`surface/vocabulary.hpp` kGuide
`tests/test_workshop_panes_guide.cpp` "the Tutor's walk on a weaver's host, in a terminal and in a window: it
reads the Builder, marks [build], the weaver's press inside the mark ends it and builds, the desk moves, and
the next mark stands" "a mark in a terminal is its part's own cells in the guide role: every row of the pane
reads as before, and its label stands on a body row wholly free or is not drawn, the guest told which" "a
mark in a window is an outline one pixel wide around its part inside the pane's body, in the guide role's ink
as the window's plan draws it, and its label names the guest" "a mark's part reads the same with the mark as
without it, and a mark is no cover" "a mark ends, and never comes back, when its part leaves a settled
picture, and its guest is told why" "a mark ends when its guest's session ends, and a guest's own press
refused at the bus never ends one"
