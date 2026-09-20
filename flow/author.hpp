// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_FLOW_AUTHOR_HPP
#define ZENGINE_FLOW_AUTHOR_HPP

#include "flow/project.hpp"
#include "operator/catalog.hpp"

#include <algorithm>
#include <limits>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace zengine::flow {

inline std::vector<std::string> words(std::string_view text) {
    std::vector<std::string> out;
    std::string word;
    bool quoted = false, started = false, escape = false;
    for (char c : text) {
        if (escape) { word += c; escape = false; continue; }
        if (c == '\\' && quoted) { escape = true; continue; }
        if (c == '"') { quoted = !quoted; started = true; continue; }
        if (!quoted && c == '#' && !started) break;
        if (!quoted && (c == ' ' || c == '\t' || c == '\r')) {
            if (started) { out.push_back(word); word.clear(); started = false; }
        } else { word += c; started = true; }
    }
    if (quoted || escape) throw std::invalid_argument("unfinished quoted text");
    if (started) out.push_back(std::move(word));
    return out;
}

inline std::int64_t integer(std::string_view text) {
    std::int64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("expected an Int, got " + std::string(text));
    return result;
}

inline std::size_t index_of(std::string_view text) {
    const auto value = integer(text);
    if (value < 0) throw std::invalid_argument("index cannot be negative");
    return static_cast<std::size_t>(value);
}

inline loom::Kind scalar_kind(std::string_view name) {
    if (name == "Int") return loom::Kind::Int;
    if (name == "Bool") return loom::Kind::Bool;
    if (name == "Float") return loom::Kind::Float;
    if (name == "Text") return loom::Kind::Text;
    throw std::invalid_argument("scalar authoring accepts Int, Bool, Float, Text; "
                                "import-json carries full maker schemas");
}

inline loom::Cell scalar(loom::Kind kind, const std::string& text) {
    switch (kind) {
    case loom::Kind::Int: return loom::Cell::integer(integer(text));
    case loom::Kind::Bool:
        if (text != "true" && text != "false") throw std::invalid_argument("expected true or false");
        return loom::Cell::boolean(text == "true");
    case loom::Kind::Text: return loom::Cell::text(text);
    case loom::Kind::Float: {
        std::size_t used = 0;
        const auto number = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(number)) throw std::invalid_argument("expected a finite Float");
        return loom::Cell::real(number);
    }
    default: throw std::invalid_argument("use JSON for structured values");
    }
}

inline std::pair<std::string, std::string> split(const std::string& text, char delimiter) {
    const auto at = text.find(delimiter);
    if (at == std::string::npos || at == 0) throw std::invalid_argument("expected name" +
        std::string(1, delimiter) + "value, got " + text);
    return {text.substr(0, at), text.substr(at + 1)};
}

inline loom::Value fields(std::shared_ptr<const loom::Schema> schema,
                          const std::vector<std::string>& values, std::size_t first) {
    loom::Value out(schema);
    std::vector<std::string> written;
    for (std::size_t i = first; i < values.size(); ++i) {
        const auto [name, text] = split(values[i], '=');
        const auto* field = schema->find(name);
        if (field == nullptr) throw std::invalid_argument("unknown field " + name);
        if (std::find(written.begin(), written.end(), name) != written.end())
            throw std::invalid_argument("duplicate field " + name);
        written.push_back(name);
        out.set(name, scalar(field->type.kind, text));
    }
    auto gated = loom::admit(std::move(out), *schema);
    if (!gated) throw std::invalid_argument(gated.first_error().message());
    return std::move(gated).value();
}

class Draft {
public:
    Draft(const op::Catalog& catalog, std::string name)
        : catalog_(&catalog) {
        definition_.name = std::move(name);
        definition_.state = loom::make_schema(definition_.name + ".State", 1, {});
    }
    Draft(const op::Catalog& catalog, const Project& project)
        : catalog_(&catalog), definition_(project.definition), initial_(project.state) {
        if (definition_.revision == std::numeric_limits<std::int64_t>::max())
            throw std::invalid_argument("revision exhausted");
        ++definition_.revision;
    }
    const maker::Definition& definition() const noexcept { return definition_; }

