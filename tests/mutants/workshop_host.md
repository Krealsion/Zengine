# The mutants `tests/test_workshop_host.cpp` claims

Each entry is a mutant that a comment in [`tests/test_workshop_host.cpp`](../test_workshop_host.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## second-pass-dropped

Completion makes one pass, so `required-first` offers the required fields alone.

In `terminal/complete.hpp`:

```diff
-            if (!split) {
-                break;
-            }
+            break;
```

Red in `zengine-workshop-host-tests`:

- arguments offer a shape's fields in its order, or its required fields first and its optional ones
  after, each in its order

## split-ignored

Completion ignores the split and keeps the shape's order.

In `terminal/complete.hpp`:

```diff
-        const bool split = field_order == CompletionFieldOrder::RequiredFirst;
+        const bool split = false && field_order == CompletionFieldOrder::RequiredFirst;
```

Red in `zengine-workshop-host-tests`:

- arguments offer a shape's fields in its order, or its required fields first and its optional ones
  after, each in its order

## unavailable-tool-fixed-words

`unavailable_tool` says a fixed sentence and names no artifact.

In `workshop/attention.hpp`:

```diff
-    return Condition{"load.unavailable/" + stem, stem + " is not in this Workshop", said,
+    return Condition{"load.unavailable/" + stem, "a tool is not in this Workshop", said,
```

Red in `zengine-workshop-host-tests`:

- an unavailable tool is named by its artifact on the host's own condition row, which no tool paints

## unavailable-tool-keyed-by-sentence

`unavailable_tool` keys the condition by the refusing layer's sentence.

In `workshop/attention.hpp`:

```diff
-    return Condition{"load.unavailable/" + stem, stem + " is not in this Workshop", said,
+    return Condition{"load.unavailable/" + said, stem + " is not in this Workshop", said,
```

Red in `zengine-workshop-host-tests`:

- an unavailable tool is named by its artifact on the host's own condition row, which no tool paints

## row-keyed-by-name

A guest row's losses are keyed by the row's name.

In `workshop/attention.hpp`:

```diff
-    return Condition{"guests.row/" + std::to_string(row),
+    (void)row;
+    return Condition{"guests.row/" + name,
```

Red in `zengine-workshop-host-tests`:

- a development host and each row's losses stand as conditions on Attention

## row-stands-whenever-a-guest

Every guest row stands as a condition, its losses or none.

In `workshop/guests.cpp`:

```diff
-        if (!losses.empty()) out.push_back(guest_row_losses(i, file.rows[i].name, file.version, losses));
+        if (true) out.push_back(guest_row_losses(i, file.rows[i].name, file.version, losses));
```

Red in `zengine-workshop-host-tests`:

- a development host and each row's losses stand as conditions on Attention

## repaint-says-no-conditions

A repaint never says the conditions.

In `workshop/weave_run.cpp`:

```diff
-    say_conditions(frontier, mail);
+    (void)frontier;
```

Red in `zengine-workshop-host-tests`:

- what is true is said across the seam, in the host's own order and words

## silence-arm-dropped

The conditions are said at every repaint, news or none.

In `workshop/weave_run.cpp`:

```diff
-    if (conditions_said_ && same_conditions(now, said_conditions_)) {
+    if (false) {
```

Red in `zengine-workshop-host-tests`:

- nothing new is nothing said, which is what stops the seam looping

## live-layout-without-reseat

A live layout is installed without `apply_setup`.

In `workshop/weave_session.cpp`:

```diff
-    apply_setup(mail);
-    // THE PALETTE PUT DOWN, by whichever close took its row -- this desk's own or an office's,
+    // THE PALETTE PUT DOWN, by whichever close took its row -- this desk's own or an office's,
```

Red in `zengine-workshop-host-tests`:

- a typed place reseats the stack through `apply_setup`

## typed-size-clamped

A typed size in the face's unit is clamped to what a pane may be rather than refused.

In `workshop/screen_pane_subject.cpp`:

```diff
-            typed = PaneSize{pane_unit::kPixels, face.px};
+            typed = PaneSize{pane_unit::kPixels, std::clamp<std::int64_t>(face.px, 1, kMaxPanePixels)};
```

Red in `zengine-workshop-host-tests`:

- a typed value that is not admissible is refused, and the authored row is untouched

## inspection-writes-back

An inspection writes the pane's place back into the setup as pixels.

In `workshop/weave_inspection.cpp`:

```diff
-    in.rows = pane_subject_rows(session_, in.ref);
+    in.rows = pane_subject_rows(session_, in.ref);
+    if (SetupPane* row = pane_of(session_.setup.active, in.ref)) {
+        row->place.mode = pane_unit::kPixels;
+    }
```

Red in `zengine-workshop-host-tests`:

- looking never authors

## subject-dropped-with-its-pane

The inspected subject is dropped when its pane leaves the layout.

In `workshop/weave_inspection.cpp`:

```diff
-    const bool member = has_pane(session_.setup.active, in.ref);
+    const bool member = has_pane(session_.setup.active, in.ref);
+    if (!member) {
+        in = InspectedPane{};
+        return;
+    }
```

Red in `zengine-workshop-host-tests`:

- a closed pane and an unresolved row are subjects with honest facts

## unresolved-refs-filtered

The inventory leaves out every authored row this build cannot present.

In `workshop/setup_reconcile.hpp`:

```diff
-        if (known) {
-            continue;
-        }
-        CatalogRow made;
+        if (known || true) {
+            continue;
+        }
+        CatalogRow made;
```

Red in `zengine-workshop-host-tests`:

- a closed pane and an unresolved row are subjects with honest facts

## status-stays-current

`link_status` answers `current` for every associated desk.

In `workshop/layouts/run.hpp`:

```diff
-    return desk == link.known ? setup_link::kCurrent : setup_link::kModified;
+    return desk == link.known || true ? setup_link::kCurrent : setup_link::kModified;
```

Red in `zengine-workshop-host-tests`:

- editing a pane in a layout related to a current Setup makes it modified

## reservation-follows-layouts

`screen_of` takes the Layouts pane's place from the room's height.

In `workshop/screen.hpp`:

```diff
-inline constexpr Screen screen_of(const Session& s) noexcept {
-    return screen_of(s.screen_w, s.screen_h, s.text_advance_px, s.text_line_px, s.cell_px,
-                     s.stack_rows);
-}
+inline Screen screen_of(const Session& s) noexcept {
+    const SetupPane* layouts = pane_of(s.setup.active, pane_ref_of(pane_kind::kLayouts));
+    return screen_of(s.screen_w, s.screen_h - (layouts != nullptr ? layouts->place.y : 0),
+                     s.text_advance_px, s.text_line_px, s.cell_px, s.stack_rows);
+}
```

Red in `zengine-workshop-host-tests`:

- moving, resizing and closing Layouts through the doors leaves the reservation alone

## final-escape-removed

Escape's final branch is never taken: a bare Escape asks the desktop for nothing.

In `workshop/weave_handlers.cpp`:

```diff
-    if ((default_row_context(ctx) || (ctx == KeyContext::kPane && !crossed)) &&
+    if (false && (default_row_context(ctx) || (ctx == KeyContext::kPane && !crossed)) &&
```

Red in `zengine-workshop-host-tests`:

- Escape clears the ordinary selection last, and the inspected subject stands

## subject-cleared-with-selection

Putting the selection down clears the inspected subject beside it.

In `workshop/weave_desktop.cpp`:

```diff
-    unselect_pane();
-    repaint(mail);
-}
-
-// ---- Launching ----------------------------------------------------------------------------
+    unselect_pane();
+    session_.inspected = InspectedPane{};
+    repaint(mail);
+}
+
+// ---- Launching ----------------------------------------------------------------------------
```

Red in `zengine-workshop-host-tests`:

- Escape clears the ordinary selection last, and the inspected subject stands
