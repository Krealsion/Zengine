// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — WHAT IS TRUE RIGHT NOW, AS A LOADED WEAVE.
//
// THIS FILE OWNS the OBJECTS and PROPERTIES columns. Everything the Info panel did a maker
// can see -- listing the objects they authored, marking the one they are looking at, showing
// that one's properties, moving a cursor through them, opening a draft on a value, typing it,
// committing or cancelling it, and pressing the two controls -- is driven here through the
// REAL `zengine-info-pane` image, over the REAL pane protocol, against the REAL publication
// this host makes. Nothing in this file constructs the weave, reaches into its state, or
// calls one of its functions: there is a shared library on disk, a plan row that loads it,
// an office it holds, and a maker's hand.
//
// ---- WHY THIS ONE IS DIFFERENT FROM THE OTHER THREE ------------------------------
//
// ⚠ THE DESK ALREADY NAMES IT. Files, the Builder and Attention arrived as strangers: no
// saved setup mentioned them, and a case that wanted one on the desk had to pick it. This
// pane's reference is what `default_setup` authors (`workshop/setup.hpp`), so the office's
// offer RESOLVES A ROW THAT WAS ALREADY THERE and the pane opens with no gesture at all.
// That is the whole shape of the migration from a maker's side: a fresh Workshop with this
// image on disk looks like the Workshop they had, and a fresh Workshop WITHOUT it says so
// with one unresolved row rather than by silently having no Info.
//
// ⚠ AND IT SHOWS THE HOST'S OWN DOCUMENT. The Builder asks a tool and the browser walks a
// filesystem; this pane is shown a PICTURE (`DocumentShown`) the host derives from the
// document the host owns, and asks back through one shape (`DocumentActRequested`). So the
// cases here drive the HOST -- creating objects, selecting them, resizing the surface -- and
// then read what the PANE made of it. Nothing in the image can touch a document; it can ask,
// and be refused in the document's own words.
//
// ⚠ AND IT IS THE ONE MIGRATED PANE WITH A DRAFT AND A REFUSAL OF ITS OWN. `info.edit` opens
// a text line inside the pane, and while it is open the pane declares two ids and no more --
// which is the pane-is-one-keyboard-context rule spent on the case it was written for.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "info-pane/vocabulary.hpp"
#include "workshop/document_seam_vocabulary.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

namespace pane = zengine::info_pane;

/// The office and pane a saved setup names, spelled through the package's own header --
/// the durable names, not literals, so a case cannot agree with a typo.
inline PaneRef pane_info_ref() { return PaneRef{pane::kInfoPaneRole, pane::kInfoPane}; }

/// A LIVE WORKSHOP WITH THE REAL INFO PANE LOADED INTO IT.
struct InfoRig {
    PaneRig r;
    std::int64_t kind = 0;

