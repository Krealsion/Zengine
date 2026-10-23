// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop desk suite, the desk said whole: one reading filled to the budget a reader's
// decoder spends, the panes past it named by their stamps and read alone a page at a time, which
// participants Workshop answers, and the desk's own menu on a window as a Python reader reads it.

#include "doctest.h"
#include "workshop_support.hpp"
#include "workshop/decoded_cells.hpp"
#include "workshop/guest_door.hpp"
#include "workshop/guests.hpp"
#include "workshop/pane_canvas_drawing.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_picture.hpp"
#include "workshop/reply_bytes.hpp"
#include "timer/vocabulary.hpp"

#include <zen/bridge/client.hpp>
#include <zen/serialize.hpp>
#include <zen/wire.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char* kReadPane = "page";

struct ReadAskerState {
    ZEN_SHAPE(ReadAskerState, 1);
};

/// WHO ASKS: an ordinary weave granted the desk read and a pane's page, keeping every answer and
/// refusal.
class ReadAsker
    : public loom::WeaveBase<ReadAsker, ReadAskerState,
                             loom::Accept<SeatDo, DeskRead, PaneView, loom::Refused>,
                             loom::Emit<DeskReadRequested, PaneViewRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<DeskRead> reads;
    std::vector<PaneView> pages;
    std::vector<std::string> refusals;
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    void on(const DeskRead& d, loom::Mail&) { reads.push_back(d); }
    void on(const PaneView& v, loom::Mail&) { pages.push_back(v); }
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); }
    /// Every answer and refusal this asker has had: what a case counts to say it asked nothing.
    std::size_t answered() const { return reads.size() + pages.size() + refusals.size(); }
};

struct NoticeEarState {
    ZEN_SHAPE(NoticeEarState, 1);
};

/// WHO FOLLOWS THE DESK: an ordinary weave declaring the notice that the desk moved, keeping every
/// one it is told, asking nothing.
class NoticeEar
    : public loom::WeaveBase<NoticeEar, NoticeEarState, loom::Accept<DeskStamps>> {
public:
    std::vector<DeskStamps> heard;
    void on(const DeskStamps& d, loom::Mail&) { heard.push_back(d); }
};

/// A PROVIDER THAT DRAWS A PICTURE IN ITS OWN AREA OF THE WINDOW, keeping each room the window gives
/// it and each drawing it refuses, and drawing its last picture again for each new room, as a pane
/// does.
class ReadCanvasSeat
    : public loom::WeaveBase<ReadCanvasSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, gfx::AreaRoom, gfx::DrawnRefused,
                                          gfx::AreaButtonChanged, gfx::AreaPointerMoved,
                                          gfx::AreaWheelTurned, gfx::AreaHoldEnded,
                                          PaneCanvasHover, PaneKey, SeatDo>,
                             loom::Emit<v3::PaneOffered, gfx::Drawn>> {
public:
    explicit ReadCanvasSeat(const std::string& office) : window_(office, office) {}
    std::vector<gfx::AreaRoom> rooms;
    std::vector<gfx::DrawnRefused> rejected;
    int drawn = 0;
    /// Why the last picture could not be drawn at all, or "".
    std::string over;
    std::function<void(loom::Mail&)> next;
    /// What the pane does with a key it is handed: a case's own answer, or nothing.
    std::function<void(loom::Mail&)> on_key;
    void on(const PaneKey&, loom::Mail& m) {
        if (on_key) on_key(m);
    }
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const gfx::AreaRoom& r, loom::Mail& m) {
        if (!window_.room(r, m)) return;
        rooms.push_back(r);
        if (last_) draw(m, *last_);
    }
    void on(const gfx::DrawnRefused& r, loom::Mail& m) {
        if (window_.refused(r, m)) rejected.push_back(r);
    }
    void on(const gfx::AreaButtonChanged& b, loom::Mail& m) { (void)window_.pointer(b, m, 0, 0); }
    void on(const gfx::AreaPointerMoved& e, loom::Mail& m) { (void)window_.pointer(e, m, 0, 0); }
    void on(const gfx::AreaWheelTurned& e, loom::Mail& m) { (void)window_.pointer(e, m, 0, 0); }
    void on(const gfx::AreaHoldEnded& e, loom::Mail& m) { (void)window_.pointer(e, m); }
    void on(const PaneCanvasHover&, loom::Mail&) {}
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    /// THE ROOM THE WINDOW LAST GAVE THE AREA, as a picture is composed for it.
    CanvasRoom room() const {
        const gfx::AreaRoom& r = rooms.back();
        return canvas_window_room(r.w, r.h, r.cell_px, r.measure());
    }
    /// `picture`, drawn in the area for the room the window last gave it.
    void draw(loom::Mail& m, const CanvasPicture& picture) {
        last_ = picture;
        const CanvasSketch said = canvas_sketch(picture, room());
        over = said.over;
        if (!over.empty()) return;
        ++drawn;
        window_.draw(said.sketch, m);
    }

private:
    WindowDrawing window_;
    std::optional<CanvasPicture> last_;
};

/// ONE CANVAS PANE ON THE DESK: its seat, its office and kind.
struct ReadCanvas {
    ReadCanvasSeat* seat = nullptr;
    loom::WeaveId id{};
    std::string office;
    std::int64_t kind = kNoPaneKind;
};

/// A FRESH DESK WITH NO PANE OF A CASE'S OWN YET, the asker beside it, and the canvas panes a case
/// opens on it, each in an office of its own.
struct ReadRig {
    PaneRig r;
    ReadAsker* asker = nullptr;
    loom::WeaveId asker_id{};
    NoticeEar* ear = nullptr;
    std::vector<ReadCanvas> canvases;
    /// The desk's extent, in cells.
    std::int64_t columns = 150, rows = 60;

    ReadRig() {
        r.mount_workshop();
        r.ready();
        r.extent(150, 60);
        auto made = std::make_unique<ReadAsker>();
        asker = made.get();
        loom::Grant grant;
        for (const auto& shape :
             {loom::schema_of<DeskReadRequested>(), loom::schema_of<PaneViewRequested>()}) {
            grant.allow_to_role(shape->name(), shape->version(), kWorkshopProvider);
        }
        asker_id = r.bus.register_weave(std::move(made), std::move(grant));
        asker->zen_set_self(asker_id);
        auto listening = std::make_unique<NoticeEar>();
        ear = listening.get();
        const loom::WeaveId ear_id = r.bus.register_weave(std::move(listening), loom::Grant{});
        ear->zen_set_self(ear_id);
    }

