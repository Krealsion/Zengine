// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Suite `neovim_live` -- the hosting library against a REAL Neovim (gate `neovim`).
//
// Everything `neovim/` claims about Neovim's behaviour was measured before it was written; this
// suite is where those measurements become repeatable claims on every lane that has a Neovim: the
// start and its failures, byte-exact adoption, the caret and selection round trip, the projection
// of a real screen, typing, the clipboard bridge, the swap guard, and ending.
//
// EVERY NEOVIM HERE IS SANDBOXED: its XDG directories, LOCALAPPDATA and log file point into a
// fresh temporary directory, so no case reads or writes the maker's own Neovim configuration,
// state, swap files or logs. The program is `NEOVIM_PROGRAM`, fixed at configure time
// (`ZENGINE_NEOVIM_PROGRAM`); this suite is not registered at all without one.

#include "doctest.h"

#include "neovim/document.hpp"
#include "neovim/host.hpp"
#include "neovim/keys.hpp"
#include "neovim/launch.hpp"
#include "neovim/lua.hpp"
#include "neovim/projection.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

namespace nv = zengine::neovim;
namespace mp = zengine::neovim::msgpack;
namespace fs = std::filesystem;

using Clock = std::chrono::steady_clock;

long long process_id() {
#if defined(_WIN32)
    return static_cast<long long>(_getpid());
#else
    return static_cast<long long>(::getpid());
#endif
}

/// A FRESH DIRECTORY this case's Neovims live in, removed afterwards.
struct Sandbox {
    fs::path root;

    explicit Sandbox(const std::string& name) {
        static int counter = 0;
        root = fs::temp_directory_path() /
               ("zengine-neovim-live-" + std::to_string(process_id()) + "-" + name + "-" +
                std::to_string(++counter));
        fs::remove_all(root);
        fs::create_directories(root);
    }
    ~Sandbox() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    nv::LaunchSpec spec(const std::string& profile = "clean",
                        nv::UiMode mode = nv::UiMode::Embedded) const {
        nv::LaunchChoice choice;
        choice.program = NEOVIM_PROGRAM;
        choice.profile = profile;
        nv::LaunchSpec s = nv::launch_spec(choice, mode, std::string(), root.string());
        for (const char* v : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME",
                              "XDG_CACHE_HOME", "XDG_RUNTIME_DIR", "LOCALAPPDATA"}) {
            const fs::path d = root / "env" / v;
            fs::create_directories(d);
            s.set_env.emplace_back(v, d.string());
        }
        s.set_env.emplace_back("NVIM_LOG_FILE", (root / "env" / "nvim.log").string());
        return s;
    }

    std::string path(const std::string& name) const { return (root / name).generic_string(); }
};

void write_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/// Pump until `done` holds or `ms` passes, accumulating what was observed.
template <class Done>
bool until(nv::Host& host, Done done, int ms = 10000, nv::Observed* seen = nullptr) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        nv::Observed o = host.pump();
        if (seen != nullptr) {
            seen->merge(std::move(o));
        }
        if (done()) {
            return true;
        }
        if (Clock::now() >= deadline) {
            return false;
        }
        (void)host.wait(10);
    }
}

bool started(nv::Host& host, const nv::LaunchSpec& spec, std::int64_t rows = 20,
             std::int64_t columns = 60, bool attach = true) {
    nv::Host::Options o;
    o.launch = spec;
    o.rows = rows;
    o.columns = columns;
    o.attach_ui = attach;
    if (!host.start(o)) {
        return false;
    }
    return until(host, [&host] { return !host.alive() || host.ready(); }) && host.ready();
}

mp::Value strs(const std::vector<std::string>& lines) {
    mp::Value::Array a;
    for (const std::string& l : lines) {
        a.push_back(mp::Value::str(l));
    }
    return mp::Value::array(std::move(a));
}

std::optional<mp::Value> lua(nv::Host& host, const char* chunk, mp::Value args, std::string& why,
                             int ms = 5000) {
    std::optional<nv::rpc::Response> r = host.call_now(
        "nvim_exec_lua", nv::rpc::params(mp::Value::str(chunk), std::move(args)), ms, why);
    if (!r.has_value()) {
        return std::nullopt;
    }
    if (!r->error.is_nil()) {
        why = nv::rpc::error_text(r->error);
        return std::nullopt;
    }
    return r->result;
}

bool command(nv::Host& host, const std::string& text) {
    std::string why;
    std::optional<nv::rpc::Response> r =
        host.call_now("nvim_command", nv::rpc::params(mp::Value::str(text)), 5000, why);
    return r.has_value() && r->error.is_nil();
}

/// ADOPT file bytes as the transfer would: Neovim's lines, its convention, its final newline.
std::optional<mp::Value> adopt(nv::Host& host, const std::string& path, const std::string& text,
                               bool modified, std::string& why) {
    const nv::NeovimText t = nv::neovim_text(text);
    return lua(host, nv::lua::kAdopt,
               nv::rpc::params(mp::Value::str(path), strs(t.lines), mp::Value::boolean(t.dos),
                               mp::Value::boolean(t.final_newline), mp::Value::boolean(modified)),
               why);
}

struct Exported {
    std::vector<std::string> lines;
    bool final_newline = false;
    bool dos = false;
    bool modified = false;
    nv::NeovimPosition at;
    mp::Value raw;
};

