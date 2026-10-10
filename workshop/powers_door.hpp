// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_POWERS_DOOR_HPP
#define ZENGINE_WORKSHOP_POWERS_DOOR_HPP

// The host's discovery door (introspection/docs/introspection.md): two derivations, pure over the live
// `op::Catalog` they read, and the participant that answers them in the office `zengine.powers`.
// It derives every answer at the ask and keeps nothing between asks, describes and cannot
// evaluate (sampling is `zengine.sources`'), answers only whoever asked, publishes nothing, and
// refuses in words an ask past its bounds.

#include "powers_vocabulary.hpp"

#include "operator/catalog.hpp"
#include "operator/fold.hpp"      // the fold, the one form a row describes beside the catalog
#include "operator/migration.hpp" // `declares_migration`, the one spelling of a conversion
#include "operator/operator.hpp"
#include "operator/provider.hpp"  // `encode_contribution`
#include "operator/source.hpp"    // `is_source`, the one spelling of "no weaver inputs"

#include <zen/serialize.hpp>
#include <zen/switchboard/bus.hpp>
#include <zen/switchboard/grant.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/weave_contract.hpp>
#include <zen/terminal/composer.hpp> // `describe_schema`: Loom's one spelling of a type
#include <zen/terminal/vocabulary.hpp>
#include <zen/value.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

namespace powers_detail {

/// ASCII case folding, and only ASCII: a byte at or above 0x80 compares exactly.
inline unsigned char fold(char c) noexcept {
    const unsigned char b = static_cast<unsigned char>(c);
    return (b >= 'A' && b <= 'Z') ? static_cast<unsigned char>(b - 'A' + 'a') : b;
}

/// Is `needle` somewhere in `hay`, ASCII case folded?
inline bool contains_folded(std::string_view hay, std::string_view needle) noexcept {
    if (needle.size() > hay.size()) {
        return false;
    }
    for (std::size_t at = 0; at + needle.size() <= hay.size(); ++at) {
        std::size_t j = 0;
        while (j < needle.size() && fold(hay[at + j]) == fold(needle[j])) {
            ++j;
        }
        if (j == needle.size()) {
            return true;
        }
    }
    return false;
}

/// The whitespace-separated terms of a text query; none for an empty or blank one.
inline std::vector<std::string_view> terms_of(std::string_view text) {
    std::vector<std::string_view> terms;
    std::size_t at = 0;
    while (at < text.size()) {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' ||
                                    text[at] == '\r')) {
            ++at;
        }
        const std::size_t begin = at;
        while (at < text.size() && text[at] != ' ' && text[at] != '\t' && text[at] != '\n' &&
               text[at] != '\r') {
            ++at;
        }
        if (at > begin) {
            terms.push_back(text.substr(begin, at - begin));
        }
    }
    return terms;
}

inline SchemaIdentity identity_of(const loom::Schema& s) {
    SchemaIdentity out;
    out.name = s.name();
    out.version = static_cast<std::int64_t>(s.version());
    out.content_id = static_cast<std::int64_t>(s.content_id());
    return out;
}

/// `name: Type, ...` for one port list, each type in Loom's own spelling.
inline std::string ports_of(const loom::Schema& ports) {
    std::string said;
    for (const loom::FieldDesc& f : loom::describe_schema(ports).fields) {
        said += (said.empty() ? "" : ", ") + f.name + ": " + f.type;
    }
    return said;
}

/// Does a port in this list carry exactly this type, as Loom spells it?
inline bool carries(const loom::Schema& ports, std::string_view type) {
    for (const loom::FieldDesc& f : loom::describe_schema(ports).fields) {
        if (f.type == type) {
            return true;
        }
    }
    return false;
}

inline const char* kind_of(const op::OperatorDef& def) noexcept {
    if (op::declares_migration(def)) {
        return kConversionKind;
    }
    return op::is_source(def) ? kSourceKind : kOperatorKind;
}

inline const char* construction_of(const op::OperatorDef& def) noexcept {
    return def.is_composite() ? kCompositeConstruction : kNativeConstruction;
}

/// Does a row of this kind answer an ask for that one? A conversion is an operator too.
inline bool kind_fits(std::string_view kind, std::string_view asked) noexcept {
    return kind == asked || (asked == kOperatorKind && kind == kConversionKind);
}

