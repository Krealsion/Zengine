# zengine.workshop -- Workshop

**Manual.** Workshop's own weave, answering to the office `zengine.workshop`: the host every pane
is presented in. It holds the desk -- which panes are on it, where each stands and in what order,
the room each is granted, where the keys go and what a press reaches -- and the files that bring a
desk back. Workshop mounts it in its own process; it is not a file you load. A pane's office and
every other weave reach it by the messages below, which are the pane protocol: what a pane, a
presenter, a skin or an input producer says to Workshop, and what Workshop does with it.

## About

A pane is a presentation an office offers; **the office authors the pane, and Workshop grants the
room.** Workshop keys every pane by the office Loom stamped on the offer and the pane's own key, so
no payload names its provider. It decides where a pane stands and how much room it gets, draws its
chrome and the band, routes each key, character, press and wheel to the one place it belongs, and
tells the pane what reached it; the pane decides what its rows mean. Nothing a pane says grants it
anything: what Workshop may say is its own grant (`workshop_grant`,
[`workshop/grant.cpp`](../grant.cpp)), checked at every send, and holding the office is no
super-grant.

Workshop judges the form of everything a pane sends and keeps nothing it refused: an update that
does not fit is refused whole and said where the weaver reads, and the last good picture stands.
Silence is never `unavailable`: Workshop learns of no provider's unloading, so it shows the last
rows an office reported and waits. Closing a pane destroys only Workshop's copy; the office, its
weave and its state are untouched.

The window, the room, the setup and the canvas a pane draws on are [the pane protocol's
reference](workshop-panes.md); where a reported pointer lands is [pointer
spaces](pointer-spaces.md). Loom's own doors are Loom's: Workshop hears `zen.DispatchRefused` to
learn that a drop, a press, a menu sentence or an open it sent was not delivered, and says so on
the band.

## Use

A weave joins the desk as a pane in five steps, each a message sent as its office
(`mail.as_role(office)`, never personal speech, which Workshop ignores):

