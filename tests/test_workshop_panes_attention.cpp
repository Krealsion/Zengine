// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- what is true right now, as a loaded weave: listing every condition
// in its owner's words, explaining the one under the cursor, moving that cursor, hiding a
// statement and finding it again when it materially changes, driven through the real
// `zengine-attention-pane` image over the real pane protocol against the host's real
// publication (WL-ATTN-12). The pane derives nothing -- every row is the host's reading -- so the
// cases drive the HOST into a state and read what the PANE made of it.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "attention-pane/vocabulary.hpp"
#include "workshop/attention_seam_vocabulary.hpp"

namespace {

namespace pane = zengine::attention_pane;

/// The office and pane a saved setup names, spelled through the package's own header --
/// the durable names, not literals, so a case cannot agree with a typo.
inline PaneRef attention_ref() {
    return PaneRef{pane::kAttentionPaneRole, pane::kAttentionPane};
}

/// A LIVE WORKSHOP WITH THE REAL VIEW LOADED INTO IT.
struct AttentionRig {
    PaneRig r;
    std::int64_t kind = 0;

    void open(std::int64_t width = 160, std::int64_t height = 48) {
        r.mount_workshop();
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kAttentionPaneStem;
        seat.weave = load::WeaveIntent{pane::kAttentionPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `attention` pane");
        r.pick(attention_ref());
        kind = row()->kind;
        focus();
    }

    /// PRESS INTO THE PANE: its rows are active only while it holds the keyboard.
    void focus() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y);
        REQUIRE(r.session().panels.keyboard == kind);
    }

    /// Hand the keys back the way a maker does -- a press on the bare workspace.
    void unfocus() {
        r.press_cell(0, screen_of(r.session()).h - 1);
        REQUIRE(r.session().panels.keyboard != kind);
    }

    const RuntimePane* row() {
        return r.session().panels.runtime.find(pane::kAttentionPaneRole, pane::kAttentionPane);
    }

    std::vector<std::string> shown() { return pane_rows(r, kind); }

    /// Every row the pane published, joined -- what a maker reads at the pane's rectangle.
    std::string text() {
        std::string all;
        for (const std::string& row_text : shown()) {
            all += row_text;
            all += '\n';
        }
        return all;
    }

    /// TELL THE HOST SOMETHING IS TRUE, and let it say so. Establishing on the session is
    /// what a real owner does (`load_keymap`'s wall, `load_prefs`' twin); the keystroke is
    /// only a repaint, which is when the host compares and speaks.
    void establish(const Condition& c) {
        const_cast<Session&>(r.session()).conditions.establish(c);
        r.key(input::scan::kDown);
    }

    void retract(const std::string& key) {
        const_cast<Session&>(r.session()).conditions.retract(key);
        r.key(input::scan::kDown);
    }

    /// A bare-letter gesture, as a backend really reports one: the key transition AND the
    /// character it produced.
    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
    }

    /// A NEW ROOM AND NOTHING ELSE: the surface changes size, Workshop grants the pane its room
    /// again, and the pane says its rows -- the ordinary repaint that exposes a notice cleared in
    /// private. Required to be a real grant, so a deduplicated extent cannot pass for one.
    void regrant() {
        const ExternalPane* seat = r.session().panels.external_pane(kind);
        REQUIRE(seat != nullptr);
        const std::int64_t rows = seat->rows;
        const std::int64_t columns = seat->columns;
        wide_ = !wide_;
        author_test_pane_room(r, kind, rows + 1, columns);
        r.extent(wide_ ? 160 : 150, wide_ ? 48 : 44);
        const ExternalPane* after = r.session().panels.external_pane(kind);
        REQUIRE(after != nullptr);
        REQUIRE_MESSAGE((after->rows != rows || after->columns != columns),
                        "the surface changed and the pane's room did not");
    }
    bool wide_ = true;
};

inline Condition thing(const char* key, const char* compact, const char* detail,
                       std::int64_t role = surface::role::kAlert,
                       const char* action = "") {
    return Condition{key, compact, detail, role, action};
}

} // namespace

// ============================================================================
// ATTN-WEAVE — the view arrives, and it is a stranger
// ============================================================================

