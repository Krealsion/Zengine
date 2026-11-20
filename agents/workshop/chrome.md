# Workshop law — chrome

Register `WL-CHROME`: a pane's edge, the boundary rungs, the ring, the chrome's colours and the looks. One law
per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-CHROME-01, WL-CHROME-03, WL-CHROME-04, WL-CHROME-06, WL-CHROME-07, WL-CHROME-08, WL-CHROME-09, WL-CHROME-11, WL-CHROME-12

## WL-CHROME-02 — Chrome thickness is presentation, never authored

LAW — How finely the boundary is drawn is the face's: `chrome_grain(sc)` from the medium's reported `cell_px`. It is on no shape, in no file, and rewrites no authored value.

MEANS
- the thinner boundary changed no foreground law and no byte of any setup;
- the smallest span a medium can show is one of its own device units, and that is the grain.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-05 — Every body resolution goes through `pane_inside`

LAW — Every resolution of a pane's body calls `pane_inside` on the rectangle it is handed, so painter, press inverse and room grant are one geometry by construction.

MEANS
- `pane_interior`'s thickness is a required argument: a default would be the forgotten call site;
- `PaneInside` carries the `RegionFit` beside the rectangle, so no interior is fitted twice;
- the graphical room is the post-chrome pixels, and selection cannot move it.

WHY — `agents/decisions/pane-boundary-rungs.md`

## WL-CHROME-10 — Night, day and block ship beside Workshop, and a layout of none wears night's look

LAW — `night`, `day` and `block` ship as themes beside Workshop, each a window file and a terminal file, which the theme role reads; a layout wearing no theme wears the pool's own look, night's.

MEANS
- an older file naming `workshop.skin` reads as a theme: a row's as its row's theme, Workshop's own as the theme of each layout wearing none, `classic` as `night`;
- in a terminal night's and day's text and grounds are the terminal's default colours, a pane's frame grey, any other the nearest ink; block gives each its own ink.

DOES NOT MEAN
- that `classic` is offered: no row, menu or directory lists it.

WHY — `agents/decisions/night-day-and-block-ship.md`

## Do not assume

- That the interior is always the outer rectangle less one cell — it is less one unit of the
  face, and less nothing on a pane too small for any boundary.

## Proven by

### WL-CHROME-02
`workshop/screen.hpp` chrome_grain kChromeCells
`workshop/setup_persist.hpp` kFormatVersion
`tests/test_workshop_panes_window.cpp` "the thinner boundary rewrites no authored value and no
foreground law"
`tests/test_graphics.cpp` "the smallest span a medium can SHOW is one of its own device units"

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

### WL-CHROME-10
`workshop/setup_persist.hpp` kRetiredSkinKey kRetiredClassic
`workshop/prefs_persist.hpp` kRetiredSkinKey
`workshop/weave_run.cpp` arrange_graphics
`tests/test_workshop_desk.cpp` "each graphics area wears the palette named for the live
layout's themes, a pane's row's theme laid over them for its area and its frame alone, and a
palette is said only when an area's moved"
`tests/test_theme.cpp` "each shipped theme laid alone is the palette the build compiled from its
two files, so no look changes when the theme weave paints it"
`tests/test_graphics.cpp` "in a terminal night and day show their text and grounds in the terminal's
default colours, a pane's frame grey and every other colour as the nearest of its sixteen inks; block
keeps an ink of its own for each, its frame grey too"
