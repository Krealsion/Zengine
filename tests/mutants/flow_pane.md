# The mutants `tests/test_flow_pane.cpp` claims

Each entry is a mutant that a comment in [`tests/test_flow_pane.cpp`](../test_flow_pane.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## empty-press-keeps-the-step

A press on empty graph room keeps the chosen step chosen.

In `flow/flow-pane/pane.cpp`:

```diff
         if (!hit && (model_.node || model_.filling)) {
-          model_.node.reset();
           model_.filling.reset();
```

Red in `zengine-flow-pane-tests`:

- a press on empty graph room lets go of the chosen step and its port, a drag from there moves the
  view, and a step moves only by a drag that starts on it

## empty-press-keeps-the-port

A press on empty graph room keeps the chosen port chosen.

In `flow/flow-pane/pane.cpp`:

```diff
         if (!hit && (model_.node || model_.filling)) {
           model_.node.reset();
-          model_.filling.reset();
           show(mail);
```

Red in `zengine-flow-pane-tests`:

- a press on empty graph room lets go of the chosen step and its port, a drag from there moves the
  view, and a step moves only by a drag that starts on it
