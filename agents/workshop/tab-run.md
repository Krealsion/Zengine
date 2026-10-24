# Workshop law — the tab run

Register `WL-TAB`: the layout tabs on the Layouts pane's first row. One law per heading; cite by
ID. Router: [`../workshop.md`](../workshop.md).

## WL-TAB-01 — The run, the association and the workspace fact are the Layouts pane

LAW — `pane_kind::kLayouts` (`placement::kTopBand`) is a catalog row, a setup row, authored geometry, a front rank, ordinary paint, occupancy, coverage, launch recovery and session persistence.

MEANS
- what moved is who owns the rectangle, hence what a weaver may do to it; the composition did not;
- removing the pane strands nobody: the keys step the run and the Pane Manager brings it back;
- a desk is applied as written, so every desk the repository ships names it.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-02 — The status is the active layout's association, in three sentences

LAW — `setup: none`, `setup: <artifact> | current`, `setup: <artifact> | modified`; `none` does not mean unsaved, `UNSAVED` is retired here, and the session file is never shown in this slot.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-03 — The path is what elides

LAW — The status fits the path against its own budget before the words: the path yields to the count and the hints, and the status column count is derived from the words' own widths.

MEANS
- a narrow row goes on distinguishing the three verdicts; which artifact it is may drop;
- the status is adjusted to the row's right edge where the row still fits.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-04 — `+` is an action, not a durable pseudo-layout

LAW — `+` is one cell with its own span at the end of the run: not counted as a layout, not steppable, unknown to the session, paid for last, and it does exactly what `layout.new` does.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-05 — `band_status` is the one composition both consumers spend

LAW — The tab run is one composition both consumers spend: the painter publishes its text and the press inverse answers out of its recorded spans, against the pane's body; it also says which row it is on.

MEANS
- narrowing the pane narrows the run; markers, reservation and `+` degrade by their own rules;
- a tab's span is recorded as the row is written, so there is no second measurement to drift.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-06 — The marker brackets the live name, one cell each side

LAW — `>name<` for the live layout and ` name ` for every other, said in characters: the markers and the pad are each one character wide, so switching moves nothing to the right.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-07 — Every name is painted bare

LAW — Every name is painted bare, the authored bytes with no quoting; the marker cells are the delimiter, and a tab's extent is `LayoutTab::column`/`columns`, recorded as written.

MEANS
- `Home >My Layout< Art` reads correctly; the notices still spend `quoted_setup_name`.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-08 — The visible window is derived and stored nowhere

LAW — A run that fits is painted whole, the live layout is always painted (cut, marked, where even it will not fit), and everything omitted is counted on its own side (`layouts_omitted_text`).

MEANS
- it grows outward from the live layout, so no offset goes stale after a switch or a removal;
- keyboard stepping traverses the whole population, painted or not, and the window follows.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-09 — A press on a painted tab is a press on the Layouts pane

LAW — A press on a painted tab is answered through ordinary occupancy and then `band_tab_at`; it selects the Layouts pane and switches, and a pane in front of the run takes the press.

MEANS
- the status, the blank between and an omitted tab answer nothing.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-10 — A second press on the same tab renames it

LAW — A second press on the same tab within `kDoubleClickMs` renames it: the first press arms a record on the way out and the completing press spends it, so there is no triple-click.

MEANS
- the first press already made the tab live, so the editor's subject and the live layout agree.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-11 — A press also takes hold of the tab

LAW — A tab drag holds nothing but whether it is active, the hand always carrying the live layout; a motion re-asks the inverse against the run as painted and moves the layout there.

MEANS
- nothing is cached or reconciled; a release ends the gesture wherever the hand is.

WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-12 — A right press on a tab names it as a subject

LAW — `context_subject::kLayout`'s identity is the position, captured at the press and re-judged at spend; asking about a tab does not stand on it, so Close and reorder mean the pointed tab.

MEANS
- `^w` is annotated beside Close only when the captured tab is the active one.

WHY — `agents/decisions/the-layouts-pane.md`

## Proven by

### WL-TAB-01
`workshop/panes.hpp` kLayouts kTopBand kDefaultPanes
`workshop/screen_layouts.cpp` paint_layouts layouts_body
`tests/test_workshop_screen.cpp` "the Layouts pane's developer default IS the historical rectangle"
"authored geometry moves the Layouts pane, and the tabs with it" "removing the Layouts pane strands
nobody"
`tests/session/demo_recipes.py`
test_every_shipped_desk_shows_layouts_and_readiness_asks_its_providers_alone