    void drive(std::size_t at, std::function<void(loom::Mail&)> what) {
        ReadCanvas& c = canvases.at(at);
        c.seat->next = std::move(what);
        (void)r.bus.send(c.id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }

    /// The desk `width` by `height` cells.
    void size(std::int64_t width, std::int64_t height) {
        columns = width;
        rows = height;
        r.extent(columns, rows);
    }

    /// A CANVAS PANE OFFERING A BODY `across` BY `down` CELLS, opened on the desk.
    std::size_t open_canvas(const std::string& office, std::int64_t across, std::int64_t down) {
        auto made = std::make_unique<ReadCanvasSeat>(office);
        ReadCanvas c;
        c.seat = made.get();
        c.office = office;
        loom::Grant grant;
        grant.allow_to_any(v3::PaneOffered::zen_name, v3::PaneOffered::zen_version);
        grant.allow_to_any(gfx::Drawn::zen_name, gfx::Drawn::zen_version);
        c.id = r.bus.register_weave(std::move(made), std::move(grant), office);
        c.seat->zen_set_self(c.id);
        canvases.push_back(c);
        const std::size_t at = canvases.size() - 1;
        drive(at, [office, across, down](loom::Mail& m) {
            (void)m.as_role(office).send_to_role(
                kWorkshopProvider, v3::PaneOffered{kReadPane, "Page", "a picture read whole",
                                                   across * kPaneCanvasUnit,
                                                   down * kPaneCanvasUnit, 0});
        });
        r.pick(PaneRef{office, kReadPane});
        const auto* row = r.session().panes.runtime.find(office, kReadPane);
        REQUIRE(row != nullptr);
        canvases[at].kind = row->kind;
        return at;
    }

    /// THE PANE AUTHORED AT (`x`, `y`), `w` BY `h`, in canvas cells from the room's corner.
    void place(std::size_t at, std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        std::size_t placed = 0;
        for (SetupPane& p : r.session().setup.active.panes) {
            if (p.ref.provider != canvases.at(at).office || p.ref.pane != kReadPane) continue;
            p.place = {pane_unit::kPixels, x * gfx::kCellPx, y * gfx::kCellPx};
            p.width = {pane_unit::kPixels, w * gfx::kCellPx};
            p.height = {pane_unit::kPixels, h * gfx::kCellPx};
            ++placed;
        }
        REQUIRE(placed == 1);
    }

    /// The desk seated again where its panes are authored: a same-size extent reseats nothing.
    void reseat() {
        r.extent(columns - 1, rows);
        r.extent(columns, rows);
    }

    /// The room the window last gave canvas `at`'s area.
    CanvasRoom room(std::size_t at) {
        REQUIRE_FALSE(canvases.at(at).seat->rooms.empty());
        return canvases.at(at).seat->room();
    }

    /// `picture`, drawn in canvas `at`'s area for the room the window last gave it, and kept there.
    void draw(std::size_t at, const CanvasPicture& picture) {
        ReadCanvas& c = canvases.at(at);
        REQUIRE_FALSE(c.seat->rooms.empty());
        const std::size_t refused = c.seat->rejected.size();
        const int before = c.seat->drawn;
        drive(at, [seat = c.seat, picture](loom::Mail& m) { seat->draw(m, picture); });
        REQUIRE_MESSAGE(c.seat->over.empty(), c.seat->over);
        const std::string why =
            c.seat->rejected.size() > refused ? c.seat->rejected.back().reason : std::string();
        REQUIRE_MESSAGE(why.empty(), why);
        REQUIRE(c.seat->drawn == before + 1);
    }

    void ask(std::function<void(loom::Mail&)> what) {
        asker->next = std::move(what);
        (void)r.bus.send(asker_id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }

    /// The desk said whole, or the refusal's reason.
    std::string read(DeskRead& out) {
        asker->refusals.clear();
        const std::size_t before = asker->reads.size();
        ask([](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, DeskReadRequested{}); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->reads.size() == before + 1);
        out = asker->reads.back();
        return std::string();
    }

    /// One page of a pane's reading, or the refusal's reason.
    std::string page(const PaneViewRequested& asked, PaneView& out) {
        asker->refusals.clear();
        const std::size_t before = asker->pages.size();
        ask([&](loom::Mail& m) { (void)m.send_to_role(kWorkshopProvider, asked); });
        if (!asker->refusals.empty()) return asker->refusals.back();
        REQUIRE(asker->pages.size() == before + 1);
        out = asker->pages.back();
        return std::string();
    }

};

/// The items one page holds: its words and its parts.
template <class View>
std::int64_t item_count(const View& v) {
    return static_cast<std::int64_t>(v.words.size() + v.parts.size());
}

bool same_pane(const PaneStamp& s, const PaneView& v) {
    return s.provider == v.stamp.provider && s.pane == v.stamp.pane;
}

/// Whether `v` is read from what `s` names: its holder, incarnation, window and what it shows.
bool stands_on(const PaneView& v, const PaneStamp& s) {
    return same_pane(s, v) && s.holder == v.stamp.holder &&
           s.incarnation == v.stamp.incarnation && s.epoch == v.stamp.epoch &&
           s.fingerprint == v.stamp.fingerprint;
}

PaneStamp view_stamp(const PaneView& v) { return v.stamp; }

/// The items of `pages`, in order, as one reading from its first item.
PaneView joined(const std::vector<PaneView>& pages) {
    REQUIRE_FALSE(pages.empty());
    PaneView whole = pages.front();
    for (std::size_t i = 1; i < pages.size(); ++i) {
        whole.words.insert(whole.words.end(), pages[i].words.begin(), pages[i].words.end());
        whole.parts.insert(whole.parts.end(), pages[i].parts.begin(), pages[i].parts.end());
    }
    return whole;
}

/// ONE PANE READ ALONE: page after page under `stamp`, each from where the last ended, until the
/// pages hold every item; the refusal's reason when a page is refused.
std::string read_alone(ReadRig& d, const PaneStamp& stamp, std::vector<PaneView>& pages) {
    pages.clear();
    std::int64_t from = 0;
    for (;;) {
        PaneView one;
        const std::string why =
            d.page(PaneViewRequested{stamp.provider, stamp.pane, from, stamp}, one);
        if (!why.empty()) return why;
        pages.push_back(one);
        const std::int64_t got = item_count(one);
        from += got;
        if (got == 0 || from >= one.total) return std::string();
    }
}

const DeskPane* desk_pane(const DeskView& desk, const std::string& provider,
                          const std::string& pane) {
    for (const DeskPane& p : desk.panes) {
        if (p.provider == provider && p.pane == pane) return &p;
    }
    return nullptr;
}

/// `n` in `digits` digits, zeros before it.
std::string padded(std::size_t n, std::size_t digits) {
    std::string s = std::to_string(n);
    return std::string(digits > s.size() ? digits - s.size() : 0, '0') + s;
}

/// WHERE THE `i`TH OF A GRID OF WORDS `cells` WIDE STANDS in `room`, a blank cell after each, row by
/// row: a window reads each as a run of its own, in their order.
std::pair<std::int64_t, std::int64_t> word_slot(std::size_t i, std::int64_t cells,
                                                const CanvasRoom& room) {
    const std::int64_t per_row = room.width / kPaneCanvasUnit / (cells + 1);
    REQUIRE(per_row > 0);
    const auto n = static_cast<std::int64_t>(i);
    return {(n % per_row) * (cells + 1) * kPaneCanvasUnit, (n / per_row) * kPaneCanvasUnit};
}

/// Whether `count` words `cells` wide stand whole in `room`.
bool words_fit(std::size_t count, std::int64_t cells, const CanvasRoom& room) {
    return count > 0 && word_slot(count - 1, cells, room).second + kPaneCanvasUnit <= room.height;
}

/// HOW MANY WORDS EACH PANE OF A DESK PAST ONE VALUE'S BUDGET DRAWS: four panes of them are more
/// than one decoded value holds.
constexpr std::size_t kReadWords = 2000;
constexpr std::int64_t kReadWordCells = 5;

std::string label_text(std::size_t pane, std::size_t i) { return std::to_string(pane) + padded(i, 4); }

/// `kReadWords` LABELS, each its own word, row by row from the body's corner.
CanvasPicture labelled(std::size_t pane, const CanvasRoom& room) {
    REQUIRE(words_fit(kReadWords, kReadWordCells, room));
    CanvasPicture p;
    for (std::size_t i = 0; i < kReadWords; ++i) {
        const auto [x, y] = word_slot(i, kReadWordCells, room);
        p.labels.push_back(CanvasLabel{x, y, label_text(pane, i), gfx::role::kFill});
    }
    return p;
}

/// A PICTURE AT A WINDOW'S BUDGETS: as many parts as an area keeps, each over a word of its own,
/// and as many words as the nodes left hold -- the ground, two groups, each word, and each part's
/// group with the word it paints again -- labels first, then runs of text.
constexpr std::size_t kFullParts = gfx::kMaxElementsPerArea;
constexpr std::size_t kFullWords = gfx::kMaxNodesPerArea - 3 - 2 * kFullParts;
constexpr std::size_t kFullLabels = kFullWords / 2;
constexpr std::int64_t kFullWordCells = 5;

/// The word a full picture draws `i`th: its labels' and then its runs'.
std::string full_word(std::size_t i) {
    return i < kFullLabels ? "l" + padded(i, 4) : "t" + padded(i - kFullLabels, 4);
}

CanvasPicture full_picture(const CanvasRoom& room) {
    REQUIRE(words_fit(kFullWords, kFullWordCells, room));
    CanvasPicture p;
    for (std::size_t i = 0; i < kFullWords; ++i) {
        const auto [x, y] = word_slot(i, kFullWordCells, room);
        if (i < kFullLabels) {
            p.labels.push_back(CanvasLabel{x, y, full_word(i), gfx::role::kFill});
        } else {
            p.texts.push_back(CanvasText{x, y, full_word(i), gfx::role::kFill});
        }
    }
    for (std::size_t i = 0; i < kFullParts; ++i) {
        const auto [x, y] = word_slot(i, kFullWordCells, room);
        p.parts.push_back(CanvasPart{"p" + std::to_string(i), x, y, kFullWordCells * kPaneCanvasUnit,
                                     kPaneCanvasUnit});
    }
    return p;
}

/// A HIGH-TEXT PICTURE: `kHighWords` rows of a word `kHighTextBytes` long, each under a part of its
/// own, so every part's reading repeats its row's word: each word said twice, its row as wide as
/// the widest body a pane may ask (`kMaxPaneComfort` cells) holds.
constexpr std::size_t kHighTextBytes = 500;
constexpr std::size_t kHighWords = 300;
static_assert(kHighTextBytes + 2 <= static_cast<std::size_t>(kMaxPaneComfort));
static_assert(kHighWords + 2 <= static_cast<std::size_t>(kMaxPaneComfort));
constexpr std::size_t kHighTextParts = kHighWords;
static_assert(3 + kHighWords + 2 * kHighTextParts <= gfx::kMaxNodesPerArea);
static_assert(kHighTextParts <= gfx::kMaxElementsPerArea);

std::string high_text_word(std::size_t i) {
    std::string word = "h" + std::to_string(i) + "-";
    word.resize(kHighTextBytes, 'x');
    return word;
}

CanvasPicture high_text_picture() {
    CanvasPicture p;
    for (std::size_t i = 0; i < kHighWords; ++i) {
        p.labels.push_back(CanvasLabel{0, static_cast<std::int64_t>(i) * kPaneCanvasUnit,
                                       high_text_word(i), gfx::role::kFill});
    }
    for (std::size_t i = 0; i < kHighTextParts; ++i) {
        p.parts.push_back(CanvasPart{"over" + std::to_string(i), 0,
                                     static_cast<std::int64_t>(i) * kPaneCanvasUnit,
                                     static_cast<std::int64_t>(kHighTextBytes) * kPaneCanvasUnit,
                                     kPaneCanvasUnit});
    }
    return p;
}

/// A DESK LARGE ENOUGH FOR A HIGH-TEXT PICTURE, and its one canvas pane, offered at the largest
/// body a pane may ask and placed at the size the picture needs.
void seat_high_text(ReadRig& d, const std::string& office) {
    const auto across = static_cast<std::int64_t>(kHighTextBytes);
    const auto down = static_cast<std::int64_t>(kHighWords);
    d.size(across + 20, down + 20);
    (void)d.open_canvas(office, (std::min)(across + 2, kMaxPaneComfort),
                        (std::min)(down + 2, kMaxPaneComfort));
    d.place(0, 2, 2, across + 8, down + 8);
    d.reseat();
}

/// ...and the pane drawing it.
void open_high_text(ReadRig& d, const std::string& office) {
    seat_high_text(d, office);
    d.draw(0, high_text_picture());
}

/// THE ITEMS `whole` HOLDS ARE THE HIGH-TEXT PICTURE'S, in order: each word, and each part by its
/// name, repeating its row's word. Counts the items that are not.
std::size_t high_text_wrong(const PaneView& whole) {
    REQUIRE(whole.words.size() == kHighWords);
    REQUIRE(whole.parts.size() == kHighTextParts);
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < whole.words.size(); ++i) {
        if (whole.words[i].text != high_text_word(i)) ++wrong;
    }
    for (std::size_t i = 0; i < whole.parts.size(); ++i) {
        if (whole.parts[i].name != "over" + std::to_string(i) ||
            whole.parts[i].text != high_text_word(i)) {
            ++wrong;
        }
    }
    return wrong;
}

void json_text(const std::string& text, std::string& out) {
    out += '"';
    for (const char ch : text) {
        const auto byte = static_cast<unsigned char>(ch);
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += ch;
        } else if (byte < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof escaped, "\\u%04x", byte);
            out += escaped;
        } else {
            out += ch;
        }
    }
    out += '"';
}

