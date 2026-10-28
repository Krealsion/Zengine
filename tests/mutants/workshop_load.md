# The mutants `tests/test_workshop_load.cpp` claims

Each entry is a mutant that a comment in [`tests/test_workshop_load.cpp`](../test_workshop_load.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## first-refusal-fails-the-row

A row's refused artifact fails the row: the next artifact it names is never tried.

In `workshop/load_execute.hpp`:

```diff
-        if (!on_demand_ && cursor_ < plan_.artifacts.size() &&
-            !plan_.artifacts[cursor_].otherwise.empty()) {
+        if (false && !on_demand_ && cursor_ < plan_.artifacts.size() &&
+            !plan_.artifacts[cursor_].otherwise.empty()) {
```

Red in `zengine-workshop-load-tests`:

- a row's artifact whose load is refused gives way to the next it names, said once in Loom's words;
  one this installation lacks gives way unsaid; the first that loads stands in the row

## window-kept-without-opening

The OS window's backend goes on as a window when SDL cannot start its video.

In `graphics/window/window_sdl_backend.hpp`:

```diff
             release_sdl(); // a failed init still allocated: see release_sdl
-            give_back_input();
-            throw std::runtime_error(why);
-        }
+            give_back_input();
+            (void)why;
+            return;
+        }
```

Red in `zengine-workshop-load-tests`:

- the shipped window row in the OS window's absence: told a driver it lacks, the OS window gives way
  to the terminal window, said once, and leaves nothing loaded; in a build without it, the terminal
  window loads unsaid

## retired-row-opened

`begin` walks the plan without looking for a retired artifact.

In `workshop/load_execute.hpp`:

```diff
-        if (RetiredRow retired = retired_row(plan_); !retired.refusal.empty()) {
+        if (RetiredRow retired = retired_row(plan_); false && !retired.refusal.empty()) {
```

Red in `zengine-workshop-load-tests`:

- a plan naming a retired artifact is refused by name, with its fix, before any row opens

## author-takes-the-plan-as-run

The plan's writer appends to the plan as it ran, not to the plan as it was read.

In `workshop/authoring.hpp`:

```diff
-    load::LoadPlan candidate = plan.rows;
+    load::LoadPlan candidate = plan.executor != nullptr ? plan.executor->plan() : plan.rows;
```

Red in `zengine-workshop-load-tests`:

- the project plan `load it` writes carries the window row as it was read, `otherwise` and all,
  never the window that opened

## optional-arm-dropped

An optional row that refuses fails the plan, as a row that is not optional does.

In `workshop/load_execute.hpp`:

```diff
-            plan_.artifacts[cursor_].optional) {
+            plan_.artifacts[cursor_].optional && false) {
```

Red in `zengine-workshop-load-tests`:

- an optional row that refuses is an unavailable tool: it is stepped over, named, and the rows
  behind it are still performed in authored order

## cursor-advanced-twice

An optional row that refuses moves the cursor itself, and the caller moves it again.

In `workshop/load_execute.hpp`:

```diff
-            stepped_over_.push_back(SteppedOver{current_.stem, why});
+            stepped_over_.push_back(SteppedOver{current_.stem, why});
+            ++cursor_;
```

Red in `zengine-workshop-load-tests`:

- an optional row that refuses is an unavailable tool: it is stepped over, named, and the rows
  behind it are still performed in authored order

## stepped-over-arm-dropped

`state_of` never answers `Unavailable` for a row the walk stepped over.

In `workshop/load_execute.hpp`:

```diff
-            if (gone.stem == stem) {
-                return RowState::Unavailable;
+            if (false && gone.stem == stem) {
+                return RowState::Unavailable;
```

Red in `zengine-workshop-load-tests`:

- an optional row that refused stays `unavailable` in the owner's own answers -- never `authored` --
  with its reason, the next step and its optional flag

## office-never-pending

`office_pending` answers false whatever the rows' states.

In `workshop/load_execute.hpp`:

```diff
-                now == RowState::Pending) {
-                return true;
+                now == RowState::Pending) {
+                return false;
```

Red in `zengine-workshop-load-tests`:

- an office is still to come while a plan row loading it has not settled, and is owed nothing once
  every such row resolved or was stepped over
