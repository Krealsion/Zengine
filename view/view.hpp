// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_VIEW_HPP
#define ZENGINE_VIEW_VIEW_HPP

// The view renderer: the behaviour of one participant the view host registers from a
// description. It draws the description on its pane's canvas against the latest value it was
// told, keeps focus, caret and field text as its own, publishes an intent when a control is used,
// and shows a refusal answered to it. It holds no business state. Law: agents/view.md.

#include "view/description.hpp"

#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/pointing.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_canvas_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/switchboard/bus.hpp>
#include <zen/switchboard/grant.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/weave_contract.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/shape.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <charconv>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace zengine::view {
namespace ws = zengine::workshop;
namespace ink = zengine::surface::role;

/// The office every pane answers, and the key of the one pane a view offers in its own office.
inline constexpr const char* kWorkshopRole = "zengine.workshop";
inline constexpr const char* kPane = "view";
/// The intents a view remembers, to know a refusal answered to one of them.
inline constexpr std::size_t kRememberedIntents = 16;

namespace detail {
/// The host's two words to a view it registered, sent with no sender.
struct ViewStart {
    ZEN_SHAPE(ViewStart, 1);
};
struct ViewRedraw {
    ZEN_SHAPE(ViewRedraw, 1);
};
} // namespace detail

/// A place in the picture and the element drawn there.
struct Hit {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::string element;
};

/// One picture as it was sent, with the map a press is read against.
struct Picture {
    ws::PaneCanvasContent content;
    std::vector<Hit> hits;
    std::int64_t grain = 1;
    const Hit* hit(std::int64_t x, std::int64_t y) const {
        for (auto at = hits.rbegin(); at != hits.rend(); ++at)
            if (surface::sub_span_contains(at->x, at->w, x, grain) &&
                surface::sub_span_contains(at->y, at->h, y, grain))
                return &*at;
        return nullptr;
    }
};

/// The view's own presentation: the text each number field holds, which has focus, and the
/// sentence on its notice row. None of it is business state.
struct Presentation {
    std::map<std::string, component::TextBox> fields;
    std::optional<std::string> focus;
    std::string notice;
    bool alert = false;
};

/// The values a view was told, by shape: the latest of each, and nothing it was not told.
using Told = std::vector<loom::Value>;

/// A told cell as the label shows it.
inline std::string shown_text(const loom::Cell& cell) {
    switch (cell.kind()) {
    case loom::Kind::Int: return std::to_string(cell.as_int());
    case loom::Kind::Bool: return cell.as_bool() ? "true" : "false";
    case loom::Kind::Float: {
        auto text = std::to_string(cell.as_float());
        while (text.size() > 1 && text.back() == '0' && text[text.size() - 2] != '.') text.pop_back();
        return text;
    }
    case loom::Kind::Text: {
        auto text = cell.as_text();
        for (auto& c : text)
            if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) c = '?';
        return text;
    }
    default: return "?";
    }
}

/// The latest value told at `shape`, if any.
inline const loom::Value* told_at(const Told& told, const loom::Schema& shape) {
    for (const auto& value : told)
        if (loom::same_identity(value.schema(), shape)) return &value;
    return nullptr;
}

/// What the notice row says when nothing else is to be said: what the view still waits to be told.
inline std::string waiting_sentence(const Description& d, const Told& told) {
    std::string missing;
    for (const auto& shape : d.told())
        if (!told_at(told, *shape)) missing += (missing.empty() ? "" : ", ") + shape->name();
    return missing.empty() ? std::string() : "waiting to be told " + missing;
}

/// The most rows the notice takes from the bottom of the room.
inline constexpr std::size_t kNoticeRows = 3;

