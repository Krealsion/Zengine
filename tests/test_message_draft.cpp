// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#include "doctest.h"
#include "message-draft/transfer.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
namespace md = zengine::message_draft;
std::shared_ptr<const loom::Schema> leaf() {
    return loom::SchemaBuilder("drafttest.Leaf", 1).field("count", loom::Kind::Int)
        .field("caption", loom::Kind::Text, false).build();
}
std::shared_ptr<const loom::Schema> shape() {
    return loom::SchemaBuilder("drafttest.Input", 3).field("enabled", loom::Kind::Bool)
        .field("label", loom::Kind::Text, false).message("child", leaf(), false)
        .list("samples", loom::type_message(leaf()), false)
        .list("matrix", loom::type_list(loom::type_of(loom::Kind::Int)), false)
        .field("amount", loom::Kind::Float, false).field("raw", loom::Kind::Bytes, false).build();
}
struct TempFile {
    std::filesystem::path directory;
    TempFile() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory = std::filesystem::temp_directory_path() / ("zengine-message-draft-" + std::to_string(stamp));
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("temporary directory collision");
    }
    ~TempFile() { std::error_code error; std::filesystem::remove_all(directory, error); }
    std::string path() const { return (directory / "presets.zen").string(); }
};
}

TEST_CASE("message drafts distinguish absent, false and empty without defaulting choices") {
    md::Draft draft(shape());
    CHECK_FALSE(draft.ready());
    CHECK(draft.get({std::string("enabled")}) == nullptr);
    draft.set_text({std::string("enabled")}, "false");
    draft.set_text({std::string("label")}, "");
    REQUIRE(draft.ready());
    REQUIRE(draft.get({std::string("enabled")}));
    CHECK_FALSE(draft.get({std::string("enabled")})->as_bool());
    REQUIRE(draft.get({std::string("label")}));
    CHECK(draft.get({std::string("label")})->as_text().empty());
    draft.unset({std::string("label")});
    CHECK(draft.get({std::string("label")}) == nullptr);
    draft.unset({std::string("enabled")});
    const auto errors = draft.errors();
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().kind == loom::ErrorKind::MissingField);
    CHECK(errors.front().path == "enabled");
}

TEST_CASE("message scalar editing uses Loom composition and refused edits are atomic") {
    md::Draft draft(shape());
    draft.set_text({std::string("label")}, "123");
    draft.set_text({std::string("amount")}, "17");
    CHECK(draft.get({std::string("label")})->as_text() == "123");
    CHECK(draft.get({std::string("amount")})->as_float() == 17.0);
    const auto before = loom::compat::serialize(draft.value());
    CHECK_THROWS_AS(draft.set_text({std::string("enabled")}, "0"), std::invalid_argument);
    CHECK_THROWS_AS(draft.set_text({std::string("amount")}, "$m1.answer"), std::invalid_argument);
    CHECK_THROWS_AS(draft.set_text({std::string("raw")}, "00ff"), std::invalid_argument);
    CHECK_THROWS_AS(draft.set({std::string("enabled")}, loom::Cell::text("true")), std::invalid_argument);
    CHECK(loom::compat::serialize(draft.value()) == before);
    draft.set({std::string("raw")}, loom::Cell::bytes({0, 255, 17}));
    CHECK(draft.get({std::string("raw")})->as_bytes() == loom::Bytes{0, 255, 17});
}

