// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
//
// A SMALL EXTERNAL CONSUMER OF THE PANE MENU, THE PRESENTER SEAM AND THE LIST HELPERS, from the
// installed package alone -- and a LIVE one: what it claims about a menu's answer it proves on a
// running bus, not by calling a function on a value it made up.
//
// Four participants on this program's own Loom bus, each holding an office:
//
//   zengine.workshop     a stand-in for Workshop's intake: it sends a press, judges nothing it
//                        does not have to, grants a requested menu to the presenter, forwards one
//                        act, and -- for the negative cases -- refuses an ask or tries to choose
//   zengine.presenter    a presenter a stranger writes from `workshop/presenter_vocabulary.hpp`:
//                        it shows the offered rows, chooses the cursor's row, and answers
//   stranger.requester   a pane offering two actions with `pane_menu::Offer` and reading every
//                        answer through `pane_menu::Asked::take` -- provenance, the ask's
//                        lifetime, once, and the subject, in one read
//   stranger.forger      an office that is not the presenter, answering under the ask's number
//
// The four checks the requester's record exists for are each made to fire: a forged choice, a
// choice the host tried to make, a duplicate and an answer to an ask a newer one replaced all
// act on nothing; the presenter's own answer to the requester's own ask acts once; the host's
// refusal settles. Then the list helpers a pane lays its rows out with. No path into a Zengine
// source or build tree, no private Workshop header, no copied interaction state machine; a
// failed check returns non-zero, so run.cmake's own exit test catches a helper the package
// stopped carrying or that stopped working out of tree. (The in-tree suites drive the real
// Workshop, the real shipped presenter and the real desktop; this proves the INSTALLED seam.)

#include "workshop/pane_menu.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/presenter_vocabulary.hpp"

#include "component/columns.hpp"
#include "component/held_choice.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace pm = zengine::workshop::pane_menu;
namespace ws = zengine::workshop;
namespace co = zengine::component;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("pane-menu consumer: FAILED -- %s\n", what);
        ++failures;
    }
}

constexpr const char* kWorkshop = "zengine.workshop";
constexpr const char* kRequester = "stranger.requester";
constexpr const char* kForger = "stranger.forger";
constexpr const char* kPane = "list";

/// WHAT THIS PROGRAM ASKS A PARTICIPANT TO DO NEXT -- the only way to make a weave speak as its
/// office is from inside one of its own deliveries.
struct Poke {
    std::int64_t what = 0;
    std::int64_t number = 0;
    std::string word;
    ZEN_SHAPE(Poke, 1, ZEN_FIELD(what), ZEN_FIELD(number), ZEN_FIELD(word));
};

struct Nothing {
    ZEN_SHAPE(Nothing, 1);
};

// ---- the requester: two actions, one record ---------------------------------------------------

class Requester : public loom::WeaveBase<Requester, Nothing,
                                         loom::Accept<ws::PaneButton, ws::PaneMenuAnswered>,
                                         loom::Emit<ws::PaneMenuRequested>> {
public:
    /// A RIGHT PRESS: offer two actions about the row under it, continuing THIS press.
    void on(const ws::PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || !b.pressed || b.button != 3) {
            return;
        }
        asked = pm::Offer(kPane, "row-" + std::to_string(b.row))
                    .at(b.row, b.column)
                    .row("mine.open", "Open")
                    .row("mine.forget", "Forget")
                    .send(mail, kRequester);
    }
    /// EVERY ANSWER, READ ONE WAY. What was taken is the whole of what this pane may act on.
    void on(const ws::PaneMenuAnswered& a, loom::Mail& mail) {
        const std::string id = asked.take(mail, a);
        taken.push_back(id);
        refusals.push_back(a.refusal);
        if (id == "mine.open") {
            opened.push_back(a.subject); // the subject is still this pane's to judge
        }
    }

    pm::Asked asked;
    std::vector<std::string> taken;
    std::vector<std::string> opened;
    std::vector<std::string> refusals; ///< the words each unchosen answer came with
};

// ---- a presenter a stranger writes ------------------------------------------------------------

