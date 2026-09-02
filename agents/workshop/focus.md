# Workshop law — focus

Register `WL-FOCUS`: the keyboard goes where the maker last pressed. One law per heading; cite
by ID. Router: [`../workshop.md`](../workshop.md). What a key looks like when it crosses the pane
seam is the protocol's law, in [`../panes.md`](../panes.md).

## WL-FOCUS-01 — `Panels::keyboard` is a pointing's memory

LAW — `Panels::keyboard` is the keyboard-taking pane the maker last aimed the keys at; `keyboard_pane(panels)` is the external answer, resolved fresh at every spend: open, runtime kind, room granted.

MEANS
- `editor_has_keyboard`, `files_has_keyboard`, `pane_editor_has_keyboard` are the built-ins';
- a pane that stops being presentable stops being typed into, with nothing to clear.

PROVEN BY — `workshop/panel.hpp` `keyboard`, `keyboard_pane`; `workshop/screen.hpp`
`editor_has_keyboard`, `files_has_keyboard`, `pane_editor_has_keyboard`; `workshop/weave.hpp`
`keyboard_pane`; `tests/test_workshop_panes_input.cpp` case `"MSG-0: a press into an external
pane's room points the keyboard at it"`, case `"MSG-0: a press into a second external pane moves
the keyboard to it"`, case `"MSG-0: a pane that stops being presentable stops being typed into"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-02 — Candidacy is declared; readiness is resolved

LAW — `PanelKind::takes_keyboard` is a fact about a kind on its catalog row; whether that pane can take keys at this instant is live state its own resolver answers, stored nowhere.

MEANS
- the Editor needs a document open and Project Files needs a listing;
- Editor, Files and the Pane Manager carry the flag; nothing registered, no focus framework.

PROVEN BY — `workshop/panel.hpp` `takes_keyboard`, `kind_takes_keyboard`, `kPanelCatalog`;
`workshop/screen.hpp` `editor_has_keyboard`, `files_has_keyboard`;
`tests/test_workshop_editor.cpp` case `"EDIT-0: an empty editor pane takes no keys and says how
to fill itself"`; `tests/test_workshop_files.cpp` case `"EDIT-1: with no origin the pane refuses
in words and guesses nothing"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-03 — One reading decides both

LAW — The pressed branch reads `selected = occupied ? kind : none`, then `keyboard = kind_takes_keyboard(selected) ? selected : none`; other built-ins clear the candidate and keep the selection.

MEANS
- the prior answer is read one line above, because Project Files' press rule needs it;
- putting the line in the routing arms would be four decisions about one fact.

PROVEN BY — `workshop/weave.hpp` `kind_takes_keyboard`; `workshop/screen.hpp` `occupied_at`,
`info_body_at`; `tests/test_workshop_panes_input.cpp` case `"MSG-0: a press anywhere else takes
the keyboard away again"`; `tests/test_workshop_files.cpp` case `"EDIT-1: the first press into a
cold pane selects and never activates"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-04 — The press that points the keys is not an act in the pane

LAW — Project Files activates a row on a press only when the pane already held the keyboard, so two presses from cold is the price of "no single press replaces what is open".

PROVEN BY — `workshop/weave.hpp` `files_open`; `workshop/screen.hpp` `files_has_keyboard`;
`tests/test_workshop_files.cpp` case `"EDIT-1: the first press into a cold pane selects and never
activates"`, case `"EDIT-1: a press selects the row the paint put under the pointer"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-05 — The candidate is never cleared and the target is never stored

LAW — A pane that closes, stops resolving or loses its room stops being the answer with nothing to clear, and comes back with its keyboard; the two pointings that write none are pointings.

MEANS
- a press on nothing, and Escape's final fallthrough, say "nowhere" — not a clearing path.

