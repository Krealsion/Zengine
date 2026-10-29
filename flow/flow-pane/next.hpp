// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_NEXT_HPP
#define ZENGINE_FLOW_PANE_NEXT_HPP
// The rule in words, and the one thing it still needs (flow/docs/zengine.flow.md, "graph"): the
// sentence above the graph, its unfinished parts open, and the Next line under it, with the press
// that does that one thing. Pure: read off the model, and nothing done here.
#include "flow/flow-pane/find.hpp"
#include "flow/flow-pane/model.hpp"
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace zengine::flow_pane {

/// Is the port unwired: an input naming nothing?
inline bool unwired(const op::Binding& b) {
    return b.from() == op::Binding::From::Input && b.input_name().empty();
}

/// THE RULE'S PART OF THE SENTENCE: plain words, a message, a kept field, a step, or a part still
/// to choose -- a step with a port still open, or nothing chosen yet -- and what it stands for on
/// the graph, by the keys the picture lights them by: a card (`card:<name>`), a step
/// (`step:<place>`) or a place still to fill (`place:message`, `place:keep`, `place:do`).
struct Chip {
    enum class Kind { Plain, Message, Kept, Step, StepOpen, Open };
    Kind kind = Kind::Plain;
    std::string text;
    std::vector<std::string> subjects{};
};

/// The step of the active trigger kept under `place`, by its index; none where none is.
inline std::optional<std::size_t> step_at(const Model& m, std::int64_t place) {
    for (const auto& p : m.workspace.graph.places)
        if (p.id == place && p.trigger == m.workspace.active_trigger && p.node >= 0)
            return static_cast<std::size_t>(p.node);
    return std::nullopt;
}
/// The stable place step `n` of the active trigger is kept under; 0 where it has none.
inline std::int64_t place_of(const Model& m, std::size_t n) {
    for (const auto& p : m.workspace.graph.places)
        if (p.trigger == m.workspace.active_trigger && p.node == static_cast<std::int64_t>(n)) return p.id;
    return 0;
}

namespace next_detail {
/// The step's expression: its short name and what each port reads, a node's answer as that node's
/// own expression, `?` for a port unwired; deeper than four steps, `...`.
inline std::string expression(const maker::On& on, std::size_t n, int depth, bool& open) {
    if (n >= on.body.nodes.size()) return "?";
    const auto& node = on.body.nodes[n];
    if (depth > 4) return short_name(node) + "(...)";
    std::string out = short_name(node) + "(";
    for (std::size_t a = 0; a < node.arguments.size(); ++a) {
        const auto& b = node.arguments[a];
        if (a > 0) out += ", ";
        if (unwired(b)) {
            open = true;
            out += "?";
        } else if (b.from() == op::Binding::From::Node) {
            out += expression(on, b.node_index(), depth + 1, open);
        } else {
            out += b.from() == op::Binding::From::Input ? b.input_name() : flow::binding_text(b);
        }
    }
    if (node.fold && node.identity.empty()) open = true;
    return out + ")";
}
} // namespace next_detail

/// THE RULE IN ONE SENTENCE: When <its message> arrives, <what it keeps> becomes <its answer>,
/// then says <a message> with <what it keeps> -- each part not yet chosen open.
inline std::vector<Chip> sentence(const Model& m) {
    using K = Chip::Kind;
    const auto& def = m.workspace.graph.project.definition;
    std::vector<Chip> out{{K::Plain, "When"}};
    const maker::On* on = def.on.empty() ? nullptr : &def.on.at(m.trigger());
    const auto message = on ? on->message->name() : def.accepts.empty() ? std::string() : def.accepts.front()->name();
    if (!message.empty()) out.push_back({K::Message, message, {"card:message:" + message}});
    else out.push_back({K::Open, "a message", {"place:message"}});
    out.push_back({K::Plain, "arrives,"});
    if (on) out.push_back({K::Kept, on->output, {"card:write:" + on->message->name()}});
    else if (!def.state->fields().empty()) out.push_back({K::Kept, def.state->fields().front().name, {"card:kept"}});
    else out.push_back({K::Open, "something", {"place:keep"}});
    out.push_back({K::Plain, "becomes"});
    if (!on || !flow::answered(*on)) {
        out.push_back({K::Open, "...", {on ? "place:do" : "place:keep"}});
        return out;
    }
    bool open = false;
    const auto said = next_detail::expression(*on, on->body.result_node, 0, open);
    // The step it becomes stands for every step its answer reads, however deep.
    std::vector<std::string> steps;
    std::vector<std::size_t> reading{on->body.result_node};
    std::set<std::size_t> seen;
    while (!reading.empty()) {
        const auto n = reading.back();
        reading.pop_back();
        if (n >= on->body.nodes.size() || !seen.insert(n).second) continue;
        steps.push_back("step:" + std::to_string(place_of(m, n)));
        for (const auto& b : on->body.nodes[n].arguments)
            if (b.from() == op::Binding::From::Node) reading.push_back(b.node_index());
    }
    out.push_back({open ? K::StepOpen : K::Step, said, std::move(steps)});
    for (std::size_t e = 0; e < on->emits.size(); ++e) {
        const auto card = "card:say:" + on->message->name() + ":" + on->emits[e].message->name();
        out.push_back({K::Plain, e == 0 ? ", then says" : "and says"});
        out.push_back({K::Message, on->emits[e].message->name(), {card}});
        bool with = false;
        for (const auto& f : on->emits[e].fields) {
            if (!f.source) continue;
            out.push_back({K::Plain, with ? "and" : "with"});
            out.push_back({K::Kept, *f.source, {card}});
            with = true;
        }
    }
    return out;
}

/// THE ONE THING THE RULE STILL NEEDS, as the Next line says it: a lead (`Next:`, `Ready.`,
/// `Running.`, `Done.`, `Stopped.`, or `Run needs this first:` once Run was refused), what is
/// missing, the press that does it -- its label, its action and arguments, none where the maker's
/// own choice is the thing missing -- and a hint.
struct NextStep {
    enum class Tone { Todo, Good, Warn };
    Tone tone = Tone::Todo;
    std::string lead, text, button, action, hint;
    std::vector<std::string> args;
};

/// The emitted message a say would declare for the trigger writing `output`: the field's name,
/// its first letter raised, inside the definition's name.
inline std::string say_name(const std::string& output) {
    if (output.empty()) return "Said";
    auto out = output;
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = static_cast<char>(out[0] - 'a' + 'A');
    return out;
}

namespace next_detail {
inline bool same_shapes(const std::vector<std::shared_ptr<const loom::Schema>>& a,
                        const std::vector<std::shared_ptr<const loom::Schema>>& b) {
    if (a.size() != b.size()) return false;
    return std::all_of(a.begin(), a.end(), [&](const auto& x) {
        return std::any_of(b.begin(), b.end(), [&](const auto& y) { return loom::same_identity(*x, *y); });
    });
}
/// Why the draft cannot be applied to what runs, in the Next line's words; empty when it can.
inline std::string restart_reason(const maker::Definition& draft, const maker::Definition& running) {
    const std::string keeps = "Apply keeps a running rule's messages as they are.";
    if (!same_shapes(draft.emits, running.emits)) {
        const bool added = std::any_of(draft.emits.begin(), draft.emits.end(), [&](const auto& x) {
            return std::none_of(running.emits.begin(), running.emits.end(),
                                [&](const auto& y) { return x->name() == y->name(); });
        });
        return (added ? "a new message needs a restart: " : "what it says changed, so it needs a restart: ") +
               keeps;
    }
    if (!same_shapes(draft.accepts, running.accepts))
        return "what it hears changed, so it needs a restart: " + keeps;
    if (!loom::same_identity(*draft.state, *running.state))
        return "what it keeps changed, so it needs a restart: Apply keeps what a running rule keeps.";
    return {};
}
} // namespace next_detail

/// THE CANDIDATE A PORT IS WIRED FROM when Next wires a step at one press: of what is in scope of
/// its type, a field of the message not yet read by this step first, then a kept field not yet read,
/// then an earlier step's answer; none when nothing in scope has its type.
inline std::optional<std::string> wire_for(const Model& m, std::size_t n, std::size_t port,
                                           const std::set<std::string>& taken) {
    const auto& def = m.workspace.graph.project.definition;
    PortView view;
    try {
        const auto ports = m.workspace.graph.node_ports(m.trigger(), n, m.palette);
        view = PortView{n, port, ports.inputs->fields().at(port), ""};
    } catch (const std::exception&) {
        return std::nullopt;
    }
    const auto scope = in_scope(m, view);
    const auto pick = [&](auto&& wanted) -> std::optional<std::string> {
        for (const auto& c : scope)
            if (wanted(c) && taken.count(c.bind) == 0) return c.bind;
        return std::nullopt;
    };
    const auto is_message = [&](const InScope& c) {
        return c.bind.size() > 1 && c.bind[0] == '$' && !def.state->find(c.bind.substr(1));
    };
    const auto is_kept = [&](const InScope& c) {
        return c.bind.size() > 1 && c.bind[0] == '$' && def.state->find(c.bind.substr(1));
    };
    if (auto c = pick(is_message)) return c;
    if (auto c = pick(is_kept)) return c;
    if (auto c = pick([](const InScope&) { return true; })) return c;
    if (!scope.empty()) return scope.front().bind;
    return std::nullopt;
}

inline NextStep next_step(const Model& m) {
    using T = NextStep::Tone;
    const auto& graph = m.workspace.graph;
    const auto& def = graph.project.definition;
    NextStep out;
    out.lead = "Next:";
    const maker::On* on = def.on.empty() ? nullptr : &def.on.at(m.trigger());
    const auto wakes = on ? on->message->name() : std::string();
    if (m.running && m.running_definition) {
        const auto why = next_detail::restart_reason(def, *m.running_definition);
        if (!why.empty()) {
            out.text = why;
            out.button = "Restart";
            out.action = "restart";
            out.hint = loom::same_identity(*def.state, *m.running_definition->state)
                           ? (on ? on->output + " carries on from its saved value" : std::string())
                           : "what it keeps starts again";
            return out;
        }
        if (def.revision != m.running_definition->revision) {
            const auto lacking = m.lacks();
            if (!lacking.empty()) {
                out.text = "the change still needs: " + lacking.front();
                return out;
            }
            out.text = "it changed since it ran.";
            out.button = "Apply";
            out.action = "apply";
            out.hint = "Apply keeps what it remembers";
            return out;
        }
        // RUNNING IS DONE, whether or not it says anything: a say is offered in the Then lane, never
        // pushed as the next step.
        out.tone = T::Good;
        out.lead = "Done.";
        if (on && on->emits.empty()) {
            out.text = wakes + " wakes it: the wires and " + on->output + " follow each one.";
            return out;
        }
        out.text = wakes + " wakes it: the wires, " + (on ? on->output : std::string("what it keeps")) +
                   " and what it says follow each one.";
        if (on && !on->emits.empty())
            out.hint = "carry " + on->emits.front().message->name() + " onto a label to show it";
        return out;
    }
    const auto nudged = [&](NextStep step) {
        if (m.nudge) {
            step.lead = "Run needs this first:";
            step.tone = T::Warn;
        }
        return step;
    };
    if (!on) {
        if (def.accepts.empty()) {
            out.text = "start with a message.";
            out.button = "New message";
            out.action = "ask-message";
            out.hint = "or carry a button's message from your panel onto Flow";
            return nudged(out);
        }
        if (def.state->fields().empty()) {
            out.text = "give the rule something to keep between messages.";
            out.button = "+ keep a field";
            out.action = "ask-keep";
            out.hint = "a number that starts at 0";
            return nudged(out);
        }
        const auto& field = def.state->fields().front().name;
        out.text = "let " + def.accepts.front()->name() + " wake the rule.";
        out.button = "Use " + def.accepts.front()->name();
        out.action = "trigger";
        out.args = {"0", field};
        out.hint = field + " is what it changes";
        return nudged(out);
    }
    if (on->body.nodes.empty()) {
        out.text = "choose what " + on->output + " becomes. Pick a step in the rail, then Add to the rule.";
        out.hint = "type what it is for: add, larger";
        if (const auto* shown = previewed(m); shown && shown->kind != workshop::kFormKind) {
            const auto dot = shown->identity.rfind('.');
            out.button = "Add " + (dot == std::string::npos ? shown->identity : shown->identity.substr(dot + 1));
            out.action = "add-found";
            out.args = {shown->kind, shown->identity};
        }
        return nudged(out);
    }
    for (std::size_t n = 0; n < on->body.nodes.size(); ++n) {
        const auto& node = on->body.nodes[n];
        if (node.fold && node.identity.empty()) {
            out.text = "choose the fold's body: press its body slot, then a step.";
            return nudged(out);
        }
        std::vector<std::size_t> open;
        for (std::size_t a = 0; a < node.arguments.size(); ++a)
            if (unwired(node.arguments[a])) open.push_back(a);
        if (open.empty()) continue;
        std::set<std::string> taken;
        for (const auto& b : node.arguments)
            if (!unwired(b)) taken.insert(flow::binding_text(b));
        std::vector<std::string> args(node.arguments.size() + 1);
        args[0] = std::to_string(n);
        std::vector<std::string> names;
        bool every = true;
        for (const auto a : open) {
            const auto from = wire_for(m, n, a, taken);
            if (!from) {
                every = false;
                break;
            }
            taken.insert(*from);
            args[a + 1] = *from;
            // A field by its name, an earlier step's answer by that step's operator.
            names.push_back((*from)[0] == '%' ? step_name(on->body.nodes, flow::index_of(from->substr(1)))
                                              : from->substr(1));
        }
        const auto name = step_name(on->body.nodes, n);
        if (!every) {
            std::string port = std::to_string(open.front());
            try {
                port = graph.node_ports(m.trigger(), n, m.palette).inputs->fields().at(open.front()).name;
            } catch (const std::exception&) {
            }
            out.text = "give " + name + "'s " + port + " a value.";
            out.hint = "click the port, then a value or a constant";
            return nudged(out);
        }
        out.text = "give " + name + " its values.";
        std::string list;
        for (std::size_t i = 0; i < names.size(); ++i)
            list += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
        out.button = "Wire " + list + " into " + name;
        out.action = "bind-all";
        out.args = std::move(args);
        out.hint = "or click a port, then a value";
        return nudged(out);
    }
    const auto lacking = m.lacks();
    if (!lacking.empty()) {
        out.text = lacking.front();
        return nudged(out);
    }
    out.tone = T::Good;
    out.button = "Run";
    out.action = "run";
    if (m.ran) {
        out.lead = "Stopped.";
        out.text = "The rule is put away; what it kept is saved.";
    } else {
        out.lead = "Ready.";
        out.text = on->emits.empty() ? "Nothing is missing."
                                     : "Run it, and it will say " + on->emits.front().message->name() + ".";
    }
    return out;
}

} // namespace zengine::flow_pane
#endif
