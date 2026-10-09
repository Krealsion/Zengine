// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop desk suite, the desk said whole: one reading filled to the budget a reader's
// decoder spends, the panes past it named by their stamps and read alone a page at a time, and
// which participants Workshop answers.

#include "doctest.h"
#include "workshop_support.hpp"
#include "workshop/decoded_cells.hpp"

#include <zen/serialize.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
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
                             loom::Accept<SeatDo, DeskRead, v4::PaneView, loom::Refused>,
                             loom::Emit<DeskReadRequested, v4::PaneViewRequested>> {
public:
    std::function<void(loom::Mail&)> next;
    std::vector<DeskRead> reads;
    std::vector<v4::PaneView> pages;
    std::vector<std::string> refusals;
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
    void on(const DeskRead& d, loom::Mail&) { reads.push_back(d); }
    void on(const v4::PaneView& v, loom::Mail&) { pages.push_back(v); }
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); }
};

/// A PROVIDER THAT DRAWS A PICTURE, keeping each room Workshop grants it and each picture refused.
class ReadCanvasSeat
    : public loom::WeaveBase<ReadCanvasSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PaneCanvasRoom,
                                          PaneCanvasPointer, PaneCanvasHover, PaneCanvasRejected,
                                          SeatDo>,
                             loom::Emit<v3::PaneOffered, v4::PaneCanvasContent>> {
public:
    std::vector<PaneCanvasRoom> rooms;
    std::vector<PaneCanvasRejected> rejected;
    std::function<void(loom::Mail&)> next;
    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
    void on(const PaneCanvasRoom& room, loom::Mail&) { rooms.push_back(room); }
    void on(const PaneCanvasPointer&, loom::Mail&) {}
    void on(const PaneCanvasHover&, loom::Mail&) {}
    void on(const PaneCanvasRejected& refused, loom::Mail&) { rejected.push_back(refused); }
    void on(const SeatDo&, loom::Mail& m) {
        auto run = std::move(next);
        next = {};
        if (run) run(m);
    }
};

/// ONE CANVAS PANE ON THE DESK: its seat, its office and kind, and the number of its last picture.
struct ReadCanvas {
    ReadCanvasSeat* seat = nullptr;
    loom::WeaveId id{};
    std::string office;
    std::int64_t kind = kNoPaneKind;
    std::int64_t number = 0;
};

/// A FRESH DESK WITH NO PANE OF A CASE'S OWN YET, the asker beside it, and the canvas panes a case
/// opens on it, each in an office of its own.
struct ReadRig {
    PaneRig r;
    ReadAsker* asker = nullptr;
    loom::WeaveId asker_id{};
    std::vector<ReadCanvas> canvases;

    ReadRig() {
        r.mount_workshop();
        r.ready();
        r.extent(150, 60);
        auto made = std::make_unique<ReadAsker>();
        asker = made.get();
        loom::Grant grant;
        for (const auto& shape :
             {loom::schema_of<DeskReadRequested>(), loom::schema_of<v4::PaneViewRequested>()}) {
            grant.allow_to_role(shape->name(), shape->version(), kWorkshopProvider);
        }
        asker_id = r.bus.register_weave(std::move(made), std::move(grant));
        asker->zen_set_self(asker_id);
    }

    void drive(std::size_t at, std::function<void(loom::Mail&)> what) {
        ReadCanvas& c = canvases.at(at);
        c.seat->next = std::move(what);
        (void)r.bus.send(c.id, loom::Message(loom::to_value(SeatDo{}), {}, {}, 0));
        r.bus.drain_until_idle();
    }

