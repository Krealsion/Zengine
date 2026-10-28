# The mutants `tests/test_ui.cpp` claims

Each entry is a mutant that a comment in [`tests/test_ui.cpp`](../test_ui.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## share-offset-as-pixels

`edges_of` reads a share place's offsets as pixels.

In `ui/layout.hpp`:

```diff
-    } else if (a.offset_share) {
+    } else if (false) {
```

Red in `zengine-ui-tests`:

- a share place stands its part a share of the frame's width and height from its top-left, floored
  to the step

## share-place-as-its-anchor

`reads_of_frame` reads a share place as its anchor alone.

In `ui/vocabulary.hpp`:

```diff
-    if (place_reads_share(place)) {
-        return std::string("stands a share of the ") + (across ? "width" : "height") + " into";
+    if (false) {
+        return std::string("stands a share of the ") + (across ? "width" : "height") + " into";
```

Red in `zengine-ui-tests`:

- problem names a share in a frame that fits, a missing context and a looping one
