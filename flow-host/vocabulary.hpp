// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_HOST_VOCABULARY_HPP
#define ZENGINE_FLOW_HOST_VOCABULARY_HPP

#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::flow_host {
inline constexpr const char* kFlowHostRole = "zengine.flow.host";

// Session names are scoped to the deliberately authored office (current holder only),
// or to the requesting participant's bus-stamped identity for personal speech.
// Saved bytes carry values, not senders, grants, reply provenance or destinations.
struct FlowRun {
    std::string session;
    loom::Bytes project;
    ZEN_SHAPE(FlowRun, 1, ZEN_FIELD(session), ZEN_FIELD(project));
};
struct FlowApply {
    std::string session;
    loom::Bytes definition;
    ZEN_SHAPE(FlowApply, 1, ZEN_FIELD(session), ZEN_FIELD(definition));
};
struct FlowSend {
    std::string session;
    loom::Bytes payload;
    ZEN_SHAPE(FlowSend, 1, ZEN_FIELD(session), ZEN_FIELD(payload));
};
struct FlowInspect {
    std::string session;
    std::int64_t after = 0;
    ZEN_SHAPE(FlowInspect, 1, ZEN_FIELD(session), ZEN_FIELD(after));
};
struct FlowStop {
    std::string session;
    ZEN_SHAPE(FlowStop, 1, ZEN_FIELD(session));
};
struct FlowCatalog {
    ZEN_SHAPE(FlowCatalog, 1);
};
struct FlowEvent {
    std::int64_t sequence = 0;
    std::string kind; // queued, output, refused, dispatched, dispatch-refused, handler-failed
    std::string correlation;
    loom::Bytes payload;
    std::string detail;
    ZEN_SHAPE(FlowEvent, 1, ZEN_FIELD(sequence), ZEN_FIELD(kind), ZEN_FIELD(correlation),
              ZEN_FIELD(payload), ZEN_FIELD(detail));
};
struct FlowAnswer {
    std::string session;
    std::string action;
    bool ok = false;
    std::string reason;
    std::string subject;
    loom::Bytes project;
    std::vector<FlowEvent> events;
    std::int64_t first_sequence = 0;
    std::int64_t last_sequence = 0;
    std::int64_t dropped = 0;
    std::int64_t pending = 0; // dispatch fences, not a promise of application completion
    ZEN_SHAPE(FlowAnswer, 1, ZEN_FIELD(session), ZEN_FIELD(action), ZEN_FIELD(ok),
              ZEN_FIELD(reason), ZEN_FIELD(subject), ZEN_FIELD(project), ZEN_FIELD(events),
              ZEN_FIELD(first_sequence), ZEN_FIELD(last_sequence), ZEN_FIELD(dropped), ZEN_FIELD(pending));
};
struct FlowOperator {
    std::string identity;
    loom::Bytes descriptor; // existing zengine.OperatorDesc, including nested schema closure
    ZEN_SHAPE(FlowOperator, 1, ZEN_FIELD(identity), ZEN_FIELD(descriptor));
};
struct FlowCatalogAnswer {
    bool ok = false;
    std::string reason;
    std::vector<FlowOperator> operators;
    ZEN_SHAPE(FlowCatalogAnswer, 1, ZEN_FIELD(ok), ZEN_FIELD(reason), ZEN_FIELD(operators));
};
struct FlowChanged {
    std::string session;
    ZEN_SHAPE(FlowChanged, 1, ZEN_FIELD(session));
};
} // namespace zengine::flow_host
#endif
