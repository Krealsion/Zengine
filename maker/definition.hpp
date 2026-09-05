// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_MAKER_DEFINITION_HPP
#define ZENGINE_MAKER_DEFINITION_HPP

// THE TWO ARTIFACTS OF A MAKER WEAVE, AS LOOM SCHEMAS (docs/reference/maker-weave.md).
//
//   THE DEFINITION   `zengine.maker.Definition v1` -- the maker's stable dotted name (it
//                    namespaces the maker's shapes), the state schema and the accepted and
//                    emitted shapes as `zen.SchemaDesc` descriptors, the triggers (`On`) and,
//                    on a schema edit's successor, the conversion from the predecessor's state.
//   THE STATE        the maker's own Value at its own schema, in its own envelope -- no wrapper.
//
// Both are persisted as NATIVE BYTES and never as JSON: a native envelope carries a mandatory
// content id, so a reader can challenge a claim before decoding a field, and canonical bytes are
// content-addressable. The definition carries its format word and its format version INSIDE the
// value, tied to the envelope's shape version by `static_assert`, so a file of another version
// is refused BY ITS NUMBER before its fields are judged, and a body that then says it is another
// vintage is a forgery -- the discipline the Workshop's session file keeps
// (workshop/session_persist.hpp), restated for this pair.
//
// THE SEVEN KINDS CLOSE THE MAKER PATH (agents/decisions/the-seven-kinds-close-the-maker-path.md):
// a keyed table is a list of entries, a one-of is several optional fields, optionality is the
// field's `required` bit, and a state that nests a Message or a List rides the same optional
// `referenced` section `zen.Manifest` uses, post-order, decoded by the same codec.
//
// NO SIGNATURE, NO PROVENANCE FIELD OF ANY KIND -- deliberately. A declared, unsigned name would
// be a claim nothing verifies; when identity arrives it is a v2 wrapping this v1 as a nested
// Message with one conversion edge, and this file's own tripwire in the suite says the word is
// absent today.
//
// A TRIGGER IS ONE COMPOSITE OVER THE HOST'S CATALOG, in the composition wire form the operator
// provider seam already carries (`zengine.OperatorComposition v1`): a pack of the state's fields
// then the message's is spent through `Catalog::evaluate` at delivery, resolving every operator
// at spend, and the one answer is written to the named state field.

#include "maker/write.hpp"
#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/provider.hpp"

#include <zen/gate.hpp>
#include <zen/kernel/schema_codec.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::maker {

// ---- the format, tied ----------------------------------------------------------------------

/// The word a definition file says it is. Another word is another kind of file.
inline constexpr const char* kFormat = "zengine-maker-definition";

/// The definition format's version, carried inside the value.
inline constexpr std::int64_t kFormatVersion = 1;

/// The definition schema's envelope version -- the number a file of another version is refused
/// by, on the claim, before a field is read.
// MW-DEF-01 -- agents/maker/definition.md
inline constexpr std::uint32_t kDefinitionSchemaVersion = 1;

/// THE TWO NUMBERS ARE ONE, for the reason the session file's pair is: the envelope's version
/// gates the claim, the field is checked again after admission, and a value whose field disagrees
/// with its envelope is a forgery -- so the two may never drift apart in this source.
static_assert(kDefinitionSchemaVersion == static_cast<std::uint32_t>(kFormatVersion),
              "the definition file's format version and its envelope's shape version are one "
              "number: a file of another version must be refused by ITS number, before its "
              "fields are judged");

// ---- the schemas -----------------------------------------------------------------------------

/// One target field and where its value comes from: a source field, or one scalar constant in
/// the field of its kind. The four constant fields are the four scalars, `Text` included.
inline std::shared_ptr<const loom::Schema> field_source_schema() {
    static const auto s = loom::SchemaBuilder("zengine.maker.FieldSource", 1)
                              .field("field", loom::Kind::Text)
                              .field("source", loom::Kind::Text, /*required=*/false)
                              .field("int_constant", loom::Kind::Int, /*required=*/false)
                              .field("float_constant", loom::Kind::Float, /*required=*/false)
                              .field("text_constant", loom::Kind::Text, /*required=*/false)
                              .field("bool_constant", loom::Kind::Bool, /*required=*/false)
                              .build();
    return s;
}

