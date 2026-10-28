# The mutants `tests/test_workshop_panes_button.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_button.cpp`](../test_workshop_panes_button.cpp) claims its cases catch,
the comment naming the entry by its path and heading. [The corpus's front page](README.md) says how
an entry reads and how `tests/mutants/run.py` runs it.

## menu-left-over-another-desk

Another desk going live leaves the menu open over the desk the weaver was on.

In `workshop/weave_session.cpp`:

```diff
-    if (live.id != d.id || static_cast<std::uint64_t>(live.put_live) != d.put_live) {
+    if (false) {
```

Red in `zengine-workshop-panes-tests`:

- WL-CTX-09: another desk going live withdraws a pane's open menu, answered unchosen, and closes the
  host's own; an edit of the same desk leaves either open

## keyboard-grab-spent-twice

A chosen row's keyboard grab is taken again once it was spent.

In `workshop/weave_external.cpp`:

```diff
     // chosen row begins an edit asks for them here.
     ChoiceAnswered* choice = nullptr;
     each_hand([&](Hand& h) {
-        if (!h.choice_answered.spent && h.choice_answered.kind == row->kind &&
-            h.choice_answered.correlation == mail.correlation() && h.choice_answered.gesture == h.latest)
+        if (h.choice_answered.kind == row->kind &&
+            h.choice_answered.correlation == mail.correlation() && h.choice_answered.gesture == h.latest)
```

Red in `zengine-workshop-panes-tests`:

- WL-CTX-09: a chosen row that begins an edit may take the keyboard -- once, while the choice is the
  weaver's latest act, and never under another number

## keyboard-grab-any-number

A keyboard grab is taken under any number, the weaver's latest choice or not.

In `workshop/weave_external.cpp`:

```diff
     // chosen row begins an edit asks for them here.
     ChoiceAnswered* choice = nullptr;
     each_hand([&](Hand& h) {
-        if (!h.choice_answered.spent && h.choice_answered.kind == row->kind &&
-            h.choice_answered.correlation == mail.correlation() && h.choice_answered.gesture == h.latest)
+        if (!h.choice_answered.spent && h.choice_answered.kind == row->kind &&
+            h.choice_answered.gesture == h.latest)
```

Red in `zengine-workshop-panes-tests`:

- WL-CTX-09: a chosen row that begins an edit may take the keyboard -- once, while the choice is the
  weaver's latest act, and never under another number
