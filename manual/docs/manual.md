# A weave's manual

**Reference.** A manual is the primary reference for one weave: one Markdown page, named by the
weave's office, with one section per command. The weave carries it compiled in, so the words a
running weave says are the words of the page at the commit it was built from. A pane answers its
manual when asked, and an office a shape's section; nothing keeps a copy. The same bytes are what a
reader of the repository reads on GitHub.

Source: [`manual/manual.hpp`](../manual.hpp) · [`manual/vocabulary.hpp`](../vocabulary.hpp) ·
[`cmake/ZengineManual.cmake`](../../cmake/ZengineManual.cmake) ·
[`workshop/pane_document.hpp`](../../workshop/pane_document.hpp). The first manuals are the
Builder's: [the Builder pane](../../builder/docs/zengine.builder-pane.md) and
[the Builder](../../builder/docs/zengine.builder.md).

## What a command is

A command is what a caller names to make a weave act or answer:

- for a **pane**, an action id it declares (`PaneActions`): what a weaver's keymap file moves, what
  the hotkey view lists, what a menu line performs. A menu line's own id is not a command, and the
  pane protocol's shapes are Workshop's, not the pane's;
- for an **office**, a request shape it accepts. The rest of what it accepts, what its collaborators
  tell it, it *hears*; Loom's own doors (`zen.DescribeAccepted`, the pokes) are Loom's to describe.

## The page

One file per weave, named by its office, in its folder's `docs/`: `builder/docs/zengine.builder-pane.md`
is the manual of the weave holding `zengine.builder-pane`. Its form, in this order; a section a weave
has nothing for is left out, never written empty:

| heading | holds |
|---|---|
| `# <office> -- <the name a weaver sees>` | the title, then a short bold-led paragraph: what this weave is and how it is reached |
| `## About` | what it is and what it is for |
| `## Use` | how to use it, in a few sentences |
| `## Commands` | one `###` section per command of its main mode, in the order the weave declares or lists them |
| `## Commands in <mode>` (or `on`) | a pane's further modes, each a group of its own |
| `## Hears` | an office's accepted shapes that are not commands, one short `###` section each |
| `## Follow` | what it says unasked, and how an observer follows it |
| `## Shows` | a pane's rows, one short `###` section each, by the row's key |
| `## Files` | the files it reads or writes |
| `## See also` | tutorials, other manuals and pages |

A section's heading is its key in backticks and nothing else, so its anchor is the key
(`#builderbuild`, `#buildrequested`) and stays put while the words change. A command's section has
four labelled lines, in order:

```markdown
### `builder.build`

Builds the recipe you have chosen.

- **Takes:** what it acts on: the pane's subject and mode, or the shape's fields by name.
- **Answers:** what the caller then sees: rows, notices, the message it sends, the shape answered.
- **Refuses:** each refusal in the words the code says, and when.
- **Example:** one concrete use: a press, a line typed, or a weave's send.
```

Each section, the page and the number of sections are bounded by the constants
[`manual/manual.hpp`](../manual.hpp) declares, which the build and Workshop judge a page by.

**A key is written in a key marker, and only there**: `<!-- key builder.build -->`b`<!-- /key -->`,
which GitHub shows as `b`. The key is spelled as a weaver's keymap file spells it (`shift+b`,
`return`, `ctrl+p`), and the marker holds the default the pane declares; a pane's own `?` can show
a weaver's own key in its place. Tutorials write keys the same way.

**What a page never writes**: a field's type, a version beyond a shape's own `Name vN`, or a count
the code owns; a fact another page owns, which it links instead.

## A weave carries its manual

`zengine_weave(<target> <source> MANUAL <page>)` builds a weave carrying its page;
`zengine_manual(<target> <PRIVATE|INTERFACE> <page>)` does it for any target, such as the vocabulary
of an office another program mounts. Both split the page with the one grammar in
[`cmake/ZengineManual.cmake`](../../cmake/ZengineManual.cmake) when the build is configured, refuse
a page out of form with every reason and its line, and write `manuals/<office>.hpp` beside the
build, rewritten when the page changes. It defines `zengine::manual::pages::<office>::kManual`
(`zengine_builder_pane` for `zengine.builder-pane`), a `zengine::manual::Manual`: the office, the
name, the page's content id (the SHA-256 of its bytes with line endings as LF) and its sections,
each a key, the `##` heading it stands under and its words. The words are compiled in as numbers,
so no character of the page meets a compiler's source encoding.

An edit to a manual is therefore an edit to the weave that carries it: the documentation lane's
change check calls a page `<folder>/docs/<office>.md` compiled, by its name and place, and the
official lane verifies it.

## A pane answers its manual

A pane answers `PaneDocumentRequested` v1 `{pane, section}`, sent to its office, to the asker alone
with `PaneDocumentShown` v1: the pane's key and the section asked, the page's content id, and the
sections, `PaneDocumentSection` v1 `{key, group, text}`, in page order -- the one under `section`, or
the whole page when `section` is empty. `workshop::pane_document(ask, pane, kManual)` is the whole
answer, so a pane answers in one line:

```cpp
void on(const ws::PaneDocumentRequested& ask, loom::Mail& mail) {
    (void)mail.answer(ws::pane_document(ask, kMyPane, manual::pages::my_office::kManual));
}
```

- A pane the office does not offer, or a section its page does not have, is refused in `refusal`,
  naming what is missing, with no sections.
- Nothing holds the answer: not Workshop, not the host. An image answers from the page it was built
  with, so a pane reloaded in place answers its new words, and none of its predecessor's.
- The words describe and decide nothing: they are not a grant, and nothing waits on them.

## An office answers for a shape

An office that carries its page answers `ShapeDocumentRequested` v1 `{shape, version, content_id}`
to the asker alone with `ShapeDocumentShown` v1: the page's section for that shape, its group, the
page's content id, and the shape's content id as the office accepts it (`0x` and sixteen hex
digits, as `tests/shapes.txt` spells one). `manual::shape_document` answers it from the office's own
accepted list, so the page describes exactly the shapes the gate lets in.

- The asker names the content id it holds, or none. Another id is another definition of the shape:
  the words are answered, `current` is false, and `said` says they are out of date for it.
- A shape the office does not accept under that name and version, or one its page has no section
  for, is refused in `refusal`, with no words.

## How a manual is kept true

| check | lane | refuses |
|---|---|---|
| the build itself | official | a page out of form: the title, the order of its headings, a command without its four lines, a section past its bound, a key twice |
| `manual` ([`tests/check_manual.cmake`](../../tests/check_manual.cmake)) | documentation | the same form; a manual named other than its office, or carried by no target; a pane's manual whose commands are not exactly the ids the action census holds for its office; a key marker, in a manual or a tutorial, whose key is not a default the census holds for its id |
| the action census (`tests/actions.txt`) | official | a shipped pane whose declared rows, in every mode its suite drives, differ from the committed census; the census as it stands is written beside the build to diff |
| the manual witnesses | official | a pane whose answered manual lacks a command it declares; an office whose accepted shapes its page does not describe |
| `doc_links` | documentation | a broken link or anchor |
