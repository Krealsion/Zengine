// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A MESSAGE TIMELINE: what an operation a case drives exchanged on the case's own bus, in order,
// one readable line a delivery or refusal -- who to whom, the shape, the office it was said as,
// and its fields -- so a change to a conversation shows in review as a diff of a kept file. It is
// the test rig's: an observer on the rig's own in-process bus, which no guest can reach.

#ifndef ZENGINE_TESTS_TIMELINE_HPP
#define ZENGINE_TESTS_TIMELINE_HPP

#include <zen/switchboard.hpp>
#include <zen/value.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace zengine::tests {

class Timeline {
public:
    /// Text fields are cut at this many bytes and lists said by their length: a line is for
    /// reading, and a picture's thousand rows would bury the conversation.
    static constexpr std::size_t kTextBytes = 48;

    explicit Timeline(loom::Switchboard& bus) : bus_(&bus) {
        tap_ = bus.add_observer([this](const loom::BusEvent& ev) { heard(ev); });
    }
    ~Timeline() { stop(); }
    Timeline(const Timeline&) = delete;
    Timeline& operator=(const Timeline&) = delete;

    /// A weave with no office, said by the name a case gives it.
    void name(loom::WeaveId id, std::string label) { labels_[id.value] = std::move(label); }

    /// Stop listening; what was heard stays.
    void stop() {
        if (bus_ != nullptr) {
            bus_->remove_observer(tap_);
            bus_ = nullptr;
        }
    }

    const std::vector<std::string>& lines() const { return lines_; }

    std::string text() const {
        std::string out;
        for (const std::string& line : lines_) {
            out += line;
            out += '\n';
        }
        return out;
    }

    /// The kept timeline at `kept`, or why the heard one differs: empty when they agree. The
    /// heard one is written to `now` either way, so a reviewer diffs the two files.
    std::string compare(const std::filesystem::path& kept, const std::filesystem::path& now) const {
        std::filesystem::create_directories(now.parent_path());
        std::ofstream(now, std::ios::binary) << text();
        std::ifstream in(kept, std::ios::binary);
        std::string was{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::string clean;
        for (const char c : was) {
            if (c != '\r') clean += c;
        }
        if (clean == text()) return std::string();
        return "the conversation differs from " + kept.generic_string() + "; as heard: " +
               now.generic_string();
    }

private:
    std::string who(loom::WeaveId id) const {
        if (!id.valid()) return "host";
        const auto label = labels_.find(id.value);
        if (label != labels_.end()) return label->second;
        const std::string role = bus_ != nullptr ? bus_->role_of(id) : std::string();
        return role.empty() ? "weave " + std::to_string(id.value) : role;
    }

    static std::string cell_text(const loom::Cell& c, int depth) {
        switch (c.kind()) {
        case loom::Kind::Int: return std::to_string(c.as_int());
        case loom::Kind::Float: {
            char out[32];
            std::snprintf(out, sizeof out, "%g", c.as_float());
            return out;
        }
        case loom::Kind::Bool: return c.as_bool() ? "true" : "false";
        case loom::Kind::Bytes: return "<" + std::to_string(c.as_bytes().size()) + " bytes>";
        case loom::Kind::Text: {
            const std::string& t = c.as_text();
            std::string out = "\"" + t.substr(0, kTextBytes) + "\"";
            if (t.size() > kTextBytes) out += "...";
            return out;
        }
        case loom::Kind::Message: return depth > 1 ? "{...}" : fields_text(*c.as_message(), depth + 1);
        case loom::Kind::List: return "[" + std::to_string(c.as_list().size()) + "]";
        }
        return "?";
    }

    static std::string fields_text(const loom::Value& v, int depth = 0) {
        std::string out = "{";
        bool first = true;
        for (const loom::Field& f : v.schema().fields()) {
            const loom::Cell* c = v.get(f.name);
            if (c == nullptr) continue;
            out += (first ? "" : ", ") + f.name + ": " + cell_text(*c, depth);
            first = false;
        }
        return out + "}";
    }

    void heard(const loom::BusEvent& ev) {
        const std::string shape = ev.schema_name + " v" + std::to_string(ev.schema_version);
        const std::string route = who(ev.sender) + " -> " +
                                  (ev.target.valid() ? who(ev.target)
                                                     : ev.addressed_role.empty() ? "any"
                                                                                 : ev.addressed_role);
        const std::string as = ev.authored_role.empty() ? "" : " as " + ev.authored_role;
        if (ev.kind == loom::EventKind::Delivered) {
            lines_.push_back(route + as + "  " + shape +
                             (ev.payload != nullptr ? "  " + fields_text(*ev.payload) : ""));
        } else if (ev.kind == loom::EventKind::Refused) {
            lines_.push_back(route + as + "  " + shape + "  REFUSED: " + ev.refusal.message());
        }
    }

    loom::Switchboard* bus_ = nullptr;
    loom::ObserverId tap_{};
    std::map<std::uint64_t, std::string> labels_;
    std::vector<std::string> lines_;
};

} // namespace zengine::tests

#endif