    /// LOAD THE IMAGE AND LET THE DESK DO THE REST. There is no `pick` here, and its absence
    /// is the claim: `default_setup` names this reference, so seating it is reconciliation
    /// rather than a gesture.
    void open(std::int64_t width = 160, std::int64_t height = 48) {
        r.mount_workshop();
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kInfoPaneStem;
        seat.weave = load::WeaveIntent{pane::kInfoPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `info` pane");
        kind = row()->kind;
        REQUIRE(r.session().panels.has(kind));
    }

    /// A WORKSHOP WITH NO INFO IMAGE AT ALL -- the other half of the shipped desk.
    void open_without(std::int64_t width = 160, std::int64_t height = 48) {
        r.mount_workshop();
        r.ready();
        r.extent(width, height);
    }

    const RuntimePane* row() {
        return r.session().panels.runtime.find(pane::kInfoPaneRole, pane::kInfoPane);
    }

    /// PRESS INTO THE PANE, which is the whole of what VD-22 made necessary: its rows are
    /// active only while it holds the keyboard. The built-in this replaces took `up`, `down`
    /// and `Return` from command mode, wherever the maker was standing.
    void focus() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y); // the OBJECTS heading: a row that means nothing
        REQUIRE(r.session().panels.keyboard == kind);
    }

    void unfocus() {
        r.press_cell(0, screen_of(r.session()).h - 1);
        REQUIRE(r.session().panels.keyboard != kind);
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

    /// THE ROW A GIVEN PREFIX IS ON, in the pane's own lattice -- so a press names a row the
    /// case actually read rather than an index it counted.
    std::int64_t row_of(const std::string& prefix) {
        const std::vector<std::string> rows = shown();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].rfind(prefix, 0) == 0) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }

    void press_row(const std::string& prefix, std::int64_t column = 1) {
        const std::int64_t at = row_of(prefix);
        REQUIRE_MESSAGE(at >= 0, "no row begins with: ", prefix);
        press_pane(r, kind, at, column);
    }

    /// A bare-letter gesture, as a backend really reports one: the key transition AND the
    /// character it produced.
    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
    }

    /// THE HOST'S LAST PICTURE OF ITS OWN DOCUMENT -- what the pane was told, so a case
    /// asks the seam which rows are the maker's rather than counting on an order.
    DocumentShown picture() {
        REQUIRE_FALSE(r.said_documents.empty());
        return r.said_documents.back();
    }

    /// PUT THE PANE'S CURSOR ON THE FIRST ROW THE MAKER OWNS, by the pane's own `info.down`.
    /// The cursor rests on `Identity`, which the workspace makes and nobody authors, so a
    /// case about a DRAFT has to walk to a row a draft can open on.
    std::size_t go_to_first_editable() {
        const DocumentShown shown = picture();
        std::size_t want = shown.properties.size();
        for (std::size_t i = 0; i < shown.properties.size() && want == shown.properties.size();
             ++i) {
            if (shown.properties[i].editable) {
                want = i;
            }
        }
        REQUIRE(want < shown.properties.size());
        for (std::size_t step = 0; step < want; ++step) {
            r.key(input::scan::kDown);
        }
        return want;
    }

    /// The ids this pane declares RIGHT NOW, sorted.
    std::vector<std::string> declared() {
        const RuntimePane* seat = row();
        REQUIRE(seat != nullptr);
        std::vector<std::string> ids;
        for (const PaneActionRow& a : seat->actions) {
            ids.push_back(a.id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }
};

/// One object on the host's document, made the way a maker makes one.
inline void make_object(InfoRig& f) {
    f.unfocus();
    f.r.key(input::scan::kN);
}

} // namespace

// ============================================================================
// INFO-WEAVE — the column arrives, and the desk was already expecting it
// ============================================================================

TEST_CASE("INFO-WEAVE: the pane arrives by a plan row and resolves a row the desk already had") {
    // ⭐ THE PHASE'S CENTRAL CLAIM, MEASURED AT THE SEAM. Workshop compiles nothing for this
    // column, mints no kind for it and holds no branch on it: what puts it on a maker's
    // screen is a row in an editable file naming an artifact, and an offer this host learns
    // about at runtime like any other.
    InfoRig f;
    f.open();

    // A RUNTIME HANDLE, MINTED FROM A LIVE OFFER -- never a compile-time kind.
    REQUIRE(f.row() != nullptr);
    CHECK(is_runtime_kind(f.kind));
    CHECK(std::string(f.row()->name) == pane::kInfoPaneName);
    CHECK(std::string(f.row()->summary) == pane::kInfoPaneSummary);

    // ...AND THE HOST'S OWN CATALOG DOES NOT OFFER IT. There is one Info pane in this
    // process and it belongs to the image that was loaded.
    for (const PanelKind& built_in : kPanelCatalog) {
        CHECK(std::string(built_in.pane) != std::string(pane::kInfoPane));
    }
    CHECK(kinds_placed_in(placement::kSideRegion) == 0);

    // THE DESK NAMED IT BEFORE THE OFFICE EXISTED, which is what makes this migration
    // different from the other three: the row was authored by `default_setup`, and the
    // office's arrival RESOLVED it rather than adding it.
    CHECK(has_pane(f.r.session().setup.active, pane_info_ref()));
    CHECK(unresolved_panes(f.r.session().setup.active, f.r.session().panels).empty());
}