std::optional<Exported> exported(nv::Host& host, std::string& why) {
    std::optional<mp::Value> v = lua(host, nv::lua::kExport, nv::rpc::params(), why);
    if (!v.has_value()) {
        return std::nullopt;
    }
    Exported e;
    e.raw = *v;
    for (const mp::Value& l : v->get("lines")->as_array()) {
        e.lines.push_back(l.as_str());
    }
    e.dos = v->get("fileformat")->as_str() == "dos";
    e.final_newline = nv::writes_final_newline(e.lines, v->get("eol")->as_bool(), v->get("fixeol")->as_bool(),
                                               v->get("binary")->as_bool(), v->get("bytes")->as_int());
    e.modified = v->get("modified")->as_bool();
    e.at.mode = v->get("mode")->as_str();
    e.at.cursor = nv::Pos{v->get("cursor")->at(0).as_int(), v->get("cursor")->at(1).as_int()};
    e.at.vstart = nv::Pos{v->get("vstart")->at(0).as_int(), v->get("vstart")->at(1).as_int()};
    return e;
}

} // namespace

TEST_SUITE("neovim_live") {

TEST_CASE("a Neovim starts, names its version, attaches as the pane's interface and is ready") {
    Sandbox box("start");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    CHECK(host.version().api_level >= nv::kMinApiLevel);
    CHECK(host.channel() > 0);
    CHECK(host.clipboard_installed());
    REQUIRE(until(host, [&host] { return host.grid().flushes() > 0; }));
    CHECK(host.grid().rows() == 20);
    CHECK(host.grid().columns() == 60);
    MESSAGE("Neovim " << host.version().text() << " api_level " << host.version().api_level);
    const zengine::neovim::Child::Ended ended = host.finish(nv::kQuitGraceMs);
    CHECK_FALSE(ended.forced);
    CHECK(ended.status == 0);
    CHECK(host.phase() == nv::Host::Phase::Ended);
}

TEST_CASE("a program that is not there fails the start in words, and nothing runs") {
    Sandbox box("missing");
    nv::LaunchSpec spec = box.spec();
    spec.program = (box.root / "no-such-dir" / "nvim").string();
    nv::Host host;
    nv::Host::Options o;
    o.launch = spec;
    CHECK_FALSE(host.start(o));
    CHECK(host.phase() == nv::Host::Phase::Failed);
    CHECK(host.failure().find("Neovim is not available") == 0);
    CHECK(host.failure().find("was not found") != std::string::npos);
}

TEST_CASE("a configuration that stops at a prompt fails the start with Neovim's own words") {
    Sandbox box("prompt");
    const std::string init = box.path("broken-init.lua");
    write_bytes(init, "error('zengine live: this configuration is broken on purpose')\n");
    nv::Host host;
    nv::Host::Options o;
    o.launch = box.spec(init);
    o.rows = 14;
    o.columns = 90;
    REQUIRE(host.start(o));
    REQUIRE(until(host, [&host] { return !host.alive(); }, 20000));
    CHECK(host.phase() == nv::Host::Phase::Failed);
    CHECK(host.failure().find("stopped at a prompt") != std::string::npos);
    CHECK(host.failure().find("broken on purpose") != std::string::npos);
    MESSAGE(host.failure());
}

TEST_CASE("adopting a document writes back byte-exact: LF, CRLF, final newline or not, empty") {
    Sandbox box("adopt");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"lf-nofinal.txt", "alpha\n\tbeta\n\ngamma"},
        {"crlf-final.txt", "one\r\ntwo\r\n\r\nthree\r\n"},
        {"lf-final.txt", "x\ny\n"},
        {"empty.txt", ""},
        {"newline.txt", "\n"},
        {"crlf-nofinal.txt", "a\r\nb"},
    };
    for (const auto& [name, bytes] : cases) {
        CAPTURE(name);
        const std::string path = box.path(name);
        for (const bool modified : {false, true}) {
            CAPTURE(modified);
            write_bytes(path, modified ? std::string("on disk before\n") : bytes);
            std::string why;
            std::optional<mp::Value> r = adopt(host, path, bytes, modified, why);
            REQUIRE_MESSAGE(r.has_value(), why);
            CHECK(r->get("refused") == nullptr);
            std::optional<Exported> e = exported(host, why);
            REQUIRE_MESSAGE(e.has_value(), why);
            CHECK(e->modified == modified);
            CHECK(nv::file_bytes(e->lines, e->dos, e->final_newline) == bytes);
            REQUIRE(command(host, "write"));
            CHECK(read_bytes(path) == bytes);
            REQUIRE(command(host, "bwipeout!"));
        }
    }
}

TEST_CASE("a modified adoption stays modified after an edit is undone") {
    Sandbox box("undo");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("doc.txt");
    write_bytes(path, "saved\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "saved\nunsaved line\n", true, why).has_value(), why);
    REQUIRE(host.input("ggix<Esc>u"));
    std::optional<Exported> e = exported(host, why);
    REQUIRE_MESSAGE(e.has_value(), why);
    CHECK(e->modified);
    CHECK(nv::file_bytes(e->lines, e->dos, e->final_newline) == "saved\nunsaved line\n");
    // ...and an undo past the adoption finds nothing: it left no undo step.
    REQUIRE(host.input("uuu"));
    e = exported(host, why);
    REQUIRE(e.has_value());
    CHECK(nv::file_bytes(e->lines, e->dos, e->final_newline) == "saved\nunsaved line\n");
}

TEST_CASE("a transferred caret and selection land where the standard model had them, and come back") {
    Sandbox box("positions");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("positions.txt");
    const std::string text = "alpha beta\n\tgamma\n\ndelta\n";
    write_bytes(path, text);
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, text, false, why).has_value(), why);
    const nv::NeovimText t = nv::neovim_text(text);
    std::vector<nv::Pos> positions;
    const std::vector<std::string> standard = {"alpha beta", "\tgamma", "", "delta", ""};
    for (std::int64_t r = 0; r < static_cast<std::int64_t>(standard.size()); ++r) {
        for (std::int64_t b = 0; b <= static_cast<std::int64_t>(standard[static_cast<std::size_t>(r)].size()); ++b) {
            positions.push_back(nv::Pos{r, b});
        }
    }
    int exact = 0;
    int adjusted = 0;
    int cases = 0;
    for (const nv::Pos& anchor : positions) {
        for (const nv::Pos& caret : positions) {
            if (!(anchor == caret) && (anchor.row + caret.row + anchor.byte + caret.byte) % 3 != 0) {
                continue;
            }
            ++cases;
            const nv::Placement p = nv::place(t.lines, t.final_newline, anchor, caret);
            REQUIRE(host.input(nv::placement_keys(p)));
            std::optional<Exported> e = exported(host, why);
            REQUIRE_MESSAGE(e.has_value(), why);
            const nv::Carried back = nv::carry(e->lines, e->final_newline, e->at);
            if (back.anchor == anchor && back.caret == caret) {
                ++exact;
                continue;
            }
            CAPTURE(anchor.row);
            CAPTURE(anchor.byte);
            CAPTURE(caret.row);
            CAPTURE(caret.byte);
            CAPTURE(e->at.mode);
            CHECK_MESSAGE(p.adjusted, "a position came back different without an adjustment");
            ++adjusted;
        }
    }
    MESSAGE(cases << " cases: " << exact << " exact, " << adjusted << " adjusted");
    CHECK(exact > 0);
    CHECK(adjusted <= 12);
    REQUIRE(host.input("<Esc>"));
}

