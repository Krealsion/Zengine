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
