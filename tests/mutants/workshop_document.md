# The mutants `tests/test_workshop_document.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_document.cpp`](../test_workshop_document.cpp) claims its cases catch, the
comment naming the entry by its path and heading. [The corpus's front page](README.md) says how an
entry reads and how `tests/mutants/run.py` runs it.

## close-from-anyone

Workshop quits on a close whoever said it.

In `workshop/weave_handlers.cpp`:

```diff
-    if (window.empty() || !mail.authored_from_role(window)) {
-        return;
-    }
-    quit(mail);
+    if (window.empty()) {
+        return;
+    }
+    quit(mail);
```

Red in `zengine-workshop-document-tests`:

- a close is the window's that shows the desk: one said by anyone else ends nothing

## example-in-another-envelope

The hotkeys guide's keymap example is written in another envelope: `"value"` for `"fields"`, its
numbers quoted.

In `workshop/docs/hotkeys.md`:

```diff
-{"zen":1,"schema":"WorkshopKeymap","version":2,"fields":{
+{"zen":"1","schema":"WorkshopKeymap","version":"2","value":{
```

Red in `zengine-workshop-document-tests`:

- the hotkeys guide's keymap example is a file Workshop reads and applies

## guide-spells-flows-chord

The hotkeys guide gives `ctrl+g`, Flow's chord for asking for native C++, as a key to type.

In `workshop/docs/hotkeys.md`:

```diff
-| `Modify (type a spelling)` | type the key as the file spells it (`alt+up`, `shift+h`, `[`), then `Enter`; a spelling is plain ASCII on one line, so text typed or pasted holding anything else is not typed, and the pane says so |
+| `Modify (type a spelling)` | type the key as the file spells it (`alt+up`, `ctrl+g`, `[`), then `Enter`; a spelling is plain ASCII on one line, so text typed or pasted holding anything else is not typed, and the pane says so |
```

Red in `zengine-workshop-document-tests`:

- the keys the hotkeys guide gives to type are keys no pane on the desk declares