class Presenter
    : public loom::WeaveBase<Presenter, ws::HeldMenu,
                             loom::Accept<ws::MenuGranted, ws::MenuInput, ws::MenuWithdrawn, Poke>,
                             loom::Emit<ws::MenuShown, ws::MenuClosed, ws::MenuReturned,
                                        ws::PaneMenuAnswered>> {
public:
    void on(const ws::MenuGranted& g, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || g.rows.empty()) {
            return;
        }
        state_ = ws::HeldMenu{g.menu,  static_cast<std::int64_t>(mail.correlation()),
                              g.office, g.pane, g.subject, g.rows, 0, g.room_rows, g.room_columns};
        ws::MenuShown shown;
        shown.menu = g.menu;
        shown.picture = 1;
        for (std::size_t i = 0; i < g.rows.size(); ++i) {
            shown.lines.push_back(zengine::surface::SurfaceTextRow{
                std::string(i == 0 ? "> " : "  ") + g.rows[i].label, 0});
        }
        (void)mail.as_role(ws::kPresenterRole).send_to_role(kWorkshop, shown);
    }
    void on(const ws::MenuInput& in, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || in.menu <= 0) {
            return;
        }
        if (in.menu != state_.menu) {
            give_back(in.menu, mail); // an act for a menu this image never held
            return;
        }
        if (in.kind == ws::menu_input::kKey && in.verb == ws::menu_verb::kChoose) {
            const std::string id = state_.rows[static_cast<std::size_t>(state_.cursor)].id;
            (void)mail.as_role(ws::kPresenterRole)
                .send_to_role(kWorkshop, ws::MenuClosed{state_.menu, true, in.input});
            answer(mail, true, id);
            last_ = state_;
            state_ = ws::HeldMenu{};
        }
    }
    /// THE HOST ENDED A MENU. One this image holds it answers; one it does not, it GIVES BACK
    /// -- the seam's word for "this requester has no answer coming from me".
    void on(const ws::MenuWithdrawn& w, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshop) || w.menu <= 0) {
            return;
        }
        if (w.menu != state_.menu) {
            give_back(w.menu, mail);
            return;
        }
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(kWorkshop, ws::MenuClosed{state_.menu, false, 0});
        answer(mail, false, std::string());
        state_ = ws::HeldMenu{};
    }
    /// A PRESENTER WITH A BUG: it says its last answer again (1), or answers late, under a number
    /// and about a subject it is told (2) -- its own office's word either way.
    void on(const Poke& p, loom::Mail& mail) {
        if (p.what == 1 && last_.menu != 0) {
            const ws::HeldMenu now = state_;
            state_ = last_;
            answer(mail, true, last_.rows[0].id);
            state_ = now;
        } else if (p.what == 3) {
            // A HOLDER THAT CARRIES NO MENU: what the office looks like from the outside when one
            // image replaces another in the middle of an interaction.
            state_ = ws::HeldMenu{};
        } else if (p.what == 2) {
            (void)mail.as_role(ws::kPresenterRole)
                .send_to_role(kRequester, ws::PaneMenuAnswered{kPane, p.word, true, "mine.open", ""},
                              static_cast<std::uint64_t>(p.number));
        }
    }

private:
    void give_back(std::int64_t menu, loom::Mail& mail) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(kWorkshop, ws::MenuReturned{menu, "this image does not hold that menu"});
    }
    void answer(loom::Mail& mail, bool chosen, const std::string& id) {
        (void)mail.as_role(ws::kPresenterRole)
            .send_to_role(state_.office,
                          ws::PaneMenuAnswered{state_.pane, state_.subject, chosen, id, ""},
                          static_cast<std::uint64_t>(state_.correlation));
    }
    ws::HeldMenu last_;
};

// ---- the stand-in host, and the forger ---------------------------------------------------------

