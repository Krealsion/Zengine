# The mutants `tests/test_workshop_panes_seam.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_seam.cpp`](../test_workshop_panes_seam.cpp) claims its cases catch, the
comment naming the entry by its path and heading. [The corpus's front page](README.md) says how an
entry reads and how `tests/mutants/run.py` runs it.

## launch-off-screen-silent

A launch of a pane whose place stands off this screen says nothing of where it went.

In `workshop/weave_desktop.cpp`:

```diff
-            const std::string off = answer.refusal.empty() && kind.has_value()
+            const std::string off = false && answer.refusal.empty() && kind.has_value()
```

Red in `zengine-workshop-panes-tests`:

- a pane launched where its own place stands off this screen stays there, and the band says so and
  how to bring it back

## off-screen-sentence-long

The off-screen sentence names the pane twice and spells each way back in full.

In `workshop/weave_desktop.cpp`:

```diff
-           " is off this screen -- Pane Manager > manage it > Reset > place, or hide and show it";
+           " is off this screen -- Reset > place brings it back (the Pane Manager's manage " + name + " > reaches it), or hide it and show it again";
```

Red in `zengine-workshop-panes-tests`:

- the off-screen sentence fits a hundred-column terminal whole, both ways back included
