// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Suite `neovim` -- the hosting library with no Neovim at all: the codec over every type byte in
// both directions and across split reads, the conversation's books, the screen model, the
// projection, the key notation, the document arithmetic, the launch line, and the conversation
// owner walked through every way a start can fail by a scriptable fake (`neovim_fixture.cpp`).
// What Neovim itself does is `neovim_live`'s subject, behind the `neovim` gate.

#include "doctest.h"

#include "neovim/child.hpp"
#include "neovim/document.hpp"
#include "neovim/grid.hpp"
#include "neovim/host.hpp"
#include "neovim/keys.hpp"
#include "neovim/launch.hpp"
#include "neovim/msgpack.hpp"
#include "neovim/projection.hpp"
#include "neovim/rpc.hpp"

#if defined(_WIN32)
#include "builder/run.hpp"
#endif

#include "input/vocabulary.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace nv = zengine::neovim;
namespace mp = zengine::neovim::msgpack;
namespace rpc = zengine::neovim::rpc;
namespace scan = zengine::input::scan;
namespace mod = zengine::input::mod;

mp::Value roundtrip(const mp::Value& v) {
    const std::string bytes = mp::encode(v);
    mp::Decoder d{mp::Limits{}};
    d.feed(bytes);
    mp::Decoder::Result r = d.next();
    REQUIRE(r.status == mp::Decoder::Status::Value);
    return r.value;
}

std::string header_of(const mp::Value& v) {
    const std::string bytes = mp::encode(v);
    return bytes.substr(0, 1);
}

/// A redraw notification's params: batches of [name, args...].
mp::Value batch(const char* name, std::vector<mp::Value> args) {
    mp::Value::Array a;
    a.push_back(mp::Value::str(name));
    for (mp::Value& one : args) {
        a.push_back(std::move(one));
    }
    return mp::Value::array(std::move(a));
}

mp::Value ints(std::initializer_list<std::int64_t> values) {
    mp::Value::Array a;
    for (const std::int64_t v : values) {
        a.push_back(mp::Value::integer(v));
    }
    return mp::Value::array(std::move(a));
}

mp::Value cells(const std::string& text, std::int64_t hl) {
    mp::Value::Array a;
    bool first = true;
    for (const char c : text) {
        if (first) {
            a.push_back(rpc::params(mp::Value::str(std::string(1, c)), mp::Value::integer(hl)));
            first = false;
        } else {
            a.push_back(rpc::params(mp::Value::str(std::string(1, c))));
        }
    }
    return mp::Value::array(std::move(a));
}

mp::Value line(std::int64_t row, std::int64_t col, const std::string& text, std::int64_t hl) {
    return rpc::params(mp::Value::integer(1), mp::Value::integer(row), mp::Value::integer(col),
                       cells(text, hl));
}

mp::Value hl_define(std::int64_t id, const char* ui_name) {
    mp::Value::Map info;
    info.push_back({mp::Value::str("kind"), mp::Value::str("ui")});
    info.push_back({mp::Value::str("ui_name"), mp::Value::str(ui_name)});
    info.push_back({mp::Value::str("hi_name"), mp::Value::str(ui_name)});
    mp::Value::Array infos;
    infos.push_back(mp::Value::map(std::move(info)));
    return rpc::params(mp::Value::integer(id), mp::Value::map({}), mp::Value::map({}),
                       mp::Value::array(std::move(infos)));
}

mp::Value redraw(std::vector<mp::Value> batches) {
    return mp::Value::array(std::move(batches));
}

/// A 3x12 grid: two text rows, a status row; hl 1 = Visual, 2 = StatusLine, 3 = ErrorMsg,
/// 4 = EndOfBuffer, 5 = PmenuSel.
nv::Grid small_grid() {
    nv::Grid g;
    const nv::Applied a = g.apply(redraw({
        batch("grid_resize", {ints({1, 12, 4})}),
        batch("hl_attr_define", {hl_define(1, "Visual"), hl_define(2, "StatusLine"),
                                 hl_define(3, "ErrorMsg"), hl_define(4, "EndOfBuffer"),
                                 hl_define(5, "PmenuSel")}),
        batch("grid_line", {line(0, 0, "hello world ", 0), line(1, 0, "second line ", 0),
                            line(2, 0, "~           ", 4), line(3, 0, "status line ", 2)}),
        batch("grid_cursor_goto", {ints({1, 0, 3})}),
        batch("mode_change", {rpc::params(mp::Value::str("normal"), mp::Value::integer(0))}),
        batch("flush", {mp::Value::array({})}),
    }));
    REQUIRE(a.refusal.empty());
    REQUIRE(a.flushed);
    return g;
}

using Clock = std::chrono::steady_clock;

template <class Done>
bool until(nv::Host& host, Done done, int ms = 10000) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        (void)host.pump();
        if (done()) {
            return true;
        }
        if (Clock::now() >= deadline) {
            return false;
        }
        (void)host.wait(10);
    }
}

nv::Host::Options fixture(const char* mode, int startup_ms = 10000) {
    nv::Host::Options o;
    o.launch.program = NEOVIM_FIXTURE;
    o.launch.args = {mode};
    o.rows = 5;
    o.columns = 40;
    o.startup_ms = startup_ms;
    return o;
}

} // namespace