    /// A CANVAS PANE OFFERING A BODY `columns` BY `rows` CELLS, opened on the desk.
    std::size_t open_canvas(const std::string& office, std::int64_t columns, std::int64_t rows) {
        auto made = std::make_unique<ReadCanvasSeat>();
        ReadCanvas c;
        c.seat = made.get();
        c.office = office;
        loom::Grant grant;
        grant.allow_to_any(v3::PaneOffered::zen_name, v3::PaneOffered::zen_version);
        grant.allow_to_any(v4::PaneCanvasContent::zen_name, v4::PaneCanvasContent::zen_version);
        c.id = r.bus.register_weave(std::move(made), std::move(grant), office);
        c.seat->zen_set_self(c.id);
        canvases.push_back(c);
        const std::size_t at = canvases.size() - 1;
        drive(at, [office, columns, rows](loom::Mail& m) {
            (void)m.as_role(office).send_to_role(
                kWorkshopProvider, v3::PaneOffered{kReadPane, "Page", "a picture read whole",
                                                   columns * kPaneCanvasUnit,
                                                   rows * kPaneCanvasUnit, 0});
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
            p.place = {pane_unit::kPixels, x * surface::kCanvasCellPx, y * surface::kCanvasCellPx};
            p.width = {pane_unit::kPixels, w * surface::kCanvasCellPx};
            p.height = {pane_unit::kPixels, h * surface::kCanvasCellPx};
            ++placed;
        }
        REQUIRE(placed == 1);
    }

    /// The desk seated again where its panes are authored: a same-size extent reseats nothing.
    void reseat() {
        r.extent(149, 60);
        r.extent(150, 60);
    }

    /// `picture`, drawn for the room last granted under the pane's next number, and admitted.
    void draw(std::size_t at, v4::PaneCanvasContent picture) {
        ReadCanvas& c = canvases.at(at);
        REQUIRE_FALSE(c.seat->rooms.empty());
        picture.pane = kReadPane;
        picture.grant = c.seat->rooms.back().grant;
        picture.picture = ++c.number;
        const std::size_t refused = c.seat->rejected.size();
        drive(at, [office = c.office, picture](loom::Mail& m) {
            (void)m.as_role(office).send_to_role(kWorkshopProvider, picture);
        });
        const std::string why =
            c.seat->rejected.size() > refused ? c.seat->rejected.back().reason : std::string();
        REQUIRE_MESSAGE(why.empty(), why);
        REQUIRE(r.session().panes.external_pane(c.kind)->canvas.heard);
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
    std::string page(const v4::PaneViewRequested& asked, v4::PaneView& out) {
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
std::int64_t item_count(const v4::PaneView& v) {
    return static_cast<std::int64_t>(v.words.size() + v.parts.size());
}

bool same_pane(const PaneStamp& s, const v4::PaneView& v) {
    return s.provider == v.provider && s.pane == v.pane;
}

/// Whether `v` is read from the picture `s` names: its holder, incarnation, room and number.
bool stands_on(const v4::PaneView& v, const PaneStamp& s) {
    return same_pane(s, v) && s.holder == v.holder && s.incarnation == v.incarnation &&
           s.grant == v.grant && s.picture == v.picture;
}

PaneStamp view_stamp(const v4::PaneView& v) {
    return PaneStamp{v.provider, v.pane, v.holder, v.incarnation, v.grant, v.picture};
}

/// The items of `pages`, in order, as one reading from its first item.
v4::PaneView joined(const std::vector<v4::PaneView>& pages) {
    REQUIRE_FALSE(pages.empty());
    v4::PaneView whole = pages.front();
    for (std::size_t i = 1; i < pages.size(); ++i) {
        whole.words.insert(whole.words.end(), pages[i].words.begin(), pages[i].words.end());
        whole.parts.insert(whole.parts.end(), pages[i].parts.begin(), pages[i].parts.end());
    }
    return whole;
}

/// ONE PANE READ ALONE: page after page under `stamp`, each from where the last ended, until the
/// pages hold every item; the refusal's reason when a page is refused.
std::string read_alone(ReadRig& d, const PaneStamp& stamp, std::vector<v4::PaneView>& pages) {
    pages.clear();
    std::int64_t from = 0;
    for (;;) {
        v4::PaneView one;
        const std::string why =
            d.page(v4::PaneViewRequested{stamp.provider, stamp.pane, from, stamp}, one);
        if (!why.empty()) return why;
        pages.push_back(one);
        const std::int64_t got = item_count(one);
        from += got;
        if (got == 0 || from >= one.total) return std::string();
    }
}

const DeskPane* desk_pane(const v3::DeskView& desk, const std::string& provider,
                          const std::string& pane) {
    for (const DeskPane& p : desk.panes) {
        if (p.provider == provider && p.pane == pane) return &p;
    }
    return nullptr;
}

std::string label_text(std::size_t pane, std::size_t i) {
    return "c" + std::to_string(pane) + "-" + std::to_string(i);
}

/// AS MANY LABELS AS A PICTURE MAY DRAW, on four rows from the body's corner, each its own word.
v4::PaneCanvasContent labelled(std::size_t pane) {
    v4::PaneCanvasContent p;
    for (std::size_t i = 0; i < kPaneCanvasMaxLabels; ++i) {
        p.labels.push_back(PaneCanvasLabel{0, static_cast<std::int64_t>(i % 4) * kPaneCanvasUnit,
                                           label_text(pane, i), surface::role::kFill});
    }
    return p;
}

/// The word a full picture draws `i`th: its labels' and then its runs'.
std::string full_word(std::size_t i) {
    return i < kPaneCanvasMaxLabels ? "l" + std::to_string(i)
                                    : "t" + std::to_string(i - kPaneCanvasMaxLabels);
}

/// A PICTURE AT EVERY CANVAS BUDGET: each label it may draw on four rows, each run of text it may
/// set on the four below, and each part it may name, one cell each, on eight rows of their own
/// beneath, so no part holds a word.
v4::PaneCanvasContent full_picture() {
    v4::PaneCanvasContent p;
    for (std::size_t i = 0; i < kPaneCanvasMaxLabels; ++i) {
        p.labels.push_back(PaneCanvasLabel{0, static_cast<std::int64_t>(i % 4) * kPaneCanvasUnit,
                                           full_word(i), surface::role::kFill});
    }
    for (std::size_t i = 0; i < kPaneCanvasMaxTexts; ++i) {
        p.texts.push_back(PaneCanvasText{0, static_cast<std::int64_t>(4 + i % 4) * kPaneCanvasUnit,
                                         full_word(kPaneCanvasMaxLabels + i), surface::role::kFill});
    }
    for (std::size_t i = 0; i < kMaxPaneParts; ++i) {
        const auto column = static_cast<std::int64_t>(i % 32);
        const auto row = static_cast<std::int64_t>(9 + (i / 32) % 8);
        p.parts.push_back(PaneCanvasPart{"p" + std::to_string(i), column * kPaneCanvasUnit,
                                         row * kPaneCanvasUnit, kPaneCanvasUnit, kPaneCanvasUnit});
    }
    return p;
}

} // namespace

TEST_CASE("the decode budget a reading is filled to is Loom's own: a value of exactly that many cells decodes, and one cell more is refused") {
    // A DESK READ OF ONE PANE'S READING: its words, and then its cover's names, a cell each, to the
    // budget's last cell.
    DeskRead full;
    full.stamps.push_back(PaneStamp{"zengine.test.read", kReadPane, 7, 1, 3, 9});
    full.panes.emplace_back();
    v4::PaneView& pane = full.panes.back();
    pane.provider = "zengine.test.read";
    pane.pane = kReadPane;
    pane.picture = 9;
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
    // FOUR CANVAS PANES SIDE BY SIDE, none over another, each drawing as many labels as a picture
    // may: their readings together are more than one decoded value holds.
    constexpr std::size_t kPanes = 4;
    for (std::size_t k = 0; k < kPanes; ++k) {
        (void)d.open_canvas("zengine.test.read-" + std::to_string(k), 30, 8);
    }
    for (std::size_t k = 0; k < kPanes; ++k) {
        d.place(k, 2 + 36 * static_cast<std::int64_t>(k), 4, 32, 12);
    }
    d.reseat();
    for (std::size_t k = 0; k < kPanes; ++k) {
        d.draw(k, labelled(k));
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
        CHECK(s.grant == c.seat->rooms.back().grant);
        CHECK(s.picture == c.number);
    }

    // THE READINGS ARE THE STAMPS' OWN, IN THE STAMPS' ORDER, each whole; a stamp with no reading
    // before the last one names a pane with nothing to read.
    std::size_t at = 0;
    for (const v4::PaneView& v : desk_read.panes) {
        while (at < desk_read.stamps.size() && !same_pane(desk_read.stamps[at], v)) ++at;
        REQUIRE_MESSAGE(at < desk_read.stamps.size(),
                        v.provider << "/" << v.pane << " is read out of its stamps' order");
        CHECK(stands_on(v, desk_read.stamps[at]));
        CHECK(v.from == 0);
        CHECK(item_count(v) == v.total);
        ++at;
    }
    REQUIRE_MESSAGE(at < desk_read.stamps.size(), "every pane was read in the one value");

    // EVERY PANE PAST THE LAST READING IS READ ALONE under the stamp the desk named, and the first of
    // them that reads is one the value had no room left for.
    std::vector<v4::PaneView> wholes = desk_read.panes;
    std::size_t alone = 0;
    for (std::size_t i = at; i < desk_read.stamps.size(); ++i) {
        std::vector<v4::PaneView> pages;
        if (!read_alone(d, desk_read.stamps[i], pages).empty()) continue;
        const v4::PaneView whole = joined(pages);
        CHECK(stands_on(whole, desk_read.stamps[i]));
        CHECK(item_count(whole) == whole.total);
        if (alone == 0) {
            const std::int64_t with = cells + 1 + decoded_cells(loom::to_value(whole));
            CHECK_MESSAGE(with > kDecodedCellBudget,
                          whole.provider << "/" << whole.pane << " would have fit: " << with << " cells");
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
                                     [&office](const v4::PaneView& v) { return v.provider == office; });
        REQUIRE_MESSAGE(it != wholes.end(), office << " was read neither in the desk read nor alone");
        CHECK_FALSE(it->in_flight);
        CHECK_MESSAGE(it->covered.words == 0, it->covered.words << " words covered");
        REQUIRE(it->words.size() == kPaneCanvasMaxLabels);
        std::size_t wrong = 0;
        for (std::size_t i = 0; i < it->words.size(); ++i) {
            if (it->words[i].text != label_text(k, i)) ++wrong;
        }
        CHECK_MESSAGE(wrong == 0, wrong << " words are not the labels drawn, in their order");
    }
}

TEST_CASE("a pane at its full canvas budgets reads whole, paged by index under one stamp, and a page asked after its picture moved is refused as stale") {
    ReadRig d;
    const std::string office = "zengine.test.read-full";
    (void)d.open_canvas(office, 64, 20);
    d.place(0, 2, 2, 70, 24);
    d.reseat();
    // ROOM FOR THE WHOLE PICTURE: eight rows of words over eight rows of parts, thirty-two cells wide.
    const PaneCanvasRoom room = d.canvases[0].seat->rooms.back();
    REQUIRE(room.width >= 32 * kPaneCanvasUnit);
    REQUIRE(room.height >= 17 * kPaneCanvasUnit);
    d.draw(0, full_picture());
    const auto drawn = static_cast<std::int64_t>(kPaneCanvasMaxLabels + kPaneCanvasMaxTexts + kMaxPaneParts);

    // THE FIRST PAGE, asked under no stamp: the reading as it stands, the stamp it stands on, and not
    // the whole of it.
    v4::PaneView first;
    const std::string unread = d.page(v4::PaneViewRequested{office, kReadPane, 0, PaneStamp{}}, first);
    REQUIRE_MESSAGE(unread.empty(), unread);
    REQUIRE_FALSE(first.in_flight);
    CHECK_MESSAGE(first.covered.words == 0, first.covered.words << " words covered");
    REQUIRE(first.total == drawn);
    CHECK(first.from == 0);
    CHECK(first.picture == d.canvases[0].number);
    REQUIRE(item_count(first) > 0);
    CHECK(item_count(first) < first.total);
    const PaneStamp stamp = view_stamp(first);

    // EACH PAGE AFTER IT, from where the last ended and under the first page's stamp, until the
    // pages hold every item.
    std::vector<v4::PaneView> pages{first};
    std::int64_t from = item_count(first);
    while (from < first.total) {
        v4::PaneView next;
        const std::string why = d.page(v4::PaneViewRequested{office, kReadPane, from, stamp}, next);
        REQUIRE_MESSAGE(why.empty(), why);
        REQUIRE(item_count(next) > 0);
        CHECK(next.from == from);
        CHECK(next.total == first.total);
        CHECK(stands_on(next, stamp));
        from += item_count(next);
        pages.push_back(next);
    }
    CHECK(from == first.total);

    // EACH PAGE IS ONE DECODED VALUE, and the first is filled to the budget: the item after it
    // would not have fit beside the rest.
    for (const v4::PaneView& p : pages) {
        const std::int64_t spent = decoded_cells(loom::to_value(p));
        CHECK_MESSAGE(spent <= kDecodedCellBudget, "the page from " << p.from << " is " << spent << " cells");
    }
    REQUIRE(pages.size() >= 2);
    const v4::PaneView& second = pages[1];
    const loom::Value after = second.words.empty() ? loom::to_value(second.parts.front())
                                                   : loom::to_value(second.words.front());
    CHECK(decoded_cells(loom::to_value(first)) + 1 + decoded_cells(after) > kDecodedCellBudget);

    // TOGETHER THE PAGES ARE THE WHOLE, in order: the labels' words, the runs', then every part by
    // its name, none holding a word.
    const v4::PaneView whole = joined(pages);
    REQUIRE(whole.words.size() == kPaneCanvasMaxLabels + kPaneCanvasMaxTexts);
    REQUIRE(whole.parts.size() == kMaxPaneParts);
    std::size_t wrong = 0;
    for (std::size_t i = 0; i < whole.words.size(); ++i) {
        if (whole.words[i].text != full_word(i)) ++wrong;
    }
    for (std::size_t i = 0; i < whole.parts.size(); ++i) {
        if (whole.parts[i].name != "p" + std::to_string(i) || !whole.parts[i].text.empty()) ++wrong;
    }
    CHECK_MESSAGE(wrong == 0, wrong << " items are not the ones drawn, in their order");

    // THE PANE DRAWS AGAIN: the page after the first, asked under the stamp it moved off, is refused
    // as stale...
    d.draw(0, full_picture());
    v4::PaneView late;
    const std::string stale =
        d.page(v4::PaneViewRequested{office, kReadPane, item_count(first), stamp}, late);
    CHECK_MESSAGE(stale.find("stale") != std::string::npos, "the late page was answered: [" << stale << "]");
    // ...AND READ AGAIN FROM THE START, the pane stands on its new picture.
    v4::PaneView again;
    REQUIRE(d.page(v4::PaneViewRequested{office, kReadPane, 0, PaneStamp{}}, again).empty());
    CHECK(again.picture == d.canvases[0].number);
    CHECK(again.picture != stamp.picture);
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
                             loom::Accept<SeatDo, DeskRead, v4::PaneView, PaneInventory, KeymapShown,
                                          loom::Refused, loom::DispatchRefused>,
                             loom::Emit<DeskReadRequested, v4::PaneViewRequested,
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
    void on(const v4::PaneView&, loom::Mail& m) { keep(m, "PaneView v4"); }
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
         {loom::schema_of<PaneViewRequested>(), loom::schema_of<v2::PaneViewRequested>(),
          loom::schema_of<v3::PaneViewRequested>(), loom::schema_of<v4::PaneViewRequested>(),
          loom::schema_of<PanePointRequested>(), loom::schema_of<v2::PanePointRequested>(),
          loom::schema_of<v3::PanePointRequested>(), loom::schema_of<DeskViewRequested>(),
          loom::schema_of<v2::DeskViewRequested>(), loom::schema_of<DeskReadRequested>(),
          loom::schema_of<PaneInventoryRequested>(), loom::schema_of<KeymapRequested>()}) {
        grant.allow_to_role(shape->name(), shape->version(), kWorkshopProvider);
    }
    grant.allow_to_role(surface::SurfaceCaptureRequested::zen_name,
                        surface::SurfaceCaptureRequested::zen_version, surface::kSkinRole);
    grant.allow_to_role(surface::SurfaceCaptureChunkRequested::zen_name,
                        surface::SurfaceCaptureChunkRequested::zen_version, surface::kSkinRole);
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
    v4::PaneCanvasContent picture;
    picture.labels.push_back(PaneCanvasLabel{0, 0, "guest words", surface::role::kFill});
    d.draw(0, picture);
    const auto ask_all = [office](loom::Mail& m) {
        (void)m.send_to_role(kWorkshopProvider, DeskReadRequested{});
        (void)m.send_to_role(kWorkshopProvider, v4::PaneViewRequested{office, kReadPane, 0, PaneStamp{}});
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
    for (const char* shape : {"DeskRead", "PaneView v4", "PaneInventory", "KeymapShown"}) {
        CAPTURE(shape);
        CHECK(count_of(guest->answered, shape) == 1);
        CHECK(count_of(bystander->answered, shape) == 0);
    }
    CHECK_MESSAGE(guest->refusals.empty(), (guest->refusals.empty() ? std::string() : guest->refusals.front()));
    CHECK(guest->denied.empty());
    // ...AND THE DESK AND THE PAGE ARE SAID TO NO ONE ELSE, the canvas pane named among the desk's.
    CHECK(count_of(bystander->heard, "DeskRead") == 0);
    CHECK(count_of(bystander->heard, "PaneView v4") == 0);
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
    for (const char* shape : {DeskReadRequested::zen_name, v4::PaneViewRequested::zen_name,
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
