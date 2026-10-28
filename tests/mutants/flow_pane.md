# The mutants `tests/test_flow_pane.cpp` claims

Each entry is a mutant that a comment in [`tests/test_flow_pane.cpp`](../test_flow_pane.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## empty-press-keeps-the-step

A press on empty graph room keeps the chosen step chosen.

In `flow/flow-pane/pane.cpp`:

```diff
         if (!hit && (model_.node || model_.filling || model_.dock)) {
-          model_.node.reset();
           model_.filling.reset();
```

Red in `zengine-flow-pane-tests`:

- a press on empty graph room lets go of the chosen step and its port, a drag from there moves the
  view, and a step moves only by a drag that starts on it

## empty-press-keeps-the-port

A press on empty graph room keeps the chosen port chosen.

In `flow/flow-pane/pane.cpp`:

```diff
         if (!hit && (model_.node || model_.filling || model_.dock)) {
           model_.node.reset();
-          model_.filling.reset();
           model_.dock.reset();
```

Red in `zengine-flow-pane-tests`:

- a press on empty graph room lets go of the chosen step and its port, a drag from there moves the
  view, and a step moves only by a drag that starts on it

## the-x-names-the-first-step

The x on the chosen step's corner names the first step, whichever step it stands on.

In `flow/flow-pane/view.hpp`:

```diff
             pen.button(rows.box.right() - xw - (light ? 4 : 0), rows.head - (light ? 3 : 0), "x", "remove",
-                       {std::to_string(n)}, Face::Quiet, Icon::None, canvas);
+                       {"0"}, Face::Quiet, Icon::None, canvas);
```

Red in `zengine-flow-pane-tests`:

- the chosen step goes by the Delete key or by the x on its corner, which the Do lane's head no
  longer holds

## delete-keeps-the-step

The Delete key leaves the chosen step where it is.

In `flow/flow-pane/pane.cpp`:

```diff
     } else if (key.scancode == input::scan::kDelete && model_.node) {
-      perform("remove", {std::to_string(*model_.node)}, mail);
     } else if (key.scancode == input::scan::kReturn) {
```

Red in `zengine-flow-pane-tests`:

- the chosen step goes by the Delete key or by the x on its corner, which the Do lane's head no
  longer holds

## card-drag-places-nothing

A drag from a When or Then card's head puts the card nowhere.

In `flow/flow-pane/pane.cpp`:

```diff
         if (!drag_->card.empty()) {
-          model_.place_dock(drag_->card,
-                            std::clamp(drag_->base_y + dy, std::int64_t{0}, drag_->card_max));
         } else if (drag_->node_id == 0) {
```

Red in `zengine-flow-pane-tests`:

- a When or Then card is chosen by its head and moved up and down its own column, where it stays
  through a save, an open and a reload; a wire let go on the write card's head lands

## write-head-misses-the-wire

A wire let go on the write card's head lands on nothing.

In `flow/flow-pane/pane.cpp`:

```diff
     const bool onto_write = hit != nullptr &&
-                            (hit->action == "write" ||
-                             (hit->action == "card" && !hit->args.empty() &&
-                              hit->args.front().rfind("write:", 0) == 0));
+                            hit->action == "write";
```

Red in `zengine-flow-pane-tests`:

- a When or Then card is chosen by its head and moved up and down its own column, where it stays
  through a save, an open and a reload; a wire let go on the write card's head lands

## write-head-ignores-a-held-answer

A press on the write card's head, with a step's answer held, chooses the card and writes nothing.

In `flow/flow-pane/pane.cpp`:

```diff
       if (chosen.action == "card" && onto_write_with_wire(chosen)) {
-        perform("write", {std::to_string(model_.trigger())}, mail);
-        return;
       }
       if (chosen.action == "card") {
         // A WHEN OR THEN CARD chosen by its head; a drag from there moves it up and down its own
```

Red in `zengine-flow-pane-tests`:

- a When or Then card is chosen by its head and moved up and down its own column, where it stays
  through a save, an open and a reload; a wire let go on the write card's head lands

## a-row-twice-adds-nothing

An operator row of the rail pressed twice at once is only shown, as one press shows it.

In `flow/flow-pane/pane.cpp`:

```diff
       } else {
-        perform("add-found", row.args, mail);
       }
       return;
```

Red in `zengine-flow-pane-tests`:

- a double click adds: an operator row as Add to the rule adds it, empty Do room a step found where
  it was clicked, empty When room a new message, empty Then room keep or say

## empty-room-twice-opens-nothing

Empty Do room pressed twice opens no search there.

In `flow/flow-pane/pane.cpp`:

```diff
     if (target == "column:do") {
-      open_wire_search(pane::WireEnd{}, event.x, event.y, event.x, event.y, it, mail);
+      (void)it, (void)event;
       return;
```

Red in `zengine-flow-pane-tests`:

- a double click adds: an operator row as Add to the rule adds it, empty Do room a step found where
  it was clicked, empty When room a new message, empty Then room keep or say

## when-twice-asks-nothing

Empty When room pressed twice asks for no message.

In `flow/flow-pane/pane.cpp`:

```diff
     if (target == "column:when") {
-      perform("ask-message", {}, mail);
       return;
```

Red in `zengine-flow-pane-tests`:

- a double click adds: an operator row as Add to the rule adds it, empty Do room a step found where
  it was clicked, empty When room a new message, empty Then room keep or say

## then-twice-offers-nothing

Empty Then room pressed twice offers nothing.

In `flow/flow-pane/pane.cpp`:

```diff
     if (target == "column:then")
-      open_menu("then", mail);
+      return;
```

Red in `zengine-flow-pane-tests`:

- a double click adds: an operator row as Add to the rule adds it, empty Do room a step found where
  it was clicked, empty When room a new message, empty Then room keep or say

## port-box-offers-nothing

The box at a chosen port lists nothing in scope of its type.

In `flow/flow-pane/view.hpp`:

```diff
     const auto n = std::to_string(port->node), a = std::to_string(port->port);
-    const auto offers = in_scope(model, *port);
+    const std::vector<InScope> offers;
     const auto kind = port->field.type.kind;
```

Red in `zengine-flow-pane-tests`:

- a press on a port opens a box at it: what is in scope of its type and an earlier step's answer,
  each by its own name, then one value of the port's kind

## port-number-check-inverted

The box at an Int port passes what is not a whole number and marks one that is.

In `flow/flow-pane/view.hpp`:

```diff
     const auto typed = model.port_value.text();
-    const bool wrong = number && !typed.empty() && !whole_number(typed);
+    const bool wrong = number && !typed.empty() && whole_number(typed);
     const auto line = m.light ? m.tall + 4 : m.row;
```

Red in `zengine-flow-pane-tests`:

- a press on a port opens a box at it: what is in scope of its type and an earlier step's answer,
  each by its own name, then one value of the port's kind

## port-yes-says-no

The box at a Bool port binds `false` for its yes.

In `flow/flow-pane/view.hpp`:

```diff
-        const auto yes = pen.button(bx0, y, "yes", "port-set", {n, a, "true"}, Face::Plain, Icon::None, box);
+        const auto yes = pen.button(bx0, y, "yes", "port-set", {n, a, "false"}, Face::Plain, Icon::None, box);
```

Red in `zengine-flow-pane-tests`:

- a press on a port opens a box at it: what is in scope of its type and an earlier step's answer,
  each by its own name, then one value of the port's kind

## editor-covers-the-page

An editor in place is drawn as the dialog that covers the page.

In `flow/flow-pane/view.hpp`:

```diff
-    if (model.dialog && !in_place(model.dialog->action)) {
+    if (model.dialog) {
```

Red in `zengine-flow-pane-tests`:

- Keep a field, Say a message and New message edit in place beside what opened them, the graph
  still in view, and a kept field's kind is chosen from the kinds Flow takes

## editor-ignores-its-opener

An editor in place stands beside what it edits on the graph, never beside the press that opened it.

In `flow/flow-pane/view.hpp`:

```diff
-    if (d.beside) at = Box{d.beside->x, d.beside->y, d.beside->w, d.beside->h};
+    if (d.beside && d.entries.empty()) at = Box{d.beside->x, d.beside->y, d.beside->w, d.beside->h};
```

Red in `zengine-flow-pane-tests`:

- Keep a field, Say a message and New message edit in place beside what opened them, the graph
  still in view, and a kept field's kind is chosen from the kinds Flow takes

## kind-choice-ignored

A kind pressed in a kept field's editor is not held.

In `flow/flow-pane/pane.cpp`:

```diff
-      entry.text.set(args.at(1), args.at(1).size());
+      entry.text.set(entry.text.text(), entry.text.size());
```

Red in `zengine-flow-pane-tests`:

- Keep a field, Say a message and New message edit in place beside what opened them, the graph
  still in view, and a kept field's kind is chosen from the kinds Flow takes

## editor-outlives-a-press-outside

A press outside an editor in place leaves it open.

In `flow/flow-pane/pane.cpp`:

```diff
-        put_editor_down();
+        (void)mail;
         show(mail);
```

Red in `zengine-flow-pane-tests`:

- Keep a field, Say a message and New message edit in place beside what opened them, the graph
  still in view, and a kept field's kind is chosen from the kinds Flow takes
