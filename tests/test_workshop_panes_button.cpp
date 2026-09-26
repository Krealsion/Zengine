// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE SECOND BUTTON AND THE MENU A PANE ASKS THE HOST TO PRESENT, driven through the real
// Workshop weave, the real bus and native provider seats (WL-PRESS-06, WL-CTX-08, WL-CTX-09):
// delivery by the holder's door, release custody across panes, the turn orders a model cannot
// establish, owner loss after the release, recipient replacement, a late menu request, and the
// presenter's lifecycle -- keys, mouse, Escape, an outside press, a newer menu, a pane that leaves,
// a withdrawal it cannot be told of (WL-CTX-10). The shipped guard example is loaded at the end.

#include "doctest.h"

#include "workshop_support.hpp"

#include "workshop/pane_menu.hpp"

#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kGuardOffice = "zengine.test.guard";
constexpr const char* kGuardPane = "guard";
constexpr const char* kGuardTwoOffice = "zengine.test.guard-two";
constexpr const char* kGuardTwoPane = "guard2";
constexpr const char* kSeatMenuId = "seat.menu";

/// A NATIVE PROVIDER WITH THE `PaneButton` DOOR. It records every button it is told about with
/// the correlation and author, and does one of four things a real consumer would: nothing (the
/// blocking game), hand the press back (a heading row), ask the host to present two rows (a
/// binding row), or ask to be revealed (the reveal door, kept distinct from the menu). It
/// interprets nothing else, and records every menu answer it hears.
class ButtonSeat
    : public loom::WeaveBase<ButtonSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PaneButton,
                                          PaneRevealAnswered, PaneMenuAnswered, PaneKey,
                                          PaneTextInput, PaneActionRequested, SeatDo>,
                             loom::Emit<PaneOffered, PaneActions, PaneContent, PanePassRequested,
                                        PaneMenuRequested, PaneManageRequested,
                                        PaneKeyboardRequested, PaneRevealRequested>> {
public:
    ButtonSeat(std::string office, std::string pane) : office_(std::move(office)), pane_(std::move(pane)) {}

    struct Heard {
        std::int64_t button = 0;
        bool pressed = false;
        std::int64_t row = 0;
        std::int64_t column = 0;
        bool lost = false;
        std::int64_t picture = 0;
        std::uint64_t correlation = 0;
        std::string author;
    };

    void on(const PaneCatalogRequested&, loom::Mail&) { ++state_.said; }
    void on(const PaneRoom&, loom::Mail&) { ++state_.said; }
    void on(const PaneButton& b, loom::Mail& mail) {
        ++state_.said;
        buttons.push_back(Heard{b.button, b.pressed, b.row, b.column, b.lost, b.picture,
                                mail.correlation(), std::string(mail.authored_role())});
        if (b.pressed && pass_on_press) {
            (void)pane_menu::pass_back(mail, office_, pane_);
        }
        if (b.pressed && menu_on_press) {
            asked = offer(b.row, b.column).send(mail, office_);
        }
        if (b.pressed && reveal_on_press) {
            (void)mail.as_role(office_).send_to_role(kWorkshopProvider, PaneRevealRequested{pane_});
        }
    }
    void on(const PaneRevealAnswered& a, loom::Mail&) { reveals.push_back(a); }
    void on(const PaneMenuAnswered& a, loom::Mail& mail) {
        answers.push_back(a);
        answer_correlations.push_back(mail.correlation());
        answer_authors.push_back(std::string(mail.authored_role()));
        // ...AND WHAT THE REQUESTER'S OWN RECORD MAKES OF IT (`pane_menu::Asked::take`).
        taken.push_back(asked.take(mail, a));
        pending_after.push_back(asked.pending());
        if (a.chosen && manage_on_choice) {
            (void)pane_menu::manage(mail, office_, pane_, manage_office, manage_pane);
        }
        // A CHOSEN ROW THAT BEGINS AN EDIT ASKS FOR THE KEYS THE MENU LEFT WHERE THEY WERE.
        if (a.chosen && keys_on_choice) {
            (void)pane_menu::take_keyboard(mail, office_, pane_);
        }
    }
    void on(const PaneKey& k, loom::Mail&) { keys.push_back(k); }
    void on(const PaneTextInput&, loom::Mail&) {}
    void on(const PaneActionRequested& a, loom::Mail& mail) {
        actions.push_back(a);
        action_correlations.push_back(mail.correlation());
        if (a.id == kSeatMenuId && menu_on_action) {
            asked = offer(0, 0).send(mail, office_);
        }
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(ButtonSeat&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }

    pane_menu::Offer offer(std::int64_t row, std::int64_t column) const {
        pane_menu::Offer o(pane_, subject);
        o.at(row, column);
        for (const std::pair<std::string, std::string>& r : rows) {
            o.row(r.first, r.second);
        }
        return o;
    }

    void offer_pane(loom::Mail& mail) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider,
                                                 PaneOffered{pane_, "Guard", "blocks on a button"});
        PaneActions actions_declared;
        actions_declared.pane = pane_;
        actions_declared.rows.push_back(
            PaneActionRow{kSeatMenuId, "menu", input::scan::kM, input::mod::kNone});
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, actions_declared);
    }
    /// HAND A PRESS BACK DELIBERATELY, under whatever correlation the case chooses.
    void pass(loom::Mail& mail, std::uint64_t correlation) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, PanePassRequested{pane_},
                                                 correlation);
    }
    /// ...NAMING SOMEBODY ELSE'S PANE: the sentence an office that never offered it must not
    /// be able to spend.
    void pass_naming(loom::Mail& mail, const char* pane, std::uint64_t correlation) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, PanePassRequested{pane},
                                                 correlation);
    }
    /// ASK FOR A MENU under a chosen number, from a later delivery.
    void ask_menu(loom::Mail& mail, std::uint64_t correlation) {
        asked = offer(0, 0).continuing(mail, office_, correlation);
    }
    /// ASK FOR THE KEYS under a chosen number, from a later delivery -- the form a pane whose
    /// edit opens only after a door has answered must use (`take_keyboard_continuing`).
    void ask_keys(loom::Mail& mail, std::uint64_t correlation) {
        (void)pane_menu::take_keyboard_continuing(mail, office_, pane_, correlation);
    }

    bool pass_on_press = false;
    bool menu_on_press = false;
    bool menu_on_action = false;
    bool reveal_on_press = false;
    bool manage_on_choice = false;
    bool keys_on_choice = false;
    std::string manage_office;
    std::string manage_pane;
    std::string subject = "subject-1";
    std::vector<std::pair<std::string, std::string>> rows = {{"seat.first", "First row"},
                                                             {"seat.second", "Second row"}};
    std::vector<Heard> buttons;
    std::vector<PaneRevealAnswered> reveals;
    std::vector<PaneMenuAnswered> answers;
    std::vector<std::uint64_t> answer_correlations;
    std::vector<std::string> answer_authors; ///< the office each answer was authored as
    pane_menu::Asked asked;                  ///< this seat's one outstanding menu
    std::vector<std::string> taken;          ///< what `asked.take` returned for each answer
    std::vector<bool> pending_after;         ///< ...and whether the ask was still pending after
    std::vector<PaneKey> keys;
    std::vector<PaneActionRequested> actions;
    std::vector<std::uint64_t> action_correlations;
    std::function<void(ButtonSeat&, loom::Mail&)> next;

private:
    std::string office_;
    std::string pane_;
};

/// Mount a button seat in `office`, have it offer `pane` as that office, open the pane, and
/// return the weave id; the pane's runtime handle is read from the desk afterwards.
loom::WeaveId mount_button_seat(PaneRig& r, ButtonSeat*& seat, const char* office, const char* pane) {
    auto held = std::make_unique<ButtonSeat>(office, pane);
    seat = held.get();
    loom::Grant grant;
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    grant.allow_to_any(PaneActions::zen_name, PaneActions::zen_version);
    grant.allow_to_any(PaneContent::zen_name, PaneContent::zen_version);
    grant.allow_to_any(PanePassRequested::zen_name, PanePassRequested::zen_version);
    grant.allow_to_any(PaneMenuRequested::zen_name, PaneMenuRequested::zen_version);
    grant.allow_to_any(PaneManageRequested::zen_name, PaneManageRequested::zen_version);
    grant.allow_to_any(PaneKeyboardRequested::zen_name, PaneKeyboardRequested::zen_version);
    grant.allow_to_any(PaneRevealRequested::zen_name, PaneRevealRequested::zen_version);
    // ...and a menu's answer, which no seat says on its own: a case forges one as this office to
    // show that an answer from an office that is not the presenter settles nothing.
    grant.allow_to_any(PaneMenuAnswered::zen_name, PaneMenuAnswered::zen_version);
    const loom::WeaveId id =
        r.bus.register_weave(std::move(held), std::move(grant), std::string(office));
    seat->zen_set_self(id);
    seat->next = [](ButtonSeat& s, loom::Mail& m) { s.offer_pane(m); };
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
    if (!has_pane(r.session().setup.active, PaneRef{office, pane})) {
        r.pick(PaneRef{office, pane});
    }
    return id;
}

/// Drive one sentence inside the seat's own delivery (an authorship moment Loom can verify).
void drive_seat(PaneRig& r, loom::WeaveId id, ButtonSeat* seat,
                std::function<void(ButtonSeat&, loom::Mail&)> what) {
    seat->next = std::move(what);
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
}