/// One power as its contribution in force says it, read off the definition and nothing beside it.
inline PowerRow row_of(const std::string& identity, const op::Contribution& in_force) {
    const op::OperatorDef& def = *in_force.definition;
    PowerRow row;
    row.identity = identity;
    row.provider = in_force.provider;
    row.kind = kind_of(def);
    row.construction = construction_of(def);
    row.offered = def.description().offered;
    row.about = def.description().about;
    row.signature = "(" + ports_of(*def.inputs()) + ") -> " + ports_of(*def.outputs());
    row.inputs = identity_of(*def.inputs());
    row.outputs = identity_of(*def.outputs());
    return row;
}

inline PowerLayer layer_of(const op::Contribution& c) {
    PowerLayer layer;
    layer.provider = c.provider;
    layer.construction = construction_of(*c.definition);
    layer.offered = c.definition->description().offered;
    layer.about = c.definition->description().about;
    return layer;
}

/// Could a fold spend this power as its body? One answer, an Int port for the count, and another
/// port of the answer's type for the accumulator; which two is the composer's to choose.
inline bool fits_fold(const op::OperatorDef& def) {
    const std::vector<loom::Field>& ins = def.inputs()->fields();
    const std::vector<loom::Field>& outs = def.outputs()->fields();
    if (outs.size() != 1) {
        return false;
    }
    for (std::size_t c = 0; c < ins.size(); ++c) {
        if (ins[c].type.kind != loom::Kind::Int) {
            continue;
        }
        for (std::size_t a = 0; a < ins.size(); ++a) {
            if (a != c && op::detail::same_type(ins[a].type, outs[0].type)) {
                return true;
            }
        }
    }
    return false;
}

/// Does the power in force fit every field the ask gave?
inline bool fits(const std::string& identity, const op::Contribution& in_force,
                 const FindPowers& asked, const std::vector<std::string_view>& terms) {
    const op::OperatorDef& def = *in_force.definition;
    for (const std::string_view term : terms) {
        if (!contains_folded(identity, term) && !contains_folded(def.description().about, term)) {
            return false;
        }
    }
    return (!asked.takes || carries(*def.inputs(), *asked.takes)) &&
           (!asked.yields || carries(*def.outputs(), *asked.yields)) &&
           (!asked.provider || in_force.provider == *asked.provider) &&
           (!asked.kind || kind_fits(kind_of(def), *asked.kind)) &&
           (!asked.construction || *asked.construction == construction_of(def)) &&
           (!asked.offered || def.description().offered == *asked.offered) &&
           (!asked.fits || fits_fold(def));
}

/// The fold as a row: not an identity anything resolves, a form the evaluator spends.
inline PowerRow fold_row() {
    PowerRow row;
    row.identity = op::kFoldForm;
    row.kind = kFormKind;
    row.construction = kEvaluatorConstruction;
    row.offered = true;
    row.about = op::kFoldAbout;
    row.signature = op::kFoldSignature;
    return row;
}

/// Does the fold fit every field the ask gave? It is found by what it is for -- an ask with text,
/// or one for the kind `form` -- and a browse of the catalog lists the catalog. Its answer is its
/// body's, so any `yields` may be a fold's; it takes Int counts, comes from no provider and is no
/// fold's body.
inline bool form_fits(const FindPowers& asked, const std::vector<std::string_view>& terms) {
    if (terms.empty() && !(asked.kind && *asked.kind == kFormKind)) {
        return false;
    }
    for (const std::string_view term : terms) {
        if (!contains_folded(op::kFoldForm, term) && !contains_folded(op::kFoldAbout, term)) {
            return false;
        }
    }
    return (!asked.takes || *asked.takes == "Int") && !asked.provider && !asked.fits &&
           (!asked.kind || *asked.kind == kFormKind) &&
           (!asked.construction || *asked.construction == kEvaluatorConstruction) &&
           (!asked.offered || *asked.offered);
}

inline std::size_t answer_bytes(const loom::Value& answer) {
    return loom::serialize(answer).size();
}

} // namespace powers_detail

