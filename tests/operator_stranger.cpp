// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// See operator_stranger.hpp for what this consumer knows and what it cannot.

#include "operator_stranger.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>

namespace stranger {
namespace {

std::vector<Port> ports_of(const loom::Schema& schema) {
    std::vector<Port> ports;
    ports.reserve(schema.fields().size());
    for (const loom::Field& f : schema.fields()) {
        ports.push_back(Port{f.name, loom::name_of(f.type.kind)});
    }
    return ports;
}

/// A text argument, converted against the kind the PORT declares. Canonical
/// spellings only: no leading `+`, no whitespace, no trailing characters — a
/// reader that guessed would be inventing an argument nobody wrote.
bool cell_from_text(loom::Kind kind, const std::string& text, loom::Cell& out,
                    std::string& why) {
    switch (kind) {
    case loom::Kind::Int: {
        std::int64_t value = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        const std::from_chars_result r = std::from_chars(first, last, value);
        if (r.ec != std::errc{} || r.ptr != last) {
            why = "'" + text + "' is not an Int";
            return false;
        }
        out = loom::Cell::integer(value);
        return true;
    }
    case loom::Kind::Bool:
        if (text == "true") {
            out = loom::Cell::boolean(true);
            return true;
        }
        if (text == "false") {
            out = loom::Cell::boolean(false);
            return true;
        }
        why = "'" + text + "' is not a Bool";
        return false;
    default:
        why = std::string("this reader understands Int and Bool ports only, not ") +
              loom::name_of(kind);
        return false;
    }
}

std::string render(const loom::Cell& cell, std::string& why) {
    switch (cell.kind()) {
    case loom::Kind::Int:
        return std::to_string(cell.as_int());
    case loom::Kind::Bool:
        return cell.as_bool() ? "true" : "false";
    default:
        why = std::string("this reader cannot render a ") + loom::name_of(cell.kind());
        return {};
    }
}

} // namespace

Signature describe(const zengine::op::Catalog& catalog, std::string_view identity) {
    Signature sig;
    const zengine::op::OperatorDef* def = catalog.find(identity);
    if (def == nullptr) {
        return sig;
    }
    sig.found = true;
    sig.composite = def->is_composite();
    sig.inputs = ports_of(*def->inputs());
    sig.outputs = ports_of(*def->outputs());
    return sig;
}

Reading ask(const zengine::op::Catalog& catalog, std::string_view identity,
            const std::vector<std::pair<std::string, std::string>>& arguments) {
    Reading reading;
    const zengine::op::OperatorDef* def = catalog.find(identity);
    if (def == nullptr) {
        reading.reason = "this catalog publishes no '" + std::string(identity) + "'";
        return reading;
    }

    loom::Value pack(def->inputs());
    for (const auto& [name, text] : arguments) {
        const loom::Field* port = def->inputs()->find(name);
        if (port == nullptr) {
            reading.reason = "'" + std::string(identity) + "' has no input port named '" + name +
                             "'";
            return reading;
        }
        loom::Cell cell = loom::Cell::integer(0);
        if (!cell_from_text(port->type.kind, text, cell, reading.reason)) {
            return reading;
        }
        pack.set(name, std::move(cell));
    }

    // Anything still missing is the GATE's refusal, said in the gate's words.
    // This reader counts nothing and checks no arity.
    const zengine::op::Evaluation answer = catalog.evaluate(identity, std::move(pack));
    if (!answer) {
        reading.reason = answer.reason();
        return reading;
    }

    const loom::Schema& out = answer.value().schema();
    reading.port = out.fields()[0].name;
    reading.answer = render(*answer.value().at(0), reading.reason);
    reading.ok = reading.reason.empty();
    return reading;
}

} // namespace stranger
