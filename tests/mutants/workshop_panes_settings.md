# The mutants `tests/test_workshop_panes_settings.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_settings.cpp`](../test_workshop_panes_settings.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## hand-off-as-workshop

The layouts' owner hands a pane its row as Workshop's office.

In `workshop/layouts/owner.cpp`:

```diff
-                ? mail.as_role(kLayoutsRole).send(owed.holder, owed.value)
-                : mail.as_role(kLayoutsRole)
+                ? mail.as_role(kWorkshopProvider).send(owed.holder, owed.value)
+                : mail.as_role(kWorkshopProvider)
```

Red in `zengine-workshop-panes-tests`:

- the layouts' owner hands a seated pane its row's settings, whether or not it declared

## record-of-the-holder-ignored

The owner hands every row at every settle, whatever its holder holds.

In `workshop/layouts/store.cpp`:

```diff
-        if (was != nullptr && was->holder == taker->first && was->revision == revision &&
+        if (false && was != nullptr && was->holder == taker->first && was->revision == revision &&
```

Red in `zengine-workshop-panes-tests`:

- a layout switch hands a pane its new layout's settings once, and only when they differ

## ask-keeps-the-record

A pane's ask for its row leaves the owner's record of what it holds.

In `workshop/layouts/store.cpp`:

```diff
-    if (at != handed_.end()) {
-        effects_.unused_moved = effects_.unused_moved || !at->unused.empty();
-        handed_.erase(at);
+    if (false && at != handed_.end()) {
+        effects_.unused_moved = effects_.unused_moved || !at->unused.empty();
+        handed_.erase(at);
```

Red in `zengine-workshop-panes-tests`:

- a pane that asks for its row and a reopened row are handed their settings again

## revision-without-the-declaration

A row's revision is minted without its counted declaration.

In `workshop/layouts/store.cpp`:

```diff
-        if (kept.desk != now.desk || kept.settings != now.settings || kept.rows != now.rows) {
+        if (kept.desk != now.desk || kept.settings != now.settings) {
```

Red in `zengine-workshop-panes-tests`:

- the row Info reads moves with what it is built from: a declaration, and a new holder

## unseat-names-no-lock

The store's unseat names no lock of the row it takes.

In `workshop/layouts/store.cpp`:

```diff
-                out.unlocked.push_back(lock.key);
```

Red in `zengine-workshop-panes-tests`:

- arrangement's remove says the settings and locks it discarded too

## word-of-another-revision-kept

The owner keeps a pane's word naming a revision its holder was not last handed.

In `workshop/layouts/store.cpp`:

```diff
-    if (unused.revision <= 0 || unused.revision != at->revision ||
+    if (unused.revision <= 0 ||
```

Red in `zengine-workshop-panes-tests`:

- a pane's word that it cannot use a setting is kept against its revision by the layouts' owner,
  said, and goes when a newer one is handed

## side-columns-still-declared

Workshop's declaration still lists `workshop.side-columns`.

In `workshop/prefs_persist.hpp`:

```diff
-            number(kStackRowsKey, kStackRows, kMinStackRows, kMaxStackRows, {"layout"},
+            number("workshop.side-columns", 28, 16, 64, {"layout"}, "right-column width in cells"),
+            number(kStackRowsKey, kStackRows, kMinStackRows, kMaxStackRows, {"layout"},
```

Red in `zengine-workshop-panes-tests`:

- Workshop's measures and its completion order apply when committed, are kept in the settings file,
  and a value outside what each takes is refused

## id-read-as-place

A layout's id is read as its place in the run.

In `workshop/layouts/run.hpp`:

```diff
-        if (at != s.active_at && s.shelved[shelf_index(s, at)].id == id) {
+        if (at != s.active_at && static_cast<std::int64_t>(at) + 1 == id) {
```

Red in `zengine-workshop-panes-tests`:

- a layout is named by its id: a lock by id lands on it after it moved, and two layouts of one name
  keep their own