void json_value(const loom::Value& v, std::size_t depth, std::string& out);

void json_cell(const loom::Cell& c, std::size_t depth, std::string& out) {
    switch (c.kind()) {
    case loom::Kind::Int: out += std::to_string(c.as_int()); break;
    case loom::Kind::Bool: out += c.as_bool() ? "true" : "false"; break;
    case loom::Kind::Text: json_text(c.as_text(), out); break;
    case loom::Kind::Message: json_value(*c.as_message(), depth, out); break;
    case loom::Kind::List: {
        const loom::Cell::Array& items = c.as_list();
        out += '[';
        for (std::size_t i = 0; i < items.size(); ++i) {
            out += i ? ",\n" : "\n";
            out.append(depth + 1, ' ');
            json_cell(items[i], depth + 1, out);
        }
        if (!items.empty()) {
            out += '\n';
            out.append(depth, ' ');
        }
        out += ']';
        break;
    }
    default: FAIL("a desk reading holds no cell of kind " << static_cast<int>(c.kind()));
    }
}

/// A VALUE AS JSON, its fields in its shape's order, one to a line and indented a space a level:
/// a desk reading said as Workshop said it, for a reader in Python.
void json_value(const loom::Value& v, std::size_t depth, std::string& out) {
    out += '{';
    bool any = false;
    for (std::size_t i = 0; i < v.field_count(); ++i) {
        const loom::Cell* c = v.at(i);
        if (c == nullptr) continue;
        out += any ? ",\n" : "\n";
        any = true;
        out.append(depth + 1, ' ');
        json_text(v.schema().fields()[i].name, out);
        out += ": ";
        json_cell(*c, depth + 1, out);
    }
    if (any) {
        out += '\n';
        out.append(depth, ' ');
    }
    out += '}';
}

} // namespace

TEST_CASE("the decode budget a reading is filled to is Loom's own: a value of exactly that many cells decodes, and one cell more is refused") {
    // A DESK READ OF ONE PANE'S READING: its words, and then its cover's names, a cell each, to the
    // budget's last cell.
    DeskRead full;
    full.stamps.push_back(PaneStamp{"zengine.test.read", kReadPane, 7, 1, 3, 9});
    full.panes.emplace_back();
    PaneView& pane = full.panes.back();
    pane.stamp = PaneStamp{"zengine.test.read", kReadPane, 7, 1, 3, 9};
    for (std::int64_t i = 0; i < 4096; ++i) {
        pane.words.push_back(PaneWord{i, "w" + std::to_string(i), DeskRect{i, 0, 12, 12}, i, 6,
                                      input::space::kCells});
    }
    pane.total = 4096;
    const std::int64_t short_of = kDecodedCellBudget - decoded_cells(loom::to_value(full));
    REQUIRE(short_of > 0);
    pane.covered.by.assign(static_cast<std::size_t>(short_of), std::string("pane"));
    REQUIRE(decoded_cells(loom::to_value(full)) == kDecodedCellBudget);

    // LOOM DECODES IT, every cell of it, and the value it gives back is the one sent.
    const std::string bytes = loom::serialize(loom::to_value(full));
    const loom::Admission exact = loom::admit(loom::parse(bytes), loom::schema_of<DeskRead>());
    REQUIRE_MESSAGE(exact.ok(), (exact.ok() ? std::string() : exact.first_error().message()));
    const DeskRead back = loom::from_value<DeskRead>(exact.value());
    CHECK(loom::serialize(loom::to_value(back)) == bytes);
    REQUIRE(back.panes.size() == 1);
    CHECK(back.panes[0].covered.by.size() == static_cast<std::size_t>(short_of));

    // ...AND ONE CELL MORE IS REFUSED before it is built, as past Loom's materialization budget.
    pane.covered.by.push_back("pane");
    REQUIRE(decoded_cells(loom::to_value(full)) == kDecodedCellBudget + 1);
    const loom::Admission over = loom::admit(loom::parse(loom::serialize(loom::to_value(full))),
                                             loom::schema_of<DeskRead>());
    REQUIRE_FALSE(over.ok());
    CHECK(over.first_error().kind == loom::ErrorKind::MalformedBytes);
    CHECK_MESSAGE(over.first_error().detail.find("materialization budget") != std::string::npos,
                  over.first_error().message());
}

