// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_PANE_STAND_IN_HPP
#define ZENGINE_TESTS_PANE_STAND_IN_HPP

// A pane's own suite with no Workshop in it: a bus the test owns, the kernel loading the pane's
// built image as a plan row loads it, and stand-ins in the roles a pane speaks to. One holds
// Workshop's role: it hears what the pane says to Workshop, answers nothing, and sends the pane
// what Workshop sends it, a key or the action a key resolves to. A window of characters and the
// graphics directory beside it give the pane the area it draws in and hold the picture it draws
// there. Every fact a case reads is a message on that bus: what the pane said to Workshop's role,
// what it drew, and what it answers an asker. It includes the pane contract's vocabularies, the
// graphics contract's, the manual's and Loom's alone; its bodies are compiled once
// (tests/pane_stand_in.cpp, the zengine-pane-stand-in library). And the check that a pane's
// manual answers for exactly the commands it declares, which Workshop's suites use too.

#include "doctest.h"

#include "graphics/operands.hpp"
#include "manual/manual.hpp"
#include "pane/document.hpp"
#include "pane/vocabulary.hpp"

#include <zen/kernel/kernel.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace zengine::testing {

/// One action row a pane declared, with the gesture it gave it by default.
struct DeclaredRow {
    std::string id;
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;
};

/// A BUS WITH A PANE LOADED BESIDE A STAND-IN FOR WORKSHOP: the kernel and its Weave Manager
/// mounted as a host mounts them, the stand-in holding Workshop's role, a window of characters and
/// the graphics directory, and a tap recording every delivery or refusal addressed to Workshop's
/// role.
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

    /// THE AREA `owner` DRAWS IN, given by the TUI window's stand-in as a window of characters gives
    /// it: a room of `columns` by `rows` cells, told now and told again whenever the owner asks the
    /// graphics directory for its room, as an owner that starts again does. Another area an owner
    /// asks for is given no room.
    void show(const std::string& owner, const std::string& area, std::int64_t columns,
              std::int64_t rows);
    /// THE TEXT OF THE PICTURE DRAWN IN `area`, as the window holds it after every drawing sent
    /// there: each text node's words, in the window's order, one line each.
    std::string picture(const std::string& area) const;
    /// A KEY WORKSHOP SENDS THE PANE `pane` OF `role`, as Workshop's role, while that pane holds
    /// the keyboard: the action whose row the pane declared with the key as its gesture, or the key
    /// itself where no row does. The rows' own gestures decide, as they do for a weaver whose
    /// keymap moves none.
    void key(const std::string& role, const std::string& pane, std::int64_t scancode,
             std::int64_t modifiers = 0);
    /// RELOADS THE WEAVE LOADED AS `name` IN PLACE from `path`, asked of the Weave Manager as a
    /// host asks it, so the kernel's control door reloads it and announces the new incarnation, and
    /// drains the bus, so that incarnation speaks and draws again: the refusal, or "" when the
    /// manager acknowledged it.
    std::string reload(const std::string& name, const std::string& path);
    /// The weave loaded as `name`, as the kernel holds it.
    loom::WeaveId weave_id(const std::string& name) const;
    /// A COPY OF `image` in a directory this rig owns and removes, named `prefix` and the image's
    /// file name, so a reload opens a file of its own as a rebuilt product would. The copy keeps
    /// the image's suffix: Windows' loader adds `.dll` to a name that has none.
    std::string copy_of(const std::string& image, const std::string& prefix);

    loom::Switchboard bus;
    /// What was said to Workshop's role, `<shape>/<version>` and ` refused` when it was refused.
    std::vector<std::string> to_workshop;
    /// ...and what the stand-in sent as Workshop's role, spelled the same.
    std::vector<std::string> from_workshop;
    /// The ids of the rows a pane said to Workshop's role last, in whichever version it spoke.
    std::vector<std::string> actions;
    std::vector<std::string> refusals;
    /// Who holds Workshop's role: the stand-in.
    loom::WeaveId holder;

    /// What the window and the directory stand-ins heard, by the rig's own weaves: each drawing as
    /// the window holds it, an area's nodes in its order, and the areas their owners asked a room
    /// for and were not yet answered.
    std::map<std::string, std::vector<zengine::graphics::Node>> drawn;
    std::vector<std::string> asked;

private:
    /// Drains the bus, then answers each room asked for an area the window shows, until no ask
    /// is left unanswered.
    void settle();
    void tell_room(const std::string& area);

    struct Shown {
        std::string owner;
        std::int64_t columns = 0, rows = 0;
    };
    /// The rig's directory of copies, removed once the kernel that may hold one open is gone:
    /// declared before the kernel, it is destroyed after it.
    struct OwnedDir {
        std::filesystem::path path;
        ~OwnedDir();
    };

    OwnedDir dir_;
    loom::Kernel kernel_;
    loom::WeaveId control_;
    loom::WeaveId manager_;
    loom::WeaveId window_;
    loom::WeaveId directory_;
    std::vector<std::string> loaded_;
    std::size_t acked_ = 0;
    loom::ObserverId tap_ = 0;
    /// The rows each pane declared last, by the role that spoke them and the pane's key.
    std::map<std::pair<std::string, std::string>, std::vector<DeclaredRow>> declared_;
    std::map<std::string, Shown> shown_;
    std::int64_t epoch_ = 0;
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
