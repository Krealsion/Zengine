// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-window-terminal`: the terminal window over the process's own
// terminal. It takes the terminal when it is activated -- its output side through the terminal's
// claim (graphics/window/terminal_io.hpp), its input side as the Input weave reads it
// (input/terminal_reader.hpp) -- and gives both back when it goes. While it is loaded it claims the
// terminal's input, so an Input weave beside it leaves the terminal to it. With no terminal (a
// pipe, a suite) it writes nothing and reads nothing, and still keeps and composes its picture.

#include "graphics/window/window_terminal.hpp"

#include "input/terminal_reader.hpp"

#include <zen/kernel/export.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include <zen/host/wait_set.hpp>

namespace {

class ProcessTerminal {
public:
    ProcessTerminal() { zengine::input::claim_window_terminal_input(); }
    ~ProcessTerminal() {
#if !defined(_WIN32)
        resize_.reset();
#endif
        in_.reset();
        out_.reset();
        zengine::input::release_window_terminal_input();
    }
    ProcessTerminal(const ProcessTerminal&) = delete;
    ProcessTerminal& operator=(const ProcessTerminal&) = delete;

    void claim() {
        if (!out_) {
            out_.emplace();
            in_.emplace(true);
#if !defined(_WIN32)
            resize_.emplace();
#endif
        }
    }
    zengine::graphics::window::TerminalSize size() const {
        return out_ ? out_->size() : zengine::graphics::window::TerminalSize{};
    }
    void write(std::string_view bytes) {
        if (out_ && out_->size().measured()) {
            out_->write(bytes);
        }
    }
    std::vector<zengine::input::InputEvent> poll() {
#if !defined(_WIN32)
        if (resize_) {
            resize_->drain(); // the size itself is asked on every look
        }
#endif
        return in_ ? in_->poll() : decltype(poll()){};
    }

    /// What a host's wait waits on for this terminal: its input, and on POSIX its resizes. Before
    /// it is taken, and with no terminal, nothing.
    loom::host::WaitSet wait_set() {
        loom::host::WaitSet set = in_ ? in_->wait_set() : loom::host::WaitSet{};
#if !defined(_WIN32)
        if (resize_ && resize_->fd() >= 0 && !set.handles.empty()) {
            set.handles.push_back(resize_->fd());
        }
#endif
        return set;
    }
    /// Whether it must look again soon whatever the terminal says: a lone Escape held.
    bool holding() const noexcept { return in_ && in_->holding(); }

private:
    std::optional<zengine::graphics::window::TuiTerminal> out_;
    std::optional<zengine::input::PlatformReader> in_;
#if !defined(_WIN32)
    std::optional<zengine::graphics::window::ResizeWatch> resize_;
#endif
};

using TerminalWindow = zengine::graphics::window::WindowT<ProcessTerminal>;

} // namespace

ZEN_EXPORT_WEAVE(TerminalWindow)