class Host : public loom::WeaveBase<Host, Nothing,
                                    loom::Accept<ws::PaneMenuRequested, ws::MenuShown,
                                                 ws::MenuClosed, ws::MenuReturned, Poke>,
                                    loom::Emit<ws::PaneButton, ws::MenuGranted, ws::MenuInput,
                                               ws::MenuWithdrawn, ws::PaneMenuAnswered>> {
public:
    /// WHAT THIS HOST DOES WITH THE NEXT ASK: grant it, refuse it, or (a host with a bug) try to
    /// answer it chosen itself.
    enum class Mode { kGrant, kRefuse, kChoose };
    Mode mode = Mode::kGrant;

    void on(const ws::PaneMenuRequested& asked, loom::Mail& mail) {
        if (mail.authored_role() != std::string_view(kRequester)) {
            return;
        }
        if (mode == Mode::kRefuse || mode == Mode::kChoose) {
            const bool chosen = mode == Mode::kChoose;
            (void)mail.as_role(kWorkshop).send_to_role(
                kRequester,
                ws::PaneMenuAnswered{asked.pane, asked.subject, chosen,
                                     chosen ? asked.rows[0].id : std::string(),
                                     chosen ? std::string() : "late"},
                mail.correlation());
            return;
        }
        ++menus_;
        // WHO ASKED, KEPT UNTIL SOMETHING SAYS NOTHING MORE IS OWED: a `MenuClosed` (the presenter
        // ended it and answered) or a `MenuReturned` (it cannot, so this host answers). The real
        // Workshop bounds the same record by a fence of its own; a small host keeps it simpler.
        owed_ = Owed{menus_, mail.correlation(), asked.pane, asked.subject};
        (void)mail.as_role(kWorkshop).send_to_role(
            ws::kPresenterRole,
            ws::MenuGranted{menus_, kRequester, asked.pane, asked.subject, asked.rows, 8, 30},
            mail.correlation());
    }
    void on(const ws::MenuShown& shown, loom::Mail& mail) {
        if (mail.authored_from_role(ws::kPresenterRole) && shown.menu == menus_) {
            lines = shown.lines;
        }
    }
    void on(const ws::MenuClosed& closed, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kPresenterRole) || closed.menu <= 0) {
            return;
        }
        if (closed.menu == menus_) {
            chosen_at = closed.chosen ? closed.input : -1;
        }
        // THE PRESENTER ENDED IT AND ANSWERED: nothing more is owed about that menu.
        if (owed_.menu == closed.menu) {
            owed_ = Owed{};
        }
    }
    /// AN INTERACTION HANDED BACK: the presenter holds no such menu, so the requester has no
    /// answer coming from it -- and this host, which kept who asked, gives it one. Once: a menu
    /// already answered is no longer owed, and no other menu is touched.
    void on(const ws::MenuReturned& returned, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kPresenterRole) || returned.menu <= 0 ||
            owed_.menu != returned.menu) {
            return;
        }
        const Owed given = owed_;
        owed_ = Owed{};
        (void)mail.as_role(kWorkshop).send_to_role(
            kRequester,
            ws::PaneMenuAnswered{given.pane, given.subject, false, std::string(),
                                 "the host ended it -- the presenter could not answer it (" +
                                     returned.why + ")"},
            given.correlation);
    }
    void on(const Poke& p, loom::Mail& mail) {
        if (p.what == 1) { // a right press on row `number`, under gesture `number` + 40
            (void)mail.as_role(kWorkshop).send_to_role(
                kRequester, ws::PaneButton{kPane, 3, true, p.number, 2, false, 0},
                static_cast<std::uint64_t>(p.number + 40));
        } else if (p.what == 2) { // the maker chooses: one key, act number `number`
            ws::MenuInput in;
            in.menu = menus_;
            in.input = p.number;
            in.kind = ws::menu_input::kKey;
            in.verb = ws::menu_verb::kChoose;
            (void)mail.as_role(kWorkshop).send_to_role(ws::kPresenterRole, in);
        } else if (p.what == 3) { // the host ends the menu it last granted
            (void)mail.as_role(kWorkshop).send_to_role(
                ws::kPresenterRole, ws::MenuWithdrawn{menus_, "the host ended it"});
        }
    }

    std::vector<zengine::surface::SurfaceTextRow> lines;
    std::int64_t chosen_at = 0;

