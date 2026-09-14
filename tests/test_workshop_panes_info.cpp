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
        for (const v2::PaneActionRow& a : seat->actions) {
            ids.push_back(a.id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    /// THE ROWS WORKSHOP HOLDS FOR THIS PANE -- the content it admitted, which the next repaint
    /// paints. `shown()` is that picture painted, a turn behind; a spent notice leaves THESE.
    std::vector<std::string> admitted() {
        const ExternalPane* seat = r.session().panels.external_pane(kind);
        REQUIRE(seat != nullptr);
        std::vector<std::string> rows;
        for (const surface::SurfaceTextRow& one : seat->shown) {
            rows.push_back(one.text);
        }
        return rows;
    }

    /// The first painted row holding `needle`, or an empty string.
    std::string row_containing(const std::string& needle) {
        for (const std::string& one : shown()) {
            if (one.find(needle) != std::string::npos) {
                return one;
            }
        }
        return std::string();
    }

    /// A NEW ROOM AND NOTHING ELSE: the surface changes size, Workshop grants the pane its room
    /// again, and the pane says its rows -- the ordinary repaint that exposes a notice cleared in
    /// private. Required to be a real grant, so the case cannot pass on a deduplicated extent.
    void regrant() {
        const ExternalPane* seat = r.session().panels.external_pane(kind);
        REQUIRE(seat != nullptr);
        const std::int64_t rows = seat->rows;
        wide_ = !wide_;
        r.extent(wide_ ? 160 : 150, wide_ ? 48 : 44);
        REQUIRE(r.session().panels.external_pane(kind) != nullptr);
        REQUIRE_MESSAGE(r.session().panels.external_pane(kind)->rows != rows,
                        "the surface changed and the pane's room did not");
    }
    bool wide_ = true;

    /// A KEY, A PRESS OR TYPED TEXT QUEUED AND NOT DRAINED, so a case places a real gesture at an
    /// exact interval of a conversation (the Files rig's own doors); `settle` drains. `r.key` and
    /// `r.text` drain, so a burst built from them is several polls rather than one.
    void enqueue_key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void enqueue_text(const std::string& typed) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{typed}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void enqueue_press(std::int64_t at) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::PointerButton{1, true, body.x + 1,
                                                body.y + kExternalHeaderRows + at +
                                                    surface::kTuiCanvasTopRow,
                                                input::space::kCells, input::mod::kNone}),
            loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void settle() { r.bus.drain_until_idle(); }

    /// A SENTENCE SAID AS WORKSHOP'S OWN OFFICE, QUEUED AND NOT DRAINED --
    /// `PaneRig::workshop_action`'s verified door, so a case can place several deliveries to the
    /// pane in one burst, in the order Workshop would send them, where a key could not: a key
    /// resolves against the rows Workshop holds, and those trail the pane's own re-declaration by
    /// a delivery. `settle` drains.
    void enqueue_as_workshop(const loom::Value& said) {
        const loom::Ticket sent = r.bus.office_send_to_role_as(
            r.workshop_id, kWorkshopProvider, pane::kInfoPaneRole,
            loom::Message(said, r.workshop_id, r.workshop_id, 0));
        REQUIRE(sent.valid());
    }
    void enqueue_action(const char* id) {
        enqueue_as_workshop(loom::to_value(PaneActionRequested{pane::kInfoPane, id}));
    }

    /// WHERE A LABEL IS in the host's picture of the selected object.
    std::size_t property_index(const std::string& label) {
        const DocumentShown said = picture();
        for (std::size_t i = 0; i < said.properties.size(); ++i) {
            if (said.properties[i].label == label) {
                return i;
            }
        }
        FAIL("the selected object has no property called ", label);
        return said.properties.size();
    }

    /// OPEN A DRAFT ON ONE PROPERTY, the way a maker does: into the pane, the cursor walked down
    /// to the row, Return. The draft holds the property's value until the case types into it.
    void draft_on(const std::string& label) {
        focus();
        const std::size_t at = property_index(label);
        for (std::size_t step = 0; step < at; ++step) {
            r.key(input::scan::kDown);
        }
        r.key(input::scan::kReturn);
        REQUIRE(declared() == std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});
    }

    /// ...AND REPLACE WHAT IT HOLDS with `typed`.
    void draft_holding(const std::string& label, const std::string& typed) {
        draft_on(label);
        for (int i = 0; i < 16; ++i) {
            r.key(input::scan::kBackspace);
        }
        r.text(typed);
    }

    /// Does the first row Workshop ADMITTED for this pane begin with `prefix`?
    bool admitted_leads(const std::string& prefix) {
        const std::vector<std::string> rows = admitted();
        return !rows.empty() && rows.front().rfind(prefix, 0) == 0;
    }

    /// The painted row of one property, by its label: the mark cell, then the label column.
    std::string property_row(const std::string& label) {
        for (const std::string& one : shown()) {
            if (one.size() > 1 && one.rfind("> #", 0) != 0 && one.rfind("  #", 0) != 0 &&
                one.compare(1, label.size() + 1, label + " ") == 0) {
                return one;
            }
        }
        return std::string();
    }
};

/// HOW MANY OF THE HOST'S ANSWERS TO AN ACT HAVE REACHED THE PANE, read off the tap -- so a case
/// can stop between the rows the pane said at its act and the answer that comes after them. `heard`
/// is the order the pane was handed its answers and the typing around them.
struct AnswerTap {
    loom::Switchboard& bus;
    loom::ObserverId tap{};
    int answered = 0;
    std::vector<std::string> heard;
    AnswerTap(loom::Switchboard& on, loom::WeaveId pane_id) : bus(on) {
        tap = bus.add_observer([this, pane_id](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered || ev.target != pane_id) {
                return;
            }
            if (ev.schema_name == DocumentActed::zen_name) {
                ++answered;
                heard.push_back(ev.schema_name);
            } else if (ev.schema_name == PaneTextInput::zen_name ||
                       ev.schema_name == PaneKey::zen_name) {
                heard.push_back(ev.schema_name);
            }
        });
    }
    ~AnswerTap() { bus.remove_observer(tap); }
    AnswerTap(const AnswerTap&) = delete;
    AnswerTap& operator=(const AnswerTap&) = delete;
};

