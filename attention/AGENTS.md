# `attention/` — Attention: working on it

The router for `attention/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `CMakeLists.txt` | target `zengine-attention-pane`, a loadable weave; needs `loom::kernel` |
| `pane.cpp` | the weave: offers the one `attention` pane, hears `StandingConditions`, says rows |
| `vocabulary.hpp` | office `zengine.attention`, pane key, stem, action ids, `AttentionPaneState` |
| `docs/attention.md` | the weaver's page; its picture is under `docs/images/` |

## Law

- [attention](../agents/workshop/attention.md) `WL-ATTN` — a thing that happened and a thing that is
  true are two surfaces; witnessed by `workshop_host`, then `workshop_panes` (attention, seam,
  canvas)

## Suites

No entry of its own. Its cases in shared suites: `workshop_panes`
(`tests/test_workshop_panes_attention.cpp`, the loaded pane against the host's real publication).
The conditions it shows are the host's, witnessed in `workshop_host`
(`tests/test_workshop_host.cpp`).
The rig in `tests/workshop_support.hpp` resolves its stem to `WORKSHOP_SO_ATTENTION_PANE`; its stem
and `attention/pane.cpp` are also sample artifact and source in `workshop_load`
(`tests/test_workshop_load.cpp`) and `workshop_files` (`tests/test_workshop_files.cpp`).

## Host side

- Plan rows: `workshop/default-load-plan.json`, `workshop/graphical-load-plan.json` (optional,
  office `zengine.attention`); staged beside the host and listed as a development pane in
  `workshop/CMakeLists.txt`.
- The seam it hears: `workshop/attention_seam_vocabulary.hpp` (`StandingConditions`,
  `attention_glance`); Workshop's grant to say it, `workshop/grant.cpp`; counted as the desk a
  guest reads with `capture` in `workshop/guests.cpp`.
- The conditions: `workshop/attention.hpp`, `workshop/screen_attention.cpp`, published by
  `WorkshopWeave::say_conditions` in `workshop/weave_run.cpp`.
- The pane protocol it speaks: `workshop/pane_vocabulary.hpp`, with `workshop/pane_text.hpp`,
  `pane_parts.hpp`, `pane_menu.hpp` and `pane_canvas_rows.hpp`.

## Pages

- [What needs your attention](docs/attention.md)

## Practices

- [Attention](../docs/contributing/best-practices.md#attention-attention)