TEST_CASE("the screen projects: a block cursor's cell, an Insert caret, Visual through the cursor, the status line") {
    Sandbox box("projection");
    nv::Host host;
    REQUIRE(started(host, box.spec(), 12, 40));
    const std::string path = box.path("screen.txt");
    const std::string text = "first line\nsecond line\nthird\n";
    write_bytes(path, text);
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, text, false, why).has_value(), why);
    const auto settle = [&host](const auto& done) {
        return until(host, [&] {
            (void)host.pump();
            return done();
        });
    };
    REQUIRE(host.input("<Esc>gg0w"));
    REQUIRE(settle([&host] {
        const nv::Screen s = nv::project(host.grid(), host.visual_kind());
        return s.source == nv::SelectionSource::Block && s.sel_begin_col == 6;
    }));
    nv::Screen s = nv::project(host.grid(), host.visual_kind());
    CHECK(s.rows.size() == 12);
    CHECK(s.rows[0].text.rfind("first line", 0) == 0);
    CHECK(s.caret_row == -1);
    CHECK(s.sel_begin_row == 0);
    CHECK(s.sel_end_col == 7);
    bool chrome = false;
    for (const nv::Screen::Row& r : s.rows) {
        chrome = chrome || r.kind == nv::RowKind::Chrome;
    }
    CHECK(chrome);
    CHECK(s.rows[4].kind == nv::RowKind::Muted); // a `~` row past the end of the buffer

    REQUIRE(host.input("i"));
    REQUIRE(settle([&host] { return nv::project(host.grid(), host.visual_kind()).caret_row == 0; }));
    s = nv::project(host.grid(), host.visual_kind());
    CHECK(s.caret_col == 6);
    CHECK(s.source == nv::SelectionSource::None);

    // ONE INPUT, SEVERAL FLUSHES: the picture is read when it shows the whole motion, not the
    // first flush after it.
    REQUIRE(host.input("<Esc>gg0vjl"));
    REQUIRE(settle([&host] {
        const nv::Screen v = nv::project(host.grid(), host.visual_kind());
        return v.source == nv::SelectionSource::Visual && v.sel_end_row == 1 && v.sel_end_col == 2;
    }));
    s = nv::project(host.grid(), host.visual_kind());
    CHECK(s.sel_begin_row == 0);
    CHECK(s.sel_begin_col == 0);
    CHECK(s.sel_end_row == 1);
    CHECK(s.sel_end_col == 2); // through the cursor cell at column 1
    REQUIRE(host.input("<Esc>"));
}

