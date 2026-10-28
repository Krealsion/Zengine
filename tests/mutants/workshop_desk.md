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

## every-area-classic

Every graphics area is given `classic`, whatever its row and Workshop keep.

In `workshop/weave_run.cpp`:

```diff
-        skins[area] = own != nullptr && own->text && is_workshop_skin_choice(*own->text)
-                          ? *own->text
-                          : session_.skin;
+        skins[area] = own != nullptr && false ? *own->text : std::string(kSkinClassic);
```

In `workshop/weave_run.cpp`:

```diff
-    skins[kWorkshopProvider] = session_.skin;
+    skins[kWorkshopProvider] = kSkinClassic;
```

Red in `zengine-workshop-desk-tests`:

- each graphics area wears its pane row's `workshop.skin`, else Workshop's own, and a skin is said
  only when an area's moved

## area-named-by-its-office

A pane's graphics area is named by its office alone.

In `workshop/pane_drawing.hpp`:

```diff
-    std::string area(office);
-    area += '.';
-    area += pane;
-    return area;
+    (void)pane;
+    return std::string(office);
```

Red in `zengine-workshop-desk-tests`:

- every pane's graphics area is `<office>.<pane>`, an office's one pane as each of its several

## only-the-terminal-window

Workshop takes the terminal window's offer alone.

In `workshop/weave_handlers.cpp`:

```diff
-    if ((w.window != graphics::kTerminalWindow && w.window != graphics::kSdlWindow) ||
+    if ((w.window != graphics::kTerminalWindow) ||
```

Red in `zengine-workshop-desk-tests`:

- the terminal window offered brings Workshop up in it, the last session restored first, and lays
  the desk out in the window's room
