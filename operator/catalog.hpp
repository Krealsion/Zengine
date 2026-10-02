// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_OPERATOR_CATALOG_HPP
#define ZENGINE_OPERATOR_CATALOG_HPP

// The operator store, the authoring builder, and the one evaluator. One store, read twice:
// `identities()` walks the map `evaluate()` resolves through, so a name a consumer can discover
// is a name it can spend. An identity holds the stack of contributions eligible to satisfy it,
// the last one active, and `find` answers that one for `evaluate`, `describe`, a composite's
// nodes and a loaded consumer alike. A composition holds identities and the signatures it was
// authored against and resolves at every spend; nothing caches an operator, an index or a
// callable, so an executor and a previewer cannot disagree about what a rule means.
// Reference: docs/reference/operator-providers.md.

// Who says what went wrong: an unresolved operator, or a signature that is not the authored one
// (a `ContentId` compare), is this file's sentence; arguments or an answer a schema refuses is
// `loom::admit`'s, quoted. There is no operator error enum. An answer is returned, not
// delivered: a round trip needs no Switchboard, no registration and no `Emit<>`. Every spend one
// evaluation reaches draws on one budget, `kEvaluationSpends` spends nested `kEvaluationDepth`
// deep, and the spend that would pass it is refused in words.

#include "operator/fold.hpp"
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

// ---- what one evaluation may spend --------------------------------------------

/// The most operators one evaluation may spend: every node, every body a form spends and every
/// nested composite's nodes, from the first `evaluate` to its answer. A nested loop is bounded by
/// it, whatever its parts' own bounds allow.
inline constexpr std::uint64_t kEvaluationSpends = 100000;
/// The deepest one evaluation may nest, an operator spending an operator. An operator that reaches
/// itself through identities nests without end, and its stack would run out long before its spends.
inline constexpr std::size_t kEvaluationDepth = 32;

namespace detail {

/// The evaluation in progress on one catalog: opened by the outermost `evaluate` and shared by
/// every spend it reaches, re-entry through a loaded consumer's host table included. A copy of a
/// catalog is a different evaluator, so it starts with nothing spent.
struct EvaluationBudget {
    std::uint64_t spent = 0;
    std::size_t depth = 0;
    EvaluationBudget() = default;
    EvaluationBudget(const EvaluationBudget&) noexcept {}
    EvaluationBudget& operator=(const EvaluationBudget&) noexcept { return *this; }
};

} // namespace detail

// ---- who contributed a definition, and under what terms ---------------------

/// One contribution: a definition, and which provider supplied it. Held by `shared_ptr` so a
/// shadowed contribution stays the same object while covered, and revealing it reveals it
/// rather than rebuilding something that compares equal.
struct Contribution {
    /// The provider's identity; empty means the host authored this one itself, through
    /// `publish`.
    std::string provider;
    std::shared_ptr<const OperatorDef> definition;
};

/// Why a mount is made: the one bit of intent the catalog cannot see for itself -- did you mean
/// to cover something?
enum class MountMode : std::uint8_t {
    /// Contribute powers nobody else supplies. A collision is a refusal.
    Ordinary,
    /// Deliberately cover an existing contribution to the same identity; allowed only where the
    /// ports are structurally what existing compositions were authored against.
    Overlay,
};

/// What a mount did, or why it did nothing. A mount is all or nothing: every contribution in a
/// batch is judged before any is installed, so a refusal leaves the catalog as it was.
struct MountReport {
    bool ok = false;
    std::string reason;
    explicit operator bool() const noexcept { return ok; }
};

/// Every operator this world knows, discoverable and invocable from one record: the current
/// resolution of every identity anything provided. Contributions are layered, `find` answers the
/// top of a stack, and `unmount` removes one provider's and reveals what it covered; the catalog
/// authors and implements nothing. Copyable: a copy shares its immutable definitions (two
/// readers, never two answers), and a consumer wanting another vocabulary makes another catalog.
class Catalog {
public:
    /// Publish one definition the host itself authored. A duplicate identity throws
    /// `std::invalid_argument`, what `loom::Schema` throws for a shape it will not build:
    /// answering with the first-sorting key would be an answer nobody authored. It is the law
    /// `mount` enforces -- covering a taken identity is something you ask for.
    void publish(OperatorDef def) {
        const std::string key = def.identity();
        if (ops_.find(key) != ops_.end()) {
            throw std::invalid_argument("operator '" + key + "' is already published");
        }
        ops_[key].push_back(Contribution{std::string(),
                                         std::make_shared<const OperatorDef>(std::move(def))});
    }