TEST_SUITE("neovim") {

// ---- the codec ----------------------------------------------------------------------------

TEST_CASE("the codec writes every type family in its smallest form and reads it back") {
    CHECK(header_of(mp::Value::nil()) == "\xc0");
    CHECK(header_of(mp::Value::boolean(true)) == "\xc3");
    CHECK(header_of(mp::Value::integer(5)) == "\x05");
    CHECK(header_of(mp::Value::integer(-3)) == "\xfd");
    CHECK(header_of(mp::Value::integer(200)) == "\xcc");
    CHECK(header_of(mp::Value::integer(-100)) == "\xd0");
    CHECK(header_of(mp::Value::integer(70000)) == "\xce");
    CHECK(header_of(mp::Value::integer(-70000)) == "\xd2");
    CHECK(header_of(mp::Value::uinteger(std::numeric_limits<std::uint64_t>::max())) == "\xcf");
    CHECK(header_of(mp::Value::integer(std::numeric_limits<std::int64_t>::min())) == "\xd3");
    CHECK(header_of(mp::Value::str("abc")) == "\xa3");
    CHECK(header_of(mp::Value::str(std::string(40, 'x'))) == "\xd9");
    CHECK(header_of(mp::Value::str(std::string(300, 'x'))) == "\xda");
    CHECK(header_of(mp::Value::str(std::string(70000, 'x'))) == "\xdb");
    CHECK(header_of(mp::Value::bin("ab")) == "\xc4");
    CHECK(header_of(mp::Value::array({})) == "\x90");
    CHECK(header_of(mp::Value::map({})) == "\x80");
    CHECK(header_of(mp::Value::ext(0, std::string(1, '\x07'))) == "\xd4");

    CHECK(roundtrip(mp::Value::integer(-70000)).as_int() == -70000);
    CHECK(roundtrip(mp::Value::uinteger(std::numeric_limits<std::uint64_t>::max())).as_uint() ==
          std::numeric_limits<std::uint64_t>::max());
    CHECK(roundtrip(mp::Value::integer(std::numeric_limits<std::int64_t>::min())).as_int() ==
          std::numeric_limits<std::int64_t>::min());
    CHECK(roundtrip(mp::Value::floating(2.5)).as_float() == doctest::Approx(2.5));
    CHECK(roundtrip(mp::Value::str(std::string(70000, 'y'))).as_str().size() == 70000);
    mp::Value::Array big;
    for (int i = 0; i < 70000; ++i) {
        big.push_back(mp::Value::integer(i));
    }
    const mp::Value back = roundtrip(mp::Value::array(std::move(big)));
    CHECK(back.size() == 70000);
    CHECK(back.at(69999).as_int() == 69999);
    // A Neovim handle: an ext whose payload is itself a msgpack integer.
    CHECK(roundtrip(mp::Value::ext(1, std::string("\xcc\xc8", 2))).handle() == 200);
    mp::Value::Map m;
    m.push_back({mp::Value::str("k"), rpc::params(mp::Value::boolean(false), mp::Value::nil())});
    const mp::Value mb = roundtrip(mp::Value::map(std::move(m)));
    REQUIRE(mb.get("k") != nullptr);
    CHECK(mb.get("k")->at(1).is_nil());
}

TEST_CASE("the reader resumes across reads of any size and never re-walks what it has scanned") {
    mp::Value::Array a;
    for (int i = 0; i < 1000; ++i) {
        a.push_back(mp::Value::str("item " + std::to_string(i)));
    }
    const std::string bytes = mp::encode(mp::Value::array(std::move(a))) + mp::encode(mp::Value::integer(7));
    for (const std::size_t step : {std::size_t{1}, std::size_t{3}, std::size_t{4096}}) {
        CAPTURE(step);
        mp::Decoder d{mp::Limits{}};
        std::vector<mp::Value> got;
        for (std::size_t i = 0; i < bytes.size(); i += step) {
            d.feed(std::string_view(bytes).substr(i, step));
            for (;;) {
                mp::Decoder::Result r = d.next();
                if (r.status != mp::Decoder::Status::Value) {
                    CHECK(r.status == mp::Decoder::Status::NeedMore);
                    break;
                }
                got.push_back(std::move(r.value));
            }
        }
        REQUIRE(got.size() == 2);
        CHECK(got[0].size() == 1000);
        CHECK(got[0].at(999).as_str() == "item 999");
        CHECK(got[1].as_int() == 7);
        CHECK(d.buffered() == 0);
    }
}

TEST_CASE("the reader refuses what no bound admits, before it is believed, and stays refused") {
    {
        mp::Decoder d{mp::Limits{}};
        d.feed(std::string("\xc1", 1));
        CHECK(d.next().status == mp::Decoder::Status::Malformed);
        d.feed(mp::encode(mp::Value::integer(1)));
        CHECK(d.next().status == mp::Decoder::Status::Malformed); // sticky
    }
    {
        mp::Limits small;
        small.max_message_bytes = 64;
        mp::Decoder d{small};
        d.feed(std::string("\xdb\x0c\x80\x00\x00", 5)); // a str32 declaring 200 MiB, nothing else
        CHECK(d.next().status == mp::Decoder::Status::TooLarge);
    }
    {
        mp::Limits shallow;
        shallow.max_depth = 4;
        mp::Decoder d{shallow};
        d.feed(std::string("\x91\x91\x91\x91\x91\x91\x01", 7));
        CHECK(d.next().status == mp::Decoder::Status::Malformed);
    }
    {
        mp::Limits few;
        few.max_elements = 10;
        mp::Decoder d{few};
        d.feed(std::string("\xdd\x00\x10\x00\x00", 5)); // an array32 declaring a million items
        CHECK(d.next().status == mp::Decoder::Status::Malformed);
    }
}

// ---- the conversation's books ---------------------------------------------------------------

TEST_CASE("a response is matched by its id alone, and an id nobody issued ends the conversation") {
    rpc::Session s;
    const auto a = s.request("nvim_one", rpc::params());
    const auto b = s.request("nvim_two", rpc::params());
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a == 1);
    CHECK(*b == 2);
    CHECK(s.pending() == 2);
    CHECK(s.outbound_size() > 0);
    const auto answer = [](std::uint32_t id, std::int64_t value) {
        std::string f;
        mp::Writer w(f);
        w.array_header(4);
        w.uinteger(1);
        w.uinteger(id);
        w.nil();
        w.integer(value);
        return f;
    };
    // OUT OF ORDER, as a Neovim inside vim.wait() answers.
    s.receive(answer(2, 20) + answer(1, 10));
    auto first = s.next();
    REQUIRE(first.has_value());
    REQUIRE(std::holds_alternative<rpc::Response>(*first));
    CHECK(std::get<rpc::Response>(*first).id == 2);
    CHECK(std::get<rpc::Response>(*first).result.as_int() == 20);
    auto second = s.next();
    REQUIRE(second.has_value());
    CHECK(std::get<rpc::Response>(*second).id == 1);
    CHECK(s.pending() == 0);
    s.receive(answer(1, 10)); // already heard
    auto stray = s.next();
    REQUIRE(stray.has_value());
    CHECK(std::holds_alternative<rpc::ProtocolError>(*stray));
    CHECK(s.broken());
    CHECK_FALSE(s.request("nvim_three", rpc::params()).has_value());
    CHECK_FALSE(s.next().has_value());
}

