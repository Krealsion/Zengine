# Workshop law — the Info body

Register `WL-INFO`: the Info panel's body, resolved once. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

## WL-INFO-01 — The Info body is resolved once

LAW — `info_body_place` is the whole Info body — its place, the rows of the active medium's type that fit, their sharing, a value's width, the members each window shows — and every consumer calls it.

MEANS
- the painter, the caret, `refresh_inspector`, both windows, `info_press` and `objects_press`.

PROVEN BY — `workshop/screen_info.cpp` `info_body_place`, `paint_info`; `workshop/screen.hpp`
`InfoBodyPlace`, `kInfoBodyMinRows`; `workshop/weave_seam.cpp` `refresh_inspector`, `info_press`,
`objects_press`; `tests/test_workshop_panels.cpp` case `"HD-6: the body's row capacity is the
ACTIVE medium's, from one equation"`, case `"HD-6: one body, two media, different row counts and
the same property facts"`; `tests/test_workshop_document.cpp` case `"HD-5: the property editor
paints, carets, measures and hits from one geometry"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-02 — Nothing in Workshop multiplies a font metric

LAW — The body is one region and a property row is one of its rows; the fit gives rows and columns with the text inset inside, and a value's width is the columns less mark, label and caret.

MEANS
- 25 cells of body is 16 rows of an 18-pixel face and 25 rows of a cell medium, one body;
- a body too short for the face falls back to cells, with no rule written to say so.

PROVEN BY — `workshop/screen.hpp` `InfoBodyPlace::value_columns`, `kPropertyMarkCols`,
`kPropertyLabelCols`, `kPropertyCaretCols`; `surface/region.hpp` `fit_region`, `kTextInsetPx`;
`tests/test_workshop_panels.cpp` case `"HD-6: the body's row capacity is the ACTIVE medium's, from
one equation"`, case `"HD-6: the property layer never learned that graphical rows got taller"`,
case `"HD-6: the body falls back to cells when it is too short for the face"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-03 — The vertical window is `list_window`

LAW — `list_window`: a population that fits is shown whole, the focused row is always in the window, every omission is counted on its own side and spends a row; derived every paint, stored nowhere.

MEANS
- there is no scroll offset, no session field and no scroll gesture on this list;
- `completion_first_shown` is deliberately not the same function: it anchors to the tail.

PROVEN BY — `workshop/screen_gestures.cpp` `list_window`, `omitted_text`; `workshop/screen.hpp`
`completion_first_shown`, `ListWindow`; `tests/test_workshop_panels.cpp` case `"HD-6: what the
body cannot show, it counts -- on the side it left it out"`, case `"HD-6: the selected row stays
visible across the boundary, by keys only"`; `tests/test_workshop_screen.cpp` case `"an object
past the list's share cannot vanish: it says what it left out"`, case `"the object-list window is
total, and never spends more rows than it has"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-04 — The row maps are inverses, and a press is never rounded to a cell

LAW — The row that must stay visible is the editing row, else the cursor; the row maps are inverses over one row arithmetic, and a press is never rounded to a Workshop cell.

MEANS
- `prose_row_in_window`/`item_at_prose_row` are helpers, deliberately not a `List` component;
- an 18-pixel row against a 12-pixel cell would name the wrong property for most of the body.

PROVEN BY — `workshop/screen_info.cpp` `inspector_focus`, `prose_row_of_property`,
`property_at_prose_row`, `prose_row_in_window`, `item_at_prose_row`; `workshop/screen.hpp`
`ProseAt`; `tests/test_workshop_panels.cpp` case `"HD-6: a press under HD row geometry names the
property the eye is on"`, case `"HD-6: entering an edit and being refused both keep the row on
screen"`, case `"HD-8: the graphical press is not rounded to a Workshop cell"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-05 — A resting value is fitted and a live draft is windowed

LAW — A resting value is fitted with a mark where it was cut, because a committed value has no caret to say it moved; a live draft is windowed unmarked, having one; the draft is reconciled once per repaint.

MEANS
- at most one row is ever editing: `begin_edit` is reachable only from command mode.

PROVEN BY — `workshop/screen_bindings.cpp` `detail::fit`; `workshop/screen_info.cpp`
`property_row_prefix`; `workshop/weave_save.cpp` `begin_edit`; `workshop/weave_seam.cpp`
`refresh_inspector`; `component/text_box.hpp` `TextBox::visible`; `workshop/property.hpp`
`Row::display`, `Row::begin`; `tests/test_workshop_panels.cpp` case `"HD-6: a resting value that
does not fit is MARKED, not dropped"`; `tests/test_workshop_document.cpp` case `"HD-5: a long
property draft is a window, and no part of it is lost"`, case `"HD-5: a resize reconciles the
property window with no path of its own"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-06 — A `SurfaceExtent` must not drop a live draft

