# zengine.layouts -- the layouts

**Manual.** The office that holds every layout of the run, the live one included, and what each
keeps for itself -- its rows' settings, its locks, its pins and its theme baseline -- beside what
holds in every layout: each office's office-wide settings, the everywhere locks and the
Workshop-wide pin lists. It is the one writer and the one judge of all of it. Workshop mounts it in
its own process, answering to the office `zengine.layouts`, and never replaces it; it is not a file
you load. A pane's office, the Terminal, a guest with `settings` and a settings tool reach it by
the messages below.

## About

Workshop presents the live layout and judges every hand; this office keeps it. Workshop draws from
the copy this office sends it, `LiveLayout`, which every change replaces whole, and asks for every
change its own gestures make -- a drag, a close, a layout switch, `s`, `r` -- with
`LayoutEditRequested`. Every other asker reaches the settings doors below, and none of them is
Workshop's: a door asked of `zengine.workshop` is refused there by Loom's grant, never forwarded.

A layout is named by its **id**, which this office mints for each layout of the run -- a new one,
a copy -- and keeps in the session, never reusing one; `0` names the live layout. The id is never
shown to the weaver: a tab shows the layout's name, and a reading says each layout's id beside its
place in the weaver's order. An id no layout holds is refused, `that layout is gone -- no layout of
this Workshop holds id <n>`, never read as another.

What a layout keeps for itself travels with it: a switch, a move, a rename and a copy (which takes
its own) keep it, and a removal takes it away. A **lock** freezes a key: in one layout it holds
that row's value, and everywhere it pins one value in every row that seats the pane. Every write
door refuses or passes over a locked key, a desk put live in place of the live one included, which
keeps every lock and is refused whole where it would change a locked value, naming each.

## Use

Ask as an office, the Terminal this host mounted, or a guest whose row has `settings`: anyone else
is answered with the refusal `settings withheld: <why>` and nothing read or written. A pane's
office declares the settings its pane takes with [`PaneSettingsDeclared`](#panesettingsdeclared)
and its office-wide settings with [`OfficeSettingsDeclared`](#officesettingsdeclared); this office
hands each declaring office's holder `OfficeSettings` at each revision, and Workshop hands a seated
pane its row (`PaneSettings`, [the pane protocol](zengine.workshop.md)).

Read before you write: every write names the revision its asker read, and one judged from a reading
the layouts have moved past is refused, `stale revision <n> -- ... stands at <m>; read it again`.

## Commands

### `PaneSettingsRequested`

`PaneSettingsRequested` asks for one pane's row of the live layout
([`settings_seam_vocabulary.hpp`](../settings_seam_vocabulary.hpp)).

- **Takes:** `provider` and `pane`, the row's address.
- **Answers:** `PaneSettingsAnswered`, to the asker alone: the row's `revision` (0 when the live
  layout has no such row), the `rows` of the pane's counted declaration with their tags, pins and
  summaries, the `settings` the row keeps, its `pins` in order, and the keys a lock holds in
  `locks`, each `everywhere` or `layout`.
- **Refuses:** `settings withheld: <why>` in `said`, with nothing read.
- **Example:** `ask @zengine.layouts PaneSettingsRequested 1 provider=zengine.terminal
  pane=terminal`, from the Terminal.

### `PaneSettingCommitRequested`

`PaneSettingCommitRequested` writes one setting of a pane's row of the live layout, at the
revision the asker read.

- **Takes:** `provider`, `pane`, `revision`, `key`, and the value in its kind's one field --
  `flag`, `number` or `text` -- or `clear` and no field for the setting's default. Every row takes
  `workshop.skin`, the skin of the pane's graphics area, which the pane is never handed.
- **Answers:** `PaneSettingCommitAnswered{accepted, refusal, revision, handed}`, to the asker alone:
  the row's revision after the write, or as it stood. Workshop's band says `committed <key> of
  <pane> = <value>` and how far the hand-off got.
- **Refuses:** `settings commit withheld: <why>`; `<office>/<pane> has no row on the live layout
  -- open it first`; `stale revision <n> -- the row of <office>/<pane> stands at <m>; read it
  again`; `a clear takes no value -- name the key alone`; ``<office>/<pane> has no counted setting
  `<key>` -- a key no declaration takes is only cleared``; `<key> is locked -- nothing writes it
  while the lock stands, and only the weaver unlocks it`; `<key> takes <what it takes>, not <the
  value>`. Each writes nothing.
- **Example:** `ask @zengine.layouts PaneSettingCommitRequested 1 provider=zengine.terminal
  pane=terminal revision=$r1.revision clear=false key=wheel-rows number=6`, from the Terminal.

### `OfficeSettingsRequested`

`OfficeSettingsRequested` asks for one office's office-wide settings.

- **Takes:** `office`; `zengine.workshop` for Workshop's own.
- **Answers:** `OfficeSettingsAnswered`, to the asker alone: the `revision` the settings stand at
  (0 when nothing declares for the office), the `rows` its declaration gives, the `settings` kept,
  the `locks`, and `written`: `written`, or why the settings file does not hold them.
- **Refuses:** `settings withheld: <why>` in `written`.
- **Example:** `ask @zengine.layouts OfficeSettingsRequested 1 office=zengine.workshop`.

### `OfficeSettingCommitRequested`

`OfficeSettingCommitRequested` writes one office-wide setting at the revision the asker read.

- **Takes:** `office`, `revision`, `key`, and the value in its kind's one field, or `clear`.
- **Answers:** `OfficeSettingCommitAnswered{accepted, refusal, revision, handed}`, its `handed`
  saying `applied` (Workshop's own) or `handed` (an office's holder), then `written` or `not
  written: <why>`.
- **Refuses:** ``nothing declares office-wide settings for `<office>` here``; `stale revision <n>
  -- the settings of <office> stand at <m>; read them again`; `Workshop's settings file <path>
  could not be read at launch and is never written over -- mend or remove it, then relaunch`; a key
  not declared, a lock, or a value the setting does not take. Each writes nothing.
- **Example:** `ask @zengine.layouts OfficeSettingCommitRequested 1 office=zengine.workshop
  revision=$r1.revision clear=false key=workshop.focus.ring text=plain`.

### `PaneSettingPinRequested`

`PaneSettingPinRequested` v2 edits one pane's pin list: the settings an editor lists first, in
order.

- **Takes:** `provider` and `pane`; `key`; `act`, one of `pin`, `unpin`, `up`, `down` or `reset`
  (back to the declared pins, naming no key); `layout`, the id of the layout whose list a
  per-layout scope edits, `0` the live one.
- **Answers:** `PaneSettingPinAnswered{accepted, refusal, pins, scope}`, to the asker alone: the
  list after the act and the scope it is kept in, `workshop-wide` (in the settings file) or
  `per-layout` (in that layout). It is never handed to the pane.
- **Refuses:** `pins withheld: <why>`; `that layout is gone -- no layout of this Workshop holds id
  <n>`; ``<office>/<pane> declares no setting `<key>` to pin``; `` `<key>` is pinned already ``;
  `` `<key>` is not pinned ``; a list past `kMaxPinsPerPane`. Each writes nothing.
- **Example:** `ask @zengine.layouts PaneSettingPinRequested 2 provider=zengine.terminal
  pane=terminal key=wheel-rows act=pin layout=0`.

### `PaneSettingLockRequested`

`PaneSettingLockRequested` v2 locks or unlocks one setting, in one layout or everywhere.

- **Takes:** `target`, `<office>/<pane>`, or `<office>` for an office-wide setting; `key`; `act`,
  `lock` or `unlock`; `scope`, `layout` (that layout's row alone) or `everywhere`; `layout`, a
  layout's id, `0` the live one; `gesture`, for an unlock from a pane, the correlation of the
  weaver's current act the pane was sent.
- **Answers:** `PaneSettingLockAnswered{accepted, refusal, written}`, to the asker alone. A
  layout's lock freezes the value that layout's row holds and is kept in that layout; `written`
  names it. An everywhere lock on a pane pins the value its layout holds, or the default when the
  row keeps none, writes it into every layout's row that seats the pane and gives it to a row
  seated later; it is kept in the settings file, and `written` names the layouts. An office-wide
  setting has one value, so its lock holds everywhere.
- **Refuses:** `lock withheld: <why>`; `` `<act>` is not a lock act -- lock or unlock ``;
  `` `<scope>` is not where a lock holds -- layout or everywhere ``; `unlocking is the weaver's --
  from the Terminal, or from a pane on the weaver's own current act`, which an unlock naming a
  gesture is judged by asking Workshop whether it is the weaver's current act sent to that
  office's pane; a target naming `@`; a gone layout; `an office-wide setting has one value in every
  layout -- lock it everywhere`; a key the target neither declares nor keeps; a pane its layout
  does not seat; locked already, or not locked; a value its row's bounds refuse in some layout.
  Each changes nothing.
- **Example:** `ask @zengine.layouts PaneSettingLockRequested 2 target=zengine.terminal/terminal
  key=wheel-rows act=lock scope=everywhere layout=0 gesture=0`, from the Terminal.

### `LayoutSettingsRequested`

`LayoutSettingsRequested` reads every layout's rows at once.

- **Takes:** nothing.
- **Answers:** `LayoutSettingsAnswered{revision, active, layouts, declared, everywhere}`, to the
  asker alone: each layout's `id`, place, name, baseline theme (`name` and `fingerprint`, a name of
  "" for none) and rows, each row's `settings` and `locked` keys there; what each pane's counted
  declaration takes, `workshop.skin` first, and each office's office-wide rows, with `pane` empty;
  the everywhere locks; the live layout's place; and the `revision` it all stands at.
- **Refuses:** an empty reading at `revision` 0, to an asker the settings doors do not serve.
- **Example:** `ask @zengine.layouts LayoutSettingsRequested 1`.

### `SettingsWriteRequested`

`SettingsWriteRequested` v2 writes many settings across layouts, and layouts' baselines, at the
revision the asker read.

- **Takes:** `revision`; `writes`, each a `layout` (an id, `0` the live one; not read for an
  office), `target`, `key` and a value in its kind's one field or `clear`; `baselines`, each a
  `layout` and a `theme`, a name of "" removing it; `cause`, what the band says the batch was.
- **Answers:** `SettingsWriteAnswered{accepted, refusal, revision, results, handed}`, to the asker
  alone: each write's `outcome` -- `written`, `unchanged`, or passed over as `locked`, `not-taken`
  or `not-in-layout` -- with why; the revision after it; and the live rows and offices handed or
  applied. A baseline is kept in its layout.
- **Refuses:** whole, writing nothing: `settings writes withheld: <why>`; `stale revision <n> --
  the layouts stand at <m>; read them again`; a batch past its bound; `write <i>: <why>`;
  `baseline <i>: <why>`; `nothing written: Workshop's settings file was not written -- <why>`.
