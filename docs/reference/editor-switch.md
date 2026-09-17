# Editor switch

**Reference, current state.** The exact contract for switching the Editor's office between the
editors a load plan authors for it, and for the document that crosses: the plan's `choices`, the
four messages a maker sends, the answers, the handoff every editor speaks, and what crosses
exactly, what is reset, what needs consent and what is refused. The walkthrough is
[Neovim in Workshop](../workshop/neovim.md). The law is `WL-SWITCH` and `WL-NVIM`
(`agents/workshop/editor-switch.md`, `agents/workshop/neovim.md`).

## The choices a load plan authors

A load plan in format **version 2** may carry `choices`: the artifacts that may hold an office, each
under a name a maker switches to it by.

```json
"choices": [
  { "role": "zengine.editor", "name": "standard", "artifact": "zengine-editor-pane" },
  { "role": "zengine.editor", "name": "neovim", "artifact": "zengine-neovim-editor" }
]
```

| rule | what it means |
|---|---|
| one start holder | exactly one row of `artifacts` loads the role, and its artifact is one of the role's choices |
| loaded on demand | every other choice is loaded only when a switch asks for it |
| one spelling | a role names each choice once and each artifact once; an artifact is a choice for one role |
| a name is a word | a lowercase letter, then lowercase letters, digits and `-`; at most 32 bytes |
| weave only | a choice's start row asks for weave participation alone, never a provider |
| versions | a plan with no choices is written as version 1, byte for byte; a version-2 file with none is refused |

Both shipped plans author `standard` (starting) and `neovim` for `zengine.editor`. The load plan
format itself is [load-plan.md](load-plan.md).

## The office and its messages

The office `zengine.editor-switch` is mounted by the Workshop host. Every message is an ask, and
every ask is answered with `EditorSwitchAnswered`.

| message | fields | asks for |
|---|---|---|
| `EditorSwitchRequested` v1 | `destination` | switch to the choice with this name |
| `EditorSwitchConfirmed` v1 | `op`, `consent` | agree to the losses a switch named, by their digest |
| `EditorSwitchCancelled` v1 | `op` | stop a switch that has not committed |
| `EditorSwitchStatusRequested` v1 | — | which choice is active, which are authored, what is under way |

Typed in Workshop's Terminal pane, exactly:

```text
ask @zengine.editor-switch EditorSwitchRequested 1 destination=neovim
ask @zengine.editor-switch EditorSwitchConfirmed 1 op=3 consent=c4f2a91c
ask @zengine.editor-switch EditorSwitchCancelled 1 op=3
ask @zengine.editor-switch EditorSwitchStatusRequested 1
```

A consent always begins with a letter, so the Terminal reads it as text.

`EditorSwitchAnswered` v1 carries `op` (0 for an answer that began no switch), `outcome`, `active`
(the choice holding the office now), `destination`, `consent`, `detail` (in words), `losses`,
`resets`, `notes` and `choices` (every authored name).

| outcome | meaning |
|---|---|
| `switched` | the office moved and the document crossed; `resets` and `notes` say what did not cross exactly |
| `already-active` | the destination already holds the office; nothing was loaded |
| `needs-confirmation` | the switch would lose `losses`; nothing was loaded; confirm with `op` and `consent`, or cancel |
| `refused` | nothing moved; `detail` is the refusing party's own words |
| `cancelled` | a pending switch was stopped; nothing moved |
| `superseded` | a newer request replaced this switch while it awaited confirmation |
| `failed-after-commit` | the office moved and the successor did not prove it serves; the retired editor is kept, and the next switch releases it |
| `status` | the answer to a status request |

While a switch is under way the office publishes `EditorSwitchProgress` v1 (`op`, `destination`,
`stage`, `awaiting`, `pending`); Workshop keeps it as a standing condition in the Attention pane,
with the cancel line in its detail.

**One switch at a time, and no timeout.** A request while one is under way is refused in words. A
request while one only awaits confirmation replaces it. A participant that never answers leaves the
switch pending and cancellable. An orderly quit is refused while an editor holds still for a switch.

