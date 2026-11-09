# The mutants `tests/test_workshop_desk.cpp` claims

Each entry is a mutant that a comment in [`tests/test_workshop_desk.cpp`](../test_workshop_desk.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## readout-says-dash

The readout says `-` for every word: no place word, and a share as `-`.

In `workshop/screen_pane_state.cpp`:

```diff
-    } else if (mode != pane_unit::kDefault) {
-        // A PLACE WORD, and the offsets it reads: a share place's as shares.
+    } else if (false) {
+        // A PLACE WORD, and the offsets it reads: a share place's as shares.
```

In `workshop/screen_pane_state.cpp`:

```diff
-        case ui::kSizeShare: return share_text(s.amount);
-        case ui::kSizeFit:
-        case ui::kSizeFill: return std::string(ui::size_word(s.mode));
+        case ui::kSizeShare:
+        case ui::kSizeFit:
+        case ui::kSizeFill: return std::string("-");
```

Red in `zengine-workshop-desk-tests`:

- Workshop's notice says a pane's words, and the desk read the pixels they came to

## every-area-night

Every graphics area is given the pool's own palette, whatever its layout and its row keep.

In `workshop/weave_run.cpp`:

```diff
-        skins[area] = seat != nullptr ? wears(theme::stack_over(stack, seat->theme)) : worn;
+        skins[area] = std::string(); (void)seat;
```

In `workshop/weave_run.cpp`:

```diff
-    skins[kWorkshopProvider] = worn;
+    skins[kWorkshopProvider] = std::string();
```

Red in `zengine-workshop-desk-tests`:

- each graphics area wears the palette named for the live layout's themes, a pane's row's theme
  laid over them for its area and its frame alone, and a palette is said only when an area's moved

## area-named-by-its-role

A pane's graphics area is named by its role alone.

In `workshop/pane_drawing.hpp`:

```diff
-    std::string area(role);
-    area += '.';
-    area += pane;
-    return area;
+    (void)pane;
+    return std::string(role);
```

Red in `zengine-workshop-desk-tests`:

- every pane's graphics area is `<role>.<pane>`, a role's one pane as each of its several

## only-the-tui-window

Workshop takes the TUI window's offer alone.

In `workshop/weave_handlers.cpp`:

```diff
-    if ((w.window != graphics::kTuiWindow && w.window != graphics::kSdlWindow) ||
+    if ((w.window != graphics::kTuiWindow) ||
```

Red in `zengine-workshop-desk-tests`:

- the TUI window offered brings Workshop up in it, the last session restored first, and lays
  the desk out in the window's room
