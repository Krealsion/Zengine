// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_GRAPH_HPP
#define ZENGINE_FLOW_GRAPH_HPP

#include "maker/definition.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::flow {

inline std::string binding_text(const op::Binding& binding) {
    switch (binding.from()) {
    case op::Binding::From::Input: return "$" + binding.input_name();
    case op::Binding::From::Node: return "%" + std::to_string(binding.node_index());
    case op::Binding::From::Constant:
        return binding.constant_cell().kind() == loom::Kind::Bool ?
            (binding.constant_cell().as_bool() ? "true" : "false") :
            std::to_string(binding.constant_cell().as_int());
    }
    return {};
}

inline std::vector<std::string> graph_lines(const maker::Definition& definition) {
    std::vector<std::string> out{definition.name + " / revision " + std::to_string(definition.revision)};
    for (const auto& trigger : definition.on) {
        out.push_back("on " + trigger.message->name() + " -> state." + trigger.output);
        for (std::size_t i = 0; i < trigger.body.nodes.size(); ++i) {
            const auto& node = trigger.body.nodes[i];
            std::string line = "%" + std::to_string(i) + " = " + node.identity + "(";
            for (std::size_t a = 0; a < node.arguments.size(); ++a) {
                if (a != 0) line += ", ";
                line += binding_text(node.arguments[a]);
            }
            line += ")";
            if (i == trigger.body.result_node) line += " -> state." + trigger.output;
            out.push_back(std::move(line));
        }
        for (const auto& emit : trigger.emits) out.push_back("publish " + emit.message->name());
    }
    return out;
}

inline std::string xml(std::string_view text) {
    std::string out;
    for (char byte : text) {
        const auto c = static_cast<unsigned char>(byte);
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else if (c < 32 || c == 127) out += "?";
        else out += static_cast<char>(c);
    }
    return out;
}

inline std::string graph_svg(const maker::Definition& definition) {
    std::size_t width = 1220, height = 90;
    for (const auto& trigger : definition.on) {
        height += 120 + std::max(trigger.body.nodes.size(),
            definition.state->fields().size() + trigger.message->fields().size()) * 78;
        for (const auto& node : trigger.body.nodes)
            width = std::max(width, std::size_t{520} + node.identity.size() * 9);
    }
    std::ostringstream out;
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height
        << "\" role=\"img\" aria-label=\"Flow data dependencies\">"
           "<defs><marker id=\"arrow\" markerWidth=\"8\" markerHeight=\"8\" refX=\"7\" refY=\"4\" orient=\"auto\">"
           "<path d=\"M0 0 L8 4 L0 8 Z\" fill=\"#71b7b0\"/></marker></defs>"
           "<rect width=\"100%\" height=\"100%\" fill=\"#141d2b\"/>"
           "<g font-family=\"monospace\" font-size=\"14\" fill=\"#e7eef8\">"
           "<text x=\"24\" y=\"32\" font-size=\"22\">" << xml(definition.name)
        << " / revision " << definition.revision << "</text>"
           "<text x=\"24\" y=\"57\" fill=\"#aebed2\">Data edges; nodes execute top to bottom. Constants appear in arguments.</text>";
    std::size_t top = 90;
    const auto node_width = width - 570;
    auto edge = [&](std::size_t x1, std::size_t y1, std::size_t x2, std::size_t y2) {
        out << "<path d=\"M" << x1 << ' ' << y1 << " C" << x1 + 45 << ' ' << y1
            << ' ' << (x2 > 45 ? x2 - 45 : 0) << ' ' << y2 << ' ' << x2 << ' ' << y2
            << "\" fill=\"none\" stroke=\"#71b7b0\" stroke-opacity=\".6\" marker-end=\"url(#arrow)\"/>";
    };
    for (const auto& trigger : definition.on) {
        out << "<text x=\"24\" y=\"" << top << "\" font-size=\"18\">on "
            << xml(trigger.message->name()) << "</text>";
        top += 24;
        std::vector<std::string> inputs;
        for (const auto& field : definition.state->fields()) inputs.push_back(field.name);
        for (const auto& field : trigger.message->fields()) inputs.push_back(field.name);
        for (std::size_t n = 0; n < trigger.body.nodes.size(); ++n) {
            const auto& node = trigger.body.nodes[n];
            for (const auto& binding : node.arguments) {
                if (binding.from() == op::Binding::From::Input) {
                    const auto found = std::find(inputs.begin(), inputs.end(), binding.input_name());
                    if (found != inputs.end()) edge(214, top + static_cast<std::size_t>(found - inputs.begin()) * 78 + 24,
                                                    285, top + n * 78 + 25);
                } else if (binding.from() == op::Binding::From::Node) {
                    edge(285 + node_width, top + binding.node_index() * 78 + 26, 285, top + n * 78 + 34);
                }
            }
        }
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const auto y = top + i * 78;
            out << "<rect x=\"24\" y=\"" << y << "\" width=\"190\" height=\"49\" rx=\"5\" fill=\"#233044\"/>"
                << "<text x=\"34\" y=\"" << y + 21 << "\">$" << xml(inputs[i]) << "</text>"
                << "<text x=\"34\" y=\"" << y + 40 << "\" fill=\"#aebed2\">"
                << (i < definition.state->fields().size() ? "state" : "message") << "</text>";
        }
        for (std::size_t n = 0; n < trigger.body.nodes.size(); ++n) {
            const auto y = top + n * 78;
            const auto& node = trigger.body.nodes[n];
            out << "<rect x=\"285\" y=\"" << y << "\" width=\"" << node_width
                << "\" height=\"58\" rx=\"5\" fill=\"#233b52\"/>"
                << "<text x=\"300\" y=\"" << y + 23 << "\">%" << n << " " << xml(node.identity) << "</text>"
                << "<text x=\"300\" y=\"" << y + 45 << "\" fill=\"#b4ded8\">";
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                if (i != 0) out << ", ";
                out << xml(binding_text(node.arguments[i]));
            }
            out << "</text>";
        }
        const auto result_y = top + trigger.body.result_node * 78 + 26;
        edge(285 + node_width, result_y, width - 230, result_y);
        out << "<text x=\"" << width - 215 << "\" y=\"" << result_y + 5 << "\">state."
            << xml(trigger.output) << "</text>";
        top += std::max(inputs.size(), trigger.body.nodes.size()) * 78 + 26;
        out << "<text x=\"285\" y=\"" << top << "\" fill=\"#b4ded8\">after write:";
        for (const auto& emit : trigger.emits) out << " publish " << xml(emit.message->name());
        out << "</text>";
        top += 70;
    }
    out << "</g></svg>\n";
    return out.str();
}

} // namespace zengine::flow
#endif
