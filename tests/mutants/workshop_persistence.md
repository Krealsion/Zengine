# The mutants `tests/test_workshop_persistence.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_persistence.cpp`](../test_workshop_persistence.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## passed-over-unsaid

The desk comes up saying nothing of what the plan gave way from.

In `workshop/weave_handlers.cpp`:

```diff
-    if (host_->passed_over) {
+    if (false && host_->passed_over) {
```

Red in `zengine-workshop-persistence-tests`:

- what the plan could not load and gave way from is said on the desk as it comes up, and nothing is
  said of it where every row loaded its own

## restore-said-in-the-old-unit

The restore's notice is said at once, in the unit the desk held before any window named one.

In `workshop/weave_session.cpp`:

```diff
-    restore_notice_ = std::move(notice);
-    if (first_unit_) {
-        (void)say_restore_notice(*first_unit_);
-    }
+    restore_notice_ = std::move(notice);
+    (void)say_restore_notice(session_.cell_px);
```

Red in `zengine-workshop-persistence-tests`:

- the restore notice says the room in the unit the medium names: pixels in a window, cells in a
  terminal

## every-coordinate-written

`place_out` writes a place's coordinates whatever its word reads.

In `workshop/setup_persist.hpp`:

```diff
-    if (ui::place_reads_x(p.mode)) {
-        out.x = p.x;
-    }
+    if (true) {
+        out.x = p.x;
+    }
```

Red in `zengine-workshop-persistence-tests`:

- every place and size word survives the setup file and the session byte for byte

## share-takes-any-amount

`check_pane_size` takes a share of any amount.

In `workshop/setup.hpp`:

```diff
-        if (s.amount < 1 || s.amount > ui::kShareWhole) {
+        if (false) {
```

Red in `zengine-workshop-persistence-tests`:

- a word's numbers out of its range are refused in the weaver's words, by the file and by the owner

## share-place-judged-as-pixels

`check_pane_place` judges a share place's numbers as pixels.

In `workshop/setup.hpp`:

```diff
-        if (reads && ui::place_reads_share(p.mode)) {
+        if (false) {
```

Red in `zengine-workshop-persistence-tests`:

- a word's numbers out of its range are refused in the weaver's words, by the file and by the owner

## room-saved-whatever-the-window

The window file's room is this run's, whichever window the run was in.

In `workshop/weave_session.cpp`:

```diff
-    const bool os_window = window_name_ == graphics::kSdlWindow;
-    place.room_w = os_window ? session_.normal_w : session_.window_room_w;
+    const bool os_window = true;
+    place.room_w = os_window ? session_.normal_w : session_.window_room_w;
```

Red in `zengine-workshop-persistence-tests`:

- the window file keeps the OS window's own room beside its placement, and a run in the terminal
  window writes it back as it read it, as it does the placement

## placed-before-shaped

A restore places the OS window as soon as it reads the window file, before the shaping.

In `workshop/weave_session.cpp`:

```diff
-        if (restored_window_) {
-            restored_window_->place = place; // after the shaping, which waits for the desk's area
+        if (false) {
+            restored_window_->place = place; // after the shaping, which waits for the desk's area
```

Red in `zengine-workshop-persistence-tests`:

- a restore in the OS window shapes it to its own room, a smaller one taken, then places it; a
  maximized one is shaped before it is maximized; the terminal window keeps its own

## v10-edge-drops-the-room

Version 10's edge leaves out the OS window's room.

In `workshop/layouts/session_history.hpp`:

```diff
-    out.window_room = old.window_room;
-    return out;
-}
-
-/// A DESK OF SETUP VERSION 5 IN THE CURRENT SHAPE, by its words, or the session's refusal naming
+    return out;
+}
+
+/// A DESK OF SETUP VERSION 5 IN THE CURRENT SHAPE, by its words, or the session's refusal naming
```

Red in `zengine-workshop-persistence-tests`:

- a version-10 session comes forward whole and is written back in lines

## v5-without-the-layouts-row

A version-5 desk converts without the Layouts row its vintage always had.

In `workshop/layouts/session_history.hpp`:

```diff
-    if (names_layouts(old)) {
-        return old;
-    }
+    if (true || names_layouts(old)) {
+        return old;
+    }
```

Red in `zengine-workshop-persistence-tests`:

- a real version-5 session comes back with nothing lost

## names-layouts-guard-dropped

