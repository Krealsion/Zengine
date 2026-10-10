# `maker/` — The maker weave: working on it

The router for `maker/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `CMakeLists.txt` | `zengine-maker` (`zengine::maker`): header-only, not a loadable weave |
| `definition.hpp` | `Definition`, `On`, `Conversion`; `admit_definition`, `read_definition` |
| `weave.hpp` | the interpreter: `Weave`, `register_definition`, `apply_behaviour_edit` |
| `runtime.hpp` | `Runtime`: delivery, spend, emit and pokes, shared with Flow's generated weaves |
| `succession.hpp` | `Succession`, `register_succession`, `begin_schema_edit`: the schema edit |
| `vocabulary.hpp` | the five ceremony shapes, `zengine.maker.Quiesce` to `Adopted` |
| `write.hpp` | `plan_fields`, `write_fields`, `default_value`, `pack`: the field-wise write |
| `files.hpp` | `read_file`, `write_file`: the two files on disk, size-capped, renamed into place |
| `docs/maker-weave.md` | the reference page |

## Law

- [maker](../agents/maker.md) — maker (router)
- [definition](../agents/maker/definition.md) `MW-DEF` — the two artifacts a maker weave is made of,
  and what admits one
- [succession](../agents/maker/succession.md) `MW-SUCC` — a schema edit as a prepared replacement
  with an authored conversion
- [weave](../agents/maker/weave.md) — the weave

## Suites

- `maker` — one weave registered from data behaves as a native one, is edited live and survives
  a process as two files: `tests/test_maker.cpp`, with `tests/maker_author.cpp` as the fresh process

The high-water fixture, `tests/maker_fixture.hpp`, is shared with the `flow`
(`tests/test_flow.cpp`, `tests/flow_generate.cpp`), `view` (`tests/test_view.cpp`) and
`view_builder` (`tests/test_view_builder.cpp`) suites. Outside CTest,
`tests/package/public_surface.cpp` and `tests/package/flow_host.cpp` build against the installed
headers.

## Host side

- No Workshop host of its own: Workshop authors, runs and edits a maker weave through Flow.
- `cmake/ZengineInstall.cmake` installs its headers and exports `zengine::maker`.
- `flow/CMakeLists.txt` links it into `zengine-flow`; `flow/flow-host/runtime.hpp` registers one
  maker weave per session; `flow/native.hpp` `NativeWeave` builds on `Runtime`.
- `files.hpp` alone: `view/description.hpp`, `message-draft/library.hpp`,
  `inventory/inventory-pane/toolbox_file.hpp`; `view/CMakeLists.txt` links `zengine-maker`.
- `files.hpp` restates `workshop/persist.hpp`'s read and write; nothing here includes `workshop/`.

## Pages

- [The maker weave — a weave from a definition](docs/maker-weave.md)

## Practices

- [The maker weave](../docs/contributing/best-practices.md#the-maker-weave-maker)
