// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPOSER_DRAFT_HPP
#define ZENGINE_COMPOSER_DRAFT_HPP

// WHAT A MAKER HAS AUTHORED SO FAR, and how it becomes a real Loom message.
//
//     Snapshot      one target's decoded vocabulary: the accepted ROOTS, and a
//                   dependency Registry that is ONLY the closure those roots need
//     MessageDraft  one root's fields, each with a PRESENCE and an authored value
//     args_of       the draft -> `loom::Arg`s, every one NAMED
//     compose/assemble/send are Loom's, unchanged and un-wrapped
//
// ---- THIS FILE WRITES NO PARSER, AND THAT IS THE POINT ----------------------
//
// Turning `1000` into an Int is `loom::lex_value`'s job and turning an Int into a
// cell of an Int field is `loom::compose_message`'s. Both already exist, both are
// already what every text frontend in the Loom spends, and a second copy of either
// here would be a second answer to "what does this text mean" -- which is exactly
// the defect `input_lex.hpp` exists to prevent between frontends.
//
// SO EXACTLY ONE DECISION IS MADE HERE, and it is the one the SCHEMA entitles this
// tool to make: `lex_value` infers a type FROM THE TOKEN, because a command line
// has nothing else to go on; a form knows the type from the field. Those are
// opposite directions, and running the command line's rule over a form's bytes is
// wrong in a way that only shows up sometimes -- `1000` typed into a Text field
// lexes to Int and is then refused for a field it was perfectly good for. The
// `quoted` argument is the door the lexer already has for exactly this: quoting is
// the command grammar's way of saying "these bytes are text", and a Text field is
// saying the same thing with a schema instead of a keyboard.
//
//     Text field    lex_value(raw, /*quoted=*/true)   -> always Text, always these bytes
//     any other     lex_value(raw, /*quoted=*/false)  -> the narrowest type, then
//                                                        `place` type-checks it
//
// Everything a maker can get wrong is then refused by the ladder, in the ladder's
// own words, naming the field and its declared kind. `1O00` in an Int field lexes
// to Text (it is not a number) and comes back as
// `field 'delay_ms' (Int) cannot take this value`. Nothing here had to know that.
//
// ---- EVERY ARGUMENT IS NAMED, AND THAT IS WHAT MAKES THIS A FORM ------------
//
// `compose_message`'s ladder is four rungs: named, positional, type-directed,
// prompt. A form has a name for every value it holds, so it only ever climbs the
// first -- and rung 1 is all-or-error, never a guess. The three guessing rungs are
// unreachable from here BY CONSTRUCTION rather than by policy: there is no way to
// build an unnamed `Arg` in this file.
//
// ---- WHAT PRESENCE MEANS ----------------------------------------------------
//
// A field has TWO facts and they are not one: whether the maker authored it at
// all, and what they wrote. `FieldDraft` holds both, separately, forever. An
// absent field contributes NO `Arg`, so `assemble` leaves it out of the Value
// entirely -- which is what makes `Text present with ""` and `Text absent` two
// different messages on the wire, and `Bool present with false` different from
// `Bool unset`. Nothing in this file initialises a value, defaults one, or reads a
// bool out of an unset field.

#include "component/text_box.hpp"

#include <zen/kind.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/terminal/composer.hpp>
#include <zen/terminal/input_lex.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::composer {

/// CAN THIS COMPOSER AUTHOR A VALUE OF THIS KIND -- and the three answers are
/// three different facts about the Loom, not three degrees of one.
///
/// `kScalar`      `lex_value` can produce it and `place` can accept it.
/// `kNoSpelling`  BYTES. `place` would accept a `loom::Bytes`, and there is no way
///                to write one down: `lex_value` has no Bytes branch at all, and
///                neither does any other text surface in the Loom -- the console's
///                own renderer spells a bytes argument `(bytes)` because it cannot
///                spell it either. Inventing hex or base64 here would be inventing
///                a wire convention in a UI, which is the one thing a generic form
///                must not do.
/// `kNotFlat`     MESSAGE and LIST. `place` refuses them outright ("Message/List
///                are not composable from a flat argument list"), so this is the
///                LADDER's own bound and not a decision made here.
enum class Composability { kScalar, kNoSpelling, kNotFlat };

