// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_BUS_TALLY_HPP
#define ZENGINE_WORKSHOP_BUS_TALLY_HPP

// Where a running bus spends its time, off Loom's own stamps: each weave's handler time
// (`BusEvent::handler_elapsed_ns`), and each input's way to its paint -- a key, text, press or
// wheel a window gave an area, or the input office published, marks every weave its dispatch
// ancestry reaches (`BusEvent::dispatch_parent`), and that weave's next painted drawing is the
// input's paint, however many turns later. A host's observer: no weave reads the stamps, and it
// changes nothing it observes.

#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"

#include <zen/switchboard/switchboard.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ONE WEAVE'S HANDLERS, as the bus timed them: named by the office it held when it was first
/// seen, else by its weave id.
struct HandlerRow {
    std::string who;
    std::uint64_t deliveries = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t max_ns = 0;
    std::string slowest; ///< the shape of its slowest delivery
};

/// ONE KIND OF INPUT REACHING ONE WEAVE'S PAINT: how many, and how long each took, from the start
/// of the input's delivery to the end of the delivery that painted the weave's next drawing.
struct PaintRow {
    std::string kind; ///< `key`, `text`, `press` or `wheel`
    std::string who;  ///< the weave whose drawing was painted
    std::uint64_t count = 0;
    std::uint64_t max_ns = 0;
    std::vector<std::uint64_t> samples; ///< the latest `BusTally::kSamples`, for the median
};

class BusTally {
public:
    /// An input not painted within this long is counted unpainted rather than measured.
    static constexpr std::uint64_t kPaintWithinNs = 1000ull * 1000ull * 1000ull;
    /// The deliveries whose ancestry is remembered at once, and the samples a row keeps.
    static constexpr std::size_t kAncestry = 16384;
    static constexpr std::size_t kSamples = 4096;

    explicit BusTally(loom::Switchboard& bus) : bus_(&bus), began_(now_ns()) {
        observer_ = bus.add_observer([this](const loom::BusEvent& e) { observe(e); });
    }
    ~BusTally() { bus_->remove_observer(observer_); }
    BusTally(const BusTally&) = delete;
    BusTally& operator=(const BusTally&) = delete;

    /// Every weave's row, the most time in all first.
    std::vector<HandlerRow> handlers() const {
        std::vector<HandlerRow> rows;
        for (const auto& [id, row] : handlers_) {
            rows.push_back(row);
        }
        std::sort(rows.begin(), rows.end(), [](const HandlerRow& a, const HandlerRow& b) {
            return a.total_ns != b.total_ns ? a.total_ns > b.total_ns : a.who < b.who;
        });
        return rows;
    }

    /// Every kind of input and the weave it was painted by, the most counted first.
    std::vector<PaintRow> paints() const {
        std::vector<PaintRow> rows;
        for (const auto& [key, row] : paints_) {
            rows.push_back(row);
        }
        std::sort(rows.begin(), rows.end(), [](const PaintRow& a, const PaintRow& b) {
            return a.count != b.count ? a.count > b.count : a.kind + a.who < b.kind + b.who;
        });
        return rows;
    }

    /// Inputs whose reached weaves drew nothing the window painted within `kPaintWithinNs`.
    std::uint64_t unpainted() const noexcept { return unpainted_; }
    /// Deliveries observed, and how long this tally has been observing.
    std::uint64_t deliveries() const noexcept { return deliveries_; }
    std::uint64_t observed_ns() const { return now_ns() - began_; }

    static std::uint64_t median(std::vector<std::uint64_t> v) {
        if (v.empty()) {
            return 0;
        }
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
        return v[v.size() / 2];
    }

    /// THE TALLY IN WORDS: the `top` weaves by time in all, then every input row.
    std::vector<std::string> lines(std::size_t top = 12) const {
        std::vector<std::string> out;
        out.push_back("bus tally over " + ms(observed_ns()) + " ms: " +
                      std::to_string(deliveries_) + " deliveries");
        const std::vector<HandlerRow> rows = handlers();
        for (std::size_t i = 0; i < rows.size() && i < top; ++i) {
            const HandlerRow& r = rows[i];
            out.push_back("handlers of " + r.who + ": " + std::to_string(r.deliveries) +
                          " deliveries, " + ms(r.total_ns) + " ms in all, slowest " +
                          ms(r.max_ns) + " ms (" + r.slowest + ")");
        }
        for (const PaintRow& p : paints()) {
            out.push_back("a " + p.kind + " to its paint by " + p.who + ": " +
                          std::to_string(p.count) + ", median " + ms(median(p.samples)) +
                          " ms, slowest " + ms(p.max_ns) + " ms");
        }
        if (unpainted_ > 0) {
            out.push_back(std::to_string(unpainted_) +
                          " inputs reached no weave that drew within a second");
        }
        return out;
    }

private:
    struct Root {
        std::uint64_t began_ns = 0;
        std::string kind;
    };

