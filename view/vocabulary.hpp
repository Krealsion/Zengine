// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_VOCABULARY_HPP
#define ZENGINE_VIEW_VOCABULARY_HPP

// What a participant asks the view host, and its one answer. A session is the asker's own: the
// office that authored the ask, or else its bus-stamped identity, never a name it may guess.
// Reference: docs/reference/view.md.

#include <zen/weave/shape.hpp>

#include <string>

namespace zengine::view {

inline constexpr const char* kViewHostRole = "zengine.view.host";

/// Register a view from its description's bytes, as its own participant.
struct ViewRun {
    std::string session;
    loom::Bytes description;
    ZEN_SHAPE(ViewRun, 1, ZEN_FIELD(session), ZEN_FIELD(description));
};

/// Change the running view: in place when its shapes are the same, else a fresh registration.
struct ViewApply {
    std::string session;
    loom::Bytes description;
    ZEN_SHAPE(ViewApply, 1, ZEN_FIELD(session), ZEN_FIELD(description));
};

/// Stop the running view: its pane says it stopped, then the participant is unregistered.
struct ViewStop {
    std::string session;
    ZEN_SHAPE(ViewStop, 1, ZEN_FIELD(session));
};

/// The host's answer to each: `office` is the view's name, `fresh` whether this ask registered a
/// participant afresh, and `reason` says what happened, or why nothing did.
struct ViewAnswer {
    std::string session;
    std::string action;
    bool ok = false;
    std::string reason;
    std::string office;
    bool fresh = false;
    ZEN_SHAPE(ViewAnswer, 1, ZEN_FIELD(session), ZEN_FIELD(action), ZEN_FIELD(ok),
              ZEN_FIELD(reason), ZEN_FIELD(office), ZEN_FIELD(fresh));
};

} // namespace zengine::view
#endif
