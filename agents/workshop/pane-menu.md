# Workshop law — a pane's menu and the participant that presents it

Register `WL-CTX`, the presenter's half: the menu a pane asks the host to present, the office
that shows it in an area of its own, reads it and answers it, and what the host does when no image
can. The contextual surface's own laws — pointing, the popup, the catalog, spending, the mode — are
in [`contextual.md`](contextual.md), and the ids are one series. One law per heading; cite by ID.

## WL-CTX-09 — A pane's menu is judged where it opens and handed to the presenter

LAW — A pane's menu request is judged where it would open and granted to the presenter's office; it moves no keys or selection, and the host answers only asks it refused or no presenter can.

MEANS
- a continuation of one gesture: a secondary press, a declared key, or a PRIMARY press -- in a graphics window numbered by the window;
- one at a time: a newer menu, a right press, the host's menu, the pane leaving withdraw it, and none opens where no window shows the desk;
- the host keeps the keys, the popup's frame and its standard rows; the menu stands in an area of the presenter's, one a menu,
inside the frame, for the weaver's hand alone where the pane is: a press there is the presenter's, its act the host's.

DOES NOT MEAN
- that the host performs a pane's row, a pane a standard row, or that a row grants one;
- that anything is restored after: a choice may continue to a manage row or the keys, once.

WHY — `agents/decisions/pointing-is-not-selection.md`

## WL-CTX-10 — The presenter owns a menu's showing and lifetime; replacing it is ordinary

LAW — The presenter's office draws a granted menu, reads its acts, ends it and answers once; a reload keeping `HeldMenu` hands an open menu over, and an interaction no image holds goes back to the host.

MEANS
- the shipped presenter chooses on the press; the numbered example on a digit or the release.
Both refuse labels over <!-- value kMaxPaneMenuLabelLen -->64<!-- /value --> bytes before fitting accepted labels to the available display width;
- it says what its area takes (`MenuShown`), losing the menu in words past its room or in another area; a moment in another menu's area moves nothing;
- a requester takes a choice only from this office, under its own ask's number, once (`Asked`); gone, not carrying it, or giving it
back (`MenuReturned`): the host answers, open or withdrawn.

DOES NOT MEAN
- that a give-back reaches a newer menu or one answered: `MenuClosed` retired that record;
- that every later participant must hand over or cancel alike, being these consumers' policy; nor that a press meets an older drawing:
one the window read before a scroll showed meets the lines drawn now.

WHY — `agents/decisions/a-menu-is-presented-by-a-participant.md`

## Proven by

### WL-CTX-09
`workshop/pane_vocabulary.hpp` PaneMenuRow PaneMenuRequested PaneMenuAnswered PaneManageRequested
PaneKeyboardRequested kPresenterRole
`workshop/presenter_vocabulary.hpp` MenuGranted MenuShown MenuInput MenuClosed MenuWithdrawn
MenuGranted::standard MenuGranted::pane_name MenuGranted::area MenuClosed::standard
`workshop/context.hpp` PresentedMenu MenuPress kMaxMenuPresses context_population
`workshop/weave.hpp` grant_menu Hand::press_sent withdraw_menu end_menu_unanswered
spend_standard_row cell_of_body_place ChoiceAnswered Hand::action_sent menu_pressed
act_of_menu_press
`workshop/weave_external.cpp` on(PaneMenuRequested) grant_menu spend_standard_row withdraw_menu
end_menu_unanswered about_open_menu forward_menu_input menu_key menu_button menu_pressed
act_of_menu_press on(MenuShown) on(MenuClosed) on(PresenterReady) on(PaneManageRequested)
on(PaneKeyboardRequested) cell_of_body_place
`workshop/weave_handlers.cpp` on(AreaPressed) on(AreaHandRefused)
`workshop/weave_run.cpp` arrange_graphics
`workshop/weave_pointer.cpp` context_key
`workshop/screen.hpp` presented_room presented_bounds presented_area_place presented_menu_area
paint_presented presented_holds
`workshop/screen_attention.cpp` presented_room presented_bounds presented_area_place
presented_menu_area paint_presented presented_holds
`tests/test_workshop_panes_desktop.cpp` "WL-CTX-09: a printable menu shortcut and the text its own
key produced are one gesture, so the menu opens"
`tests/test_workshop_panes_button.cpp` "WL-CTX-09: a menu requested on the press's own turn is
granted to the presenter and opens beside the press with the pane's rows, moves no keys and no
selection, and Return returns the first row -- answered by the presenter" "WL-CTX-09: a pane's menu
shows its own rows first and the host's standard rows beneath them in one menu, and the keyboard
passes over the rule between them" "WL-CTX-09: a standard row chosen on a pane's menu is spent by
the host on that pane, and the pane is answered unchosen and performs nothing" "WL-CTX-09: a
standard row is spent only while its choosing act is the weaver's latest -- a key queued behind the
choice leaves it unspent" "WL-CTX-09: the keyboard works the menu -- Down then Return chooses the
second row; Escape answers it unchosen; the release under it still reaches the pane" "WL-CTX-09: an
outside press dismisses the menu, is spent on dismissing, and reaches nothing beneath it"
"WL-CTX-09: a press on a presented row chooses it" "WL-CTX-09: a pane's menu stands in its
presenter's own area of the window, inside the popup's frame, in front of the desk and behind the
overlay, placed for that menu alone and withdrawn with it" "WL-CTX-09: with no window showing the
desk a pane's menu is refused in words, and nothing is granted" "WL-CTX-09: a right press on a
pane's menu ends it, the presenter answering in the host's words, and reaches nothing beneath" "a
release in a menu's area carries its own press's number when another hand acts between, and
chooses the row its press armed" "WL-CTX-09: a late request is refused where the menu opens -- a
newer primary press elsewhere keeps the keys it took; the review's second integration finding"
"WL-CTX-09: an offer echoing no gesture and one from an office that never offered the pane are
refused or dropped by the host; an empty offer is the presenter's to refuse, and it spends the
gesture" "WL-CTX-09: a newer menu replaces an open one, which its presenter answers unchosen in the
host's words; one menu at a time" "WL-CTX-09: a pane that leaves the desk while its menu is open has
it withdrawn, answered unchosen; a pane that consumes the press opens nothing" "WL-CTX-09: a menu
opened by a declared key continues that keystroke -- eligible on its turn, refused after a newer
key, and anchored in the pane's own body" "WL-CTX-09: a chosen row may continue into the host's own
pane menu on a subject the pane names -- once, while the choice is the weaver's latest act, and
never for a pane the inventory lacks" "WL-CTX-09: a chosen row that begins an edit may take the
keyboard -- once, while the choice is the weaver's latest act, and never under another number"
`workshop/pane_menu.hpp` take_keyboard take_keyboard_continuing
`files/files.cpp` take_keys
`builder/builder-pane/pane.cpp` take_keys
`tests/test_workshop_panes_files.cpp` "a Files menu choice that begins authoring takes the keyboard
the menu left behind" "a Files menu choice that opens no edit leaves the keyboard where the weaver
put it"
`tests/test_workshop_panes_builder.cpp` "a Builder menu choice that opens the role line takes the
keyboard across the door it waits on" "a Builder menu choice that opens no line leaves the keyboard
where the weaver put it"
`tests/test_workshop_desk.cpp` "a press in a pane's area of the window is the pane's, and under the window's number Workshop
tells it where the keys were, hands its press back, opens its menu and carries what it drags out
or picks up"

