// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A SCRIPTABLE FAKE NEOVIM for suite `neovim`: speaks msgpack-RPC on stdin/stdout the way
// `nvim --embed` does, well enough to walk `neovim::Host` through a start, and badly on purpose
// in exactly one way per mode. It is not Neovim and claims nothing about Neovim; the live suite
// (`neovim_live`) is where Neovim's own behaviour is pinned.
//
//     neovim-fixture <mode>
//
//   ok          api level 13, attaches, installs the "module", answers `nvim_get_mode`, echoes
//               `nvim_input` back as a `redraw` flush, and exits 0 on `qa!`
//   old         api level 12
//   garbage     writes bytes that are not msgpack-RPC after the first request
//   silent      reads everything and answers nothing
//   prompt      answers the fast requests but never the module; `nvim_get_mode` is `r`, blocking
//   crash       writes a line to stderr and exits 3 after the first request
//   flood       answers the first request with one frame larger than any bound the owner keeps
//   stranger    answers a request id nobody asked
//   ignore-quit ok, except `qa!` is ignored (the owner must force the end)
//
// In every mode but `silent`, a request named `zengine_fixture_hold` is never answered.

#include "neovim/msgpack.hpp"
#include "neovim/rpc.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

namespace mp = zengine::neovim::msgpack;
namespace rpc = zengine::neovim::rpc;

void write_all(const std::string& bytes) {
    std::fwrite(bytes.data(), 1, bytes.size(), stdout);
    std::fflush(stdout);
}

/// WHAT IS THERE NOW, not a full buffer: `fread` would wait for 4096 bytes, and an owner's first
/// request is far smaller than that.
long read_some(char* buf, unsigned size) {
#if defined(_WIN32)
    return static_cast<long>(_read(0, buf, size));
#else
    return static_cast<long>(::read(0, buf, size));
#endif
}

mp::Value api_info(std::int64_t level) {
    mp::Value::Map version;
    version.push_back({mp::Value::str("major"), mp::Value::integer(0)});
    version.push_back({mp::Value::str("minor"), mp::Value::integer(level >= 13 ? 11 : 10)});
    version.push_back({mp::Value::str("patch"), mp::Value::integer(0)});
    version.push_back({mp::Value::str("api_level"), mp::Value::integer(level)});
    version.push_back({mp::Value::str("api_compatible"), mp::Value::integer(0)});
    mp::Value::Map meta;
    meta.push_back({mp::Value::str("version"), mp::Value::map(std::move(version))});
    return rpc::params(mp::Value::integer(1), mp::Value::map(std::move(meta)));
}

std::string response(std::uint32_t id, const mp::Value& error, const mp::Value& result) {
    std::string frame;
    mp::Writer w(frame);
    w.array_header(4);
    w.uinteger(1);
    w.uinteger(id);
    w.value(error);
    w.value(result);
    return frame;
}

/// A redraw that draws `text` on row 0 of a 1x40 grid and flushes.
std::string redraw_of(const std::string& text) {
    std::string frame;
    mp::Writer w(frame);
    w.array_header(3);
    w.uinteger(2);
    w.str("redraw");
    mp::Value::Array batches;
    batches.push_back(rpc::params(mp::Value::str("grid_resize"),
                                  rpc::params(mp::Value::integer(1), mp::Value::integer(40),
                                              mp::Value::integer(1))));
    mp::Value::Array cells;
    for (const char c : text.substr(0, 40)) {
        cells.push_back(rpc::params(mp::Value::str(std::string(1, c)), mp::Value::integer(0)));
    }
    batches.push_back(rpc::params(mp::Value::str("grid_line"),
                                  rpc::params(mp::Value::integer(1), mp::Value::integer(0),
                                              mp::Value::integer(0), mp::Value::array(std::move(cells)))));
    batches.push_back(rpc::params(mp::Value::str("flush"), mp::Value::array({})));
    w.value(mp::Value::array(std::move(batches)));
    return frame;
}

} // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    const std::string mode = argc > 1 ? argv[1] : "ok";
    mp::Limits limits;
    mp::Decoder decoder(limits);
    char buf[4096];
    bool first = true;
    for (;;) {
        const long n = read_some(buf, sizeof buf);
        if (n <= 0) {
            return 0; // the owner closed our input
        }
        decoder.feed(std::string_view(buf, static_cast<std::size_t>(n)));
        for (;;) {
            mp::Decoder::Result r = decoder.next();
            if (r.status == mp::Decoder::Status::NeedMore) {
                break;
            }
            if (r.status != mp::Decoder::Status::Value) {
                return 2;
            }
            const mp::Value& m = r.value;
            if (!m.is_array() || m.size() < 3) {
                continue;
            }
            const bool request = m.at(0).as_int() == 0;
            const std::uint32_t id = request ? static_cast<std::uint32_t>(m.at(1).as_uint()) : 0;
            const std::string method = request ? m.at(2).as_str() : m.at(1).as_str();
            const mp::Value& params = request ? m.at(3) : m.at(2);
            if (mode == "silent") {
                continue;
            }
            if (first && request) {
                first = false;
                if (mode == "garbage") {
                    write_all(std::string("\xc1\xc1 this is not msgpack-rpc", 25));
                    continue;
                }
                if (mode == "crash") {
                    std::fprintf(stderr, "fixture: crashing on purpose\n");
                    std::fflush(stderr);
                    return 3;
                }
                if (mode == "flood") {
                    // A str32 header claiming 200 MiB: past every inbound bound the owner keeps.
                    write_all(std::string("\x94\x01\x01\xc0\xdb\x0c\x80\x00\x00", 9));
                    continue;
                }
                if (mode == "stranger") {
                    write_all(response(id + 1000, mp::Value::nil(), mp::Value::nil()));
                    continue;
                }
            }
            if (!request) {
                if (method == "nvim_command" && params.at(0).as_str() == "qa!" && mode != "ignore-quit") {
                    return 0;
                }
                if (method == "nvim_input") {
                    write_all(redraw_of(params.at(0).as_str()));
                }
                continue;
            }
            if (method == "nvim_get_api_info") {
                write_all(response(id, mp::Value::nil(), api_info(mode == "old" ? 12 : 13)));
            } else if (method == "nvim_ui_attach") {
                write_all(response(id, mp::Value::nil(), mp::Value::nil()));
            } else if (method == "nvim_get_mode") {
                mp::Value::Map said;
                said.push_back({mp::Value::str("mode"), mp::Value::str(mode == "prompt" ? "r" : "n")});
                said.push_back({mp::Value::str("blocking"), mp::Value::boolean(mode == "prompt")});
                write_all(response(id, mp::Value::nil(), mp::Value::map(std::move(said))));
            } else if (method == "zengine_fixture_hold") {
                continue; // a request this peer never answers
            } else if (method == "nvim_exec_lua") {
                if (mode == "prompt") {
                    continue; // never served, as a real prompt never serves it
                }
                mp::Value::Map said;
                said.push_back({mp::Value::str("did_enter"), mp::Value::integer(1)});
                said.push_back({mp::Value::str("clipboard"), mp::Value::boolean(true)});
                write_all(response(id, mp::Value::nil(), mp::Value::map(std::move(said))));
            } else {
                write_all(response(id, mp::Value::str("fixture: unknown method " + method), mp::Value::nil()));
            }
        }
    }
}