TEST_CASE("nested message and list forms retain incomplete fields and address exact names") {
    md::Draft draft(shape());
    CHECK_THROWS_AS(draft.set_text({std::string("child"), std::string("count")}, "4"), std::invalid_argument);
    draft.create_message({std::string("child")});
    draft.set_text({std::string("child"), std::string("caption")}, "partial");
    draft.create_list({std::string("samples")});
    draft.append({std::string("samples")}, loom::Cell::message(loom::Value(leaf())));
    draft.set_text({std::string("samples"), std::size_t(0), std::string("count")}, "29");
    draft.create_list({std::string("matrix")});
    draft.append({std::string("matrix")}, loom::Cell::list({loom::Cell::integer(3)}));
    draft.set_text({std::string("matrix"), std::size_t(0), std::size_t(0)}, "47");
    CHECK(draft.get({std::string("matrix"), std::size_t(0), std::size_t(0)})->as_int() == 47);
    const auto rows = draft.rows();
    const auto found = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return row.label == "samples[0].count";
    });
    REQUIRE(found != rows.end());
    CHECK(found->summary == "29");
    CHECK(found->present);
    CHECK(found->required);
    CHECK_THROWS_AS(draft.unset({std::string("samples"), std::size_t(0)}), std::invalid_argument);
    draft.erase({std::string("samples")}, 0);
    CHECK(draft.get({std::string("samples")})->as_list().empty());
    const auto punctuation = loom::SchemaBuilder("drafttest.Punctuation", 1)
        .field("a.b[0]", loom::Kind::Int).build();
    md::Draft odd(punctuation);
    odd.set_text({std::string("a.b[0]")}, "91");
    CHECK(odd.get({std::string("a.b[0]")})->as_int() == 91);
}

TEST_CASE("draft construction, copying and snapshots do not share mutable nested data") {
    loom::Value nested(leaf());
    nested.set("count", loom::Cell::integer(5));
    loom::Value source(shape());
    source.set("child", loom::Cell::message(nested));
    md::Draft first(source);
    md::Draft second(first);
    source.get("child")->as_message()->set("count", loom::Cell::integer(101));
    first.set_text({std::string("child"), std::string("count")}, "13");
    auto snapshot = second.snapshot();
    snapshot.get("child")->as_message()->set("count", loom::Cell::integer(202));
    CHECK(first.get({std::string("child"), std::string("count")})->as_int() == 13);
    CHECK(second.get({std::string("child"), std::string("count")})->as_int() == 5);
    auto wrong = loom::Value(shape());
    wrong.set("child", loom::Cell::message(loom::Value(loom::make_schema("other", 1, {}))));
    CHECK_THROWS_AS(md::Draft{wrong}, std::invalid_argument);
}

TEST_CASE("named libraries reopen complete schema closure and unfinished nested drafts") {
    md::Draft draft(shape());
    draft.create_message({std::string("child")});
    draft.set_text({std::string("child"), std::string("caption")}, "still editing");
    draft.create_list({std::string("samples")});
    draft.append({std::string("samples")}, loom::Cell::message(loom::Value(leaf())));
    draft.set({std::string("raw")}, loom::Cell::bytes({0, 1, 255}));
    md::Library library;
    library.put("unfinished", draft);
    library.put("blank schema", md::Draft(shape()));
    auto loaded = md::read_library(md::library_bytes(library));
    REQUIRE(loaded.entries().size() == 2);
    auto* reopened = loaded.find("unfinished");
    REQUIRE(reopened);
    CHECK(loom::same_identity(*reopened->schema(), *shape()));
    CHECK(reopened->get({std::string("child"), std::string("caption")})->as_text() == "still editing");
    CHECK(reopened->get({std::string("samples"), std::size_t(0), std::string("count")}) == nullptr);
    CHECK(reopened->get({std::string("raw")})->as_bytes() == loom::Bytes{0, 1, 255});
    CHECK_FALSE(reopened->admit());
    reopened->set_text({std::string("enabled")}, "true");
    reopened->set_text({std::string("child"), std::string("count")}, "7");
    reopened->set_text({std::string("samples"), std::size_t(0), std::string("count")}, "19");
    CHECK(reopened->admit());
    CHECK_FALSE(library.find("unfinished")->ready());
    CHECK(loaded.erase("blank schema"));
    CHECK_FALSE(loaded.erase("not here"));
}