private:
    /// WHO IS OWED AN ANSWER ABOUT THE MENU THIS HOST GRANTED, while anybody still is.
    struct Owed {
        std::int64_t menu = 0;
        std::uint64_t correlation = 0;
        std::string pane;
        std::string subject;
    };
    std::int64_t menus_ = 0;
    Owed owed_;
};

class Forger : public loom::WeaveBase<Forger, Nothing, loom::Accept<Poke>,
                                      loom::Emit<ws::PaneMenuAnswered>> {
public:
    void on(const Poke& p, loom::Mail& mail) {
        (void)mail.as_role(kForger).send_to_role(
            kRequester, ws::PaneMenuAnswered{kPane, p.word, true, "mine.forget", ""},
            static_cast<std::uint64_t>(p.number));
    }
};

template <class T>
T* seat(loom::Switchboard& bus, loom::WeaveId& id, loom::Grant grant, const char* office) {
    auto owned = std::make_unique<T>();
    T* raw = owned.get();
    id = bus.register_weave(std::move(owned), std::move(grant), std::string(office));
    raw->zen_set_self(id);
    return raw;
}

void poke(loom::Switchboard& bus, loom::WeaveId id, Poke p) {
    (void)bus.send(id, loom::Message(loom::to_value(p), loom::WeaveId{}, loom::WeaveId{}, 0));
    bus.drain_until_idle();
}

