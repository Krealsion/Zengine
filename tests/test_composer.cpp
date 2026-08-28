// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#include <doctest.h>

// The Message Composer's PURE half (MSG-0): what a maker has authored, what that
// composes to, and what they would see.
//
// EVERY CASE HERE IS A FUNCTION OVER A VALUE. There is no bus, no Kernel, no
// Workshop and no medium: `composer/draft.hpp` and `composer/view.hpp` link no
// switchboard, so what a DRAFT means and what a PANE would show can be asked of a
// value rather than of a running system. Where the fact comes from -- a real
// target's real answer, through the real load path -- is the Workshop panes
// suite's claim (test_workshop_panes.cpp, the MSG-0 tier), because that is where
// the real library and the real pane protocol live.
//
// NOT ONE SHAPE IN THIS FILE IS ONE THE COMPOSER COMPILED AGAINST, and that is the
// point of building them with `SchemaBuilder` rather than declaring ZEN_SHAPE
// structs. A form generated from a `loom::Schema` the tool has never heard of is
// the whole product claim; a case that used the Timer's own C++ types would prove
// the claim for exactly one target.
// Portable (no OS boundary, no library load).

#include "composer/draft.hpp"
#include "composer/view.hpp"
#include "composer/vocabulary.hpp"

#include <zen/kind.hpp>
#include <zen/registry.hpp>
#include <zen/schema.hpp>
#include <zen/terminal/composer.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cmp = zengine::composer;
namespace surface = zengine::surface;

