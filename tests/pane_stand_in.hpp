// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_PANE_STAND_IN_HPP
#define ZENGINE_TESTS_PANE_STAND_IN_HPP

// A pane's own suite with no Workshop in it: a bus the test owns, the kernel loading the pane's
// built image as a plan row loads it, and a stand-in holding Workshop's role, which hears what the
// pane says to Workshop and answers nothing. Every fact a case reads is a message on that bus:
// what the pane said to Workshop's role, and what the pane answers an asker. It includes the pane
// contract's vocabularies, the manual's and Loom's alone; its bodies are compiled once
// (tests/pane_stand_in.cpp, the zengine-pane-stand-in library). And the check that a pane's
// manual answers for exactly the commands it declares, which Workshop's suites use too.

#include "doctest.h"

#include "manual/manual.hpp"
#include "pane/document.hpp"
#include "pane/vocabulary.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace zengine::testing {

/// A BUS WITH A PANE LOADED BESIDE A STAND-IN FOR WORKSHOP: the kernel and its Weave Manager
/// mounted as a host mounts them, the stand-in holding Workshop's role, and a tap recording every
/// delivery or refusal addressed to that role.
class PaneStandInRig {
public:
    PaneStandInRig();
    ~PaneStandInRig();
    PaneStandInRig(const PaneStandInRig&) = delete;
    PaneStandInRig& operator=(const PaneStandInRig&) = delete;

    /// Loads an image under `role` through the Weave Manager, as a plan row does, and drains the
    /// bus; the loaded weave, or an invalid id with the manager's refusal in `refusals`.
    loom::WeaveId load(const std::string& name, const std::string& path, const std::string& role);
    /// One role's answer to a manual ask, asked as any weave asks: the one answer, and that it was
    /// the answer to this ask.
    zengine::pane::PaneDocumentShown ask_manual(const std::string& role, const std::string& pane,
                                                const std::string& section);

    loom::Switchboard bus;
    /// What was said to Workshop's role, `<shape>/<version>` and ` refused` when it was refused.
    std::vector<std::string> to_workshop;
    /// The ids of the rows a pane said to Workshop's role last, in whichever version it spoke.
    std::vector<std::string> actions;
    std::vector<std::string> refusals;
    /// Who holds Workshop's role: the stand-in.
    loom::WeaveId holder;

private:
    loom::Kernel kernel_;
    loom::WeaveId control_;
    loom::WeaveId manager_;
    std::vector<std::string> loaded_;
    loom::ObserverId tap_ = 0;
};

/// ASKS ONE ROLE FOR A PANE'S MANUAL on a stand-in's bus.
struct StandInManualReader {
    PaneStandInRig& r;
    std::string role;
    zengine::pane::PaneDocumentShown ask(const std::string& pane, const std::string& section) {
        return r.ask_manual(role, pane, section);
    }
};

} // namespace zengine::testing

/// THE MANUAL A PANE ANSWERS, HELD TO ITS ROWS: asked whole, it answers with no refusal and its
/// commands are exactly `declared`, the ids the role's panes declare in every mode driven; asked
/// for one, each declared id is answered by its own section, under a command group. `reader` is
/// any asker with `ask(pane, section)`: Workshop's rig's or a stand-in's.
template <class Reader>
void check_answered_manual(Reader& reader, const std::string& pane, const std::set<std::string>& declared) {
    REQUIRE_FALSE(declared.empty());
    const zengine::pane::PaneDocumentShown whole = reader.ask(pane, "");
    CHECK(whole.refusal.empty());
    CHECK_FALSE(whole.content_id.empty());
    std::set<std::string> commands;
    for (const zengine::pane::PaneDocumentSection& s : whole.sections) {
        if (zengine::manual::is_command_group(s.group) && s.key != "commands" &&
            s.key.rfind("commands-", 0) != 0) {
            commands.insert(s.key);
        }
    }
    CHECK(commands == declared);
    for (const std::string& id : declared) {
        INFO("pane ", pane, ", id ", id);
        const zengine::pane::PaneDocumentShown one = reader.ask(pane, id);
        CHECK(one.refusal.empty());
        REQUIRE(one.sections.size() == 1);
        CHECK(one.sections[0].key == id);
        CHECK(zengine::manual::is_command_group(one.sections[0].group));
        CHECK(one.content_id == whole.content_id);
    }
    const zengine::pane::PaneDocumentShown missing = reader.ask(pane, "no.such-command");
    CHECK_FALSE(missing.refusal.empty());
    CHECK(missing.sections.empty());
}

#endif // ZENGINE_TESTS_PANE_STAND_IN_HPP