TEST_CASE("ATTN-WEAVE: the view arrives by a plan row, under an office of its own") {
    // Workshop compiled nothing for this view, minted no kind for it and holds no branch on it:
    // what puts it on a maker's screen is a row in an editable file naming an artifact, and an
    // offer this host learns about at runtime like any other.
    AttentionRig f;
    f.open();

    // A RUNTIME HANDLE, MINTED FROM A LIVE OFFER -- never a compile-time kind.
    REQUIRE(f.row() != nullptr);
    CHECK(is_runtime_kind(f.kind));
    CHECK(std::string(f.row()->name) == pane::kAttentionPaneName);

    // ...AND THE INVENTORY LISTS IT UNDER THE OFFICE THAT OFFERED IT, which is the only answer
    // to "whose pane is this" (WL-CAT-03) -- a view a maker can CHOOSE.
    bool listed = false;
    for (const CatalogRow& row : combined_catalog(f.r.session().panels)) {
        listed = listed || row.ref == attention_ref();
    }
    CHECK(listed);

    // ...AND THE HOST'S OWN CATALOG DOES NOT OFFER IT. There is one Attention pane in this
    // process and it belongs to the image that was loaded.
    for (const PanelKind& built_in : kPanelCatalog) {
        CHECK(std::string(built_in.pane) != std::string(pane::kAttentionPane));
    }
}

TEST_CASE("ATTN-WEAVE: the pane declares the three ids a maker's keymap file already names") {
    // `attention.up`, `attention.down` and `attention.dismiss` are the PANE's, spelled as a
    // keymap file names them, with their defaults -- so an authored override keeps working.
    AttentionRig f;
    f.open();
    const RuntimePane* seat = f.row();
    REQUIRE(seat != nullptr);
    std::vector<std::string> declared;
    for (const v2::PaneActionRow& row : seat->actions) {
        declared.push_back(row.id);
    }
    std::sort(declared.begin(), declared.end());
    CHECK(declared == std::vector<std::string>{pane::kActionDismiss, pane::kActionDown,
                                               pane::kActionUp});

    // ...AND THE HOST DECLARES NEITHER THEM NOR `attention.close` AND `workshop.attention`. A row
    // in both catalogs would be one authored override naming two things, which `join_pane_rows`
    // refuses whole (WL-KEY-06/08); a pane has nothing to close, and nothing opens one particular
    // pane from anywhere.
    for (const std::string& gone : declared) {
        INFO("id ", gone);
        CHECK(row_of_id(gone.c_str()) == nullptr);
    }
    CHECK(row_of_id("attention.close") == nullptr);
    CHECK(row_of_id("workshop.attention") == nullptr);
}

TEST_CASE("ATTN-WEAVE: the pane shows every current condition in its owner's own words") {
    AttentionRig f;
    f.open();
    f.establish(thing("a.one", "the first thing", "a sentence its owner already had"));
    f.establish(thing("b.two", "the second thing", "and one for the second",
                      surface::role::kAccent));

    // ALL OF THEM, not only the compact winner -- and in the host's own ranking.
    CHECK(f.text().find("ATTENTION -- 2 conditions") != std::string::npos);
    CHECK(f.text().find("the first thing") != std::string::npos);
    CHECK(f.text().find("the second thing") != std::string::npos);
    // ...and the owner's own explanation for the one being read.
    CHECK(f.text().find("a sentence its owner already had") != std::string::npos);

    // MOVING THE CURSOR MOVES WHICH EXPLANATION IS SPENT, and changes nothing else. The gesture
    // is a declared id, resolved by the host against the effective keymap.
    f.r.key(input::scan::kDown);
    CHECK(f.text().find("and one for the second") != std::string::npos);
    CHECK(f.r.session().conditions.holds("a.one"));
    CHECK(f.r.session().conditions.holds("b.two"));
}

