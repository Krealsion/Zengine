# The mutants `tests/test_workshop_panes_actions.cpp` claims

Each entry is a mutant that a comment in
[`tests/test_workshop_panes_actions.cpp`](../test_workshop_panes_actions.cpp) claims its cases
catch, the comment naming the entry by its path and heading. [The corpus's front page](README.md)
says how an entry reads and how `tests/mutants/run.py` runs it.

## above-modes-arm-deleted

`on(KeyPressed)` never asks for an application row above the modes.

In `workshop/weave_handlers.cpp`:

```diff
-    if (const AppRow* row = session_.keymap.app_action_for(app_precedence::kAboveModes, ctx,
-                                                           k.scancode, k.modifiers,
-                                                           keyboard_pane())) {
+    if (const AppRow* row = session_.keymap.app_action_for(app_precedence::kAboveModes, ctx,
+                                                           k.scancode, k.modifiers,
+                                                           keyboard_pane()); false) {
```

Red in `zengine-workshop-panes-tests`:

- an application row is joined, is requested above the modes, and reaches its declarer as the
  resolved id

## authored-not-applied

Joining the application's rows keeps each row's declared key, whatever the weaver's file says.

In `workshop/keymap_join.hpp`:

```diff
             const AuthoredGestures set = authored_gestures_for(k.authored, row.id, false);
             if (!set.outcome.accepted) {
                 return set.outcome;
             }
-            if (!set.moved) {
-                widened.push_back(row);
+            if (true || !set.moved) {
+                widened.push_back(row);
```

Red in `zengine-workshop-panes-tests`:

- an application row is joined, is requested above the modes, and reaches its declarer as the
  resolved id

## none-arm-dropped

`parse_gesture` has no `none`: the word is read as a key, and refused.

In `workshop/gesture_grammar.hpp`:

```diff
-    if (text == "none") {
-        out.accepted = true;
+    if (false) {
+        out.accepted = true;
```

Red in `zengine-workshop-panes-tests`:

- a weaver's authored row moves an application row, and `none` disables it

## override-loop-dropped

Joining the application's rows drops every row the weaver's file moved.

In `workshop/keymap_join.hpp`:

```diff
-            for (const Gesture& g : set.gestures) {
-                AppRow moved = row;
-                moved.gesture = g;
-                widened.push_back(std::move(moved));
-            }
```

Red in `zengine-workshop-panes-tests`:

- a weaver's authored row moves an application row, and `none` disables it

## deselect-correlation-dropped

A deselect answer is taken as the answer to whichever hand's ask, whatever its number.

In `workshop/weave_desktop.cpp`:

```diff
-        if (mail.correlation() != 0 && mail.correlation() == h.app_asked.answering) by = &h;
+        by = &h;
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-02: the host asks the desktop for the default row, and only an answer that echoes the ask
  puts the selection down

## inventory-ask-answered-nothing

An ask for the inventory is answered nothing.

In `workshop/weave_desktop.cpp`:

```diff
-    (void)mail.answer(inventory_reading());
-}
+    (void)0;
+}
```

Red in `zengine-workshop-panes-tests`:

- an arriving presenter is answered the inventory as it is now, to itself alone, and an offer makes
  the next reading be said again

## offer-keeps-inventory-said

An offer leaves the inventory's last reading standing as said.

In `workshop/weave_seam.cpp`:

```diff
-    inventory_published_ = false; // ...and the inventory, which a presenter reads (WL-DESK-04)
```

Red in `zengine-workshop-panes-tests`:

- an arriving presenter is answered the inventory as it is now, to itself alone, and an offer makes
  the next reading be said again

## launcher-window-from-the-top

The launcher's list window starts at its first row wherever the cursor stands.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-            known_.size(), choice_.at, first_, budget > 0 ? static_cast<std::size_t>(budget) : 0);
+            known_.size(), 0, 0, budget > 0 ? static_cast<std::size_t>(budget) : 0);
```

Red in `zengine-workshop-panes-tests`:

- the launcher keeps the row it will open in view, and its feedback on a row of its own

## cursor-by-index

