# The mutants `tests/test_flow_view.cpp` claims

Each entry is a mutant that a comment in [`tests/test_flow_view.cpp`](../test_flow_view.cpp)
claims its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## doubles-at-any-time

Two presses double however far apart in time.

In `flow/flow-pane/gesture.hpp`:

```diff
-    return now_ms >= prior.at_ms && now_ms - prior.at_ms <= kDoublePressMs;
+    return now_ms >= prior.at_ms;
```

Red in `zengine-flow-tests`:

- two presses are one double click on one target, near where the first landed, within the interval,
  and time is its argument

## doubles-on-any-target

A press doubles the one before it whatever each landed on.

In `flow/flow-pane/gesture.hpp`:

```diff
-    if (!prior.armed || prior.target.empty() || prior.target != target) return false;
+    if (!prior.armed || prior.target.empty() || target.empty()) return false;
```

Red in `zengine-flow-tests`:

- two presses are one double click on one target, near where the first landed, within the interval,
  and time is its argument

## whole-number-takes-a-start

A number's start is read as the whole of the text, so `12x` is a whole number.

In `flow/flow-pane/find.hpp`:

```diff
     const auto read = std::from_chars(text.data(), text.data() + text.size(), value);
-    return !text.empty() && read.ec == std::errc{} && read.ptr == text.data() + text.size();
+    return !text.empty() && read.ec == std::errc{};
```

Red in `zengine-flow-tests`:

- a step is named by its operator, and which one where two share it; a whole number is the whole
  of the text

## place-carries-a-grazed-row

A part carries every run of words it meets, a run whose row it only grazes too.

In `flow/flow-pane/look.hpp`:

```diff
         if (!k.bounds().meets(place)) return false;
-        return k.op != Mark::Op::Text || (k.y >= place.y && k.y + k.h <= place.bottom());
+        return k.op != Mark::Op::Text || k.y + k.h <= place.bottom() + k.h;
```

Red in `zengine-flow-tests`:

- a part carries a run of words only where the run's whole row stands inside it

## bar-says-the-revision

The bar says the revision that runs.

In `flow/flow-pane/view.hpp`:

```diff
-        said.push_back("running");
+        said.push_back("running r" + std::to_string(model.running_definition->revision));
```

Red in `zengine-flow-tests`:

- Flow names a step by its operator, a port by its step and a field by its name: no step number,
  field spelling, revision or operator identity on screen

## card-head-says-its-identity

A step's card is headed by its operator's identity.

In `flow/flow-pane/view.hpp`:

```diff
-        pen.text(tx, rows.head, step_name(on->body.nodes, n), !light && selected ? hue::message() : hue::wire(),
+        pen.text(tx, rows.head, node.identity, !light && selected ? hue::message() : hue::wire(),
```

Red in `zengine-flow-tests`:

- Flow clips a large offscreen graph before spending the picture's budget
- Flow names a step by its operator, a port by its step and a field by its name: no step number,
  field spelling, revision or operator identity on screen

## lacks-in-the-graphs-words

What the draft lacks is said in the graph's own words, by node number and the library's why.

In `flow/flow-pane/model.hpp`:

```diff
-            if (said.empty()) said = (named ? (p.port ? port : step) : on.message->name()) + ": " + p.why;
+            said = (named ? (p.port ? port : step) : on.message->name()) + ": " + p.why;
```

Red in `zengine-flow-tests`:

- Flow names a step by its operator, a port by its step and a field by its name: no step number,
  field spelling, revision or operator identity on screen

## preview-names-its-provider

The preview says whose a row is, by its provider's office.

In `flow/flow-pane/find.hpp`:

```diff
-    return row.construction == workshop::kCompositeConstruction ? kind + ", made of other steps" : kind;
+    return kind + ", from " + row.provider;
```

Red in `zengine-flow-tests`:

- Flow's rail says what is in scope and where it comes from, and groups the door's rows:
  operators by their first word, then Sources, Conversions and Forms
- Flow names a step by its operator, a port by its step and a field by its name: no step number,
  field spelling, revision or operator identity on screen

## rail-row-says-one-line

A rail row's words stop at one line, cut where the rail ends.

In `flow/flow-pane/view.hpp`:

```diff
-                                           about_columns, 2);
+                                           about_columns, 1);
```

Red in `zengine-flow-tests`:

- the rail breaks a row's words and a heading to two lines, never cutting the first, and a wire in
  hand asks for and lists what takes it, each port of its type a way to add it

## heading-cut-to-one-line

A rail heading stops at one line, cut where the rail ends.

In `flow/flow-pane/view.hpp`:

```diff
-        for (const auto& line : wrapped(words, heading_columns, 2)) {
+        for (const auto& line : wrapped(words, heading_columns, 1)) {
```

Red in `zengine-flow-tests`:

- the rail breaks a row's words and a heading to two lines, never cutting the first, and a wire in
  hand asks for and lists what takes it, each port of its type a way to add it

## a-part-lights-nothing

A part of the sentence rested on lights itself alone, none of what it stands for.

In `flow/flow-pane/view.hpp`:

```diff
             out.chips.insert(i);
-            out.pointed.insert(chips[i].subjects.begin(), chips[i].subjects.end());
+            out.pointed.insert(chips[i].text);
```

Red in `zengine-flow-tests`:

- the sentence and the graph light each other: a part rested on lights its cards, steps and wires, a
  card or a step its parts, and a part still to choose is pressed as Next is

## a-card-lights-no-part

A card or a step rested on lights no part of the sentence.