1. **Offer.** When the surface is ready Workshop publishes `PaneCatalogRequested`; answer it, and
   your own activation, with [`PaneOffered`](#paneoffered). Repeating it refreshes in place.
2. **Declare.** Say the rows a weaver's keys reach with [`PaneActions`](#paneactions) and the
   settings you take with [`PaneSettingsDeclared`](#panesettingsdeclared).
3. **Be granted a room.** When the desk seats your pane -- a setup naming it, the Pane Manager's
   launch, your own [`PaneRevealRequested`](#panerevealrequested) -- Workshop hands you
   `PaneSettings` (version 2, with the row's revision, when your holder accepts it), then a
   `PaneCanvasRoom` if your holder accepts the canvas doors, and
   `PaneRoom{pane, rows, columns}`. A new room comes whenever the room changes.
4. **Draw.** Send [`PaneContent`](#panecontent) rows inside the room, or a
   [`PaneCanvasContent`](#panecanvascontent) picture under the grant, and
   [`PaneCaret`](#panecaret) where an insertion point stands.
5. **Hear presses.** Workshop sends the pane a press as `PanePressed` (or `PaneCanvasPointer` on a
   canvas), keys and text as `PaneKey` and `PaneTextInput` while the keys are yours, a declared
   row as `PaneActionRequested` with its id, the second button as `PaneButton`, the wheel as
   `PaneWheel`. Each carries a correlation; continue it with a menu
   ([`PaneMenuRequested`](#panemenurequested)) or ask before an act
   ([`PaneOperationRequested`](#paneoperationrequested)).

A walk through all five is [Make a Workshop tool](make-a-workshop-tool.md).

Workshop's own settings are office-wide, one value for every layout, read and written by message
([`OfficeSettingsRequested`](#officesettingsrequested)) and kept in its settings file.

## Commands

### `PaneLaunchRequested`

`PaneLaunchRequested` asks Workshop to show one pane and give it the keys: open it, or focus it if
it is open. It never closes anything ([`desktop_seam_vocabulary.hpp`](../desktop_seam_vocabulary.hpp)).

- **Takes:** `office` and `pane`.
- **Answers:** `PaneLaunchAnswered{office, pane, opened, focused, refusal}`, to the asker alone,
  after the desk has the pane, selected and holding the keys if it takes them; the band says
  `showed <name>`, or that it stands off this screen.
- **Refuses:** in `refusal`, said on the band too: ``no pane `<pane>` from `<office>` -- nothing has
  offered it, and a launch loads nothing``; while the run still loads it, ``<name> is not here yet
  -- this run is still loading `<office>`; launch it again once it has arrived``; ``<name> is not
  available -- `<office>` is not offering it in this Workshop``; ``<name> is not available --
  nothing holds `<office>` now; its artifact has to be loaded again (build it, or relaunch)``.
- **Example:** the Pane Manager's open sends `{office: "zengine.builder-pane", pane: "builder"}`.

### `PaneCloseRequested`

`PaneCloseRequested` asks Workshop to take one pane's row off the desk. It unloads nothing.

- **Takes:** `office` and `pane`.
- **Answers:** `PaneCloseAnswered{office, pane, closed, refusal}`, to the asker alone; the band says
  `hid <name> -- its provider and what it holds are untouched; showing it brings it back`, naming
  any settings the row kept that went with it.
- **Refuses:** `<name> is not on this desk -- nothing to hide, and a hide shows nothing`.
- **Example:** the Pane Manager's hide sends the row it stands on.

### `PaneToggleRequested`

`PaneToggleRequested` asks Workshop to show a pane that is not on the desk, or hide one that is,
judged against the desk at the ask.

- **Takes:** `office` and `pane`.
- **Answers:** `PaneToggleAnswered{office, pane, opened, closed, refusal}`, to the asker alone, with
  the band's `showed <name>` or `hid <name> -- ...`.
- **Refuses:** each refusal [`PaneLaunchRequested`](#panelaunchrequested) words.
- **Example:** a desktop row bound to one pane sends it on every press.

### `DeselectRequested`

`DeselectRequested` asks Workshop to let go of the selected pane, continuing the application row
that asked it.

- **Takes:** nothing; it rides the correlation of the `AppActionRequested` Workshop sent, and only
  `zengine.desktop` asks.
- **Answers:** nothing; the band says `unselected <name>`.
- **Refuses:** nothing, in words: an ask whose act is no longer its hand's latest is dropped.
- **Example:** the desktop's last meaning for a bare Escape over a pane that handed it back.

### `PaneInventoryRequested`

`PaneInventoryRequested` asks for every pane this Workshop could show.

- **Takes:** nothing.
- **Answers:** `PaneInventory`, to the asker alone: each pane's office, key, name and summary, and
  whether it is `open`, `available` or `pending` (still to come from the plan). It is the reading
  Workshop also publishes ([Follow](#follow)).
- **Refuses:** nothing in words: personal speech is answered by nobody; a guest whose row holds
  `capture` is answered.
- **Example:** a Pane Manager that arrives asks once, then follows the publication.

### `KeymapRequested`

`KeymapRequested` asks for the keymap in force.

- **Takes:** nothing.
- **Answers:** `KeymapShown`, to the asker alone: the keymap file and what became of it, and every
  binding with its address (`office`, `pane`, `id`), its key as a weaver spells it, its `causes`
  and `enters`, and each pane's modes. A guest reading pane by pane is not told the rows of a pane
  it may not read.
- **Refuses:** nothing in words, as `PaneInventoryRequested`.
- **Example:** the Hotkeys pane asks when it opens.

### `KeymapEditRequested`

`KeymapEditRequested` asks Workshop to change one action's keys, and write the keymap file.

- **Takes:** `action`; `op`, one of the `keymap_edit` operations (set, add, remove, disable, reset,
  and the spelled set, add and remove); `scancode` and `modifiers`, or `text` for a spelled one.
- **Answers:** `KeymapEditAnswered{action, accepted, refusal, applied, written, file_refusal,
  sentence}`, to the asker alone, after the whole keymap is judged again; then `KeymapShown` is
  published. A file that changed on disk since it was read is not written, and the edit stands for
  this run: ``the keymap file changed since this Workshop read it -- not written; relaunch to read the
  file, then edit again``, or `isolated run -- no keymap file; the change stands for this run`.
- **Refuses:** with `sentence` `` `<action>` unchanged -- <why> ``: ``no action `<action>` is in force in
  this Workshop``; ``the keymap file was refused at launch (...) -- fix the file, launch again, then
  edit``; `that key is not one this keymap can name -- type its spelling instead`; `` `none` is not a
  key -- disable the action, or reset it ``; `` `<key>` already requests `<action>` ``; `` `<key>` does
  not request `<action>` -- nothing to remove ``; and the keymap's own words for a collision,
  `` `<key>` is authored for both `<a>` and `<b>`, which can be active together -- one of them must
  move ``.
- **Example:** the Hotkeys pane's rebind sends the action, a spelled set, and the key as the
  weaver typed it.

### `PaneRevealRequested`

`PaneRevealRequested` asks Workshop to show the asker's own pane and give it the keys
([`pane_vocabulary.hpp`](../pane_vocabulary.hpp)).

- **Takes:** `pane`, a pane the asking office offered.
- **Answers:** `PaneRevealAnswered{pane, seated, refusal}`, to the asker alone; the band says
  `showing <name> -- it asked to be shown, and it has the keys`, or that it stands off this screen.
- **Refuses:** `defect: <name> was not seated`; a pane the office never offered is ignored.
- **Example:** a pane that finished a long job asks to be shown.

### `SetupApplyRequested`

`SetupApplyRequested` asks Workshop to put a whole desk live ([`setup_control.hpp`](../setup_control.hpp)).

- **Takes:** `setup`, the text of a setup file ([Setups](setups.md)).
- **Answers:** `loom::Ack`, after the desk is replaced and seated and the band says `applied setup
  <name>`; the layout is no longer related to a setup file. No file is written.
- **Refuses:** in `loom::Refused`: `close the current menu before applying a setup`; `setup names a
  pane this Workshop cannot present: <office> <pane>`; and the setup reader's words, such as
  `not a Workshop setup: ...` or `setup version <n> -- this Workshop reads versions ...`.
- **Example:** a guest with the `demo` power puts a prepared desk live.

### `WorkshopQuitRequested`

`WorkshopQuitRequested` asks Workshop to end, as a weaver's quit does.

- **Takes:** nothing.
- **Answers:** Workshop publishes `PaneQuitRequested` and waits for every pane to answer
  [`PaneQuitAnswered`](#panequitanswered); then `loom::Ack`, writes the last session and stops.
  Input that arrives meanwhile is held, and replayed if the quit is refused.
- **Refuses:** in `loom::Refused`: `a quit already asked is still waiting for its answer`; a
  guest's on a weaver's host, `Workshop stays open -- <why>; quitting writes the last session`;
  `Workshop could not ask its panes whether they hold unsaved work -- it does not hold its own
  office; Workshop stays open`; and each pane's own refusal, joined.
- **Example:** an agent's run ends Workshop with it.

### `PaneViewRequested`

`PaneViewRequested` v7 asks for a page of one pane's reading: its words and named parts where the
weaver sees them ([`pane_view.hpp`](../pane_view.hpp)).

- **Takes:** `provider`, `pane`; `from`, the page's first item; `stamp`, the reading it continues
  (an empty stamp at `from` 0 reads the pane as it stands).
- **Answers:** `PaneView` v7, to the asker alone: its `PaneStamp` -- holder, incarnation, the
  window's incarnation, and the fingerprint of what the pane shows; the place it shows in and the
  text measure; its words, then its parts, each with its place, the point a press reaches it at
  and a part's `available`; what covers it, left out and counted; `in_flight`; `from` and `total`.
  A pane that draws in an area of its own is read from the window that shows it, which Workshop
  asks first, so the answer may come a delivery or two later; one that draws in no window is read
  from what Workshop draws of it. The guest `capture` power grants it.
- **Refuses:** in `loom::Refused`: `pane view unavailable: closed or unknown`, `...: the pane has
  drawn nothing yet`, `...: the pane is not presented`, `...: the pane is not placed in a window`,
  `...: the window refused its reading: <why>`, `...: Workshop refused the pane's last update and
  kept none of it`, `...: waiting for the provider -- no participant holds its office`, `pane view
  stale: ...; read it again from the start`, and a wait's refusals ([`DeskReadRequested`](#deskreadrequested)).
  A guest hears `pane view withheld: <why>`.
- **Example:** read from 0, then the next page under the stamp the first answered.

### `PanePointRequested`

`PanePointRequested` v4 asks where to press one character of one word of a pane's reading now.

- **Takes:** `provider`, `pane`, the `fingerprint` of the reading you read, a `word` by its number
  and a `column`, a character of it.
- **Answers:** `PanePoint` v3, to the asker alone: the centre of that character, in the input
  space the medium reads. A point is not a gesture: pressing it is ordinary input, which a window
  lands as it lands the weaver's.
- **Refuses:** as [`PaneViewRequested`](#paneviewrequested) refuses the pane; `pane point
  unavailable: what the pane shows moved; read it again`; `...: outside the pane's visible words`;
  `...: no press reaches that word now`; `...: that character is not addressable`.
- **Example:** after a reading at fingerprint 41, ask `{word: 2, column: 4, fingerprint: 41}`, then
  press there.

### `DeskReadRequested`

`DeskReadRequested` v3 asks for the whole desk in one turn.

- **Takes:** nothing.
- **Answers:** `DeskRead` v3, to the asker alone: the desk as Workshop holds it -- its number, the
  extent, the unit and input space, the room, every pane the desk names with its state word, rank
  and places, arranging, the menu with its named lines, and the band's words -- then every
  presented pane's stamp from the front back, and as many of their readings as one decoded value
  and one reply hold, each whole or not at all; a stamp without a reading names a pane to ask
  alone. It waits for the window's reading of every pane that draws in an area of its own. Then
  `DeskStamps` is published if the desk moved ([Follow](#follow)).
- **Refuses:** in `loom::Refused`: past a few reads a second from one asker
  (`kDeskReadsPerSecond`, [`weave.hpp`](../weave.hpp)), `desk read refused: at most <n> a second,
  each asker -- read again in <ms> ms`; past the reads that may wait on the window at once
  (`kMaxDeskWaits`), `desk read refused: <n> reads wait on the window already; read again`; `desk
  read unavailable: the window that shows the desk started again; read again`, `...: did not
  answer in time` (`kDeskWaitMs`), or `...: cannot be asked what area '<area>' shows`. A pane a
  guest may not read is stamped and not read.
- **Example:** an agent reads the desk once, then pages each pane whose reading did not fit.

### `PaneOperationRequested`

`PaneOperationRequested` asks whether the gesture a pane was just handed may make one operation:
the approval a pane asks before an act a hand caused ([`pane_operation.hpp`](../pane_operation.hpp)).

- **Takes:** `pane`; `gesture`, the correlation of the press, action, menu choice or shortcut that
  reached it; `role`, `shape` and `version`, the send it would make. v2 adds `classes`, each of
  `build`, `write` and `open` the act is, and `subject`, what an `open` opens, and may name no send.
- **Answers:** `PaneOperationAnswered{allowed, reason}`, to the asker alone. The gesture is spent
  once, while it is still its own hand's latest act; a guest's live Loom authority is asked for
  the send, and its classes against the row that admitted it ([whose host this
  is](../../external-host/docs/external-host.md#whose-host-this-is-and-what-each-power-reaches-there)). It authorizes the intent
  at that check: it is no grant, and the pane sends under its own. Physical input is the weaver's
  hand, injected input the session that injected it. Ask once, at the act's first beat, and carry
  the approval to its later sends; an act that learns its subject later asks at the beat that
  learns it, under the first beat's gesture. An ask naming no send spends its act's record while
  that record is unspent, whatever its hand did since.
- **Refuses:** `the requesting pane is no longer on this desk`; `this operation needs a current
  attributed input gesture`; `the operation must name its destination and versioned shape`; `the
  input actor has no authority for this operation`; a guest's class, `` guest '<name>' may not
  build here: its row has no `build`, ... ``, `this is a weaver's host: only the weaver's hand writes
  a file here, ...`, `this is a weaver's host: the guests file opens only by the weaver's hand`,
  `this Workshop knows no action class '<class>'`.
- **Example:** on `PaneActionRequested` under correlation 12, ask v2 `{pane: "builder", gesture:
  12, classes: ["build"]}`, then build.

### `ActorScopeRequested`

`ActorScopeRequested` asks what one sender may do, for a message an owner received directly, past
any gesture.

- **Takes:** `session`, the sender's weave; `pane`; `classes`; `id`, the act.
- **Answers:** `ActorScopeJudged{allowed, refusal}`, to the asker alone: a guest session judged as a
  gesture's actor is, from the row its door admitted; any other participant is not narrowed.
  Nothing is remembered. A pane that relays a message as its own office -- the Composer's Submit
  -- is not the guest: the relayed send is judged at its approval, as the classes its shape is
  (`scope::classes_of_send`, [`actor_scope.hpp`](../actor_scope.hpp)).
- **Refuses:** `the question names no action class`, and each class refusal
  [`PaneOperationRequested`](#paneoperationrequested) words.
- **Example:** the Inventory pane asks it before a toolbox save a guest sent it.

### `PaneObservationRequested`

`PaneObservationRequested` asks for a lease: one gesture approving repeated reads of one shape at
one role, about one subject.

- **Takes:** `pane`, `role`, `shape`, `version`, `gesture`, and `subject`, the words the pane names
  what it watches by.
- **Answers:** `PaneObservationAnswered{allowed, reason, lease}`, to the asker alone, judged and
  spent as [`PaneOperationRequested`](#paneoperationrequested) is. A pane holds one lease, a new
  one replacing it, in a bounded book (`kMaxObservationLeases`, [`weave.hpp`](../weave.hpp)). A
  lease is no grant: each read goes under the pane's own. Leases live in the running Workshop
  only, and no timer, invalidation or old correlation makes one.
- **Refuses:** as `PaneOperationRequested`; `the observation must name its subject`; `too many
  observations are active; pause one first`; with `lease` 0.
- **Example:** an [Info view](../../info/docs/zengine.info.md#infovaluewatch) watching an Inventory entry
  asks on the press that opened it.

### `PaneObservationContinued`

`PaneObservationContinued` asks, before each read under a lease, whether it still stands.

- **Takes:** `pane`, `lease` and `subject`.
- **Answers:** `PaneObservationAnswered`, to the asker alone, the same lease while it stands. Only
  the lease's holder, still holding the office that offered the pane, continues it.
- **Refuses:** `this observation is not current; start it again with a gesture`; and, forgetting
  the lease: `the observing pane's office changed hands`, `the observation's subject changed; start
  it again`, `the observing pane is no longer on the desk`, `the actor who started this observation
  has left or lost its authority`.
- **Example:** each refresh of a watched entry asks first, and stops on a refusal.

### `PaneObservationEnded`

`PaneObservationEnded` ends a lease.

- **Takes:** `pane` and `lease`; lease 0 ends every lease the sender holds on that pane.
- **Answers:** nothing.
- **Refuses:** nothing: only the holder's own ending removes its lease.
- **Example:** ending is the pane's duty on every exit: one that pauses, closes or changes subject
  ends its lease, one that stops before the answer arrives ends the lease that answer grants, and
  an image arriving after a reload, which keeps its WeaveId, sends `{pane, lease: 0}` for each pane
  it may observe from.

### `PaneCarryRequested`

`PaneCarryRequested` asks Workshop to hold a reference the hand picks up, continuing an acquisition
[`PaneOperationRequested`](#paneoperationrequested) approved ([`pane_carry.hpp`](../pane_carry.hpp)).

- **Takes:** `pane`, `label` and `data`, bounded by `kMaxCarryBytes` and `kMaxCarryLabelBytes`;
  under the approved gesture's correlation.
- **Answers:** `PaneCarryAnswered{carried, reason}`, to the asker alone; the band says `Carrying
  <label>`, then a dash and `click a receiving pane; Escape cancels`. The carrier's click on a pane that accepts it
  sends `PaneDrop` (rows) or `PaneCanvasDrop` (a canvas) under a new correlation; elsewhere the
  band says `This place does not accept the carried item; Escape cancels`, and it stays held.
  Escape puts it down; another hand cannot place or cancel it, and a guest's is released at the
  next attributed input once that guest has left. A placement Loom refuses is said with its
  attempt, `Reference not delivered to <office> (attempt <n>): <why>`, and never retried. Workshop
  reads none of the bytes; a sent drop is not a finished operation, and the receiver owns its
  meaning and authorizes its own writes.
- **Refuses:** `the approved acquisition is no longer current`; `the carried reference exceeds its
  limits`; `another item is already being carried`; a guest's, on a weaver's host, `this is a
  weaver's host: a guest's carried item rests in no pane, so guest '<name>' carries nothing here`.
- **Example:** Inventory approves its press, then carries an `InventoryReference` to Info.

### `PaneValueCarryRequested`

`PaneValueCarryRequested` asks Workshop to carry a copy of a value.

- **Takes:** `pane`, `label`, `data`, and `drag`: true places it where the primary release lands,
  false waits for a click. v2 adds `token`, the source's own, which Workshop stamps into
  `v2::PaneValueDrop` and `PaneCanvasValueDrop` with the source office and pane.
- **Answers:** `PaneCarryAnswered`, to the asker alone, as
  [`PaneCarryRequested`](#panecarryrequested); the drop is `PaneValueDrop`, or v2's to a receiver
  that accepts it, or `PaneCanvasValueDrop` on a canvas. A value drag starts at the source's
  primary press, becomes a drag once the hand moves, and ends at that same hand's release; a click
  that never moved selects and carries nothing. A release that comes before the carry is answered
  is kept with its place and aimed picture; that hand's next key, press, text or wheel makes it
  stale, a receiver gone cancels it, and a release over a place that takes no value cancels the
  drag. A drag begun on a canvas ends that press's hold as lost. An empty `token` is a copy, and a
  receiver infers no move from the data; the host grants the v2 drop beside v1 explicitly.
- **Refuses:** as `PaneCarryRequested`; `this value drag no longer has its primary press`; v2 `the
  transfer token needs 1..128 bytes`.
- **Example:** approve the press, ask `{drag: true}`, and the weaver lets go over Info.

### `PaneShortcutInvoked`

`PaneShortcutInvoked` asks Workshop to send a pane the action a global shortcut names
([`pane_shortcuts.hpp`](../pane_shortcuts.hpp)).

- **Takes:** `office`, `pane`, `action`, and `holder`, the weave that registered the shortcut;
  under the correlation of the `AppActionRequested` it continues. Only `zengine.desktop` asks.
- **Answers:** `PaneActionRequested{pane, action}` to that office under a new correlation, which the
  pane may ask an operation under; nothing to the asker. It reaches a pane that is not shown.
  Neither a registration nor an old binding carries authority: the pane's next
  `PaneOperationRequested` is judged for the hand that pressed the shortcut.
- **Refuses:** on the band: `Shortcut unavailable: its pane, action or provider has changed`;
  `Shortcut invocation could not be queued`; a guest's toward a place only the weaver's hand
  reaches.
- **Example:** the desktop continues a pane's global binding to Inventory.

### `PaneMenuRequested`

`PaneMenuRequested` asks Workshop to present the pane's own menu, continuing the gesture it was
handed -- a secondary press, a declared action, or a primary press.

- **Takes:** `pane`, `subject` (the pane's own word, echoed unread), `row` and `column` where it
  opens, and `rows`, each `PaneMenuRow{id, label}`; under the gesture's correlation.
- **Answers:** Workshop grants the menu to whoever holds `zengine.presenter`, which shows it with
  the host's standard rows beneath and answers the pane `PaneMenuAnswered` under the request's
  number. The gesture is spent once.
- **Refuses:** Workshop itself answers `PaneMenuAnswered`, unchosen, with `refusal`: `a menu
  continues a gesture, and this request echoes none`; `the pane is not on the desk`; `late -- the
  weaver acted since that gesture, or it was already spent`; ``no presenter holds
  `zengine.presenter` -- nothing can present this menu``; and, ending a menu its presenter could not
  carry: `the presenter left -- <why>`, `the presenter was replaced`, `the presenter could not be
  reached`, `the presenter could not answer it -- <why>`.
- **Example:** `pane_menu::Offer(pane, subject).at(row, col).row(id, label).send(mail, office)`
  ([`pane_menu.hpp`](../pane_menu.hpp)).

### `PanePassRequested`

`PanePassRequested` hands a secondary press back: that press was not the pane's.

- **Takes:** `pane`; under the press's correlation.
- **Answers:** nothing to the asker: Workshop opens its own menu for the pane where the press
  landed, once, while that press is still its hand's latest act.
- **Refuses:** nothing in words: a stale, spent or foreign number moves nothing.
- **Example:** a pane with nothing to offer under the pointer calls `pane_menu::pass_back`.

### `PaneManageRequested`

`PaneManageRequested` asks for Workshop's own pane menu on a pane, continuing a menu choice.

- **Takes:** `pane`, the asker's; `office` and `target`, the pane to manage; under the choice's
  correlation.
- **Answers:** nothing to the asker: Workshop opens its pane menu for that pane where the chosen
  menu stood, once.
- **Refuses:** on the band, ``no pane `<office>/<target>` in this Workshop -- nothing to manage``; a
  stale choice moves nothing.
- **Example:** the Pane Manager's `manage` row continues to `pane_menu::manage`.

### `PaneKeyboardRequested`

`PaneKeyboardRequested` asks for the keys, continuing a menu choice.

- **Takes:** `pane`; under the choice's correlation, or the one `take_keyboard_continuing` keeps.
- **Answers:** nothing: the pane is selected and holds the keys, if it takes them.
- **Refuses:** nothing in words.
- **Example:** a menu's rename row asks the keys so its naming line takes the typing.

### `GuideRequested`

`GuideRequested` asks Workshop to mark one part of a pane for the weaver to see: a guest's pointer
([`guide_vocabulary.hpp`](../guide_vocabulary.hpp)).

- **Takes:** `pane` as `<office>/<pane>`; `control`, an action id or `part:<name>`, or empty to
  clear the mark; `words`, its label, at most `kGuideWordsBytes` of printable ASCII.
- **Answers:** `GuideAnswered{shown, said}`, to the asker alone: `marked <part> in <pane> for the
  weaver, as guest '<name>'; ...`, or `cleared`; a mark stands while its pane shows the part.
- **Refuses:** in `said`: `only a guest a guest door admitted draws a guide mark`; `refused: '<pane>'
  is not a pane; a mark names <office>/<pane>`; `` refused: pane <pane> is outside guest '<name>''s
  `guide` doors ``; `refused: a press of <part> opens a pane only the weaver's hand reaches: ...`;
  `refused: guest '<name>' marks that pane already; one guest mark a pane`; `not shown: <pane>
  shows no part <part> now`; `not shown: <pane> is not on the desk, settled and uncovered, now`.
  A mark on a pane that draws in an area of its own waits for the window's reading of it, and is
  refused as a desk read's wait is.
- **Example:** an agent marks a part the Builder pane names, `part:<name>` in
  `zengine.builder-pane/builder`, with a few words.

### `InspectPaneRequested`

`InspectPaneRequested` asks Workshop to make one pane the inspected subject
([`inspection_seam_vocabulary.hpp`](../inspection_seam_vocabulary.hpp)).

- **Takes:** `office` and `pane`.
- **Answers:** `PaneSubjectActed{accepted, refusal}`, to the asker alone, then publishes
  `PaneSubjectShown`: the pane, its name and its properties, named by a subject number.
- **Refuses:** `<office>/<pane> is in neither this build's vocabulary nor this desk -- nothing to
  inspect`. Only an office asks.
- **Example:** Info inspects the pane its weaver chose.

### `PaneSubjectRequested`

`PaneSubjectRequested` asks for the inspected pane as it stands.

- **Takes:** nothing.
- **Answers:** `PaneSubjectShown`, to the asker alone; `office` and `pane` empty when no pane is
  inspected.
- **Refuses:** nothing; personal speech is answered by nobody.
- **Example:** an inspector that arrives asks once, then follows the publication.

### `PaneCommitRequested`

`PaneCommitRequested` asks Workshop to write one property of the inspected pane: its place, size or
a setting in the layout.

- **Takes:** `subject`, the number its rows were shown under; `row`; `text`, the value typed.
- **Answers:** `PaneSubjectActed`, to the asker alone; the band says `committed <property> of <pane>
  = <value>`, and for a setting how far it got (`stored and handed`, or `stored; not handed: ...`).
- **Refuses:** `the inspected pane, its desk or its rows changed before it arrived, so nothing was
  written`; `that row is not in this pane's properties any more`; `<property> is not authored --
  it is what the screen makes of the authored value`; `<property>: <its setter's words>`.
- **Example:** Info commits a width of `480` to the pane it inspects.

### `PaneSettingsRequested`

`PaneSettingsRequested` asks for one pane's row of the live layout
([`settings_seam_vocabulary.hpp`](../settings_seam_vocabulary.hpp)).

- **Takes:** `provider` and `pane`, the row's address.
- **Answers:** `PaneSettingsShown`, to the asker alone: the row's `revision` (0 when the live layout
  has no such row), the `rows` of the pane's counted declaration with their tags, pins and summaries,
  the `settings` the row keeps, its `pins` in order (the list the pin scope reads), the keys a
  lock holds in `locks`, each `everywhere` or `layout` (this layout alone), and `said`, the band's sentence
  about the row (`stored and handed`, or `stored; not handed: ...`, and what the pane said it
  cannot use).
- **Refuses:** `settings withheld: <why>` in `said`, with nothing read, to an asker that is not an
  office, this Workshop's Terminal or a guest whose row has `settings`.
- **Example:** `ask @zengine.workshop PaneSettingsRequested 1 provider=zengine.terminal
  pane=terminal`, from the Terminal.

### `PaneSettingCommitRequested`

`PaneSettingCommitRequested` asks Workshop to write one setting of a pane's row of the live layout,
at the revision the asker read.

- **Takes:** `provider`, `pane`, `revision`, `key`, and the value in its kind's one field --
  `flag`, `number` or `text` -- or `clear` and no field for the setting's default. Every row takes
  `workshop.skin`, the skin of the pane's graphics area, which the pane is never handed.
- **Answers:** `PaneSettingActed{accepted, refusal, revision, handed}`, to the asker alone: the
  row's revision after the write, or as it stood, and how far the write got. The band says
  `committed <key> of <pane> = <value>; <how far>`, or `cleared <key> of <pane>; <how far>`.
- **Refuses:** `settings commit withheld: <why>`; `<office>/<pane> has no row on the live layout
  -- open it first`; `stale revision <n> -- the row of <office>/<pane> stands at <m>; read it
  again`; `a clear takes no value -- name the key alone`; ``<office>/<pane> has no counted setting
  `<key>` -- a key no declaration takes is only cleared``; `<key> is locked -- nothing writes it
  while the lock stands, and only the weaver unlocks it`, for a lock everywhere or in the live
  layout; `<key> takes <what it takes>, not <the value>`. Each writes nothing.
- **Example:** `ask @zengine.workshop PaneSettingCommitRequested 1 provider=zengine.terminal
  pane=terminal revision=$r1.revision clear=false key=wheel-rows number=6`, from the Terminal.

### `OfficeSettingsRequested`

`OfficeSettingsRequested` asks for one office's office-wide settings
([`settings_seam_vocabulary.hpp`](../settings_seam_vocabulary.hpp)).

- **Takes:** `office`; `zengine.workshop` for Workshop's own.
- **Answers:** `OfficeSettingsShown`, to the asker alone: the `revision` the settings stand at (0
  when nothing declares for the office), the `rows` its declaration gives, the `settings` kept, the
  `locks`, and `written`: `written`, or why the settings file does not hold them.
- **Refuses:** `settings withheld: <why>` in `written`, to an asker the settings doors do not serve.
- **Example:** `ask @zengine.workshop OfficeSettingsRequested 1 office=zengine.workshop`.

Workshop's own, `office=zengine.workshop`:

| setting | takes | default | what it does |
|---|---|---|---|
| `workshop.pane-titles` | `on` or `off` | `on` | draw title text on pane borders |
| `workshop.band-keys` | `short` or `full` | `short` | how the bottom band spells a key |
| `workshop.hotkeys-keys` | `short` or `full` | `full` | how the Hotkeys pane spells a key |
| `workshop.pinned-scope` | `workshop-wide` or `per-layout` | `workshop-wide` | which pin list an editor's pin acts edit |
| `workshop.focus.ring` | `accent` or `plain` | `accent` | the command-mode selection: the accent's two rings, or one in the chrome's ink |
| `workshop.skin` | `classic`, `block`, `day` or `night` | `classic` | each graphics area's skin; a pane's row may keep its own |
| `workshop.completion-order` | `declared` or `required-first` | `declared` | the order the Terminal offers a shape's fields |
| `workshop.arrange-step` | 2 to 16 | 4 | cells one coarse grow or shrink moves |
| `workshop.snap-reach` | 6 to 11 | 8 | canvas pixels a hand's edge snaps across |
| `workshop.side-columns` | 16 to 64 | 28 | the right column's width in cells |
| `workshop.stack-rows` | 5 to 40 | 9 | a stacked pane's height in rows, when it asks none |

### `OfficeSettingCommitRequested`

`OfficeSettingCommitRequested` asks Workshop to write one office-wide setting at the revision the
asker read.

- **Takes:** `office`, `revision`, `key`, and the value in its kind's one field, or `clear`.
- **Answers:** `PaneSettingActed`, its `handed` saying `applied` (Workshop's own) or `handed` (an
  office's holder), then `written` or `not written: <why>`; the band says the same.
- **Refuses:** `nothing declares office-wide settings for `<office>` here`; `stale revision <n>
  -- the settings of <office> stand at <m>; read them again`; `Workshop's settings file <path>
  could not be read at launch and is never written over -- mend or remove it, then relaunch`; a key
  not declared, a lock, or a value the setting does not take, in the words of
  [`PaneSettingCommitRequested`](#panesettingcommitrequested). Each writes nothing.
- **Example:** `ask @zengine.workshop OfficeSettingCommitRequested 1 office=zengine.workshop
  revision=$r1.revision clear=false key=workshop.focus.ring text=plain`.

### `PaneSettingPinRequested`

`PaneSettingPinRequested` edits one pane's pin list: the settings an editor lists first, in order.

- **Takes:** `provider` and `pane`; `key`; `act`, one of `pin`, `unpin`, `up`, `down` or `reset`
  (back to the declared pins, naming no key); `layout`, a layout's place for a per-layout list, -1
  the live one.
- **Answers:** `PaneSettingPinActed{accepted, refusal, pins, scope}`, to the asker alone: the list
  after the act and the scope it is kept in, `workshop-wide` or `per-layout`. The list is kept in
  the settings file and never handed to the pane.
- **Refuses:** `pins withheld: <why>`; ``<office>/<pane> declares no setting `<key>` to pin``;
  `` `<key>` is pinned already ``; `` `<key>` is not pinned ``; a list past `kMaxPinsPerPane`; an
  unreadable settings file. Each writes nothing.
- **Example:** `ask @zengine.workshop PaneSettingPinRequested 1 provider=zengine.terminal
  pane=terminal key=wheel-rows act=pin layout=-1`.

### `PaneSettingLockRequested`

`PaneSettingLockRequested` locks or unlocks one setting, in one layout or everywhere.

- **Takes:** `target`, `<office>/<pane>`, or `<office>` for an office-wide setting; `key`; `act`,
  `lock` or `unlock`; `scope`, `layout` (that layout's row alone) or `everywhere`; `layout`, a
  layout's place, -1 the live one; `gesture`, for an unlock from a pane, the correlation of the
  weaver's current act the pane was sent.
- **Answers:** `PaneSettingLockActed{accepted, refusal, written}`, to the asker alone. A layout's
  lock freezes the value that layout's row holds; `written` names the layout. An everywhere lock on
  a pane pins the value its layout holds, or the default when the row keeps none, writes it into
  every layout's row that seats the pane and gives it to a row seated later; `written` names those
  layouts. An office-wide setting has one value, so its lock holds everywhere. Every write door
  refuses or passes over a locked key. The locks are kept in Workshop's settings file.
- **Refuses:** `lock withheld: <why>`; `` `<act>` is not a lock act -- lock or unlock ``;
  `` `<scope>` is not where a lock holds -- layout or everywhere ``; `unlocking is the weaver's --
  from the Terminal, or from a pane on the weaver's own current act`; a target naming `@`; no layout
  at that place; an unreadable settings file; `an office-wide setting has one value in every layout
  -- lock it everywhere`; a key the target neither declares nor keeps; a pane its layout does not
  seat; locked already, or not locked; a lock everywhere asked to unlock in one layout; a value its
  row's bounds refuse in some layout; `nothing changed: Workshop's settings file was not written --
  <why>`. Each changes nothing.
- **Example:** `ask @zengine.workshop PaneSettingLockRequested 1 target=zengine.terminal/terminal
  key=wheel-rows act=lock scope=everywhere layout=-1 gesture=0`, from the Terminal.

### `LayoutSettingsRequested`

`LayoutSettingsRequested` reads every layout's rows at once.

- **Takes:** nothing.
- **Answers:** `LayoutSettingsShown{revision, active, layouts, declared, everywhere}`, to the asker
  alone: each layout's place, name, baseline theme (`name` and `fingerprint`, a name of "" for
  none) and rows, each row's `settings` and `locked` keys there; what each pane's counted
  declaration takes, `workshop.skin` first, and each office's office-wide rows, with `pane` empty;
  the everywhere locks; the live layout's place; and the `revision` it all stands at, which moves
  with any row, layout, name, declaration, lock or baseline.
- **Refuses:** an empty reading at `revision` 0, to an asker the settings doors do not serve.
- **Example:** `ask @zengine.workshop LayoutSettingsRequested 1`.

### `SettingsWriteRequested`

`SettingsWriteRequested` writes many settings across layouts, and layouts' baselines, at the
revision the asker read.

- **Takes:** `revision`; `writes`, each a `layout` (-1 the live one; not read for an office),
  `target`, `key` and a value in its kind's one field or `clear`; `baselines`, each a `layout` and
  a `theme`, a name of "" removing it; `cause`, what the band says the batch was.
- **Answers:** `SettingsWritten{accepted, refusal, revision, results, handed}`, to the asker alone:
  each write's `outcome` -- `written`, `unchanged`, or passed over as `locked`, `not-taken` (no
  counted declaration takes the key) or `not-in-layout` -- with why; the revision after it; and the
  live rows and offices handed or applied. The baselines are kept in Workshop's settings file.
- **Refuses:** whole, writing nothing: `settings writes withheld: <why>`; `stale revision <n> --
  the layouts stand at <m>; read them again`; a batch past its bound; `write <i>: <why>` for a
  write whose form, target, layout or value is wrong, or whose row or layout a bound refuses;
  `baseline <i>: <why>`; an unreadable settings file; `nothing written: Workshop's settings file
  was not written -- <why>`.
- **Example:** a script applying a theme to every layout reads
  [`LayoutSettingsRequested`](#layoutsettingsrequested), then sends one batch naming the revision
  it read, a write per row the theme names and a baseline per layout.

### `TerminalActRequested`

`TerminalActRequested` asks Workshop to submit a line to its terminal participant
([`terminal_seam_vocabulary.hpp`](../terminal_seam_vocabulary.hpp)).

- **Takes:** `act`, `submit`; `line`, as typed.
- **Answers:** `TerminalActed{accepted, refusal}`, to the asker alone; then the transcript it
  changed is published (`TranscriptShown`).
- **Refuses:** ``this door knows one act, and it is `submit` ``; `no terminal participant is mounted
  on this bus -- nothing was authored`. Only an office asks.
- **Example:** the Terminal pane submits `ask @zengine.builder StatusRequested 1`.

### `TerminalValueRequested`

`TerminalValueRequested` asks for a value the terminal's transcript keeps, to carry it.

- **Takes:** `pane`, `participant`, `observation` (the entry), and `gesture`, judged as
  [`PaneOperationRequested`](#paneoperationrequested)'s is.
- **Answers:** `TerminalValueAnswered{available, reason, label, pair}`, to the asker alone: the
  value and what the transcript says of it, encoded as a pair, within the carry's bound.
- **Refuses:** the operation's refusals; `The source terminal is no longer available`; `This
  transcript value is no longer retained, or is only local text`; `This transcript entry is no
  longer retained`; `This value exceeds the 64 KiB carry limit`; `This value could not be captured:
  <why>`.
- **Example:** the Terminal pane captures an answer it shows, then carries it.

### `TerminalCompletionRequested`

`TerminalCompletionRequested` asks what a terminal line could be completed with.

- **Takes:** `line`.
- **Answers:** `TerminalCompletionOffered{open, slot, partial, heading, candidates}`, to the asker
  alone, `open` false with no terminal participant; read off the bus at the ask, authoring nothing.
- **Refuses:** nothing; personal speech is answered by nobody.
- **Example:** the Terminal pane asks on each keystroke of its line.

### `PresentationTrialRequested`

`PresentationTrialRequested` asks whether a pane would seat, and with what room: the first step of
a managed opening ([`open_seam_vocabulary.hpp`](../open_seam_vocabulary.hpp)).

- **Takes:** `op`, the opening; `provider` and `pane`. Only `zengine.opening` asks.
- **Answers:** `PresentationTrial{op, ok, refusal, rows, columns}`, to the asker alone, measured on a
  copy of the desk; nothing moves. An asker that accepts `v2::PresentationTrial` is answered it,
  with the canvas `room` reserved for a pane that draws its picture.
- **Refuses:** `no pane <office>/<pane> is offered on this desk`; `no room for a row of <name> on
  this screen`.
- **Example:** the opening manager asks before it prepares the Editor's pane.

### `PresentationAdmitRequested`

`PresentationAdmitRequested` hands Workshop the content a trial's pane prepared, to show it.

- **Takes:** `op`, `provider`, `pane`, `generation`; v1 its `rows` and caret; v2 its `picture`, a
  `v5::PaneCanvasContent` under the room the trial reserved.
- **Answers:** `PresentationAdmitted{op, ok, refusal}`, to the asker alone, after Workshop offers
  the presentation it claims; when it is published the pane is seated with the content and the
  keys, and the band says `showing <name> -- it opened <file>, and it has the keys`.
- **Refuses:** `no trial stands for this opening -- try again`; `the room for <name> changed while
  opening -- try again`; `<name>: <why>`, the content judged as any content is, and v2 `no room was
  reserved for its picture`, `its picture names another room`.
- **Example:** the opening manager admits the Editor's first rows.

## Hears

### `PaneOffered`

`PaneOffered`, from a pane's office: it has a pane ([`pane_vocabulary.hpp`](../pane_vocabulary.hpp)).
v1 `{pane, name, summary}`; v2 adds the `rows` and `columns` it likes; v3 a canvas body, `width`,
`height`, `text_rows`. A new key joins the catalog; a known one refreshes its name and summary and
waits for a new room. A refusal is said on the band, never answered: ``a pane's name cannot be
empty``, ``... is a built-in pane``, ``Workshop holds at most <n> panes -- ... was not added``.

### `PaneActions`

`PaneActions`, from a pane's office: the rows its keys reach. v2 adds `supersedes`; v3 `mode`,
`elsewhere`, and each row's `causes` and `enters` ([what a press may
cause](workshop-panes.md#what-a-press-may-cause)). Judged whole with the keymap and answered
`ActionsJudged`, to a holder that accepts it; a refusal is said on the band too, and the rows in
force stand. A later collision withdraws them: `ActionsWithdrawn`.

### `AppActions`

`AppActions`, from `zengine.desktop` alone: the application's rows, above the modes or by default
([`desktop_seam_vocabulary.hpp`](../desktop_seam_vocabulary.hpp)); v2 adds `causes`. Judged
whole, answered `ActionsJudged`; every pane's rows are joined again, a pane that now collides told
`ActionsWithdrawn`. Another office is refused: ``application actions are declared by
`zengine.desktop` -- ...``.

### `PaneSettingsDeclared`

`PaneSettingsDeclared`, from a pane's office: the settings it takes, each a key, a default and what
it takes, and at version 2 its tags, pin and summary ([settings](workshop-panes.md#a-panes-settings)).
Judged whole; a refusal is said on the band, `<name> @<office>: its settings were not taken --
<why>`, and the settings in force stand. It counts while the incarnation that sent it holds the
office.

### `OfficeSettingsDeclared`

`OfficeSettingsDeclared`, from an office: the office-wide settings it takes, judged whole by a
pane declaration's rules and replaced by a later one; it counts while the incarnation that sent it
holds the office. Workshop hands the holder `OfficeSettings` at each revision, when it accepts it.
A refusal is said on the band, `office-wide settings of <office>: refused -- <why>`.

### `PaneSettingUnused`

`PaneSettingUnused`, from a pane's office: a setting it was handed at a revision and cannot use,
and why. Kept while the pane holds that revision, said with its row (`it cannot use <key> --
<why>`) and on Attention; one naming another revision is dropped.

### `PaneContent`

`PaneContent`, from a pane's office: its rows, within the room granted. v2 adds `generation`, an
older one dropped; v3 `picture`, echoed on presses; v4 `parts`; v5 each part's `available`. Too
many rows, a row too wide, or a byte not drawn is refused whole: the body says `(the last update
did not fit this pane's room -- none of it was kept)` and Attention names why. A canvas pane's rows
are ignored.

### `PaneCanvasContent`

`PaneCanvasContent`, from the granted holder: one whole picture under its room grant
([`pane_canvas_vocabulary.hpp`](../pane_canvas_vocabulary.hpp)). v4 adds `parts`, v5 runs on the
text lattice, v6 `available`. A refusal is `PaneCanvasRejected` to the sender, the last good
picture marked `(update refused)`: `canvas room grant is no longer current`, `canvas picture number
must increase within its grant`, a broken budget.

### `PaneCaret`

`PaneCaret`, from a pane's office: where its insertion point and selection stand in the rows it last
sent; v2 adds `generation`. One those rows do not hold leaves the pane with no caret, and the
notice band says why: `Hello @<office>: put a caret on row 2 of a pane showing 2 rows`. Each medium
draws it its own way ([a caret and a selection](workshop-panes.md#a-caret-and-a-selection)).

### `PaneEscapeUnspent`

`PaneEscapeUnspent`, from a pane's office under the correlation the Escape came on: that Escape
was unspent there. While the pane still holds the keys and the Escape is still its hand's latest
act, Workshop answers it as the desk's own: the desktop's row for a bare Escape
([escape](workshop-panes.md#escape-in-a-pane-that-holds-the-keys)).

### `MenuShown`

`MenuShown` v3, from `zengine.presenter`: the open menu is drawn in its area, so many lines of
so many characters, numbered as a picture ([`presenter_vocabulary.hpp`](../presenter_vocabulary.hpp)).
Workshop places the area to fit inside the popup's frame, in front of the desk. A menu past the
granted room, or in an area it was not granted, is withdrawn, its requester told why: `the
presenter showed a line wider than its room`.

### `MenuClosed`

`MenuClosed`, from `zengine.presenter`: the menu is over and its requester answered, chosen at an
act or not. A choice is kept for the pane's next continuation; a standard row chosen is
Workshop's to spend on that pane, while that act is its hand's latest.

### `MenuReturned`

`MenuReturned`, from `zengine.presenter`: it holds no such menu. Workshop ends the menu and answers
its requester itself, unchosen: `the presenter could not answer it -- <why>`.

### `PresenterReady`

`PresenterReady`, from `zengine.presenter`: a holder arrived carrying menu `menu`, or 0. An open
menu it carries stays open under it, its picture numbered afresh; one it does not is ended, its
requester told `the presenter was replaced`.

### `PictureFence`

`PictureFence`, from Workshop to its own office after a canvas is handed out
([`vocabulary.hpp`](../vocabulary.hpp)): when it comes round, the newest picture is the one a press
is stamped with. Only Workshop's office sends it.

### `WithdrawalFence`

`WithdrawalFence`, from Workshop to its own office behind a menu it withdrew: when it comes round,
the withdrawn menu's record is forgotten, every refusal of its sentences delivered before it.

### `SurfaceReady`

`SurfaceReady`, from the skin ([`surface/vocabulary.hpp`](../../surface/vocabulary.hpp)): Workshop
publishes `PaneCatalogRequested`, reads the keymap and preferences, paints its floor, restores the
last session and says its startup notes.

### `SurfaceExtent`

`SurfaceExtent`, from the skin: the room in canvas pixels and the medium's text metric. Workshop
lays the same panes out in it; a new room opens and closes nothing, and each pane whose room moved
is granted a new one.

### `SurfacePlacement`

`SurfacePlacement`, from the skin or a graphics window: where the window sits, and whether it is
maximized. Workshop keeps it, opaque, for the session file, and offers it back at a restore
(`SurfacePlacementRemembered`) to the medium that holds the desk.

### `SurfaceCloseRequested`

`SurfaceCloseRequested`, from the skin or a graphics window: the window was asked to close.
Workshop quits as a weaver's quit does, asking its panes first.

### `ClipboardCopy`

`ClipboardCopy`: text was copied. Workshop keeps it as the clipboard its naming line pastes.

### `ClipboardText`

`ClipboardText`, answering Workshop's `ClipboardTextRequested`, which it asks of the graphics
window that shows its area, else the window that offered itself while no skin has said its
extent, else the skin: the platform's text, pasted into the line that asked, if that draft still
stands.

### `KeyPressed`

`KeyPressed`, from input ([`input/vocabulary.hpp`](../../input/vocabulary.hpp)), or from the
graphics window that shows Workshop's area: resolved once -- the application's rows above every
mode, then a menu, arranging, a naming line, the focused pane, command mode -- and spent there. A
pane holding the keys hears `PaneKey`, or `PaneActionRequested` for a row it declared.

### `TextEntered`

`TextEntered`, from input or from that window: a character goes where a key goes, so a pane
holding the keys hears `PaneTextInput`; the character a binding's key produced is that binding's.
On a weaver's host a guest's text rests nowhere:
`this is a weaver's host: a guest's typed text rests in no pane, ...`.

### `PointerButton`

`PointerButton`, from input: a press reaches the pane its place shows, front first -- `PanePressed`
on rows, `PaneCanvasPointer` on a canvas, `PaneButton` for the second button to a holder that
accepts it, else Workshop's own pane menu. A position in a space Workshop does not know is ignored
([pointer spaces](pointer-spaces.md)).

### `PointerMoved`

`PointerMoved`, from input: carries a held gesture -- an arrangement, a sweep (`PaneDragged`), a
canvas press, a carried value -- and tells a canvas holder that accepts it where an idle pointer
rests (`PaneCanvasHover`).

### `PointerWheel`

`PointerWheel`, from input: the notches go to the pane on top under the pointer, as `PaneWheel` or
a canvas wheel. Nothing hears it while a menu or arranging is open.

### `AttributedInput`

`AttributedInput`, from `zengine.input` alone: an input moment with whose hand it is, the weaver's
or a guest's. Workshop routes it as that hand's; the producer's unattributed copy of it is dropped.

### `PaneQuitAnswered`

`PaneQuitAnswered`, from a pane answering `PaneQuitRequested`: `permitted`, or a `refusal` in its
words. The last answer decides: every pane permitting ends Workshop; any refusal keeps it open, the
band saying each refusal, or `<pane> refused the quit and gave no reason`.

### `QuitDeliveryRefusalNoted`

`QuitDeliveryRefusalNoted`, from the host's watch ([`quit_delivery.hpp`](../quit_delivery.hpp)):
Loom refused the quit question to a pane. The quit is refused: `the quit could not ask <office>
(weave <n>) -- <why>; Workshop stays open, and a quit after the repair asks again`.

### `SourceOpened`

`SourceOpened`, answering a pane's Edit Code open ([`pane_seam_vocabulary.hpp`](../pane_seam_vocabulary.hpp)):
accepted, Workshop publishes `PaneSourceOpened` and says what rebuilding it does; refused, the band
says `<why> -- <name>'s code was not opened`.

### `ManagedOpenSettled`

`ManagedOpenSettled`, from `zengine.opening`: a managed opening ended, committed and applied or
not. Its condition is retracted, a trial it held is let go, and a refusal is said on the band.

### `ManagedOpenProgress`

`ManagedOpenProgress`, from `zengine.opening`: an opening waits. While `pending`, Attention says
`opening <file>`, `waiting for <office> to <stage> -- your work is untouched`.

### `EditorSwitchProgress`

`EditorSwitchProgress`, from `zengine.editor-switch` ([`editor_switch_vocabulary.hpp`](../editor_switch_vocabulary.hpp)):
where a switch of the Editor stands. Attention says it, with the line that confirms or cancels it;
its outcome is said on the band, `editor switch <op>: <outcome>`.

### `GraphicsReady`

`GraphicsReady`, from `zengine.graphics` ([`graphics/vocabulary.hpp`](../../graphics/vocabulary.hpp)):
a directory started afresh. Workshop, its arranger, offers the four skins, chooses each area's
(`workshop.skin`), and places each open pane's frame (`zengine.workshop.pane.<kind>`) at its depth
with the pane's own area over its body in front where its office draws there; its own area,
taking the keys; and `zengine.workshop.overlay` in front, deaf. It asks its rooms.

### `WindowOffered`

`WindowOffered` v3, from the terminal or OS window as its own office (`zengine.window.terminal`,
`zengine.window.sdl`): its pixels, unit and text; Workshop's areas go on it. With no skin up,
Workshop comes up in it; while no skin has said `SurfaceExtent`, it lays the panes out in the
window's room, unit and text, again at each new size, and keeps its room for an offer of none. A
skin's extent then owns the room. An offer from any other office or window is ignored.

### `AreaRoom`

`AreaRoom` v4, from the graphics window that shows one of Workshop's areas, as that window's
office: its room. A frame draws its pane's edge and header and a body its office does not draw; its
own area the band, and every pane's frame while a weaver arranges, a menu is open or something is
carried; the overlay a carried value and guide marks. Each draws whole in a new window,
incarnation, size or measure, or an empty area, then what changed.

### `DrawnRefused`

`DrawnRefused` v1, from that window: a drawing it refused, and why. What it shows stays as it was;
a refused change is drawn again whole at once.

### `AreaButtonChanged`

`AreaButtonChanged` v2, from the window that shows `zengine.workshop` or a frame: a button pressed
or let go in one of Workshop's areas, at the screen's pixel there, by the hand it names. It is that
moment as `PointerButton` takes one, that hand's, the press held to its own release: a tab, the chrome, the room, a menu, an arrangement, a body
Workshop draws. A moment over a pane a window shows in its own area is never relayed to it.

### `AreaPointerMoved`

`AreaPointerMoved` v2, from that window: the pointer moved in Workshop's own area, or while a press
made there is held, as `PointerMoved` takes it, by the hand it names.

### `AreaWheelTurned`

`AreaWheelTurned` v2, from that window: the wheel in Workshop's own area, as `PointerWheel` takes
it, by the hand it names.

### `AreaHoldEnded`

`AreaHoldEnded` v1, from that window, to Workshop as its own area's owner and as each pane area's
arranger: a press held there ended with no release. In its own area every gesture that button holds
ends, and a pane Workshop draws there that the press reached hears it lost. In a pane's area the
pane's room no longer waits for that press.

### `AreaPointerLeft`

`AreaPointerLeft` v1, from that window, told as `AreaHoldEnded` is: the pointer left an area with
no button held. Leaving Workshop's own area it rests on no canvas Workshop draws there, which hears
that it left. A pane's area it left is the pane's to answer.

### `AreaPressed`

`AreaPressed` v2, from the window, to the arranger of a pane's area: a button pressed or let go
there, at the window's pixel, under the window's number, by the hand it names. Workshop counts the
gesture on that hand and keeps the number, which the pane's carry, pass-back or menu cites. The
weaver's own primary press also selects the pane, gives it the keys, ends a guide mark it lands in
and holds the pane's room against a title row; a guest's does not.

### `AreaHandRefused`

`AreaHandRefused` v1, from the window, to the arranger of a pane's area placed for the weaver's
hand alone -- the editor's, the Terminal's and the Hotkeys pane's, on a weaver's host: a guest's
press or wheel there, refused and given to nobody. Workshop says why, in its band.

### `AreaShown`

`AreaShown` v1, from the window that shows the desk, answering Workshop's `AreaShowAsked`: what a
pane's area shows, its words and its named parts. Workshop keeps it as that pane's reading, answers
each read waiting on it, judges a guide mark on it again, and publishes `DeskStamps` when the
reading moved. A refusal is kept as the pane's, and said to its readers.

## Follow

Workshop publishes as `zengine.workshop`, to whoever follows it, each only when it differs from
what it last said: `PaneCatalogRequested` when the surface is ready; `PaneInventory` and
`KeymapShown`; `StandingConditions`, every condition true now; `DeskStamps` v3 when the desk or a
pane's stamp moved, the newest standing for all before it
([the desk, said whole](workshop-panes.md#the-desk-said-whole)); `PaneSubjectShown` once a pane is
inspected; `TranscriptShown`, the terminal's record; `PaneSourceOpened` after an Edit Code open;
and `PaneQuitRequested`, the question a quit asks. To the skin it says `SurfaceCanvas` and
`SurfaceText`, the picture and the band. To a graphics directory it says `SkinOffered`,
`AreaPlaced`, `SkinChosen` and `RoomAsked`, each only when it moved, and to the window that shows
its area, `Drawn`, and `AreaShowAsked` for each pane's area it reads. To the office of a pane a
window shows in its own area it says `PaneKeys` whenever the keys come to the pane or leave it.

## Files

- **The setup file**, `--setup <path>`, by default `workshop-setup.json` in the launch directory:
  a desk you named, saved and restored by the weaver's own gestures ([Setups](setups.md)).
- **The session file**, `--session <path>`, by default `workshop-session.json` in the per-user
  state folder: the desk you were using, its room and the window's place, written on an orderly
  quit and read at launch ([the desk comes back on its
  own](workshop-panes.md#the-desk-comes-back-on-its-own)).
- **The keymap file**, `--keymap <path>`, by default `workshop-keymap.json` in the per-user config
  folder: read when the surface is ready, written by an accepted `KeymapEditRequested`
  ([Hotkeys and the keymap](hotkeys.md)).
- **The settings file**, `--prefs <path>`, by default `workshop-prefs.json` beside the keymap:
  every office's office-wide settings (Workshop's own among them), the pin lists, the locks and
  each layout's baseline theme, read at launch and written by the settings doors and the toggles
  only while it holds what this Workshop read or wrote; a file it could not read is never written.
- `--isolated` runs with no session, keymap or preferences file. The load plan the host runs is
  [the load plan's](load-plan.md), not this weave's.

## See also

- [The pane protocol's reference](workshop-panes.md): the window, the room, the setup, the canvas.
- [Make a Workshop tool](make-a-workshop-tool.md), the walk through a pane of your own.
- [Panes](panes.md) and [Hotkeys and the keymap](hotkeys.md), for a weaver.
- [`zengine.builder`](../../builder/docs/zengine.builder.md), an office Workshop mounts beside it.
