# The mutants `tests/test_shapes_census.cpp` claims

Each entry is a mutant that a comment in [`tests/test_shapes_census.cpp`](../test_shapes_census.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## field-at-same-version

`DeskRect` gains a field and keeps its version.

In `workshop/pane_view.hpp`:

```diff
-    std::int64_t x = 0, y = 0, w = 0, h = 0;
-    ZEN_SHAPE(DeskRect, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h));
+    std::int64_t x = 0, y = 0, w = 0, h = 0, z = 0;
+    ZEN_SHAPE(DeskRect, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h), ZEN_FIELD(z));
```

Red in `zengine-workshop-shapes-tests`:

- every shape this tree publishes is pinned by content id under its name and version

## create-given-no-sink

The census makes each weave with no sink for the words a refusal says.

In `tests/test_shapes_census.cpp`:

```diff
-    void* instance = abi->create(words.sink());
+    void* instance = abi->create(ZenByteSink{nullptr, nullptr});
```

Red in `zengine-workshop-shapes-tests`:

- a weave the census cannot make is named in the words it refused to be made in

## facet-never-asks

The settings facet every pane office takes its row through declares no ask and sends none, as a pane
office taking `PaneSettings` on its own would.

In `workshop/pane_settings_taken.hpp`:

```diff
-    using Emit = loom::Emit<PaneSettingsAsked, PaneSettingUnused>;
+    using Emit = loom::Emit<PaneSettingUnused>;
```

In `workshop/pane_settings_taken.hpp`:

```diff
-        if (!asked_) {
-            asked_ = mail.as_role(Office)
+        if constexpr (false) {
+            asked_ = mail.as_role(Office)
```

Red in `zengine-workshop-shapes-tests`:

- every weave that takes a pane's row asks the layouts' owner for it, and none hands one
