# Workshop law — the quick setup and "Start here"

Register `WL-FIRST`: the quick setup a weaver's first start shows, "Start here" after it, what they
keep and where each answer goes. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).
Office-wide settings are [`settings.md`](settings.md); the editor switch is
[`editor-switch.md`](editor-switch.md); the launcher is [`../launcher.md`](../launcher.md).

## WL-FIRST-01 — The quick setup shows once, at a start that keeps a settings file

LAW — `zengine.first-run` keeps `answered` and `story` at the layouts' owner, and asks for its `setup` pane at a start whose settings are kept and unanswered, on a fresh desk, once a run.

MEANS
- a run with no settings file (`--isolated` without `--prefs`), a file refused at launch and settings withheld show nothing by itself;
- a fresh desk is Layouts, Info and Notices alone: a pane a hand opened first keeps the keys, and the setup waits for the next start;
- clearing `answered` shows it at the next start.

DOES NOT MEAN
- that the weave keeps a file of its own: what it remembers is the layouts' owner's to keep.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-FIRST-02 — The quick setup's answers go to their owners, and the pane is put down either way

LAW — Done, Skip and The story write `answered`, and the pane is put down once the write is answered, taken or not; a setup's Start is asked of Workshop for the hand, then of the launcher.

MEANS
- the setups are the launcher's listing, each with Start; with none, the pane says why;
- a Start is judged as Files' is, `build` and `write` under the press's number; a refusal is its owner's words;
- the pane's lines and the item a press on each chooses are one pure reading (`lines_of`).

DOES NOT MEAN
- that the quick setup keeps a list of setups: the launcher's is the one catalog.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-FIRST-03 — Edit text with Neovim is the editor switch's to perform

LAW — The box asks the editor switch for the plan's `neovim` or `standard` choice and shows the choice the switch says holds the office; the office's grant names the switch's asks to the switch alone.

MEANS
- a consent the switch asked is confirmed only when the box is chosen again;
- the box is read from the switch's status, asked quietly, at every activation and every opening, and Notices says nothing of it;
- the switch's refusal is shown in its words, and the box follows the switch, never the ask.

DOES NOT MEAN
- that the quick setup keeps a second record of the editor: the session remembers it (WL-SWITCH-08);
- that a guest's hand reaches it on a weaver's host: only the weaver's does, as at the Terminal (WL-GUEST-06).

WHY — `agents/decisions/an-editor-is-replaced-not-routed.md`

## WL-FIRST-04 — A step of the story is ticked by its owner's fact, or by the weaver's hand

LAW — A step is ticked only when its owner says it happened -- the inventory, a running view's wiring, the launcher's listing or start -- or by the weaver's press, said as theirs; `story` keeps them.

MEANS
- a view's wiring counts only from an office the inventory says runs a view;
- the owner's fact turns a hand's tick into the owner's, and a press undoes only a hand's tick;
- Show me launches the step's pane and says its next move.

DOES NOT MEAN
- that Workshop knows the story: its steps and their facts are this weave's;
- that the desk's words or the weaver's files tick anything.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## WL-FIRST-05 — "Start here" opens after the quick setup, and at each later start until done

LAW — "Start here" is launched holding the keys when the quick setup is put down, and seated at each later start that keeps settings while the story is not done, the keys left where they are.

MEANS
- a later start seats it with Workshop's launch asking no keys (WL-DESK-03);
- a story done is not seated by itself again; the Pane Manager still lists it.

DOES NOT MEAN
- that it takes the keys at any start but the first.

WHY — `agents/decisions/the-application-defaults-are-a-participant.md`

## Proven by

### WL-FIRST-01
`workshop/first-run/first_run.cpp` FirstRun keeps_settings on(OfficeSettingsAnswered) on(PaneInventory)
`workshop/first-run/setup.hpp` kAnsweredKey fresh_desk
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-01: the quick setup shows itself once at a start
that keeps settings and has not answered, and never where none are kept"

### WL-FIRST-02
`workshop/first-run/first_run.cpp` choose start
`workshop/first-run/setup.hpp` lines_of stepped items_of
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-02: Done writes answered and puts the pane down,
and the next start shows nothing" "WL-FIRST-02: the quick setup's lines name the box, the setups the
launcher lists with Start on each, The story, the pages and Done and Skip, each press choosing its
own item" "WL-FIRST-02: the quick setup lists the setups the launcher lists, and a Start asks
Workshop for the hand that pressed, then the launcher, saying what it answered"

### WL-FIRST-03
`workshop/first-run/first_run.cpp` on(EditorSwitchAnswered) send_switch ask_status
`workshop/admission.hpp` first_run_office_grant
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-03: the Neovim box asks the editor switch, shows
its answer in its words, and confirms a consent only when chosen again"

### WL-FIRST-04
`workshop/first-run/first_run.cpp` FirstRun on(PaneInventory) on(ViewWiring) on(LaunchCatalog)
on(Launched) fact choose_story
`workshop/first-run/story.hpp` steps story_text ticks_of story_lines
`workshop/admission.hpp` first_run_office_grant
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-04: each step is ticked by the fact its owner
says -- the inventory, a running view's wiring, the launcher's listing and its start -- and kept; a
view's wiring said by an office no running view holds ticks nothing" "WL-FIRST-04: a box ticked by
the weaver's hand says so and is undone by it, its owner's fact makes it the owner's, and Show me
opens the step's pane and says its next move"

### WL-FIRST-05
`workshop/first-run/first_run.cpp` on(OfficeSettingsAnswered) launch_story
`tests/test_workshop_panes_first_run.cpp` "WL-FIRST-05: Start here opens when the quick setup closes,
holding the keys, and at each later start until the story is done beside what holds the keys, never
taking them"
