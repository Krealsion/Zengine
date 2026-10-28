# The mutants `tests/test_view_builder.cpp` claims

Each entry is a mutant that a comment in [`tests/test_view_builder.cpp`](../test_view_builder.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## snap-reach-fixed

A place by hand snaps within `kSnapReach` whatever the layout keeps.

In `view/view-builder/picture.hpp`:

```diff
-    auto nearest = reach + 1;
+    auto nearest = kSnapReach + 1 + 0 * reach;
```

Red in `zengine-view-builder-tests`:

- the View Builder declares its snap reach and a new view's size as settings, and a layout's values
  reach the next placing hand and the next `new`

## new-view-size-fixed

`new` makes a view `kNewViewWidth` by `kNewViewHeight` whatever the layout keeps.

In `view/view-builder/model.hpp`:

```diff
-            description.width = new_width;
-            description.height = new_height;
+            description.width = kNewViewWidth;
+            description.height = kNewViewHeight;
```

Red in `zengine-view-builder-tests`:

- the View Builder declares its snap reach and a new view's size as settings, and a layout's values
  reach the next placing hand and the next `new`

## launch-forgets-the-project-file

A launch reads nothing from the builder's project file.

In `view/view-builder/pane.cpp`:

```diff
-        remembered_ = recorded(why);
-        if (launch) {
+        remembered_ = launch ? decltype(remembered_)() : recorded(why);
+        if (launch) {
```

Red in `zengine-view-builder-tests`:

- the View Builder runs again at a launch the view it ran, its pane seated by the desk

## restore-takes-the-file-box

A reload restores the file the view was saved to from what the File box holds.

In `view/view-builder/pane.cpp`:

```diff
-                    file_ = state_.file;
+                    file_ = state_.path;
```

Red in `zengine-view-builder-tests`:

- a reload keeps the file a view was saved to apart from a name typed into File and not saved, and
  the launch record names only the file

## record-counted-before-written

What the project file holds is counted before its write succeeds.

In `view/view-builder/pane.cpp`:

```diff
-        if (remembered_ == now) return;
+        if (remembered_ == now) return;
+        remembered_ = now;
```

Red in `zengine-view-builder-tests`:

- a launch record that could not be written is written at the next chance, a reload among them

## record-written-after-a-failed-launch

The project file is written from what the builder holds after a launch that could not open its file.

In `view/view-builder/pane.cpp`:

```diff
-        if (project_dir_.empty() || kept_) return;
+        if (project_dir_.empty()) return;
```

Red in `zengine-view-builder-tests`:

- a launch record whose view file is missing keeps that file and its run until the view opens again
  or the weaver opens, saves or starts another; a name typed into File is none of these

## plan-by-hand

The export writes the plan's text other than as the load plan's codec writes it.

In `app/folder.hpp`:

```diff
-    out.plan_text = workshop::load_persist::to_text(plan);
+    out.plan_text = workshop::load_persist::to_text(plan) + "\n";
```

Red in `zengine-view-builder-tests`:

- Export app writes the app's folder from the saved view and its flow: the probe's plan byte for
  byte, an app.json the app host reads, the two files, and launchers starting the app host beside
  this program

## plan-with-another-row

The export's plan holds the timer as a provider alone.

In `app/folder.hpp`:

```diff
-                      {"zengine-timer", provider, weave("zengine.timer")}};
+                      {"zengine-timer", provider, std::nullopt}};
```

Red in `zengine-view-builder-tests`:

- Export app writes the app's folder from the saved view and its flow: the probe's plan byte for
  byte, an app.json the app host reads, the two files, and launchers starting the app host beside
  this program

## export-written-before-the-answer

The export writes the app's folder before Workshop answers its ask.

In `view/view-builder/pane.cpp`:

```diff
-        exporting.replace = judged.exists;
+        exporting.replace = judged.exists;
+        export_app(exporting);
```

Red in `zengine-view-builder-tests`:

- an export Workshop refuses writes nothing, and says why

## no-second-press

An export over an app already there replaces it at the first press.

In `view/view-builder/pane.cpp`:

```diff
-        if (judged.exists && shown_.armed != "export") {
+        if (false && judged.exists && shown_.armed != "export") {
```

Red in `zengine-view-builder-tests`:

- an app already there is replaced only after a second press, and its decisions and every other file
  stay

## percent-left-single

A command file's launcher leaves a percent sign single.

In `app/folder.hpp`:

```diff
-            escaped += '%'; // a percent sign in a command file is written twice
+            (void)0;
```

Red in `zengine-view-builder-tests`:

- the export's own judgement: a program that is not there, and launchers that keep every path whole

## project-line-unread

An exported app's shell launcher never reads the project's line.

In `app/folder.hpp`:

```diff
-           "if [ -f " + line + " ]; then IFS= read -r app < " + line + "; fi\n"
```

Red in `zengine-view-builder-tests`:

- an exported app's launcher starts the app host its project's line names, and says why not when
  that program is gone

## launcher-before-workshop

Run app asks the launcher at once, before Workshop has allowed the hand.

In `view/view-builder/pane.cpp`:

```diff
-        if (asks().approve(mail, std::move(starting), mail.correlation(),
-                           {ws::scope::kBuild, ws::scope::kWrite}) != Asks::Asked::kHeld)
-            model_.notice = "Not started: Workshop could not be asked to allow it";
+        launch(starting.path, mail);
```

Red in `zengine-view-builder-tests`:

- Run app asks Workshop for a build and a write for the hand, then the launcher for the app the
  saved view was exported as, and says what started

## side-grows-with-the-pane

The left column takes two fifths of a pane narrower than 100 columns, as it once did, so the canvas
begins further right as the pane widens.

In `view/view-builder/picture.hpp`:

```diff
-    const auto side = std::clamp<std::int64_t>(columns - kLeastCanvasColumns,
-                                               std::min(columns, kLeastSideColumns), kSideColumns);
+    const auto side = std::min(columns, std::clamp<std::int64_t>(columns * 2 / 5, 30, kSideColumns));
```

Red in `zengine-view-builder-tests`:

- one layout at every width: the left column keeps its width, Add's kinds a row each beneath Grid
  and the values below the list, and every column more goes to the design canvas

## kinds-share-a-row

Add's kinds share one row beside `Add`, as they once did.

In `view/view-builder/picture.hpp`:

```diff
-        button(5, row++, title, "kind", {word});
+        button(5 + 9 * (row - 5), 5, title, "kind", {word}), ++row;
```

Red in `zengine-view-builder-tests`:

- one layout at every width: the left column keeps its width, Add's kinds a row each beneath Grid
  and the values below the list, and every column more goes to the design canvas

## list-keeps-four-rows

The element list keeps four rows before the selected element's values get any, as it once did.

In `view/view-builder/picture.hpp`:

```diff
-    const auto few = std::min<std::int64_t>(2, std::max<std::int64_t>(1, static_cast<std::int64_t>(d.elements.size())));
+    const auto few = std::min<std::int64_t>(4, std::max<std::int64_t>(1, static_cast<std::int64_t>(d.elements.size())));
```

Red in `zengine-view-builder-tests`:

- one layout at every width: the left column keeps its width, Add's kinds a row each beneath Grid
  and the values below the list, and every column more goes to the design canvas
