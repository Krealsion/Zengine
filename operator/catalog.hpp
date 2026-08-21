// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_CATALOG_HPP
#define ZENGINE_OPERATOR_CATALOG_HPP

// THE STORE, THE AUTHORING BUILDER, AND THE ONE EVALUATOR (SEM-0).
//
// ONE STORE, READ TWICE. There is no list of names beside a map of callables:
// `identities()` walks the same map `evaluate()` resolves through, so a name a
// consumer can discover is a name it can spend, by construction, and neither
// half can drift from the other. Loom says this in four places already --
// `accepted_schemas()`, `Kernel::accepts`, `describe_authority_as` and the
// Timer's own `find_entry` -- and each says it the same way: share the
// PREDICATE and the STORE, never a copy of the ANSWER.
//
// RESOLVE AT SPEND. A composition holds an operator's IDENTITY and the two
// `ContentId`s it was authored against, and resolves everything else at the
// moment it is spent. Nothing here caches a resolved operator, an index or a
// callable: LOG-R1 measured the resolve at 9.5ns and the whole custody
// difference at +1.7% over a five-node composite, so a cache would buy noise and
// sell the one property that matters -- with resolve-at-spend, a consumer and an
// executor CANNOT disagree about what a rule means, because there is no held
// copy for one of them to still be holding.
//
// THE FOUR THINGS THAT CAN GO WRONG, and who says each:
//
//     operator unresolved              this file       -- named, never dangling
//     signature is not the authored one this file       -- a ContentId compare
//     input does not match the schema   loom::admit     -- the ONE gate
//     the answer does not match either  loom::admit     -- the same gate, host side
//
// Two of the four already had an owner, so two sentences are written here and
// there is no operator error enum.
//
// TWO ENTRANCES, ONE BODY (OPH-0). `evaluate` takes either a `loom::Value` or
// the `loom::Unverified` a caller across a module boundary is holding, and both
// meet at the same admission and the same walk. The second door exists so the
// dynamic seam does not have to admit for itself and then say, in its own words,
// what this file already says about a refused pack -- because a second wording
// of one refusal is a second answer waiting for one of them to be edited.
//
// AN OPERATOR'S ANSWER IS RETURNED, NOT DELIVERED. `loom::admit` takes a schema
// OBJECT the caller already holds, and an `OperatorDef` is where that object
// lives, so a complete round trip needs no Switchboard, no registration and no
// `Emit<>` -- which would in any case be a false claim (an operator sends
// nothing) and would not work (it is informational and unenforced, and
// `zen.Manifest` has no `emitted` section).

