# zengine.powers -- the discovery door

**Manual.** The office where a tool finds a power, answering to `zengine.powers` (an office is the
name a weave answers to): which powers this program resolves, what each is for, and whose
contribution satisfies it. Workshop mounts it in its own process before it performs your project's
plan; it is not a file you load. The `Powers` pane of
[`zengine.introspection`](zengine.introspection.md#powers), Flow's search line and the Terminal all
ask it, so one question gets the same rows wherever it is asked.

## About

A *power* is an operator the program can spend, known by its identity (`math.max`). Providers
*contribute* powers to the host's catalog, and an overlay's contribution covers the one beneath it:
the contribution *in force* is the one an evaluation spends. The door reads the catalog at the
moment you ask and keeps nothing between asks, so a provider mounted since your last ask is in the
next answer, in its own words. It describes and never runs a power: sampling a Source is
[`zengine.sources`](zengine.sources.md)'s. It publishes nothing, and its grant is its answers.

A row is the contribution in force, read off its definition: `identity`; `provider`, empty when the
host published it itself; `kind`, `source` (it takes no input of yours), `conversion` (one version
of a shape in, another out) or `operator`; `construction`, `native` or `composite`; `offered`, false
for a participant's own reaction that it offers to no one else's composition; `about`, what it is
for in its contributor's words; `signature`, in Loom's spelling; and the schemas of its `inputs` and
`outputs`. A power's words describe it and decide nothing.

The fold is a *form*, not in the catalog: the evaluator spends it with an operator reference as its
body. It answers a search with text its name or words match, or one for the kind `form`, after the
catalog's rows: `fold`, kind `form`, construction `evaluator`, no provider.

Anyone who can be answered may ask: an office, or a participant speaking for itself, as the
Terminal does. A send with no sender has nobody to answer; the door counts it (`unanswered`).

## Use

Ask [`FindPowers`](#findpowers) for the powers that fit a search, page by page, and
[`DescribePower`](#describepower) for one power's whole stack. Each answer comes to you alone.

## Commands

### `FindPowers`

`FindPowers` v2 finds the powers that fit every field given; v1, which has every field but `fits`,
is answered the same way.

- **Takes:** every field optional. `text`: each word an ASCII case-insensitive part of the identity
  or of what it is for. `takes` / `yields`: an input / output carries exactly this type, spelled as
  Loom spells it, `Message(zengine.ProjectAnchor v1)`. `provider`: who contributed the one in force.
  `kind`: `source`, `operator` (conversions too), `conversion` or `form`. `construction`: `native`,
  `composite` or `evaluator`. `offered`. `after`: the rows after this identity, from `next`.
  `limit`: rows a page, 1 to `kMaxPowerRows`. `fits`: `fold`, a fold could spend it as its body.
- **Answers:** `PowersFound` v1: `ok`; `rows` in the catalog's order, filtered and never ranked;
  `next`, the last row's identity when more remain; `total`, how many fit across every page; and
  `powers` and `providers`, the whole catalog's counts.
- **Refuses:** `ok` false and `reason`: `a query carries at most <kMaxPowersQueryBytes> bytes of
  text; this one carries <n>`; `a page holds 1 to <kMaxPowerRows> rows; 500 were asked for`; `a kind
  is source, operator, conversion or form; 'sources' is none of them`; `a construction is native,
  composite or evaluator; '<word>' is none of them`; `a fit is fold; '<word>' is not one`; `these
  <n> rows come to <bytes> bytes and an answer carries at most <kMaxPowersAnswerBytes>; ask for
  fewer with limit`. The bounds are [`workshop/powers_vocabulary.hpp`](../../workshop/powers_vocabulary.hpp)'s.
- **Example:** at the Terminal, `ask @zengine.powers FindPowers 1 text=larger` finds `math.max`;
  `ask @zengine.powers FindPowers 2 fits=fold yields=Int`.

### `DescribePower`

`DescribePower` v1 describes one power: its contribution in force, and every contribution eligible
to satisfy it.

- **Takes:** `identity`.
- **Answers:** `PowerDescribed` v1: `ok`, `identity`, `row` (as `FindPowers` rows it), `stack`
  (each contribution's `provider`, `construction`, `offered` and `about`, the one in force last),
  and `contribution`, the one in force as `zengine.OperatorContribution` bytes.
- **Refuses:** `ok` false and `reason`: `nothing supplies 'math.max' here`; `'math.max' cannot be
  written as a contribution: <why>`; `'math.max' comes to <bytes> bytes and an answer carries at
  most <kMaxPowersAnswerBytes>`; an identity past the query bound, as `FindPowers` says it.
- **Example:** `ask @zengine.powers DescribePower 1 identity=math.max`.

## See also

- [`zengine.introspection`](zengine.introspection.md), whose `Powers` pane asks both.
- The shapes and bounds: [`workshop/powers_vocabulary.hpp`](../../workshop/powers_vocabulary.hpp).
- [Sources and Operators](../../operator/docs/operator-sources.md), and
  [the fold](../../operator/docs/operator-providers.md#the-fold).
- [`zengine.sources`](zengine.sources.md), the one office that may run a Source.
