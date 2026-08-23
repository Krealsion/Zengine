// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_CATALOG_HPP
#define ZENGINE_OPERATOR_CATALOG_HPP

// THE STORE, THE AUTHORING BUILDER, AND THE ONE EVALUATOR (SEM-0, layered by PROV-0).
//
// ONE STORE, READ TWICE. There is no list of names beside a map of callables:
// `identities()` walks the same map `evaluate()` resolves through, so a name a
// consumer can discover is a name it can spend, by construction, and neither
// half can drift from the other. Loom says this in four places already --
// `accepted_schemas()`, `Kernel::accepts`, `describe_authority_as` and the
// Timer's own `find_entry` -- and each says it the same way: share the
// PREDICATE and the STORE, never a copy of the ANSWER.
//
// ...AND SINCE PROV-0 THE STORE IS LAYERED, which is the same law read once more.
// An identity does not hold a definition; it holds the STACK of contributions
// eligible to satisfy it, and the last one is ACTIVE. `find` answers the active
// one, so `evaluate`, `describe`, a composite's own nodes and a loaded consumer
// across the ABI all resolve the same contribution for the same reason they always
// did -- they ask the one store, at the moment they ask.
//
//     math.max
//         active     zengine.operators.test.min   <- what find() answers
//         shadowed   zengine.operators.basic      <- still resident, still here
//
// SHADOWING IS INTENTIONAL. A second ordinary contribution to a taken identity is
// REFUSED, exactly as a duplicate `publish` always was; only an explicit OVERLAY
// mount may cover one, and only where the ports are structurally the contract
// existing compositions were authored against. And it COVERS rather than replaces:
// unmounting the overlay reveals what was underneath, unchanged and unrebuilt,
// which is what makes replacement reversible at all.
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
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
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

// ---- who contributed a definition, and under what terms ---------------------

/// ONE CONTRIBUTION: a definition, and which provider supplied it.
///
/// The definition is held by SHARED POINTER and that is load-bearing rather than
/// tidy. A shadowed contribution has to stay the SAME OBJECT while something else
/// is active over it, so that revealing it again is revealing it -- not
/// reconstructing something that compares equal. A raw slot in a vector could not
/// promise that across a reallocation; a `shared_ptr` promises it across anything.
struct Contribution {
    /// The provider's logical identity. EMPTY means the host authored this one
    /// itself, through `publish` -- the floor, and the state every catalog in this
    /// repository was in before providers existed.
    std::string provider;
    std::shared_ptr<const OperatorDef> definition;
};

/// WHY A MOUNT IS BEING MADE, and it is a two-value question on purpose.
///
/// A provider layering system needs exactly one bit of INTENT from the caller:
/// did you mean to cover something? Everything else -- which providers exist, what
/// they supply, whether the ports agree -- the catalog can see for itself.
enum class MountMode : std::uint8_t {
    /// Contribute powers nobody else supplies. A collision is a REFUSAL.
    Ordinary,
    /// Deliberately cover an existing contribution to the same identity. Allowed
    /// only where the ports are structurally what existing compositions were
    /// authored against.
    Overlay,
};

/// What a mount did, or why it did nothing.
///
/// A MOUNT IS ALL OR NOTHING. Every contribution in a batch is judged before any
/// of them is installed, so a refusal leaves the catalog exactly as it was rather
/// than half-carrying a provider whose second operator was the problem.
struct MountReport {
    bool ok = false;
    std::string reason;
    explicit operator bool() const noexcept { return ok; }
};

/// Every operator this world knows, discoverable and invocable from one record.
///
/// Copyable on purpose: a catalog is authored data, and a consumer that wants a
/// different vocabulary makes a different catalog rather than editing somebody
/// else's. A copy shares its definitions rather than duplicating them, which is
/// what `shared_ptr` is doing there: a definition is immutable, so two catalogs
/// naming one object is two readers, never two answers.
///
/// WHAT IT OWNS SINCE PROV-0. Not just definitions: the CURRENT RESOLUTION of
/// every identity anything provided. Contributions arrive from providers and are
/// layered; `find` answers the top of a stack; `unmount` removes exactly one
/// provider's and reveals whatever it was covering. The catalog authors nothing
/// and implements nothing -- it decides, at every moment, which contribution
/// currently satisfies a logical power.
class Catalog {
public:
    /// PUBLISH ONE DEFINITION THE HOST ITSELF AUTHORED. Refuses a duplicate
    /// identity, loudly.
    ///
    /// Answering a second registration with the first-sorting map key is an
    /// answer nobody authored, and Zen refuses that shape three times already: a
    /// role is a singleton, a shadowing pane offer is refused, and a published
    /// schema is immutable. `std::invalid_argument` is what `loom::Schema`'s own
    /// constructor throws for a shape it will not build.
    ///
    /// It is the SAME LAW `mount` enforces, said in the older spelling: a taken
    /// identity is taken, and covering one is a thing you have to ask for.
    void publish(OperatorDef def) {
        const std::string key = def.identity();
        if (ops_.find(key) != ops_.end()) {
            throw std::invalid_argument("operator '" + key + "' is already published");
        }
        ops_[key].push_back(Contribution{std::string(),
                                         std::make_shared<const OperatorDef>(std::move(def))});
    }