    static std::uint64_t now_ns() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count());
    }

    /// Milliseconds with two decimals.
    static std::string ms(std::uint64_t ns) {
        const std::uint64_t hundredths = (ns + 5000) / 10000;
        const std::uint64_t frac = hundredths % 100;
        return std::to_string(hundredths / 100) + "." + (frac < 10 ? "0" : "") +
               std::to_string(frac);
    }

    std::string who(loom::WeaveId id) const {
        const std::string office = bus_->role_of(id);
        return office.empty() ? "weave " + std::to_string(id.value) : office;
    }

    /// The kind of input this delivery is, or empty: a release, a move and everything else are not.
    static std::string input_kind(const loom::BusEvent& e) {
        namespace g = zengine::graphics;
        namespace in = zengine::input;
        const auto either = [&e](const char* window, const char* office) {
            return e.schema_name == window || e.schema_name == office;
        };
        if (either(g::AreaKeyPressed::zen_name, in::KeyPressed::zen_name)) {
            return "key";
        }
        if (either(g::AreaTextEntered::zen_name, in::TextEntered::zen_name)) {
            return "text";
        }
        if (either(g::AreaWheelTurned::zen_name, in::PointerWheel::zen_name)) {
            return "wheel";
        }
        if (either(g::AreaButtonChanged::zen_name, in::PointerButton::zen_name) &&
            e.payload != nullptr) {
            const loom::Cell* pressed = e.payload->get("pressed");
            if (pressed != nullptr && pressed->as_bool()) {
                return "press";
            }
        }
        return {};
    }

    /// A drawing the window paints now: the last of a run (`more` false).
    static bool paints_now(const loom::BusEvent& e) {
        namespace g = zengine::graphics;
        if (e.schema_name != g::Drawn::zen_name && e.schema_name != g::ElementPut::zen_name &&
            e.schema_name != g::ElementChange::zen_name) {
            return false;
        }
        const loom::Cell* more = e.payload != nullptr ? e.payload->get("more") : nullptr;
        return more == nullptr || !more->as_bool();
    }

    void observe(const loom::BusEvent& e) {
        if (e.kind != loom::EventKind::Delivered && e.kind != loom::EventKind::HandlerFailed) {
            return;
        }
        const std::uint64_t at = now_ns();
        ++deliveries_;
        HandlerRow& row = handlers_[e.target.value];
        if (row.deliveries == 0) {
            row.who = who(e.target);
        }
        ++row.deliveries;
        row.total_ns += e.handler_elapsed_ns;
        if (e.handler_elapsed_ns >= row.max_ns) {
            row.max_ns = e.handler_elapsed_ns;
            row.slowest = e.schema_name;
        }

        const bool painted = paints_now(e);
        if (painted) {
            settle(e.sender, at);
        }
        const Root* root = nullptr;
        if (const auto it = ancestry_.find(e.dispatch_parent); it != ancestry_.end()) {
            root = &it->second;
        }
        Root fresh;
        if (root == nullptr) {
            fresh.kind = input_kind(e);
            if (fresh.kind.empty()) {
                return;
            }
            const std::uint64_t ran = std::min<std::uint64_t>(e.handler_elapsed_ns, at);
            fresh.began_ns = at - ran;
            root = &fresh;
        }
        const Root kept = *root;
        remember(e.seq, kept);
        if (!painted) {
            // A WEAVE THIS INPUT REACHED: its next painted drawing is the input's paint, unless an
            // earlier input it reached is still waiting for one; one that waited past the bound is
            // counted unpainted and this one waits instead.
            const auto [waiting, first] = pending_.try_emplace(e.target.value, kept);
            if (!first && at - waiting->second.began_ns > kPaintWithinNs) {
                ++unpainted_;
                waiting->second = kept;
            }
        }
    }

    /// The weave `sender` drew, and the window painted it at `at`: the earliest input waiting on
    /// it is measured, or counted unpainted when it waited too long.
    void settle(loom::WeaveId sender, std::uint64_t at) {
        const auto it = pending_.find(sender.value);
        if (it == pending_.end()) {
            return;
        }
        const Root root = it->second;
        pending_.erase(it);
        const std::uint64_t took = at > root.began_ns ? at - root.began_ns : 0;
        if (took > kPaintWithinNs) {
            ++unpainted_;
            return;
        }
        PaintRow& p = paints_[root.kind + '\n' + std::to_string(sender.value)];
        if (p.count == 0) {
            p.kind = root.kind;
            p.who = who(sender);
        }
        ++p.count;
        p.max_ns = std::max(p.max_ns, took);
        if (p.samples.size() >= kSamples) {
            p.samples.erase(p.samples.begin());
        }
        p.samples.push_back(took);
    }

    void remember(std::uint64_t seq, const Root& root) {
        if (ancestry_.size() >= kAncestry && !order_.empty()) {
            ancestry_.erase(order_.front());
            order_.pop_front();
        }
        if (ancestry_.emplace(seq, root).second) {
            order_.push_back(seq);
        }
    }

    loom::Switchboard* bus_;
    loom::ObserverId observer_ = 0;
    std::uint64_t began_ = 0;
    std::uint64_t deliveries_ = 0;
    std::uint64_t unpainted_ = 0;
    std::map<std::uint64_t, HandlerRow> handlers_;
    std::map<std::string, PaintRow> paints_;
    std::unordered_map<std::uint64_t, Root> ancestry_;
    std::deque<std::uint64_t> order_;
    std::unordered_map<std::uint64_t, Root> pending_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_BUS_TALLY_HPP
