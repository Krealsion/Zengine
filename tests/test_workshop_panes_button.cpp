// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE SECOND BUTTON AND THE MENU A PANE ASKS THE HOST TO PRESENT, driven through the real
// Workshop weave, the real bus and native provider seats (WL-PRESS-06, WL-CTX-08, WL-CTX-09).
// What is measured here is what a model cannot establish: delivery by the holder's door,
// release custody across panes, the interleavings an independent review reproduced, owner loss
// after the release, recipient replacement, a late menu request refused where the surface
// opens, and the presenter's whole lifecycle -- keyboard, mouse, Escape, an outside press, a
// newer menu, a pane that leaves. The shipped guard example is loaded as an image at the end.

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
                                        PaneRevealRequested>> {
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
            (void)offer(b.row, b.column).send(mail, office_);
        }
        if (b.pressed && reveal_on_press) {
            (void)mail.as_role(office_).send_to_role(kWorkshopProvider, PaneRevealRequested{pane_});
        }
    }
    void on(const PaneRevealAnswered& a, loom::Mail&) { reveals.push_back(a); }
    void on(const PaneMenuAnswered& a, loom::Mail& mail) {
        answers.push_back(a);
        answer_correlations.push_back(mail.correlation());
        if (a.chosen && manage_on_choice) {
            (void)pane_menu::manage(mail, office_, pane_, manage_office, manage_pane);
        }
    }
    void on(const PaneKey& k, loom::Mail&) { keys.push_back(k); }
    void on(const PaneTextInput&, loom::Mail&) {}
    void on(const PaneActionRequested& a, loom::Mail& mail) {
        actions.push_back(a);
        action_correlations.push_back(mail.correlation());
        if (a.id == kSeatMenuId && menu_on_action) {
            (void)offer(0, 0).send(mail, office_);
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
        (void)offer(0, 0).continuing(mail, office_, correlation);
    }

    bool pass_on_press = false;
    bool menu_on_press = false;
    bool menu_on_action = false;
    bool reveal_on_press = false;
    bool manage_on_choice = false;
    std::string manage_office;
    std::string manage_pane;
    std::string subject = "subject-1";
    std::vector<std::pair<std::string, std::string>> rows = {{"seat.first", "First row"},
                                                             {"seat.second", "Second row"}};
    std::vector<Heard> buttons;
    std::vector<PaneRevealAnswered> reveals;
    std::vector<PaneMenuAnswered> answers;
    std::vector<std::uint64_t> answer_correlations;
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
    grant.allow_to_any(PaneRevealRequested::zen_name, PaneRevealRequested::zen_version);
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

    Rigged() {
        r.mount_workshop();
        r.ready();
        r.extent(140, 60);
        hello = r.mount_provider(kHelloOffice);
        r.drive(hello, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
        r.pick(hello_ref());
        hello_kind = r.session().panels.runtime.entries[0].kind;
        guard_id = mount_button_seat(r, guard, kGuardOffice, kGuardPane);
        guard_kind = kind_of(r, kGuardOffice, kGuardPane);
    }

    ui::Rect hello_body() { return pane_body_cells(external_panel_rect(r.session(), hello_kind)); }
    void right_in_guard(bool pressed = true) {
        button_cell(r, 3, pressed, body_x(r, guard_kind, 1), body_y(r, guard_kind, 0));
    }
    bool menu_open() { return r.session().context.open; }
    bool foreign_open() { return r.session().context.open && r.session().context.foreign; }
};

} // namespace

// =============================================================================
// Custody and continuation: the corrections' thirteen, kept
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
    // recorded when the send was queued (the review's fourth finding; WL-PRESS-06 corrected).
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
// The menu a pane asks the host to present (WL-CTX-09)
// =============================================================================

TEST_CASE("WL-CTX-09: a menu requested on the press's own turn opens beside the press with the pane's rows, moves no keys and no selection, and Return returns the first row") {
    Rigged t;
    const ui::Rect hello = t.hello_body();
    t.r.press_cell(hello.x + 1, hello.y + 2);
    REQUIRE(t.r.session().panels.keyboard == t.hello_kind);
    t.guard->menu_on_press = true;
    button_cell(t.r, 3, true, body_x(t.r, t.guard_kind, 3), body_y(t.r, t.guard_kind, 1));
    REQUIRE(t.foreign_open());
    const ContextMenu& menu = t.r.session().context;
    CHECK(menu.anchored);
    CHECK(menu.anchor_x == body_x(t.r, t.guard_kind, 3));
    CHECK(menu.anchor_y == body_y(t.r, t.guard_kind, 1));
    CHECK(menu.office == kGuardOffice);
    CHECK(menu.pane == PaneRef{kGuardOffice, kGuardPane});
    CHECK(menu.subject_word == "subject-1");
    REQUIRE(menu.rows.size() == 2);
    // PRESENTED: the rows the pane wrote, and nothing about a key.
    const std::vector<std::string> painted = context_rows_on(t.r.last_canvas(), t.r.session());
    REQUIRE(painted.size() == 2);
    CHECK(painted[0] == "> First row");
    CHECK(painted[1] == "  Second row");
    // NOTHING MOVED: the keys stay with Hello, the selection where it was.
    CHECK(t.r.session().panels.keyboard == t.hello_kind);
    CHECK(t.r.session().panels.selected == t.hello_kind);
    // RETURN RETURNS: the choice goes to the office under the request's number, subject-bound.
    t.r.key(input::scan::kReturn);
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].id == "seat.first");
    CHECK(t.guard->answers[0].subject == "subject-1");
    CHECK(t.guard->answers[0].pane == kGuardPane);
    CHECK(t.guard->answer_correlations[0] == t.guard->buttons[0].correlation);
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
    CHECK(t.r.session().context.cursor == 1);
    t.right_in_guard(false);
    REQUIRE(t.guard->buttons.size() == 2); // the release, delivered under the open surface
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