TEST_CASE("a desk past one value's budget reads whole: the panes past it are named by their stamps and read alone") {
    ReadRig d;
    // FOUR CANVAS PANES, two by two, none over another, each drawing its words: their readings
    // together are more than one decoded value holds.
    constexpr std::size_t kPanes = 4;
    d.size(264, 224);
    for (std::size_t k = 0; k < kPanes; ++k) {
        (void)d.open_canvas("zengine.test.read-" + std::to_string(k), 30, 8);
    }
    for (std::size_t k = 0; k < kPanes; ++k) {
        d.place(k, 2 + 130 * static_cast<std::int64_t>(k % 2), 2 + 110 * static_cast<std::int64_t>(k / 2),
                126, 106);
    }
    d.reseat();
    for (std::size_t k = 0; k < kPanes; ++k) {
        d.draw(k, labelled(k, d.room(k)));
    }

    DeskRead desk_read;
    const std::string refused = d.read(desk_read);
    REQUIRE_MESSAGE(refused.empty(), refused);
    // ONE DECODED VALUE: the desk read spends no more cells than a reader's decoder allows.
    const std::int64_t cells = decoded_cells(loom::to_value(desk_read));
    CHECK_MESSAGE(cells <= kDecodedCellBudget, "the desk read is " << cells << " cells");

    // EVERY PRESENTED PANE'S STAMP, FRONT TO BACK, by the rank the desk gives it.
    std::int64_t last = -1;
    for (const PaneStamp& s : desk_read.stamps) {
        const DeskPane* p = desk_pane(desk_read.desk, s.provider, s.pane);
        REQUIRE_MESSAGE(p != nullptr, s.provider << "/" << s.pane << " is not a pane on the desk");
        CHECK_MESSAGE(p->front > last, s.provider << "/" << s.pane << " is ranked " << p->front);
        last = p->front;
    }
    // EACH CANVAS PANE IS NAMED ONCE, by its holder and that holder's incarnation, the room it drew
    // for and the picture it drew.
    for (const ReadCanvas& c : d.canvases) {
        CAPTURE(c.office);
        const auto named = [&c](const PaneStamp& s) { return s.provider == c.office && s.pane == kReadPane; };
        REQUIRE(std::count_if(desk_read.stamps.begin(), desk_read.stamps.end(), named) == 1);
        const PaneStamp& s = *std::find_if(desk_read.stamps.begin(), desk_read.stamps.end(), named);
        CHECK(s.holder == static_cast<std::int64_t>(c.id.value));
        CHECK(s.incarnation == static_cast<std::int64_t>(d.r.bus.participant(c.id).incarnation));
        CHECK(s.fingerprint != 0);
    }

    // THE READINGS ARE THE STAMPS' OWN, IN THE STAMPS' ORDER, each whole; a stamp with no reading
    // before the last one names a pane with nothing to read.
    std::size_t at = 0;
    for (const PaneView& v : desk_read.panes) {
        while (at < desk_read.stamps.size() && !same_pane(desk_read.stamps[at], v)) ++at;
        REQUIRE_MESSAGE(at < desk_read.stamps.size(),
                        v.stamp.provider << "/" << v.stamp.pane << " is read out of its stamps' order");
        CHECK(stands_on(v, desk_read.stamps[at]));
        CHECK(v.from == 0);
        CHECK(item_count(v) == v.total);
        ++at;
    }
    REQUIRE_MESSAGE(at < desk_read.stamps.size(), "every pane was read in the one value");

    // EVERY PANE PAST THE LAST READING IS READ ALONE under the stamp the desk named, and the first of
    // them that reads is one the value had no room left for.
    std::vector<PaneView> wholes = desk_read.panes;
    std::size_t alone = 0;
    for (std::size_t i = at; i < desk_read.stamps.size(); ++i) {
        std::vector<PaneView> pages;
        if (!read_alone(d, desk_read.stamps[i], pages).empty()) continue;
        const PaneView whole = joined(pages);
        CHECK(stands_on(whole, desk_read.stamps[i]));
        CHECK(item_count(whole) == whole.total);
        if (alone == 0) {
            const std::int64_t with = cells + 1 + decoded_cells(loom::to_value(whole));
            CHECK_MESSAGE(with > kDecodedCellBudget,
                          whole.stamp.provider << "/" << whole.stamp.pane << " would have fit: " << with
                                               << " cells");
        }
        ++alone;
        wholes.push_back(whole);
    }
    CHECK(alone > 0);
    MESSAGE("readings in the desk read " << desk_read.panes.size() << ", read alone " << alone
                                         << ", stamps " << desk_read.stamps.size());

    // EVERY WORD OF EVERY CANVAS PANE IS READ, in the order the pane drew them, and none is covered.
    for (std::size_t k = 0; k < kPanes; ++k) {
        CAPTURE(k);
        const std::string& office = d.canvases[k].office;
        const auto it = std::find_if(wholes.begin(), wholes.end(),
                                     [&office](const PaneView& v) { return v.stamp.provider == office; });
        REQUIRE_MESSAGE(it != wholes.end(), office << " was read neither in the desk read nor alone");
        CHECK_FALSE(it->in_flight);
        CHECK_MESSAGE(it->covered.words == 0, it->covered.words << " words covered");
        REQUIRE(it->words.size() == kReadWords);
        std::size_t wrong = 0;
        for (std::size_t i = 0; i < it->words.size(); ++i) {
            if (it->words[i].text != label_text(k, i)) ++wrong;
        }
        CHECK_MESSAGE(wrong == 0, wrong << " words are not the labels drawn, in their order");
    }
}

TEST_CASE("a pane at a window's budgets reads whole under one stamp, and from any index of it, and a page asked after its picture moved is refused as stale") {
    ReadRig d;
    d.size(140, 124);
    const std::string office = "zengine.test.read-full";
    (void)d.open_canvas(office, 120, 106);
    d.place(0, 2, 2, 126, 116);
    d.reseat();
    // ROOM FOR THE WHOLE PICTURE: its words a run each, and each part over one of them.
    d.draw(0, full_picture(d.room(0)));
    const auto drawn = static_cast<std::int64_t>(kFullWords + kFullParts);

    // THE WHOLE, asked under no stamp: what one area shows is no more than one decoded value.
    PaneView first;
    const std::string unread = d.page(PaneViewRequested{office, kReadPane, 0, PaneStamp{}}, first);
    REQUIRE_MESSAGE(unread.empty(), unread);
    REQUIRE_FALSE(first.in_flight);
    CHECK_MESSAGE(first.covered.words == 0, first.covered.words << " words covered");
    REQUIRE(first.total == drawn);
    CHECK(first.from == 0);
    CHECK(first.stamp.fingerprint != 0);
    CHECK(item_count(first) == first.total);
    CHECK(decoded_cells(loom::to_value(first)) <= kDecodedCellBudget);
    const PaneStamp stamp = view_stamp(first);

    // FROM AN INDEX, under its stamp: the rest of it.
    const std::int64_t half = first.total / 2;
    PaneView rest;
    const std::string why = d.page(PaneViewRequested{office, kReadPane, half, stamp}, rest);
    REQUIRE_MESSAGE(why.empty(), why);
    CHECK(rest.from == half);
    CHECK(rest.total == first.total);
    CHECK(item_count(rest) == first.total - half);
    CHECK(stands_on(rest, stamp));

    // THE WHOLE, in order: the labels' words, the runs', then every part by its name, holding the
    // word it stands over.
    REQUIRE(first.words.size() == kFullWords);
    REQUIRE(first.parts.size() == kFullParts);
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < first.words.size(); ++i) {
        if (first.words[i].text != full_word(i)) ++wrong;
    }
    for (std::size_t i = 0; i < first.parts.size(); ++i) {
        if (first.parts[i].name != "p" + std::to_string(i) || first.parts[i].text != full_word(i)) {
            ++wrong;
        }
    }
    CHECK_MESSAGE(wrong == 0, wrong << " items are not the ones drawn, in their order");

    // THE PANE DRAWS SOMETHING ELSE: the rest, asked under the stamp it moved off, is refused as
    // stale...
    CanvasPicture changed = full_picture(d.room(0));
    changed.labels.front().text = "other";
    d.draw(0, changed);
    PaneView late;
    const std::string stale = d.page(PaneViewRequested{office, kReadPane, half, stamp}, late);
    CHECK_MESSAGE(stale.find("stale") != std::string::npos, "the late page was answered: [" << stale << "]");
    // ...AND READ AGAIN FROM THE START, the pane stands on its new picture.
    PaneView again;
    REQUIRE(d.page(PaneViewRequested{office, kReadPane, 0, PaneStamp{}}, again).empty());
    CHECK(again.stamp.fingerprint != stamp.fingerprint);
}

namespace {

struct ReadGuestState {
    ZEN_SHAPE(ReadGuestState, 1);
};

/// A GUEST SESSION'S PARTICIPANT: it asks Workshop for the desk, a pane's page, the inventory and
/// the keymap, keeps each answer to its own ask apart from what is said to every listener, and
/// keeps Loom's word on each ask Loom refused.
class ReadGuest
    : public loom::WeaveBase<ReadGuest, ReadGuestState,
                             loom::Accept<SeatDo, DeskRead, PaneView, DeskRead, PaneView,
                                          PaneInventory, KeymapShown, loom::Refused,
                                          loom::DispatchRefused>,
                             loom::Emit<DeskReadRequested, PaneViewRequested,
                                        PaneInventoryRequested, KeymapRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<std::string> answered, heard, denied, refusals;
    std::vector<DeskRead> reads;
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    void on(const DeskRead& said, loom::Mail& m) {
        reads.push_back(said);
        keep(m, "DeskRead");
    }
    void on(const PaneView&, loom::Mail& m) { keep(m, "PaneView"); }
    void on(const PaneInventory&, loom::Mail& m) { keep(m, "PaneInventory"); }
    void on(const KeymapShown&, loom::Mail& m) { keep(m, "KeymapShown"); }
    void on(const loom::Refused& said, loom::Mail&) { refusals.push_back(said.reason); }
    void on(const loom::DispatchRefused& said, loom::Mail& m) {
        if (m.dispatch_refused()) denied.push_back(said.shape + ": " + said.reason);
    }

private:
    void keep(const loom::Mail& m, const char* what) {
        (m.answers_ask() ? answered : heard).push_back(what);
    }
};

ReadGuest* mount_guest(ReadRig& d, loom::Grant grant, loom::WeaveId& id) {
    auto made = std::make_unique<ReadGuest>();
    ReadGuest* raw = made.get();
    id = d.r.bus.register_weave(std::move(made), std::move(grant));
    raw->zen_set_self(id);
    return raw;
}

