// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_VIEW_VIEW_HPP
#define ZENGINE_VIEW_VIEW_HPP

// The view renderer: the behaviour of one participant the view host registers from a
// description. It draws the description in the graphics window that shows its area, against the
// latest value it was told, keeps focus, caret and field text as its own, publishes an intent when
// a control is used, and shows a refusal answered to it. It holds no business state; an owner in
// its process may pair a `Presenter`. Law: agents/view.md.

#include "view/description.hpp"

#include "component/text_box.hpp"
#include "graphics/canvas.hpp"
#include "graphics/pixels.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "surface/pointing.hpp"
#include "workshop/pane_canvas_drawing.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_picture.hpp"
#include "workshop/pane_causes.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include <zen/switchboard/bus.hpp>
#include <zen/switchboard/grant.hpp>
#include <zen/switchboard/message.hpp>
#include <zen/switchboard/weave_contract.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/shape.hpp>
#include <zen/weave/standard_shapes.hpp>
#include <zen/weave/weave.hpp>

#include <cctype>
#include <charconv>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace zengine::view {
namespace ws = zengine::workshop;
namespace ink = zengine::graphics::role;

/// The office every pane answers, and the key of the one pane a view offers in its own office.
inline constexpr const char* kWorkshopRole = "zengine.workshop";
inline constexpr const char* kPane = "view";
/// The intents a view remembers, to know a refusal answered to one of them.
inline constexpr std::size_t kRememberedIntents = 16;