inline Composability composability(loom::Kind k) noexcept {
    switch (k) {
    case loom::Kind::Int:
    case loom::Kind::Float:
    case loom::Kind::Text:
    case loom::Kind::Bool:
        return Composability::kScalar;
    case loom::Kind::Bytes:
        return Composability::kNoSpelling;
    case loom::Kind::Message:
    case loom::Kind::List:
        break;
    }
    return Composability::kNotFlat;
}

/// ONE FIELD OF A DRAFT: whether the maker authored it, and what they wrote.
///
/// TWO MEMBERS AND NOT ONE, which is §17's whole requirement made structural.
/// There is no encoding of absence inside the value -- no empty-means-absent, no
/// sentinel, no optional-of-string -- so the four states a maker can reach
/// (absent, present and empty, present with bytes, present with a chosen bool) are
/// four distinct values of this type and none of them can be confused for another.
///
/// THE VALUE IS A `TextBox` RATHER THAN A `std::string`, and that is the whole of
/// what editing cost this phase: HD-5 extracted text, a caret and a horizontal
/// window as one state with the operations as the only door, and a form field
/// wants exactly that. It is the component's THIRD consumer -- the Terminal's
/// command line, an Inspector property draft, and now every scalar field of every
/// message a maker composes -- and it needed no change of any kind.
///
/// A BOOL'S VALUE LIVES IN THE SAME BOX, holding the word `true` or `false`. That
/// is not a shortcut: the word is what `lex_value` reads, so a bool authored by a
/// keypress and a bool typed on a command line reach `place` through one path. The
/// box is simply never typed into for a Bool -- `cycle` writes it.
struct FieldDraft {
    bool present = false;
    zengine::component::TextBox value;
};

/// THE MESSAGE A MAKER IS WRITING: one shape, and one draft per declared field.
///
/// THE TWO ARE MADE TOGETHER AND CANNOT DRIFT. `begin_draft` is the only door, so
/// `fields.size() == schema->fields().size()` holds from the first instant, in
/// declaration order, and `field(i)` and `fields[i]` are the same field for every
/// i. That is deliberately NOT the parallel-vector shape SEL-0 recorded as a refit
/// risk: the two things paired here are a draft and an IMMUTABLE schema that
/// nothing in this file can add a row to.
struct MessageDraft {
    std::shared_ptr<const loom::Schema> schema;
    std::vector<FieldDraft> fields;
    /// THE SCHEMA'S OWN TYPE SPELLINGS, derived ONCE by `begin_draft`.
    ///
    /// `loom::describe_schema` builds the whole `ShapeDesc` -- every field's spelling
    /// -- and the presentation needs one field's, per field, per row. Deriving it at
    /// the point of use made projecting a form quadratic in the field count: measured
    /// on a 40-row body, 4 us for three fields, 473 us for forty and 1.15 ms for a
    /// hundred and twenty, on every keystroke. Nothing polls and nothing was wrong;
    /// it was simply the same answer computed `rows x fields` times.
    ///
    /// SO IT IS DERIVED WHERE THE FIELDS ARE, and for the fields' own reason: a
    /// schema is immutable and `begin_draft` is the one door, so the three vectors
    /// are made together and cannot drift. There is no second door and no cache to
    /// invalidate -- a draft's shape never changes; choosing another shape makes
    /// another draft.
    loom::ShapeDesc desc;

    bool valid() const noexcept {
        return schema != nullptr && fields.size() == schema->fields().size() &&
               desc.fields.size() == fields.size();
    }
    std::size_t size() const noexcept { return fields.size(); }
    const loom::Field& field(std::size_t i) const { return schema->fields()[i]; }
    /// How this field's declared type is SPELLED -- the schema's own word for it
    /// (`Int`, `List<Bytes>`, `Message(Leaf v1)`), and the only thing this tool ever
    /// says about what a value MEANS.
    const std::string& type_of(std::size_t i) const { return desc.fields[i].type; }
};

/// Open a fresh draft on a shape: every field ABSENT, every value empty.
///
/// NO DEFAULT IS INVENTED, and the required fields are as absent as the optional
/// ones. A required Bool does NOT begin as `false`: `false` is a value a maker
/// could choose, and a form that started there would submit a choice nobody made
/// the first time anybody pressed Submit.
inline MessageDraft begin_draft(std::shared_ptr<const loom::Schema> schema) {
    MessageDraft d;
    d.schema = std::move(schema);
    if (d.schema != nullptr) {
        d.fields.resize(d.schema->fields().size());
        d.desc = loom::describe_schema(*d.schema);
    }
    return d;
}