/// `text` in lines of at most `columns`, broken at spaces where it can be, at most `rows` of them;
/// a last line that could not hold the rest ends in `...`.
inline std::vector<std::string> wrap(std::string text, std::int64_t columns, std::size_t rows) {
    std::vector<std::string> out;
    const auto width = static_cast<std::size_t>(std::max<std::int64_t>(4, columns));
    while (!text.empty() && out.size() < rows) {
        if (text.size() <= width) {
            out.push_back(std::move(text));
            return out;
        }
        if (out.size() + 1 == rows) {
            out.push_back(text.substr(0, width - 3) + "...");
            return out;
        }
        auto cut = text.rfind(' ', width);
        if (cut == std::string::npos || cut < width / 2) cut = width;
        out.push_back(text.substr(0, cut));
        text = text.substr(cut < text.size() && text[cut] == ' ' ? cut + 1 : cut);
    }
    return out;
}

/// A number field's text box: its own, or the text it starts with.
inline component::TextBox field_box(const Presentation& p, const Element& e) {
    const auto found = p.fields.find(e.id);
    if (found != p.fields.end()) return found->second;
    component::TextBox box;
    box.set(e.text, e.text.size());
    return box;
}

/// THE PICTURE: the description against what it was told, in its room. Whole pixels become
/// subunits (`surface::subs_of_pixel`); the medium floors them to its grain, so a terminal shows
/// the same picture in cells, and each line is fitted to what the medium measures.
inline Picture picture(const Description& d, const Told& told, const Presentation& p,
                       const ws::PaneCanvasRoom& room, std::int64_t number) {
    Picture out;
    out.content.pane = kPane;
    out.content.grant = room.grant;
    out.content.picture = number;
    out.grain = std::max<std::int64_t>(1, room.grain);
    out.content.rects.push_back({0, 0, room.width, room.height, ink::kGround});
    const auto metrics = ws::canvas_text_metrics(room);
    const auto line = surface::add_cells(metrics.line, 2 * metrics.inset);
    // The notice row is the room's last rows: a long sentence, a refusal in its owner's words,
    // goes on above rather than being cut where the room ends, as far as the elements leave room.
    const auto columns = std::max<std::int64_t>(
        1, (room.width - 2 * metrics.inset) / std::max<std::int64_t>(1, metrics.advance));
    std::int64_t bottom = 0;
    for (const auto& e : d.elements) bottom = std::max(bottom, surface::subs_of_pixel(e.y + e.h));
    const auto free_rows = static_cast<std::size_t>(
        std::max<std::int64_t>(1, (room.height - bottom) / std::max<std::int64_t>(1, line)));
    const auto notice_lines = wrap(p.notice.empty() ? waiting_sentence(d, told) : p.notice, columns,
                                   std::min(kNoticeRows, free_rows));
    const auto notice_y = std::max<std::int64_t>(
        0, surface::floor_div_px(room.height - line * static_cast<std::int64_t>(notice_lines.size()),
                                 out.grain) * out.grain);
    const auto text = [&](std::int64_t x, std::int64_t y, std::string words, std::int64_t role,
                          ws::CanvasTextBox clip, std::int64_t caret = surface::kNoCaret) {
        ws::PaneCanvasText run{x, y, std::move(words), role, caret};
        auto placed = ws::clip_canvas_text(run, clip, room);
        if (placed.visible()) out.content.texts.push_back(placed.text);
    };
    // A line sits on the medium's own lattice inside its element, so a terminal's cell holds it
    // wherever in the cell the element's pixels begin; centred in what the element leaves.
    const auto down = [&](std::int64_t v) { return surface::floor_div_px(v, out.grain) * out.grain; };
    const auto up = [&](std::int64_t v) { return down(v) == v ? v : down(v) + out.grain; };
    const auto centred = [&](const ws::CanvasTextBox& box) {
        const auto top = up(box.y), room_left = down(surface::add_cells(box.y, box.h)) - top;
        return room_left < line ? top : top + down((room_left - line) / 2);
    };
    for (const auto& e : d.elements) {
        const auto x = surface::subs_of_pixel(e.x), y = surface::subs_of_pixel(e.y);
        const auto w = surface::subs_of_pixel(e.w), h = surface::subs_of_pixel(e.h);
        // Elements sit above the notice row; one placed lower is clipped there, never over it.
        const ws::CanvasTextBox clip{x, y, w, std::min(h, notice_y - y)};
        const auto middle = centred(clip);
        const auto inset = up(x + surface::subs_of_pixel(4)) - x;
        if (e.kind == Kind::number) {
            const bool focused = p.focus && *p.focus == e.id;
            out.content.rects.push_back({x, y, w, std::min(h, notice_y - y),
                                         focused ? ink::kAccent : ink::kMuted});
            const auto box = field_box(p, e);
            const auto caption = e.label.empty() ? std::string() : e.label + ": ";
            text(x + inset, middle, caption + box.text(), ink::kFill, clip,
                 focused ? static_cast<std::int64_t>(caption.size() + box.caret()) : surface::kNoCaret);
        } else if (e.kind == Kind::button) {
            out.content.rects.push_back({x, y, w, std::min(h, notice_y - y), ink::kAccent});
            text(x + inset, middle, e.label, ink::kFill, clip);
        } else {
            std::string words = e.label;
            std::int64_t role = ink::kFill;
            if (const auto* s = d.shown(e.id)) {
                const auto* value = told_at(told, *s->shape);
                const auto* cell = value ? value->get(s->field) : nullptr;
                words += (words.empty() ? "" : ": ") + (cell ? shown_text(*cell) : std::string("waiting"));
                if (!cell) role = ink::kMuted;
            }
            text(up(x), middle, std::move(words), role, clip);
        }
        if (h > 0 && w > 0 && y < notice_y) out.hits.push_back({x, y, w, std::min(h, notice_y - y), e.id});
    }
    for (std::size_t i = 0; i < notice_lines.size(); ++i) {
        const auto y = notice_y + line * static_cast<std::int64_t>(i);
        text(0, y, notice_lines[i], p.alert ? ink::kAlert : ink::kMuted, {0, y, room.width, line});
    }
    // Rectangles a room cannot hold are dropped: a picture is admitted whole or not at all.
    std::erase_if(out.content.rects, [](const auto& r) { return r.w <= 0 || r.h <= 0; });
    return out;
}

