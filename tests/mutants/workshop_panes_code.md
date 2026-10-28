# The mutants `tests/test_workshop_panes_code.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_code.cpp`](../test_workshop_panes_code.cpp) claims its cases catch, the
comment naming the entry by its path and heading. [The corpus's front page](README.md) says how an
entry reads and how `tests/mutants/run.py` runs it.

## edit-code-unjudged

Edit Code opens a recipe's source without judging the hand for class `open`.

In `workshop/weave_code.cpp`:

```diff
-    if (const std::string judged = judge_classes(input_actor_, {scope::kOpen}, recipe.source);
-        !judged.empty()) {
+    if (const std::string judged = judge_classes(input_actor_, {scope::kOpen}, recipe.source);
+        false && !judged.empty()) {
```

Red in `zengine-workshop-panes-tests`:

- Edit Code asks class open for the guest's hand: a recipe whose source is the guests file is
  refused on a weaver's host