TEST_CASE("ATTN-WEAVE: a pane that arrives after the host has spoken is told again") {
    // A PANE LOADED AFTER THE HOST LAST SPOKE HEARS WHAT IS TRUE: `say_conditions` is quiet when
    // the reading has not changed -- or the seam would not terminate -- so an OFFER, the one moment
    // a new listener certainly exists, makes the next reading news again. ⚔ MUTATION: drop
    // `conditions_said_ = false` from `on(PaneOffered)` and all three checks go red: the pane opens
    // into `ATTENTION (waiting)` and stays there until something about the world changes.
    AttentionRig f;
    // THE HOST SAYS ITS PIECE FIRST, with nobody listening for it.
    f.r.mount_workshop();
    f.r.ready();
    f.r.extent(160, 48);
    const_cast<Session&>(f.r.session())
        .conditions.establish(thing("test.wall", "a wall", "why it is a wall"));
    f.r.key(input::scan::kDown);

    // ...AND THE PANE ARRIVES AFTERWARDS.
    load::LoadPlan plan;
    load::ArtifactIntent seat;
    seat.stem = pane::kAttentionPaneStem;
    seat.weave = load::WeaveIntent{pane::kAttentionPaneRole};
    plan.artifacts.push_back(seat);
    const load::Executed done = f.r.run_plan(plan);
    REQUIRE_MESSAGE(done.ok, done.refusal);
    REQUIRE(f.row() != nullptr);
    f.r.pick(attention_ref());
    f.kind = f.row()->kind;

    // IT KNOWS WHAT IS TRUE, without having asked and without anything having changed.
    CHECK(f.text().find("ATTENTION -- 1 condition") != std::string::npos);
    CHECK(f.text().find("a wall") != std::string::npos);
    CHECK(f.text().find("waiting") == std::string::npos);
}
TEST_CASE("ATTN-WEAVE: dismissal hides a presentation and changes nothing that is true") {
    // FALSIFIER 2, AT THE SEAM -- a dismissal that mutates truth. The condition's owner is this
    // host; the pane can hide a statement and can do nothing else to it, which is a fact about
    // what it is ABLE to say rather than a discipline it keeps.
    AttentionRig f;
    f.open();
    f.establish(thing("test.wall", "a wall", "why it is a wall"));
    REQUIRE(f.text().find("a wall") != std::string::npos);

    // HIDE THE ONE THE CURSOR IS ON.
    f.letter(input::scan::kD, "d");
    CHECK(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);
    CHECK(f.text().find("hidden -- a wall is still true") != std::string::npos);

    // ...AND THE TRUTH IS NOT TOUCHED. The host still holds the condition, still derives it, and
    // still says it on the compact chip -- "dismiss is not resolve": the chip says what is true,
    // and the pane says what this maker has chosen to look at.
    CHECK(f.r.session().conditions.holds("test.wall"));
    CHECK(attention_conditions(f.r.session()).size() == 1);
}

TEST_CASE("ATTN-WEAVE: a dismissed condition comes back when it materially changes") {
    // FALSIFIER 3 -- a dismissal that never re-arms. It is scoped to the STATEMENT and not
    // to the key alone, and the pane recomposes the stamp from the fields that cross rather
    // than sharing a type with the host.
    AttentionRig f;
    f.open();
    f.establish(thing("test.wall", "a wall", "the first reason"));
    f.letter(input::scan::kD, "d");
    REQUIRE(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);

    // THE SAME STATEMENT, SAID AGAIN, IS STILL HIDDEN -- a dismissal a republication undid
    // would be a gesture with no effect.
    f.establish(thing("test.wall", "a wall", "the first reason"));
    CHECK(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);

    // A MATERIALLY DIFFERENT STATEMENT UNDER THE SAME KEY IS VISIBLE AGAIN, with nobody
    // clearing anything.
    f.establish(thing("test.wall", "a WIDER wall", "the first reason"));
    CHECK(f.text().find("ATTENTION -- 1 condition") != std::string::npos);
    CHECK(f.text().find("a WIDER wall") != std::string::npos);
}

TEST_CASE("ATTN-WEAVE: a dismissal does not outlive the condition it was about") {
    // AN ENTRY THAT OUTLIVED ITS SUBJECT IS DROPPED: the dismissal set is the pane's own state and
    // crosses a reload, so a kept entry would be a decision about a fact that no longer exists,
    // re-applied silently if it returned. ⚔ MUTATION: drop `forget_resolved` and the last check
    // goes red on its own -- the condition comes back invisible, hidden by a decision about a
    // moment that is over.
    AttentionRig f;
    f.open();
    f.establish(thing("test.wall", "a wall", "why"));
    f.letter(input::scan::kD, "d");
    REQUIRE(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);

    // RESOLVED, AND THE HIDING GOES WITH IT: dismiss is still not resolve -- what is
    // dropped is the maker's decision not to LOOK, once there is nothing left to look at.
    f.retract("test.wall");
    CHECK(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);
    f.establish(thing("test.wall", "a wall", "why"));
    CHECK(f.text().find("ATTENTION -- 1 condition") != std::string::npos);
}