/// The picture a stopped view leaves: no element, and the sentence that it stopped.
inline ws::PaneCanvasContent stopped_picture(const Description& d, const ws::PaneCanvasRoom& room,
                                             std::int64_t number, const std::string& why) {
    Presentation p;
    p.notice = d.name + " " + why;
    Description bare;
    bare.name = d.name;
    return picture(bare, {}, p, room, number).content;
}

/// The preferred size a view offers, in text rows and columns: its elements and its notice rows.
inline std::pair<std::int64_t, std::int64_t> preferred_size(const Description& d) {
    std::int64_t right = 0, bottom = 0;
    for (const auto& e : d.elements) {
        right = std::max(right, e.x + e.w);
        bottom = std::max(bottom, e.y + e.h);
    }
    const auto cells = [](std::int64_t px) { return (px + surface::kCanvasCellPx - 1) / surface::kCanvasCellPx; };
    return {std::clamp<std::int64_t>(cells(bottom) + static_cast<std::int64_t>(kNoticeRows) + 1, 4, 60),
            std::clamp<std::int64_t>(cells(right) + 2, 24, 200)};
}

/// The grant a description implies: each intent it says to any accepter, and the pane
/// conversation to Workshop's office alone. Nothing else, whoever asks.
inline loom::Grant view_grant(const Description& d) {
    loom::Grant grant;
    for (const auto& shape : d.says()) grant.allow_to_any(shape->name(), shape->version());
    const auto to_workshop = [&grant](const char* name, std::uint32_t version) {
        grant.allow_to_role(name, version, kWorkshopRole);
    };
    to_workshop(ws::v2::PaneOffered::zen_name, ws::v2::PaneOffered::zen_version);
    to_workshop(ws::PaneContent::zen_name, ws::PaneContent::zen_version);
    to_workshop(ws::PaneCanvasContent::zen_name, ws::PaneCanvasContent::zen_version);
    to_workshop(ws::PaneEscapeUnspent::zen_name, ws::PaneEscapeUnspent::zen_version);
    to_workshop(ws::PanePassRequested::zen_name, ws::PanePassRequested::zen_version);
    to_workshop(ws::PaneRevealRequested::zen_name, ws::PaneRevealRequested::zen_version);
    return grant;
}