/// WHAT REACHED THE DOCUMENT'S DOOR, read off the tap at Workshop's delivery -- so a case counts
/// the commits that LEFT the pane, and the text each carried, not the ones it meant to send.
struct DocumentAskTap {
    loom::Switchboard& bus;
    loom::ObserverId tap{};
    std::vector<std::string> commits; ///< each commit's text, in the order the door received them
    int others = 0;                   ///< a select, a create or a delete
    DocumentAskTap(loom::Switchboard& on, loom::WeaveId door) : bus(on) {
        tap = bus.add_observer([this, door](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered || ev.target != door ||
                ev.schema_name != DocumentActRequested::zen_name || ev.payload == nullptr) {
                return;
            }
            const DocumentActRequested asked = loom::from_value<DocumentActRequested>(*ev.payload);
            if (asked.act == kDocumentCommit) {
                commits.push_back(asked.text);
            } else {
                ++others;
            }
        });
    }
    ~DocumentAskTap() { bus.remove_observer(tap); }
    DocumentAskTap(const DocumentAskTap&) = delete;
    DocumentAskTap& operator=(const DocumentAskTap&) = delete;
};

/// Did the tap hear `first` before any `then`?
inline bool heard_before(const AnswerTap& tap, const char* first, const char* then) {
    const auto a = std::find(tap.heard.begin(), tap.heard.end(), first);
    const auto b = std::find(tap.heard.begin(), tap.heard.end(), then);
    return a != tap.heard.end() && (b == tap.heard.end() || a < b);
}

/// The value column of a painted property row (the mark, then the nine-cell label column).
inline std::string value_of(const std::string& property_row) {
    return property_row.size() > 10 ? property_row.substr(10) : std::string();
}

/// The Info pane's weave, for a tap on what reaches it.
inline loom::WeaveId info_id(InfoRig& f) {
    const loom::WeaveId id = f.r.kernel.weave_id(pane::kInfoPaneStem);
    REQUIRE(id.value != 0);
    return id;
}

/// One object of the host's document, by identity.
inline const ui::Element& object_of(InfoRig& f, std::int64_t id) {
    for (const ui::Element& e : f.r.w->document().elements) {
        if (e.id == id) {
            return e;
        }
    }
    FAIL("the document holds no #", id);
    return f.r.w->document().elements.front();
}

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
// A notice, and the act that spends it
// ============================================================================
//
// A PANE'S NOTICE STANDS UNTIL THE MAKER'S NEXT ACT, AND SPENT MEANS GONE FROM THE ROWS WORKSHOP
// HOLDS (`agents/panes.md`). Both halves are asked of what Workshop admitted and painted, never
// of the pane: a notice cleared in private stands painted until some unrelated grant says the
// rows again, so every "it stands" below is read again after a new room.

TEST_CASE("a press on an Info row while a notice stands names the row painted there, and a full room keeps both controls under the notice") {
    // THE NOTICE TAKES THE FIRST ROW AND EVERY ROW UNDER IT MOVES DOWN ONE. The pane composes its
    // body without the notice and `finish` puts it in front -- but the row map counted the notice
    // the other way, and the body was padded as though the notice were already in it. So while a
    // notice stood, a press landed two rows below the row it named, a blank row sat under the
    // object list, and a full room lost `[ Delete ]` off its end.
    InfoRig f;
    f.open(160, 24);
    for (int i = 0; i < 24; ++i) {
        make_object(f);
    }
    f.focus();
    const std::string label = f.picture().properties.front().label;
    f.r.key(input::scan::kReturn); // `info.edit` on the row the workspace makes: the pane refuses
    REQUIRE(f.row_of(label) == 0);

    // A FULL ROOM: every granted row is published, and the two controls are still its last two.
    const std::vector<std::string> rows = f.shown();
    const ExternalPane* seat = f.r.session().panels.external_pane(f.kind);
    REQUIRE(seat != nullptr);
    REQUIRE(static_cast<std::int64_t>(rows.size()) == seat->rows);
    CHECK(rows[rows.size() - 2].rfind("[ Create ]", 0) == 0);
    CHECK(rows[rows.size() - 1].rfind("[ Delete ]", 0) == 0);
    // ...AND NOTHING BLANK UNDER THE OBJECT LIST: the row above the properties is an object.
    const std::int64_t properties = f.row_of("PROPERTIES");
    REQUIRE(properties > 1);
    CHECK(rows[static_cast<std::size_t>(properties - 1)].rfind("> #", 0) == 0);

    // A PRESS ON A PROPERTY PAINTED UNDER THE NOTICE PUTS THE CURSOR ON THAT PROPERTY.
    const std::string second = f.picture().properties[1].label;
    f.press_row(" " + second);
    CHECK(f.row_of(">" + second) >= 0);
    CHECK(f.row_of(label) == -1); // ...and it was an act, so the notice is spent

    // A PRESS ON AN OBJECT PAINTED UNDER A NOTICE SELECTS THAT OBJECT.
    f.r.key(input::scan::kUp);
    f.r.key(input::scan::kReturn);
    REQUIRE(f.row_of(label) == 0);
    const std::int64_t before = f.r.session().selected;
    std::int64_t other = 0;
    for (const std::string& one : f.shown()) {
        if (one.rfind("  #", 0) == 0) {
            other = std::stoll(one.substr(3));
            break;
        }
    }
    REQUIRE(other != 0);
    REQUIRE(other != before);
    f.press_row("  #" + std::to_string(other));
    CHECK(f.r.session().selected == other);
    CHECK(f.row_of("> #" + std::to_string(other)) >= 0);
}

TEST_CASE("a key the Info draft line does not take is no act: the notice stands through a new room, and a key it takes spends it") {
    // THE DRAFT'S LINE REFUSES A KEY IT HAS NO MEANING FOR, and the pane cleared its notice
    // before it asked the line -- so the refusal stood painted over a private clear, and the next
    // room grant said the rows without a sentence the maker had done nothing to.
    InfoRig f;
    f.open();
    f.focus();
    f.go_to_first_editable();
    f.r.key(input::scan::kReturn); // a draft on an authored row
    REQUIRE(f.declared() == std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});
    f.r.text("77");
    f.press_row("( Create )"); // the pane's own refusal, beside the live draft
    REQUIRE(f.row_of("finish the edit") == 0);
    const std::string drafted = f.row_containing("77");
    REQUIRE_FALSE(drafted.empty());

    f.r.key(input::scan::kDown); // a key the line has no meaning for
    CHECK(f.row_of("finish the edit") == 0);
    CHECK(f.row_containing("77") == drafted); // the same room, so the same row, byte for byte
    f.regrant();
    CHECK(f.row_of("finish the edit") == 0);
    // ...AND THE DRAFT IS THE DRAFT IT WAS: still open, its text where the maker left it.
    CHECK(f.declared() == std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});
    CHECK_FALSE(f.row_containing("77").empty());

    f.r.key(input::scan::kLeft); // a key the line takes
    CHECK(f.row_of("finish the edit") == -1);
    CHECK_FALSE(f.row_containing("77").empty());
}

