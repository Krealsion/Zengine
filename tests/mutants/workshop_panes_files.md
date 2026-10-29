# The mutants `tests/test_workshop_panes_files.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_files.cpp`](../test_workshop_panes_files.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## wheel-rows-fixed

Files' wheel moves `kFilesWheelRows` rows a notch whatever the layout keeps.

In `files/files.cpp`:

```diff
-            wheel_accum_, dy * static_cast<double>(wheel_rows_));
+            wheel_accum_, dy * static_cast<double>(kFilesWheelRows));
```

Red in `zengine-workshop-panes-tests`:

- Files declares its wheel and its listing's bound as settings, and a layout's values move the
  cursor and stop the walk

## walk-bounded-by-the-constant

Files' walk of a folder is bounded by `kMaxListedEntries` alone, whatever the layout keeps.

In `files/files.cpp`:

```diff
-        listing_ = ws::enumerate_directory(state_.current_dir, listed_entries_);
+        listing_ = ws::enumerate_directory(state_.current_dir);
```

Red in `zengine-workshop-panes-tests`:

- Files declares its wheel and its listing's bound as settings, and a layout's values move the
  cursor and stop the walk

## builds-gate-dropped

Every build status Files hears is taken as a finished build, an answer to its own ask too.

In `files/files.cpp`:

```diff
-        if (first || said.builds == built_before) {
+        if (false && (first || said.builds == built_before)) {
```

Red in `zengine-workshop-panes-tests`:

- the answer the pane asked for does not erase what it just said

## notice-spent-by-say

Files' notice is spent by the publication that says it.

In `files/files.cpp`:

```diff
-            mail, [](const FilesMeaning& m) { return part_name(m); },
-            [](const FilesMeaning&) { return true; });
-    }
+            mail, [](const FilesMeaning& m) { return part_name(m); },
+            [](const FilesMeaning&) { return true; });
+        notice_.clear();
+    }
```

Red in `zengine-workshop-panes-tests`:

- a notice stands until the weaver's next act

## body-budget-from-the-room

Files' body is the room less the header, the notice's row and the strip's left in it.

In `files/browser_view.hpp`:

```diff
-    return rows() - kHeaderRows - (notice_.empty() ? 0 : 1) - strip_rows;
+    (void)strip_rows;
+    return rows() - kHeaderRows;
```

Red in `zengine-workshop-panes-tests`:

- the notice takes its row from the listing, not from the room's end

## paste-asked-elsewhere

Files asks the graphics directory for a paste, not the window showing its area.

In `files/files.cpp`:

```diff
-        const std::string office = room().clipboard_office();
+        const std::string office = "zengine.graphics";
```

Red in `zengine-workshop-panes-tests`:

- the authoring line asks the window that shows Files for a paste, the OS window or the terminal's

## rows-not-spelled

A Files row carries its text as its bytes are.

In `files/files.cpp`:

```diff
-        return fit(ascii_spelling(text), columns());
+        return fit(text, columns());
```

Red in `zengine-workshop-panes-tests`:

- a catalog refusal carrying a non-ASCII byte is still admitted

## centring-window

The listing's window centres the cursor at each picture, rather than moving by the least it can.

In `files/browser_view.hpp`:

```diff
-        total, static_cast<std::size_t>(state_.cursor), window_hint_, body_rows);
+        total, static_cast<std::size_t>(state_.cursor),
+        static_cast<std::size_t>(state_.cursor) > body_rows / 2 ? static_cast<std::size_t>(state_.cursor) - body_rows / 2 : 0,
+        body_rows);
```

Red in `zengine-workshop-panes-tests`:

- two queued presses on the row painted as one entry open THAT entry

## centring-window-unfenced

The listing's window centres the cursor, and a press aimed at moved rows is not refused.

In `files/browser_view.hpp`:

```diff
-        total, static_cast<std::size_t>(state_.cursor), window_hint_, body_rows);
+        total, static_cast<std::size_t>(state_.cursor),
+        static_cast<std::size_t>(state_.cursor) > body_rows / 2 ? static_cast<std::size_t>(state_.cursor) - body_rows / 2 : 0,
+        body_rows);
```

In `files/files.cpp`:

