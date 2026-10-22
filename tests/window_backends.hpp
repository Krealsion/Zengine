// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_WINDOW_BACKENDS_HPP
#define ZENGINE_TESTS_WINDOW_BACKENDS_HPP

// The two windows' devices as the suites hold them: a terminal the suite types into, reads and
// sizes, under the terminal window (`graphics/window/window_terminal.hpp`), and a pixel backend the
// suite reads, sizes and types into, under the OS window (`graphics/window/window_sdl.hpp`). The
// graphics suite drives the windows over them; the Workshop rig shows its desk in them
// (`rig_windows.hpp`). Each log keeps what its window wrote unless it is told to keep nothing.

#include "graphics/light_paint.hpp"
#include "graphics/vocabulary.hpp"
#include "graphics/window/window.hpp"
#include "graphics/window/window_sdl.hpp"
#include "graphics/window/window_terminal.hpp"
#include "input/translate.hpp"
#include "input/vocabulary.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::testing::window_backends {

namespace gfx = zengine::graphics;
namespace gw = zengine::graphics::window;
namespace light = zengine::graphics::light;

/// A terminal the suite types into, reads and sizes: what the window writes, kept; what it reads,
/// the bytes the suite queued, parsed by Zengine's own parser as a terminal's are.
struct TermLog {
    std::string typed;
    std::vector<zengine::input::InputEvent> moments; ///< read before what is typed, as given
    std::string written;
    std::vector<std::string> writes;              ///< each write, one showing each
    zengine::graphics::window::TerminalSize size{80, 30};  ///< its size in cells, the suite's to change
    bool keeps = true; ///< false: what the window writes is taken and not kept
};

class FakeTerminal {
public:
    FakeTerminal() = default;
    explicit FakeTerminal(TermLog* log) : log_(log) {}
    void claim() {}
    zengine::graphics::window::TerminalSize size() const {
        return log_ != nullptr ? log_->size : TermLog{}.size;
    }
    void write(std::string_view bytes) {
        if (log_ != nullptr && log_->keeps) {
            log_->written += bytes;
            log_->writes.emplace_back(bytes);
        }
    }
    std::vector<zengine::input::InputEvent> poll() {
        if (log_ == nullptr) {
            return {};
        }
        std::vector<zengine::input::InputEvent> out = std::move(log_->moments);
        log_->moments.clear();
        if (log_->typed.empty()) {
            return out;
        }
        const std::string bytes = std::move(log_->typed);
        log_->typed.clear();
        for (auto& e : zengine::input::terminal_bytes_to_events(
                 reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size())) {
            out.push_back(std::move(e));
        }
        return out;
    }

private:
    TermLog* log_ = nullptr;
};

/// What the OS window handed its backend and what the suite queues for it to read.
struct LightLog {
    struct Area {
        std::string area;
        std::int64_t x = 0, y = 0, w = 0, h = 0;
        std::vector<std::string> paths;
        std::vector<std::uint64_t> versions;
        std::vector<light::NodePrimitives> nodes;
        bool operator==(const Area&) const = default;
    };
    struct Frame {
        std::int64_t width = 0, height = 0;
        std::uint32_t ground = 0;
        std::vector<Area> areas;
        bool operator==(const Frame&) const = default;
    };
    gw::PixelSize size{gw::kDefaultWidth, gw::kDefaultHeight};
    gfx::TextMeasure face{8, 18}; ///< the face it sets text in, as the OS window's measures
    gw::PixelPolled queued;
    std::vector<Frame> frames;
    bool keeps = true; ///< false: each picture is presented and not kept in `frames`
    bool keeps_last = false; ///< true: `frames` holds the last picture presented, and only it
    int claims = 0;
    bool drawing = true; ///< false: no OS window, so nothing is presented
    bool reading = true; ///< false: a picture cannot be read back
    std::vector<Frame> captured;              ///< each picture it was asked to read back
    std::optional<std::string> clipboard;     ///< what its clipboard holds; none: no read exists
    std::vector<std::string> copies;
    bool holds_copies = false; ///< true: a copy is what its clipboard holds after, as a platform's
    int reads = 0;                            ///< how often its clipboard was read
    std::optional<gw::WindowPlacementValue> placement;
    std::vector<gw::WindowPlacementValue> placed;
    std::vector<gw::PixelSize> grown;         ///< each size it was asked to grow toward
    int maximized = 0;

