# The mutants `tests/test_component.cpp` claims

Each entry is a mutant that a comment in [`tests/test_component.cpp`](../test_component.cpp) claims
its cases catch, the comment naming the entry by its path and heading. [The corpus's front
page](README.md) says how an entry reads and how `tests/mutants/run.py` runs it.

## word-at-identifier-class

`word_at` scans a word of its own: letters, digits and `_`, as an identifier is.

In `component/text_box.hpp`:

```diff
-inline WordSpan word_at(const std::string& line, std::size_t at) noexcept {
-    return WordSpan{word_run_begin(line, at), word_run_end(line, at)};
-}
+inline WordSpan word_at(const std::string& line, std::size_t at) noexcept {
+    const auto sep = [](char c) { return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'); };
+    std::size_t b = at < line.size() ? at : line.size();
+    std::size_t e = b;
+    while (b > 0 && !sep(line[b - 1])) {
+        --b;
+    }
+    while (e < line.size() && !sep(line[e])) {
+        ++e;
+    }
+    return WordSpan{b, e};
+}
```

Red in `zengine-component-tests`:

- one run definition, and the keyboard's two answers are composed from it

## word-at-punctuation-class

`word_at` scans a word of its own, ending at punctuation as at a space.

In `component/text_box.hpp`:

```diff
-inline WordSpan word_at(const std::string& line, std::size_t at) noexcept {
-    return WordSpan{word_run_begin(line, at), word_run_end(line, at)};
-}
+inline WordSpan word_at(const std::string& line, std::size_t at) noexcept {
+    const auto sep = [](char c) { return c == ' ' || c == '.' || c == '/' || c == '-' || c == ','; };
+    std::size_t b = at < line.size() ? at : line.size();
+    std::size_t e = b;
+    while (b > 0 && !sep(line[b - 1])) {
+        --b;
+    }
+    while (e < line.size() && !sep(line[e])) {
+        ++e;
+    }
+    return WordSpan{b, e};
+}
```

Red in `zengine-component-tests`:

- one run definition, and the keyboard's two answers are composed from it

## word-at-to-next-boundary

`word_at` runs from its word to the next boundary, the spaces after the word with it.

In `component/text_box.hpp`:

```diff
-    return WordSpan{word_run_begin(line, at), word_run_end(line, at)};
+    return WordSpan{word_run_begin(line, at), word_after(line, at)};
```

Red in `zengine-component-tests`:

- one run definition, and the keyboard's two answers are composed from it

## blank-falls-back-to-word-before

A position with a space on both sides takes the word before it.

In `component/text_box.hpp`:

```diff
-    return WordSpan{word_run_begin(line, at), word_run_end(line, at)};
+    const WordSpan span{word_run_begin(line, at), word_run_end(line, at)};
+    const std::size_t before = word_before(line, at);
+    return span.present() ? span : WordSpan{before, word_run_end(line, before)};
```

Red in `zengine-component-tests`:

- the word at a position, including at both of its edges

## word-before-spelled-inline

`word_before` spells its run's walk inline, and stops a byte short of the run's start.

In `component/text_box.hpp`:

```diff
-        --i;
-    }
-    return word_run_begin(line, i);
+        --i;
+    }
+    while (i > 1 && line[i - 2] != ' ') {
+        --i;
+    }
+    return i;
```

Red in `zengine-component-tests`:

- pointer and keyboard agree about which bytes are one word

## accordion-peek-springs-back

A press on a section's header leaves the pointer's peek standing, so a section shut under the
resting pointer peeks again at once.

In `component/accordion.hpp`:

```diff
         open = !open;
-        spent = resting;
+        spent = false;
```

Red in `zengine-component-tests`:

- Accordion: shut shows the header, a press opens it, a resting pointer peeks a shut one, a press
  during a peek opens it, and one shut under the pointer peeks again only once the pointer has left

## accordion-peek-takes-room

A peeking section's body is counted as room it takes.

In `component/accordion.hpp`:

```diff
-    std::size_t taken() const noexcept { return header + (over ? 0 : body); }
+    std::size_t taken() const noexcept { return header + body; }
```

Red in `zengine-component-tests`:

- section_span: the header first, then as much of the body as the room holds -- in the room while
  open, over what follows while it peeks -- and what did not fit counted, at every room

## group-shut-all-keeps-one

Shutting every section of the independent group leaves the first open one open.

In `component/accordion.hpp`:

```diff
         const bool any = !open_->empty();
-        open_->clear();
+        open_->resize(open_->empty() ? 0 : 1);
```

Red in `zengine-component-tests`:

- IndependentAccordions: each section opens and shuts alone by its key, one act shuts them all, the
  pointer peeks one at most, and the open ones are kept across a population that moves until it no
  longer holds them

## group-keep-forgets-nothing

The independent group keeps a section open after its population no longer holds it.

In `component/accordion.hpp`:

```diff
-        open_->erase(std::remove_if(open_->begin(), open_->end(),
-                                    [&held](const Key& k) {
-                                        return !std::binary_search(held.begin(), held.end(), k);
-                                    }),
-                     open_->end());
+        (void)held;
```

Red in `zengine-component-tests`:

- IndependentAccordions: each section opens and shuts alone by its key, one act shuts them all, the
  pointer peeks one at most, and the open ones are kept across a population that moves until it no
  longer holds them

## group-keeps-its-own-copy

The independent group reads a copy of the open sections taken when it was made, not the owner's
list, so an edit to the owner's state shows nothing.

In `component/accordion.hpp`:

```diff
-    explicit IndependentAccordions(std::vector<Key>& open) noexcept : open_(&open) {}
+    explicit IndependentAccordions(std::vector<Key>& open) noexcept : open_(new std::vector<Key>(open)) {}
```

Red in `zengine-component-tests`:

- IndependentAccordions: the open sections are the owner's list, read there and written there --
  an edit to it is what the group shows, and the next fold writes over nothing it did not change

## tree-shows-under-a-shut-branch

A tree shows the rows under a shut branch.

In `component/accordion.hpp`:

```diff
-        if (hiding && depth > hidden_below) {
+        if (false && hiding && depth > hidden_below) {
```

Red in `zengine-component-tests`:

- shown_tree: a row shows while every branch above it is open, a branch is a row whose next row
  stands deeper, and tree_parent finds the nearest shallower row before it

## tree-hides-a-shut-branchs-sibling

A shut branch hides the rows after it that stand as deep as it does, its siblings.

In `component/accordion.hpp`:

```diff
-        if (hiding && depth > hidden_below) {
+        if (hiding && depth >= hidden_below) {
```

Red in `zengine-component-tests`:

- shown_tree: a row shows while every branch above it is open, a branch is a row whose next row
  stands deeper, and tree_parent finds the nearest shallower row before it
