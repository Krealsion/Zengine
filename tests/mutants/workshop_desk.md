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

Every graphics area is given `night`, whatever its row and Workshop keep.

In `workshop/weave_run.cpp`:

```diff
-        themes[area] = own != nullptr && own->text && is_workshop_theme_choice(*own->text)
-                          ? workshop_palette_of(*own->text)
-                          : worn;
+        themes[area] = own != nullptr && false ? *own->text : std::string(kThemeNight);
```

In `workshop/weave_run.cpp`:

```diff
-    themes[kWorkshopProvider] = worn;
+    themes[kWorkshopProvider] = kThemeNight;
```

Red in `zengine-workshop-desk-tests`:

- each graphics area wears the palette its row's `workshop.theme` names, else Workshop's own, and a
  palette is said only when an area's moved

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

## apply-keeps-held

An apply leaves a palette the theme reader already offers as it was.

In `settings/weave.cpp`:

```diff
-            if (worn.empty() || !offered.insert(name).second) {
+            if (worn.empty() || !offered.insert(name).second || held(name) != nullptr) {
```

Red in `zengine-workshop-desk-tests`:

- a theme edited on disk and applied changes the palette a window paints with no build, and no pane
  hears of it

## merge-from-defaults

A theme applied by `merge` is laid over the pool's defaults, not over the palette the layout wears.

In `settings/weave.cpp`:

```diff
-            if (!reset) {
+            if (!reset && false) {
```

Red in `zengine-workshop-desk-tests`:

- merge leaves what a theme does not name and reset returns it to its default, settings and colours
  alike