/// Why an ask is refused before anything is read, in words; empty when it can be answered.
inline std::string refusal_of(const FindPowers& asked) {
    std::size_t bytes = 0;
    for (const std::optional<std::string>* text :
         {&asked.text, &asked.takes, &asked.yields, &asked.provider, &asked.kind,
          &asked.construction, &asked.after, &asked.fits}) {
        bytes += *text ? (*text)->size() : 0;
    }
    if (bytes > kMaxPowersQueryBytes) {
        return "a query carries at most " + std::to_string(kMaxPowersQueryBytes) +
               " bytes of text; this one carries " + std::to_string(bytes);
    }
    if (asked.limit && (*asked.limit < 1 || *asked.limit > kMaxPowerRows)) {
        return "a page holds 1 to " + std::to_string(kMaxPowerRows) + " rows; " +
               std::to_string(*asked.limit) + " were asked for";
    }
    if (asked.kind && *asked.kind != kSourceKind && *asked.kind != kOperatorKind &&
        *asked.kind != kConversionKind && *asked.kind != kFormKind) {
        return "a kind is source, operator, conversion or form; '" + *asked.kind +
               "' is none of them";
    }
    if (asked.construction && *asked.construction != kNativeConstruction &&
        *asked.construction != kCompositeConstruction &&
        *asked.construction != kEvaluatorConstruction) {
        return "a construction is native, composite or evaluator; '" + *asked.construction +
               "' is none of them";
    }
    if (asked.fits && *asked.fits != kFitsFold) {
        return "a fit is fold; '" + *asked.fits + "' is not one";
    }
    return std::string();
}

/// Every power the ask fits, the catalog's identities in its order and then the evaluator's form,
/// one page of it: read at the ask off the store `find` resolves through, so an overlay mounted
/// since the last ask is in this answer with its own words. A cursor only ever names a catalog
/// identity, so one named like the form pages as any other. Nothing is evaluated or kept.
inline PowersFound find_powers(const op::Catalog& catalog, const FindPowers& asked) {
    PowersFound out;
    out.powers = static_cast<std::int64_t>(catalog.size());
    out.providers = static_cast<std::int64_t>(catalog.providers().size());
    out.reason = refusal_of(asked);
    if (!out.reason.empty()) {
        return out;
    }
    // The terms are views into this local, which outlives every match below.
    const std::string text = asked.text.value_or(std::string());
    const std::vector<std::string_view> terms = powers_detail::terms_of(text);
    const std::size_t page = static_cast<std::size_t>(asked.limit.value_or(kMaxPowerRows));
    for (const std::string& identity : catalog.identities()) {
        const std::vector<op::Contribution> stack = catalog.contributions(identity);
        if (stack.empty() || stack.back().definition == nullptr ||
            !powers_detail::fits(identity, stack.back(), asked, terms)) {
            continue;
        }
        ++out.total;
        if (asked.after && identity <= *asked.after) {
            continue;
        }
        if (out.rows.size() < page) {
            out.rows.push_back(powers_detail::row_of(identity, stack.back()));
        } else if (out.next.empty()) {
            out.next = out.rows.back().identity;
        }
    }
    // The form follows the catalog: on this page if it has room, else on the page after its last.
    if (powers_detail::form_fits(asked, terms)) {
        ++out.total;
        if (out.rows.size() < page) {
            out.rows.push_back(powers_detail::fold_row());
        } else if (out.next.empty()) {
            out.next = out.rows.back().identity;
        }
    }
    out.ok = true;
    const std::size_t bytes = powers_detail::answer_bytes(loom::to_value(out));
    if (bytes > kMaxPowersAnswerBytes) {
        PowersFound refused;
        refused.powers = out.powers;
        refused.providers = out.providers;
        refused.reason = "these " + std::to_string(out.rows.size()) + " rows come to " +
                         std::to_string(bytes) + " bytes and an answer carries at most " +
                         std::to_string(kMaxPowersAnswerBytes) + "; ask for fewer with limit";
        return refused;
    }
    return out;
}

/// One identity: its contribution in force as a row and as `zengine.OperatorContribution` bytes,
/// and every contribution eligible to satisfy it, the one in force last.
inline PowerDescribed describe_power(const op::Catalog& catalog, const std::string& identity) {
    PowerDescribed out;
    out.identity = identity;
    if (identity.size() > kMaxPowersQueryBytes) {
        out.identity.clear();
        out.reason = "a query carries at most " + std::to_string(kMaxPowersQueryBytes) +
                     " bytes of text; this one carries " + std::to_string(identity.size());
        return out;
    }
    const std::vector<op::Contribution> stack = catalog.contributions(identity);
    if (stack.empty() || stack.back().definition == nullptr) {
        out.reason = "nothing supplies '" + identity + "' here";
        return out;
    }
    try {
        const std::string bytes =
            loom::serialize(op::encode_contribution(*stack.back().definition));
        out.contribution.assign(bytes.begin(), bytes.end());
    } catch (const std::exception& e) {
        out.reason = "'" + identity + "' cannot be written as a contribution: " + e.what();
        return out;
    }
    out.row = powers_detail::row_of(identity, stack.back());
    for (const op::Contribution& c : stack) {
        out.stack.push_back(powers_detail::layer_of(c));
    }
    out.ok = true;
    const std::size_t bytes = powers_detail::answer_bytes(loom::to_value(out));
    if (bytes > kMaxPowersAnswerBytes) {
        PowerDescribed refused;
        refused.identity = identity;
        refused.reason = "'" + identity + "' comes to " + std::to_string(bytes) +
                         " bytes and an answer carries at most " +
                         std::to_string(kMaxPowersAnswerBytes);
        return refused;
    }
    return out;
}

