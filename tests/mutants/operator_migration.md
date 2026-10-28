# The mutants `tests/test_operator_migration.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_operator_migration.cpp`](../test_operator_migration.cpp) claims its cases catch, the
comment naming the entry by its path and heading. [The corpus's front page](README.md) says how an
entry reads and how `tests/mutants/run.py` runs it.

## source-check-removed

A migration is spent without asking what vintage its conversion reads.

In `operator/migration.hpp`:

```diff
-    if (def->inputs()->name() != target->name() ||
-        def->inputs()->version() != claim.claimed_version()) {
+    if (false) {
```

Red in `zengine-operator-tests`:

- a conversion at the right name converting the WRONG VINTAGE is not spent
