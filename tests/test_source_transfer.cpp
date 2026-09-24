// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Suite `source_transfer` -- the material editors exchange with Inventory, as pure functions: the
// two byte laws text meets on the way into an editor, the Terminal line a command becomes (proved
// by Loom's own lexer), what a dropped pair means, the pairs a capture makes, and the C++ a
// command may become in a C++ document. No weave runs here; the editors' suites drive the same
// functions through real panes.

#include "doctest.h"

#include "source-transfer/command_line.hpp"
#include "source-transfer/cpp.hpp"
#include "source-transfer/material.hpp"
#include "source-transfer/text.hpp"
#include "source-transfer/vocabulary.hpp"

#include "inventory/codec.hpp"
#include "message-draft/transfer.hpp"
#include "timer/vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include "source_transfer_samples.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {

namespace st = zengine::source_transfer;
namespace md = zengine::message_draft;

std::string pair_of(const loom::Value& item, const std::vector<loom::Value>& meta = {}) {
    return zengine::inventory::encode_pair(item, meta);
}

/// A flat command with one field of every scalar kind, and one optional field.
std::shared_ptr<const loom::Schema> sample_schema() {
    static const auto s = loom::SchemaBuilder("EditorMaterialsSample", 1)
                               .field("name", loom::Kind::Text)
                               .field("count", loom::Kind::Int)
                               .field("ratio", loom::Kind::Float)
                               .field("on", loom::Kind::Bool)
                               .field("note", loom::Kind::Text, false)
                               .build();
    return s;
}

loom::Value sample(std::string name, std::int64_t count, double ratio, bool on) {
    loom::Value v(sample_schema());
    v.set("name", loom::Cell::text(std::move(name)));
    v.set("count", loom::Cell::integer(count));
    v.set("ratio", loom::Cell::real(ratio));
    v.set("on", loom::Cell::boolean(on));
    return v;
}

