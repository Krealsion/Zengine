// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_POWERS_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_POWERS_VOCABULARY_HPP

// Finding a power: `FindPowers` -> `PowersFound` and `DescribePower` -> `PowerDescribed`, answered
// by one read-only office (docs/reference/introspection.md). Values only: a description of the
// host's catalog, and of the evaluator's forms, at the ask, which confers no authority over it. `FindPowers` is built with a
// `loom::SchemaBuilder` rather than `ZEN_SHAPE` because every field of it is optional, so a weaver
// at the Terminal types only what they mean: `ask @zengine.powers FindPowers 1 text=larger`.

#include <zen/schema.hpp>
#include <zen/switchboard/grant.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office the host's discovery door holds. A host that mounts no door holds none, and an ask
/// then reaches nobody.
inline constexpr const char* kPowersRole = "zengine.powers";

// ---- the bounds an ask is held to ------------------------------------------------

/// The most rows one answer carries. An ask for more is refused; `next` continues a page.
inline constexpr std::int64_t kMaxPowerRows = 100;
/// The most bytes of text one ask carries, every text field together.
inline constexpr std::size_t kMaxPowersQueryBytes = 1024;
/// The largest answer the door gives, measured as its bytes: a mebibyte.
inline constexpr std::size_t kMaxPowersAnswerBytes = std::size_t{1} << 20;

// ---- the words a row is classified in ----------------------------------------------

/// `PowerRow::kind`, the most specific of three, each read off the definition at the ask: a Source
/// takes no weaver input (`op::is_source`), a conversion's signature is the edge it converts
/// (`op::declares_migration`), and any other is an operator. A conversion is an operator too, so
/// asking for `operator` finds both.
inline constexpr const char* kSourceKind = "source";
inline constexpr const char* kConversionKind = "conversion";
inline constexpr const char* kOperatorKind = "operator";
/// `PowerRow::construction`, the definition's own answer (`OperatorDef::is_composite`).
inline constexpr const char* kNativeConstruction = "native";
inline constexpr const char* kCompositeConstruction = "composite";
/// A form is not in the catalog: the evaluator spends it, taking an operator reference as its
/// body (`operator/fold.hpp`). Its row's kind and construction say so; its identity is its name.
inline constexpr const char* kFormKind = "form";
inline constexpr const char* kEvaluatorConstruction = "evaluator";
/// `FindPowers::fits`: what the operators asked for could be. `fold`, a fold's body: an Int port
/// for the count, and another port of its answer's type for the accumulator.
inline constexpr const char* kFitsFold = "fold";

// ---- asking -------------------------------------------------------------------------

inline constexpr const char* kFindPowersName = "FindPowers";
inline constexpr std::uint32_t kFindPowersVersion = 2;

/// `FindPowers v1`'s fields, every one optional: an absent field asks nothing of a row.
inline loom::SchemaBuilder find_powers_fields(std::uint32_t version) {
    return loom::SchemaBuilder(kFindPowersName, version)
            // every whitespace-separated term, in any case, in the identity or the prose
            .field("text", loom::Kind::Text, /*required=*/false)
            // an input port's type as Loom spells it: `Int`, `List<Int>`, `Message(a.B v1)`
            .field("takes", loom::Kind::Text, /*required=*/false)
            // the output port's type, spelled the same way
            .field("yields", loom::Kind::Text, /*required=*/false)
            // who contributed the contribution in force
            .field("provider", loom::Kind::Text, /*required=*/false)
            // `source`, `operator` or `conversion`
            .field("kind", loom::Kind::Text, /*required=*/false)
            // `native` or `composite`
            .field("construction", loom::Kind::Text, /*required=*/false)
            // true asks only for powers offered for reuse, false only for those that are not
            .field("offered", loom::Kind::Bool, /*required=*/false)
            // the rows after this identity in the catalog's order: where a page continues
            .field("after", loom::Kind::Text, /*required=*/false)
            // how many rows, 1 to `kMaxPowerRows`; absent asks for the most an answer carries
            .field("limit", loom::Kind::Int, /*required=*/false);
}

/// `FindPowers v2`, the version this build asks with: v1's fields and `fits`.
inline std::shared_ptr<const loom::Schema> find_powers_schema() {
    static const auto s = find_powers_fields(kFindPowersVersion)
                              // `fold`: only operators a fold could spend as its body
                              .field("fits", loom::Kind::Text, /*required=*/false)
                              .build();
    return s;
}

/// `FindPowers v1`, which the door still answers: a line typed at the Terminal may say `1`.
inline std::shared_ptr<const loom::Schema> find_powers_v1_schema() {
    static const auto s = find_powers_fields(1).build();
    return s;
}

/// A `FindPowers` as C++ holds it: an absent field is `std::nullopt`, never an empty string.
struct FindPowers {
    std::optional<std::string> text;
    std::optional<std::string> takes;
    std::optional<std::string> yields;
    std::optional<std::string> provider;
    std::optional<std::string> kind;
    std::optional<std::string> construction;
    std::optional<bool> offered;
    std::optional<std::string> after;
    std::optional<std::int64_t> limit;
    std::optional<std::string> fits;
};

/// The ask as a value at `find_powers_schema()`, carrying only the fields it was given.
inline loom::Value find_powers_value(const FindPowers& asked) {
    loom::Value v(find_powers_schema());
    const auto text = [&v](const char* field, const std::optional<std::string>& said) {
        if (said) {
            v.set(field, loom::Cell::text(*said));
        }
    };
    text("text", asked.text);
    text("takes", asked.takes);
    text("yields", asked.yields);
    text("provider", asked.provider);
    text("kind", asked.kind);
    text("construction", asked.construction);
    if (asked.offered) {
        v.set("offered", loom::Cell::boolean(*asked.offered));
    }
    text("after", asked.after);
    if (asked.limit) {
        v.set("limit", loom::Cell::integer(*asked.limit));
    }
    text("fits", asked.fits);
    return v;
}