/// One emission of a trigger: an emitted shape and how its fields are written from the state.
inline std::shared_ptr<const loom::Schema> emit_record_schema() {
    static const auto s = loom::SchemaBuilder("zengine.maker.Emit", 1)
                              .field("message_name", loom::Kind::Text)
                              .field("message_version", loom::Kind::Int)
                              .list("fields", loom::type_message(field_source_schema()))
                              .build();
    return s;
}

/// One trigger: the accepted message it fires on, its body as a composition, the state field
/// its one answer is written to, and what it emits afterwards.
inline std::shared_ptr<const loom::Schema> on_record_schema() {
    static const auto s = loom::SchemaBuilder("zengine.maker.On", 1)
                              .field("message_name", loom::Kind::Text)
                              .field("message_version", loom::Kind::Int)
                              .message("body", op::composition_schema())
                              .field("output", loom::Kind::Text)
                              .list("emit", loom::type_message(emit_record_schema()),
                                    /*required=*/false)
                              .build();
    return s;
}

/// The conversion a schema edit's successor carries: the predecessor's state schema (checked by
/// identity when the edge is spent), how each successor field is written, and which predecessor
/// fields are deliberately dropped -- authored loss, never inferred.
inline std::shared_ptr<const loom::Schema> conversion_schema() {
    static const auto s = loom::SchemaBuilder("zengine.maker.Conversion", 1)
                              .message("from", loom::schema_desc_schema())
                              .list("fields", loom::type_message(field_source_schema()))
                              .list("drops", loom::type_of(loom::Kind::Text), /*required=*/false)
                              .build();
    return s;
}

/// The definition artifact.
// MW-DEF-01 -- agents/maker/definition.md
inline std::shared_ptr<const loom::Schema> definition_schema() {
    static const auto s =
        loom::SchemaBuilder("zengine.maker.Definition", kDefinitionSchemaVersion)
            .field("format", loom::Kind::Text)
            .field("format_version", loom::Kind::Int)
            .field("name", loom::Kind::Text)
            .field("revision", loom::Kind::Int)
            .list("referenced", loom::type_message(loom::schema_desc_schema()),
                  /*required=*/false)
            .message("state", loom::schema_desc_schema())
            .list("accepts", loom::type_message(loom::schema_desc_schema()))
            .list("emits", loom::type_message(loom::schema_desc_schema()))
            .list("on", loom::type_message(on_record_schema()))
            .message("conversion", conversion_schema(), /*required=*/false)
            .build();
    return s;
}

// ---- the definition, in memory --------------------------------------------------------------

/// One emission: the shape, and the write that fills it from the state.
struct Emit {
    std::shared_ptr<const loom::Schema> message;
    std::vector<FieldSource> fields;
};

/// One trigger.
struct On {
    std::shared_ptr<const loom::Schema> message;
    op::Composite body;
    std::string output;
    std::vector<Emit> emits;
};

/// The conversion of a schema edit.
struct Conversion {
    std::shared_ptr<const loom::Schema> from;
    std::vector<FieldSource> fields;
    std::optional<std::vector<std::string>> drops;
};

/// A maker weave's definition as the interpreter holds it: schemas rebuilt from descriptors,
/// bodies decoded to the graph the one evaluator walks.
struct Definition {
    std::string name;
    std::int64_t revision = 1;
    std::shared_ptr<const loom::Schema> state;
    std::vector<std::shared_ptr<const loom::Schema>> accepts;
    std::vector<std::shared_ptr<const loom::Schema>> emits;
    std::vector<On> on;
    std::optional<Conversion> conversion;

    /// The provider this revision mounts its bodies under -- per revision, so an edit mounts
    /// the successor's bodies beside the incumbent's and a collision at mount is a maker's
    /// second live copy of one definition, refused by the catalog in its own words.
    std::string provider() const {
        return "zengine.maker." + name + ".r" + std::to_string(revision);
    }

