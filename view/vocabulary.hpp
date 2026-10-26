// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_VOCABULARY_HPP
#define ZENGINE_VIEW_VOCABULARY_HPP

// What a participant asks the view host, and its one answer. A session is the asker's own: the
// office that authored the ask, or else its bus-stamped identity, never a name it may guess.
// Then what a running view says of its own wiring, and the ask for it. Reference:
// view/docs/view.md.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::view {

inline constexpr const char* kViewHostRole = "zengine.view.host";
/// The key of the one pane a view offers in its own office: a running view draws in that pane's
/// area, `<name>.view`.
inline constexpr const char* kPane = "view";

/// Register a view from its description's bytes, as its own participant; its pane asks to be
/// shown.
struct ViewRun {
    std::string session;
    loom::Bytes description;
    ZEN_SHAPE(ViewRun, 1, ZEN_FIELD(session), ZEN_FIELD(description));
};

/// Register a view as `ViewRun` does, its pane asking nothing: a desk that names the pane seats
/// it where it stood, and one that does not leaves it off. What a relaunch asks for a view that
/// ran.
struct ViewResume {
    std::string session;
    loom::Bytes description;
    ZEN_SHAPE(ViewResume, 1, ZEN_FIELD(session), ZEN_FIELD(description));
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

// ---- What a running view's wiring has done, said by the view itself -------------------------

/// One intent of a running view and what its last send met. `sent` is false until a button said
/// it; then `heard` is how many accepters Loom gave that send to, `refused` how many of them
/// answered it with a refusal Loom attests answers it, and `refusal` the last such refusal's words.
/// `accepted` is that more accepters were given it than refused it: none has refused it yet,
/// which says nothing of what an accepter did with it, and a refusal arriving later can turn it.
struct ViewIntentWiring {
    std::string control; ///< the button that says it
    std::string shape;
    std::int64_t version = 0;
    bool sent = false;
    std::int64_t heard = 0;
    std::int64_t refused = 0;
    std::string refusal;
    bool accepted = false;
    ZEN_SHAPE(ViewIntentWiring, 1, ZEN_FIELD(control), ZEN_FIELD(shape), ZEN_FIELD(version),
              ZEN_FIELD(sent), ZEN_FIELD(heard), ZEN_FIELD(refused), ZEN_FIELD(refusal),
              ZEN_FIELD(accepted));
};

/// One field a label of a running view shows, and whether the view was told it at least once.
struct ViewFieldWiring {
    std::string element; ///< the label that shows it
    std::string shape;
    std::int64_t version = 0;
    std::string field;
    bool told = false;
    ZEN_SHAPE(ViewFieldWiring, 1, ZEN_FIELD(element), ZEN_FIELD(shape), ZEN_FIELD(version),
              ZEN_FIELD(field), ZEN_FIELD(told));
};

/// WHAT A RUNNING VIEW'S WIRING HAS DONE: a row for each of its intents and for each field its
/// labels show, in the description's order. A view says it as its own office, the view's name,
/// which Loom stamps on the delivery: published to every accepter when it starts and whenever a
/// row changes, and answered to an asker of `ViewWiringRequested` as it is then.
struct ViewWiring {
    std::vector<ViewIntentWiring> intents;
    std::vector<ViewFieldWiring> fields;
    ZEN_SHAPE(ViewWiring, 1, ZEN_FIELD(intents), ZEN_FIELD(fields));
};

/// Ask every running view what its wiring has done: each answers `ViewWiring` to the asker
/// alone, as its office, under the ask's correlation.
struct ViewWiringRequested {
    ZEN_SHAPE(ViewWiringRequested, 1);
};

} // namespace zengine::view
#endif