TEST_CASE("the outbound queue and the outstanding requests are bounded, and a refusal queues nothing") {
    rpc::Session::Bounds bounds;
    bounds.max_outbound = 64;
    bounds.max_pending = 2;
    rpc::Session s(bounds);
    CHECK_FALSE(s.request("nvim_exec_lua", rpc::params(mp::Value::str(std::string(100, 'x')))).has_value());
    CHECK(s.refusal().find("not reading") != std::string::npos);
    CHECK(s.outbound_size() == 0);
    CHECK(s.pending() == 0);
    CHECK(s.request("a", rpc::params()).has_value());
    CHECK(s.request("b", rpc::params()).has_value());
    CHECK_FALSE(s.request("c", rpc::params()).has_value());
    CHECK(s.refusal().find("unanswered") != std::string::npos);
    CHECK(s.notify("d", rpc::params()));
    std::string f;
    {
        mp::Writer w(f);
        w.array_header(3);
        w.uinteger(2);
        w.str("redraw");
        w.array_header(0);
    }
    {
        mp::Writer w(f);
        w.array_header(4);
        w.uinteger(0);
        w.uinteger(9);
        w.str("zengine_clipboard_paste");
        w.array_header(0);
    }
    s.receive(f);
    auto n = s.next();
    REQUIRE(n.has_value());
    CHECK(std::get<rpc::Notification>(*n).method == "redraw");
    auto q = s.next();
    REQUIRE(q.has_value());
    CHECK(std::get<rpc::Request>(*q).id == 9);
    s.receive(std::string("\x93\x05\x01\x02", 4));
    auto bad = s.next();
    REQUIRE(bad.has_value());
    CHECK(std::holds_alternative<rpc::ProtocolError>(*bad));
}

// ---- the screen ---------------------------------------------------------------------------

