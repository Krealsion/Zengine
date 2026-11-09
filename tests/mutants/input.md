# The mutants `tests/test_input.cpp` claims

Each entry is a mutant that a comment in [`tests/test_input.cpp`](../test_input.cpp) claims its
cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## the-time-never-set

The input role hears the setting Workshop hands it and keeps its default.

In `input/double_click_setting.hpp`:

```diff
-        self.set_double_click_ms(ms, mail);
+        (void)self;
+        (void)ms;
```

Red in `zengine-input-tests`:

- the input role keeps the double-click time: declared to the layouts' owner at each activation,
  said to an asker, set from what the owner hands it, and published when it changes