std::int64_t kind_of(PaneRig& r, const char* office, const char* pane) {
    const RuntimePane* row = r.session().panels.runtime.find(office, pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

/// A secondary button transition at a canvas cell, as the terminal medium reports it.
void button_cell(PaneRig& r, std::int64_t button, bool pressed, std::int64_t cx, std::int64_t cy) {
    r.publish(loom::to_value(input::PointerButton{button, pressed, cx, cy + surface::kTuiCanvasTopRow,
                                                  input::space::kCells, input::mod::kNone}));
}

/// ...QUEUED WITHOUT BEING HANDLED, so a case can order the host's turns itself.
void queue_button(PaneRig& r, std::int64_t button, bool pressed, std::int64_t cx, std::int64_t cy) {
    (void)r.bus.publish(loom::Message(
        loom::to_value(input::PointerButton{button, pressed, cx, cy + surface::kTuiCanvasTopRow,
                                            input::space::kCells, input::mod::kNone}),
        loom::WeaveId{}, loom::WeaveId{}, 0));
}

/// The canvas cell of prose row `row`, column `col` of a pane's BODY (under its header).
ui::Rect body_of(PaneRig& r, std::int64_t kind) { return external_body_rect(r.session(), kind); }
std::int64_t body_x(PaneRig& r, std::int64_t kind, std::int64_t col) { return body_of(r, kind).x + col; }
std::int64_t body_y(PaneRig& r, std::int64_t kind, std::int64_t row) {
    return body_of(r, kind).y + kExternalHeaderRows + row;
}

struct Rigged {
    PaneRig r;
    ButtonSeat* guard = nullptr;
    loom::WeaveId guard_id{};
    std::int64_t guard_kind = kNoPaneKind;
    ProviderSeat* hello = nullptr;
    std::int64_t hello_kind = kNoPaneKind;
    loom::WeaveId presenter{};

    /// THE SHIPPED PRESENTER BY DEFAULT; a case that must show a policy is the seam's and not one
    /// image's names the replacement example instead, and every case below reads the same.
    explicit Rigged(const char* presenter_image = WORKSHOP_SO_MENU_PRESENTER) {
        r.mount_workshop();
        r.ready();
        r.extent(140, 60);
        hello = r.mount_provider(kHelloOffice);
        r.drive(hello, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
        r.pick(hello_ref());
        hello_kind = r.session().panels.runtime.entries[0].kind;
        guard_id = mount_button_seat(r, guard, kGuardOffice, kGuardPane);
        guard_kind = kind_of(r, kGuardOffice, kGuardPane);
        // THE SHIPPED PRESENTER, loaded into its office the way a plan row loads it: every menu
        // the seats ask for is presented and answered by that image, never by a stand-in.
        presenter = r.load_presenter(presenter_image);
    }

    ui::Rect hello_body() { return pane_body_cells(external_panel_rect(r.session(), hello_kind)); }
    void right_in_guard(bool pressed = true) {
        button_cell(r, 3, pressed, body_x(r, guard_kind, 1), body_y(r, guard_kind, 0));
    }
    /// ANY SURFACE OPEN: the host's own menu, or a pane's granted to the presenter.
    bool menu_open() { return r.session().context.open || r.session().presented.open; }
    /// A PANE'S MENU, GRANTED AND SHOWN BY THE PRESENTER.
    bool foreign_open() { return menu_shown(r.session()); }
};

} // namespace

// =============================================================================
// Custody and continuation
// =============================================================================

TEST_CASE("WL-PRESS-06: a right press over a pane whose holder has the door is delivered, consumed, opens no menu, and names the admitted picture; the chrome stays the host's") {
    Rigged t;
    REQUIRE(is_runtime_kind(t.guard_kind));
    REQUIRE(holder_accepts_on(t.r.bus, kGuardOffice, *loom::schema_of<PaneButton>()));
    const std::int64_t keyboard_before = t.r.session().panels.keyboard;
    const std::int64_t selected_before = t.r.session().panels.selected;

    button_cell(t.r, 3, true, body_x(t.r, t.guard_kind, 2), body_y(t.r, t.guard_kind, 1));
    REQUIRE(t.guard->buttons.size() == 1);
    CHECK(t.guard->buttons[0].button == 3);
    CHECK(t.guard->buttons[0].pressed);
    CHECK(t.guard->buttons[0].row == 1);
    CHECK(t.guard->buttons[0].column == 2);
    CHECK_FALSE(t.guard->buttons[0].lost);
    CHECK(t.guard->buttons[0].picture == 0); // a v1 content numbers no picture
    CHECK(t.guard->buttons[0].correlation != 0);
    CHECK(t.guard->buttons[0].author == std::string(kWorkshopProvider));
    // DELIVERY IS CONSUMPTION: no surface opened, and pointing changed no selection or keys.
    CHECK_FALSE(t.menu_open());
    CHECK(t.r.session().panels.keyboard == keyboard_before);
    CHECK(t.r.session().panels.selected == selected_before);
    CHECK(t.hello->presses.empty());

    // THE CHROME IS NEVER THE PANE'S: a right press on the title row opens the host's menu.
    button_cell(t.r, 3, false, body_x(t.r, t.guard_kind, 2), body_y(t.r, t.guard_kind, 1));
    const ui::Rect body = body_of(t.r, t.guard_kind);
    button_cell(t.r, 3, true, body.x + 2, body.y); // the header row
    CHECK(t.menu_open());
    CHECK_FALSE(t.foreign_open());
    CHECK(t.r.session().context.subject == context_subject::kPane);
    CHECK(t.r.session().context.pane == PaneRef{kGuardOffice, kGuardPane});
    CHECK(t.guard->buttons.size() == 2); // the press and its release; nothing for the chrome
}

TEST_CASE("WL-PRESS-06: a doorless pane's body is empty by default -- a right press there opens no menu and takes no keys; its chrome still opens the host's menu") {
    Rigged t;
    REQUIRE_FALSE(holder_accepts_on(t.r.bus, kHelloOffice, *loom::schema_of<PaneButton>()));
    const std::int64_t keyboard_before = t.r.session().panels.keyboard;
    const ui::Rect body = external_body_rect(t.r.session(), t.hello_kind);
    // THE BODY IS EMPTY BY DEFAULT: a holder that declared no `PaneButton` door is sent nothing,
    // and the press acquires no host menu and no keyboard. Silence is not pass-through -- the
    // provider hears nothing either. (WL-CTX-08, empty by default.)
    t.r.right_press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);
    CHECK_FALSE(t.menu_open());
    CHECK(t.hello->presses.empty());
    CHECK(t.guard->buttons.empty());
    CHECK(t.r.session().panels.keyboard == keyboard_before);
    // THE CHROME IS THE HOST'S ALWAYS: a right press on the title row opens the host's pane menu,
    // the retained management route for a pane whose body takes the button or means nothing by it.
    t.r.right_press_cell(body.x + 1, body.y);
    CHECK(t.menu_open());
    CHECK(t.r.session().context.pane == hello_ref());
    CHECK(t.hello->presses.empty());
}

TEST_CASE("WL-PRESS-06: the release is the pressing pane's wherever the pointer is, and leaks into no other pane") {
    Rigged t;
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    const std::int64_t hello_said_before = t.hello->said;
    const ui::Rect hello = t.hello_body();
    button_cell(t.r, 3, false, hello.x + 1, hello.y + 2);
    REQUIRE(t.guard->buttons.size() == 2);
    CHECK_FALSE(t.guard->buttons[1].pressed);
    CHECK(t.guard->buttons[1].button == 3);
    CHECK_FALSE(t.guard->buttons[1].lost);
    CHECK(t.hello->presses.empty());
    CHECK(t.hello->said == hello_said_before); // nothing pointer-shaped reached it
    // ORDINARY PRIMARY PRESSES ARE UNTOUCHED: a left press into Hello still points the keys there.
    t.r.press_cell(hello.x + 1, hello.y + 2);
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
    // ...and a second release of a button nobody holds is dropped.
    button_cell(t.r, 3, false, hello.x + 1, hello.y + 2);
    CHECK(t.guard->buttons.size() == 2);
}

TEST_CASE("WL-CTX-08: a pass-back after a clean click opens the host's menu for that pane, once") {
    Rigged t;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.guard->buttons.size() == 2);
    const std::uint64_t correlation = t.guard->buttons[0].correlation;
    CHECK_FALSE(t.menu_open());
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.pass(m, correlation); });
    CHECK(t.menu_open());
    CHECK(t.r.session().context.subject == context_subject::kPane);
    CHECK(t.r.session().context.pane == PaneRef{kGuardOffice, kGuardPane});
    // ONCE: closed, a duplicate pass-back reopens nothing.
    t.r.key(input::scan::kEscape);
    REQUIRE_FALSE(t.menu_open());
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.pass(m, correlation); });
    CHECK_FALSE(t.menu_open());
}

TEST_CASE("WL-CTX-08: a pass-back on the press's own turn opens the menu, and the release still reaches the pane under it") {
    Rigged t;
    t.guard->pass_on_press = true;
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    CHECK(t.menu_open());
    CHECK(t.r.session().context.pane == PaneRef{kGuardOffice, kGuardPane});
    t.right_in_guard(false);
    REQUIRE(t.guard->buttons.size() == 2);
    CHECK_FALSE(t.guard->buttons[1].pressed);
    CHECK(t.menu_open());
}

TEST_CASE("WL-PRESS-06: the review's interleaving -- press, an intervening key, release, a delayed pass-back -- opens nothing") {
    Rigged t;
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    const std::uint64_t correlation = t.guard->buttons[0].correlation;
    t.r.key(input::scan::kZ); // a gesture that is not the press's own release (unbound in command mode)
    t.right_in_guard(false);
    REQUIRE(t.guard->buttons.size() == 2); // custody held: the release still reached the pane
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.pass(m, correlation); });
    CHECK_FALSE(t.menu_open());
    // ...and a pass-back that was already stale before the release stays stale after it.
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 3);
    const std::uint64_t second = t.guard->buttons[2].correlation;
    t.r.key(input::scan::kZ);
    drive_seat(t.r, t.guard_id, t.guard, [second](ButtonSeat& s, loom::Mail& m) { s.pass(m, second); });
    CHECK_FALSE(t.menu_open());
    t.right_in_guard(false);
    drive_seat(t.r, t.guard_id, t.guard, [second](ButtonSeat& s, loom::Mail& m) { s.pass(m, second); });
    CHECK_FALSE(t.menu_open());
}

