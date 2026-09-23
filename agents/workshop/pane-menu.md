# Workshop law — a pane's menu and the participant that presents it

Register `WL-CTX`, the presenter's half: the menu a pane asks the host to present, the office
that shows it, reads it and answers it, and what the host does when no image can. The
contextual surface's own laws — pointing, the popup, the catalog, spending, the mode — are in
[`contextual.md`](contextual.md), and the ids are one series. One law per heading; cite by ID.

## WL-CTX-09 — A pane's menu is judged where it opens and handed to the presenter

LAW — A pane's menu request is judged where it would open and granted to the presenter's office; it moves no keys or selection, and the host answers only asks it refused or no presenter can.

MEANS
- a continuation of one gesture: a secondary press, a declared key, or a PRIMARY press;
- one at a time: a newer menu, a right press, the host's menu, the pane leaving withdraw it;
- the host keeps custody and place -- forwards acts, draws lines in the room -- and no row.

DOES NOT MEAN
- that the host performs a pane's operation, or that a row grants one: the requester acts;
- that anything is restored after: a choice may continue to `manage...` or the keyboard, once.

PROVEN BY — `workshop/pane_vocabulary.hpp` `PaneMenuRow`, `PaneMenuRequested`,
`PaneMenuAnswered`, `PaneManageRequested`, `PaneKeyboardRequested`, `kPresenterRole`;
`workshop/presenter_vocabulary.hpp` `MenuGranted`, `MenuShown`, `MenuInput`, `MenuClosed`,
`MenuWithdrawn`; `workshop/context.hpp` `PresentedMenu`; `workshop/weave.hpp`
`grant_menu`, `press_sent_`, `withdraw_menu`, `end_menu_unanswered`,
`cell_of_body_place`, `ChoiceAnswered`, `action_sent_`;
`workshop/weave_external.cpp` `on(PaneMenuRequested)`, `grant_menu`, `withdraw_menu`,
`end_menu_unanswered`, `about_open_menu`, `forward_menu_input`, `menu_key`, `menu_button`,
`on(MenuShown)`, `on(MenuClosed)`, `on(PresenterReady)`, `on(PaneManageRequested)`,
`on(PaneKeyboardRequested)`, `cell_of_body_place`; `workshop/weave_pointer.cpp` `context_key`;
`workshop/screen.hpp` `presented_room`, `presented_bounds`, `paint_presented`, `PresentedPressAt`,
`presented_press_at`; `workshop/screen_attention.cpp` `presented_room`, `presented_bounds`,
`paint_presented`, `presented_press_at`; `tests/test_workshop_panes_desktop.cpp` case
`"WL-CTX-09: a printable menu shortcut and the text its own key produced are one gesture, so the
menu opens"`; `tests/test_workshop_panes_button.cpp` case `"WL-CTX-09: a menu requested on the
press's own turn is granted to the presenter and opens beside the press with the pane's rows,
moves no keys and no selection, and Return returns the first row -- answered by the presenter"`,
case `"WL-CTX-09: the keyboard works the menu -- Down then Return chooses the second row; Escape
answers it unchosen; the release under it still reaches the pane"`, case `"WL-CTX-09: an outside
press dismisses the menu, is spent on dismissing, and reaches nothing beneath it"`, case
`"WL-CTX-09: a press on a presented row chooses it"`, case `"WL-CTX-09: a late request is refused
where the menu opens -- a newer primary press elsewhere keeps the keys it took; the review's
second integration finding"`, case `"WL-CTX-09: an offer echoing no gesture and one from an office
that never offered the pane are refused or dropped by the host; an empty offer is the presenter's
to refuse, and it spends the gesture"`, case `"WL-CTX-09: a newer menu replaces an open one, which
its presenter answers unchosen in the host's words; one menu at a time"`, case `"WL-CTX-09: a pane
that leaves the desk while its menu is open has it withdrawn, answered unchosen; a pane that
consumes the press opens nothing"`, case `"WL-CTX-09: a menu opened by a declared key continues
that keystroke -- eligible on its turn, refused after a newer key, and anchored in the pane's own
body"`, case `"WL-CTX-09: a chosen row may continue into the host's own pane menu on a subject the
pane names -- once, while the choice is the maker's latest act, and never for a pane the inventory
lacks"`, case `"WL-CTX-09: a chosen row that begins an edit may take the keyboard -- once, while
the choice is the maker's latest act, and never under another number"`;
`workshop/pane_menu.hpp` `take_keyboard`, `take_keyboard_continuing`; `files/files.cpp`
`take_keys`; `builder-pane/pane.cpp` `take_keys`; `tests/test_workshop_panes_files.cpp` case
`"a Files menu choice that begins authoring takes the keyboard the menu left behind"`, case `"a
Files menu choice that opens no edit leaves the keyboard where the maker put it"`;
`tests/test_workshop_panes_builder.cpp` case `"BLD-MOUSE: a Builder menu choice that opens the
role line takes the keyboard across the door it waits on"`, case `"BLD-MOUSE: a Builder menu
choice that opens no line leaves the keyboard where the maker put it"`.
WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-10 — The presenter owns a menu's showing and lifetime; replacing it is ordinary

