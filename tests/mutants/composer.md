# The mutants `tests/test_composer.cpp` claims

Each entry is a mutant that a comment in [`tests/test_composer.cpp`](../test_composer.cpp) claims
its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## fields-unsaid

The catalog's rows name each message by its name and version alone, and none of its fields.

In `composer/view.hpp`:

```diff
-                        std::to_string(root->version()) + "  " + fields_text(*root),
+                        std::to_string(root->version()),
```

Red in `zengine-composer-tests`:

- the catalog says each message's fields after its name and version, as the shape declares and
  spells them, and a row too long for its pane is cut to it

## optional-unmarked

An optional field is said as a required one is, with no `?` after its type.

In `composer/view.hpp`:

```diff
-        out += (i > 0 ? ", " : "") + f.name + ": " + f.type + (f.required ? "" : "?");
+        out += (i > 0 ? ", " : "") + f.name + ": " + f.type;
```

Red in `zengine-composer-tests`:

- the catalog says each message's fields after its name and version, as the shape declares and
  spells them, and a row too long for its pane is cut to it

## fields-past-the-pane

The fields are added after the row was fitted to its pane, so a long row runs past the pane's
columns.

In `composer/view.hpp`:

```diff
-            say(fit(std::string(chosen ? kSelectedMark : kUnselectedMark) + root->name() + " v" +
-                        std::to_string(root->version()) + "  " + fields_text(*root),
-                    columns),
+            say(fit(std::string(chosen ? kSelectedMark : kUnselectedMark) + root->name() + " v" +
+                        std::to_string(root->version()),
+                    columns) + "  " + fields_text(*root),
```

Red in `zengine-composer-tests`:

- the catalog says each message's fields after its name and version, as the shape declares and
  spells them, and a row too long for its pane is cut to it
