// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_FIND_HPP
#define ZENGINE_FLOW_PANE_FIND_HPP
// Finding what to compose (docs/workshop/flow.md): the question Flow asks the host's discovery
// door, and what a selected input port could be filled from. Pure: the weave owns when to ask and
// whom to believe, and the door's rows are shown as it answered them, never matched again here.
#include "flow-pane/model.hpp"
#include "workshop/powers_vocabulary.hpp"
#include <zen/terminal/composer.hpp> // `describe_schema`: Loom's one spelling of a type
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::flow_pane {

/// The selected port as the graph knows it now: its node, its field, and its type in Loom's
/// spelling, which is the spelling the door matches `yields` against.
struct PortView {
    std::size_t node = 0, port = 0;
    loom::Field field;
    std::string type;
};

/// The selected port, resolved through its node's stable place in the active trigger; none when
/// that node is gone, belongs to another trigger, or its operator's ports are not described.
inline std::optional<PortView> selected_port(const Model& m) {
    if (!m.filling) return std::nullopt;
    const auto& on = m.workspace.graph.project.definition.on;
    const auto t = m.workspace.active_trigger;
    if (t < 0 || static_cast<std::size_t>(t) >= on.size()) return std::nullopt;
    const auto& nodes = on[static_cast<std::size_t>(t)].body.nodes;
    for (const auto& place : m.workspace.graph.places) {
        if (place.id != m.filling->place || place.trigger != t) continue;
        if (place.node < 0 || static_cast<std::size_t>(place.node) >= nodes.size()) return std::nullopt;
        const auto n = static_cast<std::size_t>(place.node);
        try {
            const auto& ports = m.workspace.graph.ports(m.palette, nodes[n].identity);
            const auto described = loom::describe_schema(*ports.inputs).fields;
            if (m.filling->port >= described.size()) return std::nullopt;
            return PortView{n, m.filling->port, ports.inputs->fields().at(m.filling->port),
                            described[m.filling->port].type};
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// THE QUESTION FLOW ASKS THE DOOR: the search line's text and, with a port selected, its type as
/// what a power must yield. Only powers their contributors offer: a participant's own reaction,
/// such as a running definition's trigger body, is never Flow's to offer.
inline workshop::FindPowers flow_question(const Model& m) {
    workshop::FindPowers asked;
    if (!m.search.text().empty()) asked.text = m.search.text();
    if (const auto port = selected_port(m)) asked.yields = port->type;
    asked.offered = true;
    asked.limit = workshop::kMaxPowerRows;
    return asked;
}

/// Something already in scope that could fill the selected port, and the `bind` argument that
/// wires it there.
struct InScope {
    std::string label, bind;
};

/// What is in scope for the selected port, of exactly its type: a required state field, a required
/// field of the trigger's message that no state field shadows, then each earlier node's output.
inline std::vector<InScope> in_scope(const Model& m, const PortView& port) {
    std::vector<InScope> out;
    const auto& def = m.workspace.graph.project.definition;
    const auto& on = def.on.at(m.trigger());
    for (const auto& f : def.state->fields())
        if (f.required && op::detail::same_type(f.type, port.field.type))
            out.push_back({"state." + f.name, "$" + f.name});
    for (const auto& f : on.message->fields())
        if (f.required && !def.state->find(f.name) && op::detail::same_type(f.type, port.field.type))
            out.push_back({"input." + f.name, "$" + f.name});
    for (std::size_t n = 0; n < port.node; ++n) {
        try {
            const auto& ports = m.workspace.graph.ports(m.palette, on.body.nodes[n].identity);
            if (ports.outputs->fields().size() == 1 &&
                op::detail::same_type(ports.outputs->fields().front().type, port.field.type))
                out.push_back({"%" + std::to_string(n) + " " + on.body.nodes[n].identity,
                               "%" + std::to_string(n)});
        } catch (const std::exception&) {
        }
    }
    return out;
}

/// Can the port take a typed constant? `bind` writes Int and Bool constants, and nothing else.
inline bool takes_constant(const PortView& port) {
    return port.field.type.kind == loom::Kind::Int || port.field.type.kind == loom::Kind::Bool;
}

/// The door's classifications in the order a maker reaches for them, each with its heading.
inline constexpr std::array<std::pair<const char*, const char*>, 3> kGroups{{
    {workshop::kSourceKind, "Sources"},
    {workshop::kOperatorKind, "Operators"},
    {workshop::kConversionKind, "Conversions"},
}};

/// The row the preview shows: the one chosen, while the door's last answer still lists it.
inline const workshop::PowerRow* previewed(const Model& m) {
    if (m.preview.empty() || !m.discovered_read) return nullptr;
    for (const auto& row : m.discovered.rows)
        if (row.identity == m.preview) return &row;
    return nullptr;
}

/// A row's classification and contributor, as the door read them off the contribution in force.
inline std::string classification_of(const workshop::PowerRow& row) {
    return row.kind + ", " + row.construction + ", from " +
           (row.provider.empty() ? std::string("this host") : row.provider);
}

} // namespace zengine::flow_pane
#endif