void guest_asks(ReadRig& d, ReadGuest* guest, loom::WeaveId id,
                std::function<void(loom::Mail&)> what) {
    guest->next = std::move(what);
    (void)d.r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
    d.r.bus.drain_until_idle();
}

std::size_t count_of(const std::vector<std::string>& said, const std::string& what) {
    return static_cast<std::size_t>(std::count(said.begin(), said.end(), what));
}

/// A ROW THE GUEST DOOR ADMITTED, holding `powers`, as the host seam answers it.
scope::GuestRowFacts admitted_row(std::vector<std::string> powers) {
    scope::GuestRowFacts row;
    row.admitted = true;
    row.name = "agent";
    row.powers = std::move(powers);
    row.version = 2;
    return row;
}

/// WHAT `capture` GRANTS A GUEST SESSION, as the guests file's policy writes it
/// (`guests::grant_for`): every reading of the desk asked of Workshop's office, and the medium's
/// capture. This suite does not link that policy; the guests suite pins the same list against it.
loom::Grant capture_grant() {
    loom::Grant grant;
    for (const auto& shape :
         {loom::schema_of<DeskReadRequested>(), loom::schema_of<PaneViewRequested>(),
          loom::schema_of<PanePointRequested>(), loom::schema_of<PaneInventoryRequested>(),
          loom::schema_of<KeymapRequested>()}) {
        grant.allow_to_role(shape->name(), shape->version(), kWorkshopProvider);
    }
    grant.allow_to_role(gfx::SurfaceCaptureRequested::zen_name,
                        gfx::SurfaceCaptureRequested::zen_version, surface::kSkinRole);
    grant.allow_to_role(gfx::SurfaceCaptureChunkRequested::zen_name,
                        gfx::SurfaceCaptureChunkRequested::zen_version, surface::kSkinRole);
    return grant;
}

/// ...AND WHAT `input` GRANTS ONE: Input's session and injection doors, and nothing of the desk's.
loom::Grant hand_grant() {
    loom::Grant grant;
    for (const auto& shape :
         {loom::schema_of<input::InputSessionRequested>(), loom::schema_of<input::PointerMotionRequested>(),
          loom::schema_of<input::InjectInput>(), loom::schema_of<input::InputSessionClosed>()}) {
        grant.allow_to_role(shape->name(), shape->version(), input::kInputRole);
    }
    return grant;
}

} // namespace

TEST_CASE("a capture guest's asks for the desk, a pane's page, the inventory and the keymap are answered to it alone, and a guest without capture is refused") {
    // THE HOST SEAM ANSWERS EACH PARTICIPANT AS THE ROW ITS SESSION WAS ADMITTED UNDER, or as no row.
    std::map<std::uint64_t, scope::GuestRowFacts> rows;
    ReadRig d;
    d.r.host.guest_row = [&rows](loom::WeaveId who) {
        const auto it = rows.find(who.value);
        return it == rows.end() ? scope::GuestRowFacts{} : it->second;
    };
    const std::string office = "zengine.test.read-guest";
    (void)d.open_canvas(office, 30, 8);
    d.place(0, 4, 4, 32, 12);
    d.reseat();
    CanvasPicture picture;
    picture.labels.push_back(CanvasLabel{0, 0, "guest words", gfx::role::kFill});
    d.draw(0, picture);
    const auto ask_all = [office](loom::Mail& m) {
        (void)m.send_to_role(kWorkshopProvider, DeskReadRequested{});
        (void)m.send_to_role(kWorkshopProvider, PaneViewRequested{office, kReadPane, 0, PaneStamp{}});
        (void)m.send_to_role(kWorkshopProvider, PaneInventoryRequested{});
        (void)m.send_to_role(kWorkshopProvider, KeymapRequested{});
    };

    // A GUEST ADMITTED UNDER A ROW HOLDING `capture`, granted what that row is granted, and a
    // bystander that asks nothing.
    loom::WeaveId guest_id{}, bystander_id{};
    ReadGuest* guest = mount_guest(d, capture_grant(), guest_id);
    ReadGuest* bystander = mount_guest(d, loom::Grant{}, bystander_id);
    rows[guest_id.value] = admitted_row({scope::kPowerCapture});
    guest_asks(d, guest, guest_id, ask_all);
    // EACH ASK IS ANSWERED ONCE, to the guest's own ask, and none is answered to the bystander.
    for (const char* shape :
         {"DeskRead", "PaneView", "PaneInventory", "KeymapShown"}) {
        CAPTURE(shape);
        CHECK(count_of(guest->answered, shape) == 1);
        CHECK(count_of(bystander->answered, shape) == 0);
    }
    CHECK_MESSAGE(guest->refusals.empty(), (guest->refusals.empty() ? std::string() : guest->refusals.front()));
    CHECK(guest->denied.empty());
    // ...AND THE DESK AND THE PAGE ARE SAID TO NO ONE ELSE, the canvas pane named among the desk's.
    CHECK(count_of(bystander->heard, "DeskRead") == 0);
    CHECK(count_of(bystander->heard, "PaneView") == 0);
    REQUIRE(guest->reads.size() == 1);
    CHECK(std::any_of(guest->reads[0].stamps.begin(), guest->reads[0].stamps.end(),
                      [&office](const PaneStamp& s) { return s.provider == office; }));

    // A GUEST ADMITTED WITHOUT `capture` IS GRANTED NONE OF THOSE ASKS: Loom refuses each before
    // Workshop hears it, and tells the asker which and why.
    loom::WeaveId plain_id{};
    ReadGuest* plain = mount_guest(d, hand_grant(), plain_id);
    rows[plain_id.value] = admitted_row({"input"});
    guest_asks(d, plain, plain_id, ask_all);
    CHECK(plain->answered.empty());
    CHECK(plain->refusals.empty());
    CHECK(plain->denied.size() == 4);
    for (const char* shape : {DeskReadRequested::zen_name, PaneViewRequested::zen_name,
                              PaneInventoryRequested::zen_name, KeymapRequested::zen_name}) {
        CAPTURE(shape);
        CHECK(count_of(plain->denied, std::string(shape) + ": CapabilityDenied") == 1);
    }

    // A PARTICIPANT GRANTED THE INVENTORY AND THE KEYMAP WHOSE ROW HOLDS NO `capture` is answered by
    // nobody: Workshop asks the row the session was admitted under, not the grant...
    loom::Grant lists;
    lists.allow_to_role(PaneInventoryRequested::zen_name, PaneInventoryRequested::zen_version,
                        kWorkshopProvider);
    lists.allow_to_role(KeymapRequested::zen_name, KeymapRequested::zen_version, kWorkshopProvider);
    loom::WeaveId granted_id{};
    ReadGuest* granted = mount_guest(d, std::move(lists), granted_id);
    rows[granted_id.value] = admitted_row({"input"});
    const auto ask_lists = [](loom::Mail& m) {
        (void)m.send_to_role(kWorkshopProvider, PaneInventoryRequested{});
        (void)m.send_to_role(kWorkshopProvider, KeymapRequested{});
    };
    guest_asks(d, granted, granted_id, ask_lists);
    CHECK(granted->answered.empty());
    CHECK(granted->denied.empty());
    CHECK(granted->refusals.empty());
    // ...AND THE SAME PARTICIPANT, ITS ROW HOLDING `capture`, is answered both.
    rows[granted_id.value] = admitted_row({scope::kPowerCapture});
    guest_asks(d, granted, granted_id, ask_lists);
    CHECK(count_of(granted->answered, "PaneInventory") == 1);
    CHECK(count_of(granted->answered, "KeymapShown") == 1);
}

