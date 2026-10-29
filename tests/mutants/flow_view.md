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
