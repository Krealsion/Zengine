# The mutants `tests/test_workshop_panes_output.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_output.cpp`](../test_workshop_panes_output.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## reader-menu-close-alone

The output reader's menu offers `close` alone.

In `builder/builder-pane/view.hpp`:

```diff
-        say_row(pane::kMenuOutputUp, "a line up");
-        say_row(pane::kMenuOutputDown, "a line down");
-        say_row(pane::kMenuOutputFirst, "the first line");
-        say_row(pane::kMenuOutputLast, "the last lines");
-        say_row(pane::kMenuOutputLeft, "pan left");
-        say_row(pane::kMenuOutputRight, "pan right");
-        say_row(pane::kMenuOutputOlder, "the older build's output");
-        say_row(pane::kMenuOutputNewer, "the newer build's output");
```

Red in `zengine-workshop-panes-tests`:

- WL-OUT-04: in a room too small for its strip the reader's whole list is in its own menu, and every
  row of it acts
