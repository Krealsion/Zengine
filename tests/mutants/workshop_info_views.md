# The mutants `tests/test_workshop_info_views.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_info_views.cpp`](../test_workshop_info_views.cpp) claims its cases catch, the
comment naming the entry by its path and heading. [The corpus's front page](README.md) says how an
entry reads and how `tests/mutants/run.py` runs it.

## levels-open-at-first

Every level of a value view's tree is open when the value is first shown.

In `info/value_view.hpp`:

```diff
-            return std::find(open.begin(), open.end(), field_name(fields[i].ref)) != open.end();
+            return true;
```

Red in `zengine-workshop-panes-tests`:

- WL-INFO-16: Info's nested values are a tree -- first shown, every nested message and every list
  holding items shut with its summary, each row its own name indented by its depth; Right opens a
  level and steps in, Left shuts it and steps out, shift+left shuts every level with the cursor on
  the first field, a press on a level's mark opens or shuts it, levels open side by side, and the
  cursor walks only the rows shown

## shut-all-keeps-the-cursor

Shutting every level leaves the selection where it was, inside a level now shut.

In `info/value_view.hpp`:

```diff
-        if (id == kValueShutAll) { forget_levels(); select_first(); return out; }
+        if (id == kValueShutAll) { forget_levels(); return out; }
```

Red in `zengine-workshop-panes-tests`:

- WL-INFO-16: Info's nested values are a tree -- first shown, every nested message and every list
  holding items shut with its summary, each row its own name indented by its depth; Right opens a
  level and steps in, Left shuts it and steps out, shift+left shuts every level with the cursor on
  the first field, a press on a level's mark opens or shuts it, levels open side by side, and the
  cursor walks only the rows shown

## cursor-walks-hidden-rows

The cursor steps through every field, the ones a shut level hides included.

In `info/value_view.hpp`:

```diff
     void step(const std::vector<Field>& fields, int by) {
-        const Tree t = tree(fields);
+        Tree t = tree(fields);
+        t.shown = component::shown_tree(t.depths, [](std::size_t) { return true; });
```

Red in `zengine-workshop-panes-tests`:

- WL-INFO-16: Info's nested values are a tree -- first shown, every nested message and every list
  holding items shut with its summary, each row its own name indented by its depth; Right opens a
  level and steps in, Left shuts it and steps out, shift+left shuts every level with the cursor on
  the first field, a press on a level's mark opens or shuts it, levels open side by side, and the
  cursor walks only the rows shown

## levels-not-in-the-state

What a reload in place takes of a view leaves its open levels out, so a reload shuts them.

In `info/value_view.hpp`:

```diff
-        s.open = open_levels();
+        s.open = {};
```

Red in `zengine-workshop-panes-tests`:

- WL-INFO-16: a view's open levels are its pane's state -- read there and written there, kept by a
  reload in place, carried by a fork, and shut for a new subject

## levels-kept-for-a-new-subject

A view given a new value keeps the old value's open levels.

In `info/value_view.hpp`:

```diff
-        forget_levels(); // a new subject is first shown with every level shut
+        (void)0;
```

Red in `zengine-workshop-panes-tests`:

- WL-INFO-16: a view's open levels are its pane's state -- read there and written there, kept by a
  reload in place, carried by a fork, and shut for a new subject
