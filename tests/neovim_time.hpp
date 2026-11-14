// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_NEOVIM_TIME_HPP
#define ZENGINE_TESTS_NEOVIM_TIME_HPP

// THE TIME THE NEOVIM SUITES HAND: the hosting library (`editor/neovim/host.hpp`) and the pane read
// no clock, so a suite hands them one -- the machine's, since a real child answers in real time --
// and moves it past a bound only where a case proves that bound, choosing that moment with care,
// since a child may answer first. A question waited for is the suite's patience, never the
// library's: the library only asks (WL-NVIM-05). Shared by the Neovim suite files.

#include "editor/neovim/host.hpp"

#include <zen/host/wait_set.hpp>
#include <zen/switchboard/message.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/// THE MACHINE'S TIME, as a host's wait reads it, in milliseconds.
inline std::int64_t machine_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// ONE QUESTION WAITED FOR, as a suite waits on its child: asked later, then pumped on the machine's
/// time and waited on until Neovim answers, says it waits for input, or `ms` has passed. `late`
/// hears what became of an outstanding request; `flushes`, when given, the screen's flushes when
/// the answer arrived.
inline zengine::neovim::Host::Asked ask_waiting(zengine::neovim::Host& host, std::string_view method,
                                                const zengine::neovim::msgpack::Value& params, int ms,
                                                std::optional<zengine::neovim::rpc::Response>& got,
                                                std::string& why, zengine::neovim::Host::Callback late = {},
                                                std::uint64_t* flushes = nullptr) {
    namespace nv = zengine::neovim;
    std::optional<nv::Host::Outcome> said;
    if (!host.ask_later(method, params, ms, [&said](nv::Host::Outcome o) { said = std::move(o); }, why,
                        std::move(late))) {
        return nv::Host::Asked::Unsent;
    }
    while (!said.has_value()) {
        (void)host.pump(machine_ms());
        if (!said.has_value()) {
            (void)host.wait(10);
        }
    }
    if (said->asked == nv::Host::Asked::Outstanding) {
        why = said->why;
        return nv::Host::Asked::Outstanding;
    }
    got = std::move(said->got);
    if (flushes != nullptr) {
        *flushes = said->flushes;
    }
    return nv::Host::Asked::Answered;
}

/// A QUESTION ANSWERED WITHIN `ms`, or none and why: `ask_waiting` with nobody to hear a late answer.
inline std::optional<zengine::neovim::rpc::Response> call_now(zengine::neovim::Host& host, std::string_view method,
                                                              const zengine::neovim::msgpack::Value& params, int ms,
                                                              std::string& why) {
    namespace nv = zengine::neovim;
    std::optional<nv::rpc::Response> got;
    if (ask_waiting(host, method, params, ms, got, why) != nv::Host::Asked::Answered) {
        return std::nullopt;
    }
    if (!host.alive() && !got->error.is_nil()) {
        why = nv::rpc::error_text(got->error);
        return std::nullopt;
    }
    return got;
}

/// THE TIME A WORKSHOP RIG HANDS THE NEOVIM PANE: the machine's, moved on by `pass` where a case
/// moves time the pane does not hear, and back to the machine's at the case's end (`Moved`).
class PaneTime {
public:
    std::int64_t now_ms() const { return machine_ms() + ahead_; }
    /// The clock moves on `ms` at once; the pane hears it with the next wake.
    void pass(std::int64_t ms) { ahead_ += ms; }
    /// The machine's again, for the next case's pane, which has heard nothing yet.
    void reset() { ahead_ = 0; }
    /// THE HOST'S OWN WORD TO THE PANE, as a rig gives it: what it claimed is ready and its time
    /// came, at this time. It has no sender, as only the host's has.
    loom::Message wake() const {
        loom::host::Waited w;
        w.held = true;
        w.ready = true;
        w.due = true;
        w.now_ms = now_ms();
        return loom::Message(loom::to_value(w), loom::WeaveId{}, loom::WeaveId{}, 0);
    }

private:
    std::int64_t ahead_ = 0;
};

#endif // ZENGINE_TESTS_NEOVIM_TIME_HPP
