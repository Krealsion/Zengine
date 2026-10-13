// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_GRAPHICS_RIG_HPP
#define ZENGINE_TESTS_GRAPHICS_RIG_HPP

// The graphics suites' rig: the real kernel, manager and bus, a tap that keeps every delivery with
// its bytes, ears that hear the answers, the window's report and the snake's news, and the
// arranger's gestures -- the skins and the board's code offered, an area placed, a skin chosen --
// sent by the test, which stands where a host's arranger stands.

#include "graphics/vocabulary.hpp"
#include "examples/snake/vocabulary.hpp"

#include <zen/kernel/admission.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/serialize.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::testing::graphics_rig {

namespace gfx = zengine::graphics;

/// One delivery the tap saw: its shape, who sent it as which office, where it was addressed, and
/// its canonical bytes.
struct Delivery {
    std::string shape;
    std::uint64_t sender = 0;
    std::uint64_t target = 0;
    std::string authored_role;
    std::string addressed_role;
    std::string bytes;
};

struct Heard {
    struct Answer {
        std::uint64_t corr = 0;
        int kind = 0; // 0 Result, 1 Ack, 2 Refused
        std::string text;
    };
    std::vector<Answer> answers;
    std::vector<gfx::WindowShown> shown;
    std::vector<zengine::snake::SnakeVisual> visuals;
    int eaten = 0;

    const Answer* find(std::uint64_t corr) const {
        for (const Answer& a : answers) {
            if (a.corr == corr) {
                return &a;
            }
        }
        return nullptr;
    }
};

struct GraphicsRigEars {
    std::int64_t heard = 0;
    ZEN_SHAPE(GraphicsRigEars, 1, ZEN_FIELD(heard));
};

class Ears : public loom::WeaveBase<
                 Ears, GraphicsRigEars,
                 loom::Accept<loom::Result, loom::Ack, loom::Refused, gfx::WindowShown,
                              zengine::snake::SnakeVisual, zengine::snake::FoodEaten>,
                 loom::Emit<>> {
public:
    explicit Ears(Heard& heard) : heard_(&heard) {}
    void on(const loom::Result& r, loom::Mail& mail) { note(mail, 0, r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { note(mail, 1, ""); }
    void on(const loom::Refused& r, loom::Mail& mail) { note(mail, 2, r.reason); }
    void on(const gfx::WindowShown& s, loom::Mail&) { heard_->shown.push_back(s); }
    void on(const zengine::snake::SnakeVisual& v, loom::Mail&) { heard_->visuals.push_back(v); }
    void on(const zengine::snake::FoodEaten&, loom::Mail&) { ++heard_->eaten; }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.heard;
        heard_->answers.push_back(Heard::Answer{mail.correlation(), kind, std::move(text)});
    }
    Heard* heard_;
};

inline std::string window_office() {
    return std::string(gfx::kWindowOfficePrefix) + gfx::kTerminalWindow;
}

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus, loom::trust_every_artifact(
                                 "Zengine's test harness: it loads only this build tree's output")};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Heard heard;
    loom::WeaveId ears = mount_ears();
    std::vector<Delivery> log;
    loom::ObserverId tap = bus.add_observer([this](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered && e.payload != nullptr) {
            log.push_back(Delivery{e.schema_name, e.sender.value, e.target.value, e.authored_role,
                                   e.addressed_role, loom::serialize(*e.payload)});
        }
    });
    std::uint64_t next_corr = 1;

    Rig() = default;
    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;
    ~Rig() { bus.remove_observer(tap); }

    loom::WeaveId mount_ears() {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
        reach.allow(loom::ListLoaded::zen_name, loom::ListLoaded::zen_version, manager);
        reach.allow_to_any(gfx::WindowShowAsked::zen_name, gfx::WindowShowAsked::zen_version);
        return loom::mount_granted<Ears>(bus, std::move(reach), heard);
    }

    void drain() { bus.drain_until_idle(); }

    /// A lifecycle command as the ears, its answer by value once it lands.
    template <class Cmd>
    Heard::Answer command(const Cmd& cmd) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(ears, manager, loom::Message(loom::to_value(cmd), ears, ears, corr));
        drain();
        const Heard::Answer* a = heard.find(corr);
        return a != nullptr ? *a : Heard::Answer{corr, -1, "no answer"};
    }

    loom::WeaveId load(const char* name, const char* path, const std::string& role) {
        const Heard::Answer a = command(loom::LoadWeave{name, path, role});
        if (a.kind != 0) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a.text))};
    }

    /// Root sends to an office, as a host's arranger would speak.
    template <class T>
    void to_role(const std::string& role, const T& msg) {
        bus.send_to_role(role, loom::Message(loom::to_value(msg)));
    }

    /// The arranger's gestures for one area of the terminal window: the skins and the board's
    /// code offered, the area placed, the skin chosen.
    void arrange(const char* classic, const char* block, const char* board,
                 const std::string& area, const std::string& owner, const std::string& skin,
                 std::int64_t x = 0, std::int64_t y = 0) {
        to_role(gfx::kGraphicsRole, gfx::SkinOffered{"classic", classic});
        to_role(gfx::kGraphicsRole, gfx::SkinOffered{"block", block});
        to_role(gfx::kGraphicsRole, gfx::ElementCodeOffered{"SnakeBoardData", board});
        to_role(gfx::kGraphicsRole, gfx::AreaPlaced{area, gfx::kTerminalWindow, owner, x, y,
                                                    50 * gfx::kCellPx, 22 * gfx::kCellPx, 0,
                                                    true});
        to_role(gfx::kGraphicsRole, gfx::SkinChosen{area, skin});
        drain();
    }

    /// What the window shows now, and what it has spent.
    gfx::WindowShown shown() {
        bus.send_as_to_role(ears, window_office(),
                            loom::Message(loom::to_value(gfx::WindowShowAsked{}), ears, ears,
                                          next_corr++));
        drain();
        return heard.shown.empty() ? gfx::WindowShown{} : heard.shown.back();
    }

    std::string screen() {
        std::string out;
        for (const std::string& row : shown().rows) {
            out += row + "\n";
        }
        return out;
    }

    void tick() {
        to_role(zengine::snake::kWorldRole, zengine::snake::SnakeTick{});
        drain();
    }

    /// How many deliveries of `shape` since `from`, authored as `office` ("" for any).
    std::size_t count(std::size_t from, const std::string& shape,
                      const std::string& office = {}) const {
        std::size_t n = 0;
        for (std::size_t i = from; i < log.size(); ++i) {
            if (log[i].shape == shape && (office.empty() || log[i].authored_role == office)) {
                ++n;
            }
        }
        return n;
    }

    /// The deliveries of `shape` since `from`, read back as T.
    template <class T>
    std::vector<T> read(std::size_t from, const std::string& office = {}) const {
        std::vector<T> out;
        const std::string name = loom::schema_of<T>()->name();
        for (std::size_t i = from; i < log.size(); ++i) {
            if (log[i].shape == name && (office.empty() || log[i].authored_role == office)) {
                if (auto v = loom::claim_item<T>(loom::Bytes(log[i].bytes.begin(),
                                                             log[i].bytes.end()))) {
                    out.push_back(*v);
                }
            }
        }
        return out;
    }
};

} // namespace zengine::testing::graphics_rig

#endif // ZENGINE_TESTS_GRAPHICS_RIG_HPP