TEST_CASE("typing reaches the buffer, and the owner hears the document change") {
    Sandbox box("typing");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("typed.txt");
    write_bytes(path, "abc\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "abc\n", false, why).has_value(), why);
    nv::Observed seen;
    REQUIRE(host.input("A" + nv::text_input("<x>") + "<Esc>"));
    REQUIRE(until(host, [&host] { return host.doc().modified; }, 10000, &seen));
    CHECK(seen.doc_changed);
    CHECK(host.doc().name.find("typed.txt") != std::string::npos);
    std::optional<Exported> e = exported(host, why);
    REQUIRE(e.has_value());
    CHECK(e->lines.front() == "abc<x>");
    const std::int64_t tick = host.doc().tick;
    REQUIRE(command(host, "write"));
    REQUIRE(until(host, [&host] { return !host.doc().modified; }));
    CHECK(host.doc().tick >= tick);
    CHECK(read_bytes(path) == "abc<x>\n");
}

TEST_CASE("preparing a file hidden leaves the heard document the one Neovim shows") {
    // LOADING A BUFFER NO WINDOW SHOWS RUNS ITS AUTOCOMMANDS IN NEOVIM'S AUTOCOMMAND WINDOW, where
    // the prepared buffer is current (measured: BufEnter fires there). What the owner hears must
    // still be the buffer the maker is editing -- or an open in flight moves the document's claim
    // and aborts itself.
    Sandbox box("prepare-hidden");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string current = box.path("current.txt");
    const std::string hidden = box.path("hidden.txt");
    write_bytes(current, "one\n");
    write_bytes(hidden, "two\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, current, "one\nmore\n", true, why).has_value(), why);
    REQUIRE(until(host, [&host] { return host.doc().modified && host.doc().name.find("current.txt") != std::string::npos; }));
    const std::int64_t tick = host.doc().tick;
    nv::Observed seen = host.pump();
    std::optional<mp::Value> prepared =
        lua(host, nv::lua::kPrepare, nv::rpc::params(mp::Value::str(hidden), mp::Value::integer(5)), why);
    REQUIRE_MESSAGE(prepared.has_value(), why);
    REQUIRE(prepared->get("refused") == nullptr);
    CHECK(prepared->get("name")->as_str().find("hidden.txt") != std::string::npos);
    std::string mode_why;
    REQUIRE(host.call_now("nvim_get_mode", nv::rpc::params(), 2000, mode_why).has_value());
    seen.merge(host.pump());
    CHECK(host.doc().name.find("current.txt") != std::string::npos);
    CHECK(host.doc().modified);
    CHECK(host.doc().tick == tick);
}

TEST_CASE("a copy to + is heard, and a paste Neovim asks for is answered late and lands") {
    Sandbox box("clipboard");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("clip.txt");
    write_bytes(path, "copy me\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "copy me\n", false, why).has_value(), why);
    nv::Observed seen;
    REQUIRE(host.input("\"+yy"));
    REQUIRE(until(host, [&seen] { return !seen.copies.empty(); }, 10000, &seen));
    REQUIRE(seen.copies.front().lines.size() >= 1);
    CHECK(seen.copies.front().lines.front() == "copy me");
    CHECK(seen.copies.front().regtype == "V");
    nv::Observed asked;
    REQUIRE(host.input("\"+P"));
    REQUIRE(until(host, [&asked] { return !asked.paste_requests.empty(); }, 10000, &asked));
    // THE ANSWER COMES LATER, as the Skin's does: nothing is waited on meanwhile.
    for (int i = 0; i < 5; ++i) {
        (void)host.pump();
        (void)host.wait(20);
    }
    REQUIRE(host.answer_paste(asked.paste_requests.front(), {"from the medium"}, "v"));
    REQUIRE(until(host, [&] {
        std::string w;
        std::optional<Exported> e = exported(host, w);
        return e.has_value() && e->lines.front().rfind("from the medium", 0) == 0;
    }));
}

TEST_CASE("a file another Neovim holds is refused by its swap file, and nothing prompts") {
    Sandbox box("swap");
    const std::string path = box.path("held.txt");
    write_bytes(path, "held elsewhere\n");
    nv::Host holder;
    REQUIRE(started(holder, box.spec()));
    REQUIRE(command(holder, "edit " + path));
    REQUIRE(holder.input("ione more <Esc>"));
    REQUIRE(until(holder, [&holder] { return holder.doc().modified; }));
    nv::Host second;
    REQUIRE(started(second, box.spec()));
    std::string why;
    std::optional<mp::Value> r = adopt(second, path, "held elsewhere\n", false, why);
    REQUIRE_MESSAGE(r.has_value(), why);
    REQUIRE(r->get("refused") != nullptr);
    CHECK(r->get("refused")->as_str() == "swap");
    CHECK(r->get("why")->as_str().find("E325") != std::string::npos);
    std::string mode_why;
    std::optional<nv::rpc::Response> mode = second.call_now("nvim_get_mode", nv::rpc::params(), 2000, mode_why);
    REQUIRE(mode.has_value());
    CHECK_FALSE(mode->result.get("blocking")->as_bool());
}

TEST_CASE("ending asks first: a quit Neovim exits by itself and leaves no swap file behind") {
    Sandbox box("finish");
    const std::string path = box.path("ending.txt");
    write_bytes(path, "text\n");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "text\nmore\n", true, why).has_value(), why);
    REQUIRE(host.input("Gix<Esc>"));
    REQUIRE(until(host, [&host] { return host.doc().modified; }));
    const fs::path swaps = box.root / "env" / "XDG_STATE_HOME";
    const auto swap_files = [&swaps] {
        int n = 0;
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(swaps, ec)) {
            const std::string name = entry.path().filename().string();
            n += name.size() > 4 && name.find(".sw") != std::string::npos ? 1 : 0;
        }
        return n;
    };
    CHECK(swap_files() >= 1);
    const nv::Child::Ended ended = host.finish(nv::kQuitGraceMs);
    CHECK_FALSE(ended.forced);
    CHECK(swap_files() == 0);
}

// ---- Transfers: the selection taken, material dropped as data, a location's cursor --------------

namespace {

/// The selection's text as the module takes it: its lines, joined, and its own final break.
std::string taken(const mp::Value& v) {
    std::string out;
    const mp::Value::Array& lines = v.get("lines")->as_array();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out += (i > 0 ? "\n" : "") + lines[i].as_str();
    }
    return v.get("newline")->as_bool() ? out + "\n" : out;
}

/// WHAT NEOVIM'S OWN YANK TAKES of the selection standing now, into a register nobody else uses;
/// it ends the selection, as a yank does.
std::string yanked(nv::Host& host) {
    std::string why;
    const std::optional<mp::Value> r = lua(host, "vim.cmd('normal! \"zy') return vim.fn.getreg('z')", nv::rpc::params(), why);
    REQUIRE_MESSAGE(r.has_value(), why);
    return r->as_str();
}

/// One screen row, cell by cell, as the pane's grid holds it (what a drop's row check compares).
std::string grid_row(const nv::Host& host, std::int64_t row) {
    std::string out;
    for (std::int64_t c = 0; c < host.grid().columns(); ++c) {
        out += host.grid().at(row, c).text;
    }
    return out;
}

