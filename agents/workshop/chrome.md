# Workshop law — chrome

Register `WL-CHROME`: a pane's edge, the boundary rungs, the ring and the chrome roles. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-CHROME-01 — A pane has an edge, inside its own rectangle

LAW — Every ordinary pane and framed transient surface draws a boundary of one unit of the active face on every side, subtracted from the rectangle `bounds_of` answered.

MEANS
- the outer rectangle — authored, dragged, hit-tested, ringed — is unchanged by the edge;
- a terminal spends one cell; the shipped window spends one device pixel, drawn inside the pane.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-02 — Chrome thickness is presentation, never authored

LAW — How finely the boundary is drawn is the face's: `chrome_grain(sc)` from the medium's reported `cell_px`. It is on no shape, in no file, and rewrites no authored value.

MEANS
- the thinner boundary changed no foreground law and no byte of any setup;
- the smallest span a medium can show is one of its own device units, and that is the grain.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-03 — A pane too small for a boundary draws none

LAW — `pane_inside` tries the face's own unit, then one cell, then no boundary, and returns the first that leaves an interior the face can set; each rung is larger, so it cannot oscillate.

MEANS
- a face describing the interior in cells (a terminal, no font, a face too tall) pays the cell;
- a two-cell pane exists on every face: no ring on a terminal, a one-pixel edge on the window;
- a pane on the last rung wears no selected ink, because there is no ring to colour.

DOES NOT MEAN
- that the arrangement handles, the desk's stepping or the notice change with it.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-04 — The backdrop is the border

LAW — The frame painter pushes the outer rectangle and the body region over it owns its ground, so the visible ring is exactly the outer rectangle minus the interior; there is no border painter.

MEANS
- there is no border painter and no thickness on any paint call to get wrong;
- a face drawing the interior in pixels leaves a one-pixel ring; in cells, a one-cell ring.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-05 — Every body resolution goes through `pane_inside`

LAW — Every resolution of a pane's body calls `pane_inside` on the rectangle it is handed, so painter, press inverse and room grant are one geometry by construction.

MEANS
- `pane_interior`'s thickness is a required argument: a default would be the forgotten call site;
- `PaneInside` carries the `RegionFit` beside the rectangle, so no interior is fitted twice;
- the graphical room is the post-chrome pixels, and selection cannot move it.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-06 — A content-sized surface reserves the coarsest boundary

LAW — `chrome_outer_of` grows a content rectangle by `kChromeCells` on every side on every face, so a content-sized popup's placement never depends on the face.

MEANS
- a finer face draws a thinner ring inside that reservation and hands the slack to the interior;
- a popup sized for pixels would cut a row off itself the moment a terminal drew it.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-07 — Selection changes the ink and not one number

LAW — Ordinary and selected chrome have identical geometry — the same outer rect, body rect, capacity and press inverse — so pointing at a pane never moves its contents.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-08 — Three chrome roles, from the closed vocabulary

LAW — Three chrome roles: ordinary chrome wears the fill ink, selected chrome the accent ink, a transient surface the muted ink, all from the closed role vocabulary, with no per-medium palette.

MEANS
- the desk and the document say selection with one word, `kAccent`;
- on a character medium a transient edge over bare workspace is the hole its interior clears.

DOES NOT MEAN
- that an opaque background changes the three roles used for pane chrome.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-09 — A pane whose holder answers its manual wears a `?` and a gear at its top right

LAW — A pane's header ends in a gear and a `?` exactly while its holder takes `PaneDocumentRequested` and the application declares the row that shows a manual; each is read again before every paint.

MEANS
- a press on the `?` asks that row about the pane, as the press's act; on the gear, Workshop's own menu of its settings opens;
- a press on either is spent there and moves neither the selection nor the keys;
- a header too narrow for its title's mark and both, or a pane wearing no header, wears neither.

DOES NOT MEAN
- that Workshop shows or keeps the manual: the application's row shows it, and nothing is held.

WHY — `agents/decisions/a-panes-manual-is-shown-by-the-application.md`

## Do not assume

