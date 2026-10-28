# The mutants `tests/test_settings_themes.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_settings_themes.cpp`](../test_settings_themes.cpp) claims its cases catch, the comment
naming the entry by its path and heading. [The corpus's front page](README.md) says how an entry
reads and how `tests/mutants/run.py` runs it.

## name-takes-dot-and-slash

A theme's name takes `.` and `/` beside its letters, digits and dashes.

In `settings/theme_persist.hpp`:

```diff
-        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
+        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '/')) {
```

Red in `zengine-settings-themes-tests`:

- a theme's name is 1 to 32 bytes of lowercase letters, digits and dashes

## office-without-dot

A target's office is taken without a `.` in it.

In `settings/theme_persist.hpp`:

```diff
-    const bool office_ok = !office.empty() && office.find('.') != std::string_view::npos;
+    const bool office_ok = !office.empty();
```

Red in `zengine-settings-themes-tests`:

- a theme setting's target is every pane, an office, or one pane of an office

## pane-with-slash

A target's pane is taken with a second `/` in it.

In `settings/theme_persist.hpp`:

```diff
-        pane_ok = !pane.empty() && pane != "*" && pane.find('/') == std::string_view::npos;
+        pane_ok = !pane.empty() && pane != "*";
```

Red in `zengine-settings-themes-tests`:

- a theme setting's target is every pane, an office, or one pane of an office

## pane-of-star

A target's pane is taken as `*`.

In `settings/theme_persist.hpp`:

```diff
-        pane_ok = !pane.empty() && pane != "*" && pane.find('/') == std::string_view::npos;
+        pane_ok = !pane.empty() && pane.find('/') == std::string_view::npos;
```

Red in `zengine-settings-themes-tests`:

- a theme setting's target is every pane, an office, or one pane of an office

## clear-with-value

A clear that holds a value is admitted.

In `settings/theme_persist.hpp`:

```diff
-        if (kind != workshop::SettingKind::kNone) {
+        if (false) {
```

Red in `zengine-settings-themes-tests`:

- a theme setting holds exactly one value, or is a clear holding none

## setting-twice

A theme file's settings are not scanned for one target and key set twice.

In `settings/theme_persist.hpp`:

```diff
-        if (places[i - 1] == places[i]) {
+        if (false && places[i - 1] == places[i]) {
```

Red in `zengine-settings-themes-tests`:

- a theme file sets nothing twice, includes nothing twice or itself, and keeps its bounds

## envelope-version-unread

A theme file's version is left to admission, unread in its envelope.

In `settings/theme_persist.hpp`:

```diff
-    if (claim.claimed_version() != ThemeFile::zen_version) {
+    if (false) {
```

Red in `zengine-settings-themes-tests`:

- a theme file is refused by what its envelope says it is before its fields are judged

## read-without-ceiling

A theme file is read from disk with a ceiling a thousand times its own.

In `settings/theme_persist.hpp`:

```diff
-        workshop::persist::read_file(path, kMaxThemeBytes, "a theme file");
+        workshop::persist::read_file(path, kMaxThemeBytes * 1000, "a theme file");
```

Red in `zengine-settings-themes-tests`:

- a theme file past its ceiling is refused before it is parsed, read or written

## stale-write-unguarded

A replacing write is made whatever the file on disk holds now.

In `settings/theme_persist.hpp`:

```diff
-        if (!current.outcome.accepted || current.text != *expected_bytes) {
+        if (false && (!current.outcome.accepted || current.text != *expected_bytes)) {
```

Red in `zengine-settings-themes-tests`:

- a theme is written new, and over itself only when asked and only while it holds what was read

## fingerprint-takes-giver

A resolution's fingerprint mixes in the theme each setting came from.

In `settings/theme_persist.hpp`:

```diff
-        mix(s.target);
+        mix(r.from_theme);
+        mix(s.target);
```

Red in `zengine-settings-themes-tests`:

- a theme alone resolves to its own entries, with a fingerprint that is never zero

## clear-dropped

A resolution passes over each clear a theme holds.

In `settings/theme_persist.hpp`:

```diff
-        for (const ThemeSetting& s : loaded.theme.settings) {
+        for (const ThemeSetting& s : loaded.theme.settings) {
+            if (s.clear) continue;
```

Red in `zengine-settings-themes-tests`:

- a theme's includes apply first and in order, and later wins, a clear among them

## cycle-unseen

The walk never looks for the theme it is asked for on the path it is walking.

In `settings/theme_persist.hpp`:

```diff
-        if (const auto open = std::find(path.begin(), path.end(), name); open != path.end()) {
+        if (const auto open = path.end(); open != path.end()) {
```

Red in `zengine-settings-themes-tests`:

- a cycle refuses the whole resolution, naming its path

## themes-read-uncounted

The walk reads a thousand themes more than its bound before it refuses.

In `settings/theme_persist.hpp`:

```diff
-        if (++visited > kMaxThemesResolved) {
+        if (++visited > kMaxThemesResolved + 1000) {
```

Red in `zengine-settings-themes-tests`:

- a resolution past its depth, its themes read or its entries is refused whole, naming the bound

## broken-file-unlisted

A theme file that does not load is left out of its folder's listing.

In `settings/theme_persist.hpp`:

```diff
-            out.push_back(ThemeSummary{stem, std::string(), {}, 0, 0, loaded.outcome.refusal});
```

Red in `zengine-settings-themes-tests`:

- a folder's themes are listed by name, a broken file with its refusal
