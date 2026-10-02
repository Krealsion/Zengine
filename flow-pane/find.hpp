// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_FIND_HPP
#define ZENGINE_FLOW_PANE_FIND_HPP
// Finding what to compose (docs/workshop/flow.md): the question Flow asks the host's discovery
// door, and what a selected input port could be filled from. Pure: the weave owns when to ask and
// whom to believe, and the door's rows are shown as it answered them, never matched again here.
#include "flow-pane/model.hpp"
#include "workshop/powers_vocabulary.hpp"
#include <zen/kernel/schema_codec.hpp> // a dropped `zen.SchemaDesc` names the shape it describes
#include <zen/registry.hpp>
#include <zen/terminal/composer.hpp> // `describe_schema`: Loom's one spelling of a type
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::flow_pane {

/// A node's words are the operator package's, so a refusal names a node as this graph shows it.
using op::node_title;

/// A fold's body slot as its node shows it: the choice still to make, or which ports its body
/// threads the count and the accumulator through.
inline std::string body_slot_text(const op::Node& node) {
    if (!node.fold) return {};
    if (node.identity.empty()) return "body = [choose]";
    return "count " + node.fold->count + ", acc " + node.fold->accumulator;
}

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
            const auto ports = m.workspace.graph.node_ports(static_cast<std::size_t>(t), n, m.palette);
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

/// The fold whose body slot is open, as its node index in the active trigger; none when its node
/// is gone, moved to another trigger, or is no fold.
inline std::optional<std::size_t> body_slot_node(const Model& m) {
    if (!m.body_slot) return std::nullopt;
    const auto& on = m.workspace.graph.project.definition.on;
    const auto t = m.workspace.active_trigger;
    if (t < 0 || static_cast<std::size_t>(t) >= on.size()) return std::nullopt;
    for (const auto& place : m.workspace.graph.places) {
        if (place.id != *m.body_slot || place.trigger != t) continue;
        const auto& nodes = on[static_cast<std::size_t>(t)].body.nodes;
        if (place.node < 0 || static_cast<std::size_t>(place.node) >= nodes.size() ||
            !nodes[static_cast<std::size_t>(place.node)].fold)
            return std::nullopt;
        return static_cast<std::size_t>(place.node);
    }
    return std::nullopt;
}

/// THE QUESTION FLOW ASKS THE DOOR: the search line's text and, with a port selected, its type as
/// what a power must yield, or with a fold's body slot open, what a fold could spend. Only powers
/// their contributors offer: a participant's own reaction, such as a running definition's trigger
/// body, is never Flow's to offer.
inline workshop::FindPowers flow_question(const Model& m) {
    workshop::FindPowers asked;
    if (!m.search.text().empty()) asked.text = m.search.text();
    if (body_slot_node(m)) asked.fits = workshop::kFitsFold;
    else if (const auto port = selected_port(m)) asked.yields = port->type;
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
            if (op::detail::same_type(m.workspace.graph.node_answer_type(m.trigger(), n, m.palette),
                                      port.field.type))
                out.push_back({"%" + std::to_string(n) + " " + node_title(on.body.nodes[n]),
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
inline constexpr std::array<std::pair<const char*, const char*>, 4> kGroups{{
    {workshop::kFormKind, "Forms"},
    {workshop::kSourceKind, "Sources"},
    {workshop::kOperatorKind, "Operators"},
    {workshop::kConversionKind, "Conversions"},
}};

/// Is `row` the one previewed? A row is known by its kind and its identity, since a form and an
/// operator may share a name; a preview of no kind, a reference's, is never the form's.
inline bool is_previewed(const Model& m, const workshop::PowerRow& row) {
    return row.identity == m.preview &&
           (m.preview_kind.empty() ? row.kind != workshop::kFormKind : row.kind == m.preview_kind);
}

/// The row the preview shows: the one chosen, while the door's last answer still lists it.
inline const workshop::PowerRow* previewed(const Model& m) {
    if (m.preview.empty() || !m.discovered_read) return nullptr;
    for (const auto& row : m.discovered.rows)
        if (is_previewed(m, row)) return &row;
    return nullptr;
}

/// The two body ports a fold over `row` could thread, every way the door's row allows: the count
/// to an Int port, the accumulator to another port of the answer's type. Read off the ports the
/// graph holds for that power; none when they are not described.
inline std::vector<op::Fold> fold_choices(const Model& m, const workshop::PowerRow& row) {
    std::vector<op::Fold> out;
    const auto found = std::find_if(m.palette.begin(), m.palette.end(),
                                    [&](const auto& p) { return p.identity == row.identity; });
    if (found == m.palette.end() || found->outputs->fields().size() != 1) return out;
    const auto& ins = found->inputs->fields();
    for (const auto& count : ins)
        for (const auto& acc : ins)
            if (count.name != acc.name && count.type.kind == loom::Kind::Int &&
                op::detail::same_type(acc.type, found->outputs->fields().front().type))
                out.push_back({count.name, acc.name});
    return out;
}

/// One thing a dropped value can become here, and the control that makes it so.
struct DropOffer {
    std::string title, action;
    std::vector<std::string> args;
};

/// The shape a dropped value offers to declare: a `zen.SchemaDesc`'s described shape, or else the
/// value's own; none when a description names shapes it does not carry.
inline std::shared_ptr<const loom::Schema> dropped_shape(const loom::Value& value) {
    if (!loom::same_identity(value.schema(), *loom::schema_desc_schema())) return value.schema_ptr();
    try {
        loom::Registry none;
        return loom::decode_schema(value, none);
    } catch (const std::exception&) {
        return nullptr;
    }
}

/// WHAT A DROPPED VALUE CAN BE HERE, in the order a maker reaches for it: an example to send when
/// the definition accepts its shape, a constant from one of its fields when it landed on a port
/// of that field's kind, and a declaration of its shape -- accepted, or emitted inside the
/// definition's namespace -- when no message of that name is declared yet. A dropped description
/// offers only the declarations.
inline std::vector<DropOffer> drop_offers(const Model& m) {
    std::vector<DropOffer> out;
    if (!m.dropped) return out;
    const auto& value = m.dropped->value;
    const auto& def = m.workspace.graph.project.definition;
    const bool description = loom::same_identity(value.schema(), *loom::schema_desc_schema());
    if (!description) {
        for (const auto& accepted : def.accepts)
            if (loom::same_identity(*accepted, value.schema())) {
                out.push_back({"Send as example", "drop-send", {}});
                break;
            }
        if (m.dropped->port) {
            Model probe = m;
            probe.filling = m.dropped->port;
            if (const auto port = selected_port(probe))
                for (const auto& f : value.schema().fields()) {
                    const auto* cell = value.get(f.name);
                    if (cell && takes_constant(*port) && cell->kind() == port->field.type.kind)
                        out.push_back({"Use " + f.name + " = " + message_draft::summary(cell) +
                                           " on %" + std::to_string(port->node) + " " +
                                           port->field.name,
                                       "drop-constant", {f.name}});
                }
        }
    }
    if (const auto shape = dropped_shape(value)) {
        const auto named = [&](const auto& shapes) {
            return std::any_of(shapes.begin(), shapes.end(),
                               [&](const auto& s) { return s->name() == shape->name(); });
        };
        const auto label = shape->name() + " v" + std::to_string(shape->version());
        if (!named(def.accepts))
            out.push_back({"Declare " + label + " as an accepted message", "drop-accept", {}});
        if (!named(def.emits) && shape->name().rfind(def.name + ".", 0) == 0)
            out.push_back({"Declare " + label + " as an emitted message", "drop-emit", {}});
    }
    out.push_back({"Cancel", "drop-cancel", {}});
    return out;
}

/// A row's classification and contributor, as the door read them off the contribution in force.
inline std::string classification_of(const workshop::PowerRow& row) {
    if (row.kind == workshop::kFormKind) return "a form the evaluator spends";
    return row.kind + ", " + row.construction + ", from " +
           (row.provider.empty() ? std::string("this host") : row.provider);
}

} // namespace zengine::flow_pane
#endif