    /// INSTALL ONE PROVIDER'S CONTRIBUTIONS, all of them or none of them.
    ///
    /// `custody` is whatever must stay alive for as long as this provider is
    /// mounted -- for a loaded artifact, the record that holds its image open. The
    /// catalog does not know what it is and must not: this header is portable, has
    /// no loader in it, and a provider that is not an image at all (a suite's, say)
    /// hands over nothing. What the catalog DOES promise is the order: on unmount
    /// the contributions go first and the custody goes after, so no callable can
    /// still be reachable when the thing it calls into is released.
    MountReport mount(std::string provider, std::vector<OperatorDef> definitions,
                      MountMode mode = MountMode::Ordinary,
                      std::shared_ptr<const void> custody = nullptr) {
        if (provider.empty()) {
            // The empty name means "the host authored it", and a provider that
            // could claim it would be unmountable: `unmount("")` would take the
            // host's own vocabulary with it.
            return refused("a provider must have an identity");
        }
        if (providers_.find(provider) != providers_.end()) {
            return refused("provider '" + provider + "' is already mounted");
        }
        if (definitions.empty()) {
            // A provider that supplies nothing leaves no trace and could never be
            // unmounted meaningfully. Said out loud rather than accepted silently,
            // because the way this happens in practice is a provider whose
            // authoring failed.
            return refused("provider '" + provider + "' contributes nothing");
        }

        // ---- judge everything first ----------------------------------------
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            const OperatorDef& def = definitions[i];
            for (std::size_t k = 0; k < i; ++k) {
                if (definitions[k].identity() == def.identity()) {
                    return refused("provider '" + provider + "' contributes '" + def.identity() +
                                   "' twice");
                }
            }
            const OperatorDef* active = find(def.identity());
            if (active == nullptr) {
                continue; // a power nobody supplies: neither mode has anything to say
            }
            const std::string& holder = ops_.find(def.identity())->second.back().provider;
            const std::string held_by = holder.empty() ? "this host itself" : "'" + holder + "'";
            if (mode == MountMode::Ordinary) {
                // NO AUTOMATIC PRIORITY. Load order, filesystem order and map
                // iteration are not policy; two providers of one power without a
                // stated intent is an ambiguity nobody authored.
                return refused("'" + def.identity() + "' is already supplied by " + held_by +
                               "; mounting '" + provider +
                               "' over it needs an explicit overlay");
            }
            if (!loom::same_identity(*def.inputs(), *active->inputs()) ||
                !loom::same_identity(*def.outputs(), *active->outputs())) {
                // A DIFFERENT POWER WEARING THE SAME NAME. Compositions were
                // authored against the ports below; a shadow that changes them
                // would be answering a question nobody asked.
                return refused("'" + provider + "' would shadow '" + def.identity() +
                               "' at a different signature (" + def.inputs()->name() + " v" +
                               std::to_string(def.inputs()->version()) + " -> " +
                               def.outputs()->name() + " v" +
                               std::to_string(def.outputs()->version()) + ") than " + held_by +
                               " supplies (" + active->inputs()->name() + " v" +
                               std::to_string(active->inputs()->version()) + " -> " +
                               active->outputs()->name() + " v" +
                               std::to_string(active->outputs()->version()) + ")");
            }
        }