TEST_CASE("WL-CTX-08: a stale correlation, a zero one, and a pass-back from an office that did not offer the pane all move nothing") {
    Rigged t;
    ButtonSeat* stranger = nullptr;
    const loom::WeaveId stranger_id = mount_button_seat(t.r, stranger, kGuardTwoOffice, kGuardTwoPane);
    t.right_in_guard();
    t.right_in_guard(false);
    const std::uint64_t first = t.guard->buttons[0].correlation;
    t.right_in_guard();
    t.right_in_guard(false);
    const std::uint64_t second = t.guard->buttons[2].correlation;
    REQUIRE(second != first);
    drive_seat(t.r, t.guard_id, t.guard, [first](ButtonSeat& s, loom::Mail& m) { s.pass(m, first); });
    CHECK_FALSE(t.menu_open());
    drive_seat(t.r, t.guard_id, t.guard, [](ButtonSeat& s, loom::Mail& m) { s.pass(m, 0); });
    CHECK_FALSE(t.menu_open());
    drive_seat(t.r, stranger_id, stranger, [second](ButtonSeat& s, loom::Mail& m) { s.pass_naming(m, kGuardPane, second); });
    CHECK_FALSE(t.menu_open());
    drive_seat(t.r, t.guard_id, t.guard, [second](ButtonSeat& s, loom::Mail& m) { s.pass(m, second); });
    CHECK(t.menu_open());
}

TEST_CASE("WL-PRESS-06: holding right on one pane and pressing middle on another keeps both holds, and each release reaches its own pane") {
    Rigged t;
    ButtonSeat* other = nullptr;
    (void)mount_button_seat(t.r, other, kGuardTwoOffice, kGuardTwoPane);
    const std::int64_t other_kind = kind_of(t.r, kGuardTwoOffice, kGuardTwoPane);
    REQUIRE(is_runtime_kind(other_kind));
    REQUIRE(body_of(t.r, other_kind).y != body_of(t.r, t.guard_kind).y);
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    button_cell(t.r, 2, true, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    REQUIRE(other->buttons.size() == 1);
    CHECK(other->buttons[0].button == 2);
    button_cell(t.r, 3, false, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    REQUIRE(t.guard->buttons.size() == 2);
    CHECK_FALSE(t.guard->buttons[1].pressed);
    CHECK(other->buttons.size() == 1);
    button_cell(t.r, 2, false, body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    REQUIRE(other->buttons.size() == 2);
    CHECK_FALSE(other->buttons[1].pressed);
    CHECK(t.guard->buttons.size() == 2);
    // ...and the guard's continuation was interrupted by the middle press: no menu from it.
    const std::uint64_t guard_press = t.guard->buttons[0].correlation;
    drive_seat(t.r, t.guard_id, t.guard, [guard_press](ButtonSeat& s, loom::Mail& m) { s.pass(m, guard_press); });
    CHECK_FALSE(t.menu_open());
}

TEST_CASE("WL-PRESS-06: a pane closed while the button is down is owed one `lost` release, and its record ends") {
    Rigged t;
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    const std::uint64_t correlation = t.guard->buttons[0].correlation;
    t.r.pick(PaneRef{kGuardOffice, kGuardPane}); // the host's close door: the pane leaves the desk
    REQUIRE_FALSE(has_pane(t.r.session().setup.active, PaneRef{kGuardOffice, kGuardPane}));
    REQUIRE(t.guard->buttons.size() == 2);
    CHECK_FALSE(t.guard->buttons[1].pressed);
    CHECK(t.guard->buttons[1].lost);
    button_cell(t.r, 3, false, 5, 5);
    CHECK(t.guard->buttons.size() == 2);
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.pass(m, correlation); });
    CHECK_FALSE(t.menu_open());
}

TEST_CASE("WL-PRESS-06: closing the pane AFTER the release invalidates the continuation on its own -- the review's first integration finding") {
    Rigged t;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.guard->buttons.size() == 2);
    const std::uint64_t correlation = t.guard->buttons[0].correlation;
    // THE OFFICE-AUTHORED CLOSE DOOR, with no gesture between: the hold is long over, and only
    // the continuation could still open a menu for a pane that is no longer on the desk.
    t.r.pick(PaneRef{kGuardOffice, kGuardPane});
    REQUIRE_FALSE(has_pane(t.r.session().setup.active, PaneRef{kGuardOffice, kGuardPane}));
    CHECK(t.guard->buttons.size() == 2); // nothing was owed: no hold was active
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.pass(m, correlation); });
    CHECK_FALSE(t.menu_open());
    // ...and a menu request for it is answered unchosen, with the reason.
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, correlation); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].refusal.find("not on the desk") != std::string::npos);
}

TEST_CASE("WL-PRESS-06: a press of a button the host believes is down ends the old hold aloud, never silently") {
    Rigged t;
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    button_cell(t.r, 3, true, body_x(t.r, t.guard_kind, 2), body_y(t.r, t.guard_kind, 0));
    REQUIRE(t.guard->buttons.size() == 3);
    CHECK_FALSE(t.guard->buttons[1].pressed);
    CHECK(t.guard->buttons[1].lost);
    CHECK(t.guard->buttons[2].pressed);
    CHECK(t.guard->buttons[2].column == 2);
}

TEST_CASE("WL-PRESS-06: a holder replaced while the button is down -- the release goes to the ROLE's current holder, who never saw the press; the old holder is told nothing") {
    Rigged t;
    t.right_in_guard();
    REQUIRE(t.guard->buttons.size() == 1);
    std::unique_ptr<loom::Weave> removed = t.r.bus.unregister_weave(t.guard_id);
    REQUIRE(removed != nullptr);
    ButtonSeat* old = t.guard;
    ButtonSeat* successor = nullptr;
    const loom::WeaveId successor_id = mount_button_seat(t.r, successor, kGuardOffice, kGuardPane);
    REQUIRE(t.r.bus.role_holder(kGuardOffice) == successor_id);
    CHECK(kind_of(t.r, kGuardOffice, kGuardPane) == t.guard_kind);
    REQUIRE(successor->buttons.empty());
    t.right_in_guard(false);
    REQUIRE(successor->buttons.size() == 1);
    CHECK_FALSE(successor->buttons[0].pressed);
    CHECK(old->buttons.size() == 1);
    // THE SHIPPED HELPER'S POLICY: a release of a button this image never held is nothing.
    pane_menu::HeldButton held;
    CHECK_FALSE(held.take(PaneButton{kGuardPane, 3, false, 0, 0, false, 0}));
    CHECK_FALSE(held.held);
    const std::uint64_t correlation = old->buttons[0].correlation;
    drive_seat(t.r, successor_id, successor, [correlation](ButtonSeat& s, loom::Mail& m) { s.pass(m, correlation); });
    CHECK(t.menu_open()); // the office is the same, the pane is the same, the record stands
}

TEST_CASE("WL-PRESS-06: a press Loom refuses is settled -- the custody it recorded is dropped, so the physical release sends nothing and no second refusal follows; the failure stands on the tap") {
    Rigged t;
    std::vector<loom::EventKind> attempts;
    std::vector<loom::RefusalReason> reasons;
    const loom::ObserverId tap = t.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.schema_name == PaneButton::zen_name &&
            (ev.kind == loom::EventKind::Delivered || ev.kind == loom::EventKind::Refused)) {
            attempts.push_back(ev.kind);
            reasons.push_back(ev.refusal.reason);
        }
    });
    queue_button(t.r, 3, true, body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    REQUIRE(t.r.bus.pump_pending() >= 1);
    REQUIRE(attempts.empty());
    std::unique_ptr<loom::Weave> removed = t.r.bus.unregister_weave(t.guard_id);
    REQUIRE(removed != nullptr);
    ProviderSeat* successor = t.r.mount_provider(kGuardOffice);
    REQUIRE_FALSE(holder_accepts_on(t.r.bus, kGuardOffice, *loom::schema_of<PaneButton>()));
    t.r.bus.drain_until_idle();
    REQUIRE(attempts.size() == 1);
    CHECK(attempts[0] == loom::EventKind::Refused);
    CHECK(reasons[0] == loom::RefusalReason::NotAccepted);
    CHECK(t.guard->buttons.empty());
    // KNOWN REFUSAL IS NOT SILENCE, AND IT IS NOT SUCCESS: the host opens no menu for a press
    // Loom refused, and it does not invent a hold to hand back. Loom's tap attributes the one
    // refusal; the host settles the exact attempt on that notice and drops the custody it had
    // recorded when the send was queued (WL-PRESS-06).
    CHECK_FALSE(t.menu_open());
    // THE PHYSICAL RELEASE OF A PRESS THAT NEVER REACHED A RECIPIENT SENDS NOTHING: there is no
    // second refused button, and the successor -- who never saw the press -- sees no release.
    t.right_in_guard(false);
    t.r.bus.drain_until_idle();
    CHECK(attempts.size() == 1);
    CHECK(successor->presses.empty());
    t.r.bus.remove_observer(tap);
}

// =============================================================================
// The menu a pane asks for, presented by the presenter participant (WL-CTX-09)
// =============================================================================

TEST_CASE("WL-CTX-09: a menu requested on the press's own turn is granted to the presenter and opens beside the press with the pane's rows, moves no keys and no selection, and Return returns the first row -- answered by the presenter") {
    Rigged t;
    const ui::Rect hello = t.hello_body();
    t.r.press_cell(hello.x + 1, hello.y + 2);
    REQUIRE(t.r.session().panels.keyboard == t.hello_kind);
    t.guard->menu_on_press = true;
    button_cell(t.r, 3, true, body_x(t.r, t.guard_kind, 3), body_y(t.r, t.guard_kind, 1));
    REQUIRE(t.foreign_open());
    // THE HOST KEEPS CUSTODY AND PLACE -- whose menu, where, what it was about -- and no row.
    const PresentedMenu& menu = t.r.session().presented;
    CHECK(menu.anchored);
    CHECK(menu.anchor_x == body_x(t.r, t.guard_kind, 3));
    CHECK(menu.anchor_y == body_y(t.r, t.guard_kind, 1));
    CHECK(menu.office == kGuardOffice);
    CHECK(menu.pane == kGuardPane);
    CHECK(menu.subject == "subject-1");
    // PRESENTED: the rows the pane wrote, as the presenter lays them out.
    const std::vector<std::string> painted = context_rows_on(t.r.last_canvas(), t.r.session());
    REQUIRE(painted.size() == 2);
    CHECK(painted[0] == "> First row");
    CHECK(painted[1] == "  Second row");
    // NOTHING MOVED: the keys stay with Hello, the selection where it was.
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
    CHECK(t.r.session().panels.selected == t.hello_kind);
    // RETURN RETURNS: the choice goes to the office under the request's number, subject-bound,
    // and it is the PRESENTER'S word -- the office a requester authenticates a choice from.
    t.r.key(input::scan::kReturn);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].id == "seat.first");
    CHECK(t.guard->answers[0].subject == "subject-1");
    CHECK(t.guard->answers[0].pane == kGuardPane);
    CHECK(t.guard->answer_correlations[0] == t.guard->buttons[0].correlation);
    CHECK(t.guard->answer_authors[0] == kPresenterRole);
    CHECK(t.hello->keys.empty()); // the Return was the menu's, not Hello's
    // ...AND NOTHING IS RESTORED, because nothing was taken.
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
}