```diff
-        if (at.moved) {
-            notice_ = "the rows moved -- press again";
+        if (false) {
+            notice_ = "the rows moved -- press again";
```

Red in `zengine-workshop-panes-tests`:

- two queued presses on the row painted as one entry open THAT entry

## more-marker-unreserved

`more_marker` says the entries after the window whether or not the window reserved a row for them.

In `component/list_window.hpp`:

```diff
-    return detail::marker_per_cut_side(w) ? w.after : 0;
+    return w.after;
```

Red in `zengine-workshop-panes-tests`:

- a cut the window reserved no row for is not said, and the strip keeps its row

## no-earlier-marker

A pane's list window says nothing of the entries above it.

In `workshop/pane_rows_block.hpp`:

```diff
-        marker(component::earlier_marker(win), "earlier");
+        marker(0, "earlier");
```

Red in `zengine-workshop-panes-tests`:

- scrolled into a long listing, Files says on a row of each side how many entries the window left
  out

## next-field-commits

The `next field` control carries `files.commit-field`, Return's operation.

In `files/recipe_authoring.hpp`:

```diff
-    controls.push_back(ControlRow{files::kActionNextField, "next field",
+    controls.push_back(ControlRow{files::kActionCommitField, "next field",
```

Red in `zengine-workshop-panes-tests`:

- the unavailable `(next field)` control refuses in its own words and writes no recipe

## next-field-without-last

`next_field` has no branch for the last field.

In `files/recipe_authoring.hpp`:

```diff
-    if (authoring_.step + 1 >= field_count()) {
-        notice_ = std::string(field_name(authoring_.chosen.tree, authoring_.chosen.multi_config,
+    if (false) {
+        notice_ = std::string(field_name(authoring_.chosen.tree, authoring_.chosen.multi_config,
```

Red in `zengine-workshop-panes-tests`:

- the unavailable `(next field)` control refuses in its own words and writes no recipe

## fields-from-the-first

`say_authoring` draws the fields from the first, wherever the field being typed into stands.

In `files/recipe_authoring.hpp`:

```diff
-            component::cursor_window(field_count(), authoring_.step, field_hint_,
+            component::cursor_window(field_count(), 0, 0,
```

Red in `zengine-workshop-panes-tests`:

- a short Files pane keeps the authoring field being typed into on the screen, and the menu names
  the rest

## prompt-whole

The active field's prompt is its whole label, whatever the room.

In `files/recipe_authoring.hpp`:

```diff
-    return fitted_label(authoring_.prompt, columns(), kMinFieldValueColumns);
+    return authoring_.prompt;
```

Red in `zengine-workshop-panes-tests`:

- Files keeps typed package-prefix text visible in a thirty-column authoring room

## authoring-menu-on-shift-m

The authoring line's menu row is declared on `Shift+M`.

In `files/files.cpp`:

```diff
-            row(files::kActionMenu, "this pane's menu", input::scan::kUnknown);
+            row(files::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
```

Red in `zengine-workshop-panes-tests`:

- a capital letter typed into the Files authoring line is text, not this pane's menu

## no-take-keys

A Files menu choice that begins authoring asks for no keyboard.

In `files/files.cpp`:

```diff
-        (void)pane_menu::take_keyboard(mail, files::kFilesRole, files::kProjectFilesPane);
+        (void)mail;
```

Red in `zengine-workshop-panes-tests`:

- a Files menu choice that begins authoring takes the keyboard the menu left behind

## menu-label-is-the-name

`field_menu_label` answers each field's name, the long ones too.

In `files/recipe_authoring.hpp`:

```diff
-    return field.menu_label ? field.menu_label : field.name;
+    return field.name;
```

Red in `zengine-workshop-panes-tests`:

- a multi-config tree's fifth field has a menu row short enough to offer, at every field and the
  last one too

## write-judged-for-nobody

On a weaver's host, class `write` is answered for every hand.

In `workshop/actor_scope.hpp`:

```diff
-        if (host.development) return {};
-        return "this is a weaver's host: only the weaver's hand writes a file here, and " + who +
+        if (true) return {};
+        return "this is a weaver's host: only the weaver's hand writes a file here, and " + who +
```

Red in `zengine-workshop-panes-tests`:

