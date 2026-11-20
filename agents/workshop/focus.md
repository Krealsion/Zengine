# Workshop law — focus

Register `WL-FOCUS`: the keyboard goes where the weaver last pressed. One law per heading; cite
by ID. Router: [`../workshop.md`](../workshop.md). What a key looks like when it crosses the pane
seam is the protocol's law, in [`../panes.md`](../panes.md).

Retired: WL-FOCUS-01, WL-FOCUS-02, WL-FOCUS-03, WL-FOCUS-04, WL-FOCUS-05, WL-FOCUS-06, WL-FOCUS-09, WL-FOCUS-10, WL-FOCUS-11, WL-FOCUS-12, WL-FOCUS-13

## WL-FOCUS-08 — The pane gets every bare key, `q` included

LAW — The survivors above a pane are chorded, and admission's refusal of a bare printable on a row above every mode enforces it, which is why typing `a` into a field does not open the contextual surface.

WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## Do not assume

- That hiding pane titles can hide where typing goes — the keyboard's pane wears its title and
  its mark whatever the preference says, once a press holding its room ends.
- That the pane the keys point at is where an ordinary key goes — a mode above it takes the
  key, and a press there is reported "not here".
- That a focused pane above a live draft has its own witness — it does not.
- That Escape always leaves a pane — a pane may keep it in a mode of its own; `focus.leave` is
  the way out no pane keeps.

## Proven by

### WL-FOCUS-08
`workshop/keymap.hpp` KeyContext::kGlobal
`workshop/keymap_join.hpp` join_app_rows
`workshop/keymap_persist.hpp` from_text
`tests/test_workshop_panes_input.cpp` "typing `a` into a focused pane does not open the contextual
surface"
`tests/test_workshop_panes_actions.cpp` "an application row answered above every mode cannot take a
bare printable or a chord the text box owns, whoever wrote it"