TEST_CASE("the grid keeps cells, highlights by meaning, the cursor, the mode and its cursor shape") {
    nv::Grid g = small_grid();
    CHECK(g.rows() == 4);
    CHECK(g.columns() == 12);
    CHECK(g.at(0, 0).text == "h");
    CHECK(g.at(0, 5).text == " ");
    CHECK(g.at(3, 0).hl == 2);
    CHECK(g.at(3, 1).hl == 2); // an omitted id repeats the previous cell's
    CHECK((g.groups_at(3, 0) & nv::ui_group::kStatusLine) != 0);
    CHECK((g.groups_at(2, 0) & nv::ui_group::kEndOfBuffer) != 0);
    CHECK(g.cursor_row() == 0);
    CHECK(g.cursor_column() == 3);
    CHECK(g.mode() == "normal");
    CHECK(g.cursor_shape() == nv::CursorShape::Block);
    CHECK(g.flushes() == 1);

    mp::Value::Map normal;
    normal.push_back({mp::Value::str("name"), mp::Value::str("normal")});
    normal.push_back({mp::Value::str("cursor_shape"), mp::Value::str("block")});
    mp::Value::Map insert;
    insert.push_back({mp::Value::str("name"), mp::Value::str("insert")});
    insert.push_back({mp::Value::str("cursor_shape"), mp::Value::str("vertical")});
    mp::Value::Array infos;
    infos.push_back(mp::Value::map(std::move(normal)));
    infos.push_back(mp::Value::map(std::move(insert)));
    nv::Applied a = g.apply(redraw({
        batch("mode_info_set", {rpc::params(mp::Value::boolean(true), mp::Value::array(std::move(infos)))}),
        batch("mode_change", {rpc::params(mp::Value::str("insert"), mp::Value::integer(1))}),
        batch("grid_line", {rpc::params(mp::Value::integer(1), mp::Value::integer(1), mp::Value::integer(0),
                                        mp::Value::array({rpc::params(mp::Value::str("-"), mp::Value::integer(0),
                                                                      mp::Value::integer(4))}))}),
        batch("win_viewport", {rpc::params(mp::Value::integer(1), mp::Value::ext(1, std::string("\x01", 1)),
                                           mp::Value::integer(3), mp::Value::integer(9), mp::Value::integer(4),
                                           mp::Value::integer(2), mp::Value::integer(40))}),
    }));
    CHECK(a.refusal.empty());
    CHECK(a.mode_changed);
    CHECK_FALSE(a.flushed);
    CHECK(g.cursor_shape() == nv::CursorShape::Vertical);
    CHECK(g.at(1, 0).text == "-");
    CHECK(g.at(1, 3).text == "-");
    CHECK(g.at(1, 4).text == "n");
    CHECK(g.viewport().known);
    CHECK(g.viewport().topline == 3);
    CHECK(g.viewport().line_count == 40);

    // SCROLL UP by one inside rows [0, 3): row 0 takes row 1, row 1 takes row 2.
    a = g.apply(redraw({batch("grid_scroll", {ints({1, 0, 3, 0, 12, 1, 0})})}));
    CHECK(a.refusal.empty());
    CHECK(g.at(0, 0).text == "-");
    CHECK(g.at(1, 0).text == "~");
    // ...and down by one: row 2 takes row 1, row 1 takes row 0.
    a = g.apply(redraw({batch("grid_scroll", {ints({1, 0, 3, 0, 12, -1, 0})})}));
    CHECK(g.at(2, 0).text == "~");
    CHECK(g.at(1, 0).text == "-");
    a = g.apply(redraw({batch("grid_clear", {ints({1})})}));
    CHECK(g.at(3, 0).text == " ");
    CHECK(g.at(3, 0).hl == 0);
}

TEST_CASE("the grid refuses a screen it cannot hold, and keeps what it had") {
    nv::Grid g = small_grid();
    nv::Applied a = g.apply(redraw({batch("grid_resize", {ints({1, 5000, 5000})})}));
    CHECK(a.refusal.find("beyond what this model holds") != std::string::npos);
    CHECK(g.rows() == 4);
    a = g.apply(redraw({batch("grid_line", {line(9, 0, "x", 0)})}));
    CHECK(a.refusal.find("row 9") != std::string::npos);
    a = g.apply(redraw({batch("grid_line", {line(0, 10, "xyz", 0)})}));
    CHECK(a.refusal.find("past the end") != std::string::npos);
    a = g.apply(mp::Value::str("not a batch list"));
    CHECK_FALSE(a.refusal.empty());
    // An event this model does not read is ignored, not refused.
    a = g.apply(redraw({batch("option_set", {rpc::params(mp::Value::str("guifont"), mp::Value::str(""))})}));
    CHECK(a.refusal.empty());
}

TEST_CASE("the projection: a block cursor is its cell, a bar is a caret, rows carry their meaning") {
    nv::Grid g = small_grid();
    nv::Screen s = nv::project(g);
    REQUIRE(s.rows.size() == 4);
    CHECK(s.rows[0].text == "hello world ");
    CHECK(s.rows[0].kind == nv::RowKind::Text);
    CHECK(s.rows[2].kind == nv::RowKind::Muted);
    CHECK(s.rows[3].kind == nv::RowKind::Chrome);
    CHECK(s.caret_row == -1);
    CHECK(s.source == nv::SelectionSource::Block);
    CHECK(s.sel_begin_row == 0);
    CHECK(s.sel_begin_col == 3);
    CHECK(s.sel_end_col == 4);

    // An error on a row makes it an alert, and a bar cursor is a caret with no block.
    nv::Applied a = g.apply(redraw({
        batch("grid_line", {line(1, 0, "E42: broken", 3)}),
        batch("mode_change", {rpc::params(mp::Value::str("insert"), mp::Value::integer(1))}),
        batch("grid_cursor_goto", {ints({1, 1, 2})}),
    }));
    REQUIRE(a.refusal.empty());
    s = nv::project(g);
    CHECK(s.rows[1].kind == nv::RowKind::Alert);
    CHECK(s.caret_row == 1);
    CHECK(s.caret_col == 2);
    CHECK(s.source == nv::SelectionSource::None);
}