`find_cursor` keeps the cursor's index and never looks for the pane it chose.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-        choice_.find(known_, key_of_pane);
```

Red in `zengine-workshop-panes-tests`:

- the launcher's cursor is an identity: rows moving under it do not retarget Return, and a row that
  left the list is said, not replaced

## verdict-by-an-ordinary-send

A declaration's verdict is sent to its declarer as an ordinary send, not as the answer to it.

In `workshop/weave_desktop.cpp`:

```diff
-    (void)mail.answer(verdict);
-}
+    (void)mail.as_role(kWorkshopProvider).send_to_role(office, verdict);
+}
```

Red in `zengine-workshop-panes-tests`:

- a verdict answers the declaration it judges: a refused attempt is named by its own number after a
  later one was accepted, and an accepted one is given Workshop's

## rejoin-keeps-what-it-could-not-join

Rows the keymap file displaced are kept, and nothing is withdrawn.

In `workshop/weave_desktop.cpp`:

```diff
-    const std::int64_t was = app_declaration_;
-    app_actions_.clear();
-    app_declaration_ = 0;
-    say_withdrawn(std::string(kDesktopRole),
-                  ActionsWithdrawn{std::string(), was, "the application's keys: " + joined.refusal},
-                  mail);
+    (void)mail;
```

Red in `zengine-workshop-panes-tests`:

- a declaration the keymap file displaces is withdrawn by the number its verdict gave it, and a pane
  that reads verdicts learns both

## provider-present-from-the-catalog

`provider_present` answers yes for every pane the catalog knows.

In `workshop/weave_desktop.cpp`:

```diff
-    return (host_->role_holder && host_->role_holder(ref.provider).valid()) ||
-           (host_->holder_accepts &&
-            host_->holder_accepts(ref.provider, *loom::schema_of<graphics::AreaRoom>()));
+    (void)ref;
+    return true;
```

Red in `zengine-workshop-panes-tests`:

- a pane whose provider left is unavailable in the launcher and refused at launch, while its
  identity and the desk row naming it stay

## pending-left-unset

The inventory never says a pane is still to come.

In `workshop/weave_desktop.cpp`:

```diff
-        p.pending = !p.available && host_->office_pending && host_->office_pending(p.office);
+        p.pending = false;
```

Red in `zengine-workshop-panes-tests`:

- a pane the run is still loading is pending, not unavailable: the launcher marks it `[load]`, names
  nothing to build, and a launch says it is not here yet

## gone-note-counts-pending

The launcher's `[gone]` note counts every unavailable row, one still to come too.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-            any_gone = any_gone || (!p.available && !p.pending);
+            any_gone = any_gone || !p.available;
```

Red in `zengine-workshop-panes-tests`:

- a pane the run is still loading is pending, not unavailable: the launcher marks it `[load]`, names
  nothing to build, and a launch says it is not here yet

## close-answered-without-taking

The owner's close answers `closed` and takes no row off.

In `workshop/layouts/store.cpp`:

