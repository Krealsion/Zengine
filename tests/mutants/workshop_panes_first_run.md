# The mutants `tests/test_workshop_panes_first_run.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_first_run.cpp`](../test_workshop_panes_first_run.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## wiring-from-any-office

Start here takes a view's wiring from any office as a running view's.

In `workshop/first-run/first_run.cpp`:

```diff
-        if (view_offices_.count(std::string(mail.authored_role())) == 0) {
+        if (false) {
```

Red in `zengine-workshop-panes-tests`:

- WL-FIRST-04: each step is ticked by the fact its owner says -- the inventory, a running view's
  wiring, the launcher's listing and its start -- and kept; a view's wiring said by an office no
  running view holds ticks nothing

## fact-asks-nothing

A step's fact ticks the box and asks nothing of the desk.

In `workshop/first-run/first_run.cpp`:

```diff
-        if (story_open_) {
-            launch_story(mail, /*keys=*/false);
+        if (false) {
+            launch_story(mail, /*keys=*/false);
```

Red in `zengine-workshop-panes-tests`:

- WL-FIRST-04: a step ticked while Start here stands behind another pane brings it in front, the
  keys left where they are

## later-start-takes-the-keys

A later start opens Start here holding the keys, as the first start does.

In `workshop/first-run/first_run.cpp`:

```diff
-                state_.story_shown = true;
-                launch_story(mail, /*keys=*/false);
-            }
-            return;
+                state_.story_shown = true;
+                launch_story(mail, /*keys=*/true);
+            }
+            return;
```

Red in `zengine-workshop-panes-tests`:

- WL-FIRST-05: Start here opens when the quick setup closes, holding the keys, and at each later
  start until the story is done beside what holds the keys, never taking them

## hand-tick-kept-over-the-fact

A box the weaver's hand ticked stays the hand's when its owner's fact arrives.

In `workshop/first-run/first_run.cpp`:

```diff
-        if (ticks_[step] == fr::Tick::kFact) {
-            return;
-        }
-        ticks_[step] = fr::Tick::kFact;
+        if (ticks_[step] != fr::Tick::kNone) {
+            return;
+        }
+        ticks_[step] = fr::Tick::kFact;
```

Red in `zengine-workshop-panes-tests`:

- WL-FIRST-04: a box ticked by the weaver's hand says so and is undone by it, its owner's fact makes
  it the owner's, and Show me opens the step's pane and says its next move

## start-before-approval

The quick setup's Start reaches the launcher whatever Workshop answers for the hand.

In `workshop/first-run/first_run.cpp`:

```diff
-        if (!answer.allowed) {
-            moment_.notice = "nothing was started -- " + answer.reason;
+        if (false) {
+            moment_.notice = "nothing was started -- " + answer.reason;
```

Red in `zengine-workshop-panes-tests`:

- WL-FIRST-02: the quick setup lists the setups the launcher lists, and a Start asks Workshop for
  the hand that pressed, then the launcher, saying what it answered

## lines-left-as-written

The quick setup's and Start here's lines are left as wide as they are written.

In `workshop/first-run/setup.hpp`:

```diff
-    if (columns <= 0) {
-        return lines;
-    }
-    std::vector<Line> out;
+    if (columns <= 0 || true) {
+        return lines;
+    }
+    std::vector<Line> out;
```

Red in `zengine-workshop-panes-tests`:

- WL-FIRST-02: every line of the quick setup and of Start here is read whole at the pane's own size,
  a line wider than the pane going on in the rows beneath it
