# The mutants `tests/test_workshop_panes_canvas.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_canvas.cpp`](../test_workshop_panes_canvas.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## off-screen-reveal-said-shown

A reveal of a pane whose place stands off this screen says it is shown, and nothing of where.

In `workshop/weave_seam.cpp`:

```diff
-    if (const std::string off = off_this_screen(kind, name); !off.empty()) {
+    if (const std::string off = off_this_screen(kind, name); off.empty() && false) {
```

Red in `zengine-workshop-panes-tests`:

- a view shown where its own place stands off this screen stays there, and the band says so and how
  to bring it back
