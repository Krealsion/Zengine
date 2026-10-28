// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_PANE_OBSERVED_HPP
#define ZENGINE_FLOW_PANE_OBSERVED_HPP
// What a running rule was seen to do, read off the Flow host's journal
// (flow/docs/zengine.flow.host.md): each message that reached it, the state it kept after, and
// what it said because of that message; as one line a message, and as the value each wire last
// carried. Pure: the weave owns when to ask and whom to believe.
#include "flow/flow-host/vocabulary.hpp"
#include "maker/definition.hpp"
#include "message-draft/draft.hpp"
#include "operator/operator.hpp"
#include <zen/value.hpp>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::flow_pane {
namespace fh = zengine::flow_host;

/// One message a rule said, and its fields as it said them.
struct Said {
    std::string message;
    std::vector<std::pair<std::string, std::string>> fields;
};

/// ONE MESSAGE THAT REACHED THE RULE: the journal's number for its arrival, its shape's name, what
/// it carried, the state the rule kept before it (none when the journal no longer says) and after
/// it, and what the rule said because of it.
struct Arrival {
    std::int64_t sequence = 0;
    std::string message;
    std::optional<loom::Value> carried;
    std::optional<loom::Value> before, after;
    std::vector<Said> said;
};

/// WHAT THE JOURNAL SAYS: each arrival, oldest first, and each other word -- a refusal, a failed
/// handler -- in the host's words.
struct Observed {
    std::vector<Arrival> arrivals;
    std::vector<std::string> notes;
};

namespace observed_detail {
inline std::string text_of(const loom::Bytes& b) { return {b.begin(), b.end()}; }
/// Bytes admitted at one of `shapes`, or none.
inline std::optional<loom::Value> admitted(const loom::Bytes& bytes,
                                           const std::vector<std::shared_ptr<const loom::Schema>>& shapes) {
    if (bytes.empty()) return std::nullopt;
    const auto parsed = loom::parse(text_of(bytes));
    for (const auto& shape : shapes) {
        auto value = loom::admit(parsed, shape);
        if (value) return std::move(value).value();
    }
    return std::nullopt;
}
/// The sequence an event's detail names after `#`, as `kept` and `output` name their arrival.
inline std::optional<std::int64_t> cause_of(const std::string& detail) {
    const auto at = detail.rfind(" after #");
    const auto start = at == std::string::npos ? (detail.rfind("after #", 0) == 0 ? 7 : std::string::npos)
                                               : at + 8;
    if (start == std::string::npos || start >= detail.size()) return std::nullopt;
    std::int64_t n = 0;
    for (std::size_t i = start; i < detail.size(); ++i) {
        if (detail[i] < '0' || detail[i] > '9') return std::nullopt;
        if (n > (std::numeric_limits<std::int64_t>::max)() / 10) return std::nullopt;
        n = n * 10 + (detail[i] - '0');
    }
    return n;
}
inline std::vector<std::pair<std::string, std::string>> fields_of(const loom::Value& value) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& f : value.schema().fields())
        out.emplace_back(f.name, message_draft::summary(value.get(f.name)));
    return out;
}
} // namespace observed_detail