TEST_CASE("the projection: Visual runs through the cursor cell, blockwise is the cursor row, a menu item is the range") {
    nv::Grid g = small_grid();
    nv::Applied a = g.apply(redraw({
        batch("grid_line", {line(0, 2, "llo world ", 1), line(1, 0, "se", 1)}),
        batch("mode_change", {rpc::params(mp::Value::str("visual"), mp::Value::integer(2))}),
        batch("grid_cursor_goto", {ints({1, 1, 2})}),
    }));
    REQUIRE(a.refusal.empty());
    nv::Screen s = nv::project(g, 'v');
    CHECK(s.source == nv::SelectionSource::Visual);
    CHECK(s.sel_begin_row == 0);
    CHECK(s.sel_begin_col == 2);
    CHECK(s.sel_end_row == 1);
    CHECK(s.sel_end_col == 3); // Visual cells end at column 1; the cursor cell is column 2
    s = nv::project(g, '\x16');
    CHECK(s.sel_begin_row == 1);
    CHECK(s.sel_end_row == 1);
    CHECK(s.sel_begin_col == 0);
    CHECK(s.sel_end_col == 3);
    // A cursor BEFORE the Visual cells begins the range (a backward selection).
    a = g.apply(redraw({batch("grid_cursor_goto", {ints({1, 0, 0})})}));
    s = nv::project(g, 'v');
    CHECK(s.sel_begin_row == 0);
    CHECK(s.sel_begin_col == 0);
    CHECK(s.sel_end_row == 1);
    CHECK(s.sel_end_col == 2);

    nv::Grid menu = small_grid();
    a = menu.apply(redraw({
        batch("grid_line", {line(1, 2, "item", 5)}),
        batch("mode_change", {rpc::params(mp::Value::str("insert"), mp::Value::integer(1))}),
    }));
    s = nv::project(menu);
    CHECK(s.source == nv::SelectionSource::Popup);
    CHECK(s.sel_begin_row == 1);
    CHECK(s.sel_begin_col == 2);
    CHECK(s.sel_end_col == 6);
}

TEST_CASE("every cell becomes one drawable byte, and the document itself is never touched") {
    CHECK(nv::ascii_of("a") == 'a');
    CHECK(nv::ascii_of(" ") == ' ');
    CHECK(nv::ascii_of("") == '?');
    CHECK(nv::ascii_of("\x01") == '?');
    CHECK(nv::ascii_of("\xe2\x94\x80") == '-'); // U+2500
    CHECK(nv::ascii_of("\xe2\x94\x82") == '|'); // U+2502
    CHECK(nv::ascii_of("\xe2\x94\x8c") == '+'); // U+250C
    CHECK(nv::ascii_of("\xc2\xa0") == ' ');     // U+00A0
    CHECK(nv::ascii_of("\xe2\x80\xa6") == '.'); // U+2026
    CHECK(nv::ascii_of("\xe2\x86\x92") == '>'); // U+2192
    CHECK(nv::ascii_of("\xe4\xb8\xad") == '?'); // U+4E2D
    CHECK(nv::ascii_of("\xe4") == '?');         // a sequence cut short
}

// ---- keys ---------------------------------------------------------------------------------

TEST_CASE("keys: text is sent as text with < spelled <lt>, and a key only when no text follows it") {
    CHECK(nv::text_input("a<b>c") == "a<lt>b>c");
    CHECK(nv::text_input("<") == "<lt>");
    CHECK(nv::key_input(scan::kA, mod::kNone).empty());
    CHECK(nv::key_input(scan::kA, mod::kShift).empty());
    CHECK(nv::key_input(scan::kSpace, mod::kNone).empty());
    CHECK(nv::key_input(scan::kA, mod::kCtrl | mod::kAlt).empty()); // AltGr: its text carries it
    CHECK(nv::key_input(scan::kA, mod::kCtrl) == "<C-a>");
    CHECK(nv::key_input(scan::kO, mod::kCtrl) == "<C-o>");
    CHECK(nv::key_input(scan::kV, mod::kCtrl | mod::kShift) == "<C-S-v>");
    CHECK(nv::key_input(scan::kX, mod::kAlt) == "<M-x>");
    CHECK(nv::key_input(scan::kRightBracket, mod::kCtrl) == "<C-]>");
    CHECK(nv::key_input(scan::kReturn, mod::kNone) == "<CR>");
    CHECK(nv::key_input(scan::kEscape, mod::kNone) == "<Esc>");
    CHECK(nv::key_input(scan::kBackspace, mod::kNone) == "<BS>");
    CHECK(nv::key_input(scan::kTab, mod::kShift) == "<S-Tab>");
    CHECK(nv::key_input(scan::kSpace, mod::kCtrl) == "<C-Space>");
    CHECK(nv::key_input(scan::kLeft, mod::kCtrl | mod::kShift) == "<C-S-Left>");
    CHECK(nv::key_input(scan::kDelete, mod::kNone) == "<Del>");
    CHECK(nv::key_input(nv::sdl_scan::kPageDown, mod::kNone) == "<PageDown>");
    CHECK(nv::key_input(nv::sdl_scan::kF1 + 4, mod::kNone) == "<F5>");
    CHECK(nv::key_input(9999, mod::kCtrl).empty());
    CHECK(nv::mouse_modifiers(mod::kCtrl | mod::kShift) == "CS");
    CHECK(nv::mouse_modifiers(mod::kNone).empty());
}