TEST_CASE("INFO-WEAVE: the shipped desk puts it at the right column, by name and not by number") {
    // THE PLACE MOVED WITH THE OFFICE AND IS SAID AS A PLACE. No pair of coordinates can
    // mean "the right edge, the room's full height" on a screen the desk does not know, so
    // the row spells `right-column` and `bounds_of` resolves it.
    InfoRig f;
    f.open();
    const SetupPane* seated = pane_of(f.r.session().setup.active, pane_info_ref());
    REQUIRE(seated != nullptr);
    CHECK(seated->place.mode == pane_unit::kRightColumn);
    CHECK(seated->place.x == 0); // a named place carries no coordinates
    CHECK(seated->place.y == 0);

    const Screen sc = screen_of(f.r.session());
    const PanelBounds where =
        bounds_of(f.r.session().panels, f.r.session().setup.active, f.kind, sc);
    REQUIRE(where.open);
    CHECK(where.placed_in == placement::kSideRegion);
    CHECK(cells_covered(where.rect) == placement_bounds(placement::kSideRegion, 0, sc));
    // AND IT REACHES THE ROOM'S RIGHT EDGE, which is the whole reason the place has a name.
    CHECK(cells_covered(where.rect).x + cells_covered(where.rect).w == sc.room_w);
}

TEST_CASE("INFO-WEAVE: a Workshop with no Info OFFICE keeps the row and says so") {
    // ⚠ THE PRICE OF THE MIGRATION, SAID OUT LOUD. A run in which nothing holds
    // `zengine.info` has a desk with a row it cannot present. That is what every unresolved
    // row looks like -- authored intent, kept, explained -- and it is better than the
    // alternative, which is a Workshop that quietly has no Info and no reason why.
    //
    // ⚠ AND IT IS NOT WHAT A MISSING ARTIFACT DOES, which is worth being exact about because
    // the two are easy to confuse. THIS rig loads no plan at all, which is the state a maker
    // reaches with a saved layout naming a pane their plan does not load. A tree whose
    // `zengine-info-pane.so` is absent is a different failure: the authored plan is
    // all-or-nothing, so the host prints the refusal and EXITS, exactly as it does for any
    // other artifact on that plan (measured in this phase's witness). `1 unresolved` is on
    // the band for the frames before the refusal arrives.
    InfoRig f;
    f.open_without();
    CHECK(f.row() == nullptr);
    CHECK(has_pane(f.r.session().setup.active, pane_info_ref()));

    const std::vector<PaneRef> waiting =
        unresolved_panes(f.r.session().setup.active, f.r.session().panels);
    REQUIRE(waiting.size() == 1);
    CHECK(waiting[0] == pane_info_ref());
    CHECK(setup_rest_text(f.r.session().setup, f.r.session().panels, f.r.session().keymap)
              .find("1 unresolved") != std::string::npos);

    // ...AND THE DOCUMENT IS ALL THERE, being authored by keys that were never the panel's.
    const std::size_t born = f.r.w->document().elements.size();
    f.r.key(input::scan::kN);
    CHECK(f.r.w->document().elements.size() == born + 1);
}

TEST_CASE("INFO-WEAVE: the pane declares the ids a maker's keymap file already names") {
    // THE THREE IDS DID NOT MOVE. `info.up`, `info.down` and `info.edit` were rows of
    // Workshop's COMMAND context and are the pane's now, spelled exactly as they were, with
    // the same default gestures -- so an authored override keeps working across the
    // migration. Legal because the host's rows left in the same commit.
    InfoRig f;
    f.open();
    CHECK(f.declared() == std::vector<std::string>{pane::kActionDown, pane::kActionEdit,
                                                   pane::kActionUp});

    // ...AND THE HOST DECLARES NONE OF THEM. A row in both catalogs would be one authored
    // override naming two things, which `join_pane_rows` refuses whole (WL-KEY-06/08).
    for (const std::string& moved : f.declared()) {
        INFO("id ", moved);
        CHECK(row_of_id(moved.c_str()) == nullptr);
    }

    // ⭐ AND THE DRAFT'S TWO COULD NOT KEEP THEIR NAMES. `draft.commit` and `draft.cancel`
    // are `KeyContext::kDraft`'s rows and the Pane Manager still declares them for ITS
    // drafts, so this pane spells `info.commit` and `info.cancel` on the same gestures. A
    // maker who moved `draft.commit` finds it moved for the Pane Manager and not here --
    // named because it is the price of two panes having shared one context.
    CHECK(row_of_id("draft.commit") != nullptr);
    CHECK(row_of_id("draft.cancel") != nullptr);
    CHECK(row_of_id(pane::kActionCommit) == nullptr);
    CHECK(row_of_id(pane::kActionCancel) == nullptr);
}

