# The mutants `tests/test_graphics.cpp` claims

Each entry is a mutant that a comment in [`tests/test_graphics.cpp`](../test_graphics.cpp) claims
its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## key-moments-to-keys

The window sends a key's release and its text to every area placed to hear keys.

In `graphics/window/window.hpp`:

```diff
-    static bool hears(const WinArea& a, const AreaKeyReleased&) { return a.releases; }
+    static bool hears(const WinArea& a, const AreaKeyReleased&) { return a.keys; }
```

In `graphics/window/window.hpp`:

```diff
-    static bool hears(const WinArea& a, const AreaTextEntered&) { return a.text; }
+    static bool hears(const WinArea& a, const AreaTextEntered&) { return a.keys; }
```

Red in `zengine-graphics-tests`:

- a key, its release and the text it types go from the window to the owner of each area placed to
  hear that moment, and to no other, each naming that area and the weaver's own hand; an owner
  placed for keys alone is given nothing else, and nothing is refused

## attention-draws-before-its-row

The Attention pane draws its room before it holds its row.

In `attention/pane.cpp`:

```diff
-        if (this->template facet<Settings>().held()) {
-            room().show(*this, mail);
+        if (true) {
+            room().show(*this, mail);
```

Red in `zengine-graphics-tests`:

- the Attention pane draws nothing in its room before it holds its row, takes a row from the
  layouts' owner alone, and draws whole once it holds one

## row-from-any-office

A pane takes the row any office hands it as its own.

In `workshop/pane_settings_taken.hpp`:

```diff
-        if (!from_owner(mail) || handed.pane != Pane) {
+        if (handed.pane != Pane) {
```

Red in `zengine-graphics-tests`:

- the Attention pane draws nothing in its room before it holds its row, takes a row from the
  layouts' owner alone, and draws whole once it holds one

## shaped-by-any-office

An OS window takes its title and size from any office.

In `graphics/window/window.hpp`:

```diff
-                return !a.arranger.empty() && mail.authored_from_role(a.arranger);
+                return true || (!a.arranger.empty() && mail.authored_from_role(a.arranger));
```

Red in `zengine-graphics-tests`:

- an OS window takes its title and its size from an office that arranges an area in it, and from no
  other; a shaped window is not grown back by what it held before

## drawing-rest-from-a-drag

The drawing facet hands a pane a move a held press carries as though the pointer rested there.

In `workshop/pane_facets.hpp`:

```diff
             const std::optional<WindowPointer> p = window_.pointer(m, mail, 0, 0);
-            if (!p || p->press != 0) {
+            if (!p) {
```

Red in `zengine-graphics-tests`:

- a pane on the drawing facet is handed the part the pointer rests on, once each time that part
  changes, and its leave once a rest; a move a held press carries is no rest
