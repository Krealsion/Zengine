# The mutants `tests/test_workshop_panes_window.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_window.cpp`](../test_workshop_panes_window.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## right-column-read-as-default

A file's `right-column` is read as `default`.

In `workshop/setup_persist.hpp`:

```diff
-        out = PanePlace{ui::kPlaceTopRight, 0, 0};
+        out = PanePlace{};
```

Red in `zengine-workshop-panes-tests`:

- a file's `right-column` reads as `top-right` at no offset, and is written as that

## body-keeps-the-cell-inset

An external pane's body place keeps a cell's inset while its paint goes thin.

In `workshop/screen_external.cpp`:

```diff
-    const PaneInside inside = pane_inside(pane_rect, sc);
+    const PaneInside inside = detail::pane_inside_at(pane_rect, sc, kChromePx);
```

Red in `zengine-workshop-panes-tests`:

- on the shipped face the border is chrome and the first body pixel is row 0

## chrome-a-cell-thick

A pane's chrome stays a cell thick on a face whose unit is finer.

In `workshop/screen_chrome.cpp`:

```diff
-    if (fine < kChromePx) {
-        const PaneInside thin = detail::pane_inside_at(outer, sc, fine);
+    if (false) {
+        const PaneInside thin = detail::pane_inside_at(outer, sc, fine);
```

Red in `zengine-workshop-panes-tests`:

- on the shipped face the border is chrome and the first body pixel is row 0

## paste-asked-elsewhere

The Terminal asks the graphics directory for a paste, not the window showing it.

In `terminal/pane.cpp`:

```diff
-            .send_to_role(room().clipboard_office(), gfx::ClipboardTextRequested{},
+            .send_to_role(std::string("zengine.graphics"), gfx::ClipboardTextRequested{},
```

Red in `zengine-workshop-panes-tests`:

- the graphical room is the post-chrome pixels, and selection cannot move it

## selected-chrome-thicker

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

Red in `zengine-workshop-panes-tests`:

- the graphical room is the post-chrome pixels, and selection cannot move it

## room-at-the-cell-inset

The room a pane is granted keeps the cell-inset capacity while its body goes thin.

In `workshop/screen_external.cpp`:

```diff
-    p.fit = inside.fit;
+    p.fit = detail::pane_inside_at(pane_rect, sc, kChromePx).fit;
```

Red in `zengine-workshop-panes-tests`:

- the graphical room is the post-chrome pixels, and selection cannot move it

## painter-insets-the-selected

The painter insets the selected pane further than an ordinary one.

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

Red in `zengine-workshop-panes-tests`:

- the graphical room is the post-chrome pixels, and selection cannot move it

## selection-reaches-the-order

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

Red in `zengine-workshop-panes-tests`:

- the thinner boundary rewrites no authored value and no foreground law