#include "operator/operator.hpp"

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::op {

namespace detail {

/// Do two ports carry the same Loom type? Kind, and for a nested message the
/// schema's own identity -- which is `Schema::content_id()` and not a name, for
/// the reason the gate uses it: a name says which door, a content id says which
/// shape came through it.
inline bool same_type(const loom::TypeRef& a, const loom::TypeRef& b) {
    if (a.kind != b.kind) {
        return false;
    }
    if (a.kind == loom::Kind::Message) {
        return a.message->content_id() == b.message->content_id();
    }
    if (a.kind == loom::Kind::List) {
        return same_type(*a.element, *b.element);
    }
    return true;
}

} // namespace detail

/// Every operator this world knows, discoverable and invocable from one record.
///
/// Copyable on purpose: a catalog is authored data, and a consumer that wants a
/// different vocabulary makes a different catalog rather than editing somebody
/// else's. Immutable once built, in the only sense that matters -- there is no
/// erase, no replace and no rebind, so a name resolves to the same definition
/// for as long as the catalog exists.
class Catalog {
public:
    /// Refuses a duplicate identity, loudly.
    ///
    /// Answering a second registration with the first-sorting map key is an
    /// answer nobody authored, and Zen refuses that shape three times already: a
    /// role is a singleton, a shadowing pane offer is refused, and a published
    /// schema is immutable. `std::invalid_argument` is what `loom::Schema`'s own
    /// constructor throws for a shape it will not build.
    void publish(OperatorDef def) {
        const std::string key = def.identity();
        if (ops_.find(key) != ops_.end()) {
            throw std::invalid_argument("operator '" + key + "' is already published");
        }
        ops_.emplace(key, std::move(def));
    }

    /// The definition, or nullptr. The one lookup; `evaluate` uses it too.
    const OperatorDef* find(std::string_view identity) const {
        const auto it = ops_.find(identity);
        return it == ops_.end() ? nullptr : &it->second;
    }

    /// What is in here, derived from the record rather than maintained beside
    /// it. Sorted, because a map is.
    std::vector<std::string> identities() const {
        std::vector<std::string> names;
        names.reserve(ops_.size());
        for (const auto& [name, def] : ops_) {
            (void)def;
            names.push_back(name);
        }
        return names;
    }

    std::size_t size() const noexcept { return ops_.size(); }

    /// THE one evaluation path. Every consumer -- the Timer's own execution, a
    /// stranger reading ports off a schema, a composite's own nodes, and a
    /// dynamically loaded tool spending this catalog through the operator-host
    /// seam -- comes through here, which is what makes "one semantic path" a
    /// fact about the code rather than a claim about it.
    Evaluation evaluate(std::string_view identity, loom::Value args) const {
        const OperatorDef* def = find(identity);
        if (def == nullptr) {
            return unresolved(identity);
        }
        return run(*def, loom::admit(std::move(args), *def->inputs()));
    }

    /// THE SAME EVALUATION FOR A CALLER HOLDING BYTES (OPH-0) -- the module
    /// seam's shape, and the only reason it exists.
    ///
    /// A caller across a dynamic-library boundary has serialized bytes and no
    /// `loom::Value`, and the only way from one to the other is the gate. Doing
    /// that admission at the seam and then calling the overload above would
    /// admit twice and, worse, would put the seam's own wording beside this
    /// file's for the same failure -- two sentences for one refusal, drifting
    /// from the first edit. So the bytes come in here instead, and every word a
    /// loaded consumer reads about a refusal is the word an in-process caller
    /// reads.
    Evaluation evaluate(std::string_view identity, const loom::Unverified& args) const {
        const OperatorDef* def = find(identity);
        if (def == nullptr) {
            return unresolved(identity);
        }
        return run(*def, loom::admit(args, def->inputs()));
    }

private:
    static Evaluation unresolved(std::string_view identity) {
        return Evaluation::refuse("unresolved operator reference '" + std::string(identity) + "'");
    }

    /// What happens once the arguments have met the gate, whichever door they
    /// came through. The refusal, the native/composite fork and the output check
    /// are one body, and both public `evaluate`s are its two entrances -- so a
    /// caller holding bytes and a caller holding a Value get the same answer and
    /// the same words for it.
    Evaluation run(const OperatorDef& def, loom::Admission admitted) const {
        if (!admitted) {
            return Evaluation::refuse("'" + def.identity() +
                                      "' refused its arguments: " +
                                      admitted.first_error().message());
        }
        loom::Value out(def.outputs());
        const std::string& out_port = def.outputs()->fields()[0].name;
        if (def.is_composite()) {
            Evaluation walked = walk(def, admitted.value());
            if (!walked) {
                return walked;
            }
            out.set(out_port, *walked.value().at(0));
        } else {
            out.set(out_port, def.invoke_native(admitted.value()));
        }
        loom::Admission checked = loom::admit(std::move(out), *def.outputs());
        if (!checked) {
            // A native body that answers with the wrong shape is CAUGHT, not
            // believed. Nothing in this repository can reach it today; the gate
            // runs anyway, because the day an operator arrives from somewhere
            // else is not the day to start checking.
            return Evaluation::refuse("'" + def.identity() +
                                      "' produced an answer its own output schema refuses: " +
                                      checked.first_error().message());
        }
        return Evaluation::accept(std::move(checked).value());
    }

    /// Walk one acyclic graph. A node may only name an earlier node, so one
    /// forward pass is the whole evaluation order -- there is no scheduler, no
    /// visited set and no topological sort, because the structure IS the order.
    Evaluation walk(const OperatorDef& def, const loom::Value& inputs) const {
        const Composite& graph = *def.composition();
        std::vector<loom::Value> answers;
        answers.reserve(graph.nodes.size());

        for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
            const Node& node = graph.nodes[i];
            const OperatorDef* step = find(node.identity);
            if (step == nullptr) {
                return Evaluation::refuse("'" + def.identity() + "' step " + std::to_string(i) +
                                          ": unresolved operator reference '" + node.identity +
                                          "'");
            }
            if (step->inputs()->content_id() != node.authored_in ||
                step->outputs()->content_id() != node.authored_out) {
                // Found, and it is not the thing this rule was written for. A
                // reference that recorded no signature would bind to the new
                // shape in silence, which is the failure that reads as a wrong
                // answer rather than as a refusal.
                return Evaluation::refuse("'" + def.identity() + "' step " + std::to_string(i) +
                                          ": '" + node.identity +
                                          "' is not the signature this composition was authored "
                                          "against");
            }
            const std::vector<loom::Field>& ports = step->inputs()->fields();
            loom::Value pack(step->inputs());
            for (std::size_t k = 0; k < node.arguments.size() && k < ports.size(); ++k) {
                const Binding& b = node.arguments[k];
                switch (b.from()) {
                case Binding::From::Input: {
                    const loom::Cell* c = inputs.get(b.input_name());
                    if (c == nullptr) {
                        return Evaluation::refuse("'" + def.identity() + "' step " +
                                                  std::to_string(i) + ": no input named '" +
                                                  b.input_name() + "'");
                    }
                    pack.set(ports[k].name, *c);
                    break;
                }
                case Binding::From::Node:
                    pack.set(ports[k].name, *answers[b.node_index()].at(0));
                    break;
                case Binding::From::Constant:
                    pack.set(ports[k].name, b.constant_cell());
                    break;
                }
            }
            Evaluation stepped = evaluate(node.identity, std::move(pack));
            if (!stepped) {
                return stepped;
            }
            answers.push_back(stepped.value());
        }
        if (graph.result_node >= answers.size()) {
            return Evaluation::refuse("'" + def.identity() + "' names no result step");
        }
        return Evaluation::accept(answers[graph.result_node]);
    }

    std::map<std::string, OperatorDef, std::less<>> ops_;
};

