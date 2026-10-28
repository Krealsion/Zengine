# The mutants `tests/test_workshop_panes_palette.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_palette.cpp`](../test_workshop_panes_palette.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## keys-back-on-own-closes-only

Workshop gives the keys back only when the palette's own Escape puts it down.

In `workshop/weave_session.cpp`:

```diff
-    if (palette_seated && !has_pane(d.active, palette)) {
+    if (false && palette_seated && !has_pane(d.active, palette)) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-18: the palette closed by an office, through the layouts' owner, is put down with the keys
  given back, as its own Escape puts it down

## other-owners-left-out

The palette lists Workshop's actions and the panes, and none of the other owners' entries.

In `workshop/palette/palette.cpp`:

```diff
-        listing_ = pal::entries_of(keys_, inventory_, others_);
+        listing_ = pal::entries_of(keys_, inventory_);
```

Red in `zengine-workshop-panes-tests`:

- the palette lists each layout on the shelf, each open pane's manual while the application shows
  manuals, and each setup and app the launcher can start

## command-before-the-put-down

A pane chosen in the palette is focused before the palette's put-down is answered.

In `workshop/weave_command.cpp`:

```diff
-    if (has_pane(session_.setup.active, palette)) {
-        unseat_then(palette,
-                    [this, asked, actor, gesture, held](const LayoutEditAnswered&, loom::Mail& m) {
+    if (false) {
+        unseat_then(palette,
+                    [this, asked, actor, gesture, held](const LayoutEditAnswered&, loom::Mail& m) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-18: a pane chosen in the palette that is already on the desk is the one the keys go to,
  and the palette's put-down gives them back over nothing

## layout-by-place

A layout chosen in the palette is found by its place in the run, not by its id.

In `workshop/weave_command.cpp`:

```diff
-            if (std::to_string(layout_id_at(session_.setup, at)) == asked.id) {
+            if (std::to_string(at) == asked.id) {
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-18: a layout chosen in the palette is switched to as a press on its tab, and a pane's
  manual is asked of the desktop as its ? asks it, each as the hand that chose it

## start-without-approval

The palette's start reaches the launcher whatever Workshop answers for the hand.

In `workshop/palette/palette.cpp`:

```diff
-        if (!answer.allowed) {
-            notice_ = "nothing was started -- " + answer.reason;
+        if (false) {
+            notice_ = "nothing was started -- " + answer.reason;
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-18: a setup or an app chosen in the palette is started as Files starts one: Workshop asked
  for the hand of its row's key, then the launcher, the palette saying what it answered