TEST_CASE("an id the Info pane does not answer to in its mode is no act: a commit resolved before a cancel, and an id nobody declared, leave the notice standing through a new room") {
    // THE MODE OWNS THE PANE'S ACTIONS, AND A DECLARATION RACES A KEYSTROKE. A maker who presses
    // Escape and Return in one poll gets both resolved against the rows the draft declared --
    // `info.cancel`, then `info.commit` -- and the pane hears the commit after the cancel closed
    // the draft: a stale id, delivered by Workshop under its own office. The pane cleared its
    // notice before it asked what the id meant here, so the cancel's own sentence stood over a
    // private clear.
    InfoRig f;
    f.open();
    f.focus();
    const std::size_t editable = f.go_to_first_editable();
    const std::string authored = f.picture().properties[editable].value;
    f.r.key(input::scan::kReturn);
    f.r.text("77");
    const std::vector<std::string> resting{pane::kActionDown, pane::kActionEdit, pane::kActionUp};

    f.enqueue_key(input::scan::kEscape);
    f.enqueue_key(input::scan::kReturn);
    f.settle();
    REQUIRE(f.row_of("edit cancelled") == 0);
    CHECK(f.declared() == resting);
    f.regrant();
    CHECK(f.row_of("edit cancelled") == 0);

    // AN ID NOBODY DECLARED, SAID BY WORKSHOP'S OWN OFFICE, is the same non-act -- asked of a
    // notice of its own, so it cannot pass or fail on what the stale commit did.
    f.r.key(input::scan::kReturn);
    f.r.key(input::scan::kEscape);
    REQUIRE(f.row_of("edit cancelled") == 0);
    const PaneRig::OfficeAction unknown =
        f.r.workshop_action(pane::kInfoPaneRole, pane::kInfoPane, "info.no-such-action");
    REQUIRE(unknown.authored);
    REQUIRE(unknown.delivered);
    CHECK(unknown.author == kWorkshopProvider);
    f.regrant();
    CHECK(f.row_of("edit cancelled") == 0);
    // ...AND NEITHER ONE WROTE ANYTHING: the value the maker typed never reached the document.
    CHECK(f.picture().properties[editable].value == authored);

    // THE SAME DOOR WITH AN ID THE PANE DOES ANSWER TO IS AN ACT -- which is what shows the
    // provenance was never the reason for the silence above.
    const auto cursor_row = [&f] { // the property row wearing the mark, not the selected object's
        for (const std::string& one : f.shown()) {
            if (one.rfind(">", 0) == 0 && one.rfind("> #", 0) != 0) {
                return one;
            }
        }
        return std::string();
    };
    const std::string cursor_was = cursor_row();
    REQUIRE_FALSE(cursor_was.empty());
    const PaneRig::OfficeAction down =
        f.r.workshop_action(pane::kInfoPaneRole, pane::kInfoPane, pane::kActionDown);
    REQUIRE(down.delivered);
    CHECK(f.row_of("edit cancelled") == -1);
    CHECK(cursor_row() != cursor_was); // the mark is on the next property now
}

TEST_CASE("a press on the Info object already selected spends the notice in the rows Workshop holds while its answer is still on its way, and needs no new document picture to say so") {
    // AN ACCEPTED ACT THAT CHANGES NO PICTURE. The press asks the host to select what is
    // selected; the host answers yes and has nothing new to publish, so no `DocumentShown` comes
    // to say the pane's rows again. The pane cleared its notice at the press and said nothing of
    // its own, so the refusal it had spent stood painted after the conversation was over.
    InfoRig f;
    f.open();
    f.focus();
    const DocumentShown said = f.picture();
    REQUIRE_FALSE(said.properties.front().editable);
    const std::string label = said.properties.front().label;
    f.r.key(input::scan::kReturn); // `info.edit` on a row the workspace makes: the pane refuses
    REQUIRE(f.row_of(label) == 0);
    const std::int64_t selected = f.r.session().selected;
    const std::size_t documents = f.r.said_documents.size();
    const std::int64_t at = f.row_of("> #" + std::to_string(selected));
    REQUIRE(at > 0);

    const loom::WeaveId pane_id = f.r.kernel.weave_id(pane::kInfoPaneStem);
    REQUIRE(pane_id.value != 0);
    loom::Switchboard& bus = f.r.bus;
    bool pressed = false;
    bool answered = false;
    const loom::ObserverId tap =
        bus.add_observer([&pressed, &answered, &bus, pane_id](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered || ev.target != pane_id) {
                return;
            }
            if (!pressed && ev.schema_name == PanePressed::zen_name) {
                pressed = true; // the turn ends where the pane has acted: its ask is queued
                bus.stop();
            } else if (ev.schema_name == DocumentActed::zen_name) {
                answered = true;
            }
        });
    f.enqueue_press(at);
    for (int turns = 0; turns < 8 && !pressed; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(pressed);
    REQUIRE_FALSE(answered);
    // TURN BY TURN, until the rows Workshop holds stop saying the refusal -- which must happen
    // while the host's answer is still on its way to the pane.
    const auto leads_with_it = [&f, &label] {
        const std::vector<std::string> rows = f.admitted();
        return !rows.empty() && rows.front().rfind(label, 0) == 0;
    };
    for (int turns = 0; turns < 16 && leads_with_it() && !answered; ++turns) {
        (void)bus.pump_pending();
    }
    CHECK_FALSE(answered);
    CHECK_FALSE(leads_with_it());
    bus.drain_until_idle();
    bus.remove_observer(tap);

    // THE CONVERSATION IS OVER: accepted, the selection where it was, and no picture was said.
    CHECK(answered);
    CHECK(f.r.session().selected == selected);
    CHECK(f.r.said_documents.size() == documents);
    CHECK(f.row_of(label) == -1);
    f.regrant();
    CHECK(f.row_of(label) == -1);

    // A PRESS STILL MEANS WHAT THE PICTURE SAYS: the rows moved up when the notice left, and the
    // row the next press reads is the object painted there.
    const std::int64_t other = said.objects.back().identity;
    REQUIRE(other != selected);
    f.press_row("  #" + std::to_string(other));
    CHECK(f.r.session().selected == other);
    CHECK(f.row_of("> #" + std::to_string(other)) >= 0);
}