    /// The identity of one trigger's body in the catalog.
    std::string trigger_identity(const On& trigger) const {
        return name + ".r" + std::to_string(revision) + ".on." + trigger.message->name();
    }
};

// ---- encoding ---------------------------------------------------------------------------------

namespace detail {

inline loom::Value encode_field_source(const FieldSource& fs) {
    loom::Value v(field_source_schema());
    v.set("field", loom::Cell::text(fs.field));
    if (fs.source) {
        v.set("source", loom::Cell::text(*fs.source));
    }
    if (fs.constant) {
        switch (fs.constant->kind()) {
        case loom::Kind::Int:
            v.set("int_constant", *fs.constant);
            break;
        case loom::Kind::Float:
            v.set("float_constant", *fs.constant);
            break;
        case loom::Kind::Text:
            v.set("text_constant", *fs.constant);
            break;
        case loom::Kind::Bool:
            v.set("bool_constant", *fs.constant);
            break;
        default:
            throw std::invalid_argument("a field source's constant is one of Int, Float, Text "
                                        "or Bool; `" +
                                        fs.field + "` was given a " +
                                        loom::name_of(fs.constant->kind()));
        }
    }
    return v;
}

inline loom::Cell encode_field_sources(const std::vector<FieldSource>& fields) {
    loom::Cell::Array cells;
    cells.reserve(fields.size());
    for (const FieldSource& fs : fields) {
        cells.push_back(loom::Cell::message(encode_field_source(fs)));
    }
    return loom::Cell::list(std::move(cells));
}

inline loom::Cell encode_descriptors(const std::vector<std::shared_ptr<const loom::Schema>>& list) {
    loom::Cell::Array cells;
    cells.reserve(list.size());
    for (const auto& s : list) {
        cells.push_back(loom::Cell::message(loom::encode_schema(*s)));
    }
    return loom::Cell::list(std::move(cells));
}

inline loom::Value encode_on(const On& trigger) {
    loom::Value v(on_record_schema());
    v.set("message_name", loom::Cell::text(trigger.message->name()));
    v.set("message_version",
          loom::Cell::integer(static_cast<std::int64_t>(trigger.message->version())));
    v.set("body", loom::Cell::message(op::detail::encode_composition(trigger.body)));
    v.set("output", loom::Cell::text(trigger.output));
    if (!trigger.emits.empty()) {
        loom::Cell::Array emits;
        emits.reserve(trigger.emits.size());
        for (const Emit& e : trigger.emits) {
            loom::Value ev(emit_record_schema());
            ev.set("message_name", loom::Cell::text(e.message->name()));
            ev.set("message_version",
                   loom::Cell::integer(static_cast<std::int64_t>(e.message->version())));
            ev.set("fields", encode_field_sources(e.fields));
            emits.push_back(loom::Cell::message(std::move(ev)));
        }
        v.set("emit", loom::Cell::list(std::move(emits)));
    }
    return v;
}

inline loom::Value encode_conversion(const Conversion& c) {
    loom::Value v(conversion_schema());
    v.set("from", loom::Cell::message(loom::encode_schema(*c.from)));
    v.set("fields", encode_field_sources(c.fields));
    if (c.drops) {
        loom::Cell::Array drops;
        drops.reserve(c.drops->size());
        for (const std::string& d : *c.drops) {
            drops.push_back(loom::Cell::text(d));
        }
        v.set("drops", loom::Cell::list(std::move(drops)));
    }
    return v;
}

} // namespace detail

