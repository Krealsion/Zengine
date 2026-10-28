# The mutants `tests/test_workshop_panes_terminal.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_terminal.cpp`](../test_workshop_panes_terminal.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## draws-before-its-row

The Terminal draws its room before it holds its row.

In `terminal/view.hpp`:

```diff
-    if (!room().granted() || !this->template facet<Settings>().held()) {
+    if (!room().granted()) {
```

Red in `zengine-workshop-panes-tests`:

- the Terminal draws nothing in a window's room before it holds its row, takes a row from the
  layouts' owner alone, and draws whole once it holds one

## row-from-any-office

A pane takes the row any office hands it as its own.

In `workshop/pane_settings_taken.hpp`:

```diff
-        if (!from_owner(mail) || handed.pane != Pane) {
+        if (handed.pane != Pane) {
```

Red in `zengine-workshop-panes-tests`:

- the Terminal draws nothing in a window's room before it holds its row, takes a row from the
  layouts' owner alone, and draws whole once it holds one

## offer-says-nothing-again

An offer leaves the Terminal door's record of what it said standing.

In `workshop/weave_seam.cpp`:

```diff
-    facet<TerminalDoorFacet>().say_again();
```

Red in `zengine-workshop-panes-tests`:

- a pane that loaded after the last publication still hears the reading

## door-ignores-the-order

The Terminal's door asks for completions in the shape's order whatever the session says.

In `terminal/door.hpp`:

```diff
-                                           self.session().completion_required_first
+                                           (self.session().completion_required_first && false)
```

Red in `zengine-workshop-panes-tests`:

- `workshop.completion-order` puts a shape's required fields first in the Terminal's list, so
  accepting the first offer names what the message needs