TEST_CASE("an Info act with nothing to act on still spends the notice before it, and a refusal the document gives a press stands until the act after it") {
    // TWO HALVES OF ONE RULE. `info.edit` over an empty inspector is a declared id in the mode
    // the pane is in -- an act that happens to change nothing, like `info.up` on the first row --
    // so it spends the notice, and says the rows because nothing else will. And the document's
    // own refusal, the answer to a press, is a NEW notice: the rows said at the press must not be
    // the last word, and the next room must not take it back.
    InfoRig f;
    f.open();
    f.focus();
    f.go_to_first_editable();
    f.r.key(input::scan::kReturn);
    f.r.key(input::scan::kEscape); // a standing notice that is not the document's
    REQUIRE(f.row_of("edit cancelled") == 0);
    while (!f.r.w->document().elements.empty()) {
        f.unfocus();
        f.r.key(input::scan::kD);
    }
    f.focus(); // the OBJECTS heading: a row that means nothing, so nothing is spent
    REQUIRE(f.row_of("edit cancelled") == 0);
    REQUIRE(f.text().find("(nothing selected)") != std::string::npos);

    // A PRESS ON `( Delete )` WITH NOTHING TO DELETE: the pane asks, the document refuses.
    f.press_row("( Delete )");
    CHECK(f.row_of("edit cancelled") == -1);
    const std::vector<std::string> refused = f.shown();
    REQUIRE_FALSE(refused.empty());
    const std::string refusal = refused.front();
    CHECK(refusal.rfind("( Delete )", 0) != 0);
    CHECK(refusal.rfind("OBJECTS", 0) != 0);
    f.regrant();
    CHECK(f.shown().front().rfind(refusal.substr(0, 12), 0) == 0);

    // `info.edit` WITH NOTHING SELECTED: declared, applicable, and a no-op -- the refusal is
    // spent.
    f.r.key(input::scan::kReturn);
    CHECK(f.shown().front().rfind(refusal.substr(0, 12), 0) != 0);
    CHECK(f.row_of("OBJECTS") == 0);

    // ...AND THE NEXT PRESS READS THE PICTURE THAT SAYS SO: `[ Create ]` is where it is painted.
    const std::size_t born = f.r.w->document().elements.size();
    f.press_row("[ Create ]");
    CHECK(f.r.w->document().elements.size() == born + 1);
}

// ============================================================================
// A draft, a request and an answer
// ============================================================================
//
// THREE LIFETIMES THAT USED TO BE READ AS ONE. A draft is the pane's and ends when the maker ends
// it or when the picture stops showing its property; a request is the document's once it is sent,
// and closing a draft does not take it back; an answer belongs to the act that asked, and for a
// commit to the draft that sent it. Every case reads the document itself beside the rows Workshop
// admitted and painted, because a sentence that claims less than happened is the defect here.

TEST_CASE("a press on an object while an Info draft is live is refused in the controls' words, keeping the draft, its text, the selection and the document through a new room, and selecting resumes once the draft ends") {
    // AN ACCEPTED SELECT CLOSED THE DRAFT. The press asked the document to select the object
    // already selected, the document said yes, and the answer closed whatever draft was open --
    // the maker's typed text gone, with nothing said. A select is the one act that changes the
    // rows a draft is typed into, so while one is live the pane refuses it before asking, exactly
    // as it refuses Create and Delete.
    InfoRig f;
    f.open();
    const std::int64_t selected = f.r.session().selected;
    std::int64_t other = 0;
    for (const ui::Element& e : f.r.w->document().elements) {
        other = e.id != selected ? e.id : other;
    }
    REQUIRE(other != 0);
    const std::string authored = object_of(f, selected).label;
    f.draft_on("Name");
    f.r.text("77");
    REQUIRE(f.property_row("Name").find(authored + "77") != std::string::npos);
    const std::size_t documents = f.r.said_documents.size();
    const std::vector<std::string> draft_ids{pane::kActionCancel, pane::kActionCommit};
    const auto draft_stands = [&] {
        CHECK(f.declared() == draft_ids);
        CHECK(f.property_row("Name").find(authored + "77") != std::string::npos);
        CHECK(f.r.session().selected == selected);
        CHECK(object_of(f, selected).label == authored);
    };

    // THE OBJECT ALREADY SELECTED: refused, and nothing was asked of the document.
    f.press_row("> #" + std::to_string(selected));
    CHECK(f.row_of("finish the edit first") == 0);
    CHECK(f.admitted_leads("finish the edit first"));
    draft_stands();
    CHECK(f.r.said_documents.size() == documents);

    // ANOTHER OBJECT, PAINTED UNDER THE REFUSAL: refused the same way.
    f.press_row("  #" + std::to_string(other));
    CHECK(f.row_of("finish the edit first") == 0);
    draft_stands();

    // A NEW ROOM IS NOT AN ACT, AND KEEPS THE DRAFT.
    f.regrant();
    CHECK(f.row_of("finish the edit first") == 0);
    draft_stands();

    // THE DRAFT ENDS, NOTHING WAS SENT, AND A PRESS SELECTS AGAIN -- on the object painted under
    // the cancel's own sentence.
    f.r.key(input::scan::kEscape);
    REQUIRE(f.row_of("edit cancelled -- unwri") == 0);
    f.press_row("  #" + std::to_string(other));
    CHECK(f.r.session().selected == other);
    CHECK(f.row_of("> #" + std::to_string(other)) >= 0);
    CHECK(object_of(f, selected).label == authored);
}