/// The definition as the Value its file holds. Every schema the state, the accepted and
/// emitted shapes and the conversion's source nest is listed in `referenced`, post-order, before
/// anything that references it -- `zen.Manifest`'s rule, through `zen.Manifest`'s codec.
inline loom::Value encode_definition(const Definition& d) {
    loom::Value v(definition_schema());
    v.set("format", loom::Cell::text(kFormat));
    v.set("format_version", loom::Cell::integer(kFormatVersion));
    v.set("name", loom::Cell::text(d.name));
    v.set("revision", loom::Cell::integer(d.revision));

    std::vector<std::shared_ptr<const loom::Schema>> referenced;
    loom::collect_referenced(*d.state, referenced);
    for (const auto& s : d.accepts) {
        loom::collect_referenced(*s, referenced);
    }
    for (const auto& s : d.emits) {
        loom::collect_referenced(*s, referenced);
    }
    if (d.conversion) {
        loom::collect_referenced(*d.conversion->from, referenced);
    }
    if (!referenced.empty()) {
        v.set("referenced", detail::encode_descriptors(referenced));
    }
    v.set("state", loom::Cell::message(loom::encode_schema(*d.state)));
    v.set("accepts", detail::encode_descriptors(d.accepts));
    v.set("emits", detail::encode_descriptors(d.emits));
    loom::Cell::Array ons;
    ons.reserve(d.on.size());
    for (const On& trigger : d.on) {
        ons.push_back(loom::Cell::message(detail::encode_on(trigger)));
    }
    v.set("on", loom::Cell::list(std::move(ons)));
    if (d.conversion) {
        v.set("conversion", loom::Cell::message(detail::encode_conversion(*d.conversion)));
    }
    return v;
}

/// The definition file's bytes: native, canonical, content-addressed.
inline std::string definition_bytes(const Definition& d) {
    return loom::serialize(encode_definition(d));
}

// ---- admission --------------------------------------------------------------------------------

/// A definition the interpreter will register, or why not -- one sentence, naming the trigger,
/// the message or the field.
struct Admitted {
    bool ok = false;
    std::string reason;
    Definition definition;

    static Admitted no(std::string why) {
        Admitted a;
        a.reason = std::move(why);
        return a;
    }
    explicit operator bool() const noexcept { return ok; }
};

namespace detail {

inline std::shared_ptr<const loom::Schema>
find_shape(const std::vector<std::shared_ptr<const loom::Schema>>& list, std::string_view name,
           std::uint32_t version) {
    for (const auto& s : list) {
        if (s->name() == name && s->version() == version) {
            return s;
        }
    }
    return nullptr;
}

inline std::string shape_label(std::string_view name, std::int64_t version) {
    return "`" + std::string(name) + " v" + std::to_string(version) + "`";
}

inline FieldSource decode_field_source(const loom::Value& v) {
    FieldSource fs;
    fs.field = v.get("field")->as_text();
    if (const loom::Cell* s = v.get("source"); s != nullptr) {
        fs.source = s->as_text();
    }
    int constants = 0;
    for (const char* which : {"int_constant", "float_constant", "text_constant", "bool_constant"}) {
        if (const loom::Cell* c = v.get(which); c != nullptr) {
            ++constants;
            fs.constant = *c;
        }
    }
    if (constants > 1) {
        throw std::invalid_argument("a field source gives `" + fs.field +
                                    "` more than one constant; a field has one");
    }
    return fs;
}

inline std::vector<FieldSource> decode_field_sources(const loom::Cell& list) {
    std::vector<FieldSource> out;
    for (const loom::Cell& c : list.as_list()) {
        out.push_back(decode_field_source(*c.as_message()));
    }
    return out;
}

/// Does any node of this graph read the composite's input `field`?
inline bool binds_input(const op::Composite& body, std::string_view field) {
    for (const op::Node& n : body.nodes) {
        for (const op::Binding& b : n.arguments) {
            if (b.from() == op::Binding::From::Input && b.input_name() == field) {
                return true;
            }
        }
    }
    return false;
}

} // namespace detail

