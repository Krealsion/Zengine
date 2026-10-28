# The mutants `tests/test_workshop_panes_attention.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_attention.cpp`](../test_workshop_panes_attention.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## detail-drawn-whatever-the-setting

The cursor's explanation is drawn whatever `detail` says.

In `attention/pane.cpp`:

```diff
-            detail_shown_ ? wrap(at.detail, columns - kWrapIndent) : std::vector<std::string>{};
+            wrap(at.detail, columns - kWrapIndent);
```

Red in `zengine-workshop-panes-tests`:

- the Attention pane declares its `detail` setting, and a layout keeping it off lists each condition
  without the cursor's explanation

## press-moves-no-cursor

A press on a condition's row asks nothing of the pane: the cursor stays where it was.

In `attention/pane.cpp`:

```diff
-                    cursor_ = i;
+                    (void)i;
```

Red in `zengine-workshop-panes-tests`:

- a press on a condition's row in the Attention pane puts the cursor there, and a press where its
  picture names nothing moves nothing

## offer-keeps-conditions-said

An offer leaves the conditions' last reading standing as said.

In `workshop/weave_seam.cpp`:

```diff
-    conditions_said_ = false;
```

Red in `zengine-workshop-panes-tests`:

- a pane that arrives after the host has spoken is told again

## glance-counts-the-listed

The glance counts the conditions the list shows, the hidden ones left out.

In `attention/pane.cpp`:

```diff
-        push(ws::attention_glance(known_), known_.front().role);
+        push(ws::attention_glance(visible()), known_.front().role);
```

Red in `zengine-workshop-panes-tests`:

- the Attention pane leads with the glance: the loudest condition that is true and how many more,
  hidden ones counted

## list-floor-dropped

The list has no floor of three rows: two rows seat a condition beside a count.

In `attention/pane.cpp`:

```diff
-        if (list_rows < 3 && shown.size() > list_rows) {
+        if (false && list_rows < 3 && shown.size() > list_rows) {
```

Red in `zengine-workshop-panes-tests`:

- an Attention pane too short to seat a condition between two markers says only how many it leaves
  out

## resolved-dismissals-kept

A dismissal outlives the condition it was about.

In `attention/pane.cpp`:

```diff
-        forget_resolved();
-        const std::size_t total = visible().size();
+        const std::size_t total = visible().size();
```

Red in `zengine-workshop-panes-tests`:

- a dismissal does not outlive the condition it was about

## rows-drawn-as-they-are

The pane draws each row's words as their bytes are.

In `attention/pane.cpp`:

```diff
-            f.text(path, 0, row * height, drawable(fit(text, columns)), gfx::by_role(role));
+            f.text(path, 0, row * height, fit(text, columns), gfx::by_role(role));
```

Red in `zengine-workshop-panes-tests`:

- a condition carrying a byte a canvas cannot draw is still shown