TEST_CASE("a commit and a cancel resolved in one poll: the cancel says the commit was already sent, the answer's account takes that sentence's place, and no row says nothing was written over a write") {
    // ESCAPE ENDS THE DRAFT, AND IT CANNOT END A COMMIT THAT HAS LEFT. Return and Escape in one
    // poll resolve to a commit and then a cancel; the commit is on its way to the document before
    // the cancel closes the draft. The pane said `edit cancelled -- nothing was written` -- and
    // then either let the document's refusal replace it over a draft that was no longer there, or
    // kept it standing over the value the document had just taken.
    const auto commit_then_cancel = [](InfoRig& f, const std::string& typed) {
        f.draft_holding("X", typed);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_key(input::scan::kEscape);
        // TURN BY TURN: the rows Workshop holds say the commit was sent before its answer
        // arrives.
        for (int turns = 0; turns < 16 && !f.admitted_leads("commit already sent") &&
                            tap.answered == 0;
             ++turns) {
            (void)f.r.bus.pump_pending();
        }
        CHECK(tap.answered == 0);
        CHECK(f.admitted_leads("commit already sent"));
        f.settle();
        CHECK(tap.answered == 1);
        CHECK(f.declared() ==
              std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
    };
    const auto nothing_claims_it_was_not_written = [](InfoRig& f) {
        std::vector<std::string> rows = f.shown();
        const std::vector<std::string> held = f.admitted();
        rows.insert(rows.end(), held.begin(), held.end());
        for (const std::string& one : rows) {
            INFO("row: ", one);
            CHECK(one.find("nothing was") == std::string::npos);
            CHECK(one.find("cancelled") == std::string::npos);
        }
    };

    {
        // REFUSED: the value stands, and the refusal is the commit's, not a closed field's.
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        const std::int64_t authored = object_of(f, selected).x;
        commit_then_cancel(f, "abc");
        CHECK_MESSAGE(f.row_of("commit refused -- X: no") == 0, f.text());
        CHECK(object_of(f, selected).x == authored);
        CHECK(f.picture().properties[f.property_index("X")].value == std::to_string(authored));
        f.regrant();
        CHECK(f.row_of("commit refused") == 0);
    }
    {
        // TAKEN: the document holds the value, the pane says the commit was written, and the
        // host's own band says what it committed.
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        commit_then_cancel(f, "77");
        CHECK(object_of(f, selected).x == 77);
        CHECK(f.picture().properties[f.property_index("X")].value == "77");
        CHECK(f.row_of("commit written") == 0);
        CHECK(f.r.last_notice().find("committed X = 77") != std::string::npos);
        nothing_claims_it_was_not_written(f);
        f.regrant();
        CHECK(f.row_of("commit written") == 0);
        nothing_claims_it_was_not_written(f);
    }
}

TEST_CASE("an Info commit answered after a newer draft opened on the same field closes, alters and marks nothing on that draft, and a sentence a later act said stands") {
    // AN OLD ANSWER IS NOT THE NEW DRAFT'S. A commit, a cancel and a new edit delivered in one
    // burst, then a press on `( Create )`: the document answers the first draft's commit while
    // the second draft is open on the same property. The correlation names the request; only the
    // draft that sent it can be closed or marked by its answer.
    const auto late_answer = [](const std::string& typed, std::int64_t expected_x) {
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        const std::string opened_with = std::to_string(object_of(f, selected).x);
        f.draft_holding("X", typed);
        const std::int64_t create = f.row_of("( Create )");
        REQUIRE(create > 0);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_action(pane::kActionCommit);
        f.enqueue_action(pane::kActionCancel);
        f.enqueue_action(pane::kActionEdit);
        f.enqueue_press(create);
        f.settle();
        REQUIRE(tap.answered == 1);

        const auto newer_draft_stands = [&] {
            CHECK(f.declared() ==
                  std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});
            CHECK(f.row_of("finish the edit first") == 0);
            const std::string drafted = f.property_row("X");
            REQUIRE_FALSE(drafted.empty());
            CHECK(drafted.rfind(">X", 0) == 0);
            CHECK(drafted.substr(10).rfind(opened_with, 0) == 0);
            for (const std::string& one : f.shown()) {
                INFO("row: ", one);
                CHECK(one.find("commit written") == std::string::npos);
                CHECK(one.find("commit refused") == std::string::npos);
                CHECK(one.find("commit already") == std::string::npos);
                CHECK(one.find("not a whole") == std::string::npos);
            }
            CHECK(object_of(f, selected).x == expected_x);
        };
        newer_draft_stands();
        f.regrant();
        newer_draft_stands();

        // ...AND THE NEWER DRAFT'S OWN CANCEL IS TRUE ABOUT IT: it sent nothing.
        f.r.key(input::scan::kEscape);
        CHECK(f.row_of("edit cancelled -- unwri") == 0);
        CHECK(object_of(f, selected).x == expected_x);
    };
    SUBCASE("refused") { late_answer("abc", 3); }
    SUBCASE("taken") { late_answer("77", 77); }
}

TEST_CASE("an Info draft ended while an earlier draft's commit is still unanswered says the commit was already sent, even when an act between them asked the document something else, and that commit's account replaces the sentence") {
    // WHETHER A COMMIT IS UNANSWERED IS WHAT DECIDES WHAT ENDING A DRAFT MAY SAY. A commit and
    // its cancel, a press on `[ Create ]`, a new draft and its cancel, delivered in one burst:
    // the create is asked while the first commit is still on its way, and the second cancel must
    // not say nothing was written over the write that commit is about to make.
    InfoRig f;
    f.open();
    const std::int64_t selected = f.r.session().selected;
    f.draft_holding("X", "77");
    const std::int64_t create = f.row_of("( Create )");
    REQUIRE(create > 0);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_action(pane::kActionCommit);
    f.enqueue_action(pane::kActionCancel); // the first draft ends, its commit unanswered
    // ...and `[ Create ]` is pressed on the rows that cancel said, under its one-row sentence.
    f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, create + 1, 2}));
    f.enqueue_action(pane::kActionEdit);
    f.enqueue_action(pane::kActionCancel);
    f.settle();
    REQUIRE(tap.answered == 2);
    CHECK(f.r.w->document().elements.size() == 3); // the create was asked and taken
    CHECK(object_of(f, selected).x == 77);
    CHECK(f.row_of("commit written") == 0);
    for (const std::string& one : f.shown()) {
        INFO("row: ", one);
        CHECK(one.find("cancelled") == std::string::npos);
    }
    f.regrant();
    CHECK(f.row_of("commit written") == 0);
}

TEST_CASE("a select asked before an Info draft opened, and answered while it is open, closes nothing") {
    // AN ACCEPTED ACT THAT IS NOT THE DRAFT'S IS NOT THE DRAFT'S END. A press on the object
    // already selected and `info.edit`, delivered in one burst: the select is answered after the
    // draft has opened, and an accepted answer used to close whatever draft was open.
    InfoRig f;
    f.open();
    f.focus();
    const std::int64_t selected = f.r.session().selected;
    const std::size_t name = f.property_index("Name");
    for (std::size_t step = 0; step < name; ++step) {
        f.r.key(input::scan::kDown);
    }
    const std::int64_t at = f.row_of("> #" + std::to_string(selected));
    REQUIRE(at > 0);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, at, 2}));
    f.enqueue_action(pane::kActionEdit);
    f.settle();
    REQUIRE(tap.answered == 1);
    CHECK(f.r.session().selected == selected);
    CHECK(f.declared() == std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});
    f.r.text("77");
    CHECK(f.property_row("Name").find(object_of(f, selected).label + "77") != std::string::npos);
}