LAW — `refocus_keeping_draft` rebuilds the rows and hands the draft, its refusal and the cursor back; every other `rebuild_rows` caller follows a new selection or document, where dropping it is right.

MEANS
- `Name` is a row every object has: a draft carried across a selection would land elsewhere.

PROVEN BY — `workshop/screen_bindings.cpp` `refocus_keeping_draft`, `inspector_rows`;
`workshop/weave_save.cpp` `rebuild_rows`; `workshop/weave_seam.cpp` `refresh_inspector`;
`workshop/property.hpp` `Row::resume`; `tests/test_workshop_document.cpp` case `"HD-5: a surface
extent does not take a maker's hands off a draft"`; `tests/test_workshop_panels.cpp` case `"HD-6:
a resize reconciles the row count, the window and the draft together"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-07 — `share_body_rows` is max-min fair sharing

LAW — `share_body_rows` is max-min fair: each list gets what it needs, spare stays spare, what both cannot have is shared equally with an unneeded half going to the other; 50/50 is a consequence.

MEANS
- growing the panel never shrinks either list;
- pinned as properties over every budget from 0 to 200.

PROVEN BY — `workshop/screen_info.cpp` `share_body_rows`; `workshop/screen.hpp` `BodyShare`,
`list_demand`; `tests/test_workshop_panels.cpp` case `"HD-7: the sharing policy is monotonic,
bounded and never starves either list"`, case `"HD-7: spare room stays spare, and the heading sits
under the last name"`, case `"HD-7: growing the window gives OBJECTS more and never gives
PROPERTIES less"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-08 — `OBJECTS` is the region's first prose row; `PROPERTIES` moves

LAW — The heading rows are reserved before either list is offered anything; body rows begin at zero beneath them and the press inverse subtracts the heading, so a press on a heading names no row.

PROVEN BY — `workshop/screen_info.cpp` `info_body_place`, `info_body_at`, `paint_info`;
`workshop/screen.hpp` `kInfoHeadingRows`, `InfoBodyPlace::region_x`;
`tests/test_workshop_panels.cpp` case `"HD-7: neither list paints through the other, at any
extent"`; `tests/test_workshop_document.cpp` case `"HD-9: `PROPERTIES` is set on a ground, and the
row above it is not"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-09 — An object row is fitted whole, and a press on it selects in command mode only

LAW — `object_row_text` puts the identity before the name and cuts the row at the body's width; `objects_press` selects a visible row in command mode only, and says so while a draft is live.

PROVEN BY — `workshop/screen_info.cpp` `object_row_text`, `object_press_at`, `object_row_full`;
`workshop/weave_seam.cpp` `objects_press`; `tests/test_workshop_panels.cpp` case `"HD-7: a press
on a visible object row selects it, through the row's own geometry"`, case `"HD-7: a press on an
object row is REFUSED while a property draft is live"`, case `"HD-7: a long object name is bounded
VISIBLY, and the document keeps all of it"`.
WHY — `agents/decisions/one-body-two-lists.md`

## WL-INFO-10 — With Info removed, the inspector's keys say so and open no draft

LAW — A cursor or edit key with Info not showing says so and which key opens the panel, opens no draft and moves no cursor; silence would not tell a removed panel from a broken tool.

PROVEN BY — `workshop/weave_save.cpp` `inspector_absent`; `tests/test_workshop_panels.cpp` case
`"the inspector's keys say so when Info is not showing, and open no draft"`.
WHY — `agents/decisions/one-body-two-lists.md`