TEST_CASE("an id the Attention pane never declared is no act: the notice, the hiding and both conditions stand through a new room, and spending the notice un-says nothing true") {
    // THE THREE ROWS NEVER CHANGE, so an undeclared id reaches this pane only from Workshop's own
    // office with no key behind it; a pane that spent its notice before asking what the id meant
    // left `hidden -- ... is still true` painted over a private clear. Three lifetimes: the notice
    // ends at the maker's next act, the hiding with its condition, the condition when its owner
    // retracts it -- an ignored id ends none, and the act that ends the first ends neither other.
    AttentionRig f;
    f.open();
    f.unfocus(); // `establish` repaints with a key, which must not be this pane's
    f.establish(thing("a.one", "the first thing", "why the first"));
    f.establish(thing("b.two", "the second thing", "why the second", surface::role::kAccent));
    f.focus();
    REQUIRE(f.text().find("ATTENTION -- 2 conditions") != std::string::npos);
    f.letter(input::scan::kD, "d"); // the loudest, where the cursor rests
    REQUIRE(f.text().find("hidden -- the first thing is still true") != std::string::npos);
    REQUIRE(f.text().find("ATTENTION -- 1 condition") != std::string::npos);

    const PaneRig::OfficeAction unknown = f.r.workshop_action(
        pane::kAttentionPaneRole, pane::kAttentionPane, "attention.no-such-action");
    REQUIRE(unknown.authored);
    REQUIRE(unknown.delivered);
    CHECK(unknown.author == kWorkshopProvider);
    CHECK(f.text().find("hidden -- the first thing is still true") != std::string::npos);
    f.regrant();
    CHECK(f.text().find("hidden -- the first thing is still true") != std::string::npos);
    // ...AND IT HID NOTHING AND RESOLVED NOTHING: the second is still shown with its explanation,
    // the first is still hidden, and the host holds both.
    CHECK(f.text().find("ATTENTION -- 1 condition") != std::string::npos);
    CHECK(f.text().find("> the second thing") != std::string::npos);
    CHECK(f.text().find("why the second") != std::string::npos);
    CHECK(f.r.session().conditions.holds("a.one"));
    CHECK(f.r.session().conditions.holds("b.two"));

    // A DECLARED ID THROUGH THE SAME DOOR IS AN ACT, and spends the notice -- so the provenance
    // was never the reason for the silence above.
    const PaneRig::OfficeAction up =
        f.r.workshop_action(pane::kAttentionPaneRole, pane::kAttentionPane, pane::kActionUp);
    REQUIRE(up.delivered);
    CHECK(f.text().find("hidden -- the first thing") == std::string::npos);
    // SPENDING THE SENTENCE UN-SAYS NOTHING TRUE. The hiding stands, through a new room as well;
    // the condition stands; and the host still says it, on the compact chip and across the seam.
    f.regrant();
    CHECK(f.text().find("ATTENTION -- 1 condition") != std::string::npos);
    CHECK(f.text().find("the first thing") == std::string::npos);
    CHECK(f.r.session().conditions.holds("a.one"));
    CHECK(attention_conditions(f.r.session()).size() == 2);
    CHECK(f.r.attention_note().find("the first thing") != std::string::npos);
    REQUIRE_FALSE(f.r.said_conditions.empty());
    CHECK(f.r.said_conditions.back().rows.size() == 2);
}

