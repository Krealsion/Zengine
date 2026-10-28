# The mutants `tests/test_app_host.cpp` claims

Each entry is a mutant that a comment in [`tests/test_app_host.cpp`](../test_app_host.cpp) claims
its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## session-before-lock

The app's session file is written before the host claims the app's lock.

In `app/host.hpp`:

```diff
-        bool held_by_another = false;
+        session_ = SessionFile(place_.folder);
+        bool held_by_another = false;
```

Red in `zengine-app-host-tests`:

- while a host holds an app, its session file stands in the app's folder naming the host's process;
  a host refused the app writes none, and the file goes when the host does

## views-deaf-to-pointer

The app's weave places each view's area not hearing the pointer.

In `app/weave.cpp`:

```diff
-                                 v.h + v.notice_rows * line, 1, true, true};
+                                 v.h + v.notice_rows * line, 1, true, false};
```

Red in `zengine-app-host-tests`:

- a field of the app's view is clicked into in its window and typed into, and Count gives what was
  typed

## settled-to-every-arranger

The window tells the arranger of every area what it shows, settled, whether it follows the area or
not.

In `graphics/window/window.hpp`:

```diff
-            if (!followed(a)) {
-                continue;
-            }
-            const std::int64_t version = version_of(a.area);
+            if (false && !followed(a)) {
+                continue;
+            }
+            const std::int64_t version = version_of(a.area);
```

Red in `zengine-app-host-tests`:

- the app's window tells the app's weave and its views nothing they do not take: its showings, a
  press and typing put no refused notice on the bus

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

Red in `zengine-app-host-tests`:

- the app's window tells the app's weave and its views nothing they do not take: its showings, a
  press and typing put no refused notice on the bus

## window-unshaped

The app's weave never tells its window the title and size `app.json` says.

In `app/weave.cpp`:

```diff
-        if (!read_ || r.window != window_.window || shaped_epoch_ == r.epoch) {
+        if (true || !read_ || r.window != window_.window || shaped_epoch_ == r.epoch) {
```

Red in `zengine-app-host-tests`:

- the app's window takes the title and the size its app.json says, once a window, and the app's line
  stands at its new foot
