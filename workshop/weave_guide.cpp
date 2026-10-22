// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A GUEST'S GUIDE MARK: one a session and one guest's a pane, on a control or part the pane shows in
// a settled picture, drawn inside its body in the guide role -- an outline in a window, the part's
// own cells in a terminal -- with a label naming the guest where it covers no word. It ends at the
// weaver's press inside it, when its part or its pane's holder leaves, at a newer mark and with its
// session, and never comes back. Law: agents/workshop/guide.md.

#include "weave.hpp"

#include "screen_canvas.hpp" // where a canvas pane's picture stands

#include <zen/switchboard/door.hpp>

#include <algorithm>

namespace zengine::workshop {

namespace {

bool printable_ascii(const std::string& s) {
    for (const char c : s) {
        const auto b = static_cast<unsigned char>(c);
        if (b < 0x20 || b > 0x7e) return false;
    }
    return true;
}

/// Runs of spaces collapsed to one: a label cannot pad itself into another line's columns.
std::string collapsed(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (c == ' ' && !out.empty() && out.back() == ' ') continue;
        out.push_back(c);
    }
    return out;
}

PixelRect clipped(const PixelRect& r, const PixelRect& to) {
    const std::int64_t x0 = std::max(r.x, to.x), y0 = std::max(r.y, to.y);
    const std::int64_t x1 = std::min(r.x + r.w, to.x + to.w), y1 = std::min(r.y + r.h, to.y + to.h);
    return (x1 > x0 && y1 > y0) ? PixelRect{x0, y0, x1 - x0, y1 - y0} : PixelRect{};
}

bool meets(const PixelRect& a, const DeskRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool inside(const PixelRect& r, std::int64_t x, std::int64_t y) {
    return r.w > 0 && r.h > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/// The pane a `pane:` or `mark:` subject part names, `<office>/<pane>`; empty for any other part.
std::pair<std::string, std::string> subject_pane(const std::string& part) {
    for (const char* prefix : {"pane:", "mark:"}) {
        const std::string p = prefix;
        if (part.rfind(p, 0) != 0) continue;
        const std::string rest = part.substr(p.size());
        const std::size_t slash = rest.find('/');
        if (slash == std::string::npos) return {};
        return {rest.substr(0, slash), rest.substr(slash + 1)};
    }
    return {};
}

} // namespace

// THE MARK, ASKED: who may ask, the row's doors, the places no mark stands, then the part as the pane
// shows it now -- in that order, so a refusal by door says nothing of what the pane shows.
// WL-GUIDE-04 -- agents/workshop/guide.md; WL-GUIDE-05 -- agents/workshop/guide.md
void WorkshopWeave::on(const GuideRequested& asked, loom::Mail& mail) {
    guide_requested(asked, mail.sender(), mail, nullptr, false);
}

// ...ASKED FOR `sender`, answered now or, once the window said what its pane shows, through `due`:
// a mark on a pane that draws in an area of its own waits for the window's reading taken after the
// ask, unless `fresh` says it was.
void WorkshopWeave::guide_requested(const GuideRequested& asked, loom::WeaveId sender,
                                    loom::Mail& mail, loom::DeferredAnswer* due, bool fresh) {
    const auto say = [&](GuideAnswered answered) {
        if (due != nullptr) {
            (void)loom::answer_deferred(*due, mail, std::move(answered));
        } else {
            (void)mail.answer(std::move(answered));
        }
    };
    const scope::GuestRowFacts guest =
        host_->guest_row ? host_->guest_row(sender) : scope::GuestRowFacts{};
    if (!guest.admitted) {
        say(GuideAnswered{false, "only a guest a guest door admitted draws a guide mark"});
        return;
    }
    const std::uint64_t key = sender.value;
    if (asked.control.empty()) {
        const bool had = guide_marks_.erase(key) > 0;
        say(GuideAnswered{false, had ? "cleared" : "no mark of this session stood"});
        if (had) repaint(mail);
        return;
    }
    if (asked.words.size() > kGuideWordsBytes || !printable_ascii(asked.words)) {
        say(GuideAnswered{false, "refused: a mark's words are at most " +
                                                   std::to_string(kGuideWordsBytes) +
                                                   " bytes of printable ASCII"});
        return;
    }
    const std::size_t slash = asked.pane.find('/');
    const std::string office = asked.pane.substr(0, slash);
    const std::string pane = slash == std::string::npos ? std::string() : asked.pane.substr(slash + 1);
    std::string why;
    const std::optional<loom::Door> door =
        pane.empty() ? std::nullopt : loom::Door::parse(asked.pane, &why);
    if (!pane.empty() && (!door || pane == "*" || pane.find(':') != std::string::npos)) {
        say(GuideAnswered{false, "refused: '" + asked.pane +
                                                   "' is not a pane; a mark names <office>/<pane>"});
        return;
    }
    // THE ROW'S DOORS FIRST: its `guide` doors, asked of Loom now, and its read.
    if (!pane.empty()) {
        if (guest.guides_by_pane) {
            const loom::GrantAuthority cap =
                host_->input_authority ? host_->input_authority(sender) : loom::GrantAuthority{};
            const loom::AuthorityView live = mail.describe_authority(cap);
            if (!live.available || !live.permits_role(GuideRequested::zen_name, GuideRequested::zen_version,
                                                      kWorkshopProvider, *door)) {
                say(GuideAnswered{false, "refused: pane " + asked.pane + " is outside guest '" +
                                                           guest.name + "''s `guide` doors"});
                return;
            }
        }
        if (const std::string withheld = read_withheld(mail, sender, office, pane); !withheld.empty()) {
            say(GuideAnswered{false, "refused: " + withheld});
            return;
        }
    }
    // WHAT THE MARK WOULD NAME: a control by its id, drawn as `control:<id>`, or a subject part.
    const bool is_part = asked.control.rfind(kGuidePartPrefix, 0) == 0;
    const std::string id = is_part ? std::string() : asked.control;
    const std::string part = is_part ? asked.control.substr(std::string(kGuidePartPrefix).size())
                                     : "control:" + asked.control;
    // NEVER ON A PART WHOSE PRESS OPENS A PANE ONLY THE WEAVER'S HAND READS, by its subject or by
    // what its control may cause.
    const auto [subject_office, subject_pane_key] = subject_pane(part);
    if (const std::string only = scope::weaver_only_pane(subject_office, subject_pane_key);
        !subject_office.empty() && !only.empty()) {
        say(GuideAnswered{false, "refused: a press of " + part + " opens a pane only the "
                                                   "weaver's hand reaches: " + only});
        return;
    }
    const KeymapShown keys = keymap_shown(session_, host_->keymap_path, keymap_standing_);
    const ShownBinding* binding = nullptr;
    for (const ShownBinding& b : keys.rows) {
        if (!id.empty() && b.id == id && b.office == office && b.pane == pane) binding = &b;
    }
    if (binding != nullptr) {
        for (const ActionCause& cause : binding->causes) {
            const std::string to = "to ";
            if (cause.subject.rfind(to, 0) != 0) continue;
            const std::string target = cause.subject.substr(to.size());
            const std::size_t s = target.find('/');
            const std::string only = scope::weaver_only_pane(target.substr(0, s),
                                                             s == std::string::npos ? "" : target.substr(s + 1));
            if (!only.empty()) {
                say(GuideAnswered{false, "refused: a press of " + id + " opens a pane only "
                                                           "the weaver's hand reaches: " + only});
                return;
            }
        }
    }
    const auto key_of = [&]() {
        return binding == nullptr ? std::string("no key")
                                  : (binding->shown.empty() ? std::string("no key") : binding->shown);
    };
    // AN APPLICATION ROW, OR A CONTROL WITH NO PART, IS A KEY: answered with the weaver's own.
    if (pane.empty()) {
        say(GuideAnswered{false, binding == nullptr
            ? "not shown: " + office + " declares no row " + id + " in force"
            : "not shown: " + office + ":" + id + " is a key, with no part to mark; the weaver's key is " + key_of()});
        return;
    }
    for (const auto& [other, m] : guide_marks_) {
        if (other != key && m.office == office && m.pane == pane) {
            say(GuideAnswered{false, "refused: guest '" + m.who +
                                                       "' marks that pane already; one guest mark a pane"});
            return;
        }
    }
    if (guide_marks_.count(key) == 0 && guide_marks_.size() >= kGuideMarks) {
        say(GuideAnswered{false, "refused: the desk holds " + std::to_string(kGuideMarks) +
                                                   " guide marks already, one a session"});
        return;
    }
    // A PANE THAT DRAWS IN AN AREA OF ITS OWN IS MARKED ON WHAT THE WINDOW SAYS IT SHOWS NOW.
    if (!fresh) {
        if (const std::string area = drawn_area(PaneRef{office, pane}); !area.empty()) {
            DeskWait wait;
            wait.kind = DeskWait::Kind::kGuide;
            wait.asker = sender;
            wait.guide = asked;
            (void)wait_on_areas(mail, std::move(wait), {area});
            return;
        }
    }
    const PaneStamp now = stamp_of(office, pane);
    GuideMark mark{office, pane, part, collapsed(asked.words), guest.name, now.holder, now.incarnation, {}, false};
    const GuideDrawing drawing = guide_drawing(mark);
    if (!drawing.settled || !drawing.shown) {
        std::string said = "not shown: " + asked.pane + " shows no part " + part + " now";
        if (binding != nullptr) said += "; " + id + " is in force, and the weaver's key is " + key_of();
        if (!drawing.settled) said = "not shown: " + asked.pane + " is not on the desk, settled and uncovered, now";
        say(GuideAnswered{false, said});
        return;
    }
    mark.drawn = drawing.drawn;
    mark.labelled = drawing.labelled;
    guide_marks_[key] = mark;
    say(GuideAnswered{true, "marked " + part + " in " + asked.pane + " for the weaver, as guest '" +
                                              guest.name + "'; " +
                                              (drawing.labelled ? "its label stands beside it"
                                                                : "its label is not drawn: no free row of the "
                                                                  "pane's body beside it")});
    repaint(mail);
}

// WHAT ONE MARK DRAWS NOW, judged against the pane as it shows: nothing while the pane is not settled
// and uncovered; else the part's outline in a window or its own cells in a terminal, clipped to the
// body, and a label where it covers no word or part.
WorkshopWeave::GuideDrawing WorkshopWeave::guide_drawing(const GuideMark& mark) const {
    GuideDrawing out;
    // SETTLED AS A READING IS: what the pane shows now, by fingerprint, with nothing over it.
    PaneView view;
    if (!pane_reading(mark.office, mark.pane, view).empty() || view.in_flight || !view.covered.by.empty()) {
        return out;
    }
    const RuntimePane* row = session_.panes.runtime.find(mark.office, mark.pane);
    if (row == nullptr) return out;
    const Screen sc = screen_of(session_);
    const auto bounds = bounds_of(session_.panes, session_.setup.active, row->kind, sc);
    if (!bounds.open || view.area.w <= 0 || view.area.h <= 0) return out;
    out.settled = true;
    const std::int64_t g = chrome_grain(sc);
    const bool cells = g > 1;
    // A LABEL IS SET BY THE FACE'S MEASURE, as the window sets the overlay's text.
    const std::int64_t advance = sc.text_advance_px > 0 ? sc.text_advance_px : graphics::kCellPx;
    const std::int64_t line = sc.text_line_px > 0 ? sc.text_line_px : graphics::kCellPx;
    // THE BODY THE PANE SHOWS IN: where its reading stands.
    const PixelRect body{view.area.x, view.area.y, view.area.w, view.area.h};
    const std::vector<PaneWord>& words = view.words;
    const std::vector<PanePart>& parts = view.parts;
    const PanePart* found = nullptr;
    for (const PanePart& p : parts) {
        if (p.name == mark.part && p.place.w > 0 && p.place.h > 0) found = &p;
    }
    if (found == nullptr) return out;
    out.shown = true;
    const DeskRect& r = found->place;
    const std::int64_t role = graphics::role::kGuide;
    if (cells) {
        // THE PART'S OWN CELLS, in the guide role: no cell beside it is drawn over.
        const PixelRect own = clipped(PixelRect{r.x, r.y, r.w, r.h}, body);
        out.drawn = own;
        const std::int64_t fits = own.w / graphics::kCellPx;
        std::string text = found->text.substr(0, static_cast<std::size_t>(std::max<std::int64_t>(fits, 0)));
        if (static_cast<std::int64_t>(text.size()) < fits) text.append(static_cast<std::size_t>(fits) - text.size(), ' ');
        if (own.w > 0) out.labels.push_back(surface::SurfaceLabel{own.x, own.y, text, role});
    } else {
        // AN OUTLINE ONE GRAIN WIDE AROUND THE PART, inside the body.
        out.drawn = clipped(PixelRect{r.x - g, r.y - g, r.w + 2 * g, r.h + 2 * g}, body);
        for (const PixelRect& edge : {PixelRect{r.x - g, r.y - g, r.w + 2 * g, g},
                                      PixelRect{r.x - g, r.y + r.h, r.w + 2 * g, g},
                                      PixelRect{r.x - g, r.y, g, r.h}, PixelRect{r.x + r.w, r.y, g, r.h}}) {
            const PixelRect in = clipped(edge, body);
            if (in.w > 0) out.rects.push_back(wire_rect_of(in, role));
        }
    }
    // THE LABEL SAYS WHOSE MARK IT IS: on the row above the part, else below, where it covers no word
    // or part of the pane -- in a terminal a body row wholly free -- or not at all.
    std::string label = mark.who + ": \"" + mark.words + "\"";
    const std::int64_t x = cells ? body.x : std::max(r.x, body.x);
    const std::int64_t room = (body.x + body.w - x) / advance;
    if (room <= 0) return out;
    if (static_cast<std::int64_t>(label.size()) > room) label.resize(static_cast<std::size_t>(room));
    const std::int64_t width = cells ? body.w : static_cast<std::int64_t>(label.size()) * advance;
    for (const std::int64_t y : {r.y - (cells ? 0 : g) - line, r.y + r.h + (cells ? 0 : g)}) {
        const PixelRect spot{x, y, width, line};
        if (y < body.y || y + line > body.y + body.h) continue;
        bool free = true;
        for (const PaneWord& w : words) free = free && !meets(spot, w.place);
        for (const PanePart& p : parts) free = free && !meets(spot, p.place);
        if (!free) continue;
        out.labels.push_back(surface::SurfaceLabel{x, y, label, role});
        out.labelled = true;
        break;
    }
    return out;
}

bool WorkshopWeave::guide_marks_moved() const {
    for (const auto& [session, mark] : guide_marks_) {
        const GuideDrawing drawing = guide_drawing(mark);
        if (drawing.settled && (!drawing.shown || !(drawing.drawn == mark.drawn))) return true;
    }
    return false;
}

void WorkshopWeave::end_guide(loom::Mail& mail, std::uint64_t session, const std::string& why) {
    if (guide_marks_.erase(session) == 0) return;
    (void)mail.send(loom::WeaveId{session}, GuideAnswered{false, "ended: " + why});
}

// EVERY MARK, JUDGED AGAIN AT EVERY PICTURE: its session, its pane's holder and incarnation, and its
// part in a settled picture; a mark whose pane is not settled and uncovered now is kept, not drawn.
// WL-GUIDE-06 -- agents/workshop/guide.md
surface::SurfaceLayer WorkshopWeave::guide_layer(loom::Mail& mail) {
    surface::SurfaceLayer layer;
    std::vector<std::pair<std::uint64_t, std::string>> ended;
    for (auto& [session, mark] : guide_marks_) {
        // ITS SESSION IS GONE when the host mints nothing for it: nobody to tell.
        if (!host_->input_authority || !host_->input_authority(loom::WeaveId{session}).valid()) {
            ended.emplace_back(session, std::string());
            continue;
        }
        const PaneStamp now = stamp_of(mark.office, mark.pane);
        if (now.holder != mark.holder || now.incarnation != mark.incarnation) {
            ended.emplace_back(session, "the pane's holder changed");
            continue;
        }
        const RuntimePane* row = session_.panes.runtime.find(mark.office, mark.pane);
        const Screen sc = screen_of(session_);
        if (row == nullptr || !session_.panes.has(row->kind) ||
            !bounds_of(session_.panes, session_.setup.active, row->kind, sc).open) {
            ended.emplace_back(session, "its pane left the desk");
            continue;
        }
        const GuideDrawing drawing = guide_drawing(mark);
        mark.drawn = PixelRect{};
        if (!drawing.settled) continue;
        if (!drawing.shown) {
            ended.emplace_back(session, "its part " + mark.part + " left the pane's picture");
            continue;
        }
        mark.drawn = drawing.drawn;
        mark.labelled = drawing.labelled;
        for (const surface::SurfaceRect& r : drawing.rects) layer.rects.push_back(r);
        for (const surface::SurfaceLabel& l : drawing.labels) layer.labels.push_back(l);
    }
    for (const auto& [session, why] : ended) {
        if (why.empty()) guide_marks_.erase(session);
        else end_guide(mail, session, why);
    }
    return layer;
}

// THE WEAVER'S OWN PRESS INSIDE A MARK ENDS IT; the press goes on to its pane as it would.
// WL-GUIDE-06 -- agents/workshop/guide.md
void WorkshopWeave::guide_pressed(loom::Mail& mail, const PointedAt& at) {
    if (!at.understood) return;
    std::vector<std::uint64_t> pressed;
    for (const auto& [session, mark] : guide_marks_) {
        if (inside(mark.drawn, at.px.x, at.px.y)) pressed.push_back(session);
    }
    for (const std::uint64_t session : pressed) end_guide(mail, session, "the weaver pressed it");
}

} // namespace zengine::workshop