TEST_CASE("WL-CTX-09: the keyboard works the menu -- Down then Return chooses the second row; Escape answers it unchosen; the release under it still reaches the pane") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    REQUIRE(t.foreign_open());
    t.r.key(input::scan::kDown);
    CHECK(presented_texts(t.r.session())[1] == "> Second row");
    t.right_in_guard(false);
    REQUIRE(t.guard->buttons.size() == 2); // the release, delivered under the open menu
    CHECK(t.foreign_open());
    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].id == "seat.second");
    // A SECOND MENU, DISMISSED BY ESCAPE.
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.key(input::scan::kEscape);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 2);
    CHECK_FALSE(t.guard->answers[1].chosen);
    CHECK(t.guard->answers[1].id.empty());
    CHECK(t.guard->answers[1].refusal == "dismissed");
    CHECK(t.guard->answer_correlations[1] == t.guard->buttons[2].correlation);
}

TEST_CASE("WL-CTX-09: an outside press dismisses the menu, is spent on dismissing, and reaches nothing beneath it") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::int64_t keyboard_before = t.r.session().panels.keyboard;
    const ui::Rect hello = t.hello_body();
    t.r.press_cell(hello.x + 1, hello.y + 2);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].refusal == "dismissed");
    CHECK(t.hello->presses.empty());
    CHECK(t.r.session().panels.keyboard == keyboard_before);
    // THE NEXT PRESS IS AN ORDINARY ONE.
    t.r.press_cell(hello.x + 1, hello.y + 2);
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
}

TEST_CASE("WL-CTX-09: a press on a presented row chooses it") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.press_cell(context_cell_x(t.r.session()), context_entry_cell_y(t.r.session(), 1));
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].id == "seat.second");
}

TEST_CASE("WL-CTX-09: a late request is refused where the menu opens -- a newer primary press elsewhere keeps the keys it took; the review's second integration finding") {
    Rigged t;
    t.guard->menu_on_press = true;
    const ui::Rect hello = t.hello_body();
    // THE RIGHT PRESS IS HANDLED, ITS DELIVERY TO THE GUARD QUEUED; a primary press into Hello is
    // queued behind that delivery, so the guard's request arrives AFTER the maker's newer act.
    queue_button(t.r, 3, true, body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    REQUIRE(t.r.bus.pump_pending() >= 1);
    REQUIRE(t.guard->buttons.empty());
    queue_button(t.r, 1, true, hello.x + 1, hello.y + 2);
    t.r.bus.drain_until_idle();
    REQUIRE(t.guard->buttons.size() == 1);
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].refusal.find("late") != std::string::npos);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider); // refused at intake: the host's word
    // THE KEYS ARE HELLO'S, and a key proves it.
    t.r.key(input::scan::kReturn);
    CHECK(t.hello->keys.size() == 1);
    CHECK(t.guard->keys.empty());
}

TEST_CASE("WL-CTX-09: an offer echoing no gesture and one from an office that never offered the pane are refused or dropped by the host; an empty offer is the presenter's to refuse, and it spends the gesture") {
    Rigged t;
    // A STRANGER OFFICE with the door and its own pane, mounted before any click so that its
    // arrival (the rig seats it with a key) is not a gesture between the click and the requests.
    ButtonSeat* stranger = nullptr;
    const loom::WeaveId stranger_id = mount_button_seat(t.r, stranger, kGuardTwoOffice, kGuardTwoPane);
    t.right_in_guard();
    t.right_in_guard(false);
    const std::uint64_t correlation = t.guard->buttons[0].correlation;
    t.guard->rows = {{"a", "A"}};
    drive_seat(t.r, t.guard_id, t.guard, [](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, 0); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].refusal.find("echoes none") != std::string::npos);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
    // THE STRANGER OFFICE naming the guard's pane: no pane of its own, nothing answered.
    drive_seat(t.r, stranger_id, stranger, [correlation](ButtonSeat&, loom::Mail& m) {
        (void)m.as_role(kGuardTwoOffice)
            .send_to_role(kWorkshopProvider,
                          PaneMenuRequested{kGuardPane, "x", 0, 0, {PaneMenuRow{"a", "A"}}},
                          correlation);
    });
    CHECK_FALSE(t.menu_open());
    CHECK(stranger->answers.empty());
    CHECK(t.guard->answers.size() == 1);
    // AN EMPTY OFFER, under the live gesture: the host judges custody and grants it; the
    // PRESENTER judges what can be presented and refuses it in words -- and the gesture is spent.
    t.guard->rows.clear();
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, correlation); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].refusal.find("no rows") != std::string::npos);
    CHECK(t.guard->answer_authors[1] == kPresenterRole);
    // ...SO A CORRECTED OFFER UNDER THE SAME PRESS IS LATE: one gesture, one menu.
    t.guard->rows = {{"a", "A"}};
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, correlation); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 3);
    CHECK(t.guard->answers[2].refusal.find("late") != std::string::npos);
}

TEST_CASE("WL-CTX-09: a newer menu replaces an open one, which its presenter answers unchosen in the host's words; one menu at a time") {
    Rigged t;
    ButtonSeat* other = nullptr;
    (void)mount_button_seat(t.r, other, kGuardTwoOffice, kGuardTwoPane);
    const std::int64_t other_kind = kind_of(t.r, kGuardTwoOffice, kGuardTwoPane);
    t.guard->menu_on_press = true;
    other->menu_on_press = true;
    other->rows = {{"other.only", "The other pane's row"}};
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    REQUIRE(t.r.session().presented.office == kGuardOffice);
    // A RIGHT PRESS INTO THE OTHER PANE while the guard's menu is open: the older is withdrawn and
    // answered unchosen by its presenter, the press is offered to the other pane, and its menu
    // takes the popup.
    button_cell(t.r, 3, true, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].refusal == "a newer press");
    CHECK(t.guard->answer_authors[0] == kPresenterRole);
    REQUIRE(t.foreign_open());
    CHECK(t.r.session().presented.office == kGuardTwoOffice);
    const std::vector<std::string> painted = context_rows_on(t.r.last_canvas(), t.r.session());
    REQUIRE(painted.size() == 1);
    CHECK(painted[0] == "> The other pane's row");
    button_cell(t.r, 3, false, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    t.r.key(input::scan::kReturn);
    REQUIRE(other->answers.size() == 1);
    CHECK(other->answers[0].id == "other.only");
}

TEST_CASE("WL-CTX-09: a pane that leaves the desk while its menu is open has it withdrawn, answered unchosen; a pane that consumes the press opens nothing") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.pick(PaneRef{kGuardOffice, kGuardPane});
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].refusal == "the pane left the desk");
    // EMPTY BY DEFAULT: the same press on a pane that asks for nothing opens nothing, and the
    // reveal door, distinct from the menu, is untouched by a request.
    t.r.pick(PaneRef{kGuardOffice, kGuardPane});
    t.guard->menu_on_press = false;
    t.right_in_guard();
    t.right_in_guard(false);
    CHECK_FALSE(t.menu_open());
    CHECK(t.guard->reveals.empty());
}

TEST_CASE("WL-CTX-09: a menu opened by a declared key continues that keystroke -- eligible on its turn, refused after a newer key, and anchored in the pane's own body") {
    Rigged t;
    t.guard->menu_on_action = true;
    t.r.press_cell(body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    REQUIRE(t.r.session().panels.keyboard == t.guard_kind);
    t.r.key(input::scan::kM);
    REQUIRE(t.guard->actions.size() == 1);
    CHECK(t.guard->action_correlations[0] != 0);
    REQUIRE(t.foreign_open());
    CHECK(t.r.session().presented.anchored);
    CHECK(t.r.session().presented.anchor_y == body_y(t.r, t.guard_kind, 0));
    t.r.key(input::scan::kEscape);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    // A REQUEST FROM A LATER DELIVERY, AFTER A NEWER KEY, IS LATE.
    t.guard->menu_on_action = false;
    t.r.key(input::scan::kM);
    REQUIRE(t.guard->actions.size() == 2);
    const std::uint64_t number = t.guard->action_correlations[1];
    t.r.key(input::scan::kZ);
    drive_seat(t.r, t.guard_id, t.guard, [number](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, number); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].refusal.find("late") != std::string::npos);
}

TEST_CASE("WL-CTX-09: a chosen row may continue into the host's own pane menu on a subject the pane names -- once, while the choice is the maker's latest act, and never for a pane the inventory lacks") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.guard->manage_on_choice = true;
    t.guard->manage_office = kHelloOffice;
    t.guard->manage_pane = "hello";
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.key(input::scan::kReturn);
    // THE CHOICE WAS ANSWERED, THE SEAT CONTINUED IT, AND THE HOST'S MENU IS ON HELLO.
    REQUIRE(t.guard->answers.size() == 1);
    REQUIRE(t.menu_open());
    CHECK_FALSE(t.foreign_open());
    CHECK(t.r.session().context.subject == context_subject::kPane);
    CHECK(t.r.session().context.pane == hello_ref());
    t.r.key(input::scan::kEscape);
    // ONCE: a second continuation of the same choice is stale.
    const std::uint64_t number = t.guard->answer_correlations[0];
    drive_seat(t.r, t.guard_id, t.guard, [number](ButtonSeat&, loom::Mail& m) {
        (void)m.as_role(kGuardOffice)
            .send_to_role(kWorkshopProvider, PaneManageRequested{kGuardPane, kHelloOffice, "hello"}, number);
    });
    CHECK_FALSE(t.menu_open());
    // A PANE NOBODY OFFERED: nothing opens, and the band says so.
    t.guard->manage_pane = "nobody";
    t.right_in_guard();
    t.right_in_guard(false);
    t.r.key(input::scan::kReturn);
    CHECK_FALSE(t.menu_open());
    CHECK(t.r.session().notice.find("nothing to manage") != std::string::npos);
}