// ============================================================================
// INFO-WEAVE — the two lists, said as rows
// ============================================================================

TEST_CASE("INFO-WEAVE: the two headings and both lists are the pane's rows, over the host's "
          "document") {
    InfoRig f;
    f.open();
    // The host's boot document: two objects, the first of them selected.
    REQUIRE(f.r.w->document().elements.size() == 2);

    const std::string all = f.text();
    CHECK(all.find("OBJECTS") != std::string::npos);
    CHECK(all.find("PROPERTIES") != std::string::npos);
    // EVERY OBJECT, BY ITS OWN IDENTITY AND ITS OWN NAME -- the host's picture, not a count.
    for (const ui::Element& e : f.r.w->document().elements) {
        INFO("object #", e.id);
        CHECK(all.find("#" + std::to_string(e.id) + " " + e.label) != std::string::npos);
    }
    // ...AND THE SELECTED ONE IS MARKED, which is the one thing the list says that the
    // document does not.
    CHECK(f.row_of("> #" + std::to_string(f.r.session().selected)) >= 0);

    // THE PROPERTIES ARE THE SELECTION'S, in the host's own row order and with the host's
    // own values -- read back off the seam rather than composed here.
    const DocumentShown said = f.picture();
    REQUIRE_FALSE(said.properties.empty());
    // A ROW IS ITS LABEL IN A FIXED COLUMN, behind one mark cell -- `>` on the cursor's row
    // and a space on every other -- so the value column lines up whatever the cursor is on.
    for (const ShownProperty& p : said.properties) {
        INFO("property ", p.label);
        CHECK((f.row_of(" " + p.label) >= 0 || f.row_of(">" + p.label) >= 0));
    }
    // ...AND EXACTLY ONE OF THEM WEARS THE MARK.
    std::size_t marked = 0;
    for (const std::string& row_text : f.shown()) {
        marked += (!row_text.empty() && row_text[0] == '>' && row_text.rfind("> #", 0) != 0)
                      ? 1u
                      : 0u;
    }
    CHECK(marked == 1);
}

TEST_CASE("INFO-WEAVE: the picture is PUBLISHED, so a gesture that never touched the pane "
          "moves it") {
    // ⭐ THIS MIGRATION'S ONE NEW SENTENCE. WL-DOC-14 requires the canvas, the object list
    // and the inspector to agree after every gesture, and the document changes under this
    // pane constantly with no gesture into it -- a drag on the workspace, a nudge, a create,
    // a restore. A pane that could only ASK would be a list that is wrong most of the time.
    InfoRig f;
    f.open();
    const std::string before = f.text();
    const std::size_t said_before = f.r.said_documents.size();

    make_object(f);
    const std::int64_t made = f.r.w->document().elements.back().id;

    CHECK(f.r.said_documents.size() > said_before);
    CHECK(f.text() != before);
    CHECK(f.text().find("#" + std::to_string(made)) != std::string::npos);
    // ...and creating selects what it made, so the mark moved with it.
    CHECK(f.row_of("> #" + std::to_string(made)) >= 0);
}

TEST_CASE("INFO-WEAVE: an empty document says it is empty and says what to do next") {
    // A PANEL THAT MERELY GOES BLANK is indistinguishable from a tool that has broken, and a
    // maker can reach this state with their own hand.
    InfoRig f;
    f.open();
    while (!f.r.w->document().elements.empty()) {
        f.unfocus();
        f.r.key(input::scan::kD);
    }
    CHECK(f.text().find("(none) -- n makes one") != std::string::npos);
    CHECK(f.text().find("(nothing selected)") != std::string::npos);
}

TEST_CASE("INFO-WEAVE: what the body cannot show, it counts -- on the side it left it out") {
    // THE OMISSION MARKERS, over a document taller than the room. A list that silently
    // stopped at the last row it could draw would be a list a maker cannot trust.
    InfoRig f;
    f.open(160, 24); // a short room, so both lists are pressed
    for (int i = 0; i < 24; ++i) {
        make_object(f);
    }
    const std::string all = f.text();
    // The selection is the LAST object made, so the window is at the end of the list and
    // what it could not show is EARLIER.
    CHECK(all.find("... ") != std::string::npos);
    CHECK(all.find(" earlier") != std::string::npos);
    // AND THE PANE NEVER PUBLISHED MORE ROWS THAN THE ROOM IT WAS GRANTED, which is the
    // wall `judge_content` enforces and the reason a marker is paid for out of the budget
    // rather than added beneath it.
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    CHECK(static_cast<std::int64_t>(f.shown().size()) <= body.h);
}

