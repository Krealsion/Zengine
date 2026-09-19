// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
//
// A SMALL EXTERNAL CONSUMER OF THE PANE MENU AND LIST HELPERS, from the installed package alone.
//
// The review's fifth finding asked for this specifically: the guard example is in-tree and
// public_surface.cpp only INCLUDES the protocol header. This is a stranger that USES the
// helpers -- it offers two actions, reads a choice with the safe provenance-checking read, holds
// a secondary button, numbers a list picture, windows a cursor and lays out a table -- with no
// path into a Zengine source or build tree, no private Workshop header, and no copied interaction
// state machine. Every include is spelled as the documentation spells it; the install layout is
// what makes that identity hold. A failed check returns non-zero, so run.cmake's own exit test
// catches a helper the package stopped carrying or that stopped working out of tree.

#include "workshop/pane_menu.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "component/columns.hpp"
#include "component/held_choice.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"

#include <cstdio>
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

} // namespace

int main() {
    // A CONSUMER OFFERS TWO ACTIONS about its own subject, beside a place in its room. It builds
    // the request row by row and never touches the wire shape's fields by hand.
    pm::Offer offer("my.pane", "row-7");
    offer.at(3, 5).row("mine.open", "Open").row("mine.forget", "Forget");
    check(!offer.empty(), "an offer with rows is not empty");
    check(offer.request().rows.size() == 2, "two rows were offered");
    check(offer.request().rows[0].id == "mine.open", "the first row's id is kept");
    check(offer.request().rows[1].label == "Forget", "the second row's label is kept");
    check(offer.request().subject == "row-7", "the subject is the pane's own word");

    // THE SAFE READ ON AN ANSWER: the payload half a consumer needs. (The provenance half,
    // `chosen_from`, takes the live `loom::Mail` of a delivery and is exercised in the suites;
    // here the header's presence and self-containment are what the install must carry.)
    const ws::PaneMenuAnswered chose{"my.pane", "row-7", true, "mine.open", ""};
    check(pm::chosen(chose, "my.pane", "mine.open"), "the chosen row is recognised");
    check(!pm::chosen(chose, "my.pane", "mine.forget"), "an unchosen row is not");
    const ws::PaneMenuAnswered refused{"my.pane", "row-7", false, "", "late"};
    check(!pm::chosen(refused, "my.pane", "mine.open"), "a refusal chose nothing");

    // A HELD SECONDARY BUTTON, the whole bookkeeping a game-like pane needs.
    pm::HeldButton held;
    check(held.take(ws::PaneButton{"my.pane", 3, true, 0, 0, false, 0}), "a press moved the state");
    check(held.held, "the button is now held");
    check(!held.take(ws::PaneButton{"my.pane", 2, false, 0, 0, false, 0}), "another button's release is nothing");
    check(held.take(ws::PaneButton{"my.pane", 3, false, 0, 0, false, 0}), "the matching release moved it");
    check(!held.held, "the button is no longer held");

    // A LIST PICTURE, numbered by what its rows MEAN: a recomposition that moves no row keeps the
    // number; one that changes a row's meaning bumps it -- the targeting the corrections rest on.
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

    if (failures == 0) {
        std::printf("pane-menu consumer: ok -- offered, chose, held, numbered, windowed, tabled\n");
    }
    return failures == 0 ? 0 : 1;
}
