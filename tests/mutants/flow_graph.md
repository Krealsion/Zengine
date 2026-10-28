# The mutants `tests/test_flow_graph.cpp` claims

Each entry is a mutant that a comment in [`tests/test_flow_graph.cpp`](../test_flow_graph.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## card-places-unwritten

A saved workspace leaves out where each When and Then card was put.

In `flow/workspace.hpp`:

```diff
     file.pan_x = workspace.pan_x; file.pan_y = workspace.pan_y; file.zoom = workspace.zoom;
-    file.docks = workspace.docks;
     auto bytes = loom::serialize(loom::to_value(file));
```

Red in `zengine-flow-tests`:

- a workspace keeps where each When and Then card was put, reads one version 1 wrote with none
  put, and refuses a card put twice

## version-one-refused

A workspace version 1 wrote is refused rather than read with no card put.

In `flow/workspace.hpp`:

```diff
-    if (claim.claimed_version() == v1::WorkspaceFile::zen_version) {
+    if (false) {
```

Red in `zengine-flow-tests`:

- a workspace keeps where each When and Then card was put, reads one version 1 wrote with none
  put, and refuses a card put twice