TEST_CASE("a picture that selects another object abandons the Info draft and says so, even where that object has the same property on the same row, and writes nothing into either object") {
    // THE SUBJECT IS THE OBJECT AND ITS PROPERTY. A draft was abandoned only when its row was
    // gone or renamed -- but every object has `Name` on the same row, so a selection moved by a
    // gesture the pane never saw kept the draft open over another object, and its commit wrote
    // the maker's text into that one.
    InfoRig f;
    f.open();
    const std::int64_t first = f.r.session().selected;
    const std::string authored = object_of(f, first).label;
    const std::size_t name = f.property_index("Name");
    f.draft_on("Name");
    f.r.text("77");
    REQUIRE(f.property_row("Name").find(authored + "77") != std::string::npos);

    make_object(f); // `n` on the workspace: a new object, selected, and a picture of it
    const std::int64_t made = f.r.w->document().elements.back().id;
    REQUIRE(f.r.session().selected == made);
    const std::string made_name = object_of(f, made).label;
    REQUIRE(f.picture().properties[name].label == "Name"); // the same property on the same row

    CHECK(f.declared() ==
          std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
    CHECK_MESSAGE(f.row_of("edit abandoned -- the p") == 0, f.text());
    CHECK(f.row_containing(authored + "77").empty());
    CHECK(object_of(f, first).label == authored);

    // A COMMIT CANNOT FOLLOW IT THERE: the pane no longer answers to one, and neither object
    // moves.
    const PaneRig::OfficeAction commit =
        f.r.workshop_action(pane::kInfoPaneRole, pane::kInfoPane, pane::kActionCommit);
    REQUIRE(commit.delivered);
    CHECK(object_of(f, made).label == made_name);
    CHECK(object_of(f, first).label == authored);
    CHECK(f.row_of("edit abandoned") == 0);
}

TEST_CASE("a clipboard answer asked for by an Info draft that has closed lands in no later draft, and one asked for by the draft still standing lands in it") {
    // A PASTE BELONGS TO THE DRAFT THAT ASKED (the text-box register's paste law). The pane
    // checked only that SOME draft was open when the clipboard answered, so a paste asked for in
    // a draft that was then cancelled landed in the next draft opened before the answer came.
    InfoRig f;
    f.open();
    SkinSeat* skin = f.r.mount_skin_seat();
    REQUIRE(skin != nullptr);
    skin->platform = "PASTED";
    const std::int64_t selected = f.r.session().selected;
    const std::string authored = object_of(f, selected).label;
    f.draft_on("Name");

    // THE DRAFT THAT ASKED STILL STANDS: the text lands in it.
    f.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == 1);
    CHECK(f.property_row("Name").find(authored + "PASTED") != std::string::npos);

    // ASKED, THEN THAT DRAFT CANCELLED AND ANOTHER OPENED ON THE SAME PROPERTY BEFORE THE ANSWER.
    f.enqueue_as_workshop(
        loom::to_value(PaneKey{pane::kInfoPane, input::scan::kV, input::mod::kCtrl}));
    f.enqueue_action(pane::kActionCancel);
    f.enqueue_action(pane::kActionEdit);
    f.settle();
    CHECK(skin->clipboard_reads == 2); // the read really happened, so the absence is measured
    CHECK(f.declared() == std::vector<std::string>{pane::kActionCancel, pane::kActionCommit});
    const std::string drafted = f.property_row("Name");
    CHECK(drafted.find(authored) != std::string::npos);
    CHECK(drafted.find("PASTED") == std::string::npos);
    CHECK(object_of(f, selected).label == authored);
}

// ONE COMMIT OUTSTANDING, AND WHAT IT SENT. A commit is the document's once it has left, and until
// it is answered the pane sends no other: a second commit is declined aloud and the draft goes on
// being edited. The answer is read against what the commit SENT as well as the draft that sent it,
// because typing after Return is an edit that no write covers. Every burst below is one poll of
// ordinary input where ordinary input can make it, and Workshop's office door where it cannot.

/// Every row Workshop admitted for the pane and every row it painted, for a claim about both.
inline std::vector<std::string> admitted_and_painted(InfoRig& f) {
    std::vector<std::string> rows = f.shown();
    const std::vector<std::string> held = f.admitted();
    rows.insert(rows.end(), held.begin(), held.end());
    return rows;
}

TEST_CASE("a second Info commit in the same poll as the first is not sent: the pane says so, keeps the text typed between them, sends that text once the first is answered, and a cancel after the write and a refused retry claims no write away") {
    // ONE COMMIT HID ANOTHER. Return, Ctrl+A, `abc` and Return in one poll over a draft holding
    // `77`: both commits left, the second replaced the first's record, so the answer that wrote 77
    // was read as nobody's and the second's refusal was said over the draft -- and Escape then said
    // nothing was written, over the 77 the document held.
    InfoRig f;
    f.open();
    const std::int64_t selected = f.r.session().selected;
    f.draft_holding("X", "77");
    const std::vector<std::string> draft_ids{pane::kActionCancel, pane::kActionCommit};
    DocumentAskTap asks(f.r.bus, f.r.workshop_id);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_key(input::scan::kReturn);
    f.enqueue_key(input::scan::kA, input::mod::kCtrl);
    f.enqueue_text("abc");
    f.enqueue_key(input::scan::kReturn);
    // TURN BY TURN: the rows Workshop holds say the second commit was not sent, while the first is
    // still unanswered.
    for (int turns = 0; turns < 16 && !f.admitted_leads("commit not sent") && tap.answered == 0;
         ++turns) {
        (void)f.r.bus.pump_pending();
    }
    CHECK(tap.answered == 0);
    CHECK(f.admitted_leads("commit not sent"));
    f.settle();

    // ONE COMMIT LEFT, WITH WHAT THE DRAFT HELD AT ITS RETURN, AND THE DOCUMENT TOOK IT.
    CHECK(asks.commits == std::vector<std::string>{"77"});
    CHECK(tap.answered == 1);
    CHECK(object_of(f, selected).x == 77);
    CHECK(f.picture().properties[f.property_index("X")].value == "77");
    const auto kept = [&] {
        CHECK(f.declared() == draft_ids);
        CHECK(value_of(f.property_row("X")) == "abc");
        CHECK_MESSAGE(f.row_of("earlier commit written") == 0, f.text());
        CHECK(f.admitted_leads("earlier commit written"));
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("commit not sent") == std::string::npos);
        }
    };
    kept();
    f.regrant();
    kept();

    // THE KEPT TEXT IS SENT ONCE THE FIRST IS ANSWERED -- refused in the document's own words,
    // because it is exactly what the draft still holds.
    f.r.key(input::scan::kReturn);
    CHECK(asks.commits == std::vector<std::string>{"77", "abc"});
    CHECK(tap.answered == 2);
    CHECK_MESSAGE(f.row_of("X: not a whole") == 0, f.text());
    CHECK(f.declared() == draft_ids);
    CHECK(object_of(f, selected).x == 77);

    // ESCAPE ENDS THE DRAFT, DISCARDS WHAT WAS NEVER WRITTEN, AND TAKES NO WRITE AWAY IN WORDS.
    f.r.key(input::scan::kEscape);
    const auto cancelled = [&] {
        CHECK_MESSAGE(f.row_of("edit cancelled -- unwri") == 0, f.text());
        CHECK(f.admitted_leads("edit cancelled -- unwri"));
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("nothing was") == std::string::npos);
        }
        CHECK(f.declared() ==
              std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
        CHECK(object_of(f, selected).x == 77);
        CHECK(f.picture().properties[f.property_index("X")].value == "77");
    };
    cancelled();
    f.regrant();
    cancelled();
}

