# The mutants `tests/test_workshop_panes_editor.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_editor.cpp`](../test_workshop_panes_editor.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## reveal-seats-by-name

A reveal seats the first offered pane of its name, whichever office offered it.

In `workshop/weave_seam.cpp`:

```diff
-    const RuntimePane* row = session_.panes.runtime.find(office, asked.pane);
-    if (row == nullptr) {
-        return;
-    }
-    // COPIED OUT BEFORE THE DESK MOVES: `apply_setup` may re-ask providers and the catalog
+    const RuntimePane* row = nullptr;
+    for (const RuntimePane& r : session_.panes.runtime.entries) {
+        if (r.pane == asked.pane && row == nullptr) {
+            row = &r;
+        }
+    }
+    if (row == nullptr) {
+        return;
+    }
+    // COPIED OUT BEFORE THE DESK MOVES: `apply_setup` may re-ask providers and the catalog
```

Red in `zengine-workshop-panes-tests`:

- a reveal from an office that offered no such pane is dropped

## tab-stop-fixed

The Editor's geometry draws a tab `kEditorTabStop` wide whatever the layout keeps.

In `editor/editor-pane/editor.hpp`:

```diff
-            width = tab - (col % tab);
+            width = kEditorTabStop - (col % kEditorTabStop) + 0 * tab;
```

Red in `zengine-workshop-panes-tests`:

- the Editor declares four settings, and a layout's tab stop, status row, wheel and undo depth are
  what its rows, its wheel and its history spend

## undo-depth-fixed

The Editor's undo bound stays `kEditorUndoDepth` whatever the layout keeps.

In `editor/editor-pane/pane.cpp`:

```diff
-        doc.buffer.set_undo_depth(undo_depth_);
```

Red in `zengine-workshop-panes-tests`:

- the Editor declares four settings, and a layout's tab stop, status row, wheel and undo depth are
  what its rows, its wheel and its history spend

## quit-answer-unasked

Workshop takes a quit answer without asking whether it answers its own ask.

In `workshop/weave_run.cpp`:

```diff
-    if (!mail.answers_ask() || !quitting_ || mail.correlation() != quit_ask_) {
+    if (!quitting_) {
```

Red in `zengine-workshop-panes-tests`:

- a forged quit answer moves nothing -- only Loom's answer to the host's ask decides

## window-written-at-the-ask

The window file is written when the quit is asked, before the panes answer it.

In `workshop/weave_run.cpp`:

```diff
-    quit_ask_ = paste_asks_.mint_correlation();
+    quit_ask_ = paste_asks_.mint_correlation();
+    (void)window_persist::save_file(window_persist::path_beside(host_->session_path),
+                                    session_.normal_w, session_.normal_h,
+                                    window_persist::Placement{});
```

Red in `zengine-workshop-panes-tests`:

- an orderly close writes the window file and the session, and a refused quit writes neither