void live_menu() {
    loom::Switchboard bus;
    loom::WeaveId host_id{};
    loom::WeaveId presenter_id{};
    loom::WeaveId requester_id{};
    loom::WeaveId forger_id{};

    loom::Grant host_speaks;
    host_speaks.allow_to_role(ws::PaneButton::zen_name, ws::PaneButton::zen_version, kRequester);
    host_speaks.allow_to_role(ws::PaneMenuAnswered::zen_name, ws::PaneMenuAnswered::zen_version,
                              kRequester);
    host_speaks.allow_to_role(ws::MenuGranted::zen_name, ws::MenuGranted::zen_version,
                              ws::kPresenterRole);
    host_speaks.allow_to_role(ws::MenuInput::zen_name, ws::MenuInput::zen_version,
                              ws::kPresenterRole);
    host_speaks.allow_to_role(ws::MenuWithdrawn::zen_name, ws::MenuWithdrawn::zen_version,
                              ws::kPresenterRole);
    Host* host = seat<Host>(bus, host_id, std::move(host_speaks), kWorkshop);

    loom::Grant presenter_speaks;
    presenter_speaks.allow_to_role(ws::MenuShown::zen_name, ws::MenuShown::zen_version, kWorkshop);
    presenter_speaks.allow_to_role(ws::MenuClosed::zen_name, ws::MenuClosed::zen_version, kWorkshop);
    presenter_speaks.allow_to_role(ws::MenuReturned::zen_name, ws::MenuReturned::zen_version,
                                   kWorkshop);
    presenter_speaks.allow_to_any(ws::PaneMenuAnswered::zen_name, ws::PaneMenuAnswered::zen_version);
    Presenter* presenter =
        seat<Presenter>(bus, presenter_id, std::move(presenter_speaks), ws::kPresenterRole);

    loom::Grant requester_speaks;
    requester_speaks.allow_to_role(ws::PaneMenuRequested::zen_name,
                                   ws::PaneMenuRequested::zen_version, kWorkshop);
    Requester* requester =
        seat<Requester>(bus, requester_id, std::move(requester_speaks), kRequester);

    loom::Grant forger_speaks;
    forger_speaks.allow_to_role(ws::PaneMenuAnswered::zen_name, ws::PaneMenuAnswered::zen_version,
                                kRequester);
    (void)seat<Forger>(bus, forger_id, std::move(forger_speaks), kForger);

    // A RIGHT PRESS ON ROW 7: the requester offers two actions; the host grants; the presenter
    // shows them. The requester's record is pending under the press's own number.
    poke(bus, host_id, Poke{1, 7, ""});
    check(requester->asked.pending(), "the ask is pending after it was sent");
    check(requester->asked.correlation() == 47, "the ask continues the press's own number");
    check(host->lines.size() == 2 && host->lines[0].text == "> Open",
          "the stranger's presenter showed the offered rows");

    // A FORGED CHOICE: an office that is not the presenter answers under the ask's number.
    poke(bus, forger_id, Poke{0, 47, "row-7"});
    check(requester->taken.size() == 1 && requester->taken[0].empty(),
          "a choice from an office that is not the presenter is taken as nothing");
    check(requester->asked.pending(), "...and it settles nothing: the ask is still pending");

    // THE MAKER CHOOSES: the presenter's own answer to the requester's own ask acts, once.
    poke(bus, host_id, Poke{2, 12, ""});
    check(requester->taken.size() == 2 && requester->taken[1] == "mine.open",
          "the presenter's answer to this image's ask is taken, with its row");
    check(requester->opened.size() == 1 && requester->opened[0] == "row-7",
          "the answer is about the subject asked");
    check(!requester->asked.pending(), "the ask is settled");
    check(host->chosen_at == 12, "the presenter closed the menu naming the act that chose");

    // A DUPLICATE: the presenter says it again. Nothing is pending, so nothing is taken.
    poke(bus, presenter_id, Poke{1, 0, ""});
    check(requester->taken.size() == 3 && requester->taken[2].empty(),
          "a duplicate answer finds the ask already settled");
    check(requester->opened.size() == 1, "...and opens nothing a second time");

    // THE HOST'S REFUSAL SETTLES AN ASK IT NEVER PRESENTED.
    host->mode = Host::Mode::kRefuse;
    poke(bus, host_id, Poke{1, 2, ""});
    check(requester->taken.size() == 4 && requester->taken[3].empty(),
          "the host's refusal is taken as nothing chosen");
    check(!requester->asked.pending(), "...and it settles the ask");

    // A HOST THAT TRIES TO CHOOSE: its word settles nothing -- a choice is the presenter's.
    host->mode = Host::Mode::kChoose;
    poke(bus, host_id, Poke{1, 3, ""});
    check(requester->taken.size() == 5 && requester->taken[4].empty(),
          "a choice authored by the host is taken as nothing");
    check(requester->asked.pending(), "...and the ask is still pending");

    // A NEWER ASK REPLACES AN OLDER ONE: the older one's answer matches nothing.
    host->mode = Host::Mode::kGrant;
    poke(bus, host_id, Poke{1, 4, ""}); // ask under 44, granted
    const std::size_t before = requester->taken.size();
    poke(bus, host_id, Poke{1, 5, ""}); // a newer ask under 45 replaces the record
    poke(bus, presenter_id, Poke{2, 44, "row-4"}); // the presenter's own word, about the old ask
    check(requester->taken.size() == before + 1 && requester->taken.back().empty(),
          "an answer to an ask a newer one replaced is taken as nothing");
    check(requester->asked.pending() && requester->asked.correlation() == 45,
          "...and the newer ask is still the one pending");
    poke(bus, host_id, Poke{2, 20, ""}); // the maker chooses on the newer menu
    check(requester->taken.back() == "mine.open", "the newer ask's own answer is taken");
    check(requester->opened.back() == "row-5", "...about the newer ask's subject");

    // AN INTERACTION THE PRESENTER CANNOT CARRY, GIVEN BACK ACROSS THE INSTALLED SEAM. One image
    // of a presenter is replaced by another in the middle of a menu -- here, this presenter
    // forgetting what it held, which is what the office looks like from outside when that
    // happens. The host's cancellation then reaches a holder that carries no such menu, so it
    // hands the interaction back (`MenuReturned`) rather than saying it closed a menu it never
    // answered; the host, which kept who asked, answers that requester itself.
    poke(bus, host_id, Poke{1, 6, ""}); // ask under 46, granted and shown
    check(requester->asked.pending(), "the ask is pending before the office changes hands");
    poke(bus, presenter_id, Poke{3, 0, ""}); // a holder that carries no menu
    poke(bus, host_id, Poke{3, 0, ""});      // ...and the host ends the menu
    check(requester->taken.back().empty(), "a given-back interaction is taken as nothing chosen");
    check(!requester->asked.pending(), "...and it settles the requester the host kept");
    check(requester->refusals.back() ==
              "the host ended it -- the presenter could not answer it (this image does not hold "
              "that menu)",
          "...in words naming both why the menu ended and why nobody could answer it");

    // ...AND A MENU THE PRESENTER ANSWERED IS NOT SETTLED A SECOND TIME BY A GIVE-BACK BEHIND IT.
    // `MenuClosed` is the word that nothing more is owed, so the host stops keeping who asked;
    // the cancellation that follows finds an image holding nothing, and its give-back takes
    // nothing. The two words are different facts, and a host that read them as one would answer
    // this requester twice.
    poke(bus, host_id, Poke{1, 8, ""});  // ask under 48, granted and shown
    poke(bus, host_id, Poke{2, 30, ""}); // the maker chooses: the presenter answers and closes
    check(requester->taken.back() == "mine.open", "the choice on that menu is taken");
    const std::size_t settled = requester->taken.size();
    poke(bus, host_id, Poke{3, 0, ""}); // the host ends it late; the image holds nothing
    check(requester->taken.size() == settled,
          "a give-back about a menu its presenter already answered takes nothing");
    check(!requester->asked.pending(), "...and the settled ask stays settled");
    (void)presenter;
}