LAW — The presenter's office shows a granted menu, reads its acts, ends it and answers once; a reload keeping `HeldMenu` hands an open menu over, and an interaction no image holds goes back to the host.

MEANS
- the shipped presenter chooses on the press; the numbered example on a digit or the release.
Both refuse labels over 64 bytes before fitting accepted labels to the available display width;
- a requester takes a choice only from this office, under its own ask's number, once (`Asked`);
- gone, not carrying it, or giving it back (`MenuReturned`): the host answers, open or withdrawn.

DOES NOT MEAN
- that a give-back reaches a newer menu or one answered: `MenuClosed` retired that record;
- that every later participant must hand over or cancel alike: it is these consumers' policy.

PROVEN BY — `workshop/presenter_vocabulary.hpp` `HeldMenu`, `PresenterReady`, `MenuClosed`,
`MenuReturned`, `kMaxMenuLines`; `menu-presenter/presenter.cpp` `MenuPresenter`, `refusal_of`,
`MenuPresenter::give_back`, `drawable`;
`examples/numbered-presenter/presenter.cpp` `NumberedPresenter`,
`digit_row`, `line_for`, `NumberedPresenter::give_back`;
`workshop/pane_menu.hpp` `Asked`, `Asked::take`, `Offer`; `desktop-pane/pane.cpp`
`launcher_asked_`, `keys_asked_`; `workshop/weave_external.cpp` `on(PresenterReady)`,
`answer_withdrawn`, `end_refused_menu`, `on(WithdrawalFence)`, `on(MenuReturned)`,
`forget_withdrawn`; `workshop/context.hpp`
`WithdrawnMenu`, `PresentedMenu::first_attempt`; `workshop/vocabulary.hpp` `WithdrawalFence`;
`workshop/weave.hpp` `withdrawn_`; `workshop/weave_code.cpp` `on(DispatchRefused)`;
`workshop/weave_seam.cpp` `on(PaneOffered)`; `tests/test_workshop_panes_button.cpp` case
`"WL-CTX-09: a menu with more rows than the room is windowed by the presenter, and every row is
still reachable"`, case `"WL-CTX-10: a requester's record of its ask settles on the presenter's
answer or the host's refusal -- once, and on nothing another office says under its number"`, case
`"WL-CTX-10: a withdrawal the presenter cannot receive is answered by the host, once, under the
ask's number"`, case `"WL-CTX-10: a refused act settles its withdrawn menu, though the
withdrawal reaches a fresh presenter that cannot answer it"`, case `"WL-CTX-10: an older menu's
refused withdrawal ends and answers nothing newer; the newer menu is shown and chooses"`, case
`"WL-CTX-10: a withdrawal that queues nothing is answered at once, and nothing is kept"`, case
`"WL-CTX-10: a refusal notice anyone could send settles no menu, open or withdrawn"`, case
`"WL-CTX-10: a withdrawn menu's record is forgotten when its fence comes round twice, and
ordinary use keeps none"`, case `"WL-CTX-10: a withdrawal a fresh holder does not carry is given
back, and the host settles who asked"`, case `"WL-CTX-10: an act reaching an image that refused
the grant is given back and takes nothing"`, case `"WL-CTX-10: a menu its image answered is not
reopened by a give-back behind it"`, case `"WL-CTX-10: a give-back is one office's word about one
menu, and settles no other"`;
`tests/test_workshop_panes_desktop.cpp` case
`"WL-CTX-10: a reloaded desktop cancels its predecessor's menu -- withdrawn when the successor
offers its pane again, and a choice about it acts on nothing"`, case `"WL-CTX-10: the desktop acts
only on the presenter's answer to an ask of this image's own -- not the host's choice, not another
subject, not another number, not a predecessor's, and not twice"`, case `"WL-CTX-10: an ordinary
replacement presenter holds the office -- the Pane Manager and Hotkeys keep their own operations
while it presents their menus its way: numbered, a digit chooses, a release chooses"`, case
`"WL-CTX-10: the replacement presenter presents the Pane Manager's menu too, and a digit opens the
row -- through the desktop's own launch"`, case `"WL-CTX-10: the presenter reloaded in place by
another image while a menu is open hands the menu over -- shown again the new way with its cursor,
answered under the same number, and the Hotkeys edit completes"`, case `"WL-CTX-10: a presenter
that leaves ends its menu, answered by the host; with none in the office a menu is refused in
words and the host's own menu still opens; one loaded afresh does not carry the old menu, which
ends answered"`.
WHY — `agents/decisions/a-menu-is-presented-by-a-participant.md`
