# The mutants `tests/test_flow_view.cpp` claims

Each entry is a mutant that a comment in [`tests/test_flow_view.cpp`](../test_flow_view.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## doubles-at-any-time

Two presses double however far apart in time.

In `flow/flow-pane/gesture.hpp`:

```diff
-    return now_ms >= prior.at_ms && now_ms - prior.at_ms <= kDoublePressMs;
+    return true;
```

Red in `zengine-flow-tests`:

- two presses are one double click on one target, near where the first landed, within the interval,
  and time is its argument

## doubles-on-any-target

A press doubles the one before it whatever each landed on.

In `flow/flow-pane/gesture.hpp`:

```diff
-    if (!prior.armed || prior.target.empty() || prior.target != target) return false;
+    if (!prior.armed || prior.target.empty()) return false;
```

Red in `zengine-flow-tests`:

- two presses are one double click on one target, near where the first landed, within the interval,
  and time is its argument
