// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_ROLE_MANUAL_HPP
#define ZENGINE_TESTS_ROLE_MANUAL_HPP

// The role witness, shared by the suites of the roles that carry a manual: asked as any weave
// asks it, a role answers `ShapeDocumentRequested` for every shape its gate accepts from its
// page's Commands or Hears, and refuses a shape it does not accept. Reference:
// manual/docs/manual.md#an-role-answers-for-a-shape.

#include "manual/manual.hpp"
#include "manual/vocabulary.hpp"

#include "doctest.h"
#include <zen/weave/weave.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/// A READER OF AN ROLE'S PAGE: it asks what a shape does and keeps each answer with Loom's word
/// on whether it answered this asker.
struct ShapeDocumentAskerState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ShapeDocumentAskerState, 1, ZEN_FIELD(heard));
};
class ShapeDocumentAsker
    : public loom::WeaveBase<ShapeDocumentAsker, ShapeDocumentAskerState,
                             loom::Accept<zengine::manual::ShapeDocumentShown>,
                             loom::Emit<zengine::manual::ShapeDocumentRequested>> {
public:
    void on(const zengine::manual::ShapeDocumentShown& s, loom::Mail& mail) {
        heard.push_back(s);
        answers.push_back(mail.answers_ask());
    }
    std::vector<zengine::manual::ShapeDocumentShown> heard;
    std::vector<bool> answers;
};

/// ASK ONE ROLE WHAT A SHAPE DOES, as a weave would, and return its one answer.
struct ShapeDocumentReader {
    loom::Switchboard& bus;
    std::string role;
    ShapeDocumentAsker* asker = nullptr;
    loom::WeaveId id{};
    ShapeDocumentReader(loom::Switchboard& on, std::string role_of)
        : bus(on), role(std::move(role_of)) {
        auto held = std::make_unique<ShapeDocumentAsker>();
        asker = held.get();
        loom::Grant grant;
        grant.allow_to_role(zengine::manual::ShapeDocumentRequested::zen_name,
                            zengine::manual::ShapeDocumentRequested::zen_version, role);
        id = bus.register_weave(std::move(held), std::move(grant));
        asker->zen_set_self(id);
    }
    ~ShapeDocumentReader() { bus.unregister_weave(id); }
    ShapeDocumentReader(const ShapeDocumentReader&) = delete;
    ShapeDocumentReader& operator=(const ShapeDocumentReader&) = delete;

    zengine::manual::ShapeDocumentShown ask(const std::string& shape, std::int64_t version,
                                            const std::string& content_id = std::string()) {
        asker->heard.clear();
        asker->answers.clear();
        (void)bus.send_as_to_role(
            id, role,
            loom::Message(loom::to_value(zengine::manual::ShapeDocumentRequested{shape, version,
                                                                                 content_id}),
                          id, loom::WeaveId{}, 7));
        bus.drain_until_idle();
        REQUIRE(asker->heard.size() == 1);
        CHECK(asker->answers[0]); // the answer to this asker's own ask, and to it alone
        return asker->heard[0];
    }
};

/// THE NAMES OF THE SHAPES AN ROLE'S HOLDER ACCEPTS that its page is held to describe: its gate,
/// Loom's own doors and the document door aside, each name once.
inline std::vector<std::string> described_shapes_of(loom::Switchboard& bus, loom::WeaveId holder) {
    std::vector<std::string> out;
    for (const auto& schema : bus.accepted_schemas(holder)) {
        const std::string& name = schema->name();
        if (name.rfind("zen.", 0) == 0 ||
            name == zengine::manual::ShapeDocumentRequested::zen_name) {
            continue;
        }
        if (std::find(out.begin(), out.end(), name) == out.end()) out.push_back(name);
    }
    return out;
}

/// THE PAGE COVERS THE GATE: every shape the role's holder accepts, in every version it accepts,
/// is answered from a section under Commands or Hears, with words and no refusal; a shape it does
/// not accept is refused with none. Returns how many shapes it asked about.
inline std::size_t check_role_manual(loom::Switchboard& bus, const std::string& role) {
    const loom::WeaveId holder = bus.role_holder(role);
    REQUIRE_MESSAGE(holder.valid(), "nobody holds " << role);
    ShapeDocumentReader reader(bus, role);
    std::size_t asked = 0;
    for (const auto& schema : bus.accepted_schemas(holder)) {
        const std::string& name = schema->name();
        if (name.rfind("zen.", 0) == 0 ||
            name == zengine::manual::ShapeDocumentRequested::zen_name) {
            continue;
        }
        INFO(role, " accepts ", name, " v", schema->version());
        const zengine::manual::ShapeDocumentShown said =
            reader.ask(name, static_cast<std::int64_t>(schema->version()));
        CHECK(said.refusal.empty());
        CHECK(said.current);
        CHECK_FALSE(said.text.empty());
        CHECK((zengine::manual::is_command_group(said.group) ||
               zengine::manual::is_heard_group(said.group)));
        CHECK(said.content_id == zengine::manual::content_id_text(schema->content_id()));
        ++asked;
    }
    const zengine::manual::ShapeDocumentShown none = reader.ask("NoSuchShapeRequested", 1);
    CHECK_FALSE(none.refusal.empty());
    CHECK(none.text.empty());
    return asked;
}

/// ...AND THE PAGE NAMES NO OTHER: every section under Commands or Hears, but a group's opening
/// words, is about a shape the role accepts.
inline void check_page_names_accepted(const zengine::manual::Manual& page,
                                      const std::vector<std::string>& accepted) {
    for (std::size_t i = 0; i < page.count; ++i) {
        const zengine::manual::Section& s = page.sections[i];
        const bool described = zengine::manual::is_command_group(s.group) ||
                               zengine::manual::is_heard_group(s.group);
        if (!described || s.key == "commands" || s.key == "hears" ||
            s.key.substr(0, 9) == "commands-") {
            continue;
        }
        INFO(std::string(page.role), " describes ", std::string(s.key));
        CHECK(std::find(accepted.begin(), accepted.end(), std::string(s.key)) != accepted.end());
    }
}

#endif // ZENGINE_TESTS_ROLE_MANUAL_HPP