namespace {

// ---- vocabularies this binary has never compiled against --------------------

/// A THREE-SCALAR SHAPE, all required -- the shape of the real Timer's `StartTimer`
/// and deliberately not that type. It is built at run time, so nothing about the
/// form below can have come from a header.
std::shared_ptr<const loom::Schema> three_scalars() {
    return loom::SchemaBuilder("Whatever", 1)
        .field("id", loom::Kind::Text)
        .field("delay_ms", loom::Kind::Int)
        .field("repeat", loom::Kind::Bool)
        .build();
}

/// A SHAPE WITH NO FIELDS AT ALL.
std::shared_ptr<const loom::Schema> no_fields() {
    return loom::SchemaBuilder("Nothing", 1).build();
}

/// REQUIRED AND OPTIONAL SIDE BY SIDE.
std::shared_ptr<const loom::Schema> mixed() {
    return loom::SchemaBuilder("Mixed", 1)
        .field("must", loom::Kind::Text)
        .field("may", loom::Kind::Text, /*required=*/false)
        .field("flag", loom::Kind::Bool, /*required=*/false)
        .build();
}

std::shared_ptr<const loom::Schema> nested_leaf() {
    return loom::SchemaBuilder("Leaf", 1).field("n", loom::Kind::Int).build();
}

/// THE FOUR KINDS A FORM CANNOT AUTHOR, beside one it can: a nested message, a list,
/// a list of messages, and Bytes.
std::shared_ptr<const loom::Schema> structural() {
    return loom::SchemaBuilder("Structural", 1)
        .field("name", loom::Kind::Text)
        .message("leaf", nested_leaf())
        .list("counts", loom::type_of(loom::Kind::Int))
        .list("leaves", loom::type_message(nested_leaf()))
        .field("blob", loom::Kind::Bytes)
        .build();
}

/// A snapshot holding exactly these roots and an empty dependency Registry -- the
/// shape a decode produces for a target whose roots nest nothing.
cmp::Snapshot snapshot_of(std::vector<std::shared_ptr<const loom::Schema>> roots) {
    cmp::Snapshot s;
    s.deps = std::make_unique<loom::Registry>();
    s.roots = std::move(roots);
    return s;
}

cmp::Composing composing_form(std::shared_ptr<const loom::Schema> shape,
                              const char* role = "zengine.timer") {
    cmp::Composing c;
    c.stage = cmp::stage::kForm;
    c.library = "zengine-timer";
    c.role = role;
    c.snapshot = snapshot_of({shape});
    c.draft = cmp::begin_draft(shape);
    return c;
}

/// Author a value into a field the way typing does: present, with these bytes.
void write(cmp::MessageDraft& d, std::size_t which, const std::string& text) {
    d.fields[which].present = true;
    d.fields[which].value.set(text, text.size());
}

/// DOES THIS SOURCE KNOW THAT SHAPE -- as a BOOL, never as the shared_ptr itself.
///
/// A portability repair with a lane behind it: handing doctest a
/// `std::shared_ptr<const loom::Schema>` to compare against `nullptr` makes its
/// expression decomposer look for a way to print one, and MSVC's `<memory>` offers an
/// `operator<<` for `shared_ptr` that then fails to deduce -- a hard compile error on
/// that toolchain and silently fine on GCC. The question every case actually asks is a
/// yes or no, so it is spelled as one.
bool knows(const cmp::SnapshotSource& src, const char* name, std::uint32_t version) {
    return src.resolve_schema(name, version) != nullptr;
}

bool any_row(const cmp::ComposerView& v, const std::string& needle) {
    for (const cmp::RenderedRow& r : v.rows) {
        if (r.row.text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_SUITE("composer") {

// ---- Tier one: presence is two facts, and they are never one ----------------

TEST_CASE("MSG-0: a fresh draft has every field ABSENT, and no default is invented") {
    // A REQUIRED BOOL DOES NOT BEGIN AS `false`. `false` is a value a maker could
    // choose, and a form that started there would submit a choice nobody made the
    // first time anybody pressed Submit. Neither does a required Text begin as "".
    const cmp::MessageDraft d = cmp::begin_draft(three_scalars());
    REQUIRE(d.valid());
    REQUIRE(d.size() == 3);
    for (std::size_t i = 0; i < d.size(); ++i) {
        CAPTURE(i);
        CHECK_FALSE(d.fields[i].present);
        CHECK(d.fields[i].value.text().empty());
    }
    // ...and the draft's fields are the SCHEMA's fields, in declaration order.
    CHECK(d.field(0).name == "id");
    CHECK(d.field(1).name == "delay_ms");
    CHECK(d.field(2).name == "repeat");
}

TEST_CASE("MSG-0: an empty string is a VALUE, and it is not absence") {
    // §17's whole requirement, measured on the wire rather than in the model: a Text
    // field present with "" produces a field in the assembled Value; the same field
    // absent produces no field at all.
    const auto shape = mixed();
    const cmp::Snapshot snap = snapshot_of({shape});

    cmp::MessageDraft d = cmp::begin_draft(shape);
    write(d, 0, "x");
    write(d, 1, ""); // `may`, PRESENT and empty
    loom::Composition made = cmp::compose(snap, d);
    REQUIRE(made.status == loom::Composition::Status::Ready);
    loom::Value v = loom::assemble(made);
    REQUIRE(v.get("may") != nullptr);
    CHECK(v.get("may")->as_text() == "");

    // The same draft with `may` absent.
    d.fields[1].present = false;
    made = cmp::compose(snap, d);
    REQUIRE(made.status == loom::Composition::Status::Ready);
    v = loom::assemble(made);
    CHECK(v.get("may") == nullptr);
}

TEST_CASE("MSG-0: a Bool has three states while drafting, and `false` is one of them") {
    // unset -> false -> true -> unset. One gesture, and the three states are what
    // that gesture PRODUCES over a kind with two values -- there is no Bool-specific
    // rule beside the general one.
    const auto shape = mixed();
    const cmp::Snapshot snap = snapshot_of({shape});
    cmp::MessageDraft d = cmp::begin_draft(shape);
    write(d, 0, "x"); // the required field, so the draft can compose at all

    CHECK_FALSE(d.fields[2].present); // unset
    loom::Value v = loom::assemble(cmp::compose(snap, d));
    CHECK(v.get("flag") == nullptr); // ABSENT

    cmp::cycle(d.fields[2], loom::Kind::Bool);
    CHECK(d.fields[2].present);
    CHECK(d.fields[2].value.text() == "false");
    v = loom::assemble(cmp::compose(snap, d));
    REQUIRE(v.get("flag") != nullptr); // PRESENT(false) -- not the same message
    CHECK(v.get("flag")->as_bool() == false);

    cmp::cycle(d.fields[2], loom::Kind::Bool);
    CHECK(d.fields[2].value.text() == "true");
    v = loom::assemble(cmp::compose(snap, d));
    REQUIRE(v.get("flag") != nullptr);
    CHECK(v.get("flag")->as_bool() == true);

    cmp::cycle(d.fields[2], loom::Kind::Bool); // and back to unset
    CHECK_FALSE(d.fields[2].present);
    v = loom::assemble(cmp::compose(snap, d));
    CHECK(v.get("flag") == nullptr);
}

TEST_CASE("MSG-0: the presence gesture over a non-Bool keeps the bytes") {
    // Toggling a field out of the message and back in is a thing a person does while
    // deciding what to send. The bytes are the maker's work and nothing was said
    // about deleting them.
    cmp::MessageDraft d = cmp::begin_draft(mixed());
    write(d, 1, "hello");
    cmp::cycle(d.fields[1], loom::Kind::Text);
    CHECK_FALSE(d.fields[1].present);
    CHECK(d.fields[1].value.text() == "hello");
    cmp::cycle(d.fields[1], loom::Kind::Text);
    CHECK(d.fields[1].present);
    CHECK(d.fields[1].value.text() == "hello");
}

// ---- Tier one: what the ladder is handed, and what it refuses ---------------

TEST_CASE("MSG-0: every argument a form produces is NAMED") {
    // A form has a name for every value it holds, so `compose_message`'s ladder only
    // ever climbs its first rung -- and rung 1 is all-or-error, never a guess. The
    // three guessing rungs are unreachable from here BY CONSTRUCTION: there is no way
    // to build an unnamed `Arg` in draft.hpp.
    cmp::MessageDraft d = cmp::begin_draft(three_scalars());
    write(d, 0, "a");
    write(d, 1, "5");
    const std::vector<loom::Arg> args = cmp::args_of(d);
    REQUIRE(args.size() == 2);
    for (const loom::Arg& a : args) {
        REQUIRE(a.name.has_value());
    }
    CHECK(*args[0].name == "id");
    CHECK(*args[1].name == "delay_ms");
}

TEST_CASE("MSG-0: a Text field's bytes are TEXT, whatever they look like") {
    // THE ONE DECISION draft.hpp MAKES. `lex_value` infers a type FROM THE TOKEN
    // because a command line has nothing else to go on; a form knows the type from
    // the schema. Running the command line's rule over a form's bytes would refuse
    // `1000` for a Text field it is perfectly good for.
    const auto shape = mixed();
    const cmp::Snapshot snap = snapshot_of({shape});
    for (const char* bytes : {"1000", "true", "5.5", "$m1.count", "-7"}) {
        CAPTURE(bytes);
        cmp::MessageDraft d = cmp::begin_draft(shape);
        write(d, 0, bytes);
        const loom::Composition made = cmp::compose(snap, d);
        REQUIRE(made.status == loom::Composition::Status::Ready);
        const loom::Value v = loom::assemble(made);
        REQUIRE(v.get("must") != nullptr);
        CHECK(v.get("must")->as_text() == bytes); // exactly the bytes, as Text
    }
}

TEST_CASE("MSG-0: `1O00` is refused locally, by the ladder, naming the field and its kind") {
    // §44's witness, and the clean contrast this phase exists to draw. The letter O
    // is not a digit, so `lex_value` produces Text, and `place` refuses Text for an
    // Int field. Nothing in this repository knew that -- the sentence is Loom's.
    const auto shape = three_scalars();
    const cmp::Snapshot snap = snapshot_of({shape});
    cmp::MessageDraft d = cmp::begin_draft(shape);
    write(d, 0, "tick");
    write(d, 1, "1O00"); // letter O
    cmp::cycle(d.fields[2], loom::Kind::Bool);

    const loom::Composition made = cmp::compose(snap, d);
    REQUIRE(made.status == loom::Composition::Status::Error);
    CHECK(made.error.find("delay_ms") != std::string::npos);
    CHECK(made.error.find("Int") != std::string::npos);

    // ...and the same draft with the digit is Ready. The difference is one character.
    write(d, 1, "1000");
    const loom::Composition fixed = cmp::compose(snap, d);
    REQUIRE(fixed.status == loom::Composition::Status::Ready);
    CHECK(loom::assemble(fixed).get("delay_ms")->as_int() == 1000);
}

TEST_CASE("MSG-0: the other type refusals are the ladder's too, per kind") {
    const auto shape = three_scalars();
    const cmp::Snapshot snap = snapshot_of({shape});
    struct Case {
        const char* value;
        bool ready;
    };
    // An Int field takes an integer literal and nothing else: not a decimal, not a
    // bool word, not an empty string. `-500` IS a valid Int and this pane says
    // nothing about whether it is a sensible delay (§30).
    for (const Case c : {Case{"1000", true}, Case{"-500", true}, Case{"0", true},
                         Case{"1000.0", false}, Case{"true", false}, Case{"", false},
                         Case{"12ms", false}}) {
        CAPTURE(c.value);
        cmp::MessageDraft d = cmp::begin_draft(shape);
        write(d, 0, "tick");
        write(d, 1, c.value);
        cmp::cycle(d.fields[2], loom::Kind::Bool);
        CHECK((cmp::compose(snap, d).status == loom::Composition::Status::Ready) == c.ready);
    }
}

TEST_CASE("MSG-0: a required field left absent is NeedsInput, and it is named") {
    const auto shape = three_scalars();
    const cmp::Snapshot snap = snapshot_of({shape});
    cmp::MessageDraft d = cmp::begin_draft(shape);
    write(d, 0, "tick");
    write(d, 1, "1000");
    // `repeat` is a required Bool and nobody has chosen one.
    const loom::Composition made = cmp::compose(snap, d);
    REQUIRE(made.status == loom::Composition::Status::NeedsInput);
    bool named = false;
    for (const loom::FieldDesc& f : made.open_fields) {
        named = named || (f.name == "repeat" && f.required);
    }
    CHECK(named);
}

TEST_CASE("MSG-0: a shape with no fields is Ready immediately, and invents none") {
    // §48's empty-form case: the form has no rows a maker must fill, and nothing is
    // manufactured to give it one.
    const auto shape = no_fields();
    const cmp::MessageDraft d = cmp::begin_draft(shape);
    CHECK(d.size() == 0);
    const loom::Composition made = cmp::compose(snapshot_of({shape}), d);
    CHECK(made.status == loom::Composition::Status::Ready);
    CHECK(loom::assemble(made).schema().name() == "Nothing");
}

// ---- Tier one: what this Composer cannot author, and why ---------------------

TEST_CASE("MSG-0: composability has THREE answers, and they are three different facts") {
    CHECK(cmp::composability(loom::Kind::Int) == cmp::Composability::kScalar);
    CHECK(cmp::composability(loom::Kind::Float) == cmp::Composability::kScalar);
    CHECK(cmp::composability(loom::Kind::Text) == cmp::Composability::kScalar);
    CHECK(cmp::composability(loom::Kind::Bool) == cmp::Composability::kScalar);
    // BYTES: `place` would accept a `loom::Bytes` and no text surface in this Loom
    // can produce one -- `lex_value` has no Bytes branch at all. That is a missing
    // SPELLING, not a missing capability, and inventing hex or base64 here would be
    // inventing a wire convention in a UI.
    CHECK(cmp::composability(loom::Kind::Bytes) == cmp::Composability::kNoSpelling);
    // MESSAGE and LIST: the ladder's own bound. `place` refuses them outright.
    CHECK(cmp::composability(loom::Kind::Message) == cmp::Composability::kNotFlat);
    CHECK(cmp::composability(loom::Kind::List) == cmp::Composability::kNotFlat);
    // ...and only a scalar that is not a Bool takes typed characters.
    CHECK(cmp::typeable(loom::Kind::Text));
    CHECK_FALSE(cmp::typeable(loom::Kind::Bool));
    CHECK_FALSE(cmp::typeable(loom::Kind::Bytes));
}

TEST_CASE("MSG-0: a structural field is SHOWN, never authored, and blocks a send it is required for") {
    // §48's unsupported-structural case. The schema is visible, the field's own
    // structure is visible, the pane says it cannot compose it, there is no fake
    // scalar editor for it, and the message cannot be sent.
    const auto shape = structural();
    cmp::Composing c = composing_form(shape);
    const cmp::ComposerView v = cmp::project(c, 20, 60);

    CHECK(any_row(v, "leaf:Message(Leaf v1)"));
    CHECK(any_row(v, "counts:List<Int>"));
    CHECK(any_row(v, "leaves:List<Message(Leaf v1)>"));
    CHECK(any_row(v, "blob:Bytes"));
    CHECK(any_row(v, "(not composable in this version)"));
    CHECK(any_row(v, "(no text form -- not composable)"));

    // Even with every field a maker COULD author filled in, the draft is not ready.
    write(c.draft, 0, "a name");
    const loom::Composition made = cmp::compose(c.snapshot, c.draft);
    CHECK(made.status == loom::Composition::Status::NeedsInput);
    // ...and no argument was ever built for one of them.
    for (const loom::Arg& a : cmp::args_of(c.draft)) {
        REQUIRE(a.name.has_value());
        CHECK(*a.name == "name");
    }
}

// ---- Tier one: the snapshot owns its vocabulary -----------------------------

TEST_CASE("MSG-0: a snapshot resolves its ROOTS first and its dependencies second") {
    // MSG-1's distinction, kept alive all the way down to the send: `deps` is what a
    // root NEEDS and `roots` is what may be SENT.
    cmp::Snapshot s = snapshot_of({three_scalars()});
    const auto leaf = nested_leaf();
    s.deps->register_schema(leaf);
    const cmp::SnapshotSource src(s);

    CHECK(knows(src, "Whatever", 1));        // a root
    CHECK(knows(src, "Leaf", 1));            // a dependency
    CHECK_FALSE(knows(src, "Whatever", 2));  // a version nobody named
    CHECK_FALSE(knows(src, "Nothing", 1));   // a shape this target never mentioned
}

TEST_CASE("MSG-0: a reference is refused, and the refusal says why") {
    // `$m1.count` is the command line's way of wiring one received message's output
    // into another's input. This pane has received no messages, and `ComposeSource`
    // exists in two halves precisely so it can say so.
    const cmp::Snapshot s = snapshot_of({three_scalars()});
    const cmp::SnapshotSource src(s);
    std::string why;
    CHECK_FALSE(src.resolve_ref(loom::Ref{"m1", "count"}, &why).has_value());
    CHECK(why.find("$m1.count") != std::string::npos);
    CHECK(why.find("no received messages") != std::string::npos);
}

TEST_CASE("MSG-0: two snapshots never share a vocabulary") {
    // The append-only Registry this deliberately is not: replacing the snapshot
    // replaces the Registry, so a root of one target can never resolve a dependency
    // only another target declared.
    cmp::Snapshot a = snapshot_of({three_scalars()});
    a.deps->register_schema(nested_leaf());
    cmp::Snapshot b = snapshot_of({no_fields()});
    CHECK(knows(cmp::SnapshotSource(a), "Leaf", 1));
    CHECK_FALSE(knows(cmp::SnapshotSource(b), "Leaf", 1));
    CHECK_FALSE(knows(cmp::SnapshotSource(b), "Whatever", 1));
}

// ---- Tier two: what a maker sees --------------------------------------------

TEST_CASE("MSG-0: with no target the pane says so, and names no library") {
    cmp::Composing c;
    const cmp::ComposerView v = cmp::project(c, 8, 46);
    REQUIRE(v.rows.size() == 2);
    CHECK(v.rows[0].row.text == "no weave selected");
    CHECK(v.rows[1].row.text == "press a row in the Loaded pane");
    for (const cmp::RenderedRow& r : v.rows) {
        CHECK(r.meaning.what == cmp::meaning::kNothing);
    }
}

TEST_CASE("MSG-0: an empty role is an OBSERVED ABSENCE, and nothing is manufactured") {
    // §4. The library remains the diagnostic identity and the role remains the
    // messaging address; with no role there is nothing to address, and this pane does
    // not invent a WeaveId, address the library by name, or sweep for a participant.
    cmp::Composing c;
    c.stage = cmp::stage::kNoRole;
    c.library = "some-library";
    const cmp::ComposerView v = cmp::project(c, 8, 60);
    REQUIRE(v.rows.size() == 2);
    CHECK(v.rows[0].row.text == "selected some-library");
    CHECK(v.rows[1].row.text.find("no messaging role observed") != std::string::npos);
    CHECK_FALSE(any_row(v, "to @"));
}

TEST_CASE("MSG-0: while a discovery request is out, the pane says exactly what it knows") {
    // §6. NOT `loading...`: nothing observed promises an answer will come, there is
    // no timeout that could mean refused, and there is no spinner.
    cmp::Composing c;
    c.stage = cmp::stage::kAsking;
    c.library = "zengine-timer";
    c.role = "zengine.timer";
    const cmp::ComposerView v = cmp::project(c, 8, 60);
    CHECK(v.rows[0].row.text == "to @zengine.timer");
    CHECK(any_row(v, "asked what it accepts -- no answer observed yet"));
    CHECK_FALSE(any_row(v, "loading"));
}

TEST_CASE("MSG-0: the catalog shows (name, version) and never merges two versions") {
    // §13/§50. `Foo v1` and `Foo v2` are two message identities and this pane draws
    // no conclusion about either from the other -- no compatibility, no supersession,
    // no `latest`.
    cmp::Composing c;
    c.stage = cmp::stage::kCatalog;
    c.role = "zengine.example";
    c.snapshot = snapshot_of({loom::SchemaBuilder("Foo", 1).field("a", loom::Kind::Text).build(),
                              loom::SchemaBuilder("Foo", 2).field("b", loom::Kind::Int).build()});
    const cmp::ComposerView v = cmp::project(c, 10, 46);
    CHECK(any_row(v, "accepted messages -- 2"));
    CHECK(any_row(v, "Foo v1"));
    CHECK(any_row(v, "Foo v2"));

    // ...and each row names its own root, so choosing one cannot open the other.
    int messages = 0;
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(v.rows.size()); ++i) {
        const cmp::RowMeaning m = cmp::meaning_at_row(v, i);
        if (m.what == cmp::meaning::kMessage) {
            CHECK(v.rows[static_cast<std::size_t>(i)].row.text.find(
                      "Foo v" + std::to_string(m.which + 1)) != std::string::npos);
            ++messages;
        }
    }
    CHECK(messages == 2);
}

TEST_CASE("MSG-0: the catalog says ACCEPTED MESSAGES and filters nothing") {
    // §9. The word is load-bearing: an accept-set can hold commands, answers,
    // notifications, lifecycle vocabulary and substrate doors, and this pane knows
    // which of those any given root is -- it does not. So it hides none of them.
    cmp::Composing c;
    c.stage = cmp::stage::kCatalog;
    c.role = "zengine.example";
    c.snapshot = snapshot_of({loom::SchemaBuilder("zen.DescribeAccepted", 1).build(),
                              loom::SchemaBuilder("zen.PokeRead", 1).build(),
                              loom::SchemaBuilder("Ordinary", 1).build()});
    const cmp::ComposerView v = cmp::project(c, 12, 46);
    CHECK(any_row(v, "accepted messages -- 3"));
    CHECK(any_row(v, "zen.DescribeAccepted v1"));
    CHECK(any_row(v, "zen.PokeRead v1"));
    CHECK_FALSE(any_row(v, "Commands"));
    CHECK_FALSE(any_row(v, "Actions"));
    CHECK_FALSE(any_row(v, "Safe"));
}

TEST_CASE("MSG-0: the form is generated from the Schema, and nothing else") {
    // §15/§48. Three fields, in declaration order, each carrying the schema's own
    // type spelling. `Whatever v1` is a shape this binary and the Composer both first
    // met at run time.
    const cmp::Composing c = composing_form(three_scalars());
    const cmp::ComposerView v = cmp::project(c, 20, 60);
    CHECK(any_row(v, "Whatever v1 -> @zengine.timer"));
    CHECK(any_row(v, "id:Text"));
    CHECK(any_row(v, "delay_ms:Int"));
    CHECK(any_row(v, "repeat:Bool"));
    CHECK(any_row(v, "[ Submit ]"));
    CHECK(any_row(v, "[ Back ]"));
}

TEST_CASE("MSG-0: a field row says PRESENCE and VALUE separately") {
    // The brackets are what say PRESENT. An empty pair is a Text field a maker
    // deliberately set to the empty string, and it does not look like `(absent)`.
    cmp::Composing c = composing_form(mixed());
    c.cursor = 2; // off the two Text rows, so neither is drawn with a caret
    {
        const cmp::ComposerView v = cmp::project(c, 20, 60);
        CHECK(any_row(v, "must:Text  (required)"));
        CHECK(any_row(v, "may:Text  (absent)"));
        CHECK(any_row(v, "flag:Bool  (absent)"));
    }
    write(c.draft, 0, "hello");
    write(c.draft, 1, "");
    cmp::cycle(c.draft.fields[2], loom::Kind::Bool);
    {
        const cmp::ComposerView v = cmp::project(c, 20, 60);
        CHECK(any_row(v, "must:Text  [hello]"));
        CHECK(any_row(v, "may:Text  []")); // PRESENT and empty
        CHECK(any_row(v, "flag:Bool  [false]"));
        CHECK_FALSE(any_row(v, "(absent)"));
    }
}

TEST_CASE("MSG-0: a required field nobody has authored is in the ALERT role") {
    // It is the one thing standing between this draft and a send, so it is said in
    // characters first (`(required)`) and in ink second.
    cmp::Composing c = composing_form(three_scalars());
    c.cursor = 99; // no row is the cursor's, so the role is the field's own
    const cmp::ComposerView v = cmp::project(c, 20, 60);
    bool found = false;
    for (const cmp::RenderedRow& r : v.rows) {
        if (r.row.text.find("id:Text  (required)") != std::string::npos) {
            found = true;
            CHECK(r.row.role == surface::role::kAlert);
        }
    }
    CHECK(found);
}

TEST_CASE("MSG-0: the value being edited is WINDOWED and shows a caret; a resting one is FITTED") {
    // HD-6's rule, unchanged: `fit` marks what it cut because a committed value has
    // no caret to tell a maker it moved; a live one has.
    cmp::Composing c = composing_form(three_scalars());
    write(c.draft, 0, "abcdefghijklmnopqrstuvwxyz");
    c.cursor = 0;
    const std::int64_t room = cmp::value_capacity(c.draft, 0, 30);
    REQUIRE(room > 0);
    c.draft.fields[0].value.keep_caret_visible(room);
    const cmp::ComposerView editing = cmp::project(c, 20, 30);
    CHECK(any_row(editing, std::string(1, cmp::kCaret)));

    // The same field with the cursor elsewhere: no caret, and a marked cut.
    c.cursor = 1;
    const cmp::ComposerView resting = cmp::project(c, 20, 30);
    CHECK(any_row(resting, cmp::kElided));
}

TEST_CASE("MSG-0: every row of every projection fits the room it was granted") {
    // THE OBLIGATION THAT IS NOT A COURTESY. Workshop refuses an over-budget update
    // WHOLE, so a provider that miscounts by one row loses everything it said. The
    // sweep crosses four stages with every budget a pane can have and several it
    // cannot.
    std::vector<cmp::Composing> states;
    {
        cmp::Composing c;
        states.push_back(std::move(c));
    }
    {
        cmp::Composing c;
        c.stage = cmp::stage::kNoRole;
        c.library = "a-library-with-a-really-quite-long-name";
        states.push_back(std::move(c));
    }
    {
        cmp::Composing c;
        c.stage = cmp::stage::kAsking;
        c.library = "zengine-timer";
        c.role = "zengine.timer";
        c.notice = "SUBMITTED -- a sender is not told its fate";
        states.push_back(std::move(c));
    }
    {
        cmp::Composing c;
        c.stage = cmp::stage::kCatalog;
        c.library = "zengine-timer";
        c.role = "zengine.timer";
        std::vector<std::shared_ptr<const loom::Schema>> roots;
        for (int i = 0; i < 17; ++i) {
            roots.push_back(loom::SchemaBuilder("Shape" + std::to_string(i), 1).build());
        }
        c.snapshot = snapshot_of(std::move(roots));
        c.cursor = 9;
        c.notice = "still needed: repeat";
        states.push_back(std::move(c));
    }
    {
        cmp::Composing c = composing_form(structural());
        c.notice = "field 'delay_ms' (Int) cannot take this value";
        c.cursor = 3;
        states.push_back(std::move(c));
    }

    for (const cmp::Composing& c : states) {
        for (std::int64_t rows = 0; rows <= 14; ++rows) {
            for (const std::int64_t cols :
                 {std::int64_t{0}, std::int64_t{1}, std::int64_t{4}, std::int64_t{12},
                  std::int64_t{46}, std::int64_t{109}}) {
                CAPTURE(c.stage);
                CAPTURE(rows);
                CAPTURE(cols);
                const cmp::ComposerView v = cmp::project(c, rows, cols);
                REQUIRE(static_cast<std::int64_t>(v.rows.size()) <= rows);
                for (const cmp::RenderedRow& r : v.rows) {
                    REQUIRE(static_cast<std::int64_t>(r.row.text.size()) <= cols);
                    for (const char ch : r.row.text) {
                        // `SurfaceTextRow`'s plain-ASCII contract, which is the third
                        // rule Workshop judges an update by.
                        const unsigned char byte = static_cast<unsigned char>(ch);
                        REQUIRE(byte >= 0x20u);
                        REQUIRE(byte < 0x7Fu);
                    }
                    // A blank row is `role::kFill`, never `role::kNone` -- that value
                    // is the absence of a BACKGROUND and is not a Skin ink.
                    REQUIRE(r.row.role != surface::role::kNone);
                }
            }
        }
    }
}

TEST_CASE("MSG-0: nothing is hidden without being counted") {
    // §14. A windowed list says how much it left out, on ONE row that names both
    // sides -- and the count always adds up to the population.
    std::vector<std::shared_ptr<const loom::Schema>> roots;
    for (int i = 0; i < 17; ++i) {
        roots.push_back(loom::SchemaBuilder("Shape" + std::to_string(i), 1).build());
    }
    cmp::Composing c;
    c.stage = cmp::stage::kCatalog;
    c.role = "zengine.timer";
    c.snapshot = snapshot_of(std::move(roots));

    for (std::int64_t cursor = 0; cursor < 17; ++cursor) {
        for (std::int64_t rows = 2; rows <= 24; ++rows) {
            CAPTURE(cursor);
            CAPTURE(rows);
            c.cursor = cursor;
            const cmp::ComposerView v = cmp::project(c, rows, 46);
            std::int64_t shown = 0;
            bool cursor_visible = false;
            for (std::int64_t i = 0; i < static_cast<std::int64_t>(v.rows.size()); ++i) {
                const cmp::RowMeaning m = cmp::meaning_at_row(v, i);
                if (m.what == cmp::meaning::kMessage) {
                    ++shown;
                    cursor_visible = cursor_visible || m.which == cursor;
                }
            }
            // THE POPULATION IS ALWAYS STATED, at every budget that has a row for the
            // heading at all -- which is what makes a windowed list an honest sample
            // rather than a list. It is the floor of the accounting: a two-row pane
            // shows the target and the count and no roots, and nothing is hidden
            // without being counted even there.
            CHECK(any_row(v, "accepted messages -- 17"));
            // ...and where PART of the list is on screen, the marker says how much is
            // not, on both sides.
            if (shown > 0 && shown < 17) {
                CHECK(any_row(v, cmp::kElided));
            }
            // ...and the focused item is always inside the window when any item is.
            if (shown > 0) {
                CHECK(cursor_visible);
            }
        }
    }
}

TEST_CASE("MSG-0: the omission row names both sides when both are hidden") {
    std::vector<std::shared_ptr<const loom::Schema>> roots;
    for (int i = 0; i < 20; ++i) {
        roots.push_back(loom::SchemaBuilder("Shape" + std::to_string(i), 1).build());
    }
    cmp::Composing c;
    c.stage = cmp::stage::kCatalog;
    c.role = "zengine.timer";
    c.snapshot = snapshot_of(std::move(roots));
    c.cursor = 10;
    const cmp::ComposerView v = cmp::project(c, 8, 46);
    bool both = false;
    for (const cmp::RenderedRow& r : v.rows) {
        both = both || (r.row.text.find(" above, ") != std::string::npos &&
                        r.row.text.find(" below") != std::string::npos);
    }
    CHECK(both);
}

TEST_CASE("MSG-0: the window is total, and keeps the focus, over every budget") {
    for (std::int64_t population = 0; population <= 30; ++population) {
        for (std::int64_t rows = 0; rows <= 12; ++rows) {
            for (std::int64_t focus = 0; focus < (population > 0 ? population : 1); ++focus) {
                CAPTURE(population);
                CAPTURE(rows);
                CAPTURE(focus);
                const cmp::Window w = cmp::window_of(population, focus, rows);
                REQUIRE(w.first >= 0);
                REQUIRE(w.count >= 0);
                REQUIRE(w.first + w.count <= population);
                REQUIRE(w.before == w.first);
                REQUIRE(w.before + w.count + w.after == population);
                // Every row it claims is a row it was given -- items plus at most one
                // omission marker. At a zero-row budget it claims nothing and still
                // accounts for the whole population, which is the one case where the
                // caller has no row to say the marker in.
                const std::int64_t marker = (w.before > 0 || w.after > 0) ? 1 : 0;
                REQUIRE(w.count + (rows > 0 ? marker : 0) <= rows);
                // The focused item is inside the window whenever anything is shown.
                if (w.count > 0 && population > 0) {
                    REQUIRE(focus >= w.first);
                    REQUIRE(focus < w.first + w.count);
                }
                // A population that fits is shown whole, and says nothing about
                // omissions it does not have.
                if (population <= rows) {
                    REQUIRE(w.count == population);
                    REQUIRE(marker == 0);
                }
            }
        }
    }
}

TEST_CASE("MSG-0: the controls are anchored to the FOOT and do not move with the fields") {
    // HD-8's argument in a second place, including its reason: a control that moves
    // under the hand aiming at it is worse than an empty strip above it.
    const auto shape = loom::SchemaBuilder("Wide", 1)
                           .field("a", loom::Kind::Text)
                           .field("b", loom::Kind::Text)
                           .field("c", loom::Kind::Text)
                           .build();
    cmp::Composing c = composing_form(shape);
    for (const std::int64_t rows : {std::int64_t{7}, std::int64_t{9}, std::int64_t{20}}) {
        CAPTURE(rows);
        const cmp::ComposerView v = cmp::project(c, rows, 46);
        REQUIRE(v.rows.size() >= 2);
        const std::size_t n = v.rows.size();
        CHECK(v.rows[n - 2].meaning.what == cmp::meaning::kSubmit);
        CHECK(v.rows[n - 1].meaning.what == cmp::meaning::kBack);
    }
}

TEST_CASE("MSG-0: a row's meaning is the meaning of the row a maker sees") {
    // §54/§55's whole point: the map from row to item is built by the function that
    // draws the rows, so a press cannot name a different item from the one under the
    // hand. Read back over the projection itself.
    cmp::Composing c = composing_form(mixed());
    c.cursor = 1;
    const cmp::ComposerView v = cmp::project(c, 20, 60);
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(v.rows.size()); ++i) {
        const cmp::RowMeaning m = cmp::meaning_at_row(v, i);
        const std::string& text = v.rows[static_cast<std::size_t>(i)].row.text;
        CAPTURE(i);
        CAPTURE(text);
        switch (m.what) {
        case cmp::meaning::kField:
            REQUIRE(m.which >= 0);
            REQUIRE(m.which < static_cast<std::int64_t>(c.draft.size()));
            CHECK(text.find(c.draft.field(static_cast<std::size_t>(m.which)).name) !=
                  std::string::npos);
            break;
        case cmp::meaning::kSubmit: CHECK(text.find("[ Submit ]") != std::string::npos); break;
        case cmp::meaning::kBack: CHECK(text.find("[ Back ]") != std::string::npos); break;
        case cmp::meaning::kMessage: FAIL("a form has no message rows"); break;
        default: break;
        }
    }
    // ...and a row outside the view means nothing, in both directions.
    CHECK(cmp::meaning_at_row(v, -1).what == cmp::meaning::kNothing);
    CHECK(cmp::meaning_at_row(v, 9999).what == cmp::meaning::kNothing);
}

TEST_CASE("MSG-0: the value's room is one answer, spent by the painter and by the window") {
    // HD-4 paid for learning that a second copy of a window's capacity is right until
    // the first value long enough to scroll. `value_capacity` is that one answer, and
    // it never goes negative however narrow the pane.
    const auto shape = three_scalars();
    const cmp::MessageDraft d = cmp::begin_draft(shape);
    for (std::int64_t cols = 0; cols <= 80; ++cols) {
        for (std::size_t which = 0; which < d.size(); ++which) {
            CAPTURE(cols);
            CAPTURE(which);
            REQUIRE(cmp::value_capacity(d, which, cols) >= 0);
            REQUIRE(cmp::value_capacity(d, which, cols) <= cols);
        }
    }
    // ...and a wider pane never gives a value less room.
    for (std::int64_t cols = 1; cols <= 80; ++cols) {
        REQUIRE(cmp::value_capacity(d, 0, cols) >= cmp::value_capacity(d, 0, cols - 1));
    }
}

TEST_CASE("MSG-0: the two identities are shown apart -- the office addressed, the library pressed") {
    // §5. `send_to_role` addresses the OFFICE at delivery; the library name is
    // diagnostic and addresses nothing. A pane that showed only one of them would
    // invite a maker to believe a send is pinned to the incarnation whose row they
    // pressed.
    cmp::Composing c;
    c.stage = cmp::stage::kCatalog;
    c.library = "zengine-timer";
    c.role = "zengine.timer";
    c.snapshot = snapshot_of({no_fields()});
    const cmp::ComposerView v = cmp::project(c, 12, 46);
    CHECK(v.rows[0].row.text == "to @zengine.timer");
    CHECK(any_row(v, "from zengine-timer"));
}

TEST_CASE("MSG-0: the notice outranks the list, because a refusal nobody can see is worse") {
    cmp::Composing c;
    c.stage = cmp::stage::kCatalog;
    c.role = "zengine.timer";
    std::vector<std::shared_ptr<const loom::Schema>> roots;
    for (int i = 0; i < 12; ++i) {
        roots.push_back(loom::SchemaBuilder("Shape" + std::to_string(i), 1).build());
    }
    c.snapshot = snapshot_of(std::move(roots));
    c.notice = "field 'delay_ms' (Int) cannot take this value";
    c.notice_role = surface::role::kAlert;
    for (std::int64_t rows = 3; rows <= 10; ++rows) {
        CAPTURE(rows);
        const cmp::ComposerView v = cmp::project(c, rows, 60);
        CHECK(any_row(v, "cannot take this value"));
    }
}

TEST_CASE("MSG-0: the pane's durable names are what a saved setup would hold") {
    // A `PaneRef` is a promise to a maker's file: it survives this build, this
    // incarnation and this load order.
    CHECK(std::string(cmp::kComposerRole) == "zengine.composer");
    CHECK(std::string(cmp::kComposePane) == "compose");
    CHECK(std::string(cmp::kComposerStem) == "zengine-composer");
    // ...and the picker's name column is ten cells, so a name longer than that would
    // reach a maker's eye already cut (INTR-0's lesson, paid rather than rediscovered).
    CHECK(std::string(cmp::kComposePaneName).size() <= 10u);
    CHECK(std::string(cmp::kComposePaneSummary).size() <= 64u);
}

TEST_CASE("MSG-0: the mark and the caret are spelled here, once, as characters") {
    // THE CANARY THIS SUITE WOULD OTHERWISE NOT HAVE. Every other case in this file
    // says `cmp::kSelectedMark`, which is right -- a magic string in twenty places is
    // how two spellings drift -- and a suite in which EVERY reference is the constant
    // cannot notice the constant changing. One case spells it.
    CHECK(std::string(cmp::kSelectedMark) == "> ");
    CHECK(std::string(cmp::kUnselectedMark) == "  ");
    CHECK(std::string(cmp::kElided) == "...");
    CHECK(cmp::kCaret == '_');
    // ...and marking a row costs no columns, so a list cannot start cutting names
    // because something in it became selected.
    CHECK(std::string(cmp::kSelectedMark).size() == std::string(cmp::kUnselectedMark).size());
}

} // TEST_SUITE
