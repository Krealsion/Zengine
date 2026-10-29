# The mutants `tests/test_workshop_demo.cpp` claims

Each entry is a mutant that a comment in [`tests/test_workshop_demo.cpp`](../test_workshop_demo.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## rows-a-cell-apart

A pane's picture sets its rows a cell apart in every window, whatever the face's line.

In `workshop/pane_drawing.hpp`:

```diff
-    const std::int64_t line = room.text.line;
+    const std::int64_t line = graphics::kCellPx;
```

Red in `zengine-workshop-panes-tests`:

- WL-HAND-06: the demo controls name their reset row and the state beneath it

## fit-reads-first-offer

`pane_measure` reads a `fit` pane's first offer, whatever it offers now.

In `workshop/screen_chrome.cpp`:

```diff
-        if (fits && latest) {
+        if (fits && latest && false) {
```

Red in `zengine-workshop-panes-tests`:

- pane comfort: `fit` follows a pane switching between two sizes each time, in both windows, and
  `default` keeps the first

## every-reoffer-held

The OS window holds every re-offer, however far it is from what `fit` stands on.

In `workshop/setup_reconcile.hpp`:

```diff
-            if (!within_a_pixel(asked, row.standing)) {
+            if (false) {
```

Red in `zengine-workshop-panes-tests`:

- pane comfort: `fit` follows a pane switching between two sizes each time, in both windows, and
  `default` keeps the first

## no-hold

No re-offer is ever within a pixel of what `fit` stands on.

In `workshop/setup_reconcile.hpp`:

```diff
-    return a.width > 0 && b.width > 0 && a.text_rows == b.text_rows &&
+    return false && a.width > 0 && b.width > 0 && a.text_rows == b.text_rows &&
```

Red in `zengine-workshop-panes-tests`:

- pane comfort: a re-offer within one pixel of what `fit` stands on is held in the OS window alone,
  and the terminal window follows it

## terminal-holds-too

The terminal window holds a re-offer within a pixel as the OS window does.

In `workshop/setup_reconcile.hpp`:

```diff
-    return room.grain == 1 ? pane.standing : pane.latest;
+    return room.grain >= 1 ? pane.standing : pane.latest;
```

Red in `zengine-workshop-panes-tests`:

- pane comfort: a re-offer within one pixel of what `fit` stands on is held in the OS window alone,
  and the terminal window follows it

## two-pixel-hold

A re-offer within two pixels is held.

In `workshop/setup_reconcile.hpp`:

```diff
-        return x - y <= 1 && y - x <= 1;
+        return x - y <= 2 && y - x <= 2;
```

Red in `zengine-workshop-panes-tests`:

- pane comfort: a re-offer within one pixel of what `fit` stands on is held in the OS window alone,
  and the terminal window follows it

## held-note-silent

`pane_fit_note` says nothing of a measure held off the pane's latest offer.

In `workshop/screen_pane_state.cpp`:

```diff
-    if (fit_offer(*pane, room) == pane->latest) {
+    if (true) {
```

Red in `zengine-workshop-panes-tests`:

- pane comfort: a `fit` axis says what it stands on where that is not the pane's latest offer

## replacement-over-a-lock

`replacement_refused` answers nothing, whatever lock the incoming desk would change.

In `workshop/layouts/store.cpp`:

```diff
-    if (changed.empty()) {
-        return std::string();
-    }
-    std::string named;
+    if (true) {
+        return std::string();
+    }
+    std::string named;
```

Red in `zengine-workshop-panes-tests`:

- a desk put live keeps every lock: `r` and an agent's desk are refused whole naming each locked
  value they would change, and a lock the desk brings joins

## fold-mark-outlined-in-cells

A row's fold mark is drawn as an outline in a room of cells and as its character where the room
sets type: each medium gets the other's mark.

In `workshop/pane_drawing.hpp`:

```diff
-        const bool outlined = !room.cells && mark >= 0 &&
+        const bool outlined = room.cells && mark >= 0 &&
```

Red in `zengine-workshop-panes-tests`:

- a row's fold mark is its own character in a room of cells, and where the room sets type the icon
  table's outline in its place -- `right` for a shut `+`, `down` for an open `-` -- with the
  character left out
