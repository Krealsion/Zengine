// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- what Workshop said, kept and shown: Workshop's own publication of
// each notice, the real `zengine-notices` record keeping it to its bound and answering pages, and
// the real `zengine-notices-pane` walking it, a key and a wheel's notch one action. Workshop is
// driven by real keys; a reader weave on the bus hears what is published and asks the record as
// any weave would.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"
#include "census.hpp"

#include "notices/notices-pane/vocabulary.hpp"
#include "notices/vocabulary.hpp"
#include "workshop/notice_seam_vocabulary.hpp"

#include <functional>

namespace {

namespace nt = zengine::notices;
namespace np = zengine::notices_pane;

struct ReaderState {
    ZEN_SHAPE(ReaderState, 1);
};
struct ReaderDo {
    ZEN_SHAPE(ReaderDo, 1);
};

/// A WEAVE THAT READS WHAT WORKSHOP SAID AS ANY OTHER WOULD: it hears each `NoticeSaid` and
/// `NoticesMoved` published, asks the record for pages and the layouts' owner for the record's
/// setting, and keeps every answer; as its own office it can also say a notice a stranger says.
class NoticesReader
    : public loom::WeaveBase<
          NoticesReader, ReaderState,
          loom::Accept<NoticeSaid, nt::NoticesMoved, nt::NoticesShown, OfficeSettingsAnswered,
                       OfficeSettingCommitAnswered, ReaderDo>,
          loom::Emit<NoticeSaid, nt::NoticesRequested, OfficeSettingsRequested,
                     OfficeSettingCommitRequested>> {
public:
    void on(const NoticeSaid& said, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshopProvider)) {
            heard.push_back(said);
        }
    }
    void on(const nt::NoticesMoved& moved, loom::Mail& mail) {
        if (mail.authored_from_role(nt::kNoticesRole)) {
            moves.push_back(moved);
        }
    }
    void on(const nt::NoticesShown& page, loom::Mail& mail) {
        if (mail.answers_ask()) {
            pages.push_back(page);
        }
    }
    void on(const OfficeSettingsAnswered& shown, loom::Mail& mail) {
        if (mail.answers_ask()) {
            settings.push_back(shown);
        }
    }
    void on(const OfficeSettingCommitAnswered& acted, loom::Mail& mail) {
        if (mail.answers_ask()) {
            commits.push_back(acted);
        }
    }
    void on(const ReaderDo&, loom::Mail& mail) {
        std::function<void(loom::Mail&)> once;
        once.swap(next);
        if (once) {
            once(mail);
        }
    }

    std::function<void(loom::Mail&)> next;
    std::vector<NoticeSaid> heard;
    std::vector<nt::NoticesMoved> moves;
    std::vector<nt::NoticesShown> pages;
    std::vector<OfficeSettingsAnswered> settings;
    std::vector<OfficeSettingCommitAnswered> commits;
};

constexpr const char* kReaderOffice = "test.notices-reader";

inline PaneRef notices_ref() { return PaneRef{np::kNoticesPaneRole, np::kNoticesPane}; }

/// A LIVE WORKSHOP WITH THE RECORD AND THE PANE LOADED, AND A READER ON THE BUS.
struct NoticesRig {
    PaneRig r;
    NoticesReader* reader = nullptr;
    loom::WeaveId reader_id;

    /// The host's own note for this launch, said with the launch's notes once the desk is up.
    static constexpr const char* kLaunchNote = "the launch's own note";

    void open(std::int64_t width = 160, std::int64_t height = 48) {
        auto ear = std::make_unique<NoticesReader>();
        reader = ear.get();
        loom::Grant grant;
        grant.allow_to_any(NoticeSaid::zen_name, NoticeSaid::zen_version);
        grant.allow_to_any(nt::NoticesRequested::zen_name, nt::NoticesRequested::zen_version);
        grant.allow_to_any(OfficeSettingsRequested::zen_name, OfficeSettingsRequested::zen_version);
        grant.allow_to_any(OfficeSettingCommitRequested::zen_name,
                           OfficeSettingCommitRequested::zen_version);
        reader_id = r.bus.register_weave(std::move(ear), std::move(grant), kReaderOffice);
        reader->zen_set_self(reader_id);
        r.host.transition_note = kLaunchNote;
        r.mount_workshop();
        load::LoadPlan plan;
        for (const auto& [stem, role] :
             {std::pair<const char*, const char*>{nt::kNoticesStem, nt::kNoticesRole},
              {np::kNoticesPaneStem, np::kNoticesPaneRole}}) {
            load::ArtifactIntent row;
            row.stem = stem;
            row.weave = load::WeaveIntent{role};
            plan.artifacts.push_back(row);
        }
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
    }