PROVEN BY — `workshop/panel.hpp` `keyboard_pane`; `workshop/weave.hpp` `unselect_pane`;
`tests/test_workshop_panes_input.cpp` case `"MSG-0: a pane that stops being presentable stops
being typed into"`, case `"MSG-0: a pane with no room granted is not typed into"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-06 — The modes above it never reach that line

LAW — The priority is the above-mode classes, the modes, a focused pane, the editor holding a document, a live property draft, then `command()`, spelled once in `keyboard_context`.

MEANS
- opening the Terminal or an arrangement scope leaves the candidate where it was;
- a focused pane sits above a live draft: both are places pressed into, so the last one answers;
- pressing back into the Info body clears the candidate by the same line that set it.

PROVEN BY — `workshop/screen.hpp` `keyboard_context`, `keyboard_context_beneath_menu`;
`workshop/keymap.hpp` `above_mode_action`; `tests/test_workshop_panes_input.cpp` case `"MSG-0:
every Workshop mode owns the keyboard above a focused pane"`, case `"MSG-0: the keys that mean
the same thing in every mode still outrank a pane"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-08 — The pane gets every bare key, `q` included

LAW — The global survivors are chorded, and admission's refusal of a bare printable on a global row enforces it, which is why typing `p` into a field does not open the picker.

PROVEN BY — `workshop/keymap.hpp` `kGlobal`; `workshop/keymap_persist.hpp` `from_text`;
`tests/test_workshop_panes_input.cpp` case `"MSG-0: typing `p` into a focused pane does not open
the picker"`; `tests/test_workshop_document.cpp` case `"KEY-0: a global action cannot take a
bare printable or the editing vocabulary"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-09 — `^c` follows the keyboard

LAW — `^c` quits exactly where nothing takes text and travels the chain wherever text has the keyboard; the gate is `context_takes_text(keyboard_context(...))`; copy nothing is still consumed.

MEANS
- the Terminal line, the name editor, a live draft, and a focused runtime pane all receive it;
- quit stays one press-elsewhere away, or `q`, or the close box.

PROVEN BY — `workshop/keymap.hpp` `context_takes_text`; `workshop/screen.hpp`
`keyboard_context`; `tests/test_workshop_document.cpp` case `"TEXT-0: ^c still quits exactly where
nothing takes text"`; `tests/test_workshop_editor.cpp` case `"EDIT-0: ^c in the editor copies --
it does not quit -- and quit stays a press away"`; `tests/test_component.cpp` case `"component:
a consumed gesture that changes nothing is still consumed"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-10 — The screen says where typing goes, in two places, in characters

LAW — The pane with the keys wears the `> ` mark in its header, and the band's first legend row says `typing goes to <name> @<office> -- press elsewhere for Workshop's keys`, its chords from the keymap.

PROVEN BY — `workshop/screen.hpp` `external_header`, `kTypingHere`, `band_region`;
`workshop/panel.hpp` `keyboard_pane`; `tests/test_workshop_panes_input.cpp` case `"MSG-0: the
screen says which pane the keys are going to, in two places"`; `tests/test_workshop_editor.cpp`
case `"EDIT-0: the band and the header both say where typing goes"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## WL-FOCUS-11 — Pane titles are a presentation preference with a key

LAW — Pane titles are a preference with a key: the toggle flips the session's flag and writes the prefs file; one resolution answers a pane's header rows, and the keyboard's pane always keeps its title.

MEANS
- painter, press path and room grant spend the answer through `ExternalBodyPlace::header_rows`;
- a hidden title returns its row to the provider through the ordinary grant-on-change door.

PROVEN BY — `workshop/keymap.hpp` `workshop.pane-titles`; `workshop/screen.hpp` `pane_titles`,
`external_title_rows`, `header_rows`; `workshop/prefs_persist.hpp` `kTitlesDefaultValue`,
`kTitlesDefault`; `workshop/weave.hpp` `prefs_path`, `load_prefs`, `prefs_loaded_`, `prefs_bad_`;
`tests/test_workshop_document.cpp` case `"WUX-1/SC-5: pane titles are one action, one binding
truth, one dispatch"`, case `"WUX-1/SC-5+SC-6: hiding titles returns the row; the keyboard's pane
keeps its own"`; `tests/test_workshop_persistence.cpp` case `"WUX-3: a toggle writes the
preference, and a reopened Workshop wears it"`.
WHY — `agents/decisions/the-keys-go-where-last-pressed.md`

## Do not assume

- That hiding pane titles can hide where typing goes — the keyboard's pane keeps its title and
  its mark whatever the preference says (WL-FOCUS-11).
- That a focused pane above a live draft has its own witness — it does not (WL-FOCUS-06).