    static Frame kept_of(const gw::ComposedLightFrame& frame) {
        Frame f{frame.width, frame.height, frame.ground, {}};
        for (const gw::LightAreaShown& a : frame.areas) {
            Area kept{a.area, a.x, a.y, a.w, a.h, {}, {}, {}};
            for (const gw::LightNodeShown& n : a.nodes) {
                kept.paths.push_back(n.path);
                kept.versions.push_back(n.version);
                kept.nodes.push_back(*n.prims);
            }
            f.areas.push_back(std::move(kept));
        }
        return f;
    }

    /// The picture this backend reads back: one pixel of a frame's ground (0xRRGGBBAA), as a BMP.
    static gw::CapturedWindowPicture picture_of(std::uint32_t ground) {
        const std::uint8_t bgr[3] = {static_cast<std::uint8_t>((ground >> 8) & 0xffu),
                                     static_cast<std::uint8_t>((ground >> 16) & 0xffu),
                                     static_cast<std::uint8_t>((ground >> 24) & 0xffu)};
        return gw::CapturedWindowPicture{1, 1, gfx::kCellPx, "image/bmp",
                                         gw::bmp_of_bgr24(1, 1, 3, bgr)};
    }

    const Area* area(const std::string& name) const {
        if (frames.empty()) {
            return nullptr;
        }
        for (const Area& a : frames.back().areas) {
            if (a.area == name) {
                return &a;
            }
        }
        return nullptr;
    }
};

/// A pixel backend the suite reads, sizes and types into.
class FakeSdlBackend {
public:
    FakeSdlBackend() = default;
    explicit FakeSdlBackend(LightLog* log) : log_(log) {}
    void claim() { ++log_->claims; }
    gw::PixelSize size() const { return log_->size; }
    gfx::TextMeasure text() const { return log_->face; }
    bool present(const gw::ComposedLightFrame& frame) {
        if (!log_->drawing) {
            return false;
        }
        if (log_->keeps_last) {
            log_->frames.assign(1, LightLog::kept_of(frame));
        } else if (log_->keeps) {
            log_->frames.push_back(LightLog::kept_of(frame));
        }
        return true;
    }
    std::optional<gw::CapturedWindowPicture> capture(const gw::ComposedLightFrame& frame) {
        log_->captured.push_back(LightLog::kept_of(frame));
        if (!log_->reading) {
            return std::nullopt;
        }
        return LightLog::picture_of(frame.ground);
    }
    gw::PixelPolled poll() {
        gw::PixelPolled out = std::move(log_->queued);
        log_->queued = {};
        return out;
    }
    void clipboard_copy(std::string_view text) {
        log_->copies.emplace_back(text);
        if (log_->holds_copies) {
            log_->clipboard = std::string(text);
        }
    }
    std::optional<std::string> clipboard_text() {
        ++log_->reads;
        return log_->clipboard;
    }
    std::optional<gw::WindowPlacementValue> placement() { return log_->placement; }
    void place(const gw::WindowPlacementValue& want) { log_->placed.push_back(want); }
    void grow(const gw::PixelSize& want) { log_->grown.push_back(want); }
    void maximize() { ++log_->maximized; }

private:
    LightLog* log_ = nullptr;
};

/// The two windows over them.
using FakeTerminalWindow = gw::WindowT<FakeTerminal>;
using FakeSdlWindow = gw::SdlWindowT<FakeSdlBackend>;

} // namespace zengine::testing::window_backends

#endif // ZENGINE_TESTS_WINDOW_BACKENDS_HPP