/// Where buffer position (1-based line, 1-based byte) is on the screen, 0-based, by Neovim's map.
std::pair<std::int64_t, std::int64_t> cell_of(nv::Host& host, std::int64_t line, std::int64_t byte) {
    std::string why;
    const std::optional<mp::Value> p = lua(host, "local p = vim.fn.screenpos(0, ...) return { p.row, p.col }",
                                           nv::rpc::params(mp::Value::integer(line), mp::Value::integer(byte)), why);
    REQUIRE_MESSAGE(p.has_value(), why);
    return {p->at(0).as_int() - 1, p->at(1).as_int() - 1};
}

std::optional<mp::Value> drop(nv::Host& host, std::int64_t line, std::int64_t byte,
                              const std::vector<std::string>& lines, bool check_row = true) {
    std::string why;
    const std::optional<mp::Value> facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    REQUIRE_MESSAGE(facts.has_value(), why);
    const auto [row, col] = cell_of(host, line, byte);
    REQUIRE(until(host, [&host] { return host.grid().flushes() > 0; }));
    (void)until(host, [] { return false; }, 60); // let the screen the drop is aimed at arrive
    const std::optional<mp::Value> r =
        lua(host, nv::lua::kDrop,
            nv::rpc::params(mp::Value::integer(facts->get("buf")->as_int()), mp::Value::integer(facts->get("tick")->as_int()),
                            mp::Value::integer(row), mp::Value::integer(col), strs(lines),
                            check_row ? mp::Value::str(grid_row(host, row)) : mp::Value::nil()),
            why);
    REQUIRE_MESSAGE(r.has_value(), why);
    return r;
}

std::vector<std::string> buffer(nv::Host& host) {
    std::string why;
    const std::optional<mp::Value> r = lua(host, "return vim.api.nvim_buf_get_lines(0, 0, -1, true)", nv::rpc::params(), why);
    REQUIRE_MESSAGE(r.has_value(), why);
    std::vector<std::string> out;
    for (const mp::Value& l : r->as_array()) {
        out.push_back(l.as_str());
    }
    return out;
}

std::string mode_now(nv::Host& host) {
    std::string why;
    const std::optional<nv::rpc::Response> m = host.call_now("nvim_get_mode", nv::rpc::params(), 5000, why);
    REQUIRE(m.has_value());
    return m->result.get("mode")->as_str();
}

} // namespace

TEST_CASE("the selection taken for a copy is exactly what Neovim's own yank takes, for every kind of Visual selection, and taking it moves nothing") {
    Sandbox box("selection");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("sel.txt");
    const std::string text = "alpha beta\n\tgamma\n\ndelta\ncaf\xc3\xa9 x\n";
    write_bytes(path, text);
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, text, false, why).has_value(), why);
    const std::vector<std::string> selections = {
        "gg0lvl", "gg0lvj", "gg0lv$", "gg0lv$j", "gg0vj$", "3Gv", "3Gvj", "gg0Vj", "GV", "4Gjv$",
        "gg0<C-v>jj$", "gg0l<C-v>jl", "gg0l<C-v>jjl", "gg05lvh", "gg0lgh<C-o>l"};
    for (const std::string& keys : selections) {
        CAPTURE(keys);
        REQUIRE(host.input("<Esc>" + keys));
        const std::string before = mode_now(host);
        const std::optional<mp::Value> v = lua(host, nv::lua::kSelection, nv::rpc::params(), why);
        REQUIRE_MESSAGE(v.has_value(), why);
        REQUIRE_MESSAGE(v->get("refused") == nullptr, (v->get("why") ? v->get("why")->as_str() : std::string()));
        CHECK(mode_now(host) == before); // nothing moved: the mode, and so the selection, still stand
        const std::string mine = taken(*v);
        if (before[0] == 's' || before[0] == 'S' || before[0] == '\x13') {
            REQUIRE(host.input("<C-g>")); // a Select-mode selection is yanked from Visual
        }
        CHECK(mine == yanked(host));
    }
    REQUIRE(host.input("<Esc>"));
    // A NUL inside the selection refuses the copy rather than turning into a line break.
    REQUIRE(command(host, "call setline(1, \"a\\nb\")"));
    REQUIRE(host.input("gg0v$"));
    const std::optional<mp::Value> nul = lua(host, nv::lua::kSelection, nv::rpc::params(), why);
    REQUIRE(nul.has_value());
    REQUIRE(nul->get("refused") != nullptr);
    CHECK(nul->get("refused")->as_str() == "nul");
    REQUIRE(host.input("<Esc>"));
    const std::optional<mp::Value> none = lua(host, nv::lua::kSelection, nv::rpc::params(), why);
    REQUIRE(none.has_value());
    CHECK(none->get("refused")->as_str() == "mode"); // no selection, no copy: not the line, not the word
}

TEST_CASE("a drop is data at the aimed cell -- Escape, key notation and a colon stay text -- and one undo takes exactly the drop back") {
    Sandbox box("drop");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("drop.txt");
    write_bytes(path, "one two\n\tthree\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "one two\n\tthree\n", false, why).has_value(), why);
    REQUIRE(host.input("gg0Ax<Esc>")); // a typed change the drop must not join
    std::optional<mp::Value> r = drop(host, 1, 5, {"\x1b:qa!<CR><Esc>", "second"});
    REQUIRE(r->get("refused") == nullptr);
    CHECK(buffer(host) == std::vector<std::string>{"one \x1b:qa!<CR><Esc>", "secondtwox", "\tthree"});
    CHECK(host.alive()); // nothing it held was run
    REQUIRE(host.input("u"));
    CHECK(buffer(host) == std::vector<std::string>{"one twox", "\tthree"});
    REQUIRE(host.input("<C-r>"));
    CHECK(buffer(host) == std::vector<std::string>{"one \x1b:qa!<CR><Esc>", "secondtwox", "\tthree"});
    REQUIRE(host.input("uu"));
    CHECK(buffer(host) == std::vector<std::string>{"one two", "\tthree"});
    // A drop on the tab's cells lands before the tab; one past a line's text lands at its end.
    r = drop(host, 2, 1, {"T"});
    CHECK(buffer(host)[1] == "T\tthree");
    const auto [row, col] = cell_of(host, 1, 7);
    std::string ignored;
    const std::optional<mp::Value> facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), ignored);
    (void)until(host, [] { return false; }, 60);
    r = lua(host, nv::lua::kDrop,
            nv::rpc::params(mp::Value::integer(facts->get("buf")->as_int()), mp::Value::integer(facts->get("tick")->as_int()),
                            mp::Value::integer(row), mp::Value::integer(col + 5), strs({"!"}), mp::Value::nil()),
            ignored);
    REQUIRE(r.has_value());
    if (r->get("why") != nullptr) MESSAGE("past-end drop: " << r->get("why")->as_str());
    CHECK(buffer(host)[0] == "one two!");
    CHECK_FALSE(host.doc().name.empty());
    CHECK(read_bytes(path) == "one two\n\tthree\n"); // nothing was written
}