TEST_CASE("INFO-WEAVE: the two controls are the last rows of the body, and say their own "
          "availability in characters") {
    // UNAVAILABLE IS SAID IN CHARACTERS, not in colour, so a colourless medium reads it too.
    InfoRig f;
    f.open();
    const std::vector<std::string> rows = f.shown();
    REQUIRE(rows.size() >= 2);
    CHECK(rows[rows.size() - 2].rfind("[ Create ]", 0) == 0);
    CHECK(rows[rows.size() - 1].rfind("[ Delete ]", 0) == 0);

    // WITH NOTHING TO DELETE, Delete presents as unavailable -- and Create does not, because
    // the two reasons are two different facts.
    while (!f.r.w->document().elements.empty()) {
        f.unfocus();
        f.r.key(input::scan::kD);
    }
    const std::vector<std::string> empty = f.shown();
    REQUIRE(empty.size() >= 2);
    CHECK(empty[empty.size() - 2].rfind("[ Create ]", 0) == 0);
    CHECK(empty[empty.size() - 1].rfind("( Delete )", 0) == 0);
}

// ============================================================================
// INFO-WEAVE — the gestures, through the real seam
// ============================================================================

TEST_CASE("INFO-WEAVE: a press on an object row selects it, through the document's own door") {
    InfoRig f;
    f.open();
    make_object(f);
    const std::vector<ui::Element>& objects = f.r.w->document().elements;
    REQUIRE(objects.size() >= 2);
    const std::int64_t first = objects.front().id;
    REQUIRE(f.r.session().selected != first);

    f.press_row("  #" + std::to_string(first));
    // THE HOST SELECTED IT -- the pane asked and the document answered; nothing in the image
    // can move a selection.
    CHECK(f.r.session().selected == first);
    CHECK(f.row_of("> #" + std::to_string(first)) >= 0);
}

TEST_CASE("INFO-WEAVE: pressing Create is the SAME operation the `n` key performs") {
    InfoRig f;
    f.open();
    const std::size_t born = f.r.w->document().elements.size();
    f.press_row("[ Create ]");
    CHECK(f.r.w->document().elements.size() == born + 1);
    CHECK(f.r.session().selected == f.r.w->document().elements.back().id);
}

TEST_CASE("INFO-WEAVE: pressing Delete is the SAME operation the `d` key performs") {
    InfoRig f;
    f.open();
    const std::size_t born = f.r.w->document().elements.size();
    REQUIRE(born > 0);
    f.press_row("[ Delete ]");
    CHECK(f.r.w->document().elements.size() == born - 1);
}

TEST_CASE("INFO-WEAVE: the pane's keys act only after the maker has pressed into it") {
    // ⭐ VD-22, ON THE PANE IT COSTS THE MOST. `up`, `down` and `Return` were COMMAND MODE's
    // rows: they reached the inspector from anywhere a maker was standing. They are this
    // pane's now and they reach it only while it holds the keyboard, which is a real change
    // in the gesture and the reason the migration is felt.
    InfoRig f;
    f.open();
    const std::string resting = f.text();

    // NOT FOCUSED: the arrows are command mode's and the pane does not move.
    f.r.key(input::scan::kDown);
    CHECK(f.text() == resting);

    // FOCUSED: the same key moves the cursor.
    f.focus();
    f.r.key(input::scan::kDown);
    CHECK(f.text() != resting);

    // ...AND HANDING THE KEYS BACK STOPS IT AGAIN.
    const std::string moved = f.text();
    f.unfocus();
    f.r.key(input::scan::kDown);
    CHECK(f.text() == moved);
}

