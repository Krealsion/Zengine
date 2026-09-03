# A name is judged in bytes, whole, before it is kept

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [setup-file](../workshop/setup-file.md), [catalog](../workshop/catalog.md) and
[maker-pane](../workshop/maker-pane.md).

**Context.** Four names reach this application from parties it has never met: a setup's human
name, typed in the one-line editor or carried by a file somebody else wrote; either half of a
pane reference, which a provider may write in any script the Loom's UTF-8 gate accepts; a runtime
descriptor's display name and summary, arriving in a live message; and a maker-made pane's name,
which is a durable key and a display name at once. Each can render as nothing, move a terminal's
cursor out of the row it was given, or be longer than the line that has to show it whole. The
first spelling of the bounds said "characters": a nine-character name in four-byte UTF-8 is
thirty-six bytes, and telling that maker they had exceeded thirty-two characters was a false
sentence about a true refusal.

**Decision.** One checker per name, reached by every door that can carry it: present, more than
spaces, no control byte, and a byte count spent against `size()`, with the refusal naming the
rule and saying bytes. A control character is refused rather than rendered safe, because only a
forged file or a foreign message can carry one, and a silent substitution is a thing a maker
would then have to discover. The pane key and the maker-pane name add "no whitespace", and the
maker-pane name adds "no `/`", so `provider/pane` and `provider/name` stay one legible token in a
notice and in a file. A key is judged as a view before anything owns a copy of it. No Unicode
policy is made: valid UTF-8 is the gate's answer on the way in, and nothing counts a code point,
a grapheme or a cell.

**Alternatives considered.**
- *Tried: counting characters and saying so* — the refusal said thirty-two characters of a
  thirty-two-byte bound; corrected to say bytes, pinned by case `"WS-0a: the name and key bounds
  are BYTES, and the refusal says bytes"`.
- *Tried: an owned string as the key checker's argument* — it made the copy the precondition of
  the check that decides whether the copy is allowed; the checker takes a view, pinned by case
  `"an office longer than the key bound is delivered whole and admitted by nobody"`.
- *Argued: rendering a control character safe* — refused in the checker's own words: refusing
  names the field and leaves the live setup untouched, which is strictly more useful than a
  substitution a maker would have to discover.
- *Argued: a second checker for a runtime descriptor's keys* — never written; a key a saved setup
  could not spell would be an identity a maker could never keep, pinned by case `"a descriptor's
  two keys are judged by the setup file's own law"`.

**Consequences.** `kMaxSetupNameLen` is thirty-two because the setup line at the minimum
composition must fit the name whole beside the file and the saved marker (measured at 78 cells);
`kMaxPaneKeyLen` is sixty-four; a descriptor's name is thirty-two and its summary sixty-four,
because the picker pads a name into a ten-column field on the narrowest overlay this composition
lays out; a maker-made pane's name is thirty-two. The bounds are input boundaries and not
capacities: none is a statement about how many panes Workshop can usefully show, and a refusal
keeps nothing of what it refused.

**Laws supported.** [WL-SETUP-09](../workshop/setup-file.md),
[WL-SETUP-10](../workshop/setup-file.md), [WL-CAT-02](../workshop/catalog.md),
[WL-MAKER-13](../workshop/maker-pane.md).
