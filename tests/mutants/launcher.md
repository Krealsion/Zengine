# The mutants `tests/test_launcher.cpp` claims

Each entry is a mutant that a comment in [`tests/test_launcher.cpp`](../test_launcher.cpp) claims
its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## any-folder-a-name-reaches

The launcher takes an app folder wherever its name leads, a link out of `apps/` too.

In `launcher/catalog.hpp`:

```diff
-        if (ec || real.parent_path() != real_apps) {
+        if (ec) {
```

Red in `zengine-launcher-tests`:

- an app is a folder directly in the project's apps/ holding a plan the codec reads; a link out of
  it and a folder with no plan are refused

## package-start-names-a-tree

A setup's start names a build tree and a Loom prefix, in a package too.

In `launcher/catalog.hpp`:

```diff
-        catalog.package.empty() ? std::vector<std::string>{"--build", catalog.build,
+        true ? std::vector<std::string>{"--build", catalog.build,
```

Red in `zengine-launcher-tests`:

- a package's setup is started by a Python found on the PATH running the package's own demo service,
  against the package

## no-python-lists-no-setup

With no Python found, the listing names no setup.

In `launcher/catalog.hpp`:

```diff
-    for (const DemoSetup& d : catalog.demos) {
-        add(LaunchEntry{kKindDemo, d.name, d.title.empty() ? d.name : d.title, d.line, false});
+    for (const DemoSetup& d : catalog.python.empty() ? std::vector<DemoSetup>{} : catalog.demos) {
+        add(LaunchEntry{kKindDemo, d.name, d.title.empty() ? d.name : d.title, d.line, false});
```

Red in `zengine-launcher-tests`:

- with no Python on the PATH a package lists its setups, and the listing and a start say what a
  setup needs

## answer-before-exec

A start answers once the process is forked, before its program is executed.

In `launcher/start.hpp`:

```diff
-    const ssize_t said = got == static_cast<ssize_t>(sizeof(id)) ? ::read(back[0], &failed, sizeof(failed)) : 0;
+    const ssize_t said = 0;
```

Red in `zengine-launcher-tests`:

- an app starts detached as the process the launcher names: zengine-app on its folder, kept by
  nobody

## nothing-closed

A POSIX start closes none of the starter's descriptors before it executes the program.

In `launcher/start.hpp`:

```diff
-        detail::close_all_but(back[1], open_max);
+        (void)open_max;
```

Red in `zengine-launcher-tests`:

- a POSIX start hands on its three standard streams and nothing else of the starter's

## every-folder-listed

The listing names every folder in `apps/`, one the launcher would refuse too.

In `launcher/catalog.hpp`:

```diff
-        if (judged.ok) {
-            add(LaunchEntry{kKindApp, name, name, "an app in the project's apps/",
+        if (true) {
+            add(LaunchEntry{kKindApp, name, name, "an app in the project's apps/",
```

Red in `zengine-launcher-tests`:

- a listing names each setup the demo service lists with its own title and line, then each app the
  launcher would start, and no folder it would refuse

## running-from-the-file-alone

An app is listed running whenever its session file names a process.

In `launcher/catalog.hpp`:

```diff
-           runs_program(*pid, std::filesystem::path(catalog.app_program).filename().string());
+           (true || runs_program(*pid, std::filesystem::path(catalog.app_program).filename().string()));
```

Red in `zengine-launcher-tests`:

- an app is listed running only while its session file stands and names a process that still runs
  the app host

## askers-as-before

The launcher takes a start from the View Builder and Files alone.

In `launcher/weave.cpp`:

```diff
-constexpr const char* kAskers[] = {"zengine.view.builder", "zengine.files", "zengine.palette",
-                                   "zengine.first-run"};
+constexpr const char* kAskers[] = {"zengine.view.builder", "zengine.files"};
```

Red in `zengine-launcher-tests`:

- the launcher answers a listing to any office, and takes a start from the palette and the quick
  setup as from the View Builder and Files

## start-not-published

A start is answered to its asker and published to nobody else.

In `launcher/weave.cpp`:

```diff
-        if (answer.ok) {
-            (void)mail.as_role(launcher::kLauncherRole).publish(answer);
+        if (false) {
+            (void)mail.as_role(launcher::kLauncherRole).publish(answer);
```

Red in `zengine-launcher-tests`:

- each start is said to any as it is answered, as the launcher's office, so an office that did not
  ask hears it; a refusal is answered to its asker alone