TEST_CASE("Return twice over an unchanged Info draft sends one commit, says the second was not sent while the first is unanswered, and the first's acceptance closes the draft and retires that sentence") {
    // AN UNCHANGED DRAFT STILL CLOSES ON ITS ACCEPTANCE. The declined Return lost nothing -- the
    // draft held what the first commit sent -- so the answer ends the draft as any accepted commit
    // does, and the sentence about the pending commit goes with it.
    InfoRig f;
    f.open();
    const std::int64_t selected = f.r.session().selected;
    f.draft_holding("X", "77");
    DocumentAskTap asks(f.r.bus, f.r.workshop_id);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_key(input::scan::kReturn);
    f.enqueue_key(input::scan::kReturn);
    for (int turns = 0; turns < 16 && !f.admitted_leads("commit not sent") && tap.answered == 0;
         ++turns) {
        (void)f.r.bus.pump_pending();
    }
    CHECK(tap.answered == 0);
    CHECK(f.admitted_leads("commit not sent"));
    f.settle();

    CHECK(asks.commits == std::vector<std::string>{"77"});
    CHECK(tap.answered == 1);
    CHECK(object_of(f, selected).x == 77);
    CHECK(f.r.last_notice().find("committed X = 77") != std::string::npos);
    const auto closed = [&] {
        CHECK(f.declared() ==
              std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
        CHECK(value_of(f.property_row("X")) == "77");
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("commit not sent") == std::string::npos);
            CHECK(one.find("earlier commit") == std::string::npos);
        }
    };
    closed();
    f.regrant();
    closed();
}

TEST_CASE("text typed after an Info commit was sent outlives that commit's answer: the draft stays open with its history, the write is told apart from the unsent text, a refusal is not said of the newer text, and the newer text commits normally") {
    // AN ACCEPTED WRITE DOES NOT COVER WHAT WAS TYPED AFTER IT WAS SENT. Return and `8` in one poll
    // over a draft holding `77`: the typing reached the pane before the answer, the document
    // correctly took 77, and the answer closed the draft the 8 had been typed into.
    const std::vector<std::string> draft_ids{pane::kActionCancel, pane::kActionCommit};
    const std::vector<std::string> resting{pane::kActionDown, pane::kActionEdit, pane::kActionUp};

    SUBCASE("taken") {
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        f.draft_holding("X", "77");
        DocumentAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_text("8");
        f.settle();
        REQUIRE(tap.answered == 1);
        CHECK(heard_before(tap, PaneTextInput::zen_name, DocumentActed::zen_name));
        CHECK(asks.commits == std::vector<std::string>{"77"});
        CHECK(object_of(f, selected).x == 77);
        const auto kept = [&] {
            CHECK(f.declared() == draft_ids);
            CHECK(value_of(f.property_row("X")) == "778");
            CHECK_MESSAGE(f.row_of("earlier commit written") == 0, f.text());
            CHECK(f.admitted_leads("earlier commit written"));
        };
        kept();
        f.regrant();
        kept();

        // THE LINE IS STILL THE LINE IT WAS: undo steps back over the typing, redo brings it back.
        f.r.key(input::scan::kZ, input::mod::kCtrl);
        CHECK(value_of(f.property_row("X")).empty());
        f.r.key(input::scan::kY, input::mod::kCtrl);
        CHECK(value_of(f.property_row("X")) == "778");

        // ...AND IT COMMITS LIKE ANY DRAFT: taken, unchanged since, so closed.
        f.r.key(input::scan::kReturn);
        CHECK(asks.commits == std::vector<std::string>{"77", "778"});
        CHECK(object_of(f, selected).x == 778);
        CHECK(f.declared() == resting);
        CHECK(value_of(f.property_row("X")) == "778");
    }
    SUBCASE("refused") {
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        const std::int64_t authored = object_of(f, selected).x;
        f.draft_holding("X", "abc");
        DocumentAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_key(input::scan::kA, input::mod::kCtrl);
        f.enqueue_text("12");
        f.settle();
        REQUIRE(tap.answered == 1);
        CHECK(heard_before(tap, PaneTextInput::zen_name, DocumentActed::zen_name));
        CHECK(asks.commits == std::vector<std::string>{"abc"});
        CHECK(object_of(f, selected).x == authored);
        const auto kept = [&] {
            CHECK(f.declared() == draft_ids);
            CHECK(value_of(f.property_row("X")) == "12");
            // THE REFUSAL IS THE COMMIT'S: said as the earlier commit's, never in the document's
            // bare words over a value it was not about.
            CHECK_MESSAGE(f.row_of("earlier commit refused") == 0, f.text());
            CHECK(f.row_of("X: not a whole") == -1);
        };
        kept();
        f.regrant();
        kept();

        f.r.key(input::scan::kReturn);
        CHECK(asks.commits == std::vector<std::string>{"abc", "12"});
        CHECK(object_of(f, selected).x == 12);
        CHECK(f.declared() == resting);
    }
    SUBCASE("abandoned") {
        // AND A DRAFT THAT OUTLIVED A WRITE, ABANDONED, TAKES NO WRITE AWAY IN WORDS EITHER -- read
        // in a window wide enough for the whole sentence, because the right column's 26 columns
        // cut it before the clause that says what was lost.
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        f.draft_holding("X", "77");
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_text("8");
        f.settle();
        REQUIRE(tap.answered == 1);
        REQUIRE(f.declared() == draft_ids);
        // THE MAKER'S OWN WINDOW FOR THE PANE, through the setup's authoring doors, and a new room.
        Setup& desk = f.r.session().setup.active;
        const Written placed = author_pane_place(desk, pane_info_ref(), subs(2), subs(3));
        REQUIRE_MESSAGE(placed.accepted, placed.refusal);
        const Written sized = author_pane_size(desk, pane_info_ref(),
                                               PaneSize{pane_unit::kSubcells, subs(120)},
                                               PaneSize{pane_unit::kSubcells, subs(30)});
        REQUIRE_MESSAGE(sized.accepted, sized.refusal);
        f.r.extent(150, 44);
        const ExternalPane* seat = f.r.session().panels.external_pane(f.kind);
        REQUIRE(seat != nullptr);
        REQUIRE(seat->columns > 90);
        REQUIRE(f.declared() == draft_ids); // a new room keeps the draft
        make_object(f); // `n` on the workspace: a new object, selected, and a picture of it
        REQUIRE(f.r.session().selected != selected);
        CHECK(f.declared() == resting);
        CHECK_MESSAGE(f.row_of("edit abandoned -- the property it was on is no longer shown; "
                               "unwritten changes discarded") == 0,
                      f.text());
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("nothing was") == std::string::npos);
        }
        CHECK(object_of(f, selected).x == 77);
    }
}