TEST_CASE("an Attention pane whose every current condition is hidden says they are hidden and still true, through a spent notice and a new room, and says nothing needs attention only when nothing is true") {
    // HIDING IS A PRESENTATION CHOICE. With every current condition hidden the list was empty,
    // and the pane said the sentence it says when NOTHING is true -- `nothing needs your
    // attention right now` -- while the host held the condition, the chip named it and the
    // publication carried it.
    AttentionRig f;
    f.open();
    const std::string empty = "nothing needs your attention right now";
    CHECK(f.text().find(empty) != std::string::npos); // nothing is true yet: the empty sentence

    f.unfocus(); // `establish` repaints with a key, which must not be this pane's
    f.establish(thing("a.one", "the first thing", "why the first"));
    f.focus();
    f.letter(input::scan::kD, "d");
    REQUIRE(f.text().find("hidden -- the first thing is still true") != std::string::npos);
    CHECK(f.text().find("all conditions hidden -- 1 is still true") != std::string::npos);
    CHECK(f.text().find(empty) == std::string::npos);

    // SPENDING THE SENTENCE ABOUT THE GESTURE UN-SAYS NOTHING: the list still says it is hiding.
    f.r.key(input::scan::kUp);
    CHECK(f.text().find("hidden -- the first thing") == std::string::npos);
    CHECK(f.text().find("all conditions hidden -- 1 is still true") != std::string::npos);
    CHECK(f.text().find(empty) == std::string::npos);
    f.regrant();
    CHECK(f.text().find("all conditions hidden -- 1 is still true") != std::string::npos);
    CHECK(f.text().find(empty) == std::string::npos);
    // ...AND WHAT IS TRUE IS UNTOUCHED: the host holds it, the chip names it, the seam carries
    // it.
    CHECK(f.r.session().conditions.holds("a.one"));
    CHECK(attention_conditions(f.r.session()).size() == 1);
    CHECK(f.r.attention_note().find("the first thing") != std::string::npos);
    REQUIRE_FALSE(f.r.said_conditions.empty());
    CHECK(f.r.said_conditions.back().rows.size() == 1);

    // TWO, BOTH HIDDEN: the count is what is hidden.
    f.unfocus();
    f.establish(thing("b.two", "the second thing", "why the second", surface::role::kAccent));
    f.focus();
    REQUIRE(f.text().find("ATTENTION -- 1 condition") != std::string::npos);
    f.letter(input::scan::kD, "d");
    CHECK(f.text().find("all conditions hidden -- 2 are still true") != std::string::npos);

    // RETRACTED BY THEIR OWNER: nothing is true, so nothing is hidden, and the empty sentence is
    // the true one again.
    f.unfocus();
    f.retract("a.one");
    CHECK(f.text().find("all conditions hidden -- 1 is still true") != std::string::npos);
    f.retract("b.two");
    CHECK(f.text().find(empty) != std::string::npos);
    CHECK(f.text().find("all conditions hidden") == std::string::npos);
}

TEST_CASE("ATTN-WEAVE: the pane's keys act only after the maker has pressed into it") {
    // THE PANE'S ROWS ARE ACTIVE ONLY WHILE IT HOLDS THE KEYS, and `d` from anywhere else is an
    // ordinary command-mode keystroke that reaches nobody here.
    AttentionRig f;
    f.open();
    f.establish(thing("test.wall", "a wall", "why"));
    REQUIRE(f.text().find("ATTENTION -- 1 condition") != std::string::npos);

    f.unfocus();
    f.letter(input::scan::kD, "d");
    CHECK(f.text().find("ATTENTION -- 1 condition") != std::string::npos); // nothing hidden

    f.focus();
    f.letter(input::scan::kD, "d");
    CHECK(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);
}

TEST_CASE("ATTN-WEAVE: the action a condition names arrives as words and not as a name") {
    // THE HOLDS-NO-POWER LAW (WL-ATTN-10), at the seam. What crosses is the sentence the
    // host composed against the effective keymap; the id stays on that side, so the pane
    // could not press it if it wanted to and a maker sees the key they themselves bound.
    AttentionRig f;
    f.open();
    f.establish(thing("test.thing", "a thing", "why it is a thing", surface::role::kAlert,
                      "workshop.manage"));
    CHECK(f.text().find("try: w arrange desk") != std::string::npos);
    CHECK(f.text().find("workshop.manage") == std::string::npos);
}

TEST_CASE("ATTN-WEAVE: a condition carrying a byte a canvas cannot draw is still shown") {
    // A CONDITION'S WORDS ARE ITS OWNER'S, and nothing requires them to be printable ASCII, while
    // the seam refuses a row carrying a byte a canvas cannot draw (`judge_content`) -- so the
    // pane gates its rows at its own door, as `files.cpp` does for typed and pasted text. ⚔
    // MUTATION: drop `drawable` from `push` and the case does not terminate (SIGTERM at 120 s):
    // the refusal raises a condition, which is news, which this pane publishes and has refused
    // again. This pane's own refusal is an input to it, so its rows must be admissible by design.
    AttentionRig f;
    f.open();
    f.establish(Condition{"test.wall", "a wall",
                          std::string("line one") + "\n" + "line two" +
                              "\t" + "and back",
                          surface::role::kAlert, std::string()});

    // THE PANE'S CONTENT WAS ACCEPTED, which is the whole claim: no refusal, no cleared
    // rows, and no condition about a condition.
    const ExternalPane* seat = f.r.session().panels.external_pane(f.kind);
    REQUIRE(seat != nullptr);
    CHECK(seat->refusal.empty());
    CHECK_FALSE(seat->awaiting);
    CHECK(f.text().find("a wall") != std::string::npos);
    // ...AND THE WORDS SURVIVED, with each undrawable byte standing in for itself rather
    // than being deleted: a maker reads the sentence and can see where it was folded.
    CHECK(f.text().find("line one") != std::string::npos);
    CHECK(f.text().find("line two") != std::string::npos);
    CHECK(f.text().find("and back") != std::string::npos);
}

