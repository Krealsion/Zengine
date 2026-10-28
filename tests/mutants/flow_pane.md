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

## the-x-names-the-first-step

The x on the chosen step's corner names the first step, whichever step it stands on.

In `flow/flow-pane/view.hpp`:

```diff
             pen.button(rows.box.right() - xw - (light ? 4 : 0), rows.head - (light ? 3 : 0), "x", "remove",
-                       {std::to_string(n)}, Face::Quiet, Icon::None, canvas);
+                       {"0"}, Face::Quiet, Icon::None, canvas);
```

Red in `zengine-flow-pane-tests`:

- the chosen step goes by the Delete key or by the x on its corner, which the Do lane's head no
  longer holds

## delete-keeps-the-step

The Delete key leaves the chosen step where it is.

In `flow/flow-pane/pane.cpp`:

```diff
     } else if (key.scancode == input::scan::kDelete && model_.node) {
-      perform("remove", {std::to_string(*model_.node)}, mail);
     } else if (key.scancode == input::scan::kReturn) {
```

Red in `zengine-flow-pane-tests`:

- the chosen step goes by the Delete key or by the x on its corner, which the Do lane's head no
  longer holds