// ---- authoring a composition -----------------------------------------------

/// A small authoring surface over `Composite`, and it is justified by exactly
/// what §18 asks of it: it carries the ceremony -- node indices, port order,
/// signature snapshots -- while the rule stays legible as the rule.
///
/// It resolves against a catalog AS IT AUTHORS, which is what lets the
/// composite's OUTPUT SCHEMA be derived rather than declared and lets a wrong
/// operator name, a wrong argument count or a wrong argument type be refused at
/// the point of authorship instead of at the first evaluation.
class Builder {
public:
    /// A value inside the composition being written, carrying the Loom type it
    /// will have. A `Ref` cannot name a node that does not exist yet, which is
    /// where acyclicity comes from.
    class Ref {
    public:
        const loom::TypeRef& type() const noexcept { return type_; }

    private:
        friend class Builder;
        Ref(Binding b, loom::TypeRef t) : binding_(std::move(b)), type_(std::move(t)) {}
        Binding binding_;
        loom::TypeRef type_;
    };

    /// `inputs` are the composite's own ports, in order. They are authored here
    /// because a composite has no C++ signature to derive them from -- it is the
    /// one place in this package where a type is written by hand, and it is
    /// written once.
    Builder(const Catalog& catalog, std::string identity, std::vector<loom::Field> inputs)
        : catalog_(catalog), identity_(std::move(identity)),
          inputs_(loom::make_schema(identity_ + ".in", 1, std::move(inputs))) {}

