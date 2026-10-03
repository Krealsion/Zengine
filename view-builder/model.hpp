// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_BUILDER_MODEL_HPP
#define ZENGINE_VIEW_BUILDER_MODEL_HPP

// The View Builder's semantic edits over one view description: elements, their values, places and
// sizes, the field a label shows, the intent a button says, and the file. Every edit is taken
// whole or refused whole, and no edit leaves a description that `view::problem` refuses. An
// intent is made through Flow's one shape model. Law: agents/view.md.

#include "flow/shape.hpp"
#include "view/description.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace zengine::view_builder {

enum class Effect { None, Run, Apply, Stop };
struct Action {
    Effect effect = Effect::None;
    loom::Bytes payload;
};
/// A shape carried onto a label that holds more than one field it could show: the weaver says
/// which. Another edit puts it down.
struct Choosing {
    std::string element;
    std::shared_ptr<const loom::Schema> shape;
};

/// The fields of `shape` a label can show: its scalar fields, in order.
inline std::vector<std::string> showable(const loom::Schema& shape) {
    std::vector<std::string> out;
    for (const auto& f : shape.fields())
        if (f.type.kind == loom::Kind::Int || f.type.kind == loom::Kind::Float ||
            f.type.kind == loom::Kind::Text || f.type.kind == loom::Kind::Bool)
            out.push_back(f.name);
    return out;
}

inline std::int64_t whole(const std::string& text, const char* what) {
    std::int64_t out = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), out);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument(std::string(what) + " is a whole number; `" + text + "` is not");
    return out;
}

/// The size an element of each kind is made at, in whole pixels.
inline std::pair<std::int64_t, std::int64_t> made_size(view::Kind kind) {
    return {kind == view::Kind::number ? 144 : kind == view::Kind::button ? 96 : 192, 24};
}

class Model {
public:
    view::Description description;
    std::string path, notice = "Drag a kind onto the canvas, make an intent from the number fields, then Run.";
    bool dirty = false, running = false;
    std::optional<std::size_t> selected;
    std::optional<Choosing> choosing;
    std::size_t first_row = 0;

    Model() { description.name = "my.view"; }

    /// One edit, whole: on a refusal the model is as it was.
    Action command(const std::string& action, const std::vector<std::string>& args = {}) {
        Model candidate = *this;
        auto result = candidate.perform(action, args);
        if (auto why = view::problem(candidate.description); !why.empty()) throw std::invalid_argument(why);
        *this = std::move(candidate);
        return result;
    }

    /// A shape carried onto the label at `index`: shown at once when the carry named its field or
    /// the shape holds one field a label can show; otherwise the weaver chooses among them.
    void carried(std::size_t index, std::shared_ptr<const loom::Schema> shape, std::optional<std::string> field) {
        const auto& e = description.elements.at(index);
        if (e.kind != view::Kind::label)
            throw std::invalid_argument("`" + e.id + "` is a " + view::kind_word(e.kind) +
                                        "; carry a shape onto a label to show one of its fields");
        const auto fields = showable(*shape);
        if (fields.empty())
            throw std::invalid_argument(shape->name() + " has no Int, Float, Text or Bool field to show");
        selected = index;
        if (!field && fields.size() == 1) field = fields.front();
        if (!field) {
            choosing = Choosing{e.id, std::move(shape)};
            notice = "Choose which field of " + choosing->shape->name() + " `" + e.id + "` shows";
            return;
        }
        Model candidate = *this;
        candidate.choosing = Choosing{e.id, std::move(shape)};
        (void)candidate.perform("show", {std::to_string(index), *field});
        if (auto why = view::problem(candidate.description); !why.empty()) throw std::invalid_argument(why);
        *this = std::move(candidate);
    }

private:
    view::Element& at(const std::string& index) {
        const auto i = static_cast<std::size_t>(whole(index, "an element's index"));
        if (i >= description.elements.size()) throw std::invalid_argument("no element " + index);
        return description.elements[i];
    }
    std::size_t index_of(const std::string& index) const {
        const auto i = static_cast<std::size_t>(whole(index, "an element's index"));
        if (i >= description.elements.size()) throw std::invalid_argument("no element " + index);
        return i;
    }
    std::string fresh_id(const char* stem) const {
        for (int n = 1;; ++n) {
            const auto id = std::string(stem) + std::to_string(n);
            if (!description.element(id)) return id;
        }
    }
    void touched() { dirty = true; choosing.reset(); }
    /// An element renamed keeps what names it: the field it shows, the intent it says, the intent
    /// fields it fills. An intent field keeps its own name.
    void renamed(const std::string& before, const std::string& after) {
        for (auto& s : description.shows)
            if (s.element == before) s.element = after;
        for (auto& intent : description.intents) {
            if (intent.control == before) intent.control = after;
            for (auto& f : intent.fields)
                if (f.element == before) f.element = after;
        }
    }

