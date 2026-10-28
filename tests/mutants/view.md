# The mutants `tests/test_view.cpp` claims

Each entry is a mutant that a comment in [`tests/test_view.cpp`](../test_view.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## start-always-reveals

A view's start asks to be shown whatever it was told.

In `view/view.hpp`:

```diff
-            if (loom::from_value<detail::ViewStart>(in.payload).reveal) say(bus, ws::PaneRevealRequested{kPane});
+            if (true) say(bus, ws::PaneRevealRequested{kPane});
```

Red in `zengine-view-tests`:

- a resumed view offers its pane and asks nothing of the desk; a run asks to be shown

## successor-reveals

A successor at the same name is started as a run is, asking to be shown.

In `view/host.hpp`:

```diff
-            host_.start(successor, renamed);
+            host_.start(successor, true);
```

Red in `zengine-view-tests`:

- a view registered afresh at its own name asks nothing of the desk, so its pane keeps its place and
  the keys stay; renamed, it asks to be shown

## deaf-to-the-pointer

A view accepts no press of the window that shows its area.

In `view/view.hpp`:

```diff
-                       loom::schema_of<graphics::AreaButtonChanged>(), loom::schema_of<graphics::AreaPointerMoved>(),
+                       loom::schema_of<graphics::AreaPointerMoved>(),
```

Red in `zengine-view-tests`:

- a view hears the pointer of the window that shows its area: a field pressed takes the focus and
  what is typed, a button pressed is used once, and the same from any other office changes nothing

## grant-without-wiring

A view's grant leaves out `ViewWiring`.

In `view/view.hpp`:

```diff
-    grant.allow_to_any(ViewWiring::zen_name, ViewWiring::zen_version);
```

Red in `zengine-view-tests`:

- a running view says what its wiring has done, as its own office: each intent's last send accepted
  or refused, each field told, and again only when a row changes

## refusal-of-any-send

A refusal counts against the intent's last send whichever send it answers.

In `view/view.hpp`:

```diff
-                last != last_.end() && last->second.correlation == in.correlation) {
+                last != last_.end()) {
```

Red in `zengine-view-tests`:

- a view's intent that nothing accepts is said unheard, a refusal of an earlier send changes nothing
  of the last, and an office that arrives later asks and is answered alone