    Ref input(std::string_view name) const {
        const loom::Field* f = inputs_->find(name);
        if (f == nullptr) {
            throw std::invalid_argument("'" + identity_ + "' has no input named '" +
                                        std::string(name) + "'");
        }
        return Ref(Binding::input(std::string(name)), f->type);
    }

    Ref constant(std::int64_t v) const {
        return Ref(Binding::constant(loom::Cell::integer(v)), loom::type_of(loom::Kind::Int));
    }
    Ref constant(bool v) const {
        return Ref(Binding::constant(loom::Cell::boolean(v)), loom::type_of(loom::Kind::Bool));
    }

    /// One step. Refuses an unknown operator, a wrong argument count and a
    /// wrong argument type, each by name, and answers with a reference to what
    /// the step will produce.
    Ref call(std::string_view identity, const std::vector<Ref>& args) {
        const OperatorDef* step = catalog_.find(identity);
        if (step == nullptr) {
            throw std::invalid_argument("'" + identity_ + "' names an unpublished operator '" +
                                        std::string(identity) + "'");
        }
        const std::vector<loom::Field>& ports = step->inputs()->fields();
        if (args.size() != ports.size()) {
            throw std::invalid_argument("'" + std::string(identity) + "' takes " +
                                        std::to_string(ports.size()) + " arguments, not " +
                                        std::to_string(args.size()));
        }
        if (step->outputs()->fields().size() != 1) {
            // A binding names a NODE, not a node's port, so a multi-output
            // operator's answer has no unambiguous spelling here. Refused rather
            // than silently meaning the first one; the day such an operator
            // exists, a binding gains a port name.
            throw std::invalid_argument("'" + std::string(identity) +
                                        "' declares more than one output port, which a binding "
                                        "cannot yet name");
        }
        for (std::size_t k = 0; k < args.size(); ++k) {
            if (!detail::same_type(args[k].type_, ports[k].type)) {
                throw std::invalid_argument("'" + std::string(identity) + "' port '" +
                                            ports[k].name + "' expects " +
                                            loom::name_of(ports[k].type.kind) + ", not " +
                                            loom::name_of(args[k].type_.kind));
            }
        }
        Node node;
        node.identity = std::string(identity);
        node.authored_in = step->inputs()->content_id();
        node.authored_out = step->outputs()->content_id();
        node.arguments.reserve(args.size());
        for (const Ref& a : args) {
            node.arguments.push_back(a.binding_);
        }
        graph_.nodes.push_back(std::move(node));
        return Ref(Binding::node(graph_.nodes.size() - 1),
                   step->outputs()->fields()[0].type);
    }

    /// Name the composite's answer and finish. The output SCHEMA is derived from
    /// what the result step actually produces; only the port's NAME is authored,
    /// exactly as for a native operator.
    OperatorDef result(std::string_view port, const Ref& answer) && {
        if (answer.binding_.from() != Binding::From::Node) {
            // A composite whose answer is an input or a constant computes
            // nothing, and calling that an operator would be a name over an
            // identity function.
            throw std::invalid_argument("'" + identity_ + "' must answer with a computed step");
        }
        graph_.result_node = answer.binding_.node_index();
        auto out = loom::make_schema(
            identity_ + ".out", 1,
            std::vector<loom::Field>{loom::Field{std::string(port), answer.type_, true}});
        return OperatorDef(identity_, inputs_, std::move(out), std::move(graph_));
    }

private:
    const Catalog& catalog_;
    std::string identity_;
    std::shared_ptr<const loom::Schema> inputs_;
    Composite graph_;
};

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_CATALOG_HPP
