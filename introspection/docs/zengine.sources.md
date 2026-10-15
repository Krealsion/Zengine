# zengine.sources -- the sample door

**Manual.** The one office that may run a Source, answering to `zengine.sources` (an office is the
name a weave answers to): asked for one Source by its identity, it runs it once, now, and answers
with what it returned, as lines of text. Workshop mounts it in its own process; it is not a file you
load. The `Powers` pane of [`zengine.introspection`](zengine.introspection.md#powerssample) asks it
when you sample.

## About

A *Source* is a power that takes no input of yours: it answers with nothing supplied. Which office
can cause a power to run has a one-word answer, this one: the arrangement and discovery doors
describe and run nothing, and this door runs one Source an ask and describes nothing.

It resolves the identity in the host's catalog when it acts, so a power gone since you read its row
is refused in the catalog's words. It remembers nothing: no definition, no previous answer, so two
asks run the Source twice and may answer differently. The value is rendered to lines here, where its
shape is known, and only the lines cross: the asker receives a picture of the value, never the value.

The lines: the shape's name and version, `zengine.ProjectAnchor v1`, then a line a field, indented
for each level of nesting, a number as its digits and text quoted (`"6"` is text, `6` a number);
bytes as `(bytes, 12 octets)`, a field absent as `(absent)`. Depth, a list's length and the lines
are bounded, and each cut says so: `(deeper structure not shown)`, `... 3 more`, `... (... more,
not shown)`.

Only an office may ask: an ask authored as no office, personal speech or a root's send, runs nothing
and is not answered; the door counts it (`refused`). The rule names nobody, so a new tool asks with
no edit here. A host that mounts no such door holds no `zengine.sources`, and an ask reaches nobody.

## Use

Ask [`SampleRequested`](#samplerequested) as an office you hold, once for each sample you mean: no
timer, subscription or cache stands behind it.

## Commands

### `SampleRequested`

`SampleRequested` v1 runs one Source, now.

- **Takes:** `identity`, the Source's.
- **Answers:** to the asker alone, `SourceSampled` v1: `identity`, echoed; `ok`; `lines`, the value
  rendered (above); or, when it was refused, `reason`, in the words of whichever layer refused. The
  door counts each ask it acts on (`spent`) and each refusal (`refusals`).
- **Refuses:** in `reason`: `unresolved operator reference 'zengine.project.anchor'`, when nothing
  supplies it; `'math.max' is an operator and not a source: it declares 2 inputs (lhs, rhs), which
  sampling supplies none of`; and the catalog's own refusal of the run, quoted. An ask authored as no
  office is not answered.
- **Example:** from a weave holding `zengine.introspection`,
  `mail.as_role("zengine.introspection").send_to_role("zengine.sources", SampleRequested{"zengine.project.anchor"}, 9);`,
  answered `zengine.ProjectAnchor v1` and `  anchor  "/home/weaver/projects/zen"`.

## See also

- [`zengine.introspection`](zengine.introspection.md#powerssample), the pane that samples.
- [`zengine.powers`](zengine.powers.md), which says what a Source would yield without running it.
- The shapes: [`workshop/sample_vocabulary.hpp`](../../workshop/sample_vocabulary.hpp).
- [Sources](../../operator/docs/operator-sources.md).
