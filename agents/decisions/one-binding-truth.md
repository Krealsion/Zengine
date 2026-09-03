# One binding truth

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [keyboard](../workshop/keyboard.md).

**Context.** Gesture claims were string literals — measured drifting in six places — beside
two hand-kept mirror predicates and two hand-copied routing chains; the subset aliases let Ctrl+N
create and Alt+Q quit by accident; and the Terminal's toggle was Shift+Space, which a POSIX
terminal cannot report at all (`7b64b73`, "Make hotkeys executable truth: one keymap for dispatch,
help, and remapping"). The editor then needed `^s` to mean two things without a mode check
(`7ac1d53`).

**Decision.** An ACTION is a stable dotted id, label, context and default gesture in
`kActionCatalog`; a BINDING is the gesture that requests it; EXECUTION stays with each dispatch
site. `Session::keymap` is the effective truth, and every hint is a projection through
`hotkey_text`/`gesture_text`. `keyboard_context` is the routing chain spelled once. Matching is
exact. Three declaration-only activity classes answer above a mode, and `^s` is two declared
rows whose contexts never meet. An action may own several rows and an override moves all of
them. The keymap file holds authored differences only. Admission refuses naming what a maker can
fix. The legend preference governs the band's legend rows and nothing else. The printable-trigger
swallow is derived from the binding. A row may answer to no key.

**Alternatives considered.**
- *Subset-alias modifier matching* — removed and behaviorally falsified; pinned by case
  `"KEY-0: exact modifier matching -- the accidental subset aliases no longer fire"`.
- *Aliasing `shift+space` to the new toggle* — rejected: gone, not aliased; pinned by case
  `"KEY-0: shift+space is gone -- not a binding, not an invisible alias"`.
- *Callbacks, a command bus, a registry object, provider-contributed declarations, TextBox
  remapping, sequences, leaders, macros, new wire vocabulary* — none; the keymap holds names and
  gestures only (`7b64b73`).
- *Hard-coding the expected character at the three swallow sites* — replaced by
  `expected_text_of` (`568ed5f`, "Pin the swallow's correspondence: only the trigger's own
  character is eaten").
- *Resolving the `^s` collision by a mode check* — replaced by declared classes so admission,
  the help surfaces and dispatch read one fact; pinned by case `"EDIT-0: one physical ^s
  resolves to the document's save or the editor's, by context"`.
- *Spending free printables on the four tab operations* — rejected: the POSIX wire carries an
  unshifted printable and a shifted letter and nothing else in that family, and those go to
  whatever asks next; and two unbound actions must not read as a collision that refuses a whole
  keymap file (`2dc7626`).

**Consequences.** An override survives restart and deleting the file restores defaults; a
retired id's row is preserved byte-for-byte as unknown; a known POSIX-gap gesture is accepted
and the gap said once. A bare printable cannot be global once anything on the screen can take
text. `gesture_text` answers `unbound`, so no surface teaches a key that does not exist. Hidden
legend rows reclaim no geometry and unbind nothing.

**Laws supported.** [WL-EDIT-04](../workshop/editor.md), [WL-KEY-01](../workshop/keyboard.md),
[WL-KEY-02](../workshop/keyboard.md), [WL-KEY-03](../workshop/keyboard.md),
[WL-KEY-04](../workshop/keyboard.md), [WL-KEY-05](../workshop/keyboard.md),
[WL-KEY-06](../workshop/keyboard.md), [WL-KEY-07](../workshop/keyboard.md),
[WL-KEY-08](../workshop/keyboard.md), [WL-KEY-09](../workshop/keyboard.md),
[WL-KEY-12](../workshop/keyboard.md), [WL-KEY-13](../workshop/keyboard.md),
[WL-KEY-14](../workshop/keyboard.md).
