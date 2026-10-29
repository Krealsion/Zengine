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

## views-never-asked

An editor that names a message asks no running view what its wiring names.

In `flow/flow-pane/pane.cpp`:

```diff
     model_.known.clear();
-    (void)mail.as_role(pane::kRole).publish(zengine::view::ViewWiringRequested{});
+    (void)mail;
```

Red in `zengine-flow-pane-tests`:

- a message's name is offered as it is typed: what the rule says and what running views say or
  show, each view asked by message; a name not among them still works

## view-answer-dropped

What a running view answers is kept nowhere.

In `flow/flow-pane/pane.cpp`:

```diff
     const std::string office(mail.authored_role());
-    if (office.empty() || office == pane::kRole)
+    if (!office.empty() || office == pane::kRole)
       return;
```

Red in `zengine-flow-pane-tests`:

- a message's name is offered as it is typed: what the rule says and what running views say or
  show, each view asked by message; a name not among them still works

## offers-ignore-typing

Every name is offered, whatever is typed.

In `flow/flow-pane/find.hpp`:

```diff
-        if (!typed.empty() && lower(name).find(typed) == std::string::npos) return;
+        if (typed.empty() && lower(name).find(typed) == std::string::npos) return;
```

Red in `zengine-flow-pane-tests`:

- a message's name is offered as it is typed: what the rule says and what running views say or
  show, each view asked by message; a name not among them still works

## say-offers-what-it-hears

A name the rule hears is offered for a message it says.

In `flow/flow-pane/find.hpp`:

```diff
-        if (out.size() >= kMaxSuggestions || among(def.accepts, name) || (!said && among(def.emits, name)))
+        if (out.size() >= kMaxSuggestions || (!said && among(def.accepts, name)) || (!said && among(def.emits, name)))
```

Red in `zengine-flow-pane-tests`:

- a message's name is offered as it is typed: what the rule says and what running views say or
  show, each view asked by message; a name not among them still works

## wired-names-another

The notice of an In scope row chosen names another value than the one it wired.

In `flow/flow-pane/pane.cpp`:

```diff
-          if (candidate.bind == row.args.at(0))
+          if (candidate.bind != row.args.at(0))
```

Red in `zengine-flow-pane-tests`:

- a wire dragged from an input port and let go on empty graph room asks what fills it: in scope, a
  constant, then what yields its type, each made at the hand

## search-cuts-its-heading

A wire's search stands at its old width, so what it says it carries is cut.

In `flow/flow-pane/view.hpp`:

```diff
-    const auto w = std::min(within.w, std::max(m.chars(36), pen.width(carries) + 2 * m.ch));
+    const auto w = std::min(within.w, m.chars(36));
```

Red in `zengine-flow-pane-tests`:

- a wire dragged from an input port and let go on empty graph room asks what fills it: in scope, a
  constant, then what yields its type, each made at the hand

## held-wire-asks-nothing

A wire in hand asks the door for nothing of its own: the rail lists every power.

In `flow/flow-pane/find.hpp`:

```diff
-    else if (const auto held = held_wire(m)) asked.takes = held->type;
+    else if (const auto held = held_wire(m)) asked.offered = !held->type.empty();
```

Red in `zengine-flow-pane-tests`:

- a wire in hand from a step's answer narrows the rail to what takes it, and the preview adds a step
  wired from it at the port chosen

## add-from-wires-the-first-port

A step added from the wire in hand is wired at its first port, whichever was chosen.

In `flow/flow-pane/pane.cpp`:

```diff
-                {flow::binding_text(*model_.connecting), args.at(2)}, mail);
+                {flow::binding_text(*model_.connecting), args.at(2).empty() ? "1" : "0"}, mail);
```

Red in `zengine-flow-pane-tests`:

- a wire in hand from a step's answer narrows the rail to what takes it, and the preview adds a step
  wired from it at the port chosen

## rest-heard-as-nothing

A move with no button held is heard as resting on nothing.

In `flow/flow-pane/pane.cpp`:

```diff
-      on = pane::rest_at(*pictured, event.x, event.y);
+      on = pane::rest_at(*pictured, event.x, -event.y);
```

Red in `zengine-flow-pane-tests`:

- the pointer resting with no button held lights the sentence and the graph together, and a press on
  a part still to choose does what the Next line's button does

## a-part-to-choose-does-nothing

A press on a part of the sentence still to choose does nothing.

In `flow/flow-pane/pane.cpp`:

```diff
-      if (i >= chips.size() || (chips[i].kind != pane::Chip::Kind::Open &&
+      if (i < chips.size() || (chips[i].kind != pane::Chip::Kind::Open &&
```

Red in `zengine-flow-pane-tests`:

- the pointer resting with no button held lights the sentence and the graph together, and a press on
  a part still to choose does what the Next line's button does
