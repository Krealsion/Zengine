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

## sections-in-the-keymaps-order

The Hotkeys pane's sections stand in the keymap's order: Workshop's own first, the desk's panes
among the rest.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
-        std::stable_sort(sections_.begin(), sections_.end(),
-                         [&run](const KeysSection& a, const KeysSection& b) {
-                             return run(a) < run(b);
-                         });
+        (void)run;
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-21: the Hotkeys pane is in sections, a section a pane -- the desk's panes first,
  Workshop's own next, the panes off the desk grey at the foot -- each shut at first and saying how
  many keys it holds; Right opens one and steps into it, Left steps out of it and shuts it,
  shift+left shuts every one, a press on a header opens or shuts it, and the open ones are the
  pane's state, kept across a reload

## absent-sections-not-grey

A section of a pane off the desk is drawn as any other, not grey.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
-                     section.absent && fold != component::Fold::kPeek ? graphics::role::kMuted
-                                                                      : graphics::role::kAccent,
+                     graphics::role::kAccent,
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-21: the Hotkeys pane is in sections, a section a pane -- the desk's panes first,
  Workshop's own next, the panes off the desk grey at the foot -- each shut at first and saying how
  many keys it holds; Right opens one and steps into it, Left steps out of it and shuts it,
  shift+left shuts every one, a press on a header opens or shuts it, and the open ones are the
  pane's state, kept across a reload

## sections-not-kept-across-a-reload

The Hotkeys pane keeps its open sections in a list beside the desk weave's state, not in it, so a
reload shuts them.

In `workshop/desktop-pane/pane.cpp`:

```diff
-    pane::HotkeysView hotkeys_{state_.hotkeys_open};
+    std::vector<std::string> beside_ = {};
+    pane::HotkeysView hotkeys_{beside_};
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-21: the Hotkeys pane is in sections, a section a pane -- the desk's panes first,
  Workshop's own next, the panes off the desk grey at the foot -- each shut at first and saying how
  many keys it holds; Right opens one and steps into it, Left steps out of it and shuts it,
  shift+left shuts every one, a press on a header opens or shuts it, and the open ones are the
  pane's state, kept across a reload

## hotkeys-peek-not-drawn

The pointer resting on a shut section's header shows nothing over what follows.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
                 if (fold == component::Fold::kPeek) {
-                    peek_from = static_cast<std::int64_t>(out.size()) + 1;
+                    (void)out;
                     peeking = line.section;
                 }
```

Red in `zengine-workshop-panes-tests`:

- in a window that reports a resting pointer, a shut section of the Hotkeys pane peeks its keys
  over what follows its header and takes no room, a press on what the peek shows opens the section
  on that key, and the leave ends the peek