    /// ONE THING DONE INSIDE THE READER'S OWN DELIVERY, then the bus settled.
    void drive(std::function<void(loom::Mail&)> what) {
        reader->next = std::move(what);
        (void)r.bus.send(reader_id, loom::Message(loom::to_value(ReaderDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// A PAGE OF THE RECORD, asked as any weave asks it.
    nt::NoticesShown page(std::int64_t newest = 0, std::int64_t count = nt::kMaxNoticesPage) {
        const std::size_t before = reader->pages.size();
        drive([newest, count](loom::Mail& m) {
            (void)m.as_role(kReaderOffice)
                .send_to_role(nt::kNoticesRole, nt::NoticesRequested{newest, count}, 7);
        });
        REQUIRE(reader->pages.size() == before + 1);
        return reader->pages.back();
    }

    /// THE RECORD'S `keep` WRITTEN, at the revision read, as an office writes another's setting.
    OfficeSettingCommitAnswered keep(std::int64_t n) {
        const std::size_t asked = reader->settings.size();
        drive([](loom::Mail& m) {
            (void)m.as_role(kReaderOffice)
                .send_to_role(kLayoutsRole, OfficeSettingsRequested{nt::kNoticesRole}, 8);
        });
        REQUIRE(reader->settings.size() == asked + 1);
        OfficeSettingCommitRequested commit;
        commit.office = nt::kNoticesRole;
        commit.revision = reader->settings.back().revision;
        commit.key = nt::kSettingKeep;
        commit.number = n;
        const std::size_t before = reader->commits.size();
        drive([commit](loom::Mail& m) {
            (void)m.as_role(kReaderOffice).send_to_role(kLayoutsRole, commit, 9);
        });
        REQUIRE(reader->commits.size() == before + 1);
        return reader->commits.back();
    }

    /// WORKSHOP SAYS A STATEMENT: the titles toggle, a key a weaver presses.
    void statement() {
        r.key(input::scan::kT);
        r.text("t");
    }
    /// ...AND A REFUSAL: the only layout cannot be removed.
    void refusal() { r.key(input::scan::kW, input::mod::kCtrl); }

    /// Workshop said at least `n` more things than the record held as newest.
    void say_until(std::int64_t newest) {
        for (int i = 0; i < 4 * static_cast<int>(newest) && page(0, 1).last < newest; ++i) {
            statement();
        }
        REQUIRE(page(0, 1).last >= newest);
    }

    const RuntimePane* row() {
        return r.session().panes.runtime.find(np::kNoticesPaneRole, np::kNoticesPane);
    }
    std::int64_t kind() {
        REQUIRE(row() != nullptr);
        return row()->kind;
    }
    std::vector<std::string> shown() { return pane_rows(r, kind()); }
    /// The pane's last row that is a notice's own, its markers aside.
    std::string bottom_notice() {
        const std::vector<std::string> rows = shown();
        for (std::size_t i = rows.size(); i-- > 0;) {
            if (rows[i].rfind("  ... ", 0) != 0 && !rows[i].empty()) {
                return rows[i];
            }
        }
        return std::string();
    }
    bool shows(const std::string& text) {
        for (const std::string& row_text : shown()) {
            if (row_text == text) {
                return true;
            }
        }
        return false;
    }
    /// PRESS INTO THE PANE: its keys reach it only while it holds the keyboard.
    void focus() {
        const CellRect body = external_body_rect(r.session(), kind());
        r.press_cell(body.x, body.y);
        REQUIRE(r.session().panes.keyboard == kind());
    }
    /// Hand the keys back the way a weaver does -- a press on the bare workspace.
    void unfocus() {
        r.press_cell(0, gfx::cell_of_pixel(screen_of(r.session()).h) - 1);
        REQUIRE(r.session().panes.keyboard != kind());
    }
    /// The pane's room set to `rows` by `columns` of text, as a weaver sizes it.
    void give_room(std::int64_t rows, std::int64_t columns) {
        const Written wrote =
            staged_author_pane_size(r, notices_ref(), PaneSize{pane_unit::kPixels, cells_px(columns + 2)},
                                    PaneSize{pane_unit::kPixels, cells_px(rows + 3)});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        focus();
        const ExternalPane* seat = r.session().panes.external_pane(kind());
        REQUIRE(seat != nullptr);
        REQUIRE(held_canvas_rows(*seat).rows == rows);
    }
};

/// THE NOTICES PANE'S CENSUS: its rows in the one mode it has, which it declares by no name.
const CensusDriverOf kNoticesCensus{np::kNoticesPaneRole, [] {
    OfficeCensus census;
    NoticesRig t;
    t.open();
    const RuntimePane* held = t.row();
    REQUIRE(held != nullptr);
    CHECK(held->mode.empty());
    census_take(census, np::kNoticesPaneRole, *held);
    return census;
}};

/// THE NOTICES PANE'S PRESSES: each of its rows, with older and newer notices to move over.
const PressDriverOf kNoticesPresses{np::kNoticesPaneRole, [](PressWitness& w) {
    press_each_state<NoticesRig>(
        w,
        [] {
            auto t = std::make_unique<NoticesRig>();
            t->open();
            t->say_until(8);
            t->give_room(4, 70);
            t->r.key(input::scan::kUp);
            return t;
        },
        [](NoticesRig& t, const std::function<void(const RuntimePane&)>& at) { at(*t.row()); });
}};

} // namespace

TEST_CASE("each thing Workshop says is published once, numbered from one, a refusal told apart") {
    NoticesRig t;
    t.open();
    // THE LAUNCH'S OWN NOTES ARE THE FIRST THINGS SAID, and every number follows the one before.
    REQUIRE_FALSE(t.reader->heard.empty());
    for (std::size_t i = 0; i < t.reader->heard.size(); ++i) {
        CHECK(t.reader->heard[i].number == static_cast<std::int64_t>(i) + 1);
    }
    bool launch = false;
    for (const NoticeSaid& said : t.reader->heard) {
        launch = launch || said.text.find(NoticesRig::kLaunchNote) != std::string::npos;
    }
    CHECK(launch);
    // A STATEMENT, published once, the words `Session::notice` holds.
    const std::size_t before = t.reader->heard.size();
    t.statement();
    REQUIRE(t.reader->heard.size() > before);
    CHECK(t.reader->heard[before].number == static_cast<std::int64_t>(before) + 1);
    CHECK(t.reader->heard.back().text == t.r.session().notice);
    CHECK_FALSE(t.reader->heard[before].refusal);
    // ...AND A REFUSAL, said as one.
    t.refusal();
    REQUIRE(t.r.session().notice_is_bad);
    CHECK(t.reader->heard.back().refusal);
    CHECK(t.reader->heard.back().text == t.r.session().notice);
    CHECK(t.reader->heard.back().number == static_cast<std::int64_t>(t.reader->heard.size()));
}

TEST_CASE("the record keeps what Workshop said, the oldest let go past keep, and a lower keep "
          "lets go at once") {
    NoticesRig t;
    t.open();
    nt::NoticesShown all = t.page();
    REQUIRE_FALSE(all.rows.empty());
    CHECK(all.first == 1);
    CHECK(all.keep == nt::kDefaultKept);
    CHECK(all.rows.back().text == t.r.session().notice);
    // PAST ITS DEFAULT KEEP, THE OLDEST GO, counted.
    t.say_until(nt::kDefaultKept + 20);
    all = t.page();
    CHECK(static_cast<std::int64_t>(all.rows.size()) == nt::kDefaultKept);
    CHECK(all.last - all.first + 1 == nt::kDefaultKept);
    CHECK(all.let_go == all.first - 1);
    CHECK(all.rows.back().text == t.r.session().notice);
    // A SETTING RAISES IT: more are kept from then on.
    const OfficeSettingCommitAnswered raised = t.keep(150);
    REQUIRE_MESSAGE(raised.accepted, raised.refusal);
    CHECK(t.page().keep == 150);
    t.say_until(all.last + 80);
    all = t.page();
    CHECK(static_cast<std::int64_t>(all.rows.size()) == 150);
    // ...A VALUE IT DOES NOT TAKE IS REFUSED, and nothing moves.
    const OfficeSettingCommitAnswered low = t.keep(nt::kDefaultKept - 1);
    CHECK_FALSE(low.accepted);
    CHECK(static_cast<std::int64_t>(t.page().rows.size()) == 150);
    // ...AND A LOWER ONE LETS GO AT ONCE, each counted -- the fifty past it, and one for each
    // thing Workshop said of the write since.
    const nt::NoticesShown before = t.page();
    const OfficeSettingCommitAnswered lowered = t.keep(nt::kDefaultKept);
    REQUIRE_MESSAGE(lowered.accepted, lowered.refusal);
    const nt::NoticesShown after = t.page();
    CHECK(static_cast<std::int64_t>(after.rows.size()) == nt::kDefaultKept);
    CHECK(after.let_go == before.let_go + 50 + (after.last - before.last));
    CHECK(after.last - after.first + 1 == nt::kDefaultKept);
}

TEST_CASE("the record keeps nothing a stranger says, cuts a long notice and keeps what it holds "
          "across a reload") {
    TempDir copy("notices-reload");
    const std::string image = copy.file(
        ("zengine-notices-again" + std::filesystem::path(WORKSHOP_SO_NOTICES).extension().string())
            .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_NOTICES, image);
    NoticesRig t;
    t.open();
    const nt::NoticesShown before = t.page();
    // A STRANGER'S NOTICE IS NO NOTICE OF WORKSHOP'S.
    const std::int64_t next = before.last + 1;
    t.drive([next](loom::Mail& m) {
        (void)m.as_role(kReaderOffice).publish(NoticeSaid{next, "not Workshop's", false});
    });
    const nt::NoticesShown stranger = t.page();
    CHECK(stranger.last == before.last);
    CHECK(stranger.rows.back().text != "not Workshop's");
    // A NOTICE PAST `kMaxNoticeBytes` IS KEPT CUT THERE, ending `...`.
    t.r.host.setup_path = std::string(2 * nt::kMaxNoticeBytes, 'x') + ".json";
    t.r.key(input::scan::kR);
    REQUIRE(t.r.session().notice.size() > nt::kMaxNoticeBytes);
    const nt::NoticesShown cut = t.page();
    CHECK(cut.rows.back().text.size() == nt::kMaxNoticeBytes);
    CHECK(cut.rows.back().text.compare(nt::kMaxNoticeBytes - 3, 3, "...") == 0);
    CHECK(t.r.session().notice.compare(0, nt::kMaxNoticeBytes - 3, cut.rows.back().text, 0,
                                       nt::kMaxNoticeBytes - 3) == 0);
    // A RELOAD IN PLACE KEEPS THE RECORD, and it is said to have moved.
    const std::size_t moves = t.reader->moves.size();
    t.r.enqueue_reload(nt::kNoticesStem, image);
    t.r.bus.drain_until_idle();
    REQUIRE(t.r.load_refusals.empty());
    CHECK(t.reader->moves.size() > moves);
    const nt::NoticesShown kept = t.page();
    REQUIRE(kept.rows.size() == cut.rows.size());
    for (std::size_t i = 0; i < kept.rows.size(); ++i) {
        CHECK(kept.rows[i].number == cut.rows[i].number);
        CHECK(kept.rows[i].text == cut.rows[i].text);
        CHECK(kept.rows[i].refusal == cut.rows[i].refusal);
    }
}

TEST_CASE("a page ends where it is asked to and says what stands before and after it") {
    NoticesRig t;
    t.open();
    t.say_until(12);
    const nt::NoticesShown all = t.page();
    REQUIRE(all.rows.size() >= 12);
    const std::int64_t last = all.last;
    // FOUR ENDING THREE BEFORE THE NEWEST.
    const nt::NoticesShown four = t.page(last - 3, 4);
    REQUIRE(four.rows.size() == 4);
    CHECK(four.rows.front().number == last - 6);
    CHECK(four.rows.back().number == last - 3);
    CHECK(four.after == 3);
    CHECK(four.next == last - 2);
    CHECK(four.before == static_cast<std::int64_t>(all.rows.size()) - 7);
    CHECK(four.first == all.first);
    CHECK(four.last == last);
    // THE NEWEST, BY 0; NONE ASKED IS NONE GIVEN; AND NO PAGE IS LONGER THAN ITS BOUND.
    CHECK(t.page(0, 1).rows.back().number == last);
    const nt::NoticesShown none = t.page(0, 0);
    CHECK(none.rows.empty());
    CHECK(none.before == static_cast<std::int64_t>(all.rows.size()));
    CHECK(none.next == 0);
    CHECK(static_cast<std::int64_t>(t.page(0, 5 * nt::kMaxNoticesPage).rows.size()) <=
          nt::kMaxNoticesPage);
}

TEST_CASE("the Notices pane shows the launch's notes and every notice after them, the newest at "
          "the bottom") {
    NoticesRig t;
    t.open();
    // THE FRESH DESK SEATS IT.
    REQUIRE(t.row() != nullptr);
    CHECK(t.r.session().panes.has(t.kind()));
    bool launch = false;
    for (const std::string& row_text : t.shown()) {
        launch = launch || row_text.find(NoticesRig::kLaunchNote) != std::string::npos;
    }
    CHECK(launch);
    CHECK(t.bottom_notice() == t.r.session().notice);
    t.statement();
    CHECK(t.bottom_notice() == t.r.session().notice);
    t.refusal();
    CHECK(t.bottom_notice() == t.r.session().notice);
    // ...THE REFUSAL IN THE ALERT'S ROLE, a statement in the ordinary ink.
    std::int64_t refusal_role = gfx::role::kNone, statement_role = gfx::role::kNone;
    for (const HeldRun& run : held_runs(t.kind())) {
        if (run.text.find("only layout") != std::string::npos) refusal_role = run.role;
        if (run.text.find(NoticesRig::kLaunchNote) != std::string::npos) statement_role = run.role;
    }
    CHECK(refusal_role == gfx::role::kAlert);
    CHECK(statement_role == gfx::role::kFill);
}

TEST_CASE("a key and a wheel's notch move the Notices pane's rows the same") {
    NoticesRig t;
    t.open();
    t.say_until(20);
    t.give_room(4, 70);
    const std::vector<std::string> newest = t.shown();
    CHECK(newest.front().rfind("  ... ", 0) == 0); // what is kept and not shown, above
    t.focus();
    t.r.key(input::scan::kUp);
    const std::vector<std::string> by_key = t.shown();
    CHECK(by_key != newest);
    CHECK(by_key.back() == "  ... 1 newer");
    t.r.key(input::scan::kDown);
    CHECK(t.shown() == newest);
    const CellRect body = external_body_rect(t.r.session(), t.kind());
    t.r.wheel_cell(1.0, body.x + 2, body.y + 1);
    CHECK(t.shown() == by_key);
    t.r.wheel_cell(-1.0, body.x + 2, body.y + 1);
    CHECK(t.shown() == newest);
    // READ BACK, IT STAYS WHERE IT IS WHILE MORE IS SAID; AT THE NEWEST IT FOLLOWS.
    t.r.wheel_cell(2.0, body.x + 2, body.y + 1);
    const std::vector<std::string> back = t.shown();
    CHECK(back.back() == "  ... 2 newer");
    t.unfocus(); // a press of its own, which Workshop answers in words too
    t.statement();
    const std::vector<std::string> later = t.shown();
    CHECK(std::vector<std::string>(later.begin(), later.end() - 1) ==
          std::vector<std::string>(back.begin(), back.end() - 1));
    CHECK(later.back() != back.back());
    t.r.wheel_cell(-20.0, body.x + 2, body.y + 1);
    CHECK(t.bottom_notice() == t.r.session().notice);
    t.refusal(); // words no earlier notice says
    CHECK(t.bottom_notice() == t.r.session().notice);
    CHECK(t.shown().back().rfind("  ... ", 0) != 0); // the newest is the last row: no newer

}

TEST_CASE("a flood of wheel notches moves the Notices pane one notice a notch, none lost") {
    NoticesRig t;
    t.open();
    t.say_until(60);
    t.give_room(4, 70);
    const CellRect body = external_body_rect(t.r.session(), t.kind());
    // ONE TURN OF TWENTY NOTCHES, as a window reports a fast wheel: twenty notices older.
    t.r.wheel_cell(20.0, body.x + 2, body.y + 1);
    CHECK(t.shown().back() == "  ... 20 newer");
    // ...AND TWENTY KEYS QUEUED BEFORE ANY PAGE COMES BACK: twenty more.
    for (int i = 0; i < 20; ++i) {
        t.r.key(input::scan::kUp);
    }
    CHECK(t.shown().back() == "  ... 40 newer");
    // ...AND NOTCHES PAST THE NEWEST ARE SPENT THERE: the view follows, and what is said after
    // finds no step still waiting to move it.
    t.r.wheel_cell(-60.0, body.x + 2, body.y + 1);
    CHECK(t.shown().back().rfind("  ... ", 0) != 0);
    t.unfocus();
    t.refusal();
    CHECK(t.bottom_notice() == t.r.session().notice);
    CHECK(t.shown().back().rfind("  ... ", 0) != 0);
}

TEST_CASE("a notice said while the Notices pane is closed is there when it opens") {
    NoticesRig t;
    t.open();
    t.r.pick(notices_ref());
    REQUIRE_FALSE(t.r.session().panes.has(t.kind()));
    t.refusal();
    const std::string said = t.r.session().notice;
    t.r.pick(notices_ref());
    REQUIRE(t.r.session().panes.has(t.kind()));
    // ...ABOVE WHAT OPENING IT SAID.
    CHECK(t.shows(said));
    CHECK(t.bottom_notice() == t.r.session().notice);
}