/// THE JOURNAL READ against the definition the host runs, `started` the state it was run with: each
/// `arrived` an arrival, its `kept` the state after it, and each `output` said because of the
/// arrival it names (or, naming none, the last one before it). The first arrival's state before is
/// `started` while the journal has dropped nothing, and each later one's the state after the one
/// before it.
inline Observed observe(const fh::FlowAnswer& answer, const maker::Definition& def,
                        const std::optional<loom::Value>& started) {
    namespace od = observed_detail;
    Observed out;
    std::map<std::int64_t, std::size_t> by_sequence;
    for (const auto& event : answer.events) {
        if (event.kind == "arrived") {
            Arrival a;
            a.sequence = event.sequence;
            a.message = event.detail;
            a.carried = od::admitted(event.payload, def.accepts);
            by_sequence[event.sequence] = out.arrivals.size();
            out.arrivals.push_back(std::move(a));
        } else if (event.kind == "kept") {
            const auto cause = od::cause_of(event.detail);
            const auto at = cause ? by_sequence.find(*cause) : by_sequence.end();
            if (at == by_sequence.end()) continue;
            out.arrivals[at->second].after = od::admitted(event.payload, {def.state});
        } else if (event.kind == "output") {
            const auto cause = od::cause_of(event.detail);
            Arrival* to = nullptr;
            if (cause) {
                if (const auto at = by_sequence.find(*cause); at != by_sequence.end())
                    to = &out.arrivals[at->second];
            } else if (!out.arrivals.empty()) {
                to = &out.arrivals.back();
            }
            const auto value = od::admitted(event.payload, def.emits);
            const auto space = event.detail.find(' ');
            Said said{value ? value->schema().name() : event.detail.substr(0, space), {}};
            if (value) said.fields = od::fields_of(*value);
            if (to != nullptr) to->said.push_back(std::move(said));
            else out.notes.push_back("said " + said.message);
        } else if (event.kind == "refused" || event.kind == "dispatch-refused" ||
                   event.kind == "handler-failed") {
            out.notes.push_back(event.kind + ": " + event.detail);
        }
    }
    for (std::size_t i = 0; i < out.arrivals.size(); ++i) {
        if (i > 0) out.arrivals[i].before = out.arrivals[i - 1].after;
        else if (answer.dropped == 0) out.arrivals[i].before = started;
    }
    return out;
}

/// ONE ARRIVAL AS ONE LINE: what came in, what changed, what was said --
/// `tally.panel.Count  amount 3  ->  total 6 -> 9  ->  said tally.Total 9`.
inline std::string line_of(const Arrival& a) {
    namespace od = observed_detail;
    std::string out = a.message;
    if (a.carried)
        for (const auto& [name, value] : od::fields_of(*a.carried)) out += "  " + name + " " + value;
    std::string changed;
    if (a.after) {
        for (const auto& f : a.after->schema().fields()) {
            const auto now = message_draft::summary(a.after->get(f.name));
            const auto was = a.before && a.before->get(f.name)
                                 ? std::optional<std::string>(message_draft::summary(a.before->get(f.name)))
                                 : std::nullopt;
            if (was && *was == now) continue;
            changed += (changed.empty() ? "" : ", ") + f.name + " " + (was ? *was + " " : std::string()) +
                       "-> " + now;
        }
        if (changed.empty()) changed = "nothing changed";
    }
    if (!changed.empty()) out += "  ->  " + changed;
    for (const auto& s : a.said) {
        out += "  ->  said " + s.message;
        if (s.fields.size() == 1) out += " " + s.fields.front().second;
        else
            for (const auto& [name, value] : s.fields) out += " " + name + " " + value;
    }
    return out;
}

/// THE VALUE EACH WIRE LAST CARRIED, by where it comes from: a field of the message as it came, a
/// field the rule keeps as it was before the message, and the field it wrote as it was after.
/// Empty while nothing has reached the rule.
struct Carried {
    std::map<std::string, std::string> message, before, after;
    bool known() const { return !message.empty() || !after.empty(); }
};
inline Carried carried(const Observed& seen) {
    namespace od = observed_detail;
    Carried out;
    if (seen.arrivals.empty()) return out;
    const auto& a = seen.arrivals.back();
    if (a.carried)
        for (const auto& [name, value] : od::fields_of(*a.carried)) out.message[name] = value;
    if (a.before)
        for (const auto& [name, value] : od::fields_of(*a.before)) out.before[name] = value;
    if (a.after)
        for (const auto& [name, value] : od::fields_of(*a.after)) out.after[name] = value;
    return out;
}

} // namespace zengine::flow_pane
#endif