TEST_CASE("INFO-WEAVE: a draft opens on the cursor's row, declares two ids and no more, and "
          "commits through the document") {
    InfoRig f;
    f.open();
    f.focus();
    f.go_to_first_editable();

    // THE CURSOR IS ON AN AUTHORED ROW: `info.edit` opens a draft there.
    f.r.key(input::scan::kReturn);
    CHECK(f.declared() == std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});

    // ⭐ AND THE HOST'S OWN `Return` IS NOT REACHABLE WHILE IT IS OPEN. A pane is ONE
    // keyboard context, so while a maker is typing, these two are the only rows this pane
    // declares and every other key arrives as an ordinary `PaneKey` for the line to consume.
    f.r.text("77");
    CHECK(f.text().find("77") != std::string::npos);

    const std::string before = f.text();
    f.r.key(input::scan::kEscape);
    // ⚠ THE NOTICE IS CUT TO THE ROOM LIKE EVERY OTHER ROW, and the mark is where it was cut.
    // This pane's room is the right column's 28 cells; a sentence longer than that is fitted
    // rather than allowed to refuse the whole publication, which is what `judge_content`
    // would do to it. So the case asks for the prefix a maker can actually read.
    const std::int64_t cancelled = f.row_of("edit cancelled");
    REQUIRE(cancelled == 0); // and a notice takes the pane's first row
    CHECK(f.shown()[0].find("...") != std::string::npos);
    CHECK(f.declared() ==
          std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
    CHECK(before != f.text());
}