### WL-TAB-02
`workshop/screen_layouts.cpp` setup_link_text band_status
`workshop/screen.hpp` kSetupSlot
`tests/test_workshop_persistence.cpp` "the top row says the ACTIVE layout's Setup association"
`tests/test_workshop_screen.cpp` "the three verdicts, and what makes a fresh desk `none`"

### WL-TAB-03
`workshop/screen_layouts.cpp` setup_link_text setup_rest_text
`workshop/screen.hpp` path_columns kSetupStatusCols kElidedCols
`workshop/screen_bindings.cpp` fit_path
`tests/test_workshop_screen.cpp` "the association's verdict survives the row's cut, at every width"
"the status row is tabs on the left and the existing status right"

### WL-TAB-04
`workshop/screen.hpp` kLayoutCreate
`workshop/screen_layouts.cpp` band_status layout_count
`tests/test_workshop_host.cpp` "the `+` affordance is the pointer's spelling of `layout.new`" "at
the minimum width the `+` yields to the tab and the status"

### WL-TAB-05
`workshop/screen.hpp` LayoutTabRun::text LayoutTabRun::tabs kNoBandRow BandStatus
`workshop/screen_layouts.cpp` band_status paint_layouts band_tab_at layouts_body band_tab_row
layout_tab_run
`tests/test_workshop_screen.cpp` "the tab run is one composition on both media" "a press answers a
painted tab and nothing else on the band" "the run never spends more columns than it was given"

### WL-TAB-06
`workshop/screen.hpp` kLayoutLiveOpen kLayoutLiveClose kLayoutTabPad
`workshop/screen_layouts.cpp` layout_tab_text
`tests/test_workshop_screen.cpp` "every tab is one cell, the name, one cell" "switching the live
layout moves nothing to the right of it" "the closing marker belongs to the layout it closes"

### WL-TAB-07
`workshop/screen.hpp` LayoutTab
`workshop/screen_layouts.cpp` layout_tab_text
`workshop/setup.hpp` quoted_setup_name
`tests/test_workshop_screen.cpp` "a multi-word name is delimited by its own cells, not by quotes"
"the weaver reads `Home >Code< Art` on Workshop's first row"
`tests/test_workshop_persistence.cpp` "a name that could impersonate the setup line is one SPAN on
it"

### WL-TAB-08
`workshop/screen_layouts.cpp` layouts_omitted_text band_status layout_tab_run
`workshop/screen_gestures.cpp` list_window
`tests/test_workshop_screen.cpp` "the visible window is derived, keeps the live tab, and marks its
ends" "the live tab is cut rather than dropped when even it will not fit" "stepping wraps over the
whole population, painted or not"

### WL-TAB-09
`workshop/screen_layouts.cpp` band_tab_at
`workshop/screen_chrome.cpp` occupied_at
`workshop/screen.hpp` LayoutTabPress
`workshop/weave_pointer.cpp` pointer_button layouts_press
`tests/test_workshop_host.cpp` "a tab press IS a press on the Layouts pane, and still switches"
"pressing a painted tab switches, and the rest of the row does not"
`tests/test_workshop_screen.cpp` "a pane in front of the Layouts pane takes the press"

### WL-TAB-10
`workshop/screen.hpp` TabClickMemory Session::tab_click kDoubleClickMs
`workshop/screen_arrange.cpp` doubles_a_tab_click
`tests/test_workshop_host.cpp` "a double-click on a tab renames THAT layout, and writes no file"
`tests/test_workshop_persistence.cpp` "the rename editor opens on the tab's own name and writes
nothing"

### WL-TAB-11
`workshop/screen.hpp` LayoutTabDrag Session::tab_drag
`workshop/weave_pointer.cpp` end_held_gestures
`workshop/layout_shelf.hpp` SetupState::active_at move_layout
`tests/test_workshop_host.cpp` "dragging a tab along the run reorders it and nothing else"

### WL-TAB-12
`workshop/context.hpp` context_subject::kLayout
`workshop/screen_attention.cpp` context_annotation
`workshop/weave_pointer.cpp` open_context_on_layout
`tests/test_workshop_host.cpp` "a tab's context menu acts on THAT tab" "Move Left and Move Right
reorder from the tab that was pointed at"
