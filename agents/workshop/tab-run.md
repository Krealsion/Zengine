# Workshop law — the tab run

Register `WL-TAB`: the layout tabs on the Layouts pane's first row. One law per heading; cite by
ID. Router: [`../workshop.md`](../workshop.md).

## WL-TAB-01 — The run, the association and the workspace fact are the Layouts pane

LAW — `panel::kLayouts` (`placement::kTopBand`) is a catalog row, a setup row, authored geometry, a front rank, ordinary paint, occupancy, coverage, picker recovery and session persistence.

MEANS
- what moved is who owns the rectangle, hence what a maker may do to it; the composition did not;
- removing the pane strands nobody: the keys still step the run and the picker brings it back.

PROVEN BY — `workshop/panel.hpp` `kLayouts`, `kTopBand`, `kDefaultPanels`;
`workshop/screen_layouts.cpp` `paint_layouts`, `layouts_body`; `tests/test_workshop_screen.cpp`
case `"WUX-12/SC-2: the Layouts pane's developer default IS the historical rectangle"`, case
`"WUX-12/SC-3: authored geometry moves the Layouts pane, and the tabs with it"`, case
`"WUX-12/SC-10: removing the Layouts pane strands nobody"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-02 — The status is the active layout's association, in three sentences

LAW — `setup: none`, `setup: <artifact> | current`, `setup: <artifact> | modified`; `none` does not mean unsaved, `UNSAVED` is retired here, and the session file is never shown in this slot.

PROVEN BY — `workshop/screen_layouts.cpp` `setup_link_text`, `band_status`;
`workshop/screen.hpp` `kSetupSlot`; `tests/test_workshop_persistence.cpp` case `"WUX-11/SC-7: the
top row says the ACTIVE layout's Setup association"`; `tests/test_workshop_screen.cpp` case
`"WUX-11/SC-7: the three verdicts, and what makes a fresh desk `none`"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-03 — The path is what elides

LAW — The status fits the path against its own budget before the words: the path yields to the count and the hints, and the status column count is derived from the words' own widths.

MEANS
- a narrow row goes on distinguishing the three verdicts; which artifact it is may drop;
- the status is adjusted to the row's right edge where the row still fits.

PROVEN BY — `workshop/screen_layouts.cpp` `setup_link_text`, `setup_rest_text`;
`workshop/screen.hpp` `path_columns`, `kSetupStatusCols`, `kElidedCols`;
`workshop/screen_bindings.cpp` `fit_path`; `tests/test_workshop_screen.cpp` case `"WUX-11/SC-24:
the association's verdict survives the row's cut, at every width"`, case `"WUX-9/SC-7: the status
row is tabs on the left and the existing status right"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-04 — `+` is an action, not a durable pseudo-layout

LAW — `+` is one cell with its own span at the end of the run: not counted as a layout, not steppable, unknown to the session, paid for last, and it does exactly what `layout.new` does.

PROVEN BY — `workshop/screen.hpp` `kLayoutCreate`; `workshop/screen_layouts.cpp` `band_status`,
`layout_count`; `tests/test_workshop_panels.cpp` case `"WUX-11/SC-1: the `+` affordance is the
pointer's spelling of `layout.new`"`, case `"WUX-11/SC-8: at the minimum width the `+` yields to
the tab and the status"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-05 — `band_status` is the one composition both consumers spend

LAW — The tab run is one composition both consumers spend: the painter publishes its text and the press inverse answers out of its recorded spans, against the pane's body; it also says which row it is on.

MEANS
- narrowing the pane narrows the run; markers, reservation and `+` degrade by their own rules;
- a tab's span is recorded as the row is written, so there is no second measurement to drift.

PROVEN BY — `workshop/screen.hpp` `LayoutTabRun::text`, `LayoutTabRun::tabs`, `kNoBandRow`,
`BandStatus`; `workshop/screen_layouts.cpp` `band_status`, `paint_layouts`, `band_tab_at`,
`layouts_body`, `band_tab_row`, `layout_tab_run`; `tests/test_workshop_screen.cpp` case
`"WUX-9/SC-7: the tab run is one composition on both media"`, case `"WUX-9/SC-9: a press answers a
painted tab and nothing else on the band"`, case `"WUX-9/SC-8: the run never spends more columns
than it was given"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-06 — The marker brackets the live name, one cell each side

LAW — `>name<` for the live layout and ` name ` for every other, said in characters: the markers and the pad are each one character wide, so switching moves nothing to the right.