/// The ask back out of a value admitted at either version of `FindPowers`.
inline FindPowers find_powers_from(const loom::Value& v) {
    FindPowers asked;
    const auto text = [&v](const char* field) -> std::optional<std::string> {
        const loom::Cell* c = v.schema().find(field) == nullptr ? nullptr : v.get(field);
        return c == nullptr ? std::nullopt : std::optional<std::string>(c->as_text());
    };
    asked.text = text("text");
    asked.takes = text("takes");
    asked.yields = text("yields");
    asked.provider = text("provider");
    asked.kind = text("kind");
    asked.construction = text("construction");
    if (const loom::Cell* c = v.get("offered"); c != nullptr) {
        asked.offered = c->as_bool();
    }
    asked.after = text("after");
    if (const loom::Cell* c = v.get("limit"); c != nullptr) {
        asked.limit = c->as_int();
    }
    asked.fits = text("fits");
    return asked;
}

/// Ask for one identity: the contribution in force, and every contribution eligible to satisfy it.
struct DescribePower {
    std::string identity;
    ZEN_SHAPE(DescribePower, 1, ZEN_FIELD(identity));
};

// ---- answering ----------------------------------------------------------------------

/// Which shape: name, version and content id together, because the gate compares them as one
/// (`loom::same_identity`). An identity, not a structure.
struct SchemaIdentity {
    std::string name;
    std::int64_t version = 0;
    /// `loom::ContentId`'s 64 bits reinterpreted as Loom's signed Int: compared, never ordered.
    std::int64_t content_id = 0;

    ZEN_SHAPE(SchemaIdentity, 1, ZEN_FIELD(name), ZEN_FIELD(version), ZEN_FIELD(content_id));
};

/// One power as the door reads it off the contribution in force, at the ask.
struct PowerRow {
    std::string identity;
    std::string provider;     ///< who contributed it; empty when the host published it itself
    std::string kind;         ///< `kSourceKind`, `kConversionKind` or `kOperatorKind`
    std::string construction; ///< `kNativeConstruction` or `kCompositeConstruction`
    bool offered = true;      ///< the contributor's mark: false for a participant's own reaction
    std::string about;        ///< what it is for, in the contributor's words; empty if it said none
    std::string signature;    ///< `(lhs: Int, rhs: Int) -> result: Int`, in Loom's spelling
    SchemaIdentity inputs;    ///< the input ports' schema, which a composition binds against
    SchemaIdentity outputs;   ///< the output ports' schema, what spending it yields

    ZEN_SHAPE(PowerRow, 1, ZEN_FIELD(identity), ZEN_FIELD(provider), ZEN_FIELD(kind),
              ZEN_FIELD(construction), ZEN_FIELD(offered), ZEN_FIELD(about), ZEN_FIELD(signature),
              ZEN_FIELD(inputs), ZEN_FIELD(outputs));
};

/// What `FindPowers` found: one page of rows in the catalog's order, filtered and never ranked.
struct PowersFound {
    bool ok = false;
    std::string reason;         ///< why the ask was refused, in words; empty when `ok`
    std::vector<PowerRow> rows;
    std::string next;           ///< `after` for the next page; empty when this page is the last
    std::int64_t total = 0;     ///< how many rows the whole query matches, across every page
    std::int64_t powers = 0;    ///< how many identities the catalog resolves
    std::int64_t providers = 0; ///< how many providers are mounted

    ZEN_SHAPE(PowersFound, 1, ZEN_FIELD(ok), ZEN_FIELD(reason), ZEN_FIELD(rows), ZEN_FIELD(next),
              ZEN_FIELD(total), ZEN_FIELD(powers), ZEN_FIELD(providers));
};

/// One contribution eligible to satisfy a power, in its contributor's words.
struct PowerLayer {
    std::string provider;     ///< empty when the host published it itself
    std::string construction; ///< `kNativeConstruction` or `kCompositeConstruction`
    bool offered = true;
    std::string about;

    ZEN_SHAPE(PowerLayer, 1, ZEN_FIELD(provider), ZEN_FIELD(construction), ZEN_FIELD(offered),
              ZEN_FIELD(about));
};

/// What `DescribePower` said of one identity.
struct PowerDescribed {
    bool ok = false;
    std::string reason;            ///< why nothing is described, in words; empty when `ok`
    std::string identity;          ///< the identity asked about
    PowerRow row;                  ///< the contribution in force, as `FindPowers` rows it
    std::vector<PowerLayer> stack; ///< every eligible contribution, the one in force last
    loom::Bytes contribution;      ///< the one in force as `zengine.OperatorContribution` bytes

    ZEN_SHAPE(PowerDescribed, 1, ZEN_FIELD(ok), ZEN_FIELD(reason), ZEN_FIELD(identity),
              ZEN_FIELD(row), ZEN_FIELD(stack), ZEN_FIELD(contribution));
};

/// The two asks, to the door's office and nowhere else: what a host writes on a participant it
/// lets find powers. Asking describes; it grants nothing to send, mount or open.
inline void allow_finding_powers(loom::Grant& grant) {
    grant.allow_to_role(kFindPowersName, 1, kPowersRole);
    grant.allow_to_role(kFindPowersName, kFindPowersVersion, kPowersRole);
    grant.allow_to_role(DescribePower::zen_name, DescribePower::zen_version, kPowersRole);
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_POWERS_VOCABULARY_HPP