TEST_CASE("library vocabulary conflicts are rejected without changing current knowledge") {
    md::Library library;
    library.put("example", md::Draft(shape()));
    const auto bytes = md::library_bytes(library);
    const auto conflict = loom::SchemaBuilder("drafttest.Leaf", 1).field("renamed", loom::Kind::Bool).build();
    loom::Registry current;
    current.register_schema(conflict);
    CHECK_THROWS_AS(md::read_library(bytes, &current), loom::SchemaConflict);
    CHECK(current.size() == 1);
    CHECK(loom::same_identity(*current.lookup("drafttest.Leaf", 1), *conflict));
    CHECK_THROWS_AS(library.put("conflict", md::Draft(conflict)), loom::SchemaConflict);
    CHECK(library.find("conflict") == nullptr);
    CHECK(md::library_bytes(library) == bytes);
    md::Draft incompatible(conflict);
    CHECK_FALSE(incompatible.admit(*leaf()));
}

TEST_CASE("corrupt library envelopes, embedded drafts and repeated names are refused") {
    md::Library library;
    library.put("example", md::Draft(shape()));
    const auto bytes = md::library_bytes(library);
    for (std::size_t cut : {std::size_t(0), std::size_t(1), bytes.size() / 2, bytes.size() - 1})
        CHECK_THROWS_AS(md::read_library(std::string_view(bytes).substr(0, cut)), std::invalid_argument);
    auto admitted = loom::admit(loom::parse(bytes), md::library_schema());
    REQUIRE(admitted);
    auto envelope = std::move(admitted).value();
    auto entries = envelope.get("presets")->as_list();
    entries[0].as_message()->set("draft", loom::Cell::bytes({1, 2, 3}));
    envelope.set("presets", loom::Cell::list(entries));
    CHECK_THROWS_AS(md::read_library(loom::serialize(envelope)), std::invalid_argument);

    library.put("another", md::Draft(shape()));
    auto two = loom::admit(loom::parse(md::library_bytes(library)), md::library_schema());
    REQUIRE(two);
    auto repeated = std::move(two).value();
    auto presets = repeated.get("presets")->as_list();
    REQUIRE(presets.size() == 2);
    presets[1].as_message()->set("title", loom::Cell::text("example"));
    repeated.set("presets", loom::Cell::list(presets));
    CHECK_THROWS_AS(md::read_library(loom::serialize(repeated)), std::invalid_argument);
    CHECK_THROWS_AS(md::read_library(std::string(zengine::maker::kMaxFileBytes + 1, 'x')), std::invalid_argument);
}

TEST_CASE("library file replacement preserves prior work when validation or replacement fails") {
    TempFile temp;
    md::Library library;
    md::Draft draft(shape());
    draft.set_text({std::string("label")}, "first");
    library.put("saved", draft);
    md::save_library(temp.path(), library);
    draft.set_text({std::string("label")}, "second");
    library.put("saved", draft);
    md::save_library(temp.path(), library);
    auto reopened = md::open_library(temp.path());
    REQUIRE(reopened.find("saved"));
    CHECK(reopened.find("saved")->get({std::string("label")})->as_text() == "second");
    draft.set_text({std::string("label")}, std::string(zengine::maker::kMaxFileBytes, 'q'));
    library.put("oversized", draft);
    CHECK_THROWS_AS(md::save_library(temp.path(), library), std::invalid_argument);
    reopened = md::open_library(temp.path());
    CHECK(reopened.find("oversized") == nullptr);
    CHECK(reopened.find("saved")->get({std::string("label")})->as_text() == "second");
    const auto target_directory = temp.directory / "target-is-a-directory";
    REQUIRE(std::filesystem::create_directory(target_directory));
    CHECK_THROWS_AS(md::save_library(target_directory.string(), reopened), std::runtime_error);
    CHECK(std::filesystem::is_directory(target_directory));
}