PROVEN BY — `workshop/screen.hpp` `kLayoutLiveOpen`, `kLayoutLiveClose`, `kLayoutTabPad`;
`workshop/screen_layouts.cpp` `layout_tab_text`; `tests/test_workshop_screen.cpp` case
`"QR-15/SC-2+SC-3+SC-4: every tab is one cell, the name, one cell"`, case `"QR-15/SC-4: switching
the live layout moves nothing to the right of it"`, case `"QR-15/SC-7: the closing marker belongs
to the layout it closes"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-07 — Every name is painted bare

LAW — Every name is painted bare, the authored bytes with no quoting; the marker cells are the delimiter, and a tab's extent is `LayoutTab::column`/`columns`, recorded as written.

MEANS
- `Home >My Layout< Art` reads correctly; the notices still spend `quoted_setup_name`.

PROVEN BY — `workshop/screen.hpp` `LayoutTab`; `workshop/screen_layouts.cpp` `layout_tab_text`;
`workshop/setup.hpp` `quoted_setup_name`; `tests/test_workshop_screen.cpp` case `"QR-15/SC-5: a
multi-word name is delimited by its own cells, not by quotes"`, case `"QR-15: the maker reads
`Home >Code< Art` on Workshop's first row"`; `tests/test_workshop_persistence.cpp` case `"QR-15: a
name that could impersonate the setup line is one SPAN on it"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-08 — The visible window is derived and stored nowhere

LAW — A run that fits is painted whole, the live layout is always painted (cut, marked, where even it will not fit), and everything omitted is counted on its own side (`layouts_omitted_text`).

MEANS
- it grows outward from the live layout, so no offset goes stale after a switch or a removal;
- keyboard stepping traverses the whole population, painted or not, and the window follows.

PROVEN BY — `workshop/screen_layouts.cpp` `layouts_omitted_text`, `band_status`,
`layout_tab_run`; `workshop/screen_gestures.cpp` `list_window`; `tests/test_workshop_screen.cpp`
case `"WUX-9/SC-8: the visible window is derived, keeps the live tab, and marks its ends"`, case
`"WUX-9/SC-8: the live tab is cut rather than dropped when even it will not fit"`, case
`"WUX-9/SC-10: stepping wraps over the whole population, painted or not"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-09 — A press on a painted tab is a press on the Layouts pane

LAW — A press on a painted tab is answered through ordinary occupancy and then `band_tab_at`; it selects the Layouts pane and switches, and a pane in front of the run takes the press.

MEANS
- the status, the blank between and an omitted tab answer nothing.

PROVEN BY — `workshop/screen_layouts.cpp` `band_tab_at`; `workshop/screen_chrome.cpp`
`occupied_at`; `workshop/screen.hpp` `LayoutTabPress`; `workshop/weave_pointer.cpp` `take_hold`;
`workshop/weave_editor.cpp` `layouts_press`; `tests/test_workshop_panels.cpp` case
`"WUX-12/SC-4+SC-8: a tab press IS a press on the Layouts pane, and still switches"`, case
`"WUX-9/SC-9: pressing a painted tab switches, and the rest of the row does not"`;
`tests/test_workshop_screen.cpp` case `"WUX-12/SC-5+SC-7: a pane in front of the Layouts pane
takes the press"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-10 — A second press on the same tab renames it

LAW — A tab double-click is a second click record beside the word-selecting one, sharing the interval, the arm-on-the-way-out discipline and the spend-the-arming rule.

MEANS
- the first press already made the tab live, so the editor's subject and the live layout agree.

PROVEN BY — `workshop/screen.hpp` `ClickMemory`, `TabClickMemory`, `Session::tab_click`,
`kDoubleClickMs`; `workshop/screen_arrange.cpp` `doubles_a_tab_click`;
`tests/test_workshop_panels.cpp` case `"WUX-11/SC-3: a double-click on a tab renames THAT layout,
and writes no file"`; `tests/test_workshop_persistence.cpp` case `"WUX-11/SC-3: the rename editor
opens on the tab's own name and writes nothing"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-11 — A press also takes hold of the tab

LAW — A tab drag holds nothing but whether it is active, the hand always carrying the live layout; a motion re-asks the inverse against the run as painted and moves the layout there.

MEANS
- nothing is cached or reconciled; a release ends the gesture wherever the hand is.

PROVEN BY — `workshop/screen.hpp` `LayoutTabDrag`, `Session::tab_drag`;
`workshop/weave_pointer.cpp` `end_held_gestures`; `workshop/setup.hpp` `SetupState::active_at`,
`move_layout`; `tests/test_workshop_panels.cpp` case `"WUX-11/SC-4: dragging a tab along the run
reorders it and nothing else"`.
WHY — `agents/decisions/the-layouts-pane.md`

## WL-TAB-12 — A right press on a tab names it as a subject

LAW — `context_subject::kLayout`'s identity is the position, captured at the press and re-judged at spend; asking about a tab does not stand on it, so Close and reorder mean the pointed tab.

MEANS
- `^w` is annotated beside Close only when the captured tab is the active one.

PROVEN BY — `workshop/context.hpp` `context_subject::kLayout`; `workshop/screen_attention.cpp`
`context_annotation`; `workshop/weave_pointer.cpp` `open_context_on_layout`;
`tests/test_workshop_panels.cpp` case `"WUX-11/SC-2+SC-5: a tab's context menu acts on THAT tab"`,
case `"WUX-11/SC-4: Move Left and Move Right reorder from the tab that was pointed at"`.
WHY — `agents/decisions/the-layouts-pane.md`