/// ADMIT A DEFINITION VALUE THAT HAS PASSED THE GATE at `definition_schema()`: the format word
/// and the format version; the name and its namespace over the state; every descriptor
/// decoded through the codec with `referenced` first; every trigger's message accepted, output
/// declared, pack free of a name both carry, and free of an optional state field it binds;
/// every emit declared and its write planned; the conversion's write planned against its own
/// `from`. Refused in one sentence, naming what would have worked.
// MW-DEF-02, MW-DEF-04, MW-DEF-06 -- agents/maker/definition.md
inline Admitted admit_definition(const loom::Value& v) {
    try {
        Definition d;
        const std::string format = v.get("format")->as_text();
        if (format != kFormat) {
            return Admitted::no("not a maker definition: it says it is `" + format + "`, not `" +
                                kFormat + "`");
        }
        const std::int64_t version = v.get("format_version")->as_int();
        if (version != kFormatVersion) {
            return Admitted::no("a definition whose version field says " +
                                std::to_string(version) + " inside an envelope of version " +
                                std::to_string(kDefinitionSchemaVersion) + " is a forgery");
        }
        d.name = v.get("name")->as_text();
        if (d.name.empty()) {
            return Admitted::no("a definition needs a name; it namespaces the maker's shapes");
        }
        d.revision = v.get("revision")->as_int();
        if (d.revision < 1) {
            return Admitted::no("a definition's revision counts from 1");
        }

        loom::Registry deps;
        loom::decode_referenced(v, deps);
        d.state = loom::decode_schema(*v.get("state")->as_message(), deps);
        const std::string prefix = d.name + ".";
        if (d.state->name().size() <= prefix.size() ||
            d.state->name().compare(0, prefix.size(), prefix) != 0) {
            return Admitted::no("the state schema `" + d.state->name() +
                                "` is outside the definition's namespace; its name must begin `" +
                                prefix + "`");
        }
        for (const loom::Cell& c : v.get("accepts")->as_list()) {
            d.accepts.push_back(loom::decode_schema(*c.as_message(), deps));
        }
        for (const loom::Cell& c : v.get("emits")->as_list()) {
            d.emits.push_back(loom::decode_schema(*c.as_message(), deps));
        }

        for (const loom::Cell& c : v.get("on")->as_list()) {
            const loom::Value& rec = *c.as_message();
            On trigger;
            const std::string mname = rec.get("message_name")->as_text();
            const std::int64_t mversion = rec.get("message_version")->as_int();
            trigger.message =
                detail::find_shape(d.accepts, mname, static_cast<std::uint32_t>(mversion));
            if (!trigger.message) {
                return Admitted::no("a trigger names " + detail::shape_label(mname, mversion) +
                                    ", which the definition does not accept");
            }
            trigger.body = op::detail::decode_composition(*rec.get("body")->as_message());
            trigger.output = rec.get("output")->as_text();
            if (d.state->find(trigger.output) == nullptr) {
                return Admitted::no("the trigger on `" + mname + "` writes `" + trigger.output +
                                    "`, which `" + d.state->name() + "` does not declare");
            }
            for (const loom::Field& f : trigger.message->fields()) {
                if (d.state->find(f.name) != nullptr) {
                    return Admitted::no("`" + d.state->name() + "` and `" + mname +
                                        "` both declare `" + f.name +
                                        "`; the pack of a trigger cannot carry two");
                }
            }
            for (const loom::Field& f : d.state->fields()) {
                if (!f.required && detail::binds_input(trigger.body, f.name)) {
                    return Admitted::no("the trigger on `" + mname +
                                        "` binds the optional state field `" + f.name +
                                        "`; an optional field is admitted only where no "
                                        "trigger binds it");
                }
            }
            if (const loom::Cell* emits = rec.get("emit"); emits != nullptr) {
                for (const loom::Cell& ec : emits->as_list()) {
                    const loom::Value& erec = *ec.as_message();
                    Emit e;
                    const std::string ename = erec.get("message_name")->as_text();
                    const std::int64_t eversion = erec.get("message_version")->as_int();
                    e.message =
                        detail::find_shape(d.emits, ename, static_cast<std::uint32_t>(eversion));
                    if (!e.message) {
                        return Admitted::no("the trigger on `" + mname + "` emits " +
                                            detail::shape_label(ename, eversion) +
                                            ", which the definition does not declare");
                    }
                    e.fields = detail::decode_field_sources(*erec.get("fields"));
                    const std::string planned =
                        plan_fields(*d.state, *e.message, e.fields, std::nullopt, false,
                                    "the emit of `" + ename + "`");
                    if (!planned.empty()) {
                        return Admitted::no(planned);
                    }
                    trigger.emits.push_back(std::move(e));
                }
            }
            d.on.push_back(std::move(trigger));
        }

        if (const loom::Cell* conv = v.get("conversion"); conv != nullptr) {
            const loom::Value& crec = *conv->as_message();
            Conversion c;
            c.from = loom::decode_schema(*crec.get("from")->as_message(), deps);
            c.fields = detail::decode_field_sources(*crec.get("fields"));
            if (const loom::Cell* drops = crec.get("drops"); drops != nullptr) {
                std::vector<std::string> names;
                for (const loom::Cell& dc : drops->as_list()) {
                    names.push_back(dc.as_text());
                }
                c.drops = std::move(names);
            }
            const std::string planned =
                plan_fields(*c.from, *d.state, c.fields, c.drops, true,
                            "the conversion from " +
                                detail::shape_label(c.from->name(), c.from->version()));
            if (!planned.empty()) {
                return Admitted::no(planned);
            }
            d.conversion = std::move(c);
        }

        Admitted a;
        a.ok = true;
        a.definition = std::move(d);
        return a;
    } catch (const std::exception& e) {
        return Admitted::no(e.what());
    }
}