TEST_CASE("transfer envelopes retain missing required values without relaxing the runtime shape") {
    md::Draft draft(shape());
    draft.set_text({"enabled"}, "false"); draft.set_text({"label"}, "");
    const auto complete = draft.snapshot();
    draft.unset({"enabled"});
    auto reopened = md::read_draft(md::store_draft("my preset", draft));
    CHECK(reopened.title == "my preset");
    CHECK(reopened.draft.get({"enabled"}) == nullptr);
    REQUIRE(reopened.draft.get({"label"}));
    CHECK(reopened.draft.get({"label"})->as_text().empty());
    CHECK_FALSE(reopened.draft.admit());
    CHECK(reopened.draft.schema()->find("enabled")->required);
    reopened.draft.set_text({"enabled"}, "true");
    CHECK_FALSE(complete.get("enabled")->as_bool());
    CHECK(draft.get({"enabled"}) == nullptr);
    auto invalid = md::store_draft("my preset", draft);
    invalid.set("library", loom::Cell::bytes({1, 2, 3}));
    CHECK_THROWS(md::read_draft(invalid));
    md::Library two; two.put("a", draft); two.put("b", draft);
    invalid.set("library", md::detail::byte_cell(md::library_bytes(two)));
    CHECK_THROWS(md::read_draft(invalid));
}

TEST_CASE("field acquisition copies only the selected branch and keeps structural names and types") {
    const auto punctuation = loom::SchemaBuilder("drafttest.PunctuationField", 1)
        .field("a.b[0]", loom::Kind::Int).field("secret", loom::Kind::Text).build();
    const auto root = loom::SchemaBuilder("drafttest.FieldRoot", 1)
        .list("items", loom::type_message(punctuation)).build();
    md::Draft draft(root);
    draft.create_list({"items"});
    for (int n : {12, 29}) {
        loom::Value v(punctuation); v.set("a.b[0]", loom::Cell::integer(n));
        v.set("secret", loom::Cell::text("unrelated confidential sibling"));
        draft.append({"items"}, loom::Cell::message(v));
    }
    const auto envelope = md::grab_field(draft, {"items", std::size_t(1), "a.b[0]"});
    const auto bytes = envelope.get("library")->as_bytes();
    CHECK(std::string(bytes.begin(), bytes.end()).find("unrelated confidential sibling") == std::string::npos);
    const auto field = md::read_field(envelope);
    CHECK(field.type.kind == loom::Kind::Int); CHECK(field.cell.as_int() == 29);
    draft.set_text({"items", std::size_t(1), "a.b[0]"}, "99");
    CHECK(md::read_field(envelope).cell.as_int() == 29);
    CHECK_THROWS(md::grab_field(draft, {"missing"}));
    auto bad = envelope;
    loom::Value step(md::path_step_schema());
    step.set("field", loom::Cell::text("items")); step.set("index", loom::Cell::integer(0));
    bad.set("path", loom::Cell::list({loom::Cell::message(step)}));
    CHECK_THROWS(md::read_field(bad));
}

TEST_CASE("field acquisition preserves empty kinds and checks even an empty list's declared closure") {
    md::Draft draft(shape());
    draft.set_text({"enabled"}, "false"); draft.set_text({"label"}, ""); draft.create_list({"samples"});
    CHECK_FALSE(md::read_field(md::grab_field(draft, {"enabled"})).cell.as_bool());
    CHECK(md::read_field(md::grab_field(draft, {"label"})).cell.as_text().empty());
    auto field = md::read_field(md::grab_field(draft, {"samples"}));
    CHECK(field.cell.as_list().empty());
    CHECK_NOTHROW(md::require_type(loom::type_list(loom::type_message(leaf())), field.type));
    CHECK_THROWS(md::require_type(loom::type_list(loom::type_of(loom::Kind::Int)), field.type));
    const auto changed = loom::SchemaBuilder("drafttest.Leaf", 1).field("count", loom::Kind::Text).build();
    CHECK_THROWS(md::require_type(loom::type_list(loom::type_message(changed)), field.type));
    loom::Registry registry; registry.register_schema(changed);
    CHECK_THROWS(md::read_draft(md::store_draft("empty", draft), &registry));
}