        // ---- then install, with nothing left to refuse ----------------------
        for (OperatorDef& def : definitions) {
            const std::string key = def.identity();
            ops_[key].push_back(
                Contribution{provider, std::make_shared<const OperatorDef>(std::move(def))});
        }
        providers_.emplace(std::move(provider), std::move(custody));
        return MountReport{true, std::string()};
    }

    /// REMOVE EXACTLY ONE PROVIDER'S CONTRIBUTIONS, and reveal what they covered.
    ///
    /// For each identity it supplied: if an eligible contribution remains, that one
    /// becomes active again -- the SAME OBJECT that was there before, not a rebuild
    /// of it. If none remains, the logical operator becomes unresolved, and the
    /// deepest layer that knows says so at the next evaluation. Nothing is
    /// manufactured to fill a gap.
    ///
    /// THE ORDER IS THE POINT and it is two statements: the contributions go, THEN
    /// the custody. A native contribution's callable holds the provider's record,
    /// so dropping the contribution is what makes the callable unreachable, and
    /// only then can the record -- and the image inside it -- be released.
    bool unmount(std::string_view provider) {
        const auto mounted = providers_.find(provider);
        if (mounted == providers_.end()) {
            return false;
        }
        for (auto it = ops_.begin(); it != ops_.end();) {
            std::vector<Contribution>& stack = it->second;
            for (auto c = stack.begin(); c != stack.end();) {
                c = c->provider == provider ? stack.erase(c) : c + 1;
            }
            it = stack.empty() ? ops_.erase(it) : std::next(it);
        }
        providers_.erase(mounted);
        return true;
    }

    bool mounted(std::string_view provider) const {
        return providers_.find(provider) != providers_.end();
    }

    /// Who is mounted here. Sorted, because a map is.
    std::vector<std::string> providers() const {
        std::vector<std::string> names;
        names.reserve(providers_.size());
        for (const auto& [name, custody] : providers_) {
            (void)custody;
            names.push_back(name);
        }
        return names;
    }

    /// EVERY ELIGIBLE CONTRIBUTION TO ONE IDENTITY, ACTIVE LAST.
    ///
    /// The whole of what provider layering needs to be debuggable -- which
    /// provider is active, which are shadowed, and in what order -- read off the
    /// ONE store rather than off a ledger kept beside it. It is deliberately not a
    /// maker-facing introspection surface and deliberately not metadata: it is this
    /// object's own resolution state, and a later Metadata system may project it.
    std::vector<Contribution> contributions(std::string_view identity) const {
        const auto it = ops_.find(identity);
        return it == ops_.end() ? std::vector<Contribution>() : it->second;
    }

    /// The ACTIVE definition, or nullptr. The one lookup; `evaluate` uses it too,
    /// and so does every node of every composition, at every spend.
    const OperatorDef* find(std::string_view identity) const {
        const auto it = ops_.find(identity);
        return it == ops_.end() ? nullptr : it->second.back().definition.get();
    }

    /// What is in here, derived from the record rather than maintained beside
    /// it. Sorted, because a map is.
    std::vector<std::string> identities() const {
        std::vector<std::string> names;
        names.reserve(ops_.size());
        for (const auto& [name, stack] : ops_) {
            (void)stack;
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

    static MountReport refused(std::string why) { return MountReport{false, std::move(why)}; }

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
            // A NATIVE BODY MAY NOW LIVE IN ANOTHER IMAGE (PROV-0), so this is the
            // deepest place that can turn "the provider could not answer" into an
            // honest refusal instead of an escape. The kernel's adapter contains a
            // library's throw the same way and for the same reason; the only other
            // spelling of this branch is an exception travelling out of an
            // evaluation whose whole contract is a value or a reason.
            std::optional<loom::Cell> answered;
            try {
                answered = def.invoke_native(admitted.value());
            } catch (const std::exception& e) {
                return Evaluation::refuse("'" + def.identity() +
                                          "' could not be spent: " + e.what());
            } catch (...) {
                return Evaluation::refuse("'" + def.identity() +
                                          "' could not be spent: its implementation failed");
            }
            out.set(out_port, *answered);
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

    /// THE ONE STORE. An identity maps to the stack of contributions eligible to
    /// satisfy it, and `back()` is the active one -- so pushing is shadowing,
    /// erasing is revealing, and there is nowhere else for either to be recorded.
    /// A stack is never empty: the last erase takes the identity with it, which is
    /// what makes "unresolved" a fact about the store rather than a special value
    /// inside it.
    std::map<std::string, std::vector<Contribution>, std::less<>> ops_;

    /// WHO IS MOUNTED, and what each one keeps alive. Separate from `ops_` because
    /// "mounted" and "supplies something right now" are different facts: a provider
    /// whose every contribution is shadowed is still mounted, and unmounting it
    /// must still find it.
    std::map<std::string, std::shared_ptr<const void>, std::less<>> providers_;
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