    Action perform(const std::string& action, const std::vector<std::string>& args) {
        const auto need = [&](std::size_t size) {
            if (args.size() != size)
                throw std::invalid_argument(action + " expects " + std::to_string(size) + " arguments");
        };
        const auto previous = notice;
        if (action == "describe") {
            notice = "new(name,discard), rename(name), add(label|number|button[,x,y]), select(index), "
                     "set(index,id|label|text|x|y|w|h,value), place(index,x,y,w,h), "
                     "element(index,id,label,x,y,w,h,text), up(index), down(index), remove(index), "
                     "intent(index,name), drop-intent(index), show(index,field), unshow(index), "
                     "path(path), save([path]), open(path,discard), run, apply, stop";
            return {};
        }
        if (action == "new") {
            need(2);
            if (dirty && args[1] != "discard") throw std::invalid_argument("save the view or explicitly choose discard");
            if (running) throw std::invalid_argument("stop the running view before starting another");
            description = view::Description{};
            description.name = args[0];
            selected.reset();
            choosing.reset();
            path.clear();
            dirty = true;
        } else if (action == "rename") {
            need(1);
            // Each intent stays inside the view's name: its shape is made again under the new one.
            const auto old_prefix = description.name + ".";
            for (auto& intent : description.intents) {
                const auto short_name = intent.shape->name().substr(old_prefix.size());
                intent.shape = flow::shape::make(flow::shape::qualified(args[0], short_name), intent.shape->fields());
            }
            description.name = args[0];
            touched();
        } else if (action == "add") {
            // MADE BELOW THE LAST, OR WHERE IT WAS DROPPED, in whole pixels.
            if (args.size() != 1 && args.size() != 3) throw std::invalid_argument("add expects a kind, and a place or none");
            const auto kind = view::kind_of(args[0]);
            if (!kind) throw std::invalid_argument("an element is a label, a number field or a button");
            if (description.elements.size() >= view::kMaxElements)
                throw std::invalid_argument("a view holds at most " + std::to_string(view::kMaxElements) + " elements");
            std::int64_t below = 0;
            for (const auto& e : description.elements) below = std::max(below, e.y + e.h + 4);
            view::Element e;
            e.kind = *kind;
            e.id = fresh_id(*kind == view::Kind::number ? "field" : *kind == view::Kind::button ? "button" : "label");
            e.label = e.id;
            e.x = args.size() == 3 ? whole(args[1], "x") : 0;
            e.y = args.size() == 3 ? whole(args[2], "y") : below;
            std::tie(e.w, e.h) = made_size(*kind);
            notice = "Made " + e.id + " at " + std::to_string(e.x) + "," + std::to_string(e.y);
            description.elements.push_back(std::move(e));
            selected = description.elements.size() - 1;
            touched();
        } else if (action == "select") {
            need(1);
            selected = index_of(args[0]);
            choosing.reset();
            notice = description.elements[*selected].id + ": drag it to move it, a side or a corner to resize it, or type its values";
        } else if (action == "set") {
            // ONE VALUE, TYPED INTO ITS BOX: the element keeps every other value.
            need(3);
            auto& e = at(args[0]);
            const auto& field = args[1];
            const auto& value = args[2];
            if (field == "id") {
                const auto before = e.id;
                e.id = value;
                renamed(before, e.id);
            } else if (field == "label") {
                e.label = value;
            } else if (field == "text") {
                if (e.kind != view::Kind::number)
                    throw std::invalid_argument("`" + e.id + "` is a " + view::kind_word(e.kind) +
                                                "; only a number field starts with text");
                e.text = value;
            } else if (field == "x") {
                e.x = whole(value, "x");
            } else if (field == "y") {
                e.y = whole(value, "y");
            } else if (field == "w") {
                e.w = whole(value, "w");
            } else if (field == "h") {
                e.h = whole(value, "h");
            } else {
                throw std::invalid_argument("an element's values are id, label, text, x, y, w and h");
            }
            selected = index_of(args[0]);
            touched();
            notice = e.id + "'s " + field + " is " + value;
        } else if (action == "place") {
            // A DRAG'S PLACE AND SIZE, in whole pixels.
            need(5);
            auto& e = at(args[0]);
            e.x = whole(args[1], "x");
            e.y = whole(args[2], "y");
            e.w = whole(args[3], "w");
            e.h = whole(args[4], "h");
            selected = index_of(args[0]);
            touched();
            notice = e.id + " at " + std::to_string(e.x) + "," + std::to_string(e.y) + ", " +
                     std::to_string(e.w) + " by " + std::to_string(e.h);
        } else if (action == "element") {
            if (args.size() != 7 && args.size() != 8) throw std::invalid_argument("element expects index, id, label, x, y, w, h and a number field's text");
            auto& e = at(args[0]);
            const auto before = e.id;
            e.id = args[1];
            e.label = args[2];
            e.x = whole(args[3], "x");
            e.y = whole(args[4], "y");
            e.w = whole(args[5], "w");
            e.h = whole(args[6], "h");
            e.text = args.size() == 8 && e.kind == view::Kind::number ? args[7] : std::string();
            renamed(before, e.id);
            touched();
        } else if (action == "up" || action == "down") {
            need(1);
            const auto i = index_of(args[0]);
            const auto j = action == "up" ? (i == 0 ? i : i - 1) : std::min(i + 1, description.elements.size() - 1);
            std::swap(description.elements[i], description.elements[j]);
            selected = j;
            touched();
        } else if (action == "remove") {
            need(1);
            const auto i = index_of(args[0]);
            const auto id = description.elements[i].id;
            const auto uses = [&](const view::Intent& intent) {
                return intent.control == id || std::any_of(intent.fields.begin(), intent.fields.end(),
                                                           [&](const auto& f) { return f.element == id; });
            };
            std::string dropped;
            for (const auto& intent : description.intents)
                if (uses(intent)) dropped += (dropped.empty() ? "" : ", ") + intent.shape->name();
            std::erase_if(description.intents, uses);
            std::erase_if(description.shows, [&](const auto& s) { return s.element == id; });
            description.elements.erase(description.elements.begin() + static_cast<std::ptrdiff_t>(i));
            selected.reset();
            touched();
            notice = "Removed " + id + (dropped.empty() ? std::string() : " and the intent it was part of: " + dropped);
        } else if (action == "intent") {
            // MAKE AN INTENT FROM THE VIEW'S FIELDS: a shape inside the view's name, one required Int
            // per number field, made through Flow's shape model, said by this button.
            need(2);
            auto& control = at(args[0]);
            if (control.kind != view::Kind::button)
                throw std::invalid_argument("`" + control.id + "` is a " + view::kind_word(control.kind) + "; only a button says an intent");
            const auto name = flow::shape::qualified(description.name, args[1]);
            if (name.rfind(description.name + ".", 0) != 0)
                throw std::invalid_argument("an intent's name stays inside the view's: write it without a `.`");
            std::vector<std::shared_ptr<const loom::Schema>> others;
            for (const auto& intent : description.intents)
                if (intent.control != control.id) others.push_back(intent.shape);
            flow::shape::refuse_taken(others, name);
            auto shape = flow::shape::make(name);
            view::Intent made{control.id, nullptr, {}};
            for (const auto& e : description.elements) {
                if (e.kind != view::Kind::number) continue;
                shape = flow::shape::with_field(*shape, e.id, loom::type_of(loom::Kind::Int), true);
                made.fields.push_back({e.id, e.id});
            }
            if (made.fields.empty()) throw std::invalid_argument("add a number field first: an intent is made from the view's fields");
            made.shape = std::move(shape);
            std::erase_if(description.intents, [&](const auto& i) { return i.control == control.id; });
            description.intents.push_back(std::move(made));
            selected = index_of(args[0]);
            touched();
            notice = "`" + control.id + "` says " + name;
        } else if (action == "drop-intent") {
            need(1);
            const auto& control = at(args[0]);
            std::erase_if(description.intents, [&](const auto& i) { return i.control == control.id; });
            touched();
        } else if (action == "show") {
            need(2);
            auto& e = at(args[0]);
            if (!choosing || choosing->element != e.id)
                throw std::invalid_argument("carry a shape or a field onto `" + e.id + "` first");
            const auto shape = choosing->shape;
            std::erase_if(description.shows, [&](const auto& s) { return s.element == e.id; });
            description.shows.push_back({e.id, shape, args[1]});
            touched();
            notice = "`" + e.id + "` shows " + shape->name() + "." + args[1];
        } else if (action == "unshow") {
            need(1);
            const auto& e = at(args[0]);
            std::erase_if(description.shows, [&](const auto& s) { return s.element == e.id; });
            touched();
        } else if (action == "path") {
            need(1);
            path = args[0];
            notice = path.empty() ? "No file: name one to save the view to" : "Save writes, and Open reads, " + path;
            return {};
        } else if (action == "save") {
            if (args.size() > 1) throw std::invalid_argument("save expects a path, or none for the view's file");
            const auto to = args.empty() ? path : args[0];
            if (to.empty()) throw std::invalid_argument("name a file to save the view to");
            view::save_description(to, description);
            path = to;
            dirty = false;
            notice = "Saved " + path;
            return {};
        } else if (action == "open") {
            need(2);
            if (dirty && args[1] != "discard") throw std::invalid_argument("save the view or explicitly choose discard");
            if (running) throw std::invalid_argument("stop the running view before opening another");
            description = view::open_description(args[0]);
            path = args[0];
            dirty = false;
            selected.reset();
            choosing.reset();
            notice = "Opened " + path;
        } else if (action == "run" || action == "apply") {
            need(0);
            const auto bytes = view::description_bytes(description);
            return {action == "run" ? Effect::Run : Effect::Apply, loom::Bytes(bytes.begin(), bytes.end())};
        } else if (action == "stop") {
            need(0);
            return {Effect::Stop, {}};
        } else {
            throw std::invalid_argument("unknown View Builder edit: " + action);
        }
        if (notice == previous) notice = action + " complete";
        return {};
    }
};

} // namespace zengine::view_builder
#endif