/// ONE VIEW, registered by the view host. Its accept-set is the shapes it is told and the pane
/// conversation; its emit-set is its intents and that conversation. It speaks to Workshop as its
/// office, the view's name, and says its intents by publication: whoever accepts one hears it.
class View final : public loom::Weave {
public:
    explicit View(Description description) : description_(std::move(description)) {}

    void set_self(loom::WeaveId id) noexcept { self_ = id; }
    const Description& description() const noexcept { return description_; }
    const Told& told() const noexcept { return told_; }
    const Presentation& presentation() const noexcept { return presentation_; }
    const ws::PaneCanvasRoom& room() const noexcept { return room_; }

    /// HOST AUTHORITY: a description at the same shapes, taken in place. Field text, focus and
    /// what it was told stay; a field it no longer holds is forgotten.
    void adopt(Description next) {
        description_ = std::move(next);
        std::erase_if(presentation_.fields, [this](const auto& f) {
            const auto* e = description_.element(f.first);
            return !e || e->kind != Kind::number;
        });
        if (presentation_.focus) {
            const auto* e = description_.element(*presentation_.focus);
            if (!e || e->kind != Kind::number) presentation_.focus.reset();
        }
        pictures_.clear();
    }

    /// HOST AUTHORITY: the picture this view leaves when it stops, in the room it holds now, or
    /// none when it holds no canvas.
    std::optional<ws::PaneCanvasContent> farewell(const std::string& why) {
        if (room_.grant <= 0 || room_.width <= 0 || room_.height <= 0) return std::nullopt;
        return stopped_picture(description_, room_, ++picture_number_, why);
    }

    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        auto out = description_.told();
        for (auto s : {loom::schema_of<loom::Refused>(), loom::schema_of<detail::ViewStart>(),
                       loom::schema_of<detail::ViewRedraw>(),
                       loom::schema_of<ws::PaneCatalogRequested>(), loom::schema_of<ws::PaneRoom>(),
                       loom::schema_of<ws::PaneCanvasRoom>(), loom::schema_of<ws::PaneCanvasPointer>(),
                       loom::schema_of<ws::PaneCanvasRejected>(), loom::schema_of<ws::PaneKey>(),
                       loom::schema_of<ws::PaneTextInput>(), loom::schema_of<ws::PaneRevealAnswered>()})
            out.push_back(std::move(s));
        return out;
    }

    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        auto out = description_.says();
        for (auto s : {loom::schema_of<ws::v2::PaneOffered>(), loom::schema_of<ws::PaneContent>(),
                       loom::schema_of<ws::PaneCanvasContent>(), loom::schema_of<ws::PaneEscapeUnspent>(),
                       loom::schema_of<ws::PanePassRequested>(), loom::schema_of<ws::PaneRevealRequested>()})
            out.push_back(std::move(s));
        return out;
    }

    void handle(const loom::Message& in, loom::Bus& bus) override {
        const auto& shape = in.payload.schema();
        const auto is = [&shape](const auto& s) { return loom::same_identity(shape, *s); };
        const bool host = !in.sender.valid();
        const bool workshop = in.provenance.authored_role() == kWorkshopRole;
        if (is(loom::schema_of<detail::ViewStart>())) {
            if (!host) return;
            offer(bus);
            say(bus, ws::PaneRevealRequested{kPane});
            return;
        }
        if (is(loom::schema_of<detail::ViewRedraw>())) {
            if (host) show(bus);
            return;
        }
        if (is(loom::schema_of<loom::Refused>())) {
            // A refusal answers one of this view's own intents, by the correlation it was said on.
            if (std::find(said_.begin(), said_.end(), in.correlation) == said_.end()) return;
            const auto refused = loom::from_value<loom::Refused>(in.payload);
            presentation_.notice = "refused: " + refused.reason;
            presentation_.alert = true;
            show(bus);
            return;
        }
        for (const auto& told : description_.told())
            if (is(told)) {
                std::erase_if(told_, [&](const auto& v) { return loom::same_identity(v.schema(), *told); });
                told_.push_back(in.payload);
                if (presentation_.alert && std::find(said_.begin(), said_.end(), in.correlation) != said_.end())
                    presentation_.alert = false;
                if (!presentation_.alert) presentation_.notice.clear();
                show(bus);
                return;
            }
        if (!workshop) return;
        if (is(loom::schema_of<ws::PaneCatalogRequested>())) {
            offer(bus);
        } else if (is(loom::schema_of<ws::PaneCanvasRoom>())) {
            const auto room = loom::from_value<ws::PaneCanvasRoom>(in.payload);
            if (room.pane != kPane) return;
            room_ = room;
            pictures_.clear();
            show(bus);
        } else if (is(loom::schema_of<ws::PaneRoom>())) {
            const auto room = loom::from_value<ws::PaneRoom>(in.payload);
            if (room.pane != kPane) return;
            rows_ = room.rows;
            columns_ = room.columns;
            show(bus);
        } else if (is(loom::schema_of<ws::PaneCanvasRejected>())) {
            const auto rejected = loom::from_value<ws::PaneCanvasRejected>(in.payload);
            if (rejected.pane == kPane && rejected.grant == room_.grant) {
                presentation_.notice = "drawing rejected: " + rejected.reason;
                presentation_.alert = true;
            }
        } else if (is(loom::schema_of<ws::PaneRevealAnswered>())) {
            const auto answer = loom::from_value<ws::PaneRevealAnswered>(in.payload);
            if (answer.pane == kPane && !answer.seated) {
                presentation_.notice = "no seat on the desk: " + answer.refusal;
                presentation_.alert = true;
                show(bus);
            }
        } else if (is(loom::schema_of<ws::PaneCanvasPointer>())) {
            pointer(loom::from_value<ws::PaneCanvasPointer>(in.payload), in, bus);
        } else if (is(loom::schema_of<ws::PaneKey>())) {
            key(loom::from_value<ws::PaneKey>(in.payload), in, bus);
        } else if (is(loom::schema_of<ws::PaneTextInput>())) {
            const auto typed = loom::from_value<ws::PaneTextInput>(in.payload);
            if (typed.pane != kPane || !presentation_.focus) return;
            auto& box = field(*presentation_.focus);
            if (box.size() + typed.text.size() > kMaxFieldTextBytes) {
                presentation_.notice = "a number field holds at most " +
                                       std::to_string(kMaxFieldTextBytes) + " bytes";
                presentation_.alert = true;
            } else {
                box.type(typed.text);
            }
            show(bus);
        }
    }

    loom::Value snapshot() const override {
        return loom::Value(loom::make_schema("zengine.view.View", 1, {}));
    }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value&) override {}