// ---- the document arithmetic ---------------------------------------------------------------

TEST_CASE("file bytes and Neovim's lines are one document: conventions and the final newline") {
    const std::vector<std::string> cases = {"",       "\n",        "a",          "a\n",
                                            "a\nb",   "a\r\nb\r\n", "\r\n",       "a\r\n\r\n",
                                            "x\ry\n", "a\n\nb\n",   "mixed\r\nlf\n"};
    for (const std::string& bytes : cases) {
        CAPTURE(bytes);
        const nv::NeovimText t = nv::neovim_text(bytes);
        CHECK(nv::file_bytes(t.lines, t.dos, t.final_newline) == bytes);
        CHECK_FALSE(t.lines.empty());
    }
    CHECK(nv::neovim_text("a\r\nb\r\n").dos);
    CHECK_FALSE(nv::neovim_text("mixed\r\nlf\n").dos); // CR stays in the line: bytes are exact
    CHECK(nv::neovim_text("\n").final_newline);
    CHECK(nv::neovim_text("\n").lines.size() == 1);
    CHECK_FALSE(nv::neovim_text("").final_newline);

    CHECK(nv::writes_final_newline({""}, true, true, false, 0) == false); // a freshly read empty file
    CHECK(nv::writes_final_newline({""}, true, true, false, 1) == true);
    CHECK(nv::writes_final_newline({"a"}, false, true, false, 1) == true);  // fixendofline
    CHECK(nv::writes_final_newline({"a"}, false, true, true, 1) == false);  // ...not in binary
    CHECK(nv::writes_final_newline({"a"}, false, false, false, 1) == false);
}

TEST_CASE("a caret and a selection placed in Neovim: Normal, Insert past the end, Visual both ways") {
    const std::vector<std::string> lines = {"alpha", "", "beta"}; // + final newline
    nv::Placement p = nv::place(lines, true, nv::Pos{0, 2}, nv::Pos{0, 2});
    CHECK(p.mode == nv::Placement::Mode::Normal);
    CHECK(p.cursor == nv::Pos{0, 2});
    CHECK_FALSE(p.adjusted);
    CHECK(nv::placement_keys(p) == "<Esc><Cmd>call cursor(1, 3)<CR>");

    p = nv::place(lines, true, nv::Pos{0, 5}, nv::Pos{0, 5});
    CHECK(p.mode == nv::Placement::Mode::Insert);
    CHECK(nv::placement_keys(p) == "<Esc><Cmd>call cursor(1, 1)<CR>A");
    p = nv::place(lines, true, nv::Pos{1, 0}, nv::Pos{1, 0});
    CHECK(p.mode == nv::Placement::Mode::Normal); // an empty line has no past-the-end to insert at

    p = nv::place(lines, true, nv::Pos{3, 0}, nv::Pos{3, 0}); // the final empty line
    CHECK(p.adjusted);
    CHECK(p.cursor == nv::Pos{2, 4});
    CHECK(p.mode == nv::Placement::Mode::Insert);

    // Forward, ending at the start of a line: the previous line's newline is selected.
    p = nv::place(lines, true, nv::Pos{0, 1}, nv::Pos{1, 0});
    CHECK(p.mode == nv::Placement::Mode::Visual);
    CHECK(p.start == nv::Pos{0, 1});
    CHECK(p.cursor == nv::Pos{0, 5});
    CHECK(nv::placement_keys(p) ==
          "<Esc><Cmd>call cursor(1, 2)<CR>v<Cmd>call cursor(1, 2)<CR>o<Cmd>call cursor(1, 6)<CR>");
    // Backward: the Visual start is the last selected byte and the cursor is the caret.
    p = nv::place(lines, true, nv::Pos{2, 3}, nv::Pos{0, 4});
    CHECK(p.start == nv::Pos{2, 2});
    CHECK(p.cursor == nv::Pos{0, 4});
    CHECK_FALSE(p.adjusted);
    // One character, backward: no direction to keep.
    p = nv::place(lines, true, nv::Pos{0, 3}, nv::Pos{0, 2});
    CHECK(p.start == p.cursor);
    CHECK(p.adjusted);
}

