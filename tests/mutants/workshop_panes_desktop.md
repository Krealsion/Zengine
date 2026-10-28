# The mutants `tests/test_workshop_panes_desktop.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_desktop.cpp`](../test_workshop_panes_desktop.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## wheel-walks-the-marker

The Pane Manager's wheel walks the marker a row a notch, whatever rows are hidden its way.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-        if (budget_ == 0 || !hidden) {
+        if (true || budget_ == 0 || !hidden) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-10: the Pane Manager lists the panes by name, and its wheel scrolls the rows hidden its
  way before it walks the marker

## toggle-closes-a-covered-pane

Ctrl+P puts away a covered pane rather than bringing it forward.

In `workshop/weave_desktop.cpp`:

```diff
-    if (has_pane(session_.setup.active, ref) && !straight_from && covered_now(ref)) {
+    if (false && has_pane(session_.setup.active, ref) && !straight_from && covered_now(ref)) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-13: Ctrl+P is a visibility toggle judged by the host -- open and in sight becomes closed,
  covered is brought forward, closed becomes open and focused, whatever the desktop last heard

## toggle-judges-cover-alone

Ctrl+P brings a covered pane forward even when the weaver comes straight from it.

In `workshop/weave_desktop.cpp`:

```diff
-    if (has_pane(session_.setup.active, ref) && !straight_from && covered_now(ref)) {
+    (void)straight_from;
+    if (has_pane(session_.setup.active, ref) && covered_now(ref)) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-13: Ctrl+P puts away a covered Pane Manager the weaver comes straight from, and brings it
  forward once they have moved on from it

## manual-where-it-would-stand

The Manual is seated where a pane would stand, not over the pane whose manual was asked.

In `workshop/weave_desktop.cpp`:

```diff
-    if (row == nullptr || !session_.panes.has(manual_over_)) {
+    if (true || row == nullptr || !session_.panes.has(manual_over_)) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-20: the Manual opens over the pane whose manual was asked, a margin of it showing on every
  edge, and moves over the next one asked

## hotkeys-paste-asked-elsewhere

The Hotkeys pane asks the graphics directory for a paste, not the window showing it.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
-        paste_.office = keys_canvas_.clipboard_office();
+        paste_.office = "zengine.graphics";
```

Red in `zengine-workshop-panes-tests`:

- the Hotkeys pane asks the window that shows it for a paste into a spelling