- That the interior is always the outer rectangle less one cell — it is less one unit of the
  face, and less nothing on a pane too small for any boundary (WL-CHROME-01, WL-CHROME-03).

## Proven by

### WL-CHROME-01
`workshop/screen_chrome.cpp` pane_inside pane_interior
`workshop/screen.hpp` pane_interior chrome_grain kChromeCells kChromePx
`graphics/pixels.hpp` px_of_one_device
`tests/test_workshop_screen.cpp` "a pane's interior is its outer rectangle less one cell of chrome"
"the chrome a pane wears is one unit of the face in front of the weaver" "the graphical boundary is
one device pixel, drawn INSIDE the pane"

### WL-CHROME-02
`workshop/screen.hpp` chrome_grain kChromeCells
`workshop/setup_persist.hpp` kFormatVersion
`tests/test_workshop_panes_window.cpp` "the thinner boundary rewrites no authored value and no
foreground law"
`tests/test_graphics.cpp` "the smallest span a medium can SHOW is one of its own device units"

### WL-CHROME-03
`workshop/screen_chrome.cpp` pane_inside detail::pane_inside_at
`workshop/screen.hpp` chrome_grain kChromePx
`tests/test_workshop_screen.cpp` "a face that describes an interior in CELLS pays the cell" "a
two-cell pane keeps its content and drops its boundary"

### WL-CHROME-04
`workshop/screen_chrome.cpp` pane_inside
`workshop/screen_pane_state.cpp` paint_pane_frame
`workshop/screen_planes.hpp` kGroundOwn
`tests/test_workshop_screen.cpp` "the ring IS the backdrop the interior did not cover, on both
faces" "the border a weaver sees and the room a pane spends are one subtraction"

### WL-CHROME-05
`workshop/screen_external.cpp` external_body_place
`workshop/screen_pane_state.cpp` prose_place
`workshop/screen_layouts.cpp` layouts_body
`workshop/screen.hpp` pane_interior PaneInside RegionFit ProsePlace
`workshop/screen_chrome.cpp` pane_interior
`tests/test_workshop_screen.cpp` "the border a weaver sees and the room a pane spends are one
subtraction"
`tests/test_workshop_panes_window.cpp` "the graphical room is the post-chrome pixels, and selection
cannot move it"

### WL-CHROME-06
`workshop/screen.hpp` chrome_outer_of kChromeCells
`tests/test_workshop_screen.cpp` "a content-sized surface reserves the coarsest boundary, once"

### WL-CHROME-07
`workshop/screen.hpp` kPaneChrome kPaneChromeSelected
`workshop/screen_chrome.cpp` pane_inside
`tests/test_workshop_screen.cpp` "selected and ordinary differ in INK, and in nothing else" "the
selected pane wears its own chrome, and only it"

### WL-CHROME-08
`workshop/screen.hpp` kPaneChrome kPaneChromeSelected kTransientChrome
`graphics/role.hpp` kFill kAccent kMuted
`tests/test_workshop_screen.cpp` "selected and ordinary differ in INK, and in nothing else" "a
transient surface stays over the pane it covers, selected or not"

### WL-CHROME-09
`workshop/panes.hpp` ExternalPane::manual
`workshop/weave_external.cpp` WorkshopWeave::note_manuals WorkshopWeave::press_manual
`workshop/weave_pointer.cpp` WorkshopWeave::press_settings
`workshop/screen.hpp` kChromeMarks external_header_row chrome_mark_at
`workshop/screen_external.cpp` external_header_row chrome_mark_at
`workshop/desktop_seam_vocabulary.hpp` kManualAction
`workshop/context.hpp` kSettings kOnSettings
`tests/test_workshop_panes_builder.cpp` "WL-CHROME-09: a pane whose holder answers its manual wears
a `?` and a gear at its top right; one that does not, or a desk with no row to show one, wears
neither" "WL-KEY-16: the band's spelling and the Hotkeys view's are set apart from a pane's gear,
each changing only its own surface, the `?` spelling as the Hotkeys view does"
