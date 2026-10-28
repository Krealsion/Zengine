# The mutants `tests/test_workshop_panes_builder.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_builder.cpp`](../test_workshop_panes_builder.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## load-built-sends-the-choice

Loading what was built asks for the recipe the weaver has chosen since.

In `builder/builder-pane/pane.cpp`:

```diff
-        a.act = Act::kLoadBuilt;
-        a.subject = shown_.recipe;
+        a.act = Act::kLoadBuilt;
+        a.subject = known_.recipes[cursor_row()].recipe;
```

Red in `zengine-workshop-panes-tests`:

- the control that loads what was built names the BUILT recipe, not the choice

## arm-only-unguarded

Arming the next build is taken while an artifact stands built and unoffered.

In `builder/builder-pane/pane.cpp`:

```diff
-    void arm_only(loom::Mail& mail) {
-        if (ready_to_load()) {
+    void arm_only(loom::Mail& mail) {
+        if (false) {
```

Red in `zengine-workshop-panes-tests`:

- the control that loads what was built names the BUILT recipe, not the choice

## load-row-advertises-nothing

The menu's load row records no subject it promises to load.

In `builder/builder-pane/view.hpp`:

```diff
-                ready_to_load() ? shown_.artifact : std::string(),
+                std::string(),
```

Red in `zengine-workshop-panes-tests`:

- an open menu row naming an artifact loads THAT artifact or refuses

## target-op-always-zero

`target_op_of` answers 0 for every control: a held choice is matched by its name alone.

In `builder/builder-pane/pane.cpp`:

```diff
-    std::int64_t target_op_of(const std::string& id) const {
+    std::int64_t target_op_of(const std::string& id) const {
+        (void)id;
+        return 0;
```

Red in `zengine-workshop-panes-tests`:

- a held load menu cannot switch recipes sharing an artifact stem

## perform-on-without-the-op

`perform_on` compares a held choice's name and not its operation.

In `builder/builder-pane/pane.cpp`:

```diff
-        if (advertised_op != 0 && target_op_of(id) != advertised_op) {
+        if (false && advertised_op != 0 && target_op_of(id) != advertised_op) {
```

Red in `zengine-workshop-panes-tests`:

- a held promote or revert menu row cannot switch images sharing an artifact stem

## list-has-no-edit-source

The recipe list's dispatch has no `edit source` branch.

In `builder/builder-pane/modes.hpp`:

```diff
-    } else if (id == pane::kActionEditSource) {
-        // The list's own row, acting on the list's cursor: `edit_source` reads `list_row()`
+    } else if (false) {
+        // The list's own row, acting on the list's cursor: `edit_source` reads `list_row()`
```

Red in `zengine-workshop-panes-tests`:

- `edit source` in the recipe list opens the row the list is standing on, and leaves the choice
  alone

## edit-source-reads-the-choice

`edit_source` opens the chosen recipe's source while the list is open.

In `builder/builder-pane/pane.cpp`:

```diff
-        const std::size_t at = choosing_.open ? list_row() : cursor_row();
-        if (at >= known_.recipes.size()) {
-            notice_ = "no recipe is under the cursor -- nothing was opened";
+        const std::size_t at = cursor_row();
+        if (at >= known_.recipes.size()) {
+            notice_ = "no recipe is under the cursor -- nothing was opened";
```

Red in `zengine-workshop-panes-tests`:

- `edit source` in the recipe list opens the row the list is standing on, and leaves the choice
  alone

## role-line-menu-on-shift-m

The role line's menu row is declared on `Shift+M`.

In `builder/builder-pane/pane.cpp`:

```diff
-            row(pane::kActionMenu, "this pane's menu", input::scan::kUnknown);
+            row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
```

Red in `zengine-workshop-panes-tests`:

- a capital letter typed into the Builder's role line is text, not this pane's menu

## names-choice-never-set

The ask that opens the role line never carries the menu choice's number.

In `builder/builder-pane/modes.hpp`:

```diff
-    names.choice = choice_; // nonzero only when a menu row began this
+    names.choice = 0;
```

Red in `zengine-workshop-panes-tests`:

- a Builder menu choice that opens the role line takes the keyboard across the door it waits on

## keys-taken-before-the-line

A menu choice takes the keys at once, before the role line it begins exists.

In `builder/builder-pane/view.hpp`:

```diff
-            choice_ = mail.correlation();
-            perform_on(row.action, offered.advertised, offered.op, mail);
+            choice_ = mail.correlation();
+            take_keys(mail, choice_);
+            perform_on(row.action, offered.advertised, offered.op, mail);
```

Red in `zengine-workshop-panes-tests`:

- a Builder menu choice that opens the role line takes the keyboard across the door it waits on

## reader-menu-close-alone

The output reader's menu offers `close` alone.

In `builder/builder-pane/view.hpp`:

```diff
-        say_row(pane::kMenuOutputUp, "a line up");
-        say_row(pane::kMenuOutputDown, "a line down");
-        say_row(pane::kMenuOutputFirst, "the first line");
-        say_row(pane::kMenuOutputLast, "the last lines");
-        say_row(pane::kMenuOutputLeft, "pan left");
-        say_row(pane::kMenuOutputRight, "pan right");
-        say_row(pane::kMenuOutputOlder, "the older build's output");
-        say_row(pane::kMenuOutputNewer, "the newer build's output");
```

Red in `zengine-workshop-panes-tests`:

- a reader waiting on its first page still offers its whole list in a narrow room

## paste-asked-of-another-office

The Builder's role line asks the graphics directory for a paste, not the window showing its area.

In `builder/builder-pane/pane.cpp`:

```diff
-        const std::string office = room().clipboard_office();
+        const std::string office = "zengine.graphics";
```

Red in `zengine-workshop-panes-tests`:

- the Builder's role line asks the window that shows the pane for a paste, the OS window or the
  terminal's
