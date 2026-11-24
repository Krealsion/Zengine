# Property

**A typed connection to one property of one thing, the words a value of a type is written and read
in, and an inspector's line over one property: what a weaver edits by text, checked before it is
taken.**

A `Property<T>` reads its value and tries to write one, and a write comes back as a `Written`
(`persist/written.hpp`): taken, or refused with its reason. `TextForm<T>` is how a type becomes text
and text becomes it again, one for each type, shared by every property of that type. A `Row` is an
inspector's line over a property, read fresh at every look and written only by finished text.

When a weaver finishes typing into a row, the row tells three outcomes apart (`Commit`):

- **Accepted**: the text is a value of the row's type, and the property took it.
- **Unparseable**: the text is not a value of the row's type at all, such as `abc` for a whole
  number. Nothing is written, and the row's refusal says what it wanted: "not a whole number". The
  weaver answers it by typing again.
- **Refused**: the text is a value of the type, and the property will not take it. Nothing is
  written, and the refusal is the property's own sentence, such as "an extent is 1 to 100 cells".
  The weaver answers it by wanting something else.

A row that only shows a fact, or heads a section, takes no text: it is refused as "not authored".

A program includes `property/property.hpp` and links `zengine::property`.

## Parts

| file | what it holds |
|---|---|
| `property.hpp` | `Property`, `TextForm` (with the forms for text and whole numbers), `Commit` and `Row` |
| `CMakeLists.txt` | `zengine-property`, a header-only target aliased `zengine::property`, over `zengine-persist` for `Written` |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`property/` adds to it.

**Law.** The rule this folder keeps is Workshop's: WL-DOC-02, in
[Workshop's document register](../agents/workshop/document.md), says that unparseable text writes
nothing, a refused value is a different outcome with its reason, a shown row takes no write, and
two properties of one type share one conversion. Its proof, and the Pane Manager's section rows
(`agents/workshop/pane-manager.md`), are routed by [Workshop's router](../workshop/AGENTS.md). A
rule about one declaration is in that declaration's comment.

**Suites.** The folder has no CTest entry of its own. `workshop_document`
(`tests/test_workshop_document.cpp`) holds WL-DOC-02: a commit through the setter, unparseable text
leaving the property untouched, a refused value told apart with its reason, a shown row refused, and
the whole-number form. The suites of Workshop's files, the editors, Files and Info read and write
their properties through it. Each runs through the official lane, `tests/verify.cmake`.

**The host's side.** Workshop's files (`workshop/*_persist.hpp`, the session's, the layouts'), the
editors' panes, Files and Info's settings rows hold their properties through it. The text a weaver is still
typing is not the row's: it is the inspector's own draft, and only finished text reaches
`Row::commit_text` (WL-INFO-15, `agents/workshop/info-body.md`).
`cmake/ZengineInstall.cmake` installs the header.