TEST_CASE("a picture of long words under a part a row, as many as a window keeps, reads whole, each page and the desk read inside one reply's bytes") {
    ReadRig d;
    const std::string office = "zengine.test.read-high";
    seat_high_text(d, office);
    // A SMALL PICTURE FIRST: the desk read carries its reading whole, in the one turn.
    CanvasPicture small;
    small.labels.push_back(CanvasLabel{0, 0, "small", gfx::role::kFill});
    d.draw(0, small);
    DeskRead desk;
    REQUIRE(d.read(desk).empty());
    const auto carried = [&] {
        return std::find_if(desk.panes.begin(), desk.panes.end(), [&](const PaneView& v) {
            return v.stamp.provider == office && v.stamp.pane == kReadPane;
        });
    };
    REQUIRE(carried() != desk.panes.end());
    CHECK(carried()->total == 1);
    CHECK(item_count(*carried()) == 1);

    // THE HIGH-TEXT PICTURE: the desk read stays inside one reply's bytes and one decoded value.
    d.draw(0, high_text_picture());
    REQUIRE(d.read(desk).empty());
    CHECK(reply_bytes(loom::to_value(desk)) <= kReplyByteBudget);
    CHECK(decoded_cells(loom::to_value(desk)) <= kDecodedCellBudget);
    const auto stamp = std::find_if(desk.stamps.begin(), desk.stamps.end(), [&](const PaneStamp& s) {
        return s.provider == office && s.pane == kReadPane;
    });
    REQUIRE(stamp != desk.stamps.end());

    // READ ALONE under its stamp: each page inside one reply's bytes, in either serialization, and
    // one decoded value.
    std::vector<PaneView> pages;
    const std::string why = read_alone(d, *stamp, pages);
    REQUIRE_MESSAGE(why.empty(), why);
    for (const PaneView& p : pages) {
        const loom::Value page = loom::to_value(p);
        const std::size_t native = loom::serialize(page).size();
        const std::size_t json = loom::compat::serialize(page).size();
        CHECK_MESSAGE(native <= static_cast<std::size_t>(kReplyByteBudget),
                      "the page from " << p.from << " is " << native << " bytes");
        CHECK_MESSAGE(json <= static_cast<std::size_t>(kReplyByteBudget),
                      "the page from " << p.from << " is " << json << " bytes of JSON");
        CHECK(decoded_cells(page) <= kDecodedCellBudget);
        CHECK(stands_on(p, view_stamp(pages.front())));
    }
    // TOGETHER THE PAGES ARE THE WHOLE PICTURE, every word and every part with every word's text.
    const PaneView whole = joined(pages);
    CHECK(high_text_wrong(whole) == 0);
    CHECK(loom::serialize(loom::to_value(whole)).size() > 2 * kHighWords * kHighTextBytes);

    // ...AND READ AGAIN BY THE STAMP A SECOND DESK READ NAMES, THE SAME: the desk read and each page
    // inside one reply's bytes.
    DeskRead second;
    REQUIRE(d.read(second).empty());
    CHECK(reply_bytes(loom::to_value(second)) <= kReplyByteBudget);
    CHECK(decoded_cells(loom::to_value(second)) <= kDecodedCellBudget);
    const auto named = std::find_if(second.stamps.begin(), second.stamps.end(),
                                    [&](const PaneStamp& s) { return s.provider == office; });
    REQUIRE(named != second.stamps.end());
    std::vector<PaneView> fifth;
    for (std::int64_t from = 0;;) {
        PaneView one;
        const std::string refused = d.page(PaneViewRequested{office, kReadPane, from, *named}, one);
        REQUIRE_MESSAGE(refused.empty(), refused);
        CHECK(reply_bytes(loom::to_value(one)) <= kReplyByteBudget);
        CHECK(decoded_cells(loom::to_value(one)) <= kDecodedCellBudget);
        fifth.push_back(one);
        from += item_count(one);
        if (item_count(one) == 0 || from >= one.total) break;
    }
    CHECK(fifth.size() == pages.size());
}

