// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — WHAT IS TRUE RIGHT NOW, AS A LOADED WEAVE.
//
// THIS FILE OWNS the view that used to be chrome inside this host. Everything the
// current-condition view did a maker can see -- listing every condition in its owner's own
// words, explaining the one under the cursor, moving that cursor, hiding a statement and
// finding it again when it materially changes -- is driven here through the REAL
// `zengine-attention-pane` image, over the REAL pane protocol, against the REAL publication
// this host makes. Nothing in this file constructs the weave, reaches into its state, or
// calls one of its functions: there is a shared library on disk, a plan row that loads it,
// an office it holds, and a maker's hand.
//
// ---- WHY THIS ONE IS DIFFERENT FROM THE OTHER TWO --------------------------------
//
// ⚠ IT WAS NEVER A PANE. Files and the Builder were rows of `kPanelCatalog`: a maker could
// open them from the picker, arrange them on a desk and name them in a saved setup. This was
// an OVERLAY -- a global chord opened it, it owned the keyboard whole while it was up, it
// was drawn into a popup in the picker's own plane, and no file could name it. So there is
// no saved reference to convert (`pane_migration.hpp` gains nothing), and the arrival case
// below is asking something the other two suites could take for granted: that a thing which
// was never on the desk can be put on it.
//
// ⚠ AND IT DERIVES NOTHING. The Files browser walks a filesystem; the Builder asks a tool.
// This pane is told. Every row of it is the host's reading of the host's own state, and the
// publication that carries it is the one new sentence in the whole arc (WL-ATTN-12) -- so
// the cases here drive the HOST into a state and then read what the PANE made of it, which
// is the only honest picture of a presentation that owns no facts.

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

    /// PRESS INTO THE PANE, which is the whole of what VD-22 made necessary: its rows are
    /// active only while it holds the keyboard. The overlay this replaces owned the keys
    /// from the moment a chord opened it, wherever the maker was standing.
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
    // ⭐ THE PHASE'S CENTRAL CLAIM, MEASURED AT THE SEAM. Workshop compiled nothing for this
    // view, minted no kind for it and holds no branch on it: what puts it on a maker's
    // screen is a row in an editable file naming an artifact, and an offer this host learns
    // about at runtime like any other.
    AttentionRig f;
    f.open();

    // A RUNTIME HANDLE, MINTED FROM A LIVE OFFER -- never a compile-time kind.
    REQUIRE(f.row() != nullptr);
    CHECK(is_runtime_kind(f.kind));
    CHECK(std::string(f.row()->name) == pane::kAttentionPaneName);

    // ...AND THE PICKER LISTS IT UNDER THE OFFICE THAT OFFERED IT, which is the only answer
    // to "whose pane is this" (WL-CAT-03) -- and is the first time in this application's
    // life that the current-condition view has been a thing a maker could CHOOSE.
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
    // THE IDS DID NOT MOVE. `attention.up`, `attention.down` and `attention.dismiss` were
    // rows of a Workshop keyboard context and are the PANE's now, spelled exactly as they
    // were, with the same defaults -- so an authored override keeps working across the
    // migration. Legal because the host's rows left in the same commit.
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

    // ...AND THE HOST DECLARES NEITHER THEM NOR THE TWO THAT RETIRED. A row in both catalogs
    // would be one authored override naming two things, which `join_pane_rows` refuses whole
    // (WL-KEY-06/08). `attention.close` and `workshop.attention` are gone rather than moved:
    // a pane has nothing to close, and nothing opens one particular pane from anywhere.
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

    // MOVING THE CURSOR MOVES WHICH EXPLANATION IS SPENT, and changes nothing else. The
    // gesture is a declared id now, resolved by the host against the effective keymap.
    f.r.key(input::scan::kDown);
    CHECK(f.text().find("and one for the second") != std::string::npos);
    CHECK(f.r.session().conditions.holds("a.one"));
    CHECK(f.r.session().conditions.holds("b.two"));
}