TEST_CASE("INFO-WEAVE: a draft on a value the maker owns is written to the document") {
    InfoRig f;
    f.open();
    f.focus();
    // Walk to a row this document says is the maker's to author, using what the HOST said
    // about its own document rather than a row number this case invented.
    f.go_to_first_editable();

    f.r.key(input::scan::kReturn);
    for (int i = 0; i < 8; ++i) {
        f.r.key(input::scan::kBackspace);
    }
    f.r.text("12");
    f.r.key(input::scan::kReturn); // commit

    // THE DOCUMENT HOLDS IT, and the pane is showing the document's answer rather than its
    // own draft: the picture arrived on the same drain.
    CHECK(f.declared() ==
          std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
    CHECK(f.text().find("12") != std::string::npos);
}

TEST_CASE("INFO-WEAVE: a row the workspace makes is refused by the pane, in its own words") {
    // THE REFUSAL THAT IS THE PANE'S TO MAKE, because the reason is about the ROW: a resolved
    // value is not authored, so there is nothing to open a draft on. The document's own
    // refusals still come from the document.
    InfoRig f;
    f.open();
    f.focus();
    // THE CURSOR RESTS ON ONE, which is worth saying: the first row of this list is
    // `Identity`, a fact the workspace makes, so the very first `info.edit` a maker presses
    // is the one this case is about.
    const DocumentShown said = f.picture();
    REQUIRE_FALSE(said.properties.empty());
    REQUIRE_FALSE(said.properties.front().editable);

    f.r.key(input::scan::kReturn);
    // THE REFUSAL NAMES THE ROW IT IS ABOUT, on the pane's first row, fitted to the column
    // the pane was granted -- 28 cells, so the sentence is marked where it was cut.
    REQUIRE(f.row_of(said.properties.front().label) == 0);
    CHECK(f.shown()[0].find("is not autho") != std::string::npos);
    CHECK(f.shown()[0].find("...") != std::string::npos);
    // ...AND NO DRAFT OPENED: the ids are still the resting three.
    CHECK(f.declared() ==
          std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
}

TEST_CASE("INFO-WEAVE: a live draft holds both controls back, and the reason is the maker's") {
    // TWO REASONS, TWO OWNERS: a live draft is unfinished work the act would destroy, and the
    // pane is the party that knows. "Nothing to delete" is the document's and goes through.
    InfoRig f;
    f.open();
    f.focus();
    f.go_to_first_editable();
    f.r.key(input::scan::kReturn); // a draft on the cursor's row

    const std::vector<std::string> rows = f.shown();
    REQUIRE(rows.size() >= 2);
    CHECK(rows[rows.size() - 2].rfind("( Create )", 0) == 0);
    CHECK(rows[rows.size() - 1].rfind("( Delete )", 0) == 0);

    const std::size_t born = f.r.w->document().elements.size();
    f.press_row("( Create )");
    CHECK(f.text().find("finish the edit first") != std::string::npos);
    CHECK(f.r.w->document().elements.size() == born); // and the document did not move
}

TEST_CASE("INFO-WEAVE: a room too short for the body invents none of it") {
    // THE BUDGET IS TAKEN BEFORE EITHER LIST IS OFFERED ANYTHING, and a bound that grows
    // when it is exceeded is not a bound.
    InfoRig f;
    f.open(160, 48);
    const std::int64_t tall = static_cast<std::int64_t>(f.shown().size());
    REQUIRE(tall > 0);

    f.r.extent(160, 20);
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    CHECK(static_cast<std::int64_t>(f.shown().size()) <= body.h);
    CHECK(static_cast<std::int64_t>(f.shown().size()) < tall);
    // AND EVERY ROW FITS THE COLUMNS IT WAS GRANTED -- the other half of `judge_content`,
    // which refuses a publication WHOLE when one row is a byte too wide.
    for (const std::string& row_text : f.shown()) {
        CHECK(static_cast<std::int64_t>(row_text.size()) <= body.w);
    }
}

TEST_CASE("INFO-WEAVE: an object name a canvas cannot draw is still shown") {
    // A MAKER'S OWN TEXT HAS NEVER BEEN REQUIRED TO BE PRINTABLE ASCII, and a publication is
    // judged WHOLE: one undrawable byte would refuse every row the pane sent. Replacing the
    // byte costs the maker a character they can see is missing; sending it costs them the
    // pane.
    InfoRig f;
    f.open();
    // THE BYTE IS WRITTEN ONTO THE HOST'S OWN DOCUMENT, because no gesture in this rig can
    // type one into a LABEL -- the maker path this pane has is the property draft, and a
    // document loaded from a file is the way such a byte really arrives.
    WorkshopDoc& document = const_cast<WorkshopDoc&>(f.r.w->document());
    REQUIRE_FALSE(document.elements.empty());
    document.elements.front().label = std::string("na\x01me\x7f");
    make_object(f); // any gesture: the picture is re-derived and re-said

    CHECK_FALSE(f.shown().empty());
    for (const std::string& row_text : f.shown()) {
        for (const char c : row_text) {
            const unsigned char byte = static_cast<unsigned char>(c);
            INFO("row: ", row_text);
            CHECK(byte >= 0x20u);
            CHECK(byte < 0x7Fu);
        }
    }
    CHECK(f.text().find("na me") != std::string::npos);
}

// ============================================================================
// INFO-WEAVE — what the image is not allowed to be
// ============================================================================

TEST_CASE("INFO-WEAVE: the image that shows a maker's document holds no document") {
    // A SOURCE READ, and the reason it is one: "this pane owns no facts" is a claim about
    // what a translation unit NAMES, and only reading the file can keep it. A runtime case
    // could not tell a pane that re-derives from one that caches and happens to agree.
    std::ifstream in(INFO_PANE_SOURCE);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();
    REQUIRE(source.size() > 4096);

    // THE HOST'S PRESENTATION IS NOT REACHABLE FROM HERE. `screen.hpp` is Workshop's own
    // composition; an image that included it would be a second painter of the same surface.
    // Asked of the INCLUDE, not of the file: this pane's own comments name what it replaced
    // and what it may not reach, and a case that refused the word would be refusing the
    // explanation rather than the dependency.
    for (const char* forbidden : {"#include \"workshop/screen.hpp\"",
                                  "#include \"workshop/panel.hpp\"",
                                  "#include \"workshop/setup.hpp\"",
                                  "#include \"workshop/screen_info", "#include \"ui/"}) {
        INFO("includes ", forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
    // ...AND NO DOCUMENT TYPE IS NAMED IN ITS CODE AT ALL. `WorkshopDoc` and `ui::Element`
    // are the host's; what this image holds is the PICTURE it was shown.
    for (const char* owned : {"WorkshopDoc ", "doc::add", "doc::find"}) {
        INFO("names ", owned);
        CHECK(source.find(owned) == std::string::npos);
    }
    // ...AND WHAT IT DOES REACH IS THE PROTOCOL AND THE SEAM, both of which are values.
    CHECK(source.find("workshop/pane_vocabulary.hpp") != std::string::npos);
    CHECK(source.find("workshop/document_seam_vocabulary.hpp") != std::string::npos);
    // ...AND THE SHARED TEXT HELPERS RATHER THAN A FIFTH COPY OF THEM.
    CHECK(source.find("workshop/pane_text.hpp") != std::string::npos);
}

