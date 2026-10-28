# The mutants `tests/test_workshop_panes_input.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_input.cpp`](../test_workshop_panes_input.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## final-escape-removed

Escape's final branch is never taken: a bare Escape asks the desktop for nothing.

In `workshop/weave_handlers.cpp`:

```diff
-    if ((default_row_context(ctx) || (ctx == KeyContext::kPane && !crossed)) &&
+    if (false && (default_row_context(ctx) || (ctx == KeyContext::kPane && !crossed)) &&
```

Red in `zengine-workshop-panes-tests`:

- a focused external pane keeps Escape; a press on a pane that takes no text, then Escape, puts the
  selection down

## next-writes-the-keys

`focus.next` gives the pane it selects the keys too.

In `workshop/weave_input.cpp`:

```diff
-        session_.panes.keyboard = kNoPaneKind;
-        say("selected " + kind_name(session_.panes, *to) + " -- " + hotkey(Act::kFocusEnter) +
+        session_.panes.keyboard = *to;
+        say("selected " + kind_name(session_.panes, *to) + " -- " + hotkey(Act::kFocusEnter) +
```

Red in `zengine-workshop-panes-tests`:

- Tab and the arrows move the keyboard's selection between panes and give none of them the keys

## enter-selects-only

`focus.enter` selects the pane and gives it no keys.

In `workshop/weave_input.cpp`:

```diff
-        session_.panes.keyboard = to;
-        if (keyboard_pane() != to) {
+        session_.panes.keyboard = kNoPaneKind;
+        if (keyboard_pane() != to) {
```

Red in `zengine-workshop-panes-tests`:

- Return gives the selected pane the keys with no press, and a typed key reaches it

## no-leave-arm

Workshop's keys have no `focus.leave` arm.

In `workshop/weave_handlers.cpp`:

```diff
-    case Act::kFocusLeave: {
+    case Act::kEditCode: {
```

Red in `zengine-workshop-panes-tests`:

- ctrl+q gives Workshop the keys back from a pane that keeps Escape, and the pane stays selected

## leave-gives-the-pane-keys-alone

`focus.leave` gives back the pane's keys and leaves the menu, the arrangement and the name editor
open.

In `workshop/weave_handlers.cpp`:

```diff
-        if (session_.presented.open) {
-            withdraw_menu("the weaver left it", mail);
+        if (false && session_.presented.open) {
+            withdraw_menu("the weaver left it", mail);
```

In `workshop/weave_handlers.cpp`:

```diff
-        if (session_.arrange.open) {
-            close_arrange();
-            left = true;
+        if (false && session_.arrange.open) {
+            close_arrange();
+            left = true;
```

In `workshop/weave_handlers.cpp`:

```diff
-        if (session_.setup.naming.open) {
-            close_naming();
-            left = true;
+        if (false && session_.setup.naming.open) {
+            close_naming();
+            left = true;
```

Red in `zengine-workshop-panes-tests`:

- ctrl+q leaves every Workshop mode over a pane, and the pane with them

## leave-in-command-mode

`focus.leave` is declared in command mode rather than above every context.

In `workshop/keymap.hpp`:

```diff
-    {Act::kFocusLeave, "focus.leave", "leave pane", KeyContext::kGlobal, {scan::kQ, mod::kCtrl}},
+    {Act::kFocusLeave, "focus.leave", "leave pane", KeyContext::kCommand, {scan::kQ, mod::kCtrl}},
```

Red in `zengine-workshop-panes-tests`:

- the focus actions move with a weaver's keymap, and focus.leave's chord stands clear of every
  shipped row
