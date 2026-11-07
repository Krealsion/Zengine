# Workshop law — how a theme applies

Register `WL-THEME`: a theme the settings office applies, judged whole first, then written by the
layouts' and the keymap's doors and offered as the palette a layout wears. One law per heading; cite
by ID. Router: [`../workshop.md`](../workshop.md). Which palette an area wears is
[`settings.md`](settings.md)'s WL-SETTING-13.

## WL-THEME-01 — A theme is judged whole before anything is written

LAW — A theme is judged whole before anything is written: an entry nothing takes is skipped and said, or refuses it, as its `on_failure` says; a value of the wrong kind refuses it always.

MEANS
- a setting is judged against the declarations the layouts' owner shows, a key against the keymap's actions, a colour, font or size against the pool, a slot against its office's declaration;
- slots of an office the load plan names that has not declared wait, and apply when it declares; an entry its declaration does not hold is then skipped and said on the band;
- a version-1 theme file reads as version 2: `refuse` on a failure, each key one gesture.

DOES NOT MEAN
- that a launch applies a theme: a file edited between two runs changes nothing until applied, though each theme's own palette is offered as it stands.

WHY — `agents/decisions/a-colour-is-a-name-the-window-paints.md`

## WL-THEME-02 — Merge applies what a theme names, reset returns the rest, locks stand

LAW — `merge` applies what a theme names and leaves the rest; `reset` first returns the rest of what it touches to its default; a locked setting stands as the final layer and is listed in the answer.

MEANS
- its settings and baselines go through the layouts' batch door at the revision read, its keys through the keymap's edit door: the first gesture set, the rest added, `none` alone disabling;
- its look is offered as the palette the layout wears, at a higher revision, so every window repaints and no pane is asked;
- `block` worn over `night` or `day` is a `merge`: their colours stand, its outline and terminal words over them.

DOES NOT MEAN
- that colours carry locks: a weaver's own colours are a theme of the weaver's.

WHY — `agents/decisions/a-colour-is-a-name-the-window-paints.md`

## Proven by

### WL-THEME-01
`settings/apply.hpp` plan_apply judge_waiting
`settings/theme_persist.hpp` check_theme_color widened
`settings/weave.cpp` SettingsWeave
`tests/test_settings_apply.cpp` "an entry nothing takes is skipped and said where the theme says skip,
and refuses it whole where it says refuse" "a value of the wrong kind refuses the theme whatever
on_failure says, and a value its row does not take is nothing taking it" "a value that is no
colour, or a terminal word no terminal shows, refuses the theme whatever on_failure says" "an office
the plan names that has not declared its slots waits, and what it declares applies when it does" "a
version-1 theme file reads as version 2: refused on a failure, each key one gesture, and applies as
it would have"
`tests/test_workshop_desk.cpp` "a theme's entry nothing takes is skipped and said, or refuses it
whole naming it, and a value that is no colour refuses whatever it says" "a slot of an office the
plan names, not yet loaded, waits for it, and applies when it declares"

### WL-THEME-02
`settings/apply.hpp` plan_apply look_over default_palette
`settings/weave.cpp` SettingsWeave
`workshop/weave_run.cpp` WorkshopWeave::worn_theme
`tests/test_settings_apply.cpp` "merge writes what the theme names and leaves the rest; reset also
returns every other key of each pane it names to its default" "several gestures ask one action: the
first sets it and the rest are added, and `none` alone disables it" "a look over a palette sets what
it names: merge keeps the rest of what was worn, reset starts from the pool's defaults"
`tests/test_workshop_desk.cpp` "a theme edited on disk and applied changes the palette a window
paints with no build, and no pane hears of it" "merge leaves what a theme does not name and reset
returns it to its default, settings and colours alike" "a locked setting survives a theme as its
final layer and is listed in what the theme came to" "several gestures ask one action through the
keymap's door, and a version-1 theme file applies as it would have"
