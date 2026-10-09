// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_OPERATION_HPP
#define ZENGINE_WORKSHOP_PANE_OPERATION_HPP
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {
// Continue a current pane gesture, named separately from this request’s correlation. The application checks
// the initiating actor's live authority for this operation. This does not widen the pane's
// own grant or license later operations; the pane must still make its ordinary gated send.
struct PaneOperationRequested {
    std::string pane;
    std::string role;
    std::string shape;
    std::int64_t version = 0;
    std::int64_t gesture = 0;
    ZEN_SHAPE(PaneOperationRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(role), ZEN_FIELD(shape),
              ZEN_FIELD(version), ZEN_FIELD(gesture));
};
struct PaneOperationAnswered {
    bool allowed = false;
    std::string reason;
    ZEN_SHAPE(PaneOperationAnswered, 1, ZEN_FIELD(allowed), ZEN_FIELD(reason));
};

namespace v2 {
/// The same continuation, for an act of an action class (`actor_scope.hpp`): `classes` names every
/// class the act is -- `build`, `write`, `open` -- each judged against the gesture's actor's admitted
/// row, and `subject` what a class `open` opens. Naming `role`, `shape` and `version` too asks the
/// actor's live authority for that send as well; naming none asks the classes alone, for an act
/// the pane does itself (a save writes its file directly). Answered with `PaneOperationAnswered`.
/// One approval stands for the act's every beat: the pane carries it to the act's later sends
/// under the same gesture and never asks again.
struct PaneOperationRequested {
    std::string pane;
    std::string role;
    std::string shape;
    std::int64_t version = 0;
    std::int64_t gesture = 0;
    std::vector<std::string> classes;
    std::string subject;
    ZEN_SHAPE(PaneOperationRequested, 2, ZEN_FIELD(pane), ZEN_FIELD(role), ZEN_FIELD(shape),
              ZEN_FIELD(version), ZEN_FIELD(gesture), ZEN_FIELD(classes), ZEN_FIELD(subject));
};
} // namespace v2

// AN OWNER'S QUESTION ABOUT A DOOR NO GESTURE CROSSED: a message a participant sent this owner
// directly, past Workshop's dispatch. May `session` -- that message's sender -- do the act `id` of
// these `classes` on `pane`? Workshop judges from the session's admitted row and the guests file's
// host, as it judges a gesture's actor; a participant no guest door admitted is not narrowed.
struct ActorScopeRequested {
    std::int64_t session = 0;
    std::string pane;
    std::vector<std::string> classes;
    std::string id;
    ZEN_SHAPE(ActorScopeRequested, 1, ZEN_FIELD(session), ZEN_FIELD(pane), ZEN_FIELD(classes),
              ZEN_FIELD(id));
};
struct ActorScopeJudged {
    bool allowed = false;
    std::string refusal;
    ZEN_SHAPE(ActorScopeJudged, 1, ZEN_FIELD(allowed), ZEN_FIELD(refusal));
};

// A bounded observation a current gesture starts: the initiating actor approves repeated reads of
// one versioned shape at one role, about one declared subject, for this pane. It is not a grant:
// every observation first asks PaneObservationContinued, which the application re-judges (the
// pane's holder, its place on the desk, the actor's live authority) and refuses once any lapses.
// A refusal or PaneObservationEnded forgets the lease; a new one needs a new gesture.
struct PaneObservationRequested {
    std::string pane;
    std::string role;
    std::string shape;
    std::int64_t version = 0;
    std::int64_t gesture = 0;
    std::string subject;
    ZEN_SHAPE(PaneObservationRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(role), ZEN_FIELD(shape),
              ZEN_FIELD(version), ZEN_FIELD(gesture), ZEN_FIELD(subject));
};
struct PaneObservationContinued {
    std::string pane;
    std::int64_t lease = 0;
    std::string subject;
    ZEN_SHAPE(PaneObservationContinued, 1, ZEN_FIELD(pane), ZEN_FIELD(lease), ZEN_FIELD(subject));
};
/// The answer to both: `lease` names the approved observation, zero when refused.
struct PaneObservationAnswered {
    bool allowed = false;
    std::string reason;
    std::int64_t lease = 0;
    ZEN_SHAPE(PaneObservationAnswered, 1, ZEN_FIELD(allowed), ZEN_FIELD(reason), ZEN_FIELD(lease));
};
/// The pane stopped observing (pause, close, hide, a new subject). Unanswered. Lease 0 ends every
/// observation the sender holds on that pane: what an arriving image says, since it holds none.
struct PaneObservationEnded {
    std::string pane;
    std::int64_t lease = 0;
    ZEN_SHAPE(PaneObservationEnded, 1, ZEN_FIELD(pane), ZEN_FIELD(lease));
};
} // namespace zengine::workshop
#endif
