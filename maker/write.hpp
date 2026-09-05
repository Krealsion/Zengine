// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_WRITE_HPP
#define ZENGINE_MAKER_WRITE_HPP

// THE HOST'S FIELD-WISE WRITE -- one pure function with two callers, and the smallest closure
// that lets a maker's data move between shapes without a Message-constructing operator
// (docs/reference/maker-weave.md).
//
//   the emit       state  -> message    a trigger's answer leaves as the shapes the maker
//                                        declared; unnamed state fields are left alone
//   the edge       state v1 -> state v2  the conversion of a schema edit, mounted as a MIG-0
//                                        operator and spent by the coordinator; a predecessor
//                                        field is copied or named in `drops`, never lost quietly
//
// Two halves, deliberately. `plan_fields` judges what can be judged from the two SCHEMAS alone
// -- a target the shape lacks, a source the shape lacks, a kind that does not match, two
// sources for one field, a constant of a kind a field cannot hold, a required target with
// neither a source nor a constant, and on the edge a predecessor field neither copied nor
// dropped -- so a definition is refused at admission, where a maker can see it. `write_fields`
// runs the plan and adds the one refusal only a VALUE can raise: a required target whose source
// is an optional field that happens to be absent.
//
// A constant is one of the four scalars. `Text` is included, which the composite wire form
// refuses (provider.hpp): that wall is a binding's, not a write's, and this record is the
// write's own.
//
// Also here: the default a data-built schema starts from, and the pack a trigger is spent
// over -- the state's fields, then the message's. Both are where `construct_blind` earns its
// name: a Value for a schema nothing in C++ declared.

#include "operator/catalog.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::maker {

// ---- the default of a data-built schema -------------------------------------------------

inline loom::Value default_value(const std::shared_ptr<const loom::Schema>& schema);

/// The zero of one type: 0, 0.0, "", false, no bytes, the nested schema's own default, or an
/// empty list.
inline loom::Cell default_cell(const loom::TypeRef& type) {
    switch (type.kind) {
    case loom::Kind::Int:
        return loom::Cell::integer(0);
    case loom::Kind::Float:
        return loom::Cell::real(0.0);
    case loom::Kind::Text:
        return loom::Cell::text(std::string());
    case loom::Kind::Bool:
        return loom::Cell::boolean(false);
    case loom::Kind::Bytes:
        return loom::Cell::bytes(loom::Bytes());
    case loom::Kind::Message:
        return loom::Cell::message(default_value(type.message));
    case loom::Kind::List:
        return loom::Cell::list(loom::Cell::Array());
    }
    return loom::Cell::integer(0);
}

/// A conforming Value of `schema` with every required field at its zero and every optional
/// field absent -- what a maker weave's state is before its first trigger, and what
/// `register_weave` seeds last-known-good with.
inline loom::Value default_value(const std::shared_ptr<const loom::Schema>& schema) {
    return loom::construct_blind(schema, [](const loom::Field& f) -> std::optional<loom::Cell> {
        if (!f.required) {
            return std::nullopt;
        }
        return default_cell(f.type);
    });
}

// ---- the pack ------------------------------------------------------------------------------

/// THE PACK A TRIGGER IS SPENT OVER: the state's fields in declared order, then the message's.
/// A name both carry is refused at definition admission, so this never meets one.
// MW-WEAVE-03 -- agents/maker/weave.md
inline std::shared_ptr<const loom::Schema> pack_schema(const std::string& identity,
                                                       const loom::Schema& state,
                                                       const loom::Schema& message) {
    std::vector<loom::Field> fields;
    fields.reserve(state.fields().size() + message.fields().size());
    for (const loom::Field& f : state.fields()) {
        fields.push_back(f);
    }
    for (const loom::Field& f : message.fields()) {
        fields.push_back(f);
    }
    return loom::make_schema(identity + ".in", 1, std::move(fields));
}

/// Fill a pack from a state and a message: every present field of each lands under its own
/// name. An absent optional field stays absent, which is why an optional state field a trigger
/// binds is refused at admission -- the walk would refuse it at spend as `no input named`.
// MW-WEAVE-03 -- agents/maker/weave.md
inline loom::Value pack(const loom::Value& state, const loom::Value& message,
                        std::shared_ptr<const loom::Schema> schema) {
    loom::Value out(std::move(schema));
    for (const loom::Field& f : state.schema().fields()) {
        if (const loom::Cell* c = state.get(f.name); c != nullptr) {
            out.set(f.name, *c);
        }
    }
    for (const loom::Field& f : message.schema().fields()) {
        if (const loom::Cell* c = message.get(f.name); c != nullptr) {
            out.set(f.name, *c);
        }
    }
    return out;
}

// ---- the field-wise write --------------------------------------------------------------------

/// One target field and where its value comes from: a same-named or renamed field of the
/// source, or one scalar constant. Exactly one of the two.
struct FieldSource {
    std::string field;
    std::optional<std::string> source;
    std::optional<loom::Cell> constant;
};

/// What a write produced, or why not.
struct Written {
    bool ok = false;
    std::string reason;
    std::optional<loom::Value> value;