/// THE ONE GESTURE THAT CHANGES WHETHER A FIELD IS SENT -- and for a Bool it is
/// the same gesture as choosing its value, which is §19 falling out rather than
/// being arranged for.
///
/// A field's states are `absent` followed by its own value space, walked in a
/// ring. Every kind but Bool has a one-element value space (whatever bytes the box
/// holds), so the ring is absent -> present -> absent. Bool's has two, so the ring
/// is unset -> false -> true -> unset. One rule, spelled once, and the three-state
/// Bool is what that rule PRODUCES over a kind with two values.
///
/// GOING ABSENT KEEPS THE BYTES. They are the maker's work and nothing was said
/// about deleting them; toggling a field off to see what the message looks like
/// without it, and then back on, is a thing a person does. What LEAVES the message
/// is the presence, which is the only thing this gesture is about.
inline void cycle(FieldDraft& draft, loom::Kind kind) {
    if (kind != loom::Kind::Bool) {
        draft.present = !draft.present;
        return;
    }
    if (!draft.present) {
        draft.present = true;
        draft.value.set("false", 0);
        return;
    }
    if (draft.value.text() == "false") {
        draft.value.set("true", 0);
        return;
    }
    draft.present = false;
}

/// Is this field's authored state one that can be TYPED into? A Bool's value is
/// chosen, never written, and a field this Composer cannot author at all has no
/// value to write. Everything else takes text, and typing into an absent field is
/// what makes it present -- a maker who has started writing a value has authored
/// the field, and asking them to say so twice would be ceremony.
inline bool typeable(loom::Kind kind) noexcept {
    return composability(kind) == Composability::kScalar && kind != loom::Kind::Bool;
}

/// ONE TARGET'S DECODED VOCABULARY -- and a Composer snapshot is NOT the Registry.
///
/// ---- WHAT IT OWNS -----------------------------------------------------------
///
/// `roots` are the shapes that may actually be SENT to this target, in the target's
/// own declaration order. `deps` is a Registry holding ONLY the structural closure
/// those roots need in order to decode, which is the other half of MSG-1's answer:
/// `zen.SchemaDesc` names a nested message by (name, version), so a consumer handed
/// only the roots cannot decode a root that nests anything.
///
/// ---- WHY THE REGISTRY IS OWNED BY THE SNAPSHOT ------------------------------
///
/// A `loom::Registry`'s `register_schema` is a claim NOBODY EVER RELEASES -- its
/// own header says so. So a single long-lived Registry accumulating every target a
/// maker ever looked at would be an append-only store of every vocabulary in the
/// process, growing for the lifetime of the pane and never shrinking, and every
/// shape in it would go on resolving long after the weave that defined it stopped
/// being asked about. That is a schema CATALOG, which is a thing MSG-R0 and MSG-1
/// deliberately did not build: nothing in this Loom enumerates schemas, and a pane
/// that accumulated one would have built the enumeration the substrate declines to
/// offer.
///
/// A snapshot is therefore replaced WHOLE on every discovery, and the Registry dies
/// with it. Two targets are two snapshots and their vocabularies never meet -- so a
/// root of target A can never resolve a dependency that only target B declared, and
/// a shape cannot be composed for a target that never said it accepted one.
///
/// ---- WHY IT IS A `unique_ptr` -----------------------------------------------
///
/// `loom::Registry` is neither copyable nor movable (deliberately -- a claim has one
/// owner), so "replace it whole" is spelled by replacing the pointer. There is no
/// clear() on a Registry and there should not be: releasing claims is what the
/// destructor is.
struct Snapshot {
    std::unique_ptr<loom::Registry> deps;
    std::vector<std::shared_ptr<const loom::Schema>> roots;

    bool decoded() const noexcept { return deps != nullptr; }
};