- on a weaver's host a guest is refused u, a mark and a written recipe before any write, and still
  lists names

## guests-file-opens

On a weaver's host, class `open` is answered for every hand, the guests file too.

In `workshop/actor_scope.hpp`:

```diff
-        if (host.development || !same_file(subject, host.guests_file)) return {};
+        if (host.development || !same_file(subject, host.guests_file) || true) return {};
```

Red in `zengine-workshop-panes-tests`:

- on a weaver's host a guest opening the guests file through Files is refused, and another file
  opens

## open-sent-at-once

Files sends the open at once, without asking class `open` for the path.

In `files/files.cpp`:

```diff
-        ask_approval(std::move(next), ws::scope::kOpen, path, mail);
+        open_file(next, mail);
```

Red in `zengine-workshop-panes-tests`:

- Files asks class open for the path before it sends the open

## start-on-any-folder

Files offers to start an app on any folder.

In `files/files.cpp`:

```diff
-        if (!marks_.origin.empty() && same(here, fs::path(marks_.origin) / zengine::app::kAppsFolder)) {
+        if (true) {
```

Red in `zengine-workshop-panes-tests`:

- a right press on a folder in the project's apps/ offers to start that app, and the start is asked
  of Workshop and then of the launcher

## places-fold-kept-in-the-pane

Files opens and shuts its places on the press itself and asks the layouts' owner nothing, so the
layout's row never keeps the fold.

In `files/files.cpp`:

```diff
-        if (asks().ask(mail, ws::kLayoutsRole, fold, asking(FilesAsk::Kind::kFold)) !=
-            Asks::Asked::kHeld) {
-            notice_ = places_stay() + " -- the layouts' owner could not be asked";
-        }
+        places_.open = open;
+        (void)fold;
```

Red in `zengine-workshop-panes-tests`:

- Files' places are a section of the pane: shut at first and saying how many they hold, opened and
  shut by their header and by `p` as the pane's own word in the layout's row, and a place pressed is
  gone to
- in a window that reports a resting pointer, Files' shut places peek over the rows under their
  header and take no room, the pointer on what the peek shows keeps it, a press there opens them, a
  move a held press carries is no rest, and the leave ends the peek

## places-offered-once

Files offers its pane once, so a pane sized to fit never follows its places opening.

In `files/files.cpp`:

```diff
-        if (rows == offered_rows_) {
+        if (rows == offered_rows_ || offered_rows_ > 0) {
```

Red in `zengine-workshop-panes-tests`:

- Files' places are a section of the pane: shut at first and saying how many they hold, opened and
  shut by their header and by `p` as the pane's own word in the layout's row, and a place pressed is
  gone to

## places-peek-not-drawn

The pointer resting on the shut places' header shows nothing over the listing.

In `files/browser_view.hpp`:

```diff
     if (fold == component::Fold::kPeek) {
-        *peek_from = row + 1;
+        (void)row;
     }
```

Red in `zengine-workshop-panes-tests`:

- in a window that reports a resting pointer, Files' shut places peek over the rows under their
  header and take no room, the pointer on what the peek shows keeps it, a press there opens them, a
  move a held press carries is no rest, and the leave ends the peek

## path-cut-at-its-end

The header's path is cut at its end, as a sentence is, losing the end that says where.

In `files/browser_view.hpp`:

```diff
-    const std::string shown = room_for > 0 ? fit_path(here, room_for) : std::string();
+    const std::string shown = room_for > 0 ? fit(here, room_for) : std::string();
```

Red in `zengine-workshop-panes-tests`:

- Files' path keeps its root and as much of its end as fits, cut in the middle, and under a resting
  pointer stands whole across the row, scrolled to where the pointer's column puts it, until the
  pointer leaves

## path-never-whole

The pointer resting on the header's path leaves it cut.

In `files/files.cpp`:

```diff
-        if (m != nullptr && (m->kind == files_row::kPath || m->kind == files_row::kPlace)) {
+        if (m != nullptr && m->kind == files_row::kPlace) {
```

Red in `zengine-workshop-panes-tests`:

- Files' path keeps its root and as much of its end as fits, cut in the middle, and under a resting
  pointer stands whole across the row, scrolled to where the pointer's column puts it, until the
  pointer leaves