private:
    template <class T>
    void say(loom::Bus& bus, const T& value, std::uint64_t correlation = 0) {
        (void)bus.office_send_to_role(description_.name, kWorkshopRole,
                                      loom::Message(loom::to_value(value), self_, {}, correlation));
    }

    void offer(loom::Bus& bus) {
        const auto [rows, columns] = preferred_size(description_);
        say(bus, ws::v2::PaneOffered{kPane, description_.name, "a view made in the View Builder",
                                     rows, columns});
    }

    component::TextBox& field(const std::string& id) {
        const auto found = presentation_.fields.find(id);
        if (found != presentation_.fields.end()) return found->second;
        const auto* e = description_.element(id);
        return presentation_.fields.emplace(id, field_box(presentation_, *e)).first->second;
    }

    void show(loom::Bus& bus) {
        if (room_.grant > 0 && room_.width > 0 && room_.height > 0) {
            auto current = picture(description_, told_, presentation_, room_, ++picture_number_);
            say(bus, current.content);
            pictures_.push_back(std::move(current));
            while (pictures_.size() > 8) pictures_.pop_front();
        } else if (rows_ > 0 && columns_ > 0) {
            std::string row = description_.name + " draws on a canvas-capable Workshop";
            if (static_cast<std::int64_t>(row.size()) > columns_) row.resize(static_cast<std::size_t>(columns_));
            say(bus, ws::PaneContent{kPane, {surface::SurfaceTextRow{row, ink::kMuted}}});
        }
    }

    void pointer(const ws::PaneCanvasPointer& event, const loom::Message& in, loom::Bus& bus) {
        if (event.pane != kPane || event.grant != room_.grant || event.phase != ws::canvas_pointer::kPress)
            return;
        if (event.button == 3) {
            say(bus, ws::PanePassRequested{kPane}, in.correlation);
            return;
        }
        if (event.button != 1) return;
        const auto pictured = std::find_if(pictures_.begin(), pictures_.end(), [&](const auto& p) {
            return p.content.picture == event.picture;
        });
        if (pictured == pictures_.end()) return;
        const auto* hit = pictured->hit(event.x, event.y);
        const auto* e = hit ? description_.element(hit->element) : nullptr;
        if (!e) {
            presentation_.focus.reset();
        } else if (e->kind == Kind::number) {
            presentation_.focus = e->id;
            auto& box = field(e->id);
            box.place(box.size());
        } else if (e->kind == Kind::button) {
            use(*e, bus);
        }
        show(bus);
    }

    void key(const ws::PaneKey& k, const loom::Message& in, loom::Bus& bus) {
        if (k.pane != kPane) return;
        if (k.scancode == input::scan::kTab) {
            focus_next();
        } else if (k.scancode == input::scan::kEscape) {
            if (!presentation_.focus) {
                say(bus, ws::PaneEscapeUnspent{kPane}, in.correlation);
                return;
            }
            presentation_.focus.reset();
        } else if (k.scancode == input::scan::kReturn) {
            // Return uses the first control whose intent takes the focused field.
            for (const auto& intent : description_.intents)
                for (const auto& f : intent.fields)
                    if (presentation_.focus && f.element == *presentation_.focus) {
                        use(*description_.element(intent.control), bus);
                        show(bus);
                        return;
                    }
            return;
        } else if (presentation_.focus) {
            if (!field(*presentation_.focus).consume(k.scancode, k.modifiers, clipboard_)) return;
        } else {
            return;
        }
        show(bus);
    }

    void focus_next() {
        std::vector<std::string> numbers;
        for (const auto& e : description_.elements)
            if (e.kind == Kind::number) numbers.push_back(e.id);
        if (numbers.empty()) return;
        auto at = presentation_.focus ? std::find(numbers.begin(), numbers.end(), *presentation_.focus)
                                      : numbers.end();
        presentation_.focus = (at == numbers.end() || at + 1 == numbers.end()) ? numbers.front() : *(at + 1);
        auto& box = field(*presentation_.focus);
        box.place(box.size());
    }

    /// A CONTROL USED: its intent, every field read from the number field it names, published as
    /// this participant. A field that holds no integer is said on the notice row, and nothing is
    /// published.
    void use(const Element& control, loom::Bus& bus) {
        const auto* intent = description_.intent(control.id);
        if (!intent) {
            presentation_.notice = (control.label.empty() ? control.id : control.label) + " says nothing yet";
            presentation_.alert = false;
            return;
        }
        loom::Value value(intent->shape);
        for (const auto& f : intent->fields) {
            const auto& text = field(f.element).text();
            std::int64_t number = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
            if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
                presentation_.notice = "`" + f.element + "` holds `" + text + "`, not a whole number";
                presentation_.alert = true;
                return;
            }
            value.set(f.field, loom::Cell::integer(number));
        }
        const auto correlation = ++correlation_;
        said_.push_back(correlation);
        while (said_.size() > kRememberedIntents) said_.pop_front();
        const auto heard = bus.publish(loom::Message(std::move(value), self_, {}, correlation));
        presentation_.notice = "said " + intent->shape->name() +
                               (heard == 0 ? "; nothing accepts it" : "");
        presentation_.alert = heard == 0;
    }

    Description description_;
    loom::WeaveId self_{};
    Told told_;
    Presentation presentation_;
    ws::PaneCanvasRoom room_;
    std::int64_t rows_ = 0, columns_ = 0, picture_number_ = 0;
    std::deque<Picture> pictures_;
    std::deque<std::uint64_t> said_;
    std::uint64_t correlation_ = 0;
    component::Clipboard clipboard_;
};

} // namespace zengine::view
#endif