/// RESOLVE A SHAPE OUT OF ONE SNAPSHOT, and nowhere else.
///
/// THE ROOTS ARE ASKED FIRST AND THEY ARE NOT IN THE REGISTRY. `decode_accepted_roots`
/// hands them back without registering them, which is what keeps MSG-1's distinction
/// alive all the way down to the send: `deps` is what a root NEEDS and `roots` is what
/// may be SENT. A root that is also somebody's dependency legitimately appears in both,
/// and asking the roots first means a composed message is always built from the shape
/// the target named as a door.
///
/// `resolve_ref` REFUSES, ALWAYS, and says why. `$m1.count` is the command line's way
/// of wiring one received message's output into another's input, and this pane has
/// received no messages: it holds no reply buffer, no history and no correlation to
/// anything but its own discovery question. Refusing is not a limitation being papered
/// over -- it is the only truthful answer a source with nothing to read can give, and
/// `ComposeSource` exists in two halves precisely so it can be given.
class SnapshotSource final : public loom::ComposeSource {
public:
    explicit SnapshotSource(const Snapshot& snapshot) : snapshot_(snapshot) {}

    std::shared_ptr<const loom::Schema> resolve_schema(std::string_view name,
                                                       std::uint32_t version) const override {
        for (const auto& r : snapshot_.roots) {
            if (r != nullptr && r->name() == name && r->version() == version) {
                return r;
            }
        }
        if (snapshot_.deps == nullptr) {
            return nullptr;
        }
        return snapshot_.deps->lookup(name, version);
    }

    std::optional<loom::Cell> resolve_ref(const loom::Ref& ref, std::string* error) const override {
        if (error != nullptr) {
            *error = "cannot read $" + ref.label + "." + ref.field +
                     " -- this pane holds no received messages to refer to";
        }
        return std::nullopt;
    }

private:
    const Snapshot& snapshot_;
};

/// THE DRAFT AS ARGUMENTS -- one NAMED argument per PRESENT, composable field.
///
/// THREE OMISSIONS, EACH FOR A DIFFERENT REASON, AND NONE OF THEM A REFUSAL:
///
///   an absent field       contributes nothing, which is how absence reaches the
///                         wire. `assemble` leaves an unset field out of the Value
///                         and the gate is the backstop for a required one.
///   an unauthorable kind  contributes nothing, because there is no value to
///                         contribute. If it was REQUIRED the ladder says so, by
///                         name, when it finds the field still open -- which is a
///                         better sentence than one written here, because it is the
///                         same sentence a maker gets for any other missing field.
///   a broken draft        contributes nothing at all. A `MessageDraft` whose
///                         schema and fields disagree cannot be produced by
///                         `begin_draft`, and answering with a partial argument
///                         list would compose a message out of half a form.
///
/// SO THIS FUNCTION NEVER REFUSES ANYTHING. Every judgement about what a maker
/// wrote belongs to `compose_message`, in its vocabulary, naming the field and its
/// declared kind. That is the standing rule about where a refusal lives, and here
/// the deeper layer's vocabulary genuinely contains every reason.
inline std::vector<loom::Arg> args_of(const MessageDraft& draft) {
    std::vector<loom::Arg> args;
    if (!draft.valid()) {
        return args;
    }
    for (std::size_t i = 0; i < draft.fields.size(); ++i) {
        const loom::Field& f = draft.field(i);
        if (!draft.fields[i].present ||
            composability(f.type.kind) != Composability::kScalar) {
            continue;
        }
        loom::Arg a;
        a.name = f.name;
        // THE ONE DECISION THIS FILE MAKES. See the header comment: a Text field
        // says "these bytes are text" with a schema, which is what the command
        // grammar's quote says with a keyboard.
        a.value = loom::lex_value(draft.fields[i].value.text(),
                                  f.type.kind == loom::Kind::Text);
        args.push_back(std::move(a));
    }
    return args;
}

/// COMPOSE THE DRAFT, through Loom's ladder and nothing else.
///
/// Thin on purpose: it exists so the suite can ask a DRAFT what it composes to
/// without standing up a bus, and so the weave has one call rather than three. It
/// adds no rule, no fallback and no second refusal.
inline loom::Composition compose(const Snapshot& snapshot, const MessageDraft& draft) {
    if (!draft.valid()) {
        loom::Composition c;
        c.error = "no message is being composed";
        return c;
    }
    const SnapshotSource source(snapshot);
    return loom::compose_message(source, draft.schema->name(), draft.schema->version(),
                                 args_of(draft));
}

} // namespace zengine::composer

#endif // ZENGINE_COMPOSER_DRAFT_HPP