TEST_CASE("WL-CTX-09: a chosen row that begins an edit may take the keyboard -- once, while the choice is the maker's latest act, and never under another number") {
    // THE OTHER CONTINUATION, THE ONE THE PANES SPEND. A menu leaves the keyboard where it was, so
    // a row that opens a line, chosen by a right press into an UNFOCUSED pane, needs the keys:
    // `take_keyboard` asks, judged as a manage request is. ⚔ MUTATIONS: the guard's
    // `choice_answered_.spent` check removed takes the keys back on the second continuation below;
    // its `correlation` check removed grants keys on a stale number the maker never asked for.
    Rigged t;
    t.guard->menu_on_press = true;
    t.guard->keys_on_choice = true;
    // THE KEYS ARE SOMEBODY ELSE'S FIRST -- a maker looking at one pane and pointing at another.
    const ui::Rect hello = t.hello_body();
    button_cell(t.r, 1, true, hello.x + 1, hello.y + 1);
    button_cell(t.r, 1, false, hello.x + 1, hello.y + 1);
    REQUIRE(t.r.session().panels.keyboard == t.hello_kind);

    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    // A RIGHT PRESS IS FOCUS-NEUTRAL: the menu is the guard's and the keys are still hello's.
    CHECK(t.r.session().panels.keyboard == t.hello_kind);

    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 1);
    REQUIRE(t.guard->answers[0].chosen);
    // THE CHOICE TOOK THE KEYS, AND THE PANE IS SELECTED WITH THEM.
    CHECK(t.r.session().panels.keyboard == t.guard_kind);
    CHECK(t.r.session().panels.selected == t.guard_kind);

    // ONCE: the same number again, from a later delivery, is a spent continuation. The keys go
    // back to hello by an ordinary press first, so a grant would be visible.
    const std::uint64_t number = t.guard->answer_correlations[0];
    REQUIRE(number != 0);
    button_cell(t.r, 1, true, hello.x + 1, hello.y + 1);
    button_cell(t.r, 1, false, hello.x + 1, hello.y + 1);
    REQUIRE(t.r.session().panels.keyboard == t.hello_kind);
    drive_seat(t.r, t.guard_id, t.guard, [number](ButtonSeat& s, loom::Mail& m) {
        s.ask_keys(m, number);
    });
    CHECK(t.r.session().panels.keyboard == t.hello_kind);

    // AND NEVER UNDER ANOTHER NUMBER: a continuation echoing a number that answered no choice
    // of this pane's -- a forged one, a predecessor's -- grants nothing.
    drive_seat(t.r, t.guard_id, t.guard, [number](ButtonSeat& s, loom::Mail& m) {
        s.ask_keys(m, number + 7);
    });
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
    drive_seat(t.r, t.guard_id, t.guard, [](ButtonSeat& s, loom::Mail& m) { s.ask_keys(m, 0); });
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
}

TEST_CASE("WL-CTX-09: a menu with more rows than the room is windowed by the presenter, and every row is still reachable") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.guard->rows.clear();
    for (int i = 0; i < 80; ++i) {
        t.guard->rows.emplace_back("seat.row-" + std::to_string(i), "Row " + std::to_string(i));
    }
    // MORE ROWS THAN A MENU MAY OFFER: the presenter refuses them as too many; nothing opens.
    t.right_in_guard();
    t.right_in_guard(false);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].refusal.find("too many") != std::string::npos);
    // THE BOUND'S WORTH OF ROWS on a screen that cannot show them all: windowed, every one reachable.
    t.guard->rows.resize(kMaxPaneMenuRows);
    t.r.extent(140, 30);
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::vector<std::string> painted = context_rows_on(t.r.last_canvas(), t.r.session());
    REQUIRE(!painted.empty());
    CAPTURE(painted.size());
    CAPTURE(painted.back());
    CHECK(painted.size() < kMaxPaneMenuRows);
    CHECK(painted.back().find("more") != std::string::npos);
    for (std::size_t i = 0; i + 1 < kMaxPaneMenuRows; ++i) {
        t.r.key(input::scan::kDown);
    }
    const std::string last = "Row " + std::to_string(kMaxPaneMenuRows - 1);
    const std::int64_t at = presented_line_of(t.r.session(), last);
    REQUIRE(at >= 0);
    CHECK(presented_texts(t.r.session())[static_cast<std::size_t>(at)] == "> " + last);
    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].id == "seat.row-" + std::to_string(kMaxPaneMenuRows - 1));
}

TEST_CASE("WL-CTX-10: a requester's record of its ask settles on the presenter's answer or the host's refusal -- once, and on nothing another office says under its number") {
    Rigged t;
    ButtonSeat* stranger = nullptr;
    const loom::WeaveId stranger_id = mount_button_seat(t.r, stranger, kGuardTwoOffice, kGuardTwoPane);
    t.guard->menu_on_press = true;
    // THE HOST'S REFUSAL SETTLES THE ASK: a request made late is answered by the host, unchosen.
    const ui::Rect hello = t.hello_body();
    queue_button(t.r, 3, true, body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    REQUIRE(t.r.bus.pump_pending() >= 1);
    queue_button(t.r, 1, true, hello.x + 1, hello.y + 2);
    t.r.bus.drain_until_idle();
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
    CHECK(t.guard->taken[0].empty());
    CHECK_FALSE(t.guard->pending_after[0]);
    button_cell(t.r, 3, false, body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    // A MENU OPEN, and ANOTHER OFFICE answering it under the ask's own number, chosen: the seat
    // hears it, and its record settles nothing -- the ask is still pending.
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::uint64_t number = t.guard->asked.correlation();
    REQUIRE(number != 0);
    REQUIRE(t.r.bus.office_send_to_role_as(
        stranger_id, kGuardTwoOffice, kGuardOffice,
        loom::Message(loom::to_value(PaneMenuAnswered{kGuardPane, "subject-1", true, "seat.first", ""}),
                      stranger_id, stranger_id, number)).valid());
    t.r.bus.drain_until_idle();
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answer_authors[1] == kGuardTwoOffice);
    CHECK(t.guard->taken[1].empty());
    CHECK(t.guard->pending_after[1]);
    // THE PRESENTER'S ANSWER, the genuine one: settles, with the row.
    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 3);
    CHECK(t.guard->answer_authors[2] == kPresenterRole);
    CHECK(t.guard->taken[2] == "seat.first");
    CHECK_FALSE(t.guard->pending_after[2]);
    // ONCE: the presenter's office saying it again finds nothing pending.
    REQUIRE(t.r.bus.office_send_to_role_as(
        t.presenter, kPresenterRole, kGuardOffice,
        loom::Message(loom::to_value(PaneMenuAnswered{kGuardPane, "subject-1", true, "seat.first", ""}),
                      t.presenter, t.presenter, number)).valid());
    t.r.bus.drain_until_idle();
    REQUIRE(t.guard->answers.size() == 4);
    CHECK(t.guard->taken[3].empty());
}

// =============================================================================
// A withdrawn menu whose presenter cannot be told
// =============================================================================

namespace {

/// EVERY SENTENCE LOOM REFUSED ON THIS BUS WHILE IT IS MOUNTED, read off the tap: the shape and
/// Loom's reason. The refusals the cases below rest on are Loom's own; this only counts them.
class RefusalTap {
public:
    explicit RefusalTap(loom::Switchboard& bus) : bus_(bus) {
        id_ = bus_.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Refused) {
                refused_.emplace_back(ev.schema_name, loom::name_of(ev.refusal.reason));
            }
        });
    }
    RefusalTap(const RefusalTap&) = delete;
    RefusalTap& operator=(const RefusalTap&) = delete;
    ~RefusalTap() { bus_.remove_observer(id_); }

    std::size_t count(const std::string& shape, const std::string& reason = std::string()) const {
        std::size_t n = 0;
        for (const std::pair<std::string, std::string>& r : refused_) {
            if (r.first == shape && (reason.empty() || r.second == reason)) {
                ++n;
            }
        }
        return n;
    }

private:
    loom::Switchboard& bus_;
    loom::ObserverId id_{};
    std::vector<std::pair<std::string, std::string>> refused_;
};

/// WORKSHOP'S VIEW OF THE BUS, WITH ONE SENTENCE THAT QUEUES NOTHING. Everything goes through the
/// Switchboard's own gated doors as Workshop -- its grant, its office authorship, every delivery
/// law -- except a `MenuWithdrawn`, which this door refuses at the enqueue with the invalid ticket a
/// Loom returns when nothing was queued (an office's authorship lost, its sequence exhausted). Only
/// that ticket is made here; the answer that follows travels Loom's real path to the real seat.
class WithdrawalQueuesNothing : public loom::Bus {
public:
    WithdrawalQueuesNothing(loom::Switchboard& sb, loom::WeaveId self) : sb_(sb), self_(self) {}
    loom::Ticket send(loom::WeaveId target, loom::Message msg) override {
        return sb_.send_as(self_, target, std::move(msg));
    }
    std::size_t publish(loom::Message msg) override { return sb_.publish_as(self_, std::move(msg)); }
    loom::Ticket send_to_role(std::string_view role, loom::Message msg) override {
        return sb_.send_as_to_role(self_, role, std::move(msg));
    }
    loom::Ticket office_send(std::string_view as_role, loom::WeaveId target,
                             loom::Message msg) override {
        return sb_.office_send_as(self_, as_role, target, std::move(msg));
    }
    loom::Ticket office_send_to_role(std::string_view as_role, std::string_view to_role,
                                     loom::Message msg) override {
        if (msg.payload.schema().name() == MenuWithdrawn::zen_name) {
            ++refused;
            return loom::Ticket{};
        }
        return sb_.office_send_to_role_as(self_, as_role, to_role, std::move(msg));
    }
    loom::OfficePublication office_publish(std::string_view as_role, loom::Message msg) override {
        return sb_.office_publish_as(self_, as_role, std::move(msg));
    }