TEST_CASE("a Neovim position carried back: charwise exact both ways, linewise as lines, blockwise as its cursor") {
    const std::vector<std::string> lines = {"alpha", "", "beta"};
    nv::Carried c = nv::carry(lines, true, nv::NeovimPosition{"n", nv::Pos{2, 1}, nv::Pos{0, 0}});
    CHECK(c.anchor == nv::Pos{2, 1});
    CHECK(c.caret == nv::Pos{2, 1});
    CHECK(c.note.empty());
    c = nv::carry(lines, true, nv::NeovimPosition{"i", nv::Pos{0, 5}, nv::Pos{0, 0}});
    CHECK(c.caret == nv::Pos{0, 5});
    // Charwise forward through the past-end of line 0: the exclusive end is the start of line 1.
    c = nv::carry(lines, true, nv::NeovimPosition{"v", nv::Pos{0, 5}, nv::Pos{0, 1}});
    CHECK(c.kind == nv::Carried::Kind::Charwise);
    CHECK(c.anchor == nv::Pos{0, 1});
    CHECK(c.caret == nv::Pos{1, 0});
    // Charwise backward, ending past the end of the LAST line with a final newline: the final
    // empty standard line is where it ends.
    c = nv::carry(lines, true, nv::NeovimPosition{"v", nv::Pos{0, 0}, nv::Pos{2, 4}});
    CHECK(c.anchor == nv::Pos{3, 0});
    CHECK(c.caret == nv::Pos{0, 0});
    // ...and without a final newline there is nothing after it.
    c = nv::carry(lines, false, nv::NeovimPosition{"v", nv::Pos{2, 4}, nv::Pos{2, 0}});
    CHECK(c.caret == nv::Pos{2, 4});
    c = nv::carry(lines, true, nv::NeovimPosition{"V", nv::Pos{0, 2}, nv::Pos{2, 1}});
    CHECK(c.kind == nv::Carried::Kind::Linewise);
    CHECK(c.anchor == nv::Pos{3, 0});
    CHECK(c.caret == nv::Pos{0, 0});
    CHECK_FALSE(c.note.empty());
    c = nv::carry(lines, true, nv::NeovimPosition{std::string(1, '\x16'), nv::Pos{2, 1}, nv::Pos{0, 0}});
    CHECK(c.kind == nv::Carried::Kind::Blockwise);
    CHECK(c.caret == nv::Pos{2, 1});
    CHECK(c.anchor == nv::Pos{2, 1});
    c = nv::carry(lines, true, nv::NeovimPosition{"no", nv::Pos{9, 9}, nv::Pos{0, 0}});
    CHECK(c.caret == nv::Pos{2, 4}); // clamped into the document
    CHECK(c.note.find("was reset") != std::string::npos);
    CHECK(nv::first_row_of(12) == 11);
    CHECK(nv::first_col_of(7, true) == 0);
    CHECK(nv::first_col_of(7, false) == 7);
}

TEST_CASE("every placement a document allows carries back exactly, or says it was adjusted") {
    const std::vector<std::vector<std::string>> docs = {{"ab", "\tc", ""}, {"x"}, {""}};
    for (const std::vector<std::string>& lines : docs) {
        for (const bool final_newline : {false, true}) {
            const std::int64_t rows = static_cast<std::int64_t>(lines.size()) + (final_newline ? 1 : 0);
            std::vector<nv::Pos> all;
            for (std::int64_t r = 0; r < rows; ++r) {
                const std::int64_t len =
                    r < static_cast<std::int64_t>(lines.size()) ? static_cast<std::int64_t>(lines[static_cast<std::size_t>(r)].size()) : 0;
                for (std::int64_t b = 0; b <= len; ++b) {
                    all.push_back(nv::Pos{r, b});
                }
            }
            for (const nv::Pos& a : all) {
                for (const nv::Pos& c : all) {
                    const nv::Placement p = nv::place(lines, final_newline, a, c);
                    // WHAT NEOVIM REPORTS FOR THAT PLACEMENT, as the live suite measures it.
                    nv::NeovimPosition at;
                    at.mode = p.mode == nv::Placement::Mode::Visual ? "v"
                              : p.mode == nv::Placement::Mode::Insert ? "i"
                                                                       : "n";
                    at.cursor = p.cursor;
                    at.vstart = p.mode == nv::Placement::Mode::Visual ? p.start : p.cursor;
                    const nv::Carried back = nv::carry(lines, final_newline, at);
                    const bool exact = back.anchor == a && back.caret == c;
                    CHECK_MESSAGE((exact || p.adjusted), "anchor " << a.row << "," << a.byte << " caret "
                                                                  << c.row << "," << c.byte);
                }
            }
        }
    }
}

// ---- launching ------------------------------------------------------------------------------

TEST_CASE("the launch line: clean is isolated, user is the maker's own, a path is -u, remote listens") {
    nv::LaunchChoice clean;
    nv::LaunchSpec s = nv::launch_spec(clean, nv::UiMode::Embedded, "", "/work");
    CHECK(s.program == "nvim");
    CHECK(s.args == std::vector<std::string>{"--embed", "--clean"});
    REQUIRE(s.set_env.size() == 1);
    CHECK(s.set_env[0].first == "NVIM_APPNAME");
    CHECK(s.set_env[0].second == nv::kCleanAppName);
    CHECK(s.cwd == "/work");
    CHECK(s.unset_env == std::vector<std::string>{"NVIM", "NVIM_LISTEN_ADDRESS"});

    nv::LaunchChoice user;
    user.profile = "user";
    s = nv::launch_spec(user, nv::UiMode::Remote, "/tmp/nvim.sock", "");
    CHECK(s.args == std::vector<std::string>{"--embed", "--headless", "--listen", "/tmp/nvim.sock"});
    CHECK(s.set_env.empty());

    nv::LaunchChoice custom;
    custom.program = "/opt/nvim/bin/nvim";
    custom.profile = "/home/maker/minimal.lua";
    s = nv::launch_spec(custom, nv::UiMode::Embedded, "", "");
    CHECK(s.args == std::vector<std::string>{"--embed", "-u", "/home/maker/minimal.lua"});
    CHECK(nv::profile_words(custom).find("init file") == 0);
}