- **Example:** a script applying a theme to every layout reads
  [`LayoutSettingsRequested`](#layoutsettingsrequested), then sends one batch naming the revision
  it read, a write per row the theme names and a baseline per layout.

### `SetupApplyRequested`

`SetupApplyRequested` puts a whole desk live in place of the live layout's
([`setup_control.hpp`](../setup_control.hpp)).

- **Takes:** `setup`, the text of a setup file ([Setups](setups.md)).
- **Answers:** `loom::Ack`, once the desk is the live layout's, its id kept, every lock it held
  kept and each lock the incoming desk brings joined; the layout is no longer related to a setup
  file, and Workshop's band says `applied setup <name>`; a menu open over the desk it replaced is
  put down. No file is written.
- **Refuses:** in `loom::Refused`: `setup names a pane this Workshop cannot present: <office>
  <pane>`; `the desk would change what a lock holds -- <key> of <office>/<pane>, ...; nothing was
  replaced`; and the setup reader's words, such as `not a Workshop setup: ...`.
- **Example:** a guest with the `demo` power puts a prepared desk live.

### `LayoutEditRequested`

`LayoutEditRequested` is the presenter's: the changes Workshop's own gestures make, judged for
their hand already ([`vocabulary.hpp`](../layouts/vocabulary.hpp)).

- **Takes:** `revision`, the `LiveLayout` the edits were judged from (0 for none); `edits`, each an
  `op` -- `seat`, `unseat`, `window`, `front`, `back`, `raise`, `lower`, `reset-front`, `switch`,
  `step`, `new`, `duplicate`, `remove`, `move`, `rename`, `save` or `restore` -- and the fields it
  reads. None asks for the live layout as it stands; the presenter's first such ask restores the
  last session.
- **Answers:** a `LiveLayout` to the presenter, then `LayoutEditAnswered{accepted, refusal,
  revision, discarded, unlocked}`, written whole or not at all.
- **Refuses:** an asker that is not `zengine.workshop`; `the layout changed -- the next picture is
  the current one`, for edits judged from an older revision; the first edit's own refusal.
- **Example:** Workshop sends `{op: window, provider, pane, place, width, height}` at a drag's
  release.

### `SessionSaveRequested`

`SessionSaveRequested` is the presenter's orderly close: write the session now.

- **Takes:** nothing.
- **Answers:** `SessionSaveAnswered{written, refusal}`: the run, each layout with its id, the live
  one's place, the next id and the remembered choices, written; and, once a session holds what a
  version-3 settings file kept for layouts by name, that file written without them.
- **Refuses:** an asker that is not `zengine.workshop`; `isolated run -- no session file`; `<path>
  could not be read and will not be written over`; the write's own refusal.
- **Example:** Workshop asks once every pane accepted its quit.

## Hears

### `PaneSettingsDeclared`

`PaneSettingsDeclared`, from a pane's office: the settings it takes, each a key, a default and what
it takes, and at version 2 its tags, pin and summary. Judged whole; a refusal is said on
Workshop's band, `<name> @<office>: its settings were not taken -- <why>`, and the settings in
force stand. It counts while the incarnation that sent it holds the office.

### `OfficeSettingsDeclared`

`OfficeSettingsDeclared`, from an office, Workshop's own among them: the office-wide settings it
takes, judged whole by a pane declaration's rules and replaced by a later one; it counts while the
incarnation that sent it holds the office. Its holder is handed `OfficeSettings` at each revision,
when it accepts it. A refusal is said on Workshop's band.

### `PaneInventory`

`PaneInventory`, from Workshop: the panes it presents, each by its name, read for the names this
office says and for which panes a desk put live can name.

### `WeaversActAnswered`

`WeaversActAnswered`, from Workshop, answering this office's `WeaversActRequested{gesture,
office}`: whether `gesture` is the weaver's current act sent to a pane of `office`. An unlock
waiting on it is answered then.

## Follow

This office sends `LiveLayout` to Workshop on every change of the run or of the live layout, and
before each answer to an edit: the live desk, a tab for every layout, the revision it stands at,
each live row's settings revision, and the sentence about what moved. It sends `OfficeSettings` to
each declaring office's holder at each revision.

## Files

- **The setup file**, the live layout's associated file or `--setup <path>`: written by `s`, read
  by `r` ([Setups](setups.md)). It holds one desk and what that layout keeps; it carries no id.
- **The session file**, `--session <path>`, by default `workshop-session.json` in the per-user
  state folder: a header line -- the format, its version, how many layouts, which is live, the next
  id and the remembered choices -- then a layout a line, each with its id. Written at Workshop's
  orderly close; read at the start, an older one brought forward.
- **The settings file**, `--prefs <path>`, by default `workshop-prefs.json` in the per-user config
  folder: every office's office-wide settings, the everywhere locks and the Workshop-wide pin
  lists, written at each accepted change while it holds what this office read or wrote. A version-3
  file's layouts' entries are folded into the layouts of their name once, each one no layout takes
  named, and carried until a session holds them.

## See also

- [`zengine.workshop`](zengine.workshop.md), the presenter, and the pane protocol.
- [Setups](setups.md), the file a layout is saved to.