In `flow/flow-pane/view.hpp`:

```diff
-        if (std::find(said.begin(), said.end(), key) != said.end() ||
+        if (std::find(said.begin(), said.end(), key) == said.end() &&
```

Red in `zengine-flow-tests`:

- the sentence and the graph light each other: a part rested on lights its cards, steps and wires, a
  card or a step its parts, and a part still to choose is pressed as Next is

## a-port-tells-nothing

Resting on a port tells nothing.

In `flow/flow-pane/view.hpp`:

```diff
-        if (head != "port" && head != "answer" && head != "wire") return {};
+        if (head != "answer" && head != "wire") return {};
```

Red in `zengine-flow-tests`:

- resting tells: a port its kind and what it reads, an answer or a field what reads it, a wire its
  two ends lit and what it last carried; a wire in the hand says why a port does not fit

## a-wires-ends-unlit

Resting on a wire lights the wire alone, neither of its ends.

In `flow/flow-pane/view.hpp`:

```diff
-        for (const auto& end : wire_ends(model, key)) out.pointed.insert(end);
+        for (const auto& end : wire_ends(model, key)) out.pointed.insert(end.substr(1));
```

Red in `zengine-flow-tests`:

- resting tells: a port its kind and what it reads, an answer or a field what reads it, a wire its
  two ends lit and what it last carried; a wire in the hand says why a port does not fit

## another-type-fits

A wire of another type fits a port.

In `flow/flow-pane/find.hpp`:

```diff
-    if (*type != port->type) return words + " takes " + port->type + ", and " + from + " is " + *type;
+    if (type->empty()) return words + " takes " + port->type + ", and " + from + " is " + *type;
```

Red in `zengine-flow-tests`:

- resting tells: a port its kind and what it reads, an answer or a field what reads it, a wire its
  two ends lit and what it last carried; a wire in the hand says why a port does not fit

## the-box-has-no-part

The box of what resting tells is no part of its own, so the parts beneath it paint over it.

In `flow/flow-pane/view.hpp`:

```diff
-    pen.press(box, "tip", {"box"});
+    pen.press(Box{}, "tip", {"box"});
```

Red in `zengine-flow-tests`:

- resting tells: a port its kind and what it reads, an answer or a field what reads it, a wire its
  two ends lit and what it last carried; a wire in the hand says why a port does not fit

## the-last-message-lights-nothing

Nothing the last message changed stays lit.

In `flow/flow-pane/view.hpp`:

```diff
-    out.happened = changed_by_last(model);
+    out.happened.clear();
```

Red in `zengine-flow-tests`:

- what the last message changed stays lit while the rule runs, until the next: the wires whose
  values changed, the steps they reach, what it wrote where the kept value changed, what it said

## an-unchanged-wire-lights

A wire from a field of the message is lit whatever it carried before.

In `flow/flow-pane/view.hpp`:

```diff
-                                            : !before || value(before->carried, name) != value(last.carried, name);
+                                            : true;
```

Red in `zengine-flow-tests`:

- what the last message changed stays lit while the rule runs, until the next: the wires whose
  values changed, the steps they reach, what it wrote where the kept value changed, what it said

## an-unchanged-write-lights

What the rule writes is lit though what it keeps stayed what it was.

In `flow/flow-pane/view.hpp`:

```diff
-    if (value(last.before, on.output) != value(last.after, on.output)) out.insert("card:write:" + on.message->name());
+    if (value(last.before, on.output) != value(last.after, on.output) || last.message == on.message->name())
+        out.insert("card:write:" + on.message->name());
```

Red in `zengine-flow-tests`:

- what the last message changed stays lit while the rule runs, until the next: the wires whose
  values changed, the steps they reach, what it wrote where the kept value changed, what it said

## thread-stops-at-the-place

The then thread stops at the edge of the dashed place for a say, short of its button.

In `flow/flow-pane/view.hpp`:

```diff
-            if (light && by > box.y)
+            if (false)
```

Red in `zengine-flow-tests`:

- the then thread from what the rule writes runs on, through the dashed place for a say, onto its
  + say button

## say-button-beside-the-thread

The + say button stands in the middle of its place, beside the thread.

In `flow/flow-pane/view.hpp`:

```diff
-            const auto bx = std::max(box.x + (light ? 8 : 0), thread_x - (light ? 12 : 0));
+            const auto bx = box.x + std::max<std::int64_t>(0, (box.w - pen.button_width(label)) / 2);
```

Red in `zengine-flow-tests`:

- the then thread from what the rule writes runs on, through the dashed place for a say, onto its
  + say button

## dots-reach-their-row-only

A port's dot takes a press only where it overlaps its card's row; the half outside presses what
lies behind it.

In `flow/flow-pane/view.hpp`:

```diff
-    if (!pen.m.light) return;
+    if (pen.m.light || !pen.m.light) return;
```

Red in `zengine-flow-tests`:

- a port's dot takes a press and a rest where it is drawn, the half outside its card's row too,
  doing what its row does

## guide-pushes-a-say

Running with nothing said, the guide pushes a say as the next step.

In `flow/flow-pane/next.hpp`:

```diff
-            out.text = wakes + " wakes it: the wires and " + on->output + " follow each one.";
+            out.text = wakes + " wakes it. Next: say " + on->output + ", so a panel can show it.";
+            out.button = "+ say " + def.name + "." + say_name(on->output);
+            out.action = "ask-say";
```

Red in `zengine-flow-tests`:

- the Next line names the one thing the rule still needs, with the press that does it, in place of
  Run's refusals