TEST_CASE("one Windows quoting rule: the Neovim custody and the Builder spell the same line") {
    const std::vector<std::pair<std::string, std::string>> spelled = {
        {"plain", "plain"},
        {"with space", "\"with space\""},
        {"", "\"\""},
        {"quote\"inside", "\"quote\\\"inside\""},
        {"trailing\\ space", "\"trailing\\ space\""},
        {"end slash \\", "\"end slash \\\\\""},
        {"slashes\\\\\"quote", "\"slashes\\\\\\\\\\\"quote\""},
        {"C:\\Program Files\\x", "\"C:\\Program Files\\x\""},
    };
    for (const auto& [arg, expected] : spelled) {
        CAPTURE(arg);
        CHECK(nv::detail::windows_quote(arg) == expected);
#if defined(_WIN32)
        CHECK(zengine::builder::detail::windows_quote(arg) == expected);
        const std::wstring wide = nv::detail::windows_quote(nv::detail::wide(arg));
        CHECK(wide == nv::detail::wide(expected));
#endif
    }
}

// ---- the conversation owner, against a fake ------------------------------------------------

TEST_CASE("the owner starts a well-behaved peer, hears its screen, and ends it by asking") {
    nv::Host host;
    REQUIRE(host.start(fixture("ok")));
    REQUIRE(until(host, [&host] { return host.ready() || !host.alive(); }));
    CHECK(host.ready());
    CHECK(host.version().api_level == 13);
    CHECK(host.clipboard_installed());
    REQUIRE(host.input("typed"));
    REQUIRE(until(host, [&host] { return host.grid().flushes() > 0; }));
    CHECK(host.grid().at(0, 0).text == "t");
    std::string why;
    std::optional<rpc::Response> r = host.call_now("nvim_get_mode", rpc::params(), 5000, why);
    REQUIRE_MESSAGE(r.has_value(), why);
    CHECK(r->result.get("mode")->as_str() == "n");
    const nv::Child::Ended ended = host.finish(nv::kQuitGraceMs);
    CHECK_FALSE(ended.forced);
    CHECK(ended.status == 0);
    CHECK(host.phase() == nv::Host::Phase::Ended);
    CHECK_FALSE(host.call("nvim_get_mode", rpc::params(), [](const rpc::Response&) {}));
}

TEST_CASE("every way a start fails is said, in whose words, and nothing is left running") {
    struct Case {
        const char* mode;
        int startup_ms;
        const char* words;
    };
    const Case cases[] = {
        {"old", 10000, "older than this Workshop supports"},
        {"garbage", 10000, "not msgpack-RPC"},
        {"silent", 400, "did not become ready within"},
        {"prompt", 10000, "stopped at a prompt"},
        {"crash", 10000, "exited unexpectedly (status 3)"},
        {"flood", 10000, "a message larger than"},
        {"stranger", 10000, "never sent or already heard"},
    };
    for (const Case& c : cases) {
        const std::string mode = c.mode;
        CAPTURE(mode);
        nv::Host host;
        REQUIRE(host.start(fixture(c.mode, c.startup_ms)));
        REQUIRE(until(host, [&host] { return !host.alive(); }, 15000));
        CHECK(host.failure().find(c.words) != std::string::npos);
        MESSAGE(mode << ": " << host.failure());
        CHECK(host.pending_calls() == 0);
    }
    nv::Host crash;
    REQUIRE(crash.start(fixture("crash")));
    REQUIRE(until(crash, [&crash] { return !crash.alive(); }));
    CHECK(crash.error_tail().find("crashing on purpose") != std::string::npos);
    CHECK(crash.failure().find("crashing on purpose") != std::string::npos);
}

TEST_CASE("a peer that ignores the quit is forced after its grace, and a waiting call is answered with the ending") {
    nv::Host host;
    REQUIRE(host.start(fixture("ignore-quit")));
    REQUIRE(until(host, [&host] { return host.ready(); }));
    bool heard = false;
    std::string words;
    REQUIRE(host.call("zengine_fixture_hold", rpc::params(), [&](const rpc::Response& r) {
        heard = true;
        words = rpc::error_text(r.error);
    }));
    const auto t0 = Clock::now();
    const nv::Child::Ended ended = host.finish(200);
    CHECK(ended.forced);
    CHECK(Clock::now() - t0 < std::chrono::seconds(3));
    CHECK(heard);
    CHECK_FALSE(words.empty());
}

TEST_CASE("a call to a peer that stops answering is refused within its bound, in words") {
    nv::Host host;
    REQUIRE(host.start(fixture("ok")));
    REQUIRE(until(host, [&host] { return host.ready(); }));
    std::string why;
    const auto t0 = Clock::now();
    // The fixture answers an unknown method with an error at once; a bound of zero still reads it
    // or says it timed out -- never waits past its bound.
    std::optional<rpc::Response> r = host.call_now("nvim_unknown", rpc::params(), 0, why);
    CHECK(Clock::now() - t0 < std::chrono::seconds(2));
    if (r.has_value()) {
        CHECK_FALSE(r->error.is_nil());
    } else {
        CHECK(why.find("did not answer within 0 ms") != std::string::npos);
    }
}

} // TEST_SUITE