    /// Install one provider's contributions, all or none. `custody` is whatever must stay alive
    /// while this provider is mounted -- for a loaded artifact, the record holding its image
    /// open. The catalog does not know what it is (this header has no loader; a suite's provider
    /// hands over nothing), but it promises the order: on unmount the contributions go first and
    /// the custody after, so no callable is reachable once what it calls into is released.
    MountReport mount(std::string provider, std::vector<OperatorDef> definitions,
                      MountMode mode = MountMode::Ordinary,
                      std::shared_ptr<const void> custody = nullptr) {
        if (provider.empty()) {
            // The empty name means the host authored it; a provider claiming it would make
            // `unmount("")` take the host's own vocabulary with it.
            return refused("a provider must have an identity");
        }
        if (providers_.find(provider) != providers_.end()) {
            return refused("provider '" + provider + "' is already mounted");
        }
        if (definitions.empty()) {
            // Unmountable in any meaningful sense, and in practice a provider whose authoring
            // failed: said out loud rather than accepted.
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
                // No automatic priority: load order, filesystem order and map iteration are
                // not policy.
                return refused("'" + def.identity() + "' is already supplied by " + held_by +
                               "; mounting '" + provider +
                               "' over it needs an explicit overlay");
            }
            if (!loom::same_identity(*def.inputs(), *active->inputs()) ||
                !loom::same_identity(*def.outputs(), *active->outputs())) {
                // A different power wearing the same name: compositions were authored against
                // the ports below.
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

    /// Remove exactly one provider's contributions and reveal what they covered: the same object
    /// that was there before, not a rebuild. An identity with nothing left becomes unresolved,
    /// said at the next evaluation. The contributions go, then the custody: a native callable
    /// holds the provider's record, so it must be unreachable before the record and its image
    /// are released.
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

    /// Every eligible contribution to one identity, active last: which provider is active and
    /// which are shadowed, read off the one store rather than a ledger beside it. Resolution
    /// state, not a weaver-facing surface or metadata.
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

    /// The one evaluation path: the Timer's execution, a stranger reading ports off a schema, a
    /// composite's nodes and a loaded tool spending this catalog across the operator-host seam
    /// all come through here.
    Evaluation evaluate(std::string_view identity, loom::Value args) const {
        const OperatorDef* def = find(identity);
        if (def == nullptr) {
            return unresolved(identity);
        }
        Spend spend(budget_);
        if (std::string why = spend.charge(identity); !why.empty()) {
            return Evaluation::refuse(std::move(why));
        }
        return run(*def, loom::admit(std::move(args), *def->inputs()));
    }

    /// The same evaluation for a caller across a module boundary, holding bytes. Admitting at
    /// the seam and calling the overload above would admit twice and put the seam's words beside
    /// this file's for one refusal; here a loaded consumer reads what an in-process caller reads.
    Evaluation evaluate(std::string_view identity, const loom::Unverified& args) const {
        const OperatorDef* def = find(identity);
        if (def == nullptr) {
            return unresolved(identity);
        }
        Spend spend(budget_);
        if (std::string why = spend.charge(identity); !why.empty()) {
            return Evaluation::refuse(std::move(why));
        }
        return run(*def, loom::admit(args, def->inputs()));
    }

private:
    /// One operator spent within the evaluation in progress, or the refusal of the spend that
    /// would pass its budget. The outermost spend opens the evaluation and closing it closes it.
    class Spend {
    public:
        explicit Spend(detail::EvaluationBudget& budget) : budget_(budget) {}
        Spend(const Spend&) = delete;
        Spend& operator=(const Spend&) = delete;
        ~Spend() {
            if (!charged_) {
                return;
            }
            if (--budget_.depth == 0) {
                budget_.spent = 0;
            }
        }
        std::string charge(std::string_view identity) {
            if (budget_.spent >= kEvaluationSpends) {
                return "spending '" + std::string(identity) +
                       "' would pass this evaluation's budget of " +
                       std::to_string(kEvaluationSpends) + " operator spends";
            }
            if (budget_.depth >= kEvaluationDepth) {
                return "spending '" + std::string(identity) + "' would nest this evaluation " +
                       std::to_string(kEvaluationDepth + 1) +
                       " operators deep, past its budget of " +
                       std::to_string(kEvaluationDepth) +
                       ": an operator that reaches itself through identities nests without end";
            }
            ++budget_.spent;
            ++budget_.depth;
            charged_ = true;
            return std::string();
        }

    private:
        detail::EvaluationBudget& budget_;
        bool charged_ = false;
    };

    static Evaluation unresolved(std::string_view identity) {
        return Evaluation::refuse("unresolved operator reference '" + std::string(identity) + "'");
    }

    static MountReport refused(std::string why) { return MountReport{false, std::move(why)}; }

    /// What happens once the arguments met the gate, whichever door they came through: one body,
    /// two entrances, the same answer and the same words.
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
            // A native body may live in another image, so this is the deepest place that can
            // turn "the provider could not answer" into a refusal rather than an exception out
            // of an evaluation whose contract is a value or a reason.
            std::optional<loom::Cell> answered;
            try {
                answered = def.invoke_native(admitted.value());
            } catch (const Refusal& e) {
                return Evaluation::refuse(e.reason());
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
            // A native body answering with the wrong shape is caught, not believed: an operator
            // may arrive from another image.
            return Evaluation::refuse("'" + def.identity() +
                                      "' produced an answer its own output schema refuses: " +
                                      checked.first_error().message());
        }
        return Evaluation::accept(std::move(checked).value());
    }

    /// Walk one acyclic graph: a node may only name an earlier node, so one forward pass is the
    /// whole evaluation order -- no scheduler, no visited set, no topological sort. Each step
    /// resolves by identity here, at the spend, and is spent through `evaluate`, so a cycle through
    /// identities is stopped by the evaluation's budget (agents/operators.md).
    Evaluation walk(const OperatorDef& def, const loom::Value& inputs) const {
        const Composite& graph = *def.composition();
        std::vector<loom::Value> answers;
        answers.reserve(graph.nodes.size());

        for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
            const Node& node = graph.nodes[i];
            const std::string at = "'" + def.identity() + "' step " + std::to_string(i) + ": ";
            const OperatorDef* step = find(node.identity);
            if (step == nullptr) {
                return Evaluation::refuse(at + "unresolved operator reference '" + node.identity +
                                          "'");
            }
            if (step->inputs()->content_id() != node.authored_in ||
                step->outputs()->content_id() != node.authored_out) {
                // Found, and not what this rule was written for: a reference that recorded no
                // signature would bind to the new shape silently, a wrong answer, not a refusal.
                return Evaluation::refuse(at + "'" + node.identity +
                                          "' is not the signature this composition was authored "
                                          "against");
            }
            FoldPorts derived;
            if (node.fold) {
                try {
                    derived = fold_ports(*step, *node.fold);
                } catch (const std::exception& e) {
                    return Evaluation::refuse(at + e.what());
                }
            }
            const std::vector<loom::Field>& ports =
                node.fold ? derived.inputs : step->inputs()->fields();
            std::vector<const loom::Cell*> cells(ports.size(), nullptr);
            for (std::size_t k = 0; k < node.arguments.size() && k < ports.size(); ++k) {
                const Binding& b = node.arguments[k];
                switch (b.from()) {
                case Binding::From::Input:
                    cells[k] = inputs.get(b.input_name());
                    if (cells[k] == nullptr) {
                        return Evaluation::refuse(at + "no input named '" + b.input_name() + "'");
                    }
                    break;
                case Binding::From::Node:
                    cells[k] = answers[b.node_index()].at(0);
                    break;
                case Binding::From::Constant:
                    cells[k] = &b.constant_cell();
                    break;
                }
            }
            if (node.fold) {
                Evaluation folded = fold(at, node, *step, derived, cells);
                if (!folded) {
                    return folded;
                }
                answers.push_back(folded.value());
                continue;
            }
            loom::Value pack(step->inputs());
            for (std::size_t k = 0; k < cells.size(); ++k) {
                if (cells[k] != nullptr) {
                    pack.set(ports[k].name, *cells[k]);
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

    /// Spend one fold: count first, then spend the body once per count through `evaluate`, so
    /// each spend draws on the evaluation's budget, and thread the accumulator. A refusal says
    /// where; a body's refusal is the fold's, naming the iteration and its count.
    Evaluation fold(const std::string& at, const Node& node, const OperatorDef& body,
                    const FoldPorts& ports, const std::vector<const loom::Cell*>& cells) const {
        for (std::size_t k = 0; k < 4; ++k) {
            if (cells[k] == nullptr) {
                return Evaluation::refuse(at + "the fold's '" + ports.inputs[k].name +
                                          "' is not bound");
            }
            if (k < 3 && cells[k]->kind() != loom::Kind::Int) {
                return Evaluation::refuse(at + "the fold's '" + ports.inputs[k].name + "' is " +
                                          loom::name_of(cells[k]->kind()) + ", not Int");
            }
        }
        const std::int64_t start = cells[0]->as_int();
        const std::int64_t limit = cells[1]->as_int();
        const std::int64_t by = cells[2]->as_int();
        if (by == 0) {
            return Evaluation::refuse(at + "a step of 0 never moves the count from " +
                                      std::to_string(start) + " toward " + std::to_string(limit));
        }
        const std::uint64_t count = fold_count(start, limit, by);
        if (count > kMaxFoldCount) {
            return Evaluation::refuse(at + "this fold would count " + std::to_string(count) +
                                      " times from " + std::to_string(start) + " toward " +
                                      std::to_string(limit) + " by " + std::to_string(by) +
                                      ", and a fold counts at most " +
                                      std::to_string(kMaxFoldCount) + " times");
        }
        loom::Cell acc = *cells[3];
        for (std::uint64_t k = 0; k < count; ++k) {
            const std::int64_t value = fold_value(start, by, k);
            loom::Value pack(body.inputs());
            pack.set(node.fold->count, loom::Cell::integer(value));
            pack.set(node.fold->accumulator, acc);
            for (std::size_t j = 4; j < cells.size(); ++j) {
                if (cells[j] != nullptr) {
                    pack.set(ports.inputs[j].name, *cells[j]);
                }
            }
            Evaluation spent = evaluate(node.identity, std::move(pack));
            if (!spent) {
                return Evaluation::refuse(at + "iteration " + std::to_string(k) + " (count " +
                                          std::to_string(value) + "): " + spent.reason());
            }
            acc = *spent.value().at(0);
        }
        loom::Value out(body.outputs());
        out.set(ports.answer.name, std::move(acc));
        loom::Admission checked = loom::admit(std::move(out), *body.outputs());
        if (!checked) {
            return Evaluation::refuse(at + "the fold's answer is not '" + node.identity +
                                      "''s: " + checked.first_error().message());
        }
        return Evaluation::accept(std::move(checked).value());
    }

    /// The one store: an identity maps to the stack of contributions eligible to satisfy it and
    /// `back()` is active, so pushing shadows and erasing reveals. A stack is never empty: the
    /// last erase takes the identity with it, so "unresolved" is a fact about the store.
    std::map<std::string, std::vector<Contribution>, std::less<>> ops_;

    /// Who is mounted, and what each keeps alive. Apart from `ops_`: a provider whose every
    /// contribution is shadowed is still mounted, and unmounting it must still find it.
    std::map<std::string, std::shared_ptr<const void>, std::less<>> providers_;

    /// The evaluation in progress, if any: the one mutable thing an evaluation touches, and never
    /// a fact about what any operator means.
    mutable detail::EvaluationBudget budget_;
};

// ---- authoring a composition -----------------------------------------------

/// A small authoring surface over `Composite`: it carries the ceremony (node indices, port
/// order, signature snapshots) so the rule stays legible as the rule. It resolves against a
/// catalog as it authors, so the output schema is derived, and a wrong operator name, argument
/// count or argument type is refused at authorship rather than at the first evaluation.
class Builder {
public:
    /// A value inside the composition being written, with the Loom type it will have. A `Ref`
    /// cannot name a node that does not exist yet, which is where one graph's acyclicity comes
    /// from.
    class Ref {
    public:
        const loom::TypeRef& type() const noexcept { return type_; }

    private:
        friend class Builder;
        Ref(Binding b, loom::TypeRef t) : binding_(std::move(b)), type_(std::move(t)) {}
        Binding binding_;
        loom::TypeRef type_;
    };

    /// `inputs` are the composite's own ports, in order: written by hand, once, since a composite
    /// has no C++ signature to derive them from.
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

    /// One step. Refuses an unknown operator, a wrong argument count and a wrong argument type,
    /// each by name, and answers with a reference to what the step will produce.
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
            // A binding names a node, not a node's port, so a multi-output operator's answer
            // has no unambiguous spelling: refused rather than silently meaning the first.
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

    /// One fold over the operator `body`, threading its `accumulator` port and giving each count to
    /// its `count` port. The arguments are the fold's derived ports in order -- start, limit, step,
    /// initial, then the body's other ports -- refused by name where they do not fit, and the
    /// answer has the body's answer type.
    Ref fold(std::string_view body, std::string count, std::string accumulator,
             const std::vector<Ref>& args) {
        const OperatorDef* step = catalog_.find(body);
        if (step == nullptr) {
            throw std::invalid_argument("'" + identity_ + "' names an unpublished operator '" +
                                        std::string(body) + "'");
        }
        Fold form{std::move(count), std::move(accumulator)};
        const FoldPorts ports = fold_ports(*step, form);
        if (args.size() != ports.inputs.size()) {
            throw std::invalid_argument("a fold over '" + std::string(body) + "' takes " +
                                        std::to_string(ports.inputs.size()) + " arguments, not " +
                                        std::to_string(args.size()));
        }
        for (std::size_t k = 0; k < args.size(); ++k) {
            if (!detail::same_type(args[k].type_, ports.inputs[k].type)) {
                throw std::invalid_argument("a fold over '" + std::string(body) + "' port '" +
                                            ports.inputs[k].name + "' expects " +
                                            loom::name_of(ports.inputs[k].type.kind) + ", not " +
                                            loom::name_of(args[k].type_.kind));
            }
        }
        Node node;
        node.identity = std::string(body);
        node.authored_in = step->inputs()->content_id();
        node.authored_out = step->outputs()->content_id();
        node.fold = std::move(form);
        for (const Ref& a : args) {
            node.arguments.push_back(a.binding_);
        }
        graph_.nodes.push_back(std::move(node));
        return Ref(Binding::node(graph_.nodes.size() - 1), ports.answer.type);
    }

    /// Name the composite's answer and finish. The output schema is derived from what the result
    /// step produces; the port's name and what the composite is for are authored, as for a native
    /// operator.
    OperatorDef result(std::string_view port, const Ref& answer, std::string about = {}) && {
        if (answer.binding_.from() != Binding::From::Node) {
            // A composite answering with an input or a constant computes nothing, and calling
            // it an operator would name an identity function.
            throw std::invalid_argument("'" + identity_ + "' must answer with a computed step");
        }
        graph_.result_node = answer.binding_.node_index();
        auto out = loom::make_schema(
            identity_ + ".out", 1,
            std::vector<loom::Field>{loom::Field{std::string(port), answer.type_, true}});
        return OperatorDef(identity_, inputs_, std::move(out), std::move(graph_),
                           Description{std::move(about)});
    }

private:
    const Catalog& catalog_;
    std::string identity_;
    std::shared_ptr<const loom::Schema> inputs_;
    Composite graph_;
};

} // namespace zengine::op

#endif // ZENGINE_OPERATOR_CATALOG_HPP