namespace detail {
/// The host's two words to a view it registered, sent with no sender. A start offers the pane,
/// and asks Workshop to show it when `reveal` says so.
struct ViewStart {
    bool reveal = true;
    ZEN_SHAPE(ViewStart, 2, ZEN_FIELD(reveal));
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

/// One picture as it was drawn, with the map a press is read against, and which element drew each
/// rectangle and each run of text, by index: its id, or "" for the ground and the notice.
struct Picture {
    ws::CanvasPicture content;
    std::vector<Hit> hits;
    std::vector<std::string> rects_by, texts_by;
    std::int64_t grain = 1;
    const Hit* hit(std::int64_t x, std::int64_t y) const {
        for (auto at = hits.rbegin(); at != hits.rend(); ++at)
            if (graphics::px_span_contains(at->x, at->w, x, grain) &&
                graphics::px_span_contains(at->y, at->h, y, grain))
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

/// The notice's rows beneath a view's size, in pixels: `kNoticeRows` lines of the medium's text.
inline std::int64_t notice_band(const ws::CanvasRoom& room) {
    const auto m = ws::canvas_text_metrics(room);
    return static_cast<std::int64_t>(kNoticeRows) * graphics::add_cells(m.line, 2 * m.inset);
}

/// The room a view is drawn in: its size and its notice rows beneath, within the room its area
/// holds.
inline ws::CanvasRoom sized(const Description& d, ws::CanvasRoom room) {
    room.width = std::min(room.width, d.width);
    room.height = std::min(room.height, d.height + notice_band(room));
    return room;
}

/// THE PICTURE: the description against what it was told, laid out in its size with its notice
/// rows beneath, within its room, the rest of the room its ground. Its pixels are the window's;
/// the medium floors them to its grain, so a terminal shows the same picture in cells, and each
/// line is fitted to what the medium measures.
inline Picture picture(const Description& d, const Told& told, const Presentation& p,
                       const ws::CanvasRoom& granted) {
    const auto room = sized(d, granted);
    Picture out;
    out.grain = std::max<std::int64_t>(1, room.grain);
    out.content.rects.push_back({0, 0, granted.width, granted.height, ink::kGround});
    out.rects_by.emplace_back();
    std::string by; // the element drawing now
    const auto metrics = ws::canvas_text_metrics(room);
    const auto line = graphics::add_cells(metrics.line, 2 * metrics.inset);
    // The notice row is the room's last rows, beneath the size: a long sentence, a refusal in its
    // owner's words, goes on above rather than being cut where the room ends, as far as the
    // elements leave room. Only a pane smaller than the view brings it over an element.
    const auto columns = std::max<std::int64_t>(
        1, (room.width - 2 * metrics.inset) / std::max<std::int64_t>(1, metrics.advance));
    std::int64_t bottom = 0;
    for (const auto& e : d.elements) bottom = std::max(bottom, e.y + e.h);
    const auto free_rows = static_cast<std::size_t>(
        std::max<std::int64_t>(1, (room.height - bottom) / std::max<std::int64_t>(1, line)));
    // The notice carries other parties' words -- a shape's name, a refusal -- spelled in what a
    // window draws.
    const auto notice_lines =
        wrap(ws::pane_text::ascii_spelling(p.notice.empty() ? waiting_sentence(d, told) : p.notice),
             columns, std::min(kNoticeRows, free_rows));
    const auto notice_y = std::max<std::int64_t>(
        0, graphics::floor_div_px(room.height - line * static_cast<std::int64_t>(notice_lines.size()),
                                 out.grain) * out.grain);
    const auto text = [&](std::int64_t x, std::int64_t y, std::string words, std::int64_t role,
                          ws::CanvasTextBox clip, std::int64_t caret = ws::kNoCaret) {
        ws::CanvasText run{x, y, std::move(words), role, caret};
        auto placed = ws::clip_canvas_text(run, clip, room);
        if (!placed.visible()) return;
        out.content.texts.push_back(placed.text);
        out.texts_by.push_back(by);
    };
    const auto rect = [&](ws::CanvasRect r) {
        out.content.rects.push_back(r);
        out.rects_by.push_back(by);
    };
    // A line sits on the medium's own lattice inside its element, so a terminal's cell holds it
    // wherever in the cell the element's pixels begin; centred in what the element leaves.
    const auto down = [&](std::int64_t v) { return graphics::floor_div_px(v, out.grain) * out.grain; };
    const auto up = [&](std::int64_t v) { return down(v) == v ? v : down(v) + out.grain; };
    const auto centred = [&](const ws::CanvasTextBox& box) {
        const auto top = up(box.y), room_left = down(graphics::add_cells(box.y, box.h)) - top;
        return room_left < line ? top : top + down((room_left - line) / 2);
    };
    for (const auto& e : d.elements) {
        by = e.id;
        const auto x = e.x, y = e.y, w = e.w, h = e.h;
        // Elements sit above the notice row; one placed lower is clipped there, never over it.
        const ws::CanvasTextBox clip{x, y, w, std::min(h, notice_y - y)};
        const auto middle = centred(clip);
        const auto inset = up(x + 4) - x;
        // A control is a quiet box with its words on it: in a window a filled rectangle, in a
        // terminal a run of the quiet glyph, and either way the words stay readable. Focus is
        // the caret and the accent of a field's words; a button's words are always accented.
        if (e.kind == Kind::number) {
            const bool focused = p.focus && *p.focus == e.id;
            rect({x, y, w, std::min(h, notice_y - y), ink::kMuted});
            const auto box = field_box(p, e);
            const auto caption = e.label.empty() ? std::string() : e.label + ": ";
            const auto caret = static_cast<std::int64_t>(caption.size() + box.caret());
            // A caret after the value stands on a blank of its own where the element has one, and
            // where a character is a cell: a window's caret is a bar and needs none.
            const bool after = focused && !ws::canvas_text_metrics(granted).graphical &&
                               box.caret() >= box.text().size();
            text(x + inset, middle, caption + box.text() + (after ? " " : ""),
                 focused ? ink::kAccent : ink::kFill, clip, focused ? caret : ws::kNoCaret);
        } else if (e.kind == Kind::button) {
            rect({x, y, w, std::min(h, notice_y - y), ink::kMuted});
            text(x + inset, middle, e.label, ink::kAccent, clip);
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
    by.clear();
    for (std::size_t i = 0; i < notice_lines.size(); ++i) {
        const auto y = notice_y + line * static_cast<std::int64_t>(i);
        text(0, y, notice_lines[i], p.alert ? ink::kAlert : ink::kMuted, {0, y, room.width, line});
    }
    // Rectangles a room cannot hold are dropped.
    for (std::size_t i = out.content.rects.size(); i-- > 0;) {
        const auto& r = out.content.rects[i];
        if (r.w > 0 && r.h > 0) continue;
        out.content.rects.erase(out.content.rects.begin() + static_cast<std::ptrdiff_t>(i));
        out.rects_by.erase(out.rects_by.begin() + static_cast<std::ptrdiff_t>(i));
    }
    return out;
}

/// THE PICTURE IN A GRAPHICS WINDOW, set as `room` sets text: each rectangle and run of text a node
/// in the order drawn, named by what drew it -- `ground`, `box:<id>`, `text:<id>`, the notice's
/// lines `notice` -- and each button's nodes recorded with its id, the action a press asks; in
/// front, a focused number field's caret, `caret`, an accent bar at the left of the character it
/// stands before. A run stands inset where the room is graphical. No name holds a `/`.
inline graphics::Sketch sketch(const Description& d, const Picture& p,
                               const ws::CanvasRoom& room) {
    const auto m = ws::canvas_text_metrics(room);
    graphics::Sketch out;
    std::map<std::string, std::size_t> named;
    const auto node = [&](const char* kind, const std::string& by, const char* none) {
        std::string base = by.empty() ? std::string(none) : std::string(kind) + ":" + by;
        const auto n = named[base]++;
        if (n > 0) base += ":" + std::to_string(n);
        const auto* e = by.empty() ? nullptr : d.element(by);
        if (e && e->kind == Kind::button) out.action(base, e->id);
        return base;
    };
    for (std::size_t i = 0; i < p.content.rects.size(); ++i) {
        const auto& r = p.content.rects[i];
        out.fill_rect(node("box", p.rects_by[i], "ground"), r.x, r.y, r.w, r.h, graphics::by_role(r.role));
    }
    for (std::size_t i = 0; i < p.content.texts.size(); ++i) {
        const auto& t = p.content.texts[i];
        out.text(node("text", p.texts_by[i], "notice"), t.x + m.inset, t.y + m.inset, t.text,
                 graphics::by_role(t.role));
    }
    for (const auto& t : p.content.texts) {
        if (t.caret_col < 0) continue;
        const std::int64_t cx = t.x + m.inset + t.caret_col * m.advance;
        const std::int64_t cy = t.y + m.inset;
        out.line("caret", cx, cy, cx, cy + m.line - 1, graphics::by_role(ink::kAccent));
        break;
    }
    return out;
}

/// A graphics window's room as a view draws in it: `width` by `height` pixels, in the window's unit
/// and its text (`ws::canvas_window_room`).
inline ws::CanvasRoom window_room(std::int64_t width, std::int64_t height, std::int64_t cell_px,
                                  const graphics::TextMeasure& text) {
    return ws::canvas_window_room(width, height, cell_px, text);
}

/// A view drawn for a graphics window's room, from what it was told and its presentation.
inline graphics::Sketch sketch(const Description& d, const Told& told, const Presentation& p,
                               const ws::CanvasRoom& room) {
    return sketch(d, picture(d, told, p, room), room);
}

/// THE ACTIONS A VIEW DECLARES: one per intent, its id the button's element id, its label the
/// button's (cut to what a row holds, its id where it has none), no default key, and what a press
/// causes the intent's shape -- read from the description, so the declaration and the view cannot
/// disagree. A weaver keys one by naming its id in their keymap file.
inline ws::v3::PaneActions actions_of(const Description& d) {
    ws::v3::PaneActions out;
    out.pane = kPane;
    for (const Intent& intent : d.intents) {
        const Element* button = d.element(intent.control);
        if (!button) continue;
        std::string label = button->label.empty() ? button->id : button->label;
        if (label.size() > ws::kMaxPaneActionLabelLen) label.resize(ws::kMaxPaneActionLabelLen);
        if (label.find_first_not_of(' ') == std::string::npos) label = button->id;
        out.rows.push_back(ws::v3::PaneActionRow{
            button->id, std::move(label), input::scan::kUnknown, input::mod::kNone, {},
            {ws::ActionCause{intent.shape->name(), static_cast<std::int64_t>(intent.shape->version()),
                             std::string()}},
            {}});
    }
    return out;
}

/// The picture a stopped view leaves: no element, and the sentence that it stopped.
inline ws::CanvasPicture stopped_picture(const Description& d, const ws::CanvasRoom& room,
                                         const std::string& why) {
    Presentation p;
    p.notice = d.name + " " + why;
    Description bare;
    bare.name = d.name;
    bare.width = d.width;
    bare.height = d.height;
    return picture(bare, {}, p, room).content;
}

/// The body a view asks its pane for: its size in pixels, never more than Workshop admits
/// (`workshop::kMaxPaneBodyPx`), with its notice rows beneath in the medium's own text; a view
/// larger than its pane is drawn cut to the pane.
inline ws::v3::PaneOffered offered(const Description& d) {
    const auto px = [](std::int64_t v) { return std::clamp<std::int64_t>(v, 1, ws::kMaxPaneBodyPx); };
    return ws::v3::PaneOffered{kPane, d.name, "a view made in the View Builder", px(d.width),
                               px(d.height), static_cast<std::int64_t>(kNoticeRows)};
}

/// The grant a description implies: each intent it says to any accepter, the pane conversation to
/// Workshop's office alone, and its drawing to a graphics window, which takes an area's drawing
/// only from its owner, with the ask for its room to the graphics directory. Nothing else.
inline loom::Grant view_grant(const Description& d) {
    loom::Grant grant;
    for (const auto& shape : d.says()) grant.allow_to_any(shape->name(), shape->version());
    grant.allow_to_any(graphics::Drawn::zen_name, graphics::Drawn::zen_version);
    grant.allow_to_role(graphics::RoomAsked::zen_name, graphics::RoomAsked::zen_version,
                        graphics::kGraphicsRole);
    const auto to_workshop = [&grant](const char* name, std::uint32_t version) {
        grant.allow_to_role(name, version, kWorkshopRole);
    };
    to_workshop(ws::v3::PaneOffered::zen_name, ws::v3::PaneOffered::zen_version);
    to_workshop(ws::v3::PaneActions::zen_name, ws::v3::PaneActions::zen_version);
    to_workshop(ws::PaneEscapeUnspent::zen_name, ws::PaneEscapeUnspent::zen_version);
    to_workshop(ws::PanePassRequested::zen_name, ws::PanePassRequested::zen_version);
    to_workshop(ws::PaneRevealRequested::zen_name, ws::PaneRevealRequested::zen_version);
    return grant;
}

class View;

/// AN OWNER'S SIDE OF A VIEW, for code in the process that runs it: values its labels show, told
/// without the bus, and code run when one of its buttons is used, before any intent it says. A
/// paired view (`View`, `Host::pair`) holds it, and it tells the view of that name standing now;
/// alone, it draws its own description (`sketch`) for an owner keeping its own `graphics::Canvas`.
class Presenter {
public:
    using Act = std::function<void(const Element& button, loom::Bus& bus)>;

    Presenter() = default;
    explicit Presenter(Description own) : own_(std::move(own)) {}
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;

    /// A value of a shape a label shows, as a delivery of it tells the view: "" when told, else why not.
    template <class T>
    std::string tell(const T& value) { return tell(loom::to_value(value)); }
    std::string tell(loom::Value value);
    /// `field` of each shape a label shows it from, read from `text` as that field's kind: the
    /// latest value of the shape, with that field changed, told. "" when told, else why not.
    std::string set(std::string_view field, std::string_view text);

    /// What a press on the button `id` runs, replacing what it ran; at most `kMaxElements` buttons.
    void on_action(std::string id, Act act);
    void on_action(std::string id, std::function<void()> act) {
        on_action(std::move(id), [act = std::move(act)](const Element&, loom::Bus&) { act(); });
    }
    bool answers(std::string_view id) const { return acts_.find(id) != acts_.end(); }

    /// The latest value of each shape it told.
    const Told& told() const noexcept { return told_; }
    /// The description its values are read against: the paired view's, else its own, else none.
    const Description* description() const;

    /// ITS OWN DRAWING for a graphics window's room (`window_room`), and a press there on its
    /// button `id`: its code run, true when it has code for one. An owner that holds a
    /// `graphics::Canvas` draws the one, in the room its canvas says, and answers
    /// `graphics::ActionRequested` with the other.
    graphics::Sketch sketch(const ws::CanvasRoom& room) const;
    bool act(std::string_view id, loom::Bus& bus);

    /// Asks the paired view to draw what it was told, as its host asks: from host code, not from
    /// another participant's delivery, whose word the view does not take.
    void refresh(loom::Bus& bus) const;

private:
    friend class View;
    void attach(View* v) { views_.push_back(v); }
    void detach(View* v) { std::erase(views_, v); }
    View* paired() const { return views_.empty() ? nullptr : views_.back(); }
    /// Its code for `button` run: nothing when it has none, "" when it ran, else what went wrong.
    std::optional<std::string> run(const Element& button, loom::Bus& bus);

    std::optional<Description> own_;
    Told told_;
    std::map<std::string, Act, std::less<>> acts_;
    std::vector<View*> views_; ///< the views holding it, in the order paired; the last tells
};

/// ONE VIEW, registered by the view host. Its accept-set is the shapes it is told, the pane
/// conversation and a window's; its emit-set is its intents and those conversations. It speaks to
/// Workshop and to the window that shows its area as its office, the view's name, which is that
/// area's name too, and says its intents by publication: whoever accepts one hears it.
class View final : public loom::Weave {
public:
    explicit View(Description description, std::shared_ptr<Presenter> presenter = {})
        : description_(std::move(description)), window_(description_.name, description_.name) {
        pair(std::move(presenter));
    }
    ~View() override {
        if (presenter_) presenter_->detach(this);
    }
    View(const View&) = delete;
    View& operator=(const View&) = delete;

    void set_self(loom::WeaveId id) noexcept { self_ = id; }
    loom::WeaveId self() const noexcept { return self_; }
    const std::shared_ptr<Presenter>& presenter() const noexcept { return presenter_; }

    /// HOST AUTHORITY: the presenter this view tells and runs, replacing any; what it told of a
    /// shape this view's labels show is taken as told now.
    void pair(std::shared_ptr<Presenter> presenter) {
        if (presenter_) presenter_->detach(this);
        presenter_ = std::move(presenter);
        if (!presenter_) return;
        presenter_->attach(this);
        for (const auto& value : presenter_->told()) (void)take(value);
    }

    /// A value of a shape this view's labels show, taken as its latest: false for another shape.
    bool take(const loom::Value& value) {
        for (const auto& told : description_.told())
            if (loom::same_identity(value.schema(), *told)) {
                std::erase_if(told_, [&](const auto& v) { return loom::same_identity(v.schema(), *told); });
                told_.push_back(value);
                return true;
            }
        return false;
    }
    const Description& description() const noexcept { return description_; }
    const Told& told() const noexcept { return told_; }
    const Presentation& presentation() const noexcept { return presentation_; }

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
        window_picture_.reset();
    }

    /// HOST AUTHORITY: the picture this view leaves when it stops, for the window that shows its
    /// area, drawn whole with no action, and that window's office; none when no window shows it.
    std::optional<std::pair<std::string, graphics::Drawn>> window_farewell(const std::string& why) const {
        if (!window_.shown() || window_.width() <= 0 || window_.height() <= 0) return std::nullopt;
        const auto room =
            window_room(window_.width(), window_.height(), window_.cell_px(), window_.text());
        Picture last;
        last.content = stopped_picture(description_, room, why);
        last.rects_by.resize(last.content.rects.size());
        last.texts_by.resize(last.content.texts.size());
        const auto drawn = sketch(description_, last, room);
        return std::pair{window_.window_office(),
                         graphics::Drawn{window_.area(), true, drawn.nodes(), {}, drawn.records(), {}, false}};
    }

    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        auto out = description_.told();
        for (auto s : {loom::schema_of<loom::Refused>(), loom::schema_of<detail::ViewStart>(),
                       loom::schema_of<detail::ViewRedraw>(), loom::schema_of<graphics::AreaRoom>(),
                       loom::schema_of<graphics::DrawnRefused>(), loom::schema_of<graphics::ActionRequested>(),
                       loom::schema_of<ws::PaneCatalogRequested>(), loom::schema_of<ws::PaneKey>(),
                       loom::schema_of<ws::PaneTextInput>(), loom::schema_of<ws::PaneRevealAnswered>(),
                       loom::schema_of<ws::PaneActionRequested>(), loom::schema_of<ws::ActionsJudged>(),
                       loom::schema_of<input::KeyPressed>(), loom::schema_of<input::TextEntered>(),
                       loom::schema_of<graphics::AreaButtonChanged>(), loom::schema_of<graphics::AreaPointerMoved>(),
                       loom::schema_of<graphics::AreaWheelTurned>()})
            out.push_back(std::move(s));
        return out;
    }

    std::vector<std::shared_ptr<const loom::Schema>> emitted_schemas() const override {
        auto out = description_.says();
        for (auto s : {loom::schema_of<ws::v3::PaneOffered>(), loom::schema_of<ws::v3::PaneActions>(),
                       loom::schema_of<ws::PaneEscapeUnspent>(),
                       loom::schema_of<ws::PanePassRequested>(), loom::schema_of<ws::PaneRevealRequested>(),
                       loom::schema_of<graphics::Drawn>(), loom::schema_of<graphics::RoomAsked>()})
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
            if (loom::from_value<detail::ViewStart>(in.payload).reveal) say(bus, ws::PaneRevealRequested{kPane});
            // ...and asks its area's room once a life: a view started again draws in its window again.
            if (!room_asked_ && !window_.shown()) {
                room_asked_ = true;
                (void)bus.office_send_to_role(description_.name, graphics::kGraphicsRole,
                                              loom::Message(loom::to_value(graphics::RoomAsked{window_.area()}), self_));
            }
            return;
        }
        if (is(loom::schema_of<graphics::AreaRoom>())) {
            // THE ROOM, from the window that shows its area, as that window's office: a window, an
            // incarnation or a size it has not drawn in holds none of its drawing, so it goes whole.
            loom::Mail mail(bus, in, self_);
            if (window_.room(loom::from_value<graphics::AreaRoom>(in.payload), mail)) show(bus);
            return;
        }
        if (is(loom::schema_of<graphics::DrawnRefused>())) {
            // A REFUSED CHANGE is drawn again whole at once, against nothing the window lacks; a
            // refused whole drawing is said on the notice row, and the window is sent nothing more
            // until the drawing changes.
            const auto refused = loom::from_value<graphics::DrawnRefused>(in.payload);
            loom::Mail mail(bus, in, self_);
            if (!window_.refused(refused, mail) || !window_.refused_whole()) return;
            presentation_.notice = "the window refused this view: " + refused.reason;
            presentation_.alert = true;
            show(bus);
            return;
        }
        if (is(loom::schema_of<graphics::ActionRequested>())) {
            // A PRESS ON A BUTTON the picture last drawn in the window names, asked by that
            // window's office for an area placed deaf to the pointer, is used as a press on it is.
            const auto asked = loom::from_value<graphics::ActionRequested>(in.payload);
            const loom::Mail mail(bus, in, self_);
            if (!window_.from_window(asked.area, mail) || window_actions_.count(asked.action) == 0)
                return;
            const Element* button = description_.element(asked.action);
            if (!button || button->kind != Kind::button) return;
            use(*button, bus);
            show(bus);
            return;
        }
        if (is(loom::schema_of<graphics::AreaButtonChanged>())) {
            // A PRESS IN THE WINDOW THAT SHOWS ITS AREA, as that window's office, read against the
            // picture last drawn there; one the window drew on another element than that picture
            // shows there uses nothing, and is said.
            const auto b = loom::from_value<graphics::AreaButtonChanged>(in.payload);
            const loom::Mail mail(bus, in, self_);
            if (!b.pressed || !window_.from_window(b.area, mail) || !window_picture_) return;
            if ((b.button == 1 || b.button == 3) && window_.moved(b.under, b.part, b.drawing, b.x, b.y)) {
                presentation_.notice = "the view changed under the press -- press again";
                show(bus);
                return;
            }
            if (b.button == 3) {
                say(bus, ws::PanePassRequested{kPane}, in.correlation); // Workshop's own pane menu
            } else if (b.button == 1) {
                left_press(*window_picture_, b.x, b.y, bus);
            }
            return;
        }
        if (is(loom::schema_of<graphics::AreaPointerMoved>()) || is(loom::schema_of<graphics::AreaWheelTurned>()))
            return; // a move and the wheel mean nothing to a view
        if (is(loom::schema_of<detail::ViewRedraw>())) {
            if (!host) return;
            declare(bus); // a change in place may name other buttons
            show(bus);
            return;
        }
        if (is(loom::schema_of<input::KeyPressed>()) || is(loom::schema_of<input::TextEntered>())) {
            // A KEY OR ITS TEXT FROM THE WINDOW THAT SHOWS ITS AREA, as that window's office, which
            // gives its keys only to an area placed to hear them: taken as Workshop's are.
            if (!window_.shown() || in.provenance.authored_role() != window_.window_office()) return;
            if (is(loom::schema_of<input::KeyPressed>())) {
                const auto k = loom::from_value<input::KeyPressed>(in.payload);
                key(ws::PaneKey{kPane, k.scancode, k.modifiers}, in, bus, /*workshop_sent=*/false);
            } else {
                type(loom::from_value<input::TextEntered>(in.payload).text, bus);
            }
            return;
        }
        if (is(loom::schema_of<loom::Refused>())) {
            // Shown only when Loom attests it answers this view's own delivery (ANS-01); the
            // correlation then says which of its intents it answers.
            if (!in.provenance.answers_ask() ||
                std::find(said_.begin(), said_.end(), in.correlation) == said_.end())
                return;
            const auto refused = loom::from_value<loom::Refused>(in.payload);
            presentation_.notice = "refused: " + refused.reason;
            presentation_.alert = true;
            show(bus);
            return;
        }
        if (take(in.payload)) {
            if (presentation_.alert && std::find(said_.begin(), said_.end(), in.correlation) != said_.end())
                presentation_.alert = false;
            if (!presentation_.alert) presentation_.notice.clear();
            show(bus);
            return;
        }
        if (!workshop) return;
        if (is(loom::schema_of<ws::PaneCatalogRequested>())) {
            offer(bus);
        } else if (is(loom::schema_of<ws::PaneRevealAnswered>())) {
            const auto answer = loom::from_value<ws::PaneRevealAnswered>(in.payload);
            if (answer.pane == kPane && !answer.seated) {
                presentation_.notice = "no seat on the desk: " + answer.refusal;
                presentation_.alert = true;
                show(bus);
            }
        } else if (is(loom::schema_of<ws::PaneKey>())) {
            key(loom::from_value<ws::PaneKey>(in.payload), in, bus);
        } else if (is(loom::schema_of<ws::PaneActionRequested>())) {
            // A BUTTON ASKED FOR BY ITS ID -- a weaver's key bound to it, or an act by address --
            // used as a press on it is.
            const auto asked = loom::from_value<ws::PaneActionRequested>(in.payload);
            const Element* button = asked.pane == kPane && description_.intent(asked.id)
                                        ? description_.element(asked.id) : nullptr;
            if (!button) return;
            use(*button, bus);
            show(bus);
        } else if (is(loom::schema_of<ws::ActionsJudged>())) {
            // A REFUSED DECLARATION IS SAID: the buttons still answer a press, and no key reaches one.
            const auto judged = loom::from_value<ws::ActionsJudged>(in.payload);
            if (judged.pane != kPane || judged.accepted) return;
            presentation_.notice = "no key can name these buttons: " + judged.refusal;
            presentation_.alert = true;
            show(bus);
        } else if (is(loom::schema_of<ws::PaneTextInput>())) {
            const auto typed = loom::from_value<ws::PaneTextInput>(in.payload);
            if (typed.pane != kPane) return;
            type(typed.text, bus);
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
        say(bus, offered(description_));
        declare(bus);
    }

    /// WHAT ITS BUTTONS DO, said to Workshop as the pane's actions (`actions_of`).
    void declare(loom::Bus& bus) { say(bus, actions_of(description_)); }

    component::TextBox& field(const std::string& id) {
        const auto found = presentation_.fields.find(id);
        if (found != presentation_.fields.end()) return found->second;
        const auto* e = description_.element(id);
        return presentation_.fields.emplace(id, field_box(presentation_, *e)).first->second;
    }

    /// SHOW WHAT IT HOLDS NOW in the window that shows its area: what changed there (nothing when
    /// nothing did), or the picture whole (`ws::WindowDrawing`). The picture is kept, so a press
    /// there is read against it.
    void show(loom::Bus& bus) {
        if (!window_.shown() || window_.width() <= 0 || window_.height() <= 0) return;
        const auto room = window_room(window_.width(), window_.height(), window_.cell_px(), window_.text());
        window_picture_ = picture(description_, told_, presentation_, room);
        const auto drawn = sketch(description_, *window_picture_, room);
        window_actions_.clear();
        for (const auto& record : drawn.records()) window_actions_.insert(record.action);
        // The mail only carries this view's statements as its office; it answers nothing.
        static const loom::Message quiet(loom::to_value(detail::ViewRedraw{}));
        loom::Mail mail(bus, quiet, self_);
        window_.draw(drawn, mail);
    }

    /// A LEFT PRESS AT `x`, `y` of `pictured`, in the window that shows its area: a number field
    /// focused, a button used, or the focus taken away.
    void left_press(const Picture& pictured, std::int64_t x, std::int64_t y, loom::Bus& bus) {
        const auto* hit = pictured.hit(x, y);
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

    /// A key, from Workshop or from the window that shows its area. An Escape it has no use for is
    /// handed back to Workshop when Workshop sent it, and goes nowhere when the window did.
    void key(const ws::PaneKey& k, const loom::Message& in, loom::Bus& bus, bool workshop_sent = true) {
        if (k.pane != kPane) return;
        if (k.scancode == input::scan::kTab) {
            focus_next();
        } else if (k.scancode == input::scan::kEscape) {
            if (!presentation_.focus) {
                if (workshop_sent) say(bus, ws::PaneEscapeUnspent{kPane}, in.correlation);
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

    /// Text typed into the focused field, from Workshop or from the window that shows its area.
    void type(const std::string& text, loom::Bus& bus) {
        if (!presentation_.focus) return;
        auto& box = field(*presentation_.focus);
        if (!ws::pane_text::admissible(text)) {
            presentation_.notice = "not typed -- a number field holds plain ASCII";
            presentation_.alert = true;
        } else if (box.size() + text.size() > kMaxFieldTextBytes) {
            presentation_.notice = "a number field holds at most " +
                                   std::to_string(kMaxFieldTextBytes) + " bytes";
            presentation_.alert = true;
        } else {
            box.type(text);
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

    /// A CONTROL USED: its presenter's code first, then its intent, every field read from the
    /// number field it names, published as this participant. A field that holds no integer, or
    /// the presenter's code failing, is said on the notice row, and nothing is published.
    void use(const Element& control, loom::Bus& bus) {
        const auto* intent = description_.intent(control.id);
        const auto presenter = presenter_; // held while its code runs, which may unpair it
        if (const auto ran = presenter ? presenter->run(control, bus) : std::nullopt) {
            presentation_.notice = ran->empty() ? std::string() : "`" + control.id + "` failed: " + *ran;
            presentation_.alert = !ran->empty();
            if (!ran->empty() || !intent) return;
        }
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
    std::deque<std::uint64_t> said_;
    std::uint64_t correlation_ = 0;
    component::Clipboard clipboard_;
    std::shared_ptr<Presenter> presenter_;
    ws::WindowDrawing window_;              ///< its area in the window that shows it
    std::set<std::string> window_actions_;  ///< the buttons the picture last sent there names
    std::optional<Picture> window_picture_; ///< that picture, which a press there is read against
    bool room_asked_ = false;
};

// ---- the presenter, which reaches the view it is paired with -----------------------------

namespace detail {
/// `text` as a cell of a label's `kind`, as a weaver writes one; none when it is not one.
inline std::optional<loom::Cell> typed(loom::Kind kind, std::string_view text) {
    const std::string s(text);
    if (kind == loom::Kind::Text) return loom::Cell::text(s);
    if (kind == loom::Kind::Bool && (s == "true" || s == "false")) return loom::Cell::boolean(s == "true");
    if (kind == loom::Kind::Int) {
        std::int64_t n = 0;
        const auto read = std::from_chars(s.data(), s.data() + s.size(), n);
        if (!s.empty() && read.ec == std::errc{} && read.ptr == s.data() + s.size()) return loom::Cell::integer(n);
    }
    if (kind == loom::Kind::Float && !s.empty() && !std::isspace(static_cast<unsigned char>(s.front()))) {
        try {
            std::size_t used = 0;
            const double x = std::stod(s, &used);
            if (used == s.size() && std::isfinite(x)) return loom::Cell::real(x);
        } catch (const std::exception&) {
        }
    }
    return std::nullopt;
}
} // namespace detail

inline const Description* Presenter::description() const {
    if (const View* v = paired()) return &v->description();
    return own_ ? &*own_ : nullptr;
}

inline std::string Presenter::tell(loom::Value value) {
    const Description* d = description();
    if (!d) return "no description to tell";
    const auto shown = d->told();
    if (std::none_of(shown.begin(), shown.end(),
                     [&](const auto& s) { return loom::same_identity(*s, value.schema()); }))
        return "no label of `" + d->name + "` shows " + detail::label_of(value.schema());
    std::erase_if(told_, [&](const auto& v) { return loom::same_identity(v.schema(), value.schema()); });
    told_.push_back(value);
    if (View* v = paired()) (void)v->take(value);
    return {};
}

inline std::string Presenter::set(std::string_view field, std::string_view text) {
    const Description* d = description();
    if (!d) return "no description to tell";
    std::vector<loom::Value> next;
    for (const auto& s : d->shows) {
        if (s.field != field || std::any_of(next.begin(), next.end(), [&](const auto& v) {
                return loom::same_identity(v.schema(), *s.shape);
            }))
            continue;
        const View* v = paired();
        const auto* latest = told_at(v ? v->told() : told_, *s.shape);
        loom::Value value = latest ? *latest : loom::Value(s.shape);
        const auto kind = s.shape->find(s.field)->type.kind;
        auto cell = detail::typed(kind, text);
        if (!cell)
            return "`" + std::string(text) + "` does not read as `" + std::string(field) + "` of " +
                   detail::label_of(*s.shape) + ", a field of kind " + loom::name_of(kind);
        value.set(s.field, std::move(*cell));
        next.push_back(std::move(value));
    }
    if (next.empty()) return "no label of `" + d->name + "` shows a field `" + std::string(field) + "`";
    for (auto& value : next) (void)tell(std::move(value));
    return {};
}

inline void Presenter::on_action(std::string id, Act act) {
    if (!answers(id) && acts_.size() >= kMaxElements)
        throw std::length_error("a presenter runs code for at most " + std::to_string(kMaxElements) + " buttons");
    acts_[std::move(id)] = std::move(act);
}

inline std::optional<std::string> Presenter::run(const Element& button, loom::Bus& bus) {
    const auto found = acts_.find(button.id);
    if (found == acts_.end()) return std::nullopt;
    const Act act = found->second; // its code may replace itself as it runs
    try {
        act(button, bus);
    } catch (const std::exception& error) {
        return std::string(error.what());
    }
    return std::string();
}

inline graphics::Sketch Presenter::sketch(const ws::CanvasRoom& room) const {
    const Description* d = description();
    return d ? view::sketch(*d, told_, {}, room) : graphics::Sketch{};
}

inline bool Presenter::act(std::string_view id, loom::Bus& bus) {
    const Description* d = description();
    const Element* button = d ? d->element(id) : nullptr;
    const auto found = acts_.find(id);
    if (!button || button->kind != Kind::button || found == acts_.end()) return false;
    const Act code = found->second;
    code(*button, bus);
    return true;
}

inline void Presenter::refresh(loom::Bus& bus) const {
    if (const View* v = paired())
        (void)bus.send(v->self(), loom::Message(loom::to_value(detail::ViewRedraw{})));
}

} // namespace zengine::view
#endif