void list_helpers() {
    // A HELD SECONDARY BUTTON, the whole bookkeeping a game-like pane needs.
    pm::HeldButton held;
    check(held.take(ws::PaneButton{"my.pane", 3, true, 0, 0, false, 0}), "a press moved the state");
    check(held.held, "the button is now held");
    check(!held.take(ws::PaneButton{"my.pane", 2, false, 0, 0, false, 0}),
          "another button's release is nothing");
    check(held.take(ws::PaneButton{"my.pane", 3, false, 0, 0, false, 0}), "the matching release moved it");
    check(!held.held, "the button is no longer held");

    // A LIST PICTURE, numbered by what its rows MEAN: a recomposition that moves no row keeps the
    // number; one that changes a row's meaning bumps it.
    co::RowMap<std::string> map;
    map.begin();
    map.row(0, std::string("alpha"));
    map.row(1, std::string("beta"));
    const std::int64_t first = map.settle();
    map.begin();
    map.row(0, std::string("alpha"));
    map.row(1, std::string("beta"));
    check(map.settle() == first, "an equal recomposition keeps the picture number");
    map.begin();
    map.row(0, std::string("gamma")); // the row now MEANS a different subject
    map.row(1, std::string("beta"));
    check(map.settle() != first, "a changed subject bumps the picture number");
    check(map.at_row(0) != nullptr && *map.at_row(0) == "gamma", "the row reads back its meaning");

    // A CURSOR-ANCHORED WINDOW over a list too tall for its room, and a table laid out in columns.
    const co::ListWindow window = co::cursor_window(20, 15, 0, 6);
    check(window.count > 0 && window.count <= 6, "the window fits the budget");
    check(window.shows(15), "the cursor's row is visible");

    co::HeldChoice<std::string> choice;
    const std::vector<std::string> rows = {"a", "b", "c"};
    choice.hold(rows, 1, [](const std::string& s) { return s; });
    choice.find(rows, [](const std::string& s) { return s; });
    check(choice.chosen && !choice.lost && choice.at == 1, "the choice is held by identity");

    const std::vector<co::Column> asks = {co::Column{10, 4}, co::Column{6, 3}};
    const std::vector<std::size_t> widths = co::layout_columns(asks, 20);
    check(widths.size() == 2, "two columns were laid out");
    const std::string line = co::table_line(
        {"key", "label"}, widths,
        [](const std::string& t, std::int64_t) { return t; },
        [](std::string t, std::size_t w) { t.resize(w, ' '); return t; });
    check(!line.empty(), "a table line was composed");
}

} // namespace

int main() {
    live_menu();
    list_helpers();
    if (failures == 0) {
        std::printf("pane-menu consumer: ok -- a live menu asked, presented and answered across "
                    "four offices; forged, host-chosen, duplicate and replaced answers took "
                    "nothing; the list helpers held, numbered, windowed and tabled\n");
    }
    return failures == 0 ? 0 : 1;
}