```diff
-    const LayoutEditAnswered taken = edit(asked);
+    LayoutEditAnswered taken;
+    taken.accepted = true;
+    (void)asked;
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-12: a close takes a pane off the desk and leaves its provider holding; a close of a pane
  that is not there is refused and opens nothing

## close-falls-through-to-a-seat

A close of a pane that is not on the desk seats it, as the launch door does, before it refuses.

In `workshop/layouts/store.cpp`:

```diff
-    if (!has_pane(live_desk(run_), ref)) {
-        out.refusal = name + " is not on this desk -- nothing to hide, and a hide shows nothing";
+    if (!has_pane(live_desk(run_), ref)) {
+        LayoutEdit seat;
+        seat.op = layout_op::kSeat;
+        seat.provider = ref.provider;
+        seat.pane = ref.pane;
+        LayoutEditRequested opened;
+        opened.edits.push_back(std::move(seat));
+        (void)edit(opened);
+        out.refusal = name + " is not on this desk -- nothing to hide, and a hide shows nothing";
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-12: a close takes a pane off the desk and leaves its provider holding; a close of a pane
  that is not there is refused and opens nothing

## close-names-no-lock

The owner's close answers naming no lock.

In `workshop/layouts/store.cpp`:

```diff
-    out.locks = taken.unlocked;
+    out.locks = {};
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-12: a close is the layouts' owner's: the row leaves the live layout whole, its settings
  and this layout's locks named in the answer and on the band, and the presenter takes no close

## personal-speech-closes

The owner takes a row off for personal speech's close too.

In `workshop/layouts/owner.cpp`:

```diff
-    if (mail.authored_role().empty()) {
-        // AN OFFICE, AND ONLY AN OFFICE, changes who is on the desk -- the seam's rule -- and the
+    if (false) {
+        // AN OFFICE, AND ONLY AN OFFICE, changes who is on the desk -- the seam's rule -- and the
```

Red in `zengine-workshop-panes-tests`:

- WL-DESK-12: a close is the layouts' owner's: the row leaves the live layout whole, its settings
  and this layout's locks named in the answer and on the band, and the presenter takes no close

## lost-choice-cleared

`find_cursor` clears the keys of a choice whose row left the list.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-        choice_.find(known_, key_of_pane);
+        choice_.find(known_, key_of_pane);
+        if (choice_.lost) {
+            choice_.chosen = false;
+        }
```

Red in `zengine-workshop-panes-tests`:

- a choice whose row left stays unchosen across a desktop replacement and the publications after it:
  Return and x reach no neighbour, and a row chosen then is obeyed

## new-launches-another-pane

The Pane Manager's `n` launches the Hotkeys pane.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-            launch(mail, kViewBuilderOffice, kViewBuilderPane, launches);
+            launch(mail, "zengine.desktop", "hotkeys", launches);
```

Red in `zengine-workshop-panes-tests`:

- WL-MAKER-11: the shipped Pane Manager's `n` shows the View Builder through the host's launch door,
  and a Workshop without one says so in the door's words

## new-launches-nothing

The Pane Manager's `n` launches nothing.

In `workshop/desktop-pane/launcher_view.hpp`:

```diff
-            launch(mail, kViewBuilderOffice, kViewBuilderPane, launches);
+            (void)launches;
```

Red in `zengine-workshop-panes-tests`:

- WL-MAKER-11: the shipped Pane Manager's `n` shows the View Builder through the host's launch door,
  and a Workshop without one says so in the door's words

## file-row-bytes-as-they-are

The Hotkeys pane's file row carries the keymap file's path as its bytes are.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
-                                                                     : drawable(keymap_.file)));
+                                                                     : keymap_.file));
```

Red in `zengine-workshop-panes-tests`:

- the Hotkeys pane names a keymap file whose path a canvas cannot draw with the path made drawable,
  in a picture Workshop admits

## help-lines-kept

The Hotkeys pane's two help lines are drawn whatever `help` says.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
             footer.push_back("  " + drawable(keys_rows_decl.word));
         }
-        if (help_shown_) {
+        if (true) {
```

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
             footer.push_back("  " + drawable(keymap_.word));
         }
-        if (help_shown_) {
+        if (true) {
```

Red in `zengine-workshop-panes-tests`:

- the Hotkeys pane declares its `help` setting, and a layout keeping it off gives the footer's help
  lines to the list

## example-names-flows-chord

The Hotkeys pane's example row names `ctrl+g`, Flow's chord for asking for native C++.

In `workshop/desktop-pane/hotkeys_view.hpp`:

```diff
-            footer.push_back("or write {\"action\": \"<id>\", \"gesture\": \"alt+up\"} in the "
+            footer.push_back("or write {\"action\": \"<id>\", \"gesture\": \"ctrl+g\"} in the "
```

Red in `zengine-workshop-panes-tests`:

- the Hotkeys pane's example row names a chord no pane on the desk declares

## renamed-id-pass-dropped

Joining the application's rows reads no row written for an id's old owner.

In `workshop/keymap_join.hpp`:

```diff
-            const AuthoredGestures set = authored_gestures_for(k.authored, row.id, true);
+            const AuthoredGestures set = authored_gestures_for(k.authored, row.id, false);
```

Red in `zengine-workshop-panes-tests`:

- a keymap row written for an id whose owner changed is read as its successor, once, and the load
  says which rename to make