    std::size_t refused = 0; ///< the withdrawals this door queued nothing for

private:
    loom::Switchboard& sb_;
    loom::WeaveId self_;
};

} // namespace

TEST_CASE("WL-CTX-10: a withdrawal the presenter cannot receive is answered by the host, once, under the ask's number") {
    bool presenter_left = false;
    SUBCASE("the presenter hears the withdrawal and answers it itself") {}
    SUBCASE("the presenter left while its menu was open: Loom refuses the withdrawal") {
        presenter_left = true;
    }
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    REQUIRE(t.guard->asked.pending());
    const std::uint64_t number = t.r.session().presented.correlation;
    if (presenter_left) {
        // UNLOADED THROUGH ITS CONTROL DOOR with the menu open, and nothing tells the host: the
        // menu is still on the screen.
        REQUIRE(t.r.unload("zengine-menu-presenter"));
        REQUIRE(t.foreign_open());
    }
    RefusalTap tap(t.r.bus);
    // THE PANE LEAVES THE DESK -- an ordinary withdrawal, and the requesting weave stays alive to
    // hear how its ask ended.
    t.r.pick(PaneRef{kGuardOffice, kGuardPane});
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->taken[0].empty());
    CHECK_FALSE(t.guard->pending_after[0]);
    CHECK_FALSE(t.guard->asked.pending());
    if (presenter_left) {
        // LOOM'S OWN WORD, by the withdrawal's attempt: it reached nobody -- so the host answered,
        // as its office, saying why the menu ended and what Loom said.
        CHECK(tap.count(MenuWithdrawn::zen_name, "NoSuchTarget") == 1);
        CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
        CHECK(t.guard->answers[0].refusal ==
              "the pane left the desk -- the presenter could not be told (NoSuchTarget)");
    } else {
        CHECK(tap.count(MenuWithdrawn::zen_name) == 0);
        CHECK(t.guard->answer_authors[0] == kPresenterRole);
        CHECK(t.guard->answers[0].refusal == "the pane left the desk");
    }
    // ...AND NOTHING IS KEPT once Loom has had its say.
    CHECK(t.r.w->withdrawn_menus().empty());
}

TEST_CASE("WL-CTX-10: a refused act settles its withdrawn menu, though the withdrawal reaches a fresh presenter that cannot answer it") {
    // THE MENU IS ITS WHOLE SPAN OF SENTENCES, not its withdrawal alone. The presenter is swapped
    // by an unload and a fresh load while a key and a click are in flight: the key reaches an
    // empty office and is refused, the click withdraws the menu, and by the time the withdrawal
    // is dispatched the fresh presenter holds the office -- it receives it, carries no such menu,
    // and says nothing. Loom's refusal of the key is the only word that this ask is orphaned.
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::uint64_t number = t.r.session().presented.correlation;
    RefusalTap tap(t.r.bus);
    // ONE TURN: the presenter unloads, and Down is forwarded to the menu it no longer shows.
    t.r.enqueue_unload("zengine-menu-presenter");
    (void)t.r.bus.publish(
        loom::Message(loom::to_value(input::KeyPressed{input::scan::kDown, "", input::mod::kNone}),
                      loom::WeaveId{}, loom::WeaveId{}, 0));
    (void)t.r.bus.pump_pending();
    REQUIRE_FALSE(t.r.bus.role_holder(kPresenterRole).valid());
    REQUIRE(t.foreign_open());
    // THE NEXT: the forwarded act is refused, a fresh presenter is loaded, and a right press on a
    // doorless pane withdraws the menu -- queued behind the load's own work.
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, t.r.manager);
    const loom::WeaveId booter =
        loom::mount_granted<Booter>(t.r.bus, std::move(reach), t.r.loaded, t.r.load_refusals);
    t.r.bus.send_as(booter, t.r.manager,
                    loom::Message(loom::to_value(loom::LoadWeave{"zengine-menu-presenter",
                                                                 WORKSHOP_SO_MENU_PRESENTER,
                                                                 kPresenterRole}),
                                  booter, booter, 0));
    const ui::Rect hello = t.hello_body();
    queue_button(t.r, 3, true, hello.x + 1, hello.y + 1);
    (void)t.r.bus.pump_pending();
    CHECK(tap.count(MenuInput::zen_name, "NoSuchTarget") == 1);
    REQUIRE(t.r.w->withdrawn_menus().size() == 1);
    t.r.bus.drain_until_idle();
    // THE WITHDRAWAL WAS DELIVERED -- to a presenter that cannot answer it -- and the refused act
    // is what settled the ask: once, by the host, unchosen, under its own number.
    REQUIRE(t.r.bus.role_holder(kPresenterRole).valid());
    CHECK(tap.count(MenuWithdrawn::zen_name) == 0);
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->answers[0].refusal ==
          "a newer press -- the presenter could not be told (NoSuchTarget)");
    CHECK_FALSE(t.guard->asked.pending());
    CHECK(t.r.w->withdrawn_menus().empty());
    // ...AND THE FRESH PRESENTER PRESENTS THE NEXT MENU.
    button_cell(t.r, 3, false, hello.x + 1, hello.y + 1);
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].chosen);
    CHECK(t.guard->answers[1].id == "seat.first");
    CHECK(t.guard->answer_authors[1] == kPresenterRole);
}

TEST_CASE("WL-CTX-10: an older menu's refused withdrawal ends and answers nothing newer; the newer menu is shown and chooses") {
    // THE INTERLEAVING, established rather than assumed. A requester asking when its press
    // arrives asks behind every sentence queued about the older menu, so the newer menu is not
    // open when those refusals land. It is when a pane asks under the number its press will carry
    // (the host's counter) before hearing the press, and the older withdrawal is refused while the
    // newer grant still reaches a presenter: here the presenter is killed and revived -- Loom's
    // crash-revival door -- around the older withdrawal's dispatch.
    Rigged t;
    ButtonSeat* other = nullptr;
    const loom::WeaveId other_id = mount_button_seat(t.r, other, kGuardTwoOffice, kGuardTwoPane);
    const std::int64_t other_kind = kind_of(t.r, kGuardTwoOffice, kGuardTwoPane);
    other->rows = {{"other.first", "Other first"}, {"other.second", "Other second"}};
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::int64_t older_menu = t.r.session().presented.menu;
    const std::uint64_t older = t.r.session().presented.correlation;
    RefusalTap tap(t.r.bus);
    // THE PRESENTER DIES WITH THE OLDER MENU OPEN; its state is kept to revive it from.
    const std::string held = t.r.bus.snapshot_bytes(t.presenter);
    t.r.bus.kill(t.presenter);
    // THE OTHER PANE ASKS UNDER THE NUMBER ITS PRESS WILL CARRY, and the press follows the ask
    // into the queue.
    const std::uint64_t newer = older + 1;
    other->next = [newer](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, newer); };
    (void)t.r.bus.send(other_id,
                       loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    queue_button(t.r, 3, true, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    // ONE TURN: the ask is queued, and the press withdraws the older menu.
    REQUIRE(t.r.bus.pump_pending() == 2);
    REQUIRE_FALSE(t.r.session().presented.open);
    // THE NEXT: the ask is granted -- the newer menu is open -- and the older withdrawal meets a
    // dead presenter. Loom's refusal of it is queued behind the newer grant.
    (void)t.r.bus.pump_pending();
    REQUIRE(t.r.session().presented.open);
    const std::int64_t newer_menu = t.r.session().presented.menu;
    REQUIRE(newer_menu > older_menu);
    REQUIRE(t.r.session().presented.correlation == newer);
    REQUIRE(tap.count(MenuWithdrawn::zen_name, "TargetUnavailable") == 1);
    // REVIVED before the newer grant reaches it; then everything queued is delivered.
    REQUIRE(t.r.bus.reload(t.presenter, held).revived);
    t.r.bus.drain_until_idle();
    CHECK(tap.count(MenuWithdrawn::zen_name) == 1);
    CHECK(tap.count(MenuGranted::zen_name) == 0);
    // THE NEWER MENU STANDS: the other pane's, shown, and its requester still waiting.
    REQUIRE(t.foreign_open());
    CHECK(t.r.session().presented.menu == newer_menu);
    CHECK(t.r.session().presented.office == kGuardTwoOffice);
    CHECK(other->answers.empty());
    CHECK(other->asked.pending());
    // THE OLDER ASK IS SETTLED under its own number, unchosen, whoever said it first; a second
    // word about it takes nothing.
    CHECK_FALSE(t.guard->asked.pending());
    REQUIRE_FALSE(t.guard->answers.empty());
    for (std::size_t i = 0; i < t.guard->answers.size(); ++i) {
        CHECK_FALSE(t.guard->answers[i].chosen);
        CHECK(t.guard->answer_correlations[i] == older);
        CHECK(t.guard->taken[i].empty());
    }
    // ...AND THE NEWER MENU WORKS: Down, then Return, chooses its second row, answered by the
    // presenter under the newer number.
    t.r.key(input::scan::kDown);
    t.r.key(input::scan::kReturn);
    CHECK_FALSE(t.menu_open());
    REQUIRE(other->answers.size() == 1);
    CHECK(other->answers[0].chosen);
    CHECK(other->answers[0].id == "other.second");
    CHECK(other->answer_authors[0] == kPresenterRole);
    CHECK(other->answer_correlations[0] == newer);
    CHECK(other->taken[0] == "other.second");
    CHECK(t.r.w->withdrawn_menus().empty());
}

TEST_CASE("WL-CTX-10: a withdrawal that queues nothing is answered at once, and nothing is kept") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::uint64_t number = t.r.session().presented.correlation;
    // A RIGHT PRESS INTO THE GUARD AGAIN, handled by Workshop through a view of the bus where its
    // withdrawal of the open menu queues nothing.
    WithdrawalQueuesNothing door(t.r.bus, t.r.workshop_id);
    const input::PointerButton press{3,
                                     true,
                                     body_x(t.r, t.guard_kind, 1),
                                     body_y(t.r, t.guard_kind, 0) + surface::kTuiCanvasTopRow,
                                     input::space::kCells,
                                     input::mod::kNone};
    const loom::Message in(loom::to_value(press));
    loom::Mail mail(door, in, t.r.workshop_id);
    t.r.w->on(press, mail);
    CHECK(door.refused == 1);
    CHECK_FALSE(t.r.session().presented.open);
    CHECK(t.r.w->withdrawn_menus().empty()); // no attempt, so nothing for Loom to say
    t.r.bus.drain_until_idle();
    // ANSWERED AT ONCE by the host, unchosen, under the ask's own number...
    REQUIRE_FALSE(t.guard->answers.empty());
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->answers[0].refusal ==
          "a newer press -- the presenter could not be told (nothing was queued)");
    CHECK(t.guard->taken[0].empty());
    CHECK_FALSE(t.guard->pending_after[0]);
    // ...AND THE PRESS WENT ON AS IT WOULD HAVE: the guard heard it and asked again, and that
    // menu opens and chooses, answered by the presenter. The presenter never heard the
    // withdrawal, so it still held the older menu and answered it too when the newer one arrived:
    // a second word under the older number, which takes nothing.
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.key(input::scan::kReturn);
    std::size_t chosen = 0;
    for (std::size_t i = 0; i < t.guard->answers.size(); ++i) {
        if (t.guard->answer_correlations[i] == number) {
            CHECK_FALSE(t.guard->answers[i].chosen);
            CHECK(t.guard->taken[i].empty());
            continue;
        }
        ++chosen;
        CHECK(t.guard->answers[i].chosen);
        CHECK(t.guard->answers[i].id == "seat.first");
        CHECK(t.guard->answer_authors[i] == kPresenterRole);
        CHECK(t.guard->taken[i] == "seat.first");
    }
    CHECK(chosen == 1);
    CHECK(t.r.w->withdrawn_menus().empty());
}