TEST_CASE("WL-CTX-09: a late request is refused where the surface opens -- a newer primary press elsewhere keeps the keys it took; the review's second integration finding") {
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
    // THE KEYS ARE HELLO'S, and a key proves it.
    t.r.key(input::scan::kReturn);
    CHECK(t.hello->keys.size() == 1);
    CHECK(t.guard->keys.empty());
}

TEST_CASE("WL-CTX-09: an empty offer, an offer echoing no gesture, and one from an office that never offered the pane are refused or dropped, and nothing opens") {
    Rigged t;
    // A STRANGER OFFICE with the door and its own pane, mounted before any click so that its
    // arrival (the rig seats it with a key) is not a gesture between the click and the requests.
    ButtonSeat* stranger = nullptr;
    const loom::WeaveId stranger_id = mount_button_seat(t.r, stranger, kGuardTwoOffice, kGuardTwoPane);
    t.right_in_guard();
    t.right_in_guard(false);
    const std::uint64_t correlation = t.guard->buttons[0].correlation;
    t.guard->rows.clear();
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, correlation); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 1);
    CHECK(t.guard->answers[0].refusal.find("no rows") != std::string::npos);
    t.guard->rows = {{"a", "A"}};
    drive_seat(t.r, t.guard_id, t.guard, [](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, 0); });
    CHECK_FALSE(t.menu_open());
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].refusal.find("echoes none") != std::string::npos);
    // THE STRANGER OFFICE naming the guard's pane: no pane of its own, nothing answered.
    drive_seat(t.r, stranger_id, stranger, [correlation](ButtonSeat&, loom::Mail& m) {
        (void)m.as_role(kGuardTwoOffice)
            .send_to_role(kWorkshopProvider,
                          PaneMenuRequested{kGuardPane, "x", 0, 0, {PaneMenuRow{"a", "A"}}},
                          correlation);
    });
    CHECK_FALSE(t.menu_open());
    CHECK(stranger->answers.empty());
    CHECK(t.guard->answers.size() == 2);
    // ...AND THE SPENT PRESS CANNOT BE SPENT TWICE: a second request under it is late.
    drive_seat(t.r, t.guard_id, t.guard, [correlation](ButtonSeat& s, loom::Mail& m) { s.ask_menu(m, correlation); });
    // (the refused requests did not spend the continuation; this one opens it, unanswered yet)
    CHECK(t.guard->answers.size() == 2);
    CHECK(t.foreign_open());
}

TEST_CASE("WL-CTX-09: a newer menu replaces an open one, which is answered unchosen; one surface at a time") {
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
    REQUIRE(t.r.session().context.office == kGuardOffice);
    // A RIGHT PRESS INTO THE OTHER PANE while the guard's menu is open: the older is answered
    // unchosen, the press is offered to the other pane, and its menu takes the surface.
    button_cell(t.r, 3, true, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    REQUIRE(t.guard->answers.size() == 1);
    CHECK_FALSE(t.guard->answers[0].chosen);
    CHECK(t.guard->answers[0].refusal == "a newer press");
    REQUIRE(t.foreign_open());
    CHECK(t.r.session().context.office == kGuardTwoOffice);
    const std::vector<std::string> painted = context_rows_on(t.r.last_canvas(), t.r.session());
    REQUIRE(painted.size() == 1);
    CHECK(painted[0] == "> The other pane's row");
    button_cell(t.r, 3, false, body_x(t.r, other_kind, 1), body_y(t.r, other_kind, 0));
    t.r.key(input::scan::kReturn);
    REQUIRE(other->answers.size() == 1);
    CHECK(other->answers[0].id == "other.only");
}

TEST_CASE("WL-CTX-09: a pane that leaves the desk while its menu is open closes it, answered unchosen; a pane that consumes the press opens nothing") {
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
    CHECK(t.r.session().context.anchored);
    CHECK(t.r.session().context.anchor_y == body_y(t.r, t.guard_kind, 0));
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

TEST_CASE("WL-CTX-09: a menu with more rows than the room is windowed by the presenter, and every row is still reachable") {
    Rigged t;
    t.guard->menu_on_press = true;
    t.guard->rows.clear();
    for (int i = 0; i < 80; ++i) {
        t.guard->rows.emplace_back("seat.row-" + std::to_string(i), "Row " + std::to_string(i));
    }
    // MORE ROWS THAN THE HOST PRESENTS AT ONCE: refused as too many, and nothing opens.
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
    CHECK(t.r.session().context.cursor == kMaxPaneMenuRows - 1);
    t.r.key(input::scan::kReturn);
    REQUIRE(t.guard->answers.size() == 2);
    CHECK(t.guard->answers[1].id == "seat.row-" + std::to_string(kMaxPaneMenuRows - 1));
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