    // These commands change only the draft. end constructs a complete trigger;
    // finish admits the complete definition, before any live weave is touched.
    void command(const std::vector<std::string>& args) {
        if (args.empty()) return;
        const auto& verb = args[0];
        auto exactly = [&](std::size_t count) {
            if (args.size() != count) throw std::invalid_argument("wrong argument count for " + verb);
        };
        if (verb == "state") {
            exactly(4);
            if (builder_ || !definition_.on.empty() || initial_)
                throw std::invalid_argument("declare state before triggers; schema edits need a new project");
            auto declared = definition_.state->fields();
            if (definition_.state->find(args[1])) throw std::invalid_argument("duplicate state field");
            const auto kind = scalar_kind(args[2]);
            const auto value = scalar(kind, args[3]);
            declared.push_back({args[1], loom::type_of(kind), true});
            definition_.state = loom::make_schema(definition_.state->name(), 1, std::move(declared));
            defaults_.emplace(args[1], value);
        } else if (verb == "accept" || verb == "publish") {
            if (args.size() < 2 || builder_) throw std::invalid_argument("declare a message outside a trigger");
            auto& list = verb == "accept" ? definition_.accepts : definition_.emits;
            const auto name = qualify(args[1]);
            for (const auto& shape : list)
                if (shape->name() == name) throw std::invalid_argument("duplicate message " + name);
            std::vector<loom::Field> declared;
            for (std::size_t i = 2; i < args.size(); ++i) {
                auto [field, type] = split(args[i], ':');
                bool required = true;
                if (!type.empty() && type.back() == '?') { required = false; type.pop_back(); }
                for (const auto& previous : declared)
                    if (previous.name == field) throw std::invalid_argument("duplicate field " + field);
                declared.push_back({std::move(field), loom::type_of(scalar_kind(type)), required});
            }
            list.push_back(loom::make_schema(name, 1, std::move(declared)));
        } else if (verb == "on") {
            exactly(3);
            if (builder_) throw std::invalid_argument("end the current trigger first");
            maker::On candidate;
            candidate.message = find(definition_.accepts, args[1]);
            candidate.output = args[2];
            if (!definition_.state->find(candidate.output)) throw std::invalid_argument("unknown output field");
            const auto pack = maker::pack_schema(definition_.trigger_identity(candidate),
                                                 *definition_.state, *candidate.message);
            auto builder = std::make_unique<op::Builder>(*catalog_, definition_.trigger_identity(candidate), pack->fields());
            pending_ = std::move(candidate);
            builder_ = std::move(builder);
            nodes_.clear();
            result_.reset();
        } else if (verb == "node") {
            need_trigger();
            if (args.size() < 2) throw std::invalid_argument("node needs an operator identity");
            std::vector<op::Builder::Ref> bindings;
            for (std::size_t i = 2; i < args.size(); ++i) {
                const auto& word = args[i];
                if (!word.empty() && word[0] == '$') bindings.push_back(builder_->input(word.substr(1)));
                else if (!word.empty() && word[0] == '%') bindings.push_back(nodes_.at(index_of(word.substr(1))));
                else if (word == "true" || word == "false") bindings.push_back(builder_->constant(word == "true"));
                else bindings.push_back(builder_->constant(integer(word)));
            }
            nodes_.push_back(builder_->call(args[1], bindings));
        } else if (verb == "result") {
            exactly(2); need_trigger();
            const auto index = index_of(args[1]);
            (void)nodes_.at(index);
            result_ = index;
        } else if (verb == "emit") {
            need_trigger();
            if (args.size() < 2) throw std::invalid_argument("emit needs a published message");
            maker::Emit emit;
            emit.message = find(definition_.emits, args[1]);
            for (std::size_t i = 2; i < args.size(); ++i) {
                const auto [field, value] = split(args[i], '=');
                const auto* target = emit.message->find(field);
                if (target == nullptr) throw std::invalid_argument("unknown emitted field " + field);
                if (!value.empty() && value[0] == '$')
                    emit.fields.push_back({field, value.substr(1), std::nullopt});
                else emit.fields.push_back({field, std::nullopt, scalar(target->type.kind, value)});
            }
            pending_.emits.push_back(std::move(emit));
        } else if (verb == "end") {
            exactly(1); need_trigger();
            if (!result_) throw std::invalid_argument("choose a result node before end");
            pending_.body = *std::move(*builder_).result("value", nodes_.at(*result_)).composition();
            auto replacement = definition_.on;
            const auto found = std::find_if(replacement.begin(), replacement.end(), [&](const maker::On& on) {
                return loom::same_identity(*on.message, *pending_.message);
            });
            if (found == replacement.end()) replacement.push_back(pending_);
            else *found = pending_;
            definition_.on = std::move(replacement);
            builder_.reset(); nodes_.clear(); result_.reset();
        } else throw std::invalid_argument("unknown draft command " + verb);
    }

    Project finish() const {
        if (builder_) throw std::invalid_argument("end the current trigger first");
        auto state = initial_ ? *initial_ : maker::default_value(definition_.state);
        for (const auto& [field, value] : defaults_) state.set(field, value);
        return make_project(definition_, std::move(state));
    }

private:
    void need_trigger() const { if (!builder_) throw std::invalid_argument("start a trigger with on"); }
    std::string qualify(const std::string& name) const {
        return name.find('.') == std::string::npos ? definition_.name + "." + name : name;
    }
    std::shared_ptr<const loom::Schema> find(const std::vector<std::shared_ptr<const loom::Schema>>& shapes,
                                           const std::string& name) const {
        for (const auto& shape : shapes) if (shape->name() == qualify(name)) return shape;
        throw std::invalid_argument("unknown message " + name);
    }
    const op::Catalog* catalog_;
    maker::Definition definition_;
    std::optional<loom::Value> initial_;
    std::map<std::string, loom::Cell> defaults_;
    maker::On pending_;
    std::unique_ptr<op::Builder> builder_;
    std::vector<op::Builder::Ref> nodes_;
    std::optional<std::size_t> result_;
};

} // namespace zengine::flow
#endif