TEST_CASE("an Info commit's answer settles the sentence that said a second commit was not sent, and leaves a sentence a later act said standing") {
    // A SENTENCE ABOUT A PENDING COMMIT BELONGS TO THAT COMMIT, AND ONLY THAT ONE. Two commits and
    // a press on `( Create )` in one burst: the second commit is declined and the press says the
    // draft comes first. The first commit's answer must not leave `commit not sent` painted once it
    // has settled, and must not take the press's own sentence away. Workshop's office door places
    // the press on the rows the decline said, which a pointer queued in the same poll could not
    // name.
    const std::vector<std::string> draft_ids{pane::kActionCancel, pane::kActionCommit};
    const auto burst = [](InfoRig& f, bool typed_between) {
        const std::int64_t create = f.row_of("( Create )");
        REQUIRE(create > 0);
        f.enqueue_action(pane::kActionCommit);
        if (typed_between) {
            f.enqueue_as_workshop(loom::to_value(PaneTextInput{pane::kInfoPane, "8"}));
        }
        f.enqueue_action(pane::kActionCommit);
        f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, create + 1, 2}));
        f.settle();
    };
    const auto no_pending_sentence = [](InfoRig& f) {
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("commit not sent") == std::string::npos);
            CHECK(one.find("earlier commit") == std::string::npos);
        }
    };

    SUBCASE("unchanged since the commit: the answer closes the draft") {
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        f.draft_holding("X", "77");
        DocumentAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        burst(f, false);
        CHECK(asks.commits == std::vector<std::string>{"77"});
        CHECK(tap.answered == 1);
        CHECK(object_of(f, selected).x == 77);
        const auto stands = [&] {
            CHECK(f.declared() ==
                  std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
            CHECK_MESSAGE(f.row_of("finish the edit first") == 0, f.text());
            CHECK(f.admitted_leads("finish the edit first"));
            no_pending_sentence(f);
        };
        stands();
        f.regrant();
        stands();
    }
    SUBCASE("refused, unchanged since the commit: the draft stays, and the press's sentence stands") {
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        const std::int64_t authored = object_of(f, selected).x;
        f.draft_holding("X", "abc");
        DocumentAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        burst(f, false);
        CHECK(asks.commits == std::vector<std::string>{"abc"});
        CHECK(tap.answered == 1);
        CHECK(object_of(f, selected).x == authored);
        const auto stands = [&] {
            CHECK(f.declared() == draft_ids);
            CHECK(value_of(f.property_row("X")) == "abc");
            CHECK_MESSAGE(f.row_of("finish the edit first") == 0, f.text());
            CHECK(f.admitted_leads("finish the edit first"));
            no_pending_sentence(f);
        };
        stands();
        f.regrant();
        stands();
    }
    SUBCASE("typed between the two commits: the draft stays open with the newer text") {
        InfoRig f;
        f.open();
        const std::int64_t selected = f.r.session().selected;
        f.draft_holding("X", "77");
        DocumentAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        burst(f, true);
        CHECK(asks.commits == std::vector<std::string>{"77"});
        CHECK(tap.answered == 1);
        CHECK(object_of(f, selected).x == 77);
        const auto stands = [&] {
            CHECK(f.declared() == draft_ids);
            CHECK(value_of(f.property_row("X")) == "778");
            CHECK_MESSAGE(f.row_of("finish the edit first") == 0, f.text());
            CHECK(f.admitted_leads("finish the edit first"));
            no_pending_sentence(f);
        };
        stands();
        f.regrant();
        stands();
    }
}

TEST_CASE("a commit from a newer Info draft is not sent while an earlier draft's commit is unanswered, even with a select asked between them, and the earlier commit's account replaces that sentence without closing or altering the newer draft") {
    // ONE OUTSTANDING COMMIT IS THE PANE'S, NOT ONE DRAFT'S. A commit and its cancel, a press on
    // the object already selected (a select of its own), a newer draft on the same field and its
    // commit, delivered in one burst: the select must not hide the first commit, the newer commit
    // waits, and the account of the first is said over the newer draft as the earlier commit's.
    InfoRig f;
    f.open();
    const std::int64_t selected = f.r.session().selected;
    const std::string opened_with = std::to_string(object_of(f, selected).x);
    f.draft_holding("X", "77");
    const std::int64_t chosen = f.row_of("> #" + std::to_string(selected));
    REQUIRE(chosen > 0);
    const std::vector<std::string> draft_ids{pane::kActionCancel, pane::kActionCommit};
    DocumentAskTap asks(f.r.bus, f.r.workshop_id);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_action(pane::kActionCommit);
    f.enqueue_action(pane::kActionCancel); // the first draft ends, its commit unanswered
    // ...the object already selected is pressed on the rows that cancel said, under its sentence.
    f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, chosen + 1, 2}));
    f.enqueue_action(pane::kActionEdit);
    f.enqueue_action(pane::kActionCommit);
    f.settle();

    CHECK(asks.commits == std::vector<std::string>{"77"});
    CHECK(asks.others == 1);
    CHECK(tap.answered == 2);
    CHECK(object_of(f, selected).x == 77);
    const auto newer_stands = [&] {
        CHECK(f.declared() == draft_ids);
        CHECK(value_of(f.property_row("X")) == opened_with);
        CHECK_MESSAGE(f.row_of("earlier commit written") == 0, f.text());
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("commit not sent") == std::string::npos);
        }
    };
    newer_stands();
    f.regrant();
    newer_stands();

    // ...AND ONCE IT IS ANSWERED THE NEWER DRAFT SENDS ITS OWN TEXT.
    f.r.key(input::scan::kA, input::mod::kCtrl);
    f.r.text("12");
    f.r.key(input::scan::kReturn);
    CHECK(asks.commits == std::vector<std::string>{"77", "12"});
    CHECK(object_of(f, selected).x == 12);
    CHECK(f.declared() ==
          std::vector<std::string>{pane::kActionDown, pane::kActionEdit, pane::kActionUp});
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

