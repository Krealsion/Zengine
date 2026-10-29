# The mutants `tests/test_workshop_inventory_info.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_inventory_info.cpp`](../test_workshop_inventory_info.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## every-view-offered-again

The Inventory pane offers every view again at each change, unchanged ones too.

In `inventory/inventory-pane/pane.cpp`:

```diff
-            if(!all && heard==said) return;
+            if(false && !all && heard==said) return;
```

Red in `zengine-workshop-panes-tests`:

- views: making a view leaves a menu open on main Inventory open, since only the view made is offered again