TEST_CASE("ATTN-WEAVE: the pane never publishes more rows than the room it was granted") {
    // A REGION PADS WHAT IT WAS NOT GIVEN AND SILENTLY DROPS WHAT WILL NOT FIT, in BOTH media --
    // so a composition that over-spends its budget loses whatever it wrote last, which here is
    // the omission marker: the one row that exists to say something was dropped. Asked from
    // OUTSIDE the image: what is counted is what Workshop accepted and drew.
    AttentionRig f;
    f.open();
    for (int i = 0; i < 12; ++i) {
        // Long explanations on purpose: the cursor's block is what makes the naive window
        // arithmetic wrong, and a one-line detail would never reach the defect.
        f.establish(Condition{
            "k." + std::to_string(i), "condition number " + std::to_string(i),
            std::string("a long explanation that will certainly have to be wrapped across "
                        "several rows of any column this pane is ever given, ") +
                std::to_string(i),
            i % 2 == 0 ? surface::role::kAlert : surface::role::kAccent, "workshop.manage"});
    }
    // EVERY EXTENT THIS COMPOSITION IS HONEST AT, and the cursor walked the whole way down
    // each of them -- the cursor's own reserved block is what the arithmetic turns on.
    for (const std::int64_t height : {kScreenMinH, kScreenMinH + 7, kScreenMinH + 20}) {
        f.r.extent(kScreenMinW, height);
        const ExternalPane* pane = f.r.session().panels.external_pane(f.kind);
        REQUIRE(pane != nullptr);
        const std::int64_t room = pane->rows;
        for (int at = 0; at < 12; ++at) {
            f.r.key(input::scan::kDown);
            CAPTURE(height);
            CAPTURE(at);
            CHECK(static_cast<std::int64_t>(pane->shown.size()) <= room);
            CHECK_FALSE(pane->awaiting); // ...and every one of them was ACCEPTED
        }
    }
}

// ⚠ WHAT A SAME-SHAPE RELOAD KEEPS IS NOT WITNESSED HERE: `AttentionPaneState::dismissed` is the
// pane's shape, carried by the machinery `tests/test_workshop_load.cpp` drives end to end.

TEST_CASE("a pane whose holder has no door for a key is put down by Escape, and nothing is sent") {
    // ESCAPE-TO-DESELECT IS THE DESKTOP'S DECLARED ROW (WL-DESK-02), so this case supplies the
    // declarer. ⚠ NOT SILENCE, A DECLARATION: Workshop reads the office holder's accept-set -- the
    // answer it reads to choose a press's version -- and a holder with no `PaneKey` door could not
    // have spent this Escape. Attention is such a pane: three declared rows, none of them Escape's,
    // and no key accepted.
    AttentionRig a;
    a.open();
    mount_desktop(a.r);
    REQUIRE(a.r.session().panels.selected == a.kind);
    REQUIRE(a.r.session().panels.keyboard == a.kind);
    const std::size_t panes = a.r.session().panels.open.size();
    const Setup desk = a.r.session().setup.active;
    const std::vector<std::string> shown = a.shown();

    a.r.key(input::scan::kEscape);
    CHECK(a.r.session().panels.selected == kNoPaneKind);
    CHECK(a.r.session().panels.keyboard == kNoPaneKind);
    CHECK(a.r.last_notice().find("unselected") != std::string::npos);
    // THE PANE IS UNTOUCHED: open, in the same desk, showing the same rows.
    CHECK(a.r.session().panels.open.size() == panes);
    CHECK(a.r.session().setup.active == desk);
    CHECK(a.r.session().panels.has(a.kind));
    CHECK(a.shown() == shown);
}
