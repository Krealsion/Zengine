# Workshop law — chrome

Register `WL-CHROME`: a pane's edge, the boundary rungs, the ring and the chrome roles. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-CHROME-01 — A pane has an edge, inside its own rectangle

LAW — Every ordinary pane and framed transient surface draws a boundary of one unit of the active face on every side, subtracted from the rectangle `bounds_of` answered.

MEANS
- the outer rectangle — authored, dragged, hit-tested, ringed — is unchanged by the edge;
- a terminal spends one cell; the shipped window spends one device pixel, drawn inside the pane.

PROVEN BY — `workshop/screen.hpp` `pane_inside`, `pane_interior`, `chrome_grain`,
`kChromeCells`, `kChromeSubs`; `surface/region.hpp` `subs_of_one_device`;
`tests/test_workshop_screen.cpp` case
`"WUX-5: a pane's interior is its outer rectangle less one cell of chrome"`, case `"WUX-8: the
chrome a pane wears is one unit of the face in front of the maker"`, case `"WUX-8: the
graphical boundary is one device pixel, drawn INSIDE the pane"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-02 — Chrome thickness is presentation, never authored

LAW — How finely the boundary is drawn is the face's: `chrome_grain(sc)` from the medium's reported `cell_px`. It is on no shape, in no file, and rewrites no authored value.

MEANS
- the thinner boundary changed no foreground law and no byte of any setup;
- the smallest span a medium can show is one of its own device units, and that is the grain.

PROVEN BY — `workshop/screen.hpp` `chrome_grain`, `kChromeCells`; `workshop/setup_persist.hpp`
`kFormatVersion`; `tests/test_workshop_panes_window.cpp` case `"WUX-8: the thinner boundary
rewrites no authored value and no foreground law"`; `tests/test_surface.cpp` case `"WUX-8: the
smallest span a medium can SHOW is one of its own device units"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-03 — A pane too small for a boundary draws none

LAW — `pane_inside` tries the face's own unit, then one cell, then no boundary, and returns the first that leaves an interior the face can set; each rung is larger, so it cannot oscillate.

MEANS
- a face describing the interior in cells (a terminal, no font, a face too tall) pays the cell;
- a two-cell pane exists on every face: no ring on a terminal, a one-pixel edge on the window;
- a pane on the last rung wears no selected ink, because there is no ring to colour.

DOES NOT MEAN
- that the arrangement handles, the desk's stepping, the notice or the picker change with it.

PROVEN BY — `workshop/screen.hpp` `pane_inside`, `detail::pane_inside_at`, `chrome_grain`,
`kChromeSubs`; `tests/test_workshop_screen.cpp` case `"WUX-8: a face that describes an interior
in CELLS pays the cell"`, case `"WUX-12/SC-2: a two-cell pane keeps its content and drops its
boundary"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-04 — The backdrop is the border

LAW — The frame painter pushes the outer rectangle and the body region over it owns its ground, so the visible ring is exactly the outer rectangle minus the interior; there is no border painter.

MEANS
- there is no border painter and no thickness on any paint call to get wrong;
- a face drawing the interior in pixels leaves a one-pixel ring; in cells, a one-cell ring.

PROVEN BY — `workshop/screen.hpp` `pane_inside`, `paint_panel_frame`; `surface/vocabulary.hpp`
`kGroundOwn`; `tests/test_workshop_screen.cpp` case `"WUX-8: the ring IS the backdrop the interior
did not cover, on both faces"`, case `"WUX-5: the border a maker sees and the room a pane spends
are one subtraction"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-05 — Every body resolution goes through `pane_inside`

LAW — Every resolution of a pane's body calls `pane_inside` on the rectangle it is handed, so painter, press inverse and room grant are one geometry by construction.

MEANS
- `pane_interior`'s thickness is a required argument: a default would be the forgotten call site;
- `PaneInside` carries the `RegionFit` beside the rectangle, so no interior is fitted twice;
- the graphical room is the post-chrome pixels, and selection cannot move it.

PROVEN BY — `workshop/screen.hpp` `external_body_place`, `info_body_place`, `panel_prose_place`,
`layouts_body`, `pane_interior`, `PaneInside`, `RegionFit`, `PanelProsePlace`;
`tests/test_workshop_screen.cpp` case `"WUX-5: the border a maker sees and the room a pane spends
are one subtraction"`; `tests/test_workshop_panes_window.cpp` case `"WUX-8: the graphical room is
the post-chrome pixels, and selection cannot move it"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-06 — A content-sized surface reserves the coarsest boundary

LAW — `chrome_outer_of` grows a content rectangle by `kChromeCells` on every side on every face, so a content-sized popup's placement never depends on the face.

MEANS
- a finer face draws a thinner ring inside that reservation and hands the slack to the interior;
- a popup sized for pixels would cut a row off itself the moment a terminal drew it.

PROVEN BY — `workshop/screen.hpp` `chrome_outer_of`, `kChromeCells`;
`tests/test_workshop_screen.cpp` case `"WUX-8: a content-sized surface reserves the coarsest
boundary, once"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-07 — Selection changes the ink and not one number

LAW — Ordinary and selected chrome have identical geometry — the same outer rect, body rect, capacity and press inverse — so pointing at a pane never moves its contents.

PROVEN BY — `workshop/screen.hpp` `kPaneChrome`, `kPaneChromeSelected`, `pane_inside`;
`tests/test_workshop_screen.cpp` case `"WUX-8: selected and ordinary differ in INK, and in
nothing else"`, case `"WUX-5: the selected pane wears its own chrome, and only it"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-08 — Three chrome roles, from the closed vocabulary

LAW — Three chrome roles and no fifth: ordinary chrome wears the fill ink, selected chrome the accent ink, a transient surface the muted ink, all from the closed role vocabulary, with no per-medium palette.

MEANS
- the desk and the document say selection with one word, `kAccent`;
- on a character medium a transient edge over bare workspace is the hole its interior clears.

DOES NOT MEAN
- that the hole is a defect to repair with a fifth role — the shipped face has three inks.

PROVEN BY — `workshop/screen.hpp` `kPaneChrome`, `kPaneChromeSelected`, `kTransientChrome`;
`surface/vocabulary.hpp` `kFill`, `kAccent`, `kMuted`; `tests/test_workshop_screen.cpp` case
`"WUX-8: selected and ordinary differ in INK, and in nothing else"`, case `"WUX-5: a transient
surface stays over the pane it covers, selected or not"`.
WHY — `agents/decisions/pane-boundary-rungs.md`

## Do not assume

- That the interior is always the outer rectangle less one cell — it is less one unit of the
  face, and less nothing on a pane too small for any boundary (WL-CHROME-01, WL-CHROME-03).