    static Written yes(loom::Value v) {
        Written w;
        w.ok = true;
        w.value = std::move(v);
        return w;
    }
    static Written no(std::string why) {
        Written w;
        w.reason = std::move(why);
        return w;
    }
    explicit operator bool() const noexcept { return ok; }
};

namespace detail {

inline bool scalar_kind(loom::Kind k) noexcept {
    return k == loom::Kind::Int || k == loom::Kind::Float || k == loom::Kind::Text ||
           k == loom::Kind::Bool;
}

inline bool names(const std::vector<std::string>& list, std::string_view name) {
    for (const std::string& n : list) {
        if (n == name) {
            return true;
        }
    }
    return false;
}

} // namespace detail

/// JUDGE A WRITE FROM ITS TWO SCHEMAS -- every refusal that needs no value. Answers the empty
/// string when the plan is sound, else one sentence naming the field. `edge` is the
/// conversion's stricter law: a predecessor field is copied or named in `drops`, and `drops`
/// names only predecessor fields that are not also copied.
// MW-SUCC-04 -- agents/maker/succession.md
inline std::string plan_fields(const loom::Schema& from, const loom::Schema& to,
                               const std::vector<FieldSource>& fields,
                               const std::optional<std::vector<std::string>>& drops, bool edge,
                               std::string_view what) {
    const std::string w(what);
    std::vector<std::string> targets;
    std::vector<std::string> copied;
    for (const FieldSource& fs : fields) {
        const loom::Field* target = to.find(fs.field);
        if (target == nullptr) {
            return w + " names a field `" + to.name() + "` does not declare: `" + fs.field + "`";
        }
        if (detail::names(targets, fs.field)) {
            return w + " names `" + fs.field + "` twice";
        }
        targets.push_back(fs.field);
        if (fs.source && fs.constant) {
            return w + " gives `" + fs.field +
                   "` two sources, a field and a constant; a field has one";
        }
        if (!fs.source && !fs.constant) {
            return w + " gives `" + fs.field + "` neither a source nor a constant";
        }
        if (fs.source) {
            const loom::Field* source = from.find(*fs.source);
            if (source == nullptr) {
                return w + " reads `" + *fs.source + "`, which `" + from.name() +
                       "` does not declare";
            }
            if (!op::detail::same_type(source->type, target->type)) {
                return w + " writes `" + fs.field + "` (" + loom::name_of(target->type.kind) +
                       ") from `" + *fs.source + "` (" + loom::name_of(source->type.kind) +
                       "); the kinds differ";
            }
            copied.push_back(*fs.source);
            continue;
        }
        if (!detail::scalar_kind(target->type.kind)) {
            return w + " writes `" + fs.field + "` (" + loom::name_of(target->type.kind) +
                   ") from a constant; a constant is one of Int, Float, Text or Bool";
        }
        if (fs.constant->kind() != target->type.kind) {
            return w + " writes `" + fs.field + "` (" + loom::name_of(target->type.kind) +
                   ") from a " + loom::name_of(fs.constant->kind()) + " constant";
        }
    }
    for (const loom::Field& f : to.fields()) {
        if (f.required && !detail::names(targets, f.name)) {
            return w + " leaves the required field `" + f.name + "` of `" + to.name() +
                   "` with neither a source nor a constant";
        }
    }
    if (!edge) {
        return std::string();
    }
    std::vector<std::string> dropped;
    if (drops) {
        for (const std::string& d : *drops) {
            if (from.find(d) == nullptr) {
                return w + " drops `" + d + "`, which `" + from.name() + "` does not declare";
            }
            if (detail::names(copied, d)) {
                return w + " both copies and drops `" + d + "`";
            }
            dropped.push_back(d);
        }
    }
    for (const loom::Field& f : from.fields()) {
        if (!detail::names(copied, f.name) && !detail::names(dropped, f.name)) {
            return w + " neither copies nor drops the predecessor's `" + f.name +
                   "`; loss is authored, never inferred";
        }
    }
    return std::string();
}

/// WRITE ONE VALUE INTO ANOTHER SHAPE, field by field, as the plan says -- pure, and the same
/// function for the emit (state -> message) and for the edge (predecessor state -> successor
/// state). Refuses everything `plan_fields` refuses, plus the one thing only a value can show:
/// a required target whose source field is absent.
// MW-SUCC-04 -- agents/maker/succession.md
inline Written write_fields(const loom::Value& from, const std::shared_ptr<const loom::Schema>& to,
                            const std::vector<FieldSource>& fields,
                            const std::optional<std::vector<std::string>>& drops, bool edge,
                            std::string_view what) {
    const std::string planned = plan_fields(from.schema(), *to, fields, drops, edge, what);
    if (!planned.empty()) {
        return Written::no(planned);
    }
    loom::Value out(to);
    for (const FieldSource& fs : fields) {
        if (fs.constant) {
            out.set(fs.field, *fs.constant);
            continue;
        }
        const loom::Cell* cell = from.get(*fs.source);
        if (cell == nullptr) {
            if (to->find(fs.field)->required) {
                return Written::no(std::string(what) + " requires `" + fs.field +
                                   "`, and the source's `" + *fs.source + "` is absent");
            }
            continue;
        }
        out.set(fs.field, *cell);
    }
    return Written::yes(std::move(out));
}

} // namespace zengine::maker

#endif // ZENGINE_MAKER_WRITE_HPP