TEST_CASE("WL-CTX-10: a refusal notice anyone could send settles no menu, open or withdrawn") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    // THE SHAPE ALONE IS ORDINARY SPEECH. Sent through the host's root door it names a menu's
    // attempt exactly -- and carries no word of Loom's, which is what a refusal is.
    const auto forge = [&t](std::uint64_t attempt, const char* shape) {
        loom::DispatchRefused notice;
        notice.attempt = std::to_string(attempt);
        notice.role = kPresenterRole;
        notice.shape = shape;
        notice.version = 1;
        notice.reason = "NoSuchTarget";
        (void)t.r.bus.send(t.r.workshop_id, loom::Message(loom::to_value(notice)));
    };
    forge(t.r.session().presented.first_attempt, MenuGranted::zen_name);
    t.r.bus.drain_until_idle();
    REQUIRE(t.foreign_open());
    CHECK(t.guard->answers.empty());
    // ...AND ONE NAMING A WITHDRAWN MENU'S OWN WITHDRAWAL, while its record is kept.
    const ui::Rect hello = t.hello_body();
    queue_button(t.r, 3, true, hello.x + 1, hello.y + 1);
    (void)t.r.bus.pump_pending();
    REQUIRE(t.r.w->withdrawn_menus().size() == 1);
    forge(t.r.w->withdrawn_menus()[0].last_attempt, MenuWithdrawn::zen_name);
    t.r.bus.drain_until_idle();
    button_cell(t.r, 3, false, hello.x + 1, hello.y + 1);
    // THE PRESENTER'S ANSWER IS THE ONLY ONE: neither notice settled anything.
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answer_authors[0] == kPresenterRole);
    CHECK(t.guard->answers[0].refusal == "a newer press");
    CHECK_FALSE(t.guard->asked.pending());
    CHECK(t.r.w->withdrawn_menus().empty());
}

TEST_CASE("WL-CTX-10: a withdrawn menu's record is forgotten when its fence comes round twice, and ordinary use keeps none") {
    Rigged t;
    t.guard->menu_on_press = true;
    const ui::Rect hello = t.hello_body();
    for (std::size_t round = 1; round <= 3; ++round) {
        t.right_in_guard();
        t.right_in_guard(false);
        REQUIRE(t.foreign_open());
        const std::int64_t menu = t.r.session().presented.menu;
        const std::uint64_t number = t.r.session().presented.correlation;
        // A RIGHT PRESS ON A DOORLESS PANE -- nothing opens there -- withdraws the menu.
        queue_button(t.r, 3, true, hello.x + 1, hello.y + 1);
        (void)t.r.bus.pump_pending();
        // KEPT WHILE LOOM MAY STILL SAY SOMETHING ABOUT IT: whose it was, and why it ended.
        REQUIRE(t.r.w->withdrawn_menus().size() == 1);
        const WithdrawnMenu kept = t.r.w->withdrawn_menus()[0];
        CHECK(kept.menu == menu);
        CHECK(kept.office == kGuardOffice);
        CHECK(kept.pane == kGuardPane);
        CHECK(kept.correlation == number);
        CHECK(kept.why == "a newer press");
        CHECK(kept.first_attempt != 0);
        CHECK(kept.last_attempt > kept.first_attempt);
        // ...THROUGH THE FENCE'S FIRST TRIP ROUND, while the withdrawal is delivered...
        (void)t.r.bus.pump_pending();
        CHECK(t.r.w->withdrawn_menus().size() == 1);
        // ...AND FORGOTTEN ON ITS SECOND, the presenter's answer the only one given.
        t.r.bus.drain_until_idle();
        CHECK(t.r.w->withdrawn_menus().empty());
        button_cell(t.r, 3, false, hello.x + 1, hello.y + 1);
        REQUIRE(t.guard->answers.size() == round);
        CHECK(t.guard->answer_authors.back() == kPresenterRole);
        CHECK(t.guard->answers.back().refusal == "a newer press");
        CHECK_FALSE(t.guard->asked.pending());
    }
}

// =============================================================================
// An interaction the holder of the office cannot carry, given back
// =============================================================================

namespace {

/// A FRESH PRESENTER LOAD, QUEUED AND NOT DRAINED: the manager's own work takes its turns in FIFO
/// order beside whatever else a case has queued, so a maker's act can land inside the arrival --
/// after the image holds the office, before it has said what it carries.
void enqueue_presenter_load(PaneRig& r, const char* image) {
    loom::Grant reach;
    reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, r.manager);
    const loom::WeaveId booter =
        loom::mount_granted<Booter>(r.bus, std::move(reach), r.loaded, r.load_refusals);
    r.bus.send_as(booter, r.manager,
                  loom::Message(loom::to_value(loom::LoadWeave{"zengine-menu-presenter", image,
                                                               kPresenterRole}),
                                booter, booter, 0));
}

/// A KEY AT ITS PLACE IN A BURST, not drained: `PaneRig::key` publishes and drains.
void queue_key(PaneRig& r, std::int64_t scancode) {
    (void)r.bus.publish(loom::Message(
        loom::to_value(input::KeyPressed{scancode, "", input::mod::kNone}), loom::WeaveId{},
        loom::WeaveId{}, 0));
}

} // namespace

TEST_CASE("WL-CTX-10: a withdrawal a fresh holder does not carry is given back, and the host settles who asked") {
    const char* image = WORKSHOP_SO_MENU_PRESENTER;
    bool ready_first = false;
    SUBCASE("the shipped presenter, arriving with the press") {}
    SUBCASE("the shipped presenter, fully arrived before the press") { ready_first = true; }
    SUBCASE("the replacement presenter, arriving with the press") {
        image = WORKSHOP_SO_NUMBERED_PRESENTER;
    }
    SUBCASE("the replacement presenter, fully arrived before the press") {
        image = WORKSHOP_SO_NUMBERED_PRESENTER;
        ready_first = true;
    }
    Rigged t(image);
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::uint64_t number = t.r.session().presented.correlation;
    REQUIRE(t.guard->asked.pending());
    // THE HOLDER LEAVES WITH THE MENU OPEN: the requester is alive and still owed an answer.
    REQUIRE(t.r.unload("zengine-menu-presenter"));
    RefusalTap tap(t.r.bus);
    const ui::Rect hello = t.hello_body();
    std::size_t answered_at = 0;
    std::size_t forgotten_at = 0;
    if (ready_first) {
        // THE CONTROL: the fresh image finishes arriving first, so its `PresenterReady{0}` ends
        // the menu it does not carry and the host answers there. The press withdraws nothing.
        (void)t.r.load_presenter(image);
        REQUIRE(t.guard->answers.size() == 1);
        queue_button(t.r, 3, true, hello.x + 1, hello.y + 1);
        t.r.bus.drain_until_idle();
        CHECK(t.r.w->withdrawn_menus().empty());
    } else {
        // THE OVERLAP: the load and the maker's press are pending together. The image holds the
        // office before the press withdraws the menu, so the withdrawal is DELIVERED -- to an
        // image carrying no such menu, which Loom has nothing to refuse and nothing to say about.
        enqueue_presenter_load(t.r, image);
        queue_button(t.r, 3, true, hello.x + 1, hello.y + 1);
        // ONE TURN AT A TIME: the record is the host's while the withdrawal is in flight, and it
        // stops being kept only when something settles it. WHICH something is what the answer's
        // own words say below -- the fence forgetting it would leave no answer at all.
        bool kept = false;
        for (std::size_t turn = 1; turn <= 40; ++turn) {
            if (t.r.bus.pump_pending() == 0) {
                break;
            }
            if (!t.r.w->withdrawn_menus().empty()) {
                kept = true;
            } else if (kept && forgotten_at == 0) {
                forgotten_at = turn;
            }
            if (answered_at == 0 && !t.guard->answers.empty()) {
                answered_at = turn;
            }
        }
        CHECK(kept);          // who asked was kept while the withdrawal was in flight
        CHECK(forgotten_at != 0);
        CHECK(answered_at != 0);
        CHECK(tap.count(MenuWithdrawn::zen_name) == 0);
    }
    button_cell(t.r, 3, false, hello.x + 1, hello.y + 1);
    // SETTLED ONCE, BY THE HOST, under the ask's own number and unchosen, and nothing is kept.
    REQUIRE(t.r.bus.role_holder(kPresenterRole).valid());
    CHECK_FALSE(t.menu_open());
    CHECK(t.r.w->withdrawn_menus().empty());
    CHECK_FALSE(t.guard->asked.pending());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->taken[0].empty());
    CHECK_FALSE(t.guard->pending_after[0]);
    CHECK(t.guard->answers[0].refusal ==
          (ready_first ? std::string("the presenter was replaced")
                       : std::string("a newer press -- the presenter could not answer it "
                                     "(this image does not hold that menu)")));
    // ...AND THE OFFICE STILL WORKS: the next menu is presented and chosen by the fresh image.
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].chosen);
    CHECK(t.guard->answers[1].id == "seat.first");
    CHECK(t.guard->answer_authors[1] == kPresenterRole);
}

