# The mutants `tests/test_flow_graph.cpp` claims

Each entry is a mutant that a comment in [`tests/test_flow_graph.cpp`](../test_flow_graph.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## card-places-unwritten

A saved workspace leaves out where each When and Then card was put.

In `flow/workspace.hpp`:

```diff
     file.pan_x = workspace.pan_x; file.pan_y = workspace.pan_y; file.zoom = workspace.zoom;
-    file.docks = workspace.docks;
     auto bytes = loom::serialize(loom::to_value(file));
```

Red in `zengine-flow-tests`:

- a workspace keeps where each When and Then card was put, reads one version 1 wrote with none
  put, and refuses a card put twice

## version-one-refused

A workspace version 1 wrote is refused rather than read with no card put.

In `flow/workspace.hpp`:

```diff
-    if (claim.claimed_version() == v1::WorkspaceFile::zen_version) {
+    if (false) {
```

Red in `zengine-flow-tests`:

- a workspace keeps where each When and Then card was put, reads one version 1 wrote with none
  put, and refuses a card put twice

## delete-refuses-a-read-step

A step another step reads is refused a delete, as before.

In `flow/graph_edit.hpp`:

```diff
-          b = op::Binding::input("");
+          throw std::invalid_argument("another node uses this output; reconnect it before deleting");
```

Red in `zengine-flow-tests`:

- a delete takes every connection with it, no questions asked: the ports that read the step are
  left open, and deleting the answer leaves the rule with none, which a save keeps

## delete-refuses-the-answer

The answer is refused a delete while other steps stand, as before.

In `flow/graph_edit.hpp`:

```diff
-    const bool answer = body.result_node == n;
+    const bool answer = body.result_node == n;
+    if (answer && body.nodes.size() > 1)
+      throw std::invalid_argument("choose another result before deleting this node");
```

Red in `zengine-flow-tests`:

- a delete takes every connection with it, no questions asked: the ports that read the step are
  left open, and deleting the answer leaves the rule with none, which a save keeps

## deleted-answer-moves-on

Deleting the answer makes the step before it the answer.

In `flow/graph_edit.hpp`:

```diff
-    if (answer || body.result_node > body.nodes.size())
+    if ((answer && false) || body.result_node > body.nodes.size())
```

Red in `zengine-flow-tests`:

- a delete takes every connection with it, no questions asked: the ports that read the step are
  left open, and deleting the answer leaves the rule with none, which a save keeps

## unanswered-unsaved

A save forgets that a rule has no answer: it opens with its first step as the answer.

In `flow/workspace.hpp`:

```diff
-            file.unanswered.push_back(static_cast<std::int64_t>(i));
+            (void)i;
```

Red in `zengine-flow-tests`:

- a delete takes every connection with it, no questions asked: the ports that read the step are
  left open, and deleting the answer leaves the rule with none, which a save keeps

## foreign-message-renamed

A message declared elsewhere is renamed, so its sender's no longer wakes the rule.

In `flow/graph_edit.hpp`:

```diff
-    if (!owns(shape))
+    if (!owns(shape) && false)
```

Red in `zengine-flow-tests`:

- the edits a right press offers each take what they break with them: a message or a field renamed,
  retyped or removed, a trigger taken out, a kept field renamed, retyped, removed or started, a step
  or a port disconnected, another field written, what a say carries

## renamed-field-unread

A message field renamed leaves the ports that read it on the old name.

In `flow/graph_edit.hpp`:

```diff
-    rewire_message_reads(*shape, from, op::Binding::input(to));
+    (void)to;
```

Red in `zengine-flow-tests`:

- the edits a right press offers each take what they break with them: a message or a field renamed,
  retyped or removed, a trigger taken out, a kept field renamed, retyped, removed or started, a step
  or a port disconnected, another field written, what a say carries

## renamed-kept-unwritten

A kept field renamed leaves the trigger writing the old name.

In `flow/graph_edit.hpp`:

```diff
-      if (on.output == from)
+      if (on.output == from && false)
```

Red in `zengine-flow-tests`:

- the edits a right press offers each take what they break with them: a message or a field renamed,
  retyped or removed, a trigger taken out, a kept field renamed, retyped, removed or started, a step
  or a port disconnected, another field written, what a say carries

## removed-kept-still-said

A kept field removed leaves the say that carried it.

In `flow/graph_edit.hpp`:

```diff
     rekeep(std::move(fields), [](const std::string &n) { return n; });
     for (auto &on : project.definition.on)
       reads_of(on, name, op::Binding::input(""));
-    prune_emits();
```

Red in `zengine-flow-tests`:

- the edits a right press offers each take what they break with them: a message or a field renamed,
  retyped or removed, a trigger taken out, a kept field renamed, retyped, removed or started, a step
  or a port disconnected, another field written, what a say carries

## disconnect-keeps-readers

Disconnecting a step leaves the ports that read it wired.

In `flow/graph_edit.hpp`:

```diff
     for (auto &b : body.nodes.at(n).arguments)
       b = op::Binding::input("");
     for (auto &node : body.nodes)
       for (auto &b : node.arguments)
-        if (b.from() == op::Binding::From::Node && b.node_index() == n)
+        if (b.from() == op::Binding::From::Node && b.node_index() == n && false)
```

Red in `zengine-flow-tests`:

- the edits a right press offers each take what they break with them: a message or a field renamed,
  retyped or removed, a trigger taken out, a kept field renamed, retyped, removed or started, a step
  or a port disconnected, another field written, what a say carries