### WL-CTX-10
`workshop/presenter_vocabulary.hpp` HeldMenu HeldMenu::standard HeldMenu::pane_name HeldMenu::area
PresenterReady MenuClosed MenuReturned MenuShown kMaxMenuLines MenuDrawing MenuLine
close_held_menu
`workshop/menu-presenter/presenter.cpp` MenuPresenter refusal_of MenuPresenter::give_back
MenuPresenter::row_of MenuPresenter::is_rule rule_line drawable
`examples/numbered-presenter/presenter.cpp` NumberedPresenter digit_row line_for
NumberedPresenter::give_back NumberedPresenter::row_of
`workshop/pane_menu.hpp` Asked Asked::take Offer
`workshop/desktop-pane/launcher_view.hpp` launcher_asked_
`workshop/desktop-pane/hotkeys_view.hpp` keys_asked_
`workshop/weave_external.cpp` on(PresenterReady) on(MenuShown) answer_withdrawn end_refused_menu
on(WithdrawalFence) on(MenuReturned) forget_withdrawn
`workshop/context.hpp` WithdrawnMenu PresentedMenu::first_attempt
`workshop/vocabulary.hpp` WithdrawalFence
`workshop/weave.hpp` withdrawn_
`workshop/weave_code.cpp` on(DispatchRefused)
`workshop/weave_seam.cpp` on(PaneOffered)
`tests/test_workshop_panes_button.cpp` "WL-CTX-09: a menu with more rows than the room is windowed
by the presenter, and every row is still reachable" "WL-CTX-09: a presenter that shows its menu in
an area it was not granted, or wider than its room, loses the menu in words" "WL-CTX-10: a
requester's record of its ask settles on the presenter's answer or the host's refusal -- once, and
on nothing another office says under its number" "WL-CTX-10: a withdrawal the presenter cannot
receive is answered by the host, once, under the ask's number" "WL-CTX-10: a refused act settles its
withdrawn menu, though the withdrawal reaches a fresh presenter that cannot answer it" "WL-CTX-10:
an older menu's refused withdrawal ends and answers nothing newer; the newer menu is shown and
chooses" "WL-CTX-10: a withdrawal that queues nothing is answered at once, and nothing is kept"
"WL-CTX-10: a refusal notice anyone could send settles no menu, open or withdrawn" "WL-CTX-10: a
withdrawn menu's record is forgotten when its fence comes round twice, and ordinary use keeps none"
"WL-CTX-10: a withdrawal a fresh holder does not carry is given back, and the host settles who
asked" "WL-CTX-10: an act reaching an image that refused the grant is given back and takes nothing"
"WL-CTX-10: a menu its image answered is not reopened by a give-back behind it" "WL-CTX-10: a
give-back is one office's word about one menu, and settles no other"
`tests/test_workshop_panes_desktop.cpp` "WL-CTX-10: a reloaded desktop cancels its predecessor's
menu -- withdrawn when the successor offers its pane again, and a choice about it acts on nothing"
"WL-CTX-10: the desktop acts only on the presenter's answer to an ask of this image's own -- not the
host's choice, not another subject, not another number, not a predecessor's, and not twice"
"WL-CTX-10: an ordinary replacement presenter holds the office -- the Pane Manager and Hotkeys keep
their own operations while it presents their menus its way: numbered, a digit chooses, a release
chooses" "WL-CTX-10: the replacement presenter presents the Pane Manager's menu too, and a digit
opens the row -- through the desktop's own launch" "WL-CTX-10: the presenter reloaded in place by
another image while a menu is open hands the menu over -- shown again the new way with its cursor,
answered under the same number, and the Hotkeys edit completes" "WL-CTX-10: a presenter that leaves
ends its menu, answered by the host; with none in the office a menu is refused in words and the
host's own menu still opens; one loaded afresh does not carry the old menu, which ends answered"