/// READ A DEFINITION FILE: the envelope's claim first -- another shape, or another version of
/// this one, is refused by its number before a field is decoded -- then the one gate, then
/// admission. Nothing converts a definition of another version; a definition is the maker's to
/// re-save.
// MW-DEF-01 -- agents/maker/definition.md
inline Admitted read_definition(std::string_view bytes) {
    const loom::Unverified claim = loom::parse(bytes);
    if (!claim.well_formed()) {
        return Admitted::no("these bytes are not a Zen value");
    }
    if (claim.claimed_name() != definition_schema()->name()) {
        return Admitted::no("not a maker definition: the bytes claim `" + claim.claimed_name() +
                            "`");
    }
    if (claim.claimed_version() != kDefinitionSchemaVersion) {
        return Admitted::no("a definition of version " + std::to_string(claim.claimed_version()) +
                            "; this build reads version " +
                            std::to_string(kDefinitionSchemaVersion) +
                            " and converts no other");
    }
    loom::Admission admitted = loom::admit(claim, definition_schema());
    if (!admitted) {
        return Admitted::no(admitted.first_error().message());
    }
    return admit_definition(admitted.value());
}

/// READ A STATE FILE AT THE DEFINITION'S STATE SCHEMA, and at nothing else: the claim must be
/// this shape at this version, and the bytes must admit. A state of another version is refused
/// by name -- reload is shape-only. Nothing here asks a conversion; the one live edge a schema
/// edit mounts (`op::migrate` over the successor's catalog) is the arm this reader does not take
/// this phase.
// MW-DEF-07 -- agents/maker/definition.md
inline Written read_state(std::string_view bytes, const std::shared_ptr<const loom::Schema>& schema) {
    const loom::Unverified claim = loom::parse(bytes);
    if (!claim.well_formed()) {
        return Written::no("these bytes are not a Zen value");
    }
    if (claim.claimed_name() != schema->name() || claim.claimed_version() != schema->version()) {
        return Written::no("a state of " + detail::shape_label(claim.claimed_name(),
                                                                claim.claimed_version()) +
                           " was written; this definition's state is " +
                           detail::shape_label(schema->name(), schema->version()) +
                           ", and nothing converts a state file this phase");
    }
    loom::Admission admitted = loom::admit(claim, schema);
    if (!admitted) {
        return Written::no(admitted.first_error().message());
    }
    return Written::yes(std::move(admitted).value());
}

} // namespace zengine::maker

#endif // ZENGINE_MAKER_DEFINITION_HPP