TEST_CASE("WL-CTX-10: an act reaching an image that refused the grant is given back and takes nothing") {
    const char* image = WORKSHOP_SO_MENU_PRESENTER;
    SUBCASE("the shipped presenter") {}
    SUBCASE("the replacement presenter") { image = WORKSHOP_SO_NUMBERED_PRESENTER; }
    Rigged t(image);
    // AN OFFER NO PRESENTER WILL SHOW -- a row without an id, which the presenter owns the
    // judgement of. It answers the requester itself and hands the menu straight back.
    t.guard->rows = {{"", "No id"}};
    t.guard->menu_on_press = true;
    // TURN BY TURN, so the act lands between the grant and the image's reading of it: the press,
    // the seat's ask, and then the key QUEUED BEHIND THAT ASK. The host grants the menu and
    // forwards the key to it in the same turn, and the image reads the key after refusing the
    // grant -- holding nothing, with its requester already answered.
    queue_button(t.r, 3, true, body_x(t.r, t.guard_kind, 1), body_y(t.r, t.guard_kind, 0));
    (void)t.r.bus.pump_pending(); // the host reads the press
    (void)t.r.bus.pump_pending(); // the seat hears it and asks
    queue_key(t.r, input::scan::kDown);
    (void)t.r.bus.pump_pending(); // the host grants, then forwards the key to the open menu
    const std::uint64_t number = t.r.session().presented.correlation;
    REQUIRE(number != 0);
    REQUIRE(t.r.session().presented.open);
    t.r.bus.drain_until_idle();
    t.right_in_guard(false);
    CHECK_FALSE(t.menu_open());
    CHECK(t.r.w->withdrawn_menus().empty());
    CHECK_FALSE(t.guard->asked.pending());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answer_authors[0] == kPresenterRole);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->answers[0].refusal ==
          "a row's id is empty or too long, or its label is too long");
}

TEST_CASE("WL-CTX-10: a menu its image answered is not reopened by a give-back behind it") {
    const char* image = WORKSHOP_SO_MENU_PRESENTER;
    SUBCASE("the shipped presenter") {}
    SUBCASE("the replacement presenter") { image = WORKSHOP_SO_NUMBERED_PRESENTER; }
    Rigged t(image);
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::uint64_t number = t.r.session().presented.correlation;
    // ONE BURST: the maker chooses, and a press on a doorless pane withdraws the menu behind it.
    // The image chooses and answers on the act, then meets a withdrawal for a menu it no longer
    // holds -- work it finished, not work abandoned.
    const ui::Rect hello = t.hello_body();
    queue_key(t.r, input::scan::kReturn);
    queue_button(t.r, 3, true, hello.x + 1, hello.y + 1);
    t.r.bus.drain_until_idle();
    button_cell(t.r, 3, false, hello.x + 1, hello.y + 1);
    CHECK_FALSE(t.menu_open());
    CHECK(t.r.w->withdrawn_menus().empty());
    CHECK_FALSE(t.guard->asked.pending());
    // EXACTLY ONE ANSWER, the image's own, and it is the choice.
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].id == "seat.first");
    CHECK(t.guard->answer_authors[0] == kPresenterRole);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->taken[0] == "seat.first");
}


TEST_CASE("WL-CTX-10: a give-back is one office's word about one menu, and settles no other") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.right_in_guard();
    t.right_in_guard(false);
    REQUIRE(t.foreign_open());
    const std::int64_t menu = t.r.session().presented.menu;
    const std::uint64_t number = t.r.session().presented.correlation;
    /// THE SHAPE ITSELF, AUTHORED AS THE PRESENTER'S OFFICE -- what an image says when it is
    /// handed an interaction it cannot carry. Sent from the image that holds the office, so Loom
    /// authenticates the office the way it does every other sentence of this seam.
    const auto give_back = [&t](std::int64_t which) {
        return t.r.bus
            .office_send_to_role_as(t.presenter, kPresenterRole, kWorkshopProvider,
                                    loom::Message(loom::to_value(
                                        MenuReturned{which, "this image does not hold that menu"})))
            .valid();
    };
    // NOT ANYBODY'S: the same sentence through the host's root door carries no office of Loom's.
    (void)t.r.bus.send(t.r.workshop_id,
                       loom::Message(loom::to_value(MenuReturned{menu, "forged"})));
    t.r.bus.drain_until_idle();
    CHECK(t.foreign_open());
    CHECK(t.guard->answers.empty());
    // ...AND NOT ABOUT A MENU THIS HOST NEVER GRANTED.
    REQUIRE(give_back(menu + 7));
    t.r.bus.drain_until_idle();
    CHECK(t.foreign_open());
    CHECK(t.guard->answers.empty());
    // THE OPEN MENU, GIVEN BACK BY THE OFFICE THAT HOLDS IT: nobody is left to answer it, so the
    // host ends it and settles the requester itself, unchosen, under the ask's own number.
    REQUIRE(give_back(menu));
    t.r.bus.drain_until_idle();
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answer_authors[0] == kWorkshopProvider);
    CHECK(t.guard->answer_correlations[0] == number);
    CHECK(t.guard->answers[0].refusal ==
          "the presenter could not answer it -- this image does not hold that menu");
    CHECK_FALSE(t.guard->asked.pending());
    // ...AND SAID AGAIN ABOUT THAT SETTLED MENU IT TAKES NOTHING FROM A NEWER ONE. The newer menu
    // is another pane's, so what the image says about the older one -- which it was never told
    // about and still holds, and answers when the newer grant arrives -- stays under the older
    // number and cannot be mistaken for the newer requester's.
    ButtonSeat* other = nullptr;
    const loom::WeaveId other_id = mount_button_seat(t.r, other, kGuardTwoOffice, kGuardTwoPane);
    (void)other_id;
    other->rows = {{"other.first", "Other first"}, {"other.second", "Other second"}};
    other->menu_on_press = true;
    const std::int64_t other_kind = kind_of(t.r, kGuardTwoOffice, kGuardTwoPane);
    button_cell(t.r, 3, true, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    button_cell(t.r, 3, false, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    REQUIRE(t.foreign_open());
    const std::int64_t newer = t.r.session().presented.menu;
    REQUIRE(newer > menu);
    const std::size_t said_about_older = t.guard->answers.size();
    REQUIRE(give_back(menu));
    t.r.bus.drain_until_idle();
    CHECK(t.foreign_open());
    CHECK(t.r.session().presented.menu == newer);
    CHECK(t.guard->answers.size() == said_about_older);
    CHECK(other->answers.empty());
    CHECK(other->asked.pending());
    // ...AND THE NEWER MENU IS STILL THE IMAGE'S TO ANSWER, under its own requester's number.
    t.r.key(input::scan::kReturn);
    REQUIRE(other->answers.size() == 1);
    CHECK(other->answers[0].chosen);
    CHECK(other->answers[0].id == "other.first");
    CHECK(other->answer_authors[0] == kPresenterRole);
    CHECK_FALSE(other->asked.pending());
    // NOTHING THE OLDER REQUESTER HEARD WAS A CHOICE, and every word of it was under its number.
    for (std::size_t i = 0; i < t.guard->answers.size(); ++i) {
        CHECK_FALSE(t.guard->answers[i].chosen);
        CHECK(t.guard->answer_correlations[i] == number);
        CHECK(t.guard->taken[i].empty());
    }
}

// =============================================================================
// The shipped guard example, as an image
// =============================================================================

TEST_CASE("the guard example blocks while the right button is held, lowers on the release wherever it lands, says a dropped guard, and never asks for a menu") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(140, 60);
    ProviderSeat* hello = r.mount_provider(kHelloOffice);
    r.drive(hello, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t hello_kind = r.session().panels.runtime.entries[0].kind;
    const loom::WeaveId id = r.load("zengine-example-guard", WORKSHOP_SO_GUARD, "example.guard");
    REQUIRE(id.valid());
    REQUIRE(r.load_refusals.empty());
    const PaneRef guard{"example.guard", "guard"};
    r.pick(guard);
    const std::int64_t kind = kind_of(r, "example.guard", "guard");
    REQUIRE(is_runtime_kind(kind));
    REQUIRE(holder_accepts_on(r.bus, "example.guard", *loom::schema_of<PaneButton>()));
    const auto first_row = [&r, kind] {
        const ExternalPane* shown = r.session().panels.external_pane(kind);
        REQUIRE(shown != nullptr);
        REQUIRE(!shown->shown.empty());
        return shown->shown[0].text;
    };
    CHECK(first_row().find("guard down") != std::string::npos);
    button_cell(r, 3, true, body_x(r, kind, 1), body_y(r, kind, 0));
    CHECK(first_row().find("GUARD UP") != std::string::npos);
    CHECK_FALSE(r.session().context.open);
    // RELEASED OVER HELLO: the guard hears it, Hello does not.
    const ui::Rect hb = pane_body_cells(external_panel_rect(r.session(), hello_kind));
    button_cell(r, 3, false, hb.x + 1, hb.y + 2);
    CHECK(first_row().find("guard down") != std::string::npos);
    CHECK(hello->presses.empty());
    // CLOSED WHILE HELD: the `lost` release says the host could not keep the hold.
    button_cell(r, 3, true, body_x(r, kind, 1), body_y(r, kind, 0));
    CHECK(first_row().find("GUARD UP") != std::string::npos);
    r.pick(guard);
    r.pick(guard);
    CHECK(first_row().find("guard dropped") != std::string::npos);
    CHECK_FALSE(r.session().context.open);
    // THE CHROME IS STILL THE HOST'S ROUTE to its own menu.
    const ui::Rect body = external_body_rect(r.session(), kind);
    button_cell(r, 3, true, body.x + 2, body.y);
    CHECK(r.session().context.open);
    CHECK(r.session().context.pane == guard);
}
