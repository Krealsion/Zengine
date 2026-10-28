# The mutants `tests/test_workshop_screen.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_screen.cpp`](../test_workshop_screen.cpp) claims its cases catch, the comment
naming the entry by its path and heading. [The corpus's front page](README.md) says how an entry
reads and how `tests/mutants/run.py` runs it.

## place-word-read-as-pixels

`pane_part` reads every place word as `pixels`.

In `workshop/screen_chrome.cpp`:

```diff
-        e.place = ui_place_of(authored->place);
+        e.place = ui::Place{ui::kPlacePixels, authored->place.x, authored->place.y};
```

Red in `zengine-workshop-screen-tests`:

- an anchored pane keeps its corner where its word puts it, at every size of the window

## center-read-as-default

A `center` place is read as `default`.

In `workshop/screen_chrome.cpp`:

```diff
-    if (place != pane_unit::kDefault) {
-        e.context = kRoomPart;
+    if (place != pane_unit::kDefault && place != ui::kPlaceCenter) {
+        e.context = kRoomPart;
```

Red in `zengine-workshop-screen-tests`:

- a centred pane stands in the middle of the room, and a fill runs to the room's far edge

## share-place-numbers-dropped

`ui_place_of` drops a share place's down number.

In `workshop/setup.hpp`:

```diff
-    if (ui::place_reads_y(p.mode)) {
-        out.y = p.y;
+    if (ui::place_reads_y(p.mode) && !ui::place_reads_share(p.mode)) {
+        out.y = p.y;
```

Red in `zengine-workshop-screen-tests`:

- a bar a third of the way down stays a third down at every size, in both windows, its ends at the
  room's sides

## pulled-share-written-as-pixels

A hand pulling a share's edge writes its extent as pixels.

In `workshop/setup.hpp`:

```diff
-            if (pulled) {
-                at.amount = share_reaching(m.length, p.span, now);
-            }
+            if (pulled) {
+                at = PaneSize{pane_unit::kPixels, m.length};
+            }
```

Red in `zengine-workshop-screen-tests`:

- a hand pulling a share's edge writes the share whose edge is the hand's pixel, at every pixel of
  the widest room

## share-place-moved-as-an-anchor

A hand moving a share place writes it as an anchor's count of pixels.

In `workshop/setup.hpp`:

```diff
-    } else if (ui::place_reads_share(place.mode)) {
+    } else if (false) {
```

Red in `zengine-workshop-screen-tests`:

- a hand moving a share place writes the share whose edge is the hand's pixel, at every pixel of the
  widest room

## fill-far-edge-as-pixels

A fill pulled at its far edge is written as pixels, and nothing is said.

In `workshop/setup.hpp`:

```diff
-            if (pulled && far_pulled) {
+            if (pulled && far_pulled && false) {
```

Red in `zengine-workshop-screen-tests`:

- a fill pulled at its far edge, with nothing sharing its frame, writes the share whose edge is the
  hand's pixel, and says so

## refused-axis-sized-anyway

An axis whose offset was refused has its size written anyway.

In `workshop/setup.hpp`:

```diff
-        mx.lands = h_lands;
-        my.lands = v_lands;
```

Red in `zengine-workshop-screen-tests`:

- an anchored pane pulled past its own edge is refused across and still follows the hand down

## anchored-move-as-pixels

A hand moving an anchored pane writes its place as pixels from the room's top-left.

In `workshop/setup.hpp`:

```diff
-    const bool centred_moved = (was_x == ui::Anchor::Center && (mx.near_moved || mx.far_moved)) ||
+    const bool centred_moved = true || (was_x == ui::Anchor::Center && (mx.near_moved || mx.far_moved)) ||
```

Red in `zengine-workshop-screen-tests`:

- a hand moves an anchored pane in its word, a centred one into pixels, and pulls a fitted size into
  pixels, each said

## drag-without-snap

A drag never snaps.

In `workshop/weave_pointer.cpp`:

```diff
-        arrange_motion(here.px.x, here.px.y, (m.modifiers & input::mod::kAlt) == 0, mail);
+        arrange_motion(here.px.x, here.px.y, false && (m.modifiers & input::mod::kAlt) == 0, mail);
```

Red in `zengine-workshop-screen-tests`:

- a pane dragged near another pane's edge or the room's comes to it, and the edge met is marked
  across the room while held

## alt-ignored

A drag snaps with Alt held.

In `workshop/weave_pointer.cpp`:

```diff
-        arrange_motion(here.px.x, here.px.y, (m.modifiers & input::mod::kAlt) == 0, mail);
+        arrange_motion(here.px.x, here.px.y, (m.modifiers & input::mod::kAlt) == 0 || true, mail);
```

Red in `zengine-workshop-screen-tests`:

- Alt held while dragging sets every snap aside, and the hand places to the pixel

## reach-left-at-the-constant

The snap reaches `kPaneSnapReachPx`, whatever `workshop.snap-reach` says.

In `workshop/weave_arrange.cpp`:

```diff
-            session_.snap_reach_px);
-        g.met_x = got.met_x;
+            kPaneSnapReachPx);
+        g.met_x = got.met_x;
```

In `workshop/weave_arrange.cpp`:

```diff
-            snap_pane_window(want, kNoPaneEdge, lines_after(want, base), session_.snap_reach_px);
+            snap_pane_window(want, kNoPaneEdge, lines_after(want, base), kPaneSnapReachPx);
```

Red in `zengine-workshop-screen-tests`:

- the snap reaches as far as `workshop.snap-reach` says: eleven pixels meets an edge ten away, six
  misses one seven away

## step-left-at-the-constant

The coarse grow spends `kCoarseStepCells`, whatever `workshop.arrange-step` says.

In `workshop/weave_arrange.cpp`:

```diff
-        arrange_grow(+session_.arrange_step_cells, +session_.arrange_step_cells, mail);
+        arrange_grow(+kCoarseStepCells, +kCoarseStepCells, mail);
```

Red in `zengine-workshop-screen-tests`:

- the coarse step is `workshop.arrange-step` cells, read from Workshop's settings file

## stack-rows-left-at-the-constant

A stacked pane is `kStackRows` tall, whatever `workshop.stack-rows` says.

In `workshop/screen.hpp`:

```diff
-    s.stack_rows = std::clamp(stack_rows, kMinStackRows, kMaxStackRows);
+    s.stack_rows = kStackRows + 0 * stack_rows;
```

Red in `zengine-workshop-screen-tests`:

- a stacked pane is `workshop.stack-rows` tall, read from Workshop's settings file, and a file still
  keeping `workshop.side-columns` reads

## edge-pull-without-snap

A pulled edge never snaps.

In `workshop/weave_pointer.cpp`:

```diff
-        arrange_motion(here.px.x, here.px.y, (m.modifiers & input::mod::kAlt) == 0, mail);
+        arrange_motion(here.px.x, here.px.y, false && (m.modifiers & input::mod::kAlt) == 0, mail);
```

Red in `zengine-workshop-screen-tests`:

- an edge pulled near another pane's edge or the room's comes to it, its opposite edge held

## keys-snapped

The arrow keys' place snaps to the room's edge within the reach.

In `workshop/weave_arrange.cpp`:

```diff
-void WorkshopWeave::arrange_place(std::int64_t x, std::int64_t y, loom::Mail& mail) {
+void WorkshopWeave::arrange_place(std::int64_t x, std::int64_t y, loom::Mail& mail) {
+    if (x > 0 && x <= session_.snap_reach_px) {
+        x = 0;
+    }
+    if (y > 0 && y <= session_.snap_reach_px) {
+        y = 0;
+    }
```

Red in `zengine-workshop-screen-tests`:

- the arrow keys and a typed value place exactly, near an edge or not

## cells-without-snap

A drag by cells never snaps.

In `workshop/weave_pointer.cpp`:

```diff
-        arrange_motion(here.px.x, here.px.y, (m.modifiers & input::mod::kAlt) == 0, mail);
+        arrange_motion(here.px.x, here.px.y, false && (m.modifiers & input::mod::kAlt) == 0, mail);
```

Red in `zengine-workshop-screen-tests`:

- in a terminal a hand moving by cells meets an edge between them, and the mark is a cell across the
  room

## lines-of-the-desk-as-it-stands

The snap's lines are measured on the desk as it stands, the held pane in its old place.

In `workshop/weave_arrange.cpp`:

```diff
-        return pane_snap_lines(session_.panes, after, sc, *kind);
+        return pane_snap_lines(session_.panes, session_.setup.active, sc, *kind);
```

Red in `zengine-workshop-screen-tests`:

- a pane dragged out of the stack meets the desk as its move leaves it, the pane below risen

## only-default-unplaced

Only a `default` axis is taken as one its numbers alone do not place.

In `workshop/screen_pane_state.cpp`:

```diff
-    return row->place.mode != pane_unit::kPixels || row->width.mode != pane_unit::kPixels ||
-           row->height.mode != pane_unit::kPixels;
+    return row->place.mode == pane_unit::kDefault || row->width.mode == pane_unit::kDefault ||
+           row->height.mode == pane_unit::kDefault;
```

Red in `zengine-workshop-screen-tests`:

- which parts of a pane's window its numbers alone do not place

## readout-says-dash

The readout says `-` for every axis that is not pixels, and no place word.

In `workshop/screen_pane_state.cpp`:

```diff
-    } else if (mode != pane_unit::kDefault) {
-        // A PLACE WORD, and the offsets it reads: a share place's as shares.
+    } else if (false) {
+        // A PLACE WORD, and the offsets it reads: a share place's as shares.
```

In `workshop/screen_pane_state.cpp`:

```diff
-        case ui::kSizeShare: return share_text(s.amount);
-        case ui::kSizeFit:
-        case ui::kSizeFill: return std::string(ui::size_word(s.mode));
+        case ui::kSizeShare:
+        case ui::kSizeFit:
+        case ui::kSizeFill: return std::string("-");
```

Red in `zengine-workshop-screen-tests`:

- the readout says each word, and where the words put the pane

## share-place-said-as-counts

The readout says a share place's numbers as counts of the face's unit.

In `workshop/screen_pane_state.cpp`:

```diff
-        if (ui::place_reads_share(mode)) {
-            text += " " + share_text(row->place.x) + "," + share_text(row->place.y);
+        if (false) {
+            text += " " + share_text(row->place.x) + "," + share_text(row->place.y);
```

Red in `zengine-workshop-screen-tests`:

- the readout says each word, and where the words put the pane

## interior-is-its-argument

`pane_interior` answers its own argument.

In `workshop/screen.hpp`:

```diff
-                                        std::int64_t chrome_px) noexcept {
-    const std::int64_t w = outer.w - 2 * chrome_px;
+                                        std::int64_t chrome_px) noexcept {
+    (void)chrome_px;
+    return outer;
+    const std::int64_t w = outer.w - 2 * chrome_px;
```

Red in `zengine-workshop-screen-tests`:

- a pane's interior is its outer rectangle less one cell of chrome

## interior-guard-dropped

`pane_interior` answers a rectangle of no width or height as it computed it.

In `workshop/screen.hpp`:

```diff
-    const std::int64_t h = outer.h - 2 * chrome_px;
-    if (w <= 0 || h <= 0) {
-        return PixelRect{};
-    }
+    const std::int64_t h = outer.h - 2 * chrome_px;
```

Red in `zengine-workshop-screen-tests`:

- a pane's interior is its outer rectangle less one cell of chrome

## painter-inset-alone

The painter's prose place keeps a cell's inset while the body and the room go thin.

In `workshop/screen_pane_state.cpp`:

```diff
-    const PaneInside inside = pane_inside(b, sc);
-    p.inside = inside.rect;
+    const PaneInside inside = detail::pane_inside_at(b, sc, kChromePx);
+    p.inside = inside.rect;
```

Red in `zengine-workshop-screen-tests`:

- the border a weaver sees and the room a pane spends are one subtraction

## hit-walks-the-authored-order

`occupied_at` walks the authored order, not the effective one with the selection lifted.

In `workshop/screen_chrome.cpp`:

```diff
-    // Every test is the pointer's own grain against fine geometry (the aligned-span law), so a
-    // pane answers on exactly the cells and pixels it paints.
-    const std::vector<std::int64_t> order = effective_pane_order(setup, panes);
+    // Every test is the pointer's own grain against fine geometry (the aligned-span law), so a
+    // pane answers on exactly the cells and pixels it paints.
+    const std::vector<std::int64_t> order = presentation_order(setup, panes);
```

Red in `zengine-workshop-screen-tests`:

- selecting a pane lifts it, in the picture and under the hand at once

## paint-walks-the-authored-order

`paint_panes` walks the authored order while the hit order lifts the selection.

In `workshop/screen_compose.cpp`:

```diff
-    for (const std::int64_t kind : effective_pane_order(s.setup.active, panes)) {
+    for (const std::int64_t kind : presentation_order(s.setup.active, panes)) {
```

Red in `zengine-workshop-screen-tests`:

- selecting a pane lifts it, in the picture and under the hand at once

## every-pane-ordinary-chrome

`paint_panes` hands every pane the ordinary chrome.

In `workshop/screen_compose.cpp`:

```diff
-        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
+        const std::int64_t chrome = p.kind == lifted && false ? kPaneChromeSelected : kPaneChrome;
```

Red in `zengine-workshop-screen-tests`:

- the selected pane wears its own chrome, and only it

## selection-writes-front

A press that selects a pane writes it to the front of the authored order.

In `workshop/weave_pointer.cpp`:

```diff
-        session_.panes.selected = here.occupied ? here.kind : kNoPaneKind;
+        session_.panes.selected = here.occupied ? here.kind : kNoPaneKind;
+        for (SetupPane& row : session_.setup.active.panes) {
+            if (resolve_pane(row.ref, session_.panes) == std::optional<std::int64_t>(session_.panes.selected)) {
+                row.front = static_cast<std::int64_t>(session_.setup.active.panes.size());
+            }
+        }
```

Red in `zengine-workshop-screen-tests`:

- the selection lift never reaches the file, and no session starts with one

## lift-over-the-overlay

The contextual surface is painted before the panes, so the lifted pane stands over it.

In `workshop/screen_compose.cpp`:

```diff
-    // The contextual-action surface, last in the band: over everything as the band's latest
-    // gesture, and it takes the band's keys first for the same reason (`keyboard_context`).
-    detail::on_own_layer(c, [&](ScreenPlane& layer) {
-        paint_context(layer, s, sc);
-    });
```

In `workshop/screen_compose.cpp`:

```diff
-    const std::int64_t lifted = selected_pane(panes);
+    const std::int64_t lifted = selected_pane(panes);
+    detail::on_own_layer(c, [&](ScreenPlane& layer) {
+        paint_context(layer, s, sc);
+    });
```

Red in `zengine-workshop-screen-tests`:

- a transient surface stays over the pane it covers, selected or not

## heading-row-back

The contextual surface reserves a row beyond its actions, as a heading's.

In `workshop/screen_attention.cpp`:

```diff
-    const std::int64_t want_rows = static_cast<std::int64_t>(rows.size());
+    const std::int64_t want_rows = static_cast<std::int64_t>(rows.size()) + 1;
```

Red in `zengine-workshop-screen-tests`:

- the contextual surface is its actions, and its width is theirs

## gesture-claim-in-the-layouts-pane

The Layouts pane's status line teaches a key the keymap owns.

In `workshop/screen_layouts.cpp`:

```diff
-    line += status == setup_link::kCurrent ? kSetupLinkCurrent : kSetupLinkModified;
+    line += status == setup_link::kCurrent ? kSetupLinkCurrent : kSetupLinkModified;
+    line += "  ctrl+n new layout";
```

Red in `zengine-workshop-screen-tests`:

- no ordinary pane spends a row teaching a key the keymap already owns

## grain-always-a-cell

`chrome_grain` answers a whole cell whatever the medium said.

In `workshop/screen.hpp`:

```diff
-    return graphics::px_of_one_device(sc.cell_px);
-}
+    (void)sc;
+    return kChromePx;
+}
```

Red in `zengine-workshop-screen-tests`:

- the chrome a pane wears is one unit of the face in front of the weaver

## grain-always-a-pixel

`chrome_grain` answers one pixel on every medium, the terminal's too.

In `workshop/screen.hpp`:

```diff
-    return graphics::px_of_one_device(sc.cell_px);
-}
+    (void)sc;
+    return 1;
+}
```

Red in `zengine-workshop-screen-tests`:

- the chrome a pane wears is one unit of the face in front of the weaver

## ring-outside-the-pane

A pane's frame is drawn a pixel outside the pane's own rectangle.

In `workshop/screen_pane_state.cpp`:

```diff
-    layer.rects.push_back(wire_rect_of(b, role));
+    layer.rects.push_back(wire_rect_of(PixelRect{b.x - 1, b.y - 1, b.w + 2, b.h + 2}, role));
```

Red in `zengine-workshop-screen-tests`:

- the graphical boundary is one device pixel, drawn INSIDE the pane

## body-keeps-the-cell-inset

A pane's body and room keep a cell's inset while its paint goes thin.

In `workshop/screen_pane_state.cpp`:

```diff
-    const PaneInside inside = pane_inside(b, sc);
+    const PaneInside inside = detail::pane_inside_at(b, sc, kChromePx);
```

Red in `zengine-workshop-screen-tests`:

- the interior a window no longer reserves goes back to the pane

## cell-reservation-kept

The room a pane is granted keeps the cell-inset capacity while its body goes thin.

In `workshop/screen_pane_state.cpp`:

```diff
-    p.fit = inside.fit;
+    p.fit = detail::pane_inside_at(b, sc, kChromePx).fit;
```

Red in `zengine-workshop-screen-tests`:

- the interior a window no longer reserves goes back to the pane

## thin-candidate-always

`pane_inside` answers the thin candidate on every face.

In `workshop/screen_chrome.cpp`:

```diff
-    if (fine < kChromePx) {
-        const PaneInside thin = detail::pane_inside_at(outer, sc, fine);
-        if (thin.fit.graphical()) {
-            return thin;
-        }
-    }
+    return detail::pane_inside_at(outer, sc, fine);
```

Red in `zengine-workshop-screen-tests`:

- a face that describes an interior in CELLS pays the cell

## painter-strokes-its-own-border

The frame's painter strokes a border line of its own over the pane's top edge.

In `workshop/screen_pane_state.cpp`:

```diff
-    layer.rects.push_back(wire_rect_of(b, role));
+    layer.rects.push_back(wire_rect_of(b, role));
+    layer.rects.push_back(wire_rect_of(PixelRect{b.x, b.y, b.w, 1}, role));
```

Red in `zengine-workshop-screen-tests`:

- the ring IS the backdrop the interior did not cover, on both faces

## selected-inset-further

The painter insets the selected pane a unit further than an ordinary one.

In `workshop/screen_compose.cpp`:

```diff
-        const PixelRect b = bounds_of(panes, s.setup.active, p.kind, sc).rect;
-        if (b.w <= 0 || b.h <= 0) {
-            continue;
-        }
-        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
+        PixelRect b = bounds_of(panes, s.setup.active, p.kind, sc).rect;
+        if (p.kind == lifted) {
+            b = pane_interior(b, chrome_grain(sc));
+        }
+        if (b.w <= 0 || b.h <= 0) {
+            continue;
+        }
+        const std::int64_t chrome = p.kind == lifted ? kPaneChromeSelected : kPaneChrome;
```

Red in `zengine-workshop-screen-tests`:

- selected and ordinary differ in INK, and in nothing else

## outer-sized-for-pixels

`chrome_outer_of` reserves a pixel's boundary, the finest face's, rather than a cell's.

In `workshop/screen.hpp`:

```diff
-    return PixelRect{x, y, w + 2 * kChromePx, h + 2 * kChromePx};
+    return PixelRect{x, y, w + 2, h + 2};
```

Red in `zengine-workshop-screen-tests`:

- a content-sized surface reserves the coarsest boundary, once

## surface-anchored-to-the-body

The contextual surface is anchored a chrome unit in from where it was pointed.

In `workshop/screen_attention.cpp`:

```diff
-    std::int64_t x = menu.anchor_x;
-    std::int64_t y = menu.anchor_y;
+    std::int64_t x = menu.anchor_x + chrome_grain(sc);
+    std::int64_t y = menu.anchor_y + chrome_grain(sc);
```

Red in `zengine-workshop-screen-tests`:

- a content-sized surface reserves the coarsest boundary, once

## double-click-on-any-tab

Two presses on two different tabs count as one double-click.

In `workshop/screen_arrange.cpp`:

```diff
-    if (!prior.armed || prior.at != at) {
+    (void)at;
+    if (!prior.armed) {
```

Red in `zengine-workshop-screen-tests`:

- what makes two presses on a tab one double-click, and what does not

## context-arranges-the-topmost

Contextual Arrange arranges the pane in front, not the one the surface was opened on.

In `workshop/weave_pointer.cpp`:

```diff
-    case Act::kArrange: enter_arrange_pane(spent.pane); break;
+    case Act::kArrange: {
+        const std::vector<std::int64_t> order = effective_pane_order(session_.setup.active, session_.panes);
+        PaneRef top = spent.pane;
+        for (const SetupPane& row : session_.setup.active.panes) {
+            if (!order.empty() && resolve_pane(row.ref, session_.panes) == std::optional<std::int64_t>(order.back())) {
+                top = row.ref;
+            }
+        }
+        enter_arrange_pane(top);
+        break;
+    }
```

Red in `zengine-workshop-screen-tests`:

- contextual Arrange lifts the pane it addressed, not the one in front

## arrange-without-selecting

`enter_arrange_pane` binds the arrangement without selecting the pane.

In `workshop/weave_arrange.cpp`:

```diff
-    if (kind.has_value()) {
-        session_.panes.selected = *kind;
-    }
-    PaneArrange& a = session_.arrange;
+    if (false && kind.has_value()) {
+        session_.panes.selected = *kind;
+    }
+    PaneArrange& a = session_.arrange;
```

Red in `zengine-workshop-screen-tests`:

- contextual Arrange lifts the pane it addressed, not the one in front

## arrange-writes-the-keyboard

`enter_arrange_pane` gives the pane the keyboard as well as the selection.

In `workshop/weave_arrange.cpp`:

```diff
-        session_.panes.selected = *kind;
-    }
-    PaneArrange& a = session_.arrange;
+        session_.panes.selected = *kind;
+        session_.panes.keyboard = *kind;
+    }
+    PaneArrange& a = session_.arrange;
```

Red in `zengine-workshop-screen-tests`:

- contextual Arrange lifts the pane it addressed, not the one in front

## select-before-admission

`enter_arrange_pane` selects the pane before its geometry is admitted.

In `workshop/weave_arrange.cpp`:

```diff
-void WorkshopWeave::enter_arrange_pane(const PaneRef& ref, bool opened) {
+void WorkshopWeave::enter_arrange_pane(const PaneRef& ref, bool opened) {
+    if (const std::optional<std::int64_t> early = resolve_pane(ref, session_.panes)) {
+        session_.panes.selected = *early;
+    }
```

Red in `zengine-workshop-screen-tests`:

- every pane a weaver can point at can be arranged, and the refusals are blind

## new-layout-pushed-back

`add_layout` shelves the live layout at the run's end, wherever it stood.

In `workshop/layouts/run.hpp`:

```diff
     if (to >= layout_count(s) || to == s.active_at) {
         return false;
     }
-    s.shelved.insert(s.shelved.begin() + static_cast<std::ptrdiff_t>(s.active_at),
-                     Layout{std::move(s.active), std::move(s.active_link), s.active_id});
+    s.shelved.push_back(Layout{std::move(s.active), std::move(s.active_link), s.active_id});
```

Red in `zengine-workshop-screen-tests`:

- a new layout appends however far into the run you stand

## no-boundary-candidate-dropped

`pane_inside` has no last candidate with no boundary at all.

In `workshop/screen_chrome.cpp`:

```diff
-    return detail::pane_inside_at(outer, sc, 0);
+    return cell;
```

Red in `zengine-workshop-screen-tests`:

- a two-cell pane keeps its content and drops its boundary

## tabs-before-occupancy

A press asks the Layouts pane's tabs before the occupancy walk names the pane in front.

In `workshop/weave_pointer.cpp`:

```diff
-        if (here.occupied && is_runtime_kind(here.kind)) {
-            repaint(mail);
-            return;
-        }
+        if (layouts_press(b, mail)) {
+            repaint(mail);
+            return;
+        }
+        if (here.occupied && is_runtime_kind(here.kind)) {
+            repaint(mail);
+            return;
+        }
```

Red in `zengine-workshop-screen-tests`:

- a pane in front of the Layouts pane takes the press

## reservation-follows-the-layouts-pane

`screen_of` gives the room the top band's rows when the Layouts pane is not on the desk.

In `workshop/screen.hpp`:

```diff
-inline constexpr Screen screen_of(const Session& s) noexcept {
-    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px, s.cell_px,
-                     s.stack_rows);
-}
+inline Screen screen_of(const Session& s) noexcept {
+    const bool layouts = pane_of(s.setup.active, pane_ref_of(pane_kind::kLayouts)) != nullptr;
+    return screen_of(s.screen_w, s.screen_h + (layouts ? 0 : graphics::px_of_cells(kTopRows)),
+                     s.text_advance_px, s.text_line_px, s.cell_px, s.stack_rows);
+}
```

Red in `zengine-workshop-screen-tests`:

- the reservation does not follow the Layouts pane

## walk-in-authored-order

The keyboard's selection walks the panes in the order they were authored.

In `workshop/screen_arrange.cpp`:

```diff
-    std::stable_sort(out.begin(), out.end(), [](const Selectable& a, const Selectable& b) {
+    std::stable_sort(out.begin(), out.begin(), [](const Selectable& a, const Selectable& b) {
```

Red in `zengine-workshop-screen-tests`:

- the keyboard's selection walks the open panes in reading order, whatever order they were authored
  in, and wraps both ways

## left-and-right-swapped

`spatial_neighbour`'s filter reads left as right.

In `workshop/screen_arrange.cpp`:

```diff
-        case Act::kFocusLeft: return edge ? cx <= f.x : cx < fcx;
+        case Act::kFocusLeft: return edge ? cx >= f.x + f.w : cx > fcx;
```

Red in `zengine-workshop-screen-tests`:

- a direction selects the neighbour past the selected pane's edge, and nothing past the desk's

## misalignment-weighted-once

A neighbour's misalignment across the direction counts once.

In `workshop/screen_arrange.cpp`:

```diff
-        return (gap > 0 ? gap : 0) + 2 * across;
+        return (gap > 0 ? gap : 0) + across;
```

Red in `zengine-workshop-screen-tests`:

- misalignment across the direction counts twice the gap along it

## tie-to-the-pane-behind

Two neighbours scored alike go to the one behind.

In `workshop/screen_arrange.cpp`:

```diff
-                (points == best_score && p.depth > best->depth)) {
+                (points == best_score && p.depth < best->depth)) {
```

Red in `zengine-workshop-screen-tests`:

- two neighbours scored alike go to the one in front

## edge-filter-alone

A direction reaches only a pane past the selected pane's edge.

In `workshop/screen_arrange.cpp`:

```diff
-    for (const bool edge : {true, false}) {
+    for (const bool edge : {true}) {
```

Red in `zengine-workshop-screen-tests`:

- a pane overlapping the selected one is reached past its centre

## no-ring

Command mode draws no ring round the selected pane.

In `workshop/screen_compose.cpp`:

```diff
-    if (keyboard_context(s) != KeyContext::kCommand) {
-        return PixelRect{};
+    if (true) {
+        return PixelRect{};
```

Red in `zengine-workshop-screen-tests`:

- command mode marks the selected pane, with brackets where text is a cell and rings where an edge
  is finer, and the pane the keys are in wears none

## gesture-writes-workshops-copy

A window edit is kept on Workshop's copy of the desk and never sent to the owner.

In `workshop/weave_session.cpp`:

```diff
-    LayoutEditRequested asked;
-    asked.revision = judged ? session_.setup.revision : 0;
+    if (!edits.empty() && edits.front().op == std::string(layout_op::kWindow)) {
+        return;
+    }
+    LayoutEditRequested asked;
+    asked.revision = judged ? session_.setup.revision : 0;
```

Red in `zengine-workshop-screen-tests`:

- every writer of the live desk asks the layouts' owner, and the picture is the owner's next copy

## stale-edit-written

An edit judged from an older picture of the desk is written.

In `workshop/layouts/store.cpp`:

```diff
-    if (asked.revision != 0 && asked.revision != revision_) {
+    if (false) {
```

Red in `zengine-workshop-screen-tests`:

- two edits judged from one picture: the first is written, the second refused whole, and the picture
  is the owner's

## save-before-the-whole-batch

A batch's setup file is written as its save edit is judged, before the rest of the batch.

In `workshop/layouts/store.cpp`:

```diff
-        saving = SetupWrite{path, next.active};
+        saving = SetupWrite{path, next.active};
+        (void)setup_persist::save_file(path, next.active);
```

Red in `zengine-workshop-screen-tests`:

- a batch's setup file is written once every edit in it is judged, and not at all when one is
  refused

## seat-forgets-where-left

A seat forgets where the layout left a pane.

In `workshop/layouts/run.hpp`:

```diff
-    const auto found = s.left.find(s.active_id);
-    if (found == s.left.end()) {
+    const auto found = s.left.end();
+    if (found == s.left.end()) {
```

Red in `zengine-workshop-screen-tests`:

- WL-DESK-19: a pane the layout never had opens at the room's centre, placed by its title and edges
  while its keys and body stay its own; shown again after a hide it comes back where it was left

## no-veil

The window's drawing has no veil over the panes not arranged.

In `workshop/screen_compose.cpp`:

```diff
-        draw_arranging_veil(out.sketch, desk + "/veil", s, sc);
```

Red in `zengine-workshop-screen-tests`:

- WL-ARR-18: arranging says so at the room's top, naming what it arranges, and in a window every
  other pane fades

## veil-each-pane-whole

`draw_arranging_veil` veils each faded pane whole but for the arranged one, over any veil already
laid.

In `workshop/screen_compose.cpp`:

```diff
-        for (const PixelRect& done : veiled) {
+        for (const PixelRect& done : std::vector<PixelRect>{}) {
```

Red in `zengine-workshop-screen-tests`:

- WL-ARR-18: where faded panes overlap the veil falls once, and on all of each that shows