zengine::timer::EnsureTimer beat() {
    zengine::timer::EnsureTimer t;
    t.id = "editor-materials.beat";
    t.delay_ms = 250;
    t.repeat = true;
    t.preferred = "keep-remaining";
    t.fallback = "restart";
    return t;
}

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST_SUITE("source_transfer") {

TEST_CASE("the standard Editor's law splits LF and CRLF, keeps tabs, and refuses what it cannot hold whole") {
    const st::Lines ok = st::standard_lines("a\tb\r\nc\n\nd\n");
    REQUIRE(ok.ok);
    CHECK(ok.lines == std::vector<std::string>{"a\tb", "c", "", "d", ""});
    CHECK(st::standard_lines("").lines == std::vector<std::string>{""});
    const st::Lines cr = st::standard_lines("one\ntwo\rthree");
    CHECK_FALSE(cr.ok);
    CHECK(cr.lines.empty());
    CHECK(cr.refusal.find("line 2") != std::string::npos);
    CHECK(cr.refusal.find("carriage return") != std::string::npos);
    const st::Lines esc = st::standard_lines("fine\nbad \x1b[31m");
    CHECK_FALSE(esc.ok);
    CHECK(esc.refusal.find("control byte (0x1b)") != std::string::npos);
    const st::Lines utf = st::standard_lines("caf\xc3\xa9");
    CHECK_FALSE(utf.ok);
    CHECK(utf.refusal.find("outside plain ASCII (0xc3)") != std::string::npos);
}

TEST_CASE("Neovim's law holds any UTF-8 as data -- Escape, key notation and a bare CR stay characters -- and refuses NUL and broken UTF-8") {
    const st::Lines ok = st::neovim_lines("\x1b:q!<Esc>\r\ncaf\xc3\xa9 \x0d end\n");
    REQUIRE(ok.ok);
    CHECK(ok.lines == std::vector<std::string>{"\x1b:q!<Esc>", "caf\xc3\xa9 \x0d end", ""});
    const st::Lines nul = st::neovim_lines(std::string("a\nb\0c", 5));
    CHECK_FALSE(nul.ok);
    CHECK(nul.refusal.find("line 2 holds a NUL") != std::string::npos);
    CHECK_FALSE(st::neovim_lines("bad \xc3").ok);
    CHECK_FALSE(st::neovim_lines("\xed\xa0\x80").ok); // a UTF-16 surrogate is not a character
    CHECK(st::join_lf(ok.lines) == "\x1b:q!<Esc>\ncaf\xc3\xa9 \x0d end\n");
}

TEST_CASE("a command becomes the Terminal's line: present fields in order, text quoted, and Loom's lexer reads every field back exactly") {
    const loom::Value v = sample("42", -7, 0.1, true);
    const st::TerminalLine t = st::terminal_line(v, "@zengine.timer");
    REQUIRE_MESSAGE(t.ok, t.refusal);
    CHECK(t.address_supplied);
    CHECK(t.missing.empty());
    CHECK(t.line == "send @zengine.timer EditorMaterialsSample 1 name=\"42\" count=-7 ratio=0.1 on=true");
    // A text that looks like a number, a boolean or a reference stays text, and the others keep kind.
    for (const char* tricky : {"true", "5", "$m1.count", "a=b", "", "two words"}) {
        CAPTURE(tricky);
        const st::TerminalLine q = st::terminal_line(sample(tricky, 0, 5.0, false), "");
        REQUIRE_MESSAGE(q.ok, q.refusal);
        CHECK(q.line.rfind("send <address> EditorMaterialsSample 1 name=\"", 0) == 0);
    }
    CHECK(st::terminal_line(sample("x", 0, 5.0, false), "").line.find("ratio=5.0 ") != std::string::npos);
    CHECK(st::terminal_line(sample("x", 0, 1e20, false), "").line.find("ratio=1.0e+20 ") != std::string::npos);
    CHECK(st::terminal_line(sample("x", 0, -0.0, false), "").line.find("ratio=-0.0 ") != std::string::npos);
    const st::TerminalLine timer = st::terminal_line(loom::to_value(beat()), "@zengine.timer");
    REQUIRE(timer.ok);
    CHECK(timer.line == "send @zengine.timer EnsureTimer 1 id=\"editor-materials.beat\" delay_ms=250 repeat=true "
                        "preferred=\"keep-remaining\" fallback=\"restart\"");
}

TEST_CASE("a value the Terminal grammar cannot spell is refused whole, and a missing required field is left missing, never filled") {
    CHECK(st::terminal_line(sample("say \"hi\"", 1, 1.0, true), "").refusal.find("double quote") != std::string::npos);
    CHECK(st::terminal_line(sample("two\nlines", 1, 1.0, true), "").refusal.find("line break") != std::string::npos);
    CHECK(st::terminal_line(sample("x", 1, std::nan(""), true), "").refusal.find("non-finite") != std::string::npos);
    const auto bytes = loom::SchemaBuilder("Blob", 1).field("data", loom::Kind::Bytes).build();
    loom::Value b(bytes);
    b.set("data", loom::Cell::bytes(loom::Bytes{1, 2}));
    CHECK(st::terminal_line(b, "").refusal.find("bytes") != std::string::npos);
    const auto listed = loom::SchemaBuilder("Listed", 1).list("rows", loom::type_of(loom::Kind::Int)).build();
    loom::Value l(listed);
    l.set("rows", loom::Cell::list({loom::Cell::integer(1)}));
    CHECK(st::terminal_line(l, "").refusal.find("list") != std::string::npos);
    // An address that is not one word is refused before the line is written.
    CHECK(st::terminal_line(sample("x", 1, 1.0, true), "two words").refusal.find("not one word") != std::string::npos);
    // An unfinished preset: `count` and `on` were never authored.
    loom::Value partial(sample_schema());
    partial.set("name", loom::Cell::text("n"));
    partial.set("ratio", loom::Cell::real(2.5));
    const st::TerminalLine p = st::terminal_line(partial, "");
    REQUIRE(p.ok);
    CHECK(p.missing == std::vector<std::string>{"count", "on"});
    CHECK(p.line == "send <address> EditorMaterialsSample 1 name=\"n\" ratio=2.5");
    CHECK_FALSE(p.address_supplied);
}

TEST_CASE("a dropped pair is read as text, a location, a command with the address its capture supplied, or refused in words") {
    // Text, and an Info FieldValue whose selected field is Text.
    const st::Material text = st::read_material(pair_of(loom::to_value(st::SourceText{"hello\nworld"})));
    CHECK(text.kind == st::MaterialKind::Text);
    CHECK(text.text == "hello\nworld");
    const loom::Value picked = md::grab_field(md::Draft(sample("picked", 1, 1.0, true)), md::Path{std::string("name")});
    const st::Material field = st::read_material(pair_of(picked));
    CHECK(field.kind == st::MaterialKind::Text);
    CHECK(field.text == "picked");
    const loom::Value count = md::grab_field(md::Draft(sample("picked", 1, 1.0, true)), md::Path{std::string("count")});
    const st::Material not_text = st::read_material(pair_of(count));
    CHECK(not_text.kind == st::MaterialKind::Unsupported);
    CHECK(not_text.refusal.find("Int, not text") != std::string::npos);
    // A location, with its context beside it.
    st::SourceLocationContext ctx;
    ctx.project_root = "C:/work/a";
    ctx.relative = "src/main.cpp";
    const st::Material loc = st::read_material(
        pair_of(loom::to_value(st::SourceLocation{"C:/work/a/src/main.cpp", 12, 5}), {loom::to_value(ctx)}));
    REQUIRE(loc.kind == st::MaterialKind::Location);
    CHECK(loc.location.path == "C:/work/a/src/main.cpp");
    CHECK(loc.location.line == 12);
    REQUIRE(loc.location_context.has_value());
    CHECK(loc.location_context->relative == "src/main.cpp");
    // A Terminal capture: submitted to an office, published, sent to a weave, received.
    zengine::workshop::TerminalCaptureFacts facts;
    facts.kind = "submitted";
    facts.addressing = "role";
    facts.role = "zengine.timer";
    const st::Material role = st::read_material(pair_of(loom::to_value(beat()), {loom::to_value(facts)}));
    CHECK(role.kind == st::MaterialKind::Command);
    CHECK(role.address == "@zengine.timer");
    facts.addressing = "publish";
    CHECK(st::read_material(pair_of(loom::to_value(beat()), {loom::to_value(facts)})).address == "*");
    facts.addressing = "weave";
    facts.target = "12";
    const st::Material weave = st::read_material(pair_of(loom::to_value(beat()), {loom::to_value(facts)}));
    CHECK(weave.address.empty());
    CHECK(weave.address_note.find("#12") != std::string::npos);
    facts.kind = "received";
    facts.addressing.clear();
    facts.sender = "4";
    CHECK(st::read_material(pair_of(loom::to_value(beat()), {loom::to_value(facts)})).address.empty());
    // A preset keeps its absences.
    loom::Value partial(sample_schema());
    partial.set("name", loom::Cell::text("n"));
    const st::Material preset = st::read_material(pair_of(md::store_draft("half", md::Draft(partial))));
    REQUIRE(preset.kind == st::MaterialKind::Command);
    CHECK(preset.preset);
    REQUIRE(preset.command.has_value());
    CHECK(st::terminal_line(*preset.command, "").missing == std::vector<std::string>{"count", "ratio", "on"});
    // Bytes that are not a pair.
    const st::Material junk = st::read_material("not a pair");
    CHECK(junk.kind == st::MaterialKind::Unsupported);
    CHECK(junk.refusal.find("could not be read") != std::string::npos);
}

TEST_CASE("a captured selection is an owned text item with its observation beside it, and a copy the carrier cannot hold is refused, never cut") {
    st::SourceSelection sel;
    sel.editor = "the standard Editor";
    sel.path = "C:/work/a/notes.txt";
    sel.kind = st::kCharacters;
    sel.first_line = 2;
    sel.first_column = 3;
    sel.end_line = 4;
    sel.end_column = 1;
    sel.line_ending = "CRLF";
    sel.unsaved = true;
    const st::Pair p = st::text_pair("ne two\n\tthree\n", sel);
    REQUIRE_MESSAGE(p.ok, p.refusal);
    const auto decoded = zengine::inventory::decode_pair(p.bytes);
    CHECK(loom::from_value<st::SourceText>(decoded.item).text == "ne two\n\tthree\n");
    REQUIRE(decoded.metadata.size() == 1);
    const auto back = loom::from_value<st::SourceSelection>(decoded.metadata[0]);
    CHECK(back.path == sel.path);
    CHECK(back.unsaved);
    CHECK(back.line_ending == "CRLF");
    CHECK(decoded.item.schema().fields().size() == 1); // the text and nothing else
    const st::Pair big = st::text_pair(std::string(70000, 'x'), sel);
    CHECK_FALSE(big.ok);
    CHECK(big.bytes.empty());
    CHECK(big.refusal.find("64 KiB") != std::string::npos);
    CHECK(st::text_pair(std::string("a\0b", 3), sel).refusal.find("NUL") != std::string::npos);
    CHECK(st::text_pair("\xff", sel).refusal.find("UTF-8") != std::string::npos);
    CHECK(st::relative_to("C:/work/a/src/x.cpp", "C:/work/a") == "src/x.cpp");
    CHECK(st::relative_to("C:/work/ab/x.cpp", "C:/work/a").empty());
    CHECK(st::relative_to("C:/work/a", "C:/work/a").empty());
}

TEST_CASE("a location observes its caret line whole below the bound and as a prefix at it, never half a character, and a whole line must still read exactly") {
    // BELOW THE BOUND the observation is the whole line, compared exactly: a line that gained text
    // at its end has changed as surely as one rewritten.
    CHECK(st::observe_line("second line") == "second line");
    CHECK(st::whole_line("second line"));
    CHECK(st::still_reads("second line", "second line"));
    CHECK_FALSE(st::still_reads("second line now changed", "second line")); // appended
    CHECK_FALSE(st::still_reads("second LINE", "second line"));            // replaced
    CHECK_FALSE(st::still_reads("second", "second line"));                 // cut short
    // AT THE BOUND it is a prefix: it proves the line's beginning and nothing after it -- a line of
    // exactly the bound reads the same way as a longer one cut there.
    const std::string bound(st::kMaxLineText, 'a');
    CHECK(st::observe_line(bound) == bound);
    CHECK_FALSE(st::whole_line(bound));
    const std::string longer = bound + " and more";
    const std::string seen = st::observe_line(longer);
    CHECK(seen == bound);
    CHECK(st::still_reads(longer, seen));
    CHECK(st::still_reads(bound + " and something else entirely", seen)); // past the bound is unproved
    CHECK_FALSE(st::still_reads("b" + longer.substr(1), seen));          // within it, a change is caught
    CHECK(st::whole_line(std::string(st::kMaxLineText - 1, 'a'))); // one byte under the bound is whole
    // NEVER HALF A CHARACTER: a bound that falls inside one carries on to that character's end.
    const std::string utf = std::string(st::kMaxLineText - 1, 'a') + "\xc3\xa9 and more";
    const std::string cut = st::observe_line(utf);
    CHECK(cut == std::string(st::kMaxLineText - 1, 'a') + "\xc3\xa9");
    CHECK(st::valid_utf8(cut));
    CHECK_FALSE(st::whole_line(cut));
    CHECK(st::still_reads(utf, cut));
    // ...so the location that carries it is kept and read back whole.
    st::SourceLocationContext ctx;
    ctx.line_text = cut;
    const st::Pair p = st::location_pair(st::SourceLocation{"C:/w/a.txt", 3, 1}, ctx);
    REQUIRE_MESSAGE(p.ok, p.refusal);
    const auto decoded = zengine::inventory::decode_pair(p.bytes);
    REQUIRE(decoded.metadata.size() == 1);
    CHECK(loom::from_value<st::SourceLocationContext>(decoded.metadata[0]).line_text == cut);
    // A LINE THAT IS NOT UTF-8 cannot be recorded, said now, rather than as a pair no reader admits.
    ctx.line_text = "bad \xff";
    const st::Pair bad = st::location_pair(st::SourceLocation{"C:/w/a.txt", 3, 1}, ctx);
    CHECK_FALSE(bad.ok);
    CHECK(bad.bytes.empty());
    CHECK(bad.refusal.find("not valid UTF-8") != std::string::npos);
    // AN EMPTY OBSERVATION says nothing about the line, exactly as a location saved with no context.
    CHECK(st::still_reads("anything at all", ""));
}

TEST_CASE("C++ generation is offered for C++ documents by extension, and a .h is honestly ambiguous") {
    CHECK(st::cpp_document("C:/w/a.cpp") == st::CppDocument::Yes);
    CHECK(st::cpp_document("/w/b.HPP") == st::CppDocument::Yes);
    CHECK(st::cpp_document("/w/c.cc") == st::CppDocument::Yes);
    CHECK(st::cpp_document("/w/d.h") == st::CppDocument::Ambiguous);
    CHECK(st::cpp_document("/w/e.c") == st::CppDocument::No);
    CHECK(st::cpp_document("/w/notes.txt") == st::CppDocument::No);
    CHECK(st::cpp_document("/w.cpp/README") == st::CppDocument::No);
}

TEST_CASE("generated C++ is the golden the compiled witness builds: the value's own schema, escaped data, no send, and the includes it lacks named") {
    const st::GeneratedCpp g = st::cpp_value_function(loom::to_value(beat()), {"#include <cstdio>", "int main() {}"});
    REQUIRE_MESSAGE(g.ok, g.refusal);
    CHECK(g.function == "make_ensure_timer_v1");
    CHECK(g.missing_includes == std::vector<std::string>{"<zen/schema.hpp>", "<zen/value.hpp>"});
    CHECK(g.holes.empty());
    CHECK(st::join_lf(g.lines) + "\n" == read_file(SOURCE_TRANSFER_GOLDEN));
    const std::string all = st::join_lf(g.lines);
    for (const char* act : {"send_to", ".send(", "publish(", "mail", "answer("}) {
        CAPTURE(act);
        CHECK(all.find(act) == std::string::npos); // it builds a value and does nothing with it
    }
    // A document that includes them is told so; an umbrella include counts for both.
    CHECK(st::cpp_value_function(loom::to_value(beat()), {"#include <zen/schema.hpp>", "  #  include <zen/value.hpp>"})
              .missing_includes.empty());
    CHECK(st::cpp_value_function(loom::to_value(beat()), {"#include <zen/zen.hpp>"}).missing_includes.empty());
}

TEST_CASE("generated C++ escapes every payload byte as data, leaves a preset's missing required field as a labelled hole, and refuses a nested shape") {
    const st::GeneratedCpp g = st::cpp_value_function(sample("say \"hi\"\\ \n caf\xc3\xa9 */", std::numeric_limits<std::int64_t>::min(), 5.0, false), {});
    REQUIRE(g.ok);
    const std::string all = st::join_lf(g.lines);
    CHECK(all.find("loom::Cell::text(\"say \\\"hi\\\"\\\\ \\012 caf\\303\\251 */\")") != std::string::npos);
    CHECK(all.find("loom::Cell::integer(-9223372036854775807 - 1)") != std::string::npos);
    CHECK(all.find("loom::Cell::real(5.0)") != std::string::npos);
    CHECK(all.find(".field(\"note\", loom::Kind::Text, false)") != std::string::npos);
    CHECK(all.find("value.set(\"note\"") == std::string::npos); // absent optional: absent
    loom::Value partial(sample_schema());
    partial.set("name", loom::Cell::text("n"));
    const st::GeneratedCpp holes = st::cpp_value_function(partial, {});
    REQUIRE(holes.ok);
    CHECK(holes.holes == std::vector<std::string>{"count", "ratio", "on"});
    const std::string h = st::join_lf(holes.lines);
    CHECK(h.find("// INCOMPLETE -- fill the required fields count, ratio, on before this compiles.") != std::string::npos);
    CHECK(h.find("value.set(\"count\", /* FILL: required Int */);") != std::string::npos);
    const auto nested = loom::SchemaBuilder("Outer", 1).message("inner", sample_schema()).build();
    loom::Value n(nested);
    n.set("inner", loom::Cell::message(sample("x", 1, 1.0, true)));
    const st::GeneratedCpp refused = st::cpp_value_function(n, {});
    CHECK_FALSE(refused.ok);
    CHECK(refused.refusal.find("field `inner` is a nested message") != std::string::npos);
    CHECK(refused.lines.empty());
}

TEST_CASE("generated C++ keeps every byte of every string: a NUL is written as a std::string literal, a name is escaped in code and in comments, and the compiled witness builds the same value") {
    const loom::Value v = source_transfer_samples::string_bytes_sample();
    const st::GeneratedCpp g = st::cpp_value_function(v, {"#include <cstdio>", "int main() {}"});
    REQUIRE_MESSAGE(g.ok, g.refusal);
    // THE GOLDEN program `source_transfer_cpp` compiles, calls and compares with this very value.
    CHECK(st::join_lf(g.lines) + "\n" == read_file(SOURCE_TRANSFER_BYTES_GOLDEN));
    CHECK(g.function == "make_editor_materials_bytes_v1");
    CHECK(g.needs == std::vector<std::string>{"<zen/schema.hpp>", "<zen/value.hpp>", "<string>"});
    CHECK(g.missing_includes == std::vector<std::string>{"<zen/schema.hpp>", "<zen/value.hpp>", "<string>"});
    // NO BYTE OF A NAME OR A PAYLOAD IS RAW IN THE CODE: a line break or a NUL in the schema's name
    // cannot end a comment and become code, and every line is one line.
    bool raw = false;
    for (const std::string& line : g.lines) {
        for (const char c : line) {
            raw = raw || static_cast<unsigned char>(c) < 0x20u || static_cast<unsigned char>(c) >= 0x7Fu;
        }
    }
    CHECK_FALSE(raw);
    const std::string all = st::join_lf(g.lines);
    CHECK(all.find("// ---- Zengine: C++ that builds Editor\\012Materials\\000Bytes v1,") != std::string::npos);
    CHECK(all.find("loom::SchemaBuilder(\"Editor\\012Materials\\000Bytes\"s, 1)") != std::string::npos);
    CHECK(all.find("loom::Cell::text(\"A\\000B\"s)") != std::string::npos); // three bytes, not one
    CHECK(all.find("loom::Cell::text(\"\\0007\\0008\\000\\0000\"s)") != std::string::npos);
    CHECK(all.find("?\\?=") != std::string::npos); // a `??` of a payload is never a trigraph
    CHECK(all.find("loom::Cell::text(\"no NUL here\"));") != std::string::npos); // no NUL: a plain literal
    // WITHOUT A NUL, NOTHING OF THAT: the ordinary command needs no <string>, and says no more.
    const st::GeneratedCpp plain = st::cpp_value_function(loom::to_value(beat()), {});
    CHECK(plain.needs == std::vector<std::string>{"<zen/schema.hpp>", "<zen/value.hpp>"});
    CHECK(st::join_lf(plain.lines).find("string_literals") == std::string::npos);
    // <string> is named missing only where the document lacks it; the umbrella covers Loom's two.
    CHECK(st::cpp_value_function(v, {"#include <string>"}).missing_includes ==
          std::vector<std::string>{"<zen/schema.hpp>", "<zen/value.hpp>"});
    CHECK(st::cpp_value_function(v, {"#include <zen/zen.hpp>", "#include <string>"}).missing_includes.empty());
    CHECK(st::cpp_value_function(v, {"#include <zen/zen.hpp>"}).missing_includes == std::vector<std::string>{"<string>"});
}

} // TEST_SUITE