TEST_CASE("a drop replaces a charwise or linewise highlight only when dropped onto it, and refuses beside it, on a block, and in a mode that is not editing") {
    Sandbox box("drop-modes");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("modes.txt");
    write_bytes(path, "keep this\nand this\nlast\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "keep this\nand this\nlast\n", false, why).has_value(), why);
    REQUIRE(host.input("gg0ve"));
    std::optional<mp::Value> r = drop(host, 2, 2, {"X"});
    CHECK(r->get("refused")->as_str() == "visual");
    CHECK(buffer(host)[1] == "and this");
    r = drop(host, 1, 2, {"KEPT"});
    REQUIRE(r->get("refused") == nullptr);
    CHECK(buffer(host)[0] == "KEPT this");
    CHECK(mode_now(host) == "n");
    REQUIRE(host.input("2GVj"));
    r = drop(host, 3, 1, {"one", "two", ""});
    REQUIRE(r->get("refused") == nullptr);
    CHECK(buffer(host) == std::vector<std::string>{"KEPT this", "one", "two"});
    REQUIRE(host.input("gg0<C-v>j"));
    r = drop(host, 1, 1, {"B"});
    CHECK(r->get("refused")->as_str() == "block");
    REQUIRE(host.input("<Esc>:"));
    r = drop(host, 1, 1, {"C"}, false);
    if (r->get("why") != nullptr) MESSAGE("command-line drop: " << r->get("why")->as_str() << " mode " << mode_now(host));
    CHECK(r->get("refused")->as_str() == "mode");
    REQUIRE(host.input("<Esc>"));
    CHECK(buffer(host) == std::vector<std::string>{"KEPT this", "one", "two"});
}

TEST_CASE("a drop in Insert mode is its own undo step and leaves Insert mode as it was; a stale buffer or screen refuses it") {
    Sandbox box("drop-insert");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("insert.txt");
    write_bytes(path, "abc\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "abc\n", false, why).has_value(), why);
    REQUIRE(host.input("Aty"));
    std::optional<mp::Value> r = drop(host, 1, 1, {"D"});
    REQUIRE(r->get("refused") == nullptr);
    CHECK(mode_now(host) == "i");
    REQUIRE(host.input("<Esc>u"));
    CHECK(buffer(host)[0] == "abcty");
    // Stale: the buffer changed after the drop was aimed.
    const std::optional<mp::Value> facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    REQUIRE(host.input("ix<Esc>"));
    r = lua(host, nv::lua::kDrop,
            nv::rpc::params(mp::Value::integer(facts->get("buf")->as_int()), mp::Value::integer(facts->get("tick")->as_int()),
                            mp::Value::integer(0), mp::Value::integer(0), strs({"S"}), mp::Value::nil()),
            why);
    REQUIRE(r.has_value());
    CHECK(r->get("refused")->as_str() == "moved");
    // Stale: the screen row no longer reads as the hand saw it.
    r = drop(host, 1, 1, {"Z"}, false);
    const std::optional<mp::Value> now = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    const std::optional<mp::Value> stale = lua(host, nv::lua::kDrop,
        nv::rpc::params(mp::Value::integer(now->get("buf")->as_int()), mp::Value::integer(now->get("tick")->as_int()),
                        mp::Value::integer(0), mp::Value::integer(0), strs({"S"}), mp::Value::str("not what is shown")),
        why);
    REQUIRE(stale.has_value());
    CHECK(stale->get("refused")->as_str() == "moved");
}

TEST_CASE("a drop into a read-only or unmodifiable buffer, or while Neovim waits for the rest of a command, is refused and changes nothing") {
    Sandbox box("drop-refused");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("ro.txt");
    write_bytes(path, "abc\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "abc\n", false, why).has_value(), why);
    REQUIRE(host.input(":setlocal readonly<CR>"));
    std::optional<mp::Value> r = drop(host, 1, 1, {"X"});
    CHECK(r->get("refused")->as_str() == "readonly");
    REQUIRE(host.input(":setlocal noreadonly nomodifiable<CR>"));
    r = drop(host, 1, 1, {"X"});
    CHECK(r->get("refused")->as_str() == "readonly");
    // AN OPERATOR WAITING FOR ITS MOTION: the drop is refused for the mode (measured). Were it held
    // instead it would run when the wait ends -- an unanswered drop is not a refusal -- so the
    // buffer is read after the Escape, where such a drop would show.
    REQUIRE(host.input(":setlocal modifiable<CR>d"));
    REQUIRE(until(host, [&host] { return mode_now(host) == "no"; }));
    const std::optional<mp::Value> facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    if (facts.has_value()) {
        r = lua(host, nv::lua::kDrop,
                nv::rpc::params(mp::Value::integer(facts->get("buf")->as_int()), mp::Value::integer(facts->get("tick")->as_int()),
                                mp::Value::integer(0), mp::Value::integer(0), strs({"X"}), mp::Value::nil()),
                why);
        CHECK((!r.has_value() || r->get("refused") != nullptr));
    } else {
        CHECK(why.find("waiting") != std::string::npos);
    }
    REQUIRE(host.input("<Esc>"));
    CHECK(buffer(host) == std::vector<std::string>{"abc"});
}

TEST_CASE("a location's cursor is placed only in the buffer it names, unchanged, on a line that still reads as it did") {
    Sandbox box("locate");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("loc.txt");
    write_bytes(path, "first\nsecond line\nthird\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "first\nsecond line\nthird\n", false, why).has_value(), why);
    REQUIRE(host.input("2G3|"));
    const std::optional<mp::Value> here = lua(host, nv::lua::kLocation, nv::rpc::params(), why);
    REQUIRE(here.has_value());
    CHECK(here->get("line")->as_int() == 2);
    CHECK(here->get("col")->as_int() == 3);
    CHECK(here->get("text")->as_str() == "second line");
    REQUIRE(host.input("gg"));
    const std::int64_t buf = here->get("buf")->as_int();
    std::int64_t tick = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why)->get("tick")->as_int();
    std::optional<mp::Value> placed = lua(host, nv::lua::kLocate,
        nv::rpc::params(mp::Value::integer(buf), mp::Value::integer(tick), mp::Value::integer(2), mp::Value::integer(3),
                        mp::Value::str("second line")), why);
    REQUIRE(placed.has_value());
    CHECK(placed->get("placed")->as_bool());
    CHECK(lua(host, "return vim.api.nvim_win_get_cursor(0)", nv::rpc::params(), why)->at(0).as_int() == 2);
    REQUIRE(host.input("2Gccchanged<Esc>gg"));
    tick = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why)->get("tick")->as_int();
    placed = lua(host, nv::lua::kLocate,
        nv::rpc::params(mp::Value::integer(buf), mp::Value::integer(tick), mp::Value::integer(2), mp::Value::integer(3),
                        mp::Value::str("second line")), why);
    CHECK_FALSE(placed->get("placed")->as_bool());
    CHECK(placed->get("why")->as_str().find("no longer reads") != std::string::npos);
    CHECK(lua(host, "return vim.api.nvim_win_get_cursor(0)", nv::rpc::params(), why)->at(0).as_int() == 1);
}

TEST_CASE("a location's saved line must still read exactly while it was whole, and begin the line once the bound cut it; no saved line checks nothing, and a refusal moves nothing") {
    Sandbox box("locate-whole");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("whole.txt");
    const std::string capped(240, 'a'); // the observation's bound, `source-transfer`'s kMaxLineText
    const std::string text = "first\nsecond line now changed\nthirD\n" + capped + "0123456789\n";
    write_bytes(path, text);
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, text, false, why).has_value(), why);
    const std::int64_t buf = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why)->get("buf")->as_int();
    const std::int64_t tick = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why)->get("tick")->as_int();
    const auto locate = [&](std::int64_t line, mp::Value saved, bool whole) {
        const std::optional<mp::Value> r = lua(host, nv::lua::kLocate,
            nv::rpc::params(mp::Value::integer(buf), mp::Value::integer(tick), mp::Value::integer(line),
                            mp::Value::integer(2), std::move(saved), mp::Value::boolean(whole)), why);
        REQUIRE_MESSAGE(r.has_value(), why);
        return r->get("placed")->as_bool();
    };
    const auto cursor_line = [&] { return lua(host, "return vim.api.nvim_win_get_cursor(0)", nv::rpc::params(), why)->at(0).as_int(); };
    // A WHOLE SAVED LINE THAT GAINED TEXT, OR WAS REWRITTEN, HAS CHANGED: nothing moves, not even the
    // cursor, and the buffer is as it was.
    CHECK_FALSE(locate(2, mp::Value::str("second line"), true));
    CHECK_FALSE(locate(3, mp::Value::str("third"), true));
    // ...AND A CALLER THAT DOES NOT SAY WHETHER THE LINE WAS CUT GETS THE STRICT RULE, not the prefix.
    const std::optional<mp::Value> unsaid = lua(host, nv::lua::kLocate,
        nv::rpc::params(mp::Value::integer(buf), mp::Value::integer(tick), mp::Value::integer(2),
                        mp::Value::integer(2), mp::Value::str("second line")), why);
    REQUIRE_MESSAGE(unsaid.has_value(), why);
    CHECK_FALSE(unsaid->get("placed")->as_bool());
    CHECK(cursor_line() == 1);
    CHECK(lua(host, nv::lua::kDocFacts, nv::rpc::params(), why)->get("tick")->as_int() == tick);
    CHECK(buffer(host) == std::vector<std::string>{"first", "second line now changed", "thirD", capped + "0123456789"});
    // ...one still as saved is where the cursor goes.
    CHECK(locate(2, mp::Value::str("second line now changed"), true));
    CHECK(cursor_line() == 2);
    // A CAPPED ONE IS A PREFIX: the line must begin with it, and what follows the bound is not proved.
    CHECK(locate(4, mp::Value::str(capped), false));
    CHECK(cursor_line() == 4);
    CHECK_FALSE(locate(4, mp::Value::str("b" + capped.substr(1)), false));
    CHECK(cursor_line() == 4);
    // NO SAVED LINE -- no context, or an empty observation -- says nothing about the line.
    CHECK(locate(1, mp::Value::nil(), false));
    CHECK(cursor_line() == 1);
    CHECK(locate(3, mp::Value::str(""), true));
    CHECK(cursor_line() == 3);
}

