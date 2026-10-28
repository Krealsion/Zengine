# The mutants `tests/test_workshop_panes_info.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_info.cpp`](../test_workshop_panes_info.cpp) claims its cases catch, the
comment naming the entry by its path and heading. [The corpus's front page](README.md) says how an
entry reads and how `tests/mutants/run.py` runs it.

## info-seated-by-default

The shipped desk seats Info with a `default` place.

In `workshop/setup.hpp`:

```diff
-        {PaneRef{kInfoPaneProvider, kInfoPaneKey}, ui::kPlaceTopRight},
+        {PaneRef{kInfoPaneProvider, kInfoPaneKey}, pane_unit::kDefault},
```

Red in `zengine-workshop-panes-tests`:

- the shipped desk puts it at the room's top-right, by word and not by number

## typed-size-a-count

`write_pane_axis` reads a typed size as a count of the face's unit, a share too.

In `workshop/screen_pane_subject.cpp`:

```diff
-        } else if (const std::optional<std::int64_t> share = typed_share(body)) {
+        } else if (const std::optional<std::int64_t> share = std::optional<std::int64_t>{}) {
```

Red in `zengine-workshop-panes-tests`:

- Info's typed axes take a word as well as a number, and each reads back as it was typed

## typed-place-a-count

`write_pane_axis` reads a typed share place as a count of the face's unit.

In `workshop/screen_pane_subject.cpp`:

```diff
-    if (share || head == ui::place_word(ui::kPlaceShare)) {
+    if (false) {
```

Red in `zengine-workshop-panes-tests`:

- Info's X and Y take a share place as a size's share is typed, and read it back

## tab-passes-over-the-invisible

`arrangeable` passes over a pane its words leave with no visible cell.

In `workshop/weave_arrange.cpp`:

```diff
-        if (has_pane(session_.setup.active, row.ref)) {
-            out.push_back(row.ref);
+        if (has_pane(session_.setup.active, row.ref) && row.kind != kNoPaneKind &&
+            !bounds_of(session_.panes, session_.setup.active, row.kind, screen_of(session_)).rect.empty()) {
+            out.push_back(row.ref);
```

Red in `zengine-workshop-panes-tests`:

- a pane its words leave too small to see, or off the room, is still listed, reached by tab,
  inspected by Info and given its size back there