## The handoff

A switch replaces the weave holding the office through Loom's prepared replacement, and carries the
document through an authored handoff every editor speaks, as incumbent and as candidate:

| step | who | what |
|---|---|---|
| judge | incumbent | what a switch away would lose (for consent), reset and refuse; nothing is held or loaded |
| load | the office | the destination's artifact, sealed: it may speak only to the coordinator |
| warm | candidate | start what it needs, in the incumbent's room; the coordinator relays a beat while it starts |
| boundary | incumbent | author the exact document and hold still: no input is applied until the outcome, and what is refused is counted |
| adopt | candidate | Loom's preparation ask: adopt the document, or refuse it in words |
| commit | Loom | the admission, which is also the successor's activation; traffic queued behind it reaches the successor |
| prove | successor | say it serves; the retired editor is then unloaded |
| retire | retired | say how many inputs it refused while it held still |

Before the commitment, every ending moves nothing: the candidate is discarded and the incumbent is
told, resumes, and says so on its pane. After it, nothing is rolled back.

## What crosses

The transfer is the standard Editor's model: file bytes, the saved comparison, a line convention,
the modified flag, a caret and an anchor as (line, byte) with an exclusive selection end, and the
first visible line and column.

| state | standard → Neovim | Neovim → standard |
|---|---|---|
| bytes | adopted exactly: the file is loaded, its lines replaced with no undo step | exported exactly |
| line endings | LF or CRLF, set on the buffer | LF or CRLF; `mac` is **refused** |
| final newline | set on the buffer (`endofline`) | carried; a byte-order mark or `binary` is **refused** |
| unsaved edits | the modified flag carried | the saved comparison is the file on disk; a file not yet written counts as unsaved |
| caret | Normal mode on the next character; past a line's last byte, Insert mode there | Normal before the cursor character; Insert and Replace exact |
| selection | charwise Visual with both ends and its direction | charwise exact; linewise as the same lines; blockwise as its cursor (each said) |
| viewport | first line and column restored | first line; first column only when lines do not wrap |
| **adjusted, and said** | a caret after the final newline; a one-character backward selection | Neovim's mode other than Normal, Insert and Replace |
| **reset, and said** | the standard Editor's undo history | Neovim's undo history, registers, marks and jumplist; extra windows, tab pages and clean buffers; a macro being recorded; an unfinished count (cancelled with Escape) |
| **needs consent** | never | unsaved changes in other Neovim buffers; running terminal jobs |
| **refused** | an open, a relay or a paste in flight | a prompt; a buffer that is not a file; a modified buffer with no file name; an open or paste in flight |
| **refused by the standard Editor** | — | bytes outside printable ASCII and tab, mixed line endings, or more than 4 MiB, naming the line |

## The Neovim-backed choice

`zengine-neovim-editor` holds `zengine.editor` with the Editor's pane key. Its three asks, answered
as `zen.Result` or `zen.Refused`, are for a Loom with no Workshop:

| message | fields | does |
|---|---|---|
| `NeovimStartRequested` v1 | `path`, `listen` | start Neovim headless and listening (`listen` empty: an address this weave makes), editing `path` when named; answers the address and the line that attaches to it |
| `NeovimStatusRequested` v1 | — | which Neovim, how it is attached, the current buffer and whether it is modified |
| `NeovimStopRequested` v1 | `discard` | end Neovim; refused while a buffer holds unsaved changes, naming them, unless `discard` is true |

The header is `neovim-editor/vocabulary.hpp`, exported as `zengine::neovim`.

| environment | meaning |
|---|---|
| `ZENGINE_NEOVIM` | the Neovim program (default: `nvim` on the search path) |
| `ZENGINE_NEOVIM_PROFILE` | `clean` (default: no configuration, and Neovim's state kept apart under the application name `zengine-neovim-clean`), `user` (your own configuration), or the path of an init file |

Supported: Neovim 0.11 and newer (API level 13). Tested: 0.11.6 and 0.12.5. A newer release runs,
and says it is untested.