TEST_CASE("a change asked while Neovim waits for input is outstanding, not refused: Neovim runs it when the wait ends, after the keys typed with its end, and its answer arrives exactly once") {
    // THE RECORDERS OUTLIVE THE HOST, which answers a callback still waiting when it ends.
    int heard = 0;
    std::optional<nv::rpc::Response> late;
    Sandbox box("held");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("held.txt");
    write_bytes(path, "abc\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "abc\n", false, why).has_value(), why);
    REQUIRE(until(host, [&host] { return host.grid().flushes() > 0; }));
    (void)until(host, [] { return false; }, 60);
    const std::string shown = grid_row(host, 0);
    const auto blocked = [&host] {
        std::string w;
        const std::optional<nv::rpc::Response> m = host.call_now("nvim_get_mode", nv::rpc::params(), 1000, w);
        return m.has_value() && m->result.get("blocking")->as_bool();
    };
    const auto held_drop = [&](const mp::Value& facts, std::string& said) {
        std::optional<nv::rpc::Response> got;
        const nv::Host::Asked asked = host.ask(
            "nvim_exec_lua",
            nv::rpc::params(mp::Value::str(nv::lua::kDrop),
                            nv::rpc::params(mp::Value::integer(facts.get("buf")->as_int()),
                                            mp::Value::integer(facts.get("tick")->as_int()), mp::Value::integer(0),
                                            mp::Value::integer(0), strs({"D"}), mp::Value::str(shown))),
            2000, got, said, [&late, &heard](const nv::rpc::Response& r) {
                late = r;
                ++heard;
            });
        CHECK_FALSE(got.has_value());
        return asked;
    };
    // HELD: an unfinished `g` holds every module call, so the drop is outstanding -- sent, and Neovim's.
    std::optional<mp::Value> facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    REQUIRE(facts.has_value());
    REQUIRE(host.input("g"));
    REQUIRE(until(host, blocked));
    std::string said;
    CHECK(held_drop(*facts, said) == nv::Host::Asked::Outstanding);
    CHECK(said.find("waiting for input") != std::string::npos);
    (void)until(host, [] { return false; }, 300);
    CHECK(heard == 0);
    // ...AND IT RUNS WHEN THE WAIT ENDS: its answer arrives once, the drop is in, and one undo takes
    // exactly it back.
    REQUIRE(host.input("<Esc>"));
    REQUIRE(until(host, [&heard] { return heard == 1; }));
    REQUIRE(late.has_value());
    CHECK(late->error.is_nil());
    CHECK(late->result.get("refused") == nullptr);
    CHECK(buffer(host) == std::vector<std::string>{"Dabc"});
    (void)until(host, [] { return false; }, 200);
    CHECK(heard == 1);
    REQUIRE(host.input("u"));
    CHECK(buffer(host) == std::vector<std::string>{"abc"});
    // AN EDIT TYPED WITH THE ESCAPE comes first, so the held drop finds its buffer changed and refuses
    // when it runs: once, and the buffer holds the edit alone.
    facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    REQUIRE(facts.has_value());
    REQUIRE(host.input("g"));
    REQUIRE(until(host, blocked));
    CHECK(held_drop(*facts, said) == nv::Host::Asked::Outstanding);
    REQUIRE(host.input("<Esc>ix<Esc>"));
    REQUIRE(until(host, [&heard] { return heard == 2; }));
    REQUIRE(late->result.get("refused") != nullptr);
    CHECK(late->result.get("refused")->as_str() == "moved");
    CHECK(buffer(host) == std::vector<std::string>{"xabc"});
    CHECK(read_bytes(path) == "abc\n"); // nothing was written
}

TEST_CASE("a change Neovim holds when it ends is answered exactly once, with the ending as its error") {
    int heard = 0;
    std::optional<nv::rpc::Response> late;
    Sandbox box("held-end");
    nv::Host host;
    REQUIRE(started(host, box.spec()));
    const std::string path = box.path("end.txt");
    write_bytes(path, "abc\n");
    std::string why;
    REQUIRE_MESSAGE(adopt(host, path, "abc\n", false, why).has_value(), why);
    const std::optional<mp::Value> facts = lua(host, nv::lua::kDocFacts, nv::rpc::params(), why);
    REQUIRE(facts.has_value());
    REQUIRE(host.input("g"));
    REQUIRE(until(host, [&host] {
        std::string w;
        const std::optional<nv::rpc::Response> m = host.call_now("nvim_get_mode", nv::rpc::params(), 1000, w);
        return m.has_value() && m->result.get("blocking")->as_bool();
    }));
    std::optional<nv::rpc::Response> got;
    const nv::Host::Asked asked = host.ask(
        "nvim_exec_lua",
        nv::rpc::params(mp::Value::str(nv::lua::kDrop),
                        nv::rpc::params(mp::Value::integer(facts->get("buf")->as_int()),
                                        mp::Value::integer(facts->get("tick")->as_int()), mp::Value::integer(0),
                                        mp::Value::integer(0), strs({"D"}), mp::Value::nil())),
        2000, got, why, [&late, &heard](const nv::rpc::Response& r) {
            late = r;
            ++heard;
        });
    REQUIRE(asked == nv::Host::Asked::Outstanding);
    CHECK(heard == 0);
    (void)host.finish(nv::kQuitGraceMs);
    CHECK_FALSE(host.alive());
    REQUIRE(heard == 1);
    REQUIRE(late.has_value());
    CHECK_FALSE(late->error.is_nil());
    CHECK(nv::rpc::error_text(late->error) == host.failure());
    (void)host.pump();
    (void)host.finish(nv::kQuitGraceMs);
    CHECK(heard == 1);
    CHECK(read_bytes(path) == "abc\n");
}

} // TEST_SUITE