A version-5 desk that already names the Layouts pane is given a second Layouts row.

In `workshop/layouts/session_history.hpp`:

```diff
-    if (names_layouts(old)) {
-        return old;
-    }
+    if (false) {
+        return old;
+    }
```

Red in `zengine-workshop-persistence-tests`:

- an explicit historical row is preserved, never duplicated

## v5-edge-removed

The session's conversions hold no edge for version 5.

In `workshop/layouts/session_history.hpp`:

```diff
-    edges.push_back(op::make_migration(
-        loom::schema_of<v5::WorkshopSession>(), current, [](const loom::Value& old) {
-            return loom::Cell::message(
-                loom::to_value(session_v5_to_v12(loom::from_value<v5::WorkshopSession>(old))));
-        }));
```

Red in `zengine-workshop-persistence-tests`:

- the weaver sees no loss, and the next run spends no conversion

## setup-in-without-the-rewrite

`setup_in` reads a reference that changed hands without rewriting it.

In `workshop/setup_persist.hpp`:

```diff
     // converted candidate then meets the whole law, so a file naming both spellings is refused.
-    add_converted(converted, pane_migration::convert_retired_panes(candidate));
```

Red in `zengine-workshop-persistence-tests`:

- a saved setup naming the built-in browser opens as the loaded pane

## fifth-retired-row-dropped

`kRetired` loses its fifth row, the host's Pane Manager become the desktop's.

In `workshop/pane_migration.hpp`:

```diff
-    {kRetiredManagerProvider, kRetiredManagerPane, kManagerProvider, kManagerPane,
-     pane_unit::kDefault},
```

Red in `zengine-workshop-persistence-tests`:

- a saved setup naming the host's Pane Manager opens as the desktop's

## v9-viewport-as-the-room

A version-9 session converts taking its viewport as the OS window's room.

In `workshop/layouts/session_history.hpp`:

```diff
-    out.choices = old.choices;
-    return out;
-}
-
-/// A SESSION OF VERSION 10 AS ONE OF VERSION 11: every field as it was; only its file's form
+    out.choices = old.choices;
+    out.window_room = old.viewport;
+    return out;
+}
+
+/// A SESSION OF VERSION 10 AS ONE OF VERSION 11: every field as it was; only its file's form
```

Red in `zengine-workshop-persistence-tests`:

- a version-9 session converts with every field kept and no OS window's room

## import-over-the-window-file

An older session's room is imported whatever window file stands beside it.

In `workshop/weave_session.cpp`:

```diff
-    if (!window.present) {
-        window = window_persist::import_from_session(host_->session_path,
+    if (true) {
+        window = window_persist::import_from_session(host_->session_path,
```

Red in `zengine-workshop-persistence-tests`:

- an older session's room is Workshop's window file's once: read while none stands, and never again
  once one does

## fold-to-the-first-desk

A version-3 settings file's entries fold into the first desk of their layout's name alone.

In `workshop/layouts/store.cpp`:

```diff
-                desk_at(run_, i) = std::move(candidate);
-                taken = true;
-            }
+                desk_at(run_, i) = std::move(candidate);
+                taken = true;
+                break;
+            }
```

Red in `zengine-workshop-persistence-tests`:

- what a version-3 settings file kept by layout name goes into each desk of the name, a copy each,
  and each entry no desk takes is named in full

## entries-dropped-at-the-fold

The settings file drops its layouts' entries as soon as they fold into the desks.

In `workshop/layouts/store.cpp`:

```diff
-        out.folded = fold_layout_entries();
-        folded = true;
+        out.folded = fold_layout_entries();
+        folded = true;
+        layout_entries_held_ = true;
+        (void)write_prefs();
```

Red in `zengine-workshop-persistence-tests`:

- the settings file keeps its layouts' entries until a session holds them: a run that ends without
  its close finds them again

## named-place-keeps-its-numbers

The version-5 lift copies a place's numbers whatever its word reads.

In `workshop/setup_persist.hpp`:

```diff
-        out = setup_persist::WorkshopPanePlace{w.mode, std::nullopt, std::nullopt};
+        out = setup_persist::WorkshopPanePlace{w.mode, w.x, w.y};
```

Red in `zengine-workshop-persistence-tests`:

- a place and a size carry a number exactly where the word reads one: a version-5 desk converts by
  its words, and a number where the word reads none is refused
