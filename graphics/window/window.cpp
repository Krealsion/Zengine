// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-window-terminal`: the terminal window over the process's own
// terminal. It takes the terminal when it is activated -- its output side as a terminal Skin takes
// it (surface/tui_terminal.hpp), its input side as the Input weave reads it
// (input/terminal_reader.hpp) -- and gives both back when it goes. While it is loaded it claims the
// terminal's input, so an Input weave beside it leaves the terminal to it. With no terminal (a
// pipe, a suite) it writes nothing and reads nothing, and still keeps and composes its picture.

#include "graphics/window/window.hpp"

#include "input/terminal_reader.hpp"

#include <zen/kernel/export.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace {

class ProcessTerminal {
public:
    ProcessTerminal() { zengine::input::claim_window_terminal_input(); }
    ~ProcessTerminal() {
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
        }
    }
    zengine::surface::TerminalSize size() const {
        return out_ ? out_->size() : zengine::surface::TerminalSize{};
    }
    void write(std::string_view bytes) {
        if (out_ && out_->size().measured()) {
            out_->write(bytes);
        }
    }
    std::vector<zengine::input::InputEvent> poll() { return in_ ? in_->poll() : decltype(poll()){}; }

private:
    std::optional<zengine::surface::TuiTerminal> out_;
    std::optional<zengine::input::PlatformReader> in_;
};

using TerminalWindow = zengine::graphics::window::WindowT<ProcessTerminal>;

} // namespace

ZEN_EXPORT_WEAVE(TerminalWindow)