// ---- The door -------------------------------------------------------------------

/// What this door has done: counters only. No answer is kept between asks.
struct PowersDoorState {
    std::int64_t found = 0;      ///< `FindPowers` asks answered, a refusal being an answer
    std::int64_t described = 0;  ///< `DescribePower` asks answered
    std::int64_t unanswered = 0; ///< asks with no sender, so nobody to answer
    ZEN_SHAPE(PowersDoorState, 1, ZEN_FIELD(found), ZEN_FIELD(described), ZEN_FIELD(unanswered));
};

/// The host's discovery participant. A `loom::Weave` rather than a `WeaveBase`, because one of its
/// doors is the hand-built `find_powers_schema()`, which `WeaveBase`'s accept list, made of
/// `ZEN_SHAPE` types, cannot hold. It holds the
/// catalog as `const` and owns nothing: a copy would be a mirror, a non-const reference a
/// controller. Any participant may ask, an office or one speaking for itself as the Terminal does,
/// and the answer goes to it alone; only a send with no sender is left unanswered, and counted.
class PowersDoor final : public loom::Weave {
public:
    explicit PowersDoor(const op::Catalog& catalog) : catalog_(&catalog) {}

    void zen_set_self(loom::WeaveId id) noexcept { self_ = id; }

    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {find_powers_schema(), find_powers_v1_schema(), loom::schema_of<DescribePower>()};
    }
    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        return {loom::schema_of<PowersFound>(), loom::schema_of<PowerDescribed>()};
    }

    void handle(const loom::Message& in, loom::Bus& bus) override {
        if (!in.sender.valid()) {
            ++state_.unanswered;
            return; // a root's send: there is nobody for an answer to go to
        }
        // Answered, not sent: the recipient and correlation are Loom's, and the answer is
        // derived now, from the catalog as it stands at this ask.
        if (loom::same_identity(in.payload.schema(), *find_powers_schema()) ||
            loom::same_identity(in.payload.schema(), *find_powers_v1_schema())) {
            ++state_.found;
            (void)bus.answer(loom::Message(
                loom::to_value(find_powers(*catalog_, find_powers_from(in.payload))), self_));
            return;
        }
        ++state_.described;
        (void)bus.answer(loom::Message(
            loom::to_value(describe_power(
                *catalog_, loom::from_value<DescribePower>(in.payload).identity)),
            self_));
    }

    loom::Value snapshot() const override { return loom::to_value(state_); }
    loom::Value policy() const override {
        const loom::LifecyclePolicy p{};
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(p.max_reloads));
        v.set("revive_from_last_good", loom::Cell::boolean(p.revive_from_last_good));
        return v;
    }
    void revive(const loom::Value& v) override { state_ = loom::from_value<PowersDoorState>(v); }

private:
    const op::Catalog* catalog_;
    loom::WeaveId self_{};
    PowersDoorState state_;
};

/// What a terminal participant may ask the door and hear back: the two asks, known and granted to
/// the door's office alone, and the two answers accepted. The host widens the terminal it mounts
/// with exactly this, so what a case typing the documented line proves is what the host grants.
// WL-TERM-18 -- agents/workshop/terminal.md
inline void let_terminal_find_powers(loom::TerminalVocabulary& vocab, loom::Grant& grant) {
    vocab.knows(find_powers_schema())
        .knows(find_powers_v1_schema())
        .knows(loom::schema_of<DescribePower>())
        .accepts(loom::schema_of<PowersFound>())
        .accepts(loom::schema_of<PowerDescribed>());
    allow_finding_powers(grant);
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_POWERS_DOOR_HPP