TEST_CASE("a capture guest reads a high-text picture whole over the bridge, page by page, and its connection answers the next ask") {
    ReadRig d;
    const std::string office = "zengine.test.read-high";
    open_high_text(d, office);

    // WORKSHOP'S OWN DOOR on the rig's bus, a row of the guests file holding `capture`.
    guests::GuestsFile file;
    file.rows.push_back(guests::GuestRow{"agent", "desk-key", {"capture"}, {}, false, "", false, {}, {}});
    std::string err;
    REQUIRE(loom::bridge_net_init(&err));
    const loom::socket_t listener = loom::bridge_listen_tcp(0, &err);
    REQUIRE_MESSAGE(listener != loom::kInvalidSocket, err);
    const std::uint16_t port = loom::bridge_socket_port(listener);
    auto made = std::make_unique<GuestDoor>(d.r.bus, listener, "127.0.0.1:" + std::to_string(port),
                                            file);
    GuestDoor* door = made.get();
    const loom::WeaveId door_id =
        d.r.bus.register_weave(std::move(made), guest_door_grant(), std::string(kGuestsRole));
    door->zen_set_self(door_id);

    // THE GUEST, on the far side of the socket, keeping each answer by the ask it answers.
    const loom::socket_t s = loom::bridge_connect_tcp("127.0.0.1", port, &err);
    REQUIRE_MESSAGE(s != loom::kInvalidSocket, err);
    loom::BridgeClient guest(s);
    REQUIRE(guest.hello("agent", "desk-key"));
    std::map<std::uint64_t, std::string> answers;
    const auto until = [&](const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        for (;;) {
            (void)d.r.bus.send(door_id, loom::Message(loom::to_value(
                                            zengine::timer::TimerFired{kGuestBeatId})));
            d.r.bus.drain_until_idle();
            std::vector<loom::BridgeEvent> got;
            guest.poll(got);
            for (loom::BridgeEvent& e : got) {
                if (e.kind == loom::BridgeEvent::Kind::Delivered) {
                    answers[e.correlation] = std::move(e.payload);
                }
            }
            if (done()) return true;
            if (guest.disconnected() || std::chrono::steady_clock::now() >= deadline) return done();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    const auto ask = [&](const char* role, std::uint64_t correlation, const loom::Value& v) {
        guest.send_to_role(role, correlation, loom::serialize(v));
        guest.flush();
        return until([&] { return answers.count(correlation) != 0; });
    };
    REQUIRE(until([&] { return guest.admitted(); }));

    // THE DESK, then the pane page by page under the stamp the desk read names, each answer inside
    // one reply's bytes.
    REQUIRE(ask("zengine.workshop", 1, loom::to_value(DeskReadRequested{})));
    const loom::Admission read = loom::admit(loom::parse(answers[1]), loom::schema_of<DeskRead>());
    REQUIRE(read.ok());
    const DeskRead desk = loom::from_value<DeskRead>(read.value());
    const auto stamp = std::find_if(desk.stamps.begin(), desk.stamps.end(), [&](const PaneStamp& st) {
        return st.provider == office && st.pane == kReadPane;
    });
    REQUIRE(stamp != desk.stamps.end());
    std::vector<PaneView> pages;
    std::uint64_t correlation = 2;
    for (std::int64_t from = 0;; ++correlation) {
        REQUIRE_MESSAGE(ask("zengine.workshop", correlation,
                            loom::to_value(PaneViewRequested{office, kReadPane, from, *stamp})),
                        "the page from " << from << " never came; the connection "
                                         << (guest.disconnected() ? "ended" : "is still open"));
        const std::string& payload = answers[correlation];
        CHECK(payload.size() <= static_cast<std::size_t>(kReplyByteBudget));
        const loom::Admission page = loom::admit(loom::parse(payload), loom::schema_of<PaneView>());
        REQUIRE_MESSAGE(page.ok(), "the page from " << from << " was not a PaneView");
        pages.push_back(loom::from_value<PaneView>(page.value()));
        answers.erase(correlation);
        REQUIRE(item_count(pages.back()) > 0);
        from += item_count(pages.back());
        if (from >= pages.back().total) break;
    }
    CHECK(high_text_wrong(joined(pages)) == 0);

    // ...AND THE SAME CONNECTION ANSWERS THE NEXT ASK.
    CHECK_FALSE(guest.disconnected());
    REQUIRE(ask("zengine.workshop", correlation + 1, loom::to_value(DeskReadRequested{})));
    CHECK(loom::admit(loom::parse(answers[correlation + 1]), loom::schema_of<DeskRead>()).ok());
}

TEST_CASE("the desk's own menu opened on a window is answered as tests/session/desk_window_menu.json holds it, the menu the desk reader's glance check draws") {
    ReadRig rig;
    rig.r.extent_on_window(150, 60);
    // A SECONDARY PRESS ON THE EMPTY DESK, as the window reports it: Workshop's own menu opens
    // there, its first line on the glance row of its top edge at the window's 8 x 18 text cell.
    rig.r.publish(loom::to_value(input::PointerButton{3, true, 80, 180, input::space::kPixels,
                                                      input::mod::kNone}));
    DeskRead read;
    REQUIRE(rig.read(read).empty());
    REQUIRE(read.desk.space == input::space::kPixels);
    REQUIRE(read.desk.menu.open);
    REQUIRE(read.desk.menu.lines.size() > 1);

    std::string said = "{\n \"width\": " + std::to_string(read.desk.width) + ",\n \"height\": " +
                       std::to_string(read.desk.height) + ",\n \"space\": " +
                       std::to_string(read.desk.space) + ",\n \"menu\": ";
    json_value(loom::to_value(read.desk.menu), 1, said);
    said += "\n}\n";
    std::ifstream file(std::string(ZENGINE_SOURCE_DIR) + "/tests/session/desk_window_menu.json",
                       std::ios::binary);
    REQUIRE(file.good());
    std::string kept((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    kept.erase(std::remove(kept.begin(), kept.end(), '\r'), kept.end());
    CHECK_MESSAGE(said == kept, "tests/session/desk_window_menu.json is not the menu Workshop "
                                "answers; write Workshop's answer there:\n" << said);
}

// ============================================================================
// NOTICES: the desk says when it moved, and names each picture by what it shows
// ============================================================================

namespace {

constexpr const char* kTypingOffice = "zengine.test.desk-typing";
constexpr const char* kLeftOffice = "zengine.test.desk-left";
constexpr const char* kRightOffice = "zengine.test.desk-right";
constexpr const char* kProseOffice = "zengine.test.desk-prose";
constexpr const char* kProsePane = "board";

bool same_stamp(const PaneStamp& a, const PaneStamp& b) {
    return a.provider == b.provider && a.pane == b.pane && a.holder == b.holder &&
           a.incarnation == b.incarnation && a.epoch == b.epoch && a.fingerprint == b.fingerprint;
}

bool same_stamps(const std::vector<PaneStamp>& a, const std::vector<PaneStamp>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const PaneStamp& x, const PaneStamp& y) { return same_stamp(x, y); });
}

/// The stamp `stamps` names for `office`'s `pane`; a case requires it is there.
PaneStamp stamp_for(const std::vector<PaneStamp>& stamps, const std::string& office,
                        const std::string& pane = kReadPane) {
    for (const PaneStamp& s : stamps) {
        if (s.provider == office && s.pane == pane) return s;
    }
    FAIL("no stamp for " << office << "/" << pane);
    return {};
}

/// A LINE A PANE TYPED: `text` on its first row, and a caret after it when `caret`.
CanvasPicture typed(const std::string& text, bool caret) {
    CanvasPicture p;
    CanvasText line{0, 0, text.empty() ? std::string(" ") : text, gfx::role::kFill};
    if (caret) line.caret_col = static_cast<std::int64_t>(text.size());
    p.texts.push_back(line);
    return p;
}

/// A picture saying `word` alone.
CanvasPicture saying(const std::string& word) {
    CanvasPicture p;
    p.labels.push_back(CanvasLabel{0, 0, word, gfx::role::kFill});
    return p;
}

/// `picture`, drawn by canvas `at` from inside a delivery.
void send_from(ReadRig& d, std::size_t at, loom::Mail& m, const CanvasPicture& picture) {
    ReadCanvas& c = d.canvases.at(at);
    REQUIRE_FALSE(c.seat->rooms.empty());
    c.seat->draw(m, picture);
}

/// ...queued for the next turn, nothing drained: so two panes can draw in one turn.
void queue_draw(ReadRig& d, std::size_t at, const CanvasPicture& picture) {
    ReadCanvas& c = d.canvases.at(at);
    c.seat->next = [seat = c.seat, picture](loom::Mail& m) { seat->draw(m, picture); };
    (void)d.r.bus.send(c.id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
}

/// Two canvas panes side by side, each drawn once, and nothing holding the keys.
void two_panes(ReadRig& d, std::size_t& left, std::size_t& right) {
    left = d.open_canvas(kLeftOffice, 20, 6);
    right = d.open_canvas(kRightOffice, 20, 6);
    d.place(left, 2, 2, 20, 6);
    d.place(right, 40, 2, 20, 6);
    d.reseat();
    d.draw(left, saying("left"));
    d.draw(right, saying("right"));
    d.r.session().panes.keyboard = kNoPaneKind;
    d.r.session().panes.selected = kNoPaneKind;
}

/// A PROSE PANE, as the tower-defense game draws one: rows said with no picture number.
ProviderSeat* prose_pane(ReadRig& d) {
    ProviderSeat* seat = d.r.mount_provider(kProseOffice);
    REQUIRE(seat_pane_open(d.r, seat, kProseOffice, kProsePane) != kNoPaneKind);
    return seat;
}

void say_rows(ReadRig& d, ProviderSeat* seat, std::vector<std::string> rows) {
    SeatRows c{kProsePane, {}};
    for (std::string& row : rows) c.rows.push_back(TextRow{std::move(row)});
    d.r.drive(seat, [c](ProviderSeat& s, loom::Mail& m) { s.say(m, c); });
}

} // namespace

// THE WINDOW SAYS WHAT AN AREA SHOWS SETTLED (`AreaSettled`), whenever the pane drew there, so the
// notice follows the pane's own drawing and not Workshop's next delivery.
TEST_CASE("a keystroke gives at least one notice, the last naming the pane's final picture, with no ask between") {
    ReadRig d;
    const std::size_t at = d.open_canvas(kTypingOffice, 40, 6);
    d.place(at, 2, 2, 40, 6);
    d.reseat();
    d.draw(at, typed("", true));
    const std::int64_t kind = d.canvases[at].kind;
    d.r.session().panes.selected = kind;
    d.r.session().panes.keyboard = kind;
    // THE PANE ANSWERS A KEY AS THE TERMINAL DOES, COMPOSING TWICE: the line typed, then its caret.
    d.canvases[at].seat->on_key = [&d, at](loom::Mail& m) {
        send_from(d, at, m, typed("a", false));
        send_from(d, at, m, typed("a", true));
    };
    DeskRead before;
    REQUIRE(d.read(before).empty());
    d.ear->heard.clear();
    const std::size_t answered = d.asker->answered();
    d.r.key(input::scan::kA);
    REQUIRE_FALSE(d.ear->heard.empty());
    CHECK(d.asker->answered() == answered); // the follower asked nothing to be told
    const DeskStamps last = d.ear->heard.back();
    DeskRead after;
    REQUIRE(d.read(after).empty());
    CHECK(last.desk == after.desk.desk);
    CHECK(same_stamps(last.panes, after.stamps));
    const PaneStamp now = stamp_for(last.panes, kTypingOffice);
    CHECK(now.fingerprint != stamp_for(before.stamps, kTypingOffice).fingerprint);
    // ...THE FINAL PICTURE: read at the stamp the notice names, the pane says the line and is not in
    // flight, and stands on the pane's last number.
    PaneView reading;
    REQUIRE(d.page(PaneViewRequested{kTypingOffice, kReadPane, 0, now}, reading).empty());
    CHECK_FALSE(reading.in_flight);
    CHECK(reading.stamp.fingerprint == now.fingerprint);
    REQUIRE(reading.words.size() == 1);
    CHECK(reading.words[0].text == "a");
}

TEST_CASE("moving only the selection, closing a pane or opening arranging gives exactly one notice, with a new desk number") {
    ReadRig d;
    // WITH TITLES SHOWN, as they are by default: hidden, the keys' holder gains a title row, a room.
    d.r.session().pane_titles = true;
    std::size_t left = 0, right = 0;
    two_panes(d, left, right);
    // THE BAND'S WORDS MOVED WITH NO DELIVERY: the desk read that finds the number moved publishes
    // the notice no delivery did.
    d.ear->heard.clear();
    d.r.session().notice = "a notice said with no repaint";
    DeskRead numbered;
    REQUIRE(d.read(numbered).empty());
    REQUIRE_FALSE(d.ear->heard.empty());
    CHECK(d.ear->heard.back().desk == numbered.desk.desk);
    // ...AT EITHER VERSION'S READ.
    d.ear->heard.clear();
    d.r.session().notice = "another notice said with no repaint";
    DeskRead before;
    REQUIRE(d.read(before).empty());
    REQUIRE_FALSE(d.ear->heard.empty());
    CHECK(d.ear->heard.back().desk == before.desk.desk);
    d.ear->heard.clear();
    SUBCASE("the selection") {
        press_body(d.r, d.canvases[right].kind);
        REQUIRE(d.r.session().panes.selected == d.canvases[right].kind);
    }
    SUBCASE("closing a pane") {
        d.r.pick(PaneRef{kRightOffice, kReadPane});
        REQUIRE_FALSE(d.r.session().panes.has(d.canvases[right].kind));
    }
    SUBCASE("opening arranging") {
        d.r.key(input::scan::kW);
        REQUIRE(d.r.session().arrange.open);
    }
    REQUIRE(d.ear->heard.size() == 1);
    CHECK(d.ear->heard.front().desk > before.desk.desk);
    DeskRead after;
    REQUIRE(d.read(after).empty());
    CHECK(d.ear->heard.size() == 1);
    CHECK(d.ear->heard.front().desk == after.desk.desk);
    CHECK(same_stamps(d.ear->heard.front().panes, after.stamps));
}

TEST_CASE("two panes changing in one turn both reach the follower: the newest notice names both new pictures") {
    ReadRig d;
    std::size_t left = 0, right = 0;
    two_panes(d, left, right);
    DeskRead before;
    REQUIRE(d.read(before).empty());
    d.ear->heard.clear();
    queue_draw(d, left, saying("left again"));
    queue_draw(d, right, saying("right again"));
    d.r.bus.drain_until_idle();
    REQUIRE_FALSE(d.ear->heard.empty());
    const DeskStamps& newest = d.ear->heard.back();
    CHECK(stamp_for(newest.panes, kLeftOffice).fingerprint !=
          stamp_for(before.stamps, kLeftOffice).fingerprint);
    CHECK(stamp_for(newest.panes, kRightOffice).fingerprint !=
          stamp_for(before.stamps, kRightOffice).fingerprint);
    DeskRead after;
    REQUIRE(d.read(after).empty());
    CHECK(same_stamps(newest.panes, after.stamps));
}

TEST_CASE("a stamp names what a pane shows: the same picture drawn again keeps it and sends no notice, and every change moves it, rows included") {
    ReadRig d;
    SUBCASE("a canvas picture drawn again") {
        const std::size_t at = d.open_canvas(kTypingOffice, 30, 6);
        d.place(at, 2, 2, 30, 6);
        d.reseat();
        d.draw(at, saying("same"));
        DeskRead was;
        REQUIRE(d.read(was).empty());
        d.ear->heard.clear();
        d.draw(at, saying("same"));
        CHECK(d.ear->heard.empty());
        DeskRead again;
        REQUIRE(d.read(again).empty());
        CHECK(same_stamp(stamp_for(again.stamps, kTypingOffice), stamp_for(was.stamps, kTypingOffice)));
        d.draw(at, saying("changed"));
        REQUIRE(d.ear->heard.size() == 1);
        CHECK(stamp_for(d.ear->heard.back().panes, kTypingOffice).fingerprint !=
              stamp_for(was.stamps, kTypingOffice).fingerprint);
    }
    SUBCASE("rows, as the tower-defense game draws them") {
        ProviderSeat* seat = prose_pane(d);
        say_rows(d, seat, {"wave 1", "enemy at 3"});
        DeskRead was;
        REQUIRE(d.read(was).empty());
        d.ear->heard.clear();
        say_rows(d, seat, {"wave 1", "enemy at 3"});
        CHECK(d.ear->heard.empty());
        say_rows(d, seat, {"wave 1", "enemy at 4"});
        REQUIRE(d.ear->heard.size() == 1);
        const PaneStamp moved = stamp_for(d.ear->heard.back().panes, kProseOffice, kProsePane);
        CHECK(moved.fingerprint != stamp_for(was.stamps, kProseOffice, kProsePane).fingerprint);
    }
    SUBCASE("rows drawn at last saying nothing: the pane no longer waits for its provider") {
        ProviderSeat* seat = prose_pane(d);
        DeskRead was;
        REQUIRE(d.read(was).empty());
        const PaneStamp waiting = stamp_for(was.stamps, kProseOffice, kProsePane);
        PaneView unheard;
        REQUIRE(d.page(PaneViewRequested{kProseOffice, kProsePane, 0, waiting}, unheard).empty());
        REQUIRE(unheard.in_flight);
        d.ear->heard.clear();
        say_rows(d, seat, {});
        REQUIRE_FALSE(d.ear->heard.empty());
        const PaneStamp heard = stamp_for(d.ear->heard.back().panes, kProseOffice, kProsePane);
        CHECK(heard.fingerprint != waiting.fingerprint);
        PaneView settled;
        REQUIRE(d.page(PaneViewRequested{kProseOffice, kProsePane, 0, heard}, settled).empty());
        CHECK_FALSE(settled.in_flight);
        CHECK(settled.words.empty());
    }
}

TEST_CASE("a page under a stamp naming its picture by fingerprint is stale only when the holder, the incarnation, the room or what the pane shows moved: the same picture sent again keeps it") {
    ReadRig d;
    d.size(140, 124);
    const std::size_t at = d.open_canvas(kTypingOffice, 120, 106);
    d.place(at, 2, 2, 126, 116);
    d.reseat();
    d.draw(at, full_picture(d.room(at)));
    PaneView first;
    REQUIRE(d.page(PaneViewRequested{kTypingOffice, kReadPane, 0, {}}, first).empty());
    REQUIRE_FALSE(first.in_flight);
    const PaneStamp stamp = first.stamp;
    const std::int64_t from = first.total / 2;
    // THE SAME PICTURE AGAIN: the reading from an index of it, under its stamp, still stands.
    d.draw(at, full_picture(d.room(at)));
    PaneView next;
    CHECK(d.page(PaneViewRequested{kTypingOffice, kReadPane, from, stamp}, next).empty());
    CHECK(next.from == from);
    SUBCASE("what the pane shows moved") {
        CanvasPicture changed = full_picture(d.room(at));
        changed.labels.front().text = "other";
        d.draw(at, changed);
    }
    SUBCASE("its room moved") {
        d.place(at, 2, 2, 128, 116);
        d.reseat();
    }
    PaneView stale;
    CHECK(d.page(PaneViewRequested{kTypingOffice, kReadPane, from, stamp}, stale)
              .find("stale") != std::string::npos);
}

TEST_CASE("a canvas pane moved and drawn again the same ends on a notice naming its stamp in its new place, read with its words") {
    ReadRig d;
    const std::size_t at = d.open_canvas(kTypingOffice, 30, 6);
    d.place(at, 2, 2, 30, 6);
    d.reseat();
    d.draw(at, saying("kept"));
    DeskRead was;
    REQUIRE(d.read(was).empty());
    d.ear->heard.clear();
    // ANOTHER PLACE: the notice names the pane where it stands now.
    d.place(at, 4, 2, 30, 6);
    d.reseat();
    REQUIRE_FALSE(d.ear->heard.empty());
    CHECK(stamp_for(d.ear->heard.back().panes, kTypingOffice).fingerprint !=
          stamp_for(was.stamps, kTypingOffice).fingerprint);
    // ...AND THE PANE DRAWS THE SAME PICTURE THERE: the newest notice names that picture, settled.
    d.draw(at, saying("kept"));
    const PaneStamp settled = stamp_for(d.ear->heard.back().panes, kTypingOffice);
    CHECK(settled.fingerprint != 0);
    CHECK(settled.fingerprint != stamp_for(was.stamps, kTypingOffice).fingerprint); // another place
    PaneView reading;
    REQUIRE(d.page(PaneViewRequested{kTypingOffice, kReadPane, 0, settled}, reading).empty());
    CHECK_FALSE(reading.in_flight);
    REQUIRE(reading.words.size() == 1);
    CHECK(reading.words[0].text == "kept");
}

TEST_CASE("Layouts' stamp names what it shows: a new layout's tab and the naming line's caret each move it, and a desk seated again the same keeps it") {
    ReadRig d;
    const PaneRef layouts{kWorkshopProvider, pane_key::kLayouts};
    // TWO LAYOUTS, so the tab row is presented.
    d.r.session().setup.shelved.push_back(Layout{Setup{"second", {}}, SetupLink{}});
    d.reseat();
    DeskRead was;
    REQUIRE(d.read(was).empty());
    const PaneStamp first = stamp_for(was.stamps, layouts.provider, layouts.pane);
    REQUIRE(first.fingerprint != 0);
    // A DESK SEATED AGAIN THE SAME: Layouts says the same tabs, so its stamp holds.
    d.reseat();
    DeskRead again;
    REQUIRE(d.read(again).empty());
    CHECK(same_stamp(stamp_for(again.stamps, layouts.provider, layouts.pane), first));
    // A NEW LAYOUT: its tab is said, so a notice names Layouts' moved stamp.
    d.ear->heard.clear();
    d.r.session().setup.shelved.push_back(Layout{Setup{"third", {}}, SetupLink{}});
    d.reseat();
    REQUIRE_FALSE(d.ear->heard.empty());
    const PaneStamp tabbed = stamp_for(d.ear->heard.back().panes, layouts.provider, layouts.pane);
    CHECK(tabbed.fingerprint != first.fingerprint);
    // THE NAMING LINE'S CARET ALONE: the words hold and the stamp moves.
    open_rename_on_live_tab(d.r);
    REQUIRE(d.r.session().setup.naming.open);
    d.r.key(input::scan::kA);
    d.r.text("a");
    DeskRead typed;
    REQUIRE(d.read(typed).empty());
    const PaneStamp at_end = stamp_for(typed.stamps, layouts.provider, layouts.pane);
    PaneView before;
    REQUIRE(d.page(PaneViewRequested{layouts.provider, layouts.pane, 0, at_end}, before).empty());
    d.ear->heard.clear();
    d.r.key(input::scan::kLeft);
    REQUIRE_FALSE(d.ear->heard.empty());
    const PaneStamp moved = stamp_for(d.ear->heard.back().panes, layouts.provider, layouts.pane);
    CHECK(moved.fingerprint != at_end.fingerprint);
    PaneView after;
    REQUIRE(d.page(PaneViewRequested{layouts.provider, layouts.pane, 0, moved}, after).empty());
    REQUIRE(after.words.size() == before.words.size());
    for (std::size_t i = 0; i < after.words.size(); ++i) CHECK(after.words[i].text == before.words[i].text);
    d.r.key(input::scan::kEscape);
}
