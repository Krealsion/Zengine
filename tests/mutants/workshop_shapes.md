# The mutants `tests/test_workshop_shapes.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_shapes.cpp`](../test_workshop_shapes.cpp) claims its cases catch, the comment
naming the entry by its path and heading. [The corpus's front page](README.md) says how an entry
reads and how `tests/mutants/run.py` runs it.

## inventory-kept-at-version-one

`PaneInventory` stays at version 1 while the `InventoryPane` it carries is at version 2.

In `workshop/desktop_seam_vocabulary.hpp`:

```diff
-    ZEN_SHAPE(PaneInventory, 2, ZEN_FIELD(panes));
+    ZEN_SHAPE(PaneInventory, 1, ZEN_FIELD(panes));
```

Red in `zengine-workshop-shapes-tests`:

- a shape that encloses a changed shape is a new version, so the shapes published before claim
  beside the current ones in one registry

## second-component-declares-run-build

The Builder pane's vocabulary declares a `RunBuild` of its own beside the builder's.

In `builder/builder-pane/vocabulary.hpp`:

```diff
-} // namespace zengine::builder_pane
+struct RunBuild {
+    std::string recipe;
+    ZEN_SHAPE(RunBuild, 2, ZEN_FIELD(recipe));
+};
+
+} // namespace zengine::builder_pane
```

The cases read `builder/builder-pane/vocabulary.hpp` as text.

Red in `zengine-workshop-shapes-tests`:

- no two components of this tree declare a shape of one name

## rig-booter-state-named-boot-state

The test rig's booter state is named `BootState`, as the plan booter's is.

In `tests/workshop_support.hpp`:

```diff
-struct RigBootState {
+struct BootState {
```

In `tests/workshop_support.hpp`:

```diff
-    ZEN_SHAPE(RigBootState, 1, ZEN_FIELD(n));
+    ZEN_SHAPE(BootState, 1, ZEN_FIELD(n));
```

In `tests/workshop_support.hpp`:

```diff
-class Booter : public loom::WeaveBase<Booter, RigBootState,
+class Booter : public loom::WeaveBase<Booter, BootState,
```

Red in `zengine-workshop-shapes-tests`:

- the test rig's booter loads a library after a plan has run, its state no shape of Workshop's