TEST_CASE("ATTN-WEAVE: a pane that arrives after the host has spoken is told again") {
    // ⭐ THE DEFECT THE SILENCE RULE CREATED, AND ITS REPAIR. `say_conditions` is quiet when
    // the reading has not changed -- without that the seam does not terminate -- and the
    // cost of it is that a pane loaded AFTER this host last spoke would never hear a word.
    // It would sit there saying `(waiting)` over a screen where everything was known. An
    // OFFER is the one moment a new listener certainly exists, so an offer makes the next
    // reading news again.
    //
    // ⚔ MUTATION, MEASURED: drop `conditions_said_ = false` from `on(PaneOffered)`. All three
    //   checks go red together -- the pane opens into `ATTENTION (waiting)` and stays there
    //   until something about the world happens to change.
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
    // FALSIFIER 2, AT THE SEAM -- a dismissal that mutates truth. The condition's owner is
    // this host; the pane can hide a statement and can do nothing else to it, which is now
    // a fact about what it is ABLE to say rather than a discipline it keeps.
    AttentionRig f;
    f.open();
    f.establish(thing("test.wall", "a wall", "why it is a wall"));
    REQUIRE(f.text().find("a wall") != std::string::npos);

    // HIDE THE ONE THE CURSOR IS ON.
    f.letter(input::scan::kD, "d");
    CHECK(f.text().find("ATTENTION -- 0 conditions") != std::string::npos);
    CHECK(f.text().find("hidden -- a wall is still true") != std::string::npos);

    // ...AND THE TRUTH IS NOT TOUCHED. The host still holds the condition, still derives it,
    // and still says it on the compact chip -- which is the CHANGE this migration made and
    // is the honest reading of "dismiss is not resolve": the chip says what is true, and the
    // pane says what this maker has chosen to look at.
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
    // ⚠ THE ONE RULE THE PANE HAS THAT THE BUILT-IN DID NOT, and it exists because the set
    // is durable now. The host rebuilt its dismissal set against a fresh derivation every
    // paint and could hold a stale entry harmlessly for a session; this set is the pane's
    // own state and crosses a reload, so an entry that outlived its subject would be a
    // decision about a fact that no longer exists, re-applied silently if it ever returned.
    //
    // ⚔ MUTATION, MEASURED: drop `forget_resolved`. The last check goes red on its own -- the
    //   condition comes back and is invisible, hidden by a decision made about a moment that
    //   is over.
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

TEST_CASE("ATTN-WEAVE: the pane's keys act only after the maker has pressed into it") {
    // ⭐ VD-22, AT THIS PANE. The overlay owned the keyboard from the moment a chord opened
    // it, wherever the maker was standing. A pane's rows are active only while it holds the
    // keys, and `d` from anywhere else is an ordinary command-mode keystroke that reaches
    // nobody here.
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
    // ⭐ THE DEFECT THE WHOLE-LOOP WITNESS FOUND, pinned. A condition's words are its
    // OWNER's -- a file loader's refusal, a pane's own sentence about why an update did not
    // fit -- and nothing has ever required them to be printable ASCII. The built-in drew
    // them into a region and let each medium make of them what it could; the seam JUDGES a
    // publication instead and refuses a row carrying a byte a canvas cannot draw
    // (`judge_content`). So one control byte inside a loader's refusal took the WHOLE pane
    // down: Workshop refused the content, cleared the rows, and raised a condition about
    // the refusal -- which this pane then could not show either.
    //
    // The pane gates its own rows at its own door, which is the discipline `files.cpp`
    // already keeps for typed and pasted text one pane over.
    //
    // ⚔ MUTATION, MEASURED: drop `drawable` from `push`. The case does not merely fail -- it
    //   does not TERMINATE (SIGTERM at a 120s wall). A refused publication makes Workshop
    //   clear the pane's rows and raise a condition ABOUT the refusal, which is news, which
    //   is published, which this pane answers with another refused publication. This pane is
    //   the one pane for which its own refusal is an input, and that is the sharpest possible
    //   statement of why its rows must be admissible by construction rather than by luck.
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
    // A REGION PADS WHAT IT WAS NOT GIVEN AND SILENTLY DROPS WHAT WILL NOT FIT, in BOTH
    // media -- so a composition that over-spends its budget loses whatever it wrote last,
    // which here is the omission marker: the one row that exists to say something was
    // dropped. The built-in's own claim, asked of the weave, and now asked from OUTSIDE the
    // image: what is counted is what Workshop accepted and drew.
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

// ⚠ WHAT A SAME-SHAPE RELOAD KEEPS IS NOT WITNESSED HERE, and that is named rather than
// implied. `AttentionPaneState::dismissed` is the pane's shape, so RELOAD-1's own machinery
// carries it -- the same machinery `tests/test_workshop_load.cpp` drives end to end over a
// real Kernel, a real Manager and a staged image. Doing it again for this pane needs that
// rig, not this one, and this migration bought no new claim about reloading; the shape is
// true by construction and is not pinned for THIS pane.
