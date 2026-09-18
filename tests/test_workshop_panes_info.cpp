// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — WHAT IS TRUE RIGHT NOW, AS A LOADED WEAVE.
//
// THIS FILE OWNS the PANES and PROPERTIES columns. Everything the Info pane does a maker can
// see -- listing the panes, naming one as the subject, showing its rows, moving a cursor through
// them, opening a draft on a value, typing it, committing or cancelling it -- is driven here
// through the REAL `zengine-info-pane` image, over the REAL pane protocol, against the REAL
// publications this host makes. Nothing in this file constructs the weave, reaches into its
// state, or calls one of its functions: there is a shared library on disk, a plan row that loads
// it, an office it holds, and a maker's hand.
//
// ⚠ THE DESK ALREADY NAMES IT. This pane's reference is what `default_setup` authors
// (`workshop/setup.hpp`), so the office's offer RESOLVES A ROW THAT WAS ALREADY THERE and the
// pane opens with no gesture at all.
//
// ⚠ AND IT SHOWS THE HOST'S OWN ROWS. The pane is shown a PICTURE (`PaneSubjectShown`) of the
// rows this host keeps over one pane -- the pane THIS pane named -- and asks back through two
// shapes (`InspectPaneRequested`, `PaneCommitRequested`). So the cases drive the HOST -- a desk
// put live, the surface resized, a file restored -- and then read what the PANE made of it.
// Nothing in the image can touch a desk; it can ask, and be refused in the owner's words. The
// subject most cases edit is the Layouts pane's Width: a built-in on every desk, whose room
// moving does not move this pane's.
//
// ⚠ IT INSPECTED THE PROTOTYPE OBJECT DOCUMENT BEFORE THAT RETIRED. The draft discipline --
// one commit outstanding, an answer read against the draft and the text that sent it, a send
// Loom refused released aloud -- is the same law over a new subject, and its cases are here in
// the same order. Cases about objects, and about the two object controls, retired with them.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "info-pane/vocabulary.hpp"
#include "workshop/inspection_seam_vocabulary.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

namespace pane = zengine::info_pane;

/// The office and pane a saved setup names, spelled through the package's own header --
/// the durable names, not literals, so a case cannot agree with a typo.
inline PaneRef pane_info_ref() { return PaneRef{pane::kInfoPaneRole, pane::kInfoPane}; }

/// THE SUBJECT MOST CASES EDIT: the host's own Layouts pane, on every desk.
inline PaneRef layouts_ref() { return pane_ref_of(panel::kLayouts); }

const std::vector<std::string> kResting{pane::kActionDown, pane::kActionEdit, pane::kActionSwitch,
                                        pane::kActionUp};
const std::vector<std::string> kDrafting{pane::kActionCancel, pane::kActionCommit};

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

    /// PRESS INTO THE PANE: its rows are active only while it holds the keyboard. The first body
    /// row is the `PANES` heading, or a sentence in front of it -- a row that means nothing.
    void focus() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y);
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

    /// THE ROW ONE PANE OF THE LIST IS PAINTED ON: a cursor mark, a subject mark, the name.
    std::int64_t pane_row(const std::string& name) {
        const std::vector<std::string> rows = shown();
        const std::string want = name + " -- ";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].size() > 2 && rows[i].compare(2, want.size(), want) == 0) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }

    void press_pane_row(const std::string& name) {
        const std::int64_t at = pane_row(name);
        REQUIRE_MESSAGE(at >= 0, "no pane row for ", name, " in:\n", text());
        press_pane(r, kind, at, 3);
    }

    /// NAME A PANE AS THE SUBJECT the way a maker does: into the pane, a press on its row.
    void inspect(const std::string& name) {
        focus();
        press_pane_row(name);
        REQUIRE_MESSAGE(picture().name == name, text());
    }

    /// THE HOST'S LAST PICTURE OF THE SUBJECT -- what the pane was told, so a case asks the seam
    /// which rows are the maker's rather than counting on an order.
    PaneSubjectShown picture() {
        REQUIRE_FALSE(r.said_subjects.empty());
        return r.said_subjects.back();
    }

    /// WHERE A LABEL IS in the host's picture of the subject.
    std::size_t property_index(const std::string& label) {
        const PaneSubjectShown said = picture();
        for (std::size_t i = 0; i < said.properties.size(); ++i) {
            if (said.properties[i].label == label) {
                return i;
            }
        }
        FAIL("the subject has no property called ", label);
        return said.properties.size();
    }

    /// The painted row of one property, by its label: the mark cell, then the label column.
    std::int64_t property_at(const std::string& label) {
        const std::vector<std::string> rows = shown();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const std::string& one = rows[i];
            if (one.size() > 1 && one.compare(1, label.size() + 1, label + " ") == 0) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }
    std::string property_row(const std::string& label) {
        const std::int64_t at = property_at(label);
        return at < 0 ? std::string() : shown()[static_cast<std::size_t>(at)];
    }

    /// PUT THE KEYS AND THE CURSOR ON ONE PROPERTY, by a press on its painted row.
    void press_property(const std::string& label) {
        const std::int64_t at = property_at(label);
        REQUIRE_MESSAGE(at >= 0, "no property row for ", label, " in:\n", text());
        press_pane(r, kind, at, 3);
    }

    /// OPEN A DRAFT ON ONE PROPERTY OF `subject`, the way a maker does: the pane named, a press
    /// on the property's row, Return. The draft holds the property's value until the case types.
    void draft_on(const std::string& label, const std::string& subject = "Layouts") {
        if (r.said_subjects.empty() || picture().name != subject) {
            inspect(subject);
        } else {
            focus();
        }
        press_property(label);
        r.key(input::scan::kReturn);
        REQUIRE_MESSAGE(declared() == kDrafting, text());
    }

    /// ...AND REPLACE WHAT IT HOLDS with `typed`.
    void draft_holding(const std::string& label, const std::string& typed,
                       const std::string& subject = "Layouts") {
        draft_on(label, subject);
        for (int i = 0; i < 16; ++i) {
            r.key(input::scan::kBackspace);
        }
        r.text(typed);
    }

    /// A bare-letter gesture, as a backend really reports one: the key transition AND the
    /// character it produced.
    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
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
    /// exact interval of a conversation; `settle` drains. `r.key` and `r.text` drain, so a burst
    /// built from them is several polls rather than one.
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
    /// ...AND A PRESS ON THE BAND, queued: the keys leave the pane for command mode.
    void enqueue_unfocus() {
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::PointerButton{1, true, 0,
                                                screen_of(r.session()).h - 1 +
                                                    surface::kTuiCanvasTopRow,
                                                input::space::kCells, input::mod::kNone}),
            loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void settle() { r.bus.drain_until_idle(); }

    /// A SENTENCE SAID AS WORKSHOP'S OWN OFFICE, QUEUED AND NOT DRAINED -- `PaneRig::workshop_action`'s
    /// verified door, so a case can place several deliveries to the pane in one burst, in the
    /// order Workshop would send them, where a key could not: a key resolves against the rows
    /// Workshop holds, and those trail the pane's own re-declaration by a delivery.
    void enqueue_as_workshop(const loom::Value& said) {
        const loom::Ticket sent = r.bus.office_send_to_role_as(
            r.workshop_id, kWorkshopProvider, pane::kInfoPaneRole,
            loom::Message(said, r.workshop_id, r.workshop_id, 0));
        REQUIRE(sent.valid());
    }
    void enqueue_action(const char* id) {
        enqueue_as_workshop(loom::to_value(PaneActionRequested{pane::kInfoPane, id}));
    }

    /// Does the first row Workshop ADMITTED for this pane begin with `prefix`?
    bool admitted_leads(const std::string& prefix) {
        const std::vector<std::string> rows = admitted();
        return !rows.empty() && rows.front().rfind(prefix, 0) == 0;
    }

    /// WHAT THE LIVE DESK AUTHORS FOR ONE AXIS OF A PANE, as the owner's own row reads it.
    std::string axis(const PaneRef& ref, std::size_t which) {
        return pane_axis_text(r.session(), ref, which);
    }
    std::string layouts_width() { return axis(layouts_ref(), 2); }
};

/// HOW MANY OF THE HOST'S ANSWERS TO AN ASK HAVE REACHED THE PANE, read off the tap -- so a case
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
            if (ev.schema_name == PaneSubjectActed::zen_name) {
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

/// WHAT REACHED THE HOST'S DOORS, read off the tap at Workshop's delivery -- so a case counts the
/// commits that LEFT the pane, and the text each carried, not the ones it meant to send.
struct SubjectAskTap {
    loom::Switchboard& bus;
    loom::ObserverId tap{};
    std::vector<std::string> commits;   ///< each commit's text, in the order the door received them
    std::vector<std::int64_t> subjects; ///< ...and the name each one returned
    int others = 0;                     ///< an inspect
    SubjectAskTap(loom::Switchboard& on, loom::WeaveId door) : bus(on) {
        tap = bus.add_observer([this, door](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered || ev.target != door || ev.payload == nullptr) {
                return;
            }
            if (ev.schema_name == PaneCommitRequested::zen_name) {
                const PaneCommitRequested asked = loom::from_value<PaneCommitRequested>(*ev.payload);
                commits.push_back(asked.text);
                subjects.push_back(asked.subject);
            } else if (ev.schema_name == InspectPaneRequested::zen_name) {
                ++others;
            }
        });
    }
    ~SubjectAskTap() { bus.remove_observer(tap); }
    SubjectAskTap(const SubjectAskTap&) = delete;
    SubjectAskTap& operator=(const SubjectAskTap&) = delete;
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

/// Every row Workshop admitted for the pane and every row it painted, for a claim about both.
inline std::vector<std::string> admitted_and_painted(InfoRig& f) {
    std::vector<std::string> rows = f.shown();
    const std::vector<std::string> held = f.admitted();
    rows.insert(rows.end(), held.begin(), held.end());
    return rows;
}

/// WHAT ONE DESK OF THE RUN AUTHORS FOR THE LAYOUTS PANE'S WIDTH -- the live one or a shelved one.
inline bool layouts_width_default_on_every_desk(InfoRig& f) {
    const SetupState& run = f.r.session().setup;
    for (std::size_t at = 0; at < layout_count(run); ++at) {
        const SetupPane* row = pane_of(layout_at(run, at), layouts_ref());
        if (row == nullptr || row->width.mode != pane_unit::kDefault) {
            return false;
        }
    }
    return true;
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

    // THE DESK NAMED IT BEFORE THE OFFICE EXISTED: the row was authored by `default_setup`,
    // and the office's arrival RESOLVED it rather than adding it.
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
    // ⚠ AND IT IS NOT WHAT A MISSING ARTIFACT DOES. THIS rig loads no plan at all, which is the
    // state a maker reaches with a saved layout naming a pane their plan does not load; a plan
    // row whose artifact is absent is the realization's to say (`agents/realization.md`).
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

    // ...AND NOTHING IS INSPECTED: the host holds a subject only when an inspector names one.
    CHECK_FALSE(f.r.session().inspected.addressed());
}

TEST_CASE("INFO-WEAVE: the pane declares the ids a maker's keymap file already names") {
    // THE THREE IDS DID NOT MOVE. `info.up`, `info.down` and `info.edit` were rows of
    // Workshop's COMMAND context and are the pane's, spelled exactly as they were, with the same
    // default gestures -- so an authored override keeps working. `info.switch` arrived with the
    // pane list, and is the pane's own.
    InfoRig f;
    f.open();
    CHECK(f.declared() == kResting);

    // ...AND THE HOST DECLARES NONE OF THEM. A row in both catalogs would be one authored
    // override naming two things, which `join_pane_rows` refuses whole (WL-KEY-06/08).
    for (const std::string& moved : f.declared()) {
        INFO("id ", moved);
        CHECK(row_of_id(moved.c_str()) == nullptr);
    }

    // ⭐ AND THE DRAFT'S TWO COULD NOT KEEP THEIR NAMES. `draft.commit` and `draft.cancel` were
    // `KeyContext::kDraft`'s rows, declared for the host Pane Manager's drafts while it lived, so
    // this pane spells `info.commit` and `info.cancel` on the same gestures. The host's two
    // retired with that manager: a maker's file naming them is kept and told where the act went.
    CHECK(row_of_id("draft.commit") == nullptr);
    CHECK(row_of_id("draft.cancel") == nullptr);
    CHECK(retired_instead("draft.commit") != nullptr);
    CHECK(retired_instead("draft.cancel") != nullptr);
    CHECK(row_of_id(pane::kActionCommit) == nullptr);
    CHECK(row_of_id(pane::kActionCancel) == nullptr);
}

// ============================================================================
// INFO-WEAVE — the two lists, said as rows
// ============================================================================

TEST_CASE("INFO-WEAVE: the two headings and both lists are the pane's rows, over the host's "
          "inventory and subject") {
    InfoRig f;
    f.open();
    const std::vector<CatalogRow> inventory =
        inventory_rows(f.r.session().setup.active, f.r.session().panels);
    REQUIRE(inventory.size() >= 2); // three while the host's Pane Manager was a built-in

    std::string all = f.text();
    CHECK(all.find("PANES -- " + std::to_string(inventory.size())) != std::string::npos);
    // EVERY PANE OF THE ONE INVENTORY, BY ITS OWN NAME -- the host's reading, not a count.
    for (const CatalogRow& row : inventory) {
        INFO("pane ", row.name);
        CHECK(f.pane_row(row.name) >= 0);
    }
    // NOTHING IS INSPECTED UNTIL THE MAKER NAMES SOMETHING, and the rows say so.
    CHECK(all.find("PROPERTIES") != std::string::npos);
    CHECK(all.find("(no subject") != std::string::npos);

    f.inspect("Layouts");
    all = f.text();
    CHECK(all.find("PANE Layouts") != std::string::npos);
    // ...AND THE SUBJECT IS MARKED IN THE LIST, which is the one thing the list says that the
    // inventory does not.
    CHECK(f.row_of(">*Layouts -- ") >= 0);

    // THE PROPERTIES ARE THE SUBJECT'S, in the host's own row order and with the host's own
    // values -- read back off the seam rather than composed here.
    const PaneSubjectShown said = f.picture();
    CHECK(said.office == layouts_ref().provider);
    CHECK(said.pane == layouts_ref().pane);
    REQUIRE_FALSE(said.properties.empty());
    for (const ShownProperty& p : said.properties) {
        INFO("property ", p.label);
        CHECK((f.row_of(" " + p.label) >= 0 || f.row_of(">" + p.label) >= 0));
    }
    // ...AND EXACTLY ONE ROW WEARS THE KEYS' MARK: the list's, while the keys are in the list.
    std::size_t marked = 0;
    for (const std::string& row_text : f.shown()) {
        marked += (!row_text.empty() && row_text[0] == '>') ? 1u : 0u;
    }
    CHECK(marked == 1);
}

TEST_CASE("INFO-WEAVE: the picture is PUBLISHED, so a gesture that never touched the pane "
          "moves it") {
    // ⭐ THE SEAM'S ONE PUBLICATION. A pane's resolved window changes under this pane with no
    // gesture into it -- the surface resized, a drag, a desk put live. A pane that could only ASK
    // would be a list that is wrong most of the time.
    InfoRig f;
    f.open();
    f.inspect("Layouts");
    const std::int64_t named = f.picture().subject;
    const std::string window = f.picture().properties[f.property_index("Window")].value;
    const std::string painted = f.property_row("Window");
    REQUIRE_FALSE(painted.empty());
    const std::size_t said_before = f.r.said_subjects.size();

    f.r.extent(150, 44); // the surface, and nothing about the pane

    CHECK(f.r.said_subjects.size() > said_before);
    CHECK(f.picture().properties[f.property_index("Window")].value != window);
    CHECK(f.property_row("Window") != painted);
    // ...UNDER THE SAME NAME: a value moving is not another subject.
    CHECK(f.picture().subject == named);
}

TEST_CASE("INFO-WEAVE: with nothing inspected, the properties say so and say what to do next") {
    // A PANEL THAT MERELY GOES BLANK is indistinguishable from a tool that has broken.
    InfoRig f;
    f.open();
    CHECK(f.text().find("(no subject") != std::string::npos);
    // ...AND THE KEYS CANNOT BE SENT TO ROWS THAT DO NOT EXIST: said, not silently refused.
    f.focus();
    f.r.key(input::scan::kTab);
    CHECK_MESSAGE(f.row_of("nothing is inspected") == 0, f.text());
    CHECK(f.declared() == kResting);
}

TEST_CASE("INFO-WEAVE: what the body cannot show, it counts -- on the side it left it out") {
    // THE OMISSION MARKERS, over a subject taller than the room. A list that silently stopped at
    // the last row it could draw would be a list a maker cannot trust.
    InfoRig f;
    f.open(160, 22); // the shortest room, so both lists are pressed
    f.inspect("Layouts");
    std::string all = f.text();
    // THE CURSOR IS AT THE TOP, so what the window could not show is MORE, below.
    CHECK(all.find("... ") != std::string::npos);
    CHECK(all.find(" more") != std::string::npos);
    // ...AND WALKED TO THE BOTTOM, what it cannot show is EARLIER.
    f.r.key(input::scan::kTab);
    for (int i = 0; i < 20; ++i) {
        f.r.key(input::scan::kDown);
    }
    all = f.text();
    CHECK(all.find(" earlier") != std::string::npos);
    // AND THE PANE NEVER PUBLISHED MORE ROWS THAN THE ROOM IT WAS GRANTED.
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    CHECK(static_cast<std::int64_t>(f.shown().size()) <= body.h);
}

// ============================================================================
// INFO-WEAVE — the gestures, through the real seam
// ============================================================================

TEST_CASE("INFO-WEAVE: a press on a pane row inspects it, through the host's own door") {
    InfoRig f;
    f.open();
    REQUIRE_FALSE(f.r.session().inspected.addressed());
    f.focus();
    f.press_pane_row("Layouts");
    // THE HOST HOLDS THE SUBJECT -- the pane asked and the host answered; nothing in the image can
    // name a subject for the host.
    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.picture().name == "Layouts");
    CHECK(f.row_of(">*Layouts -- ") >= 0);
    CHECK(f.text().find("PANE Layouts") != std::string::npos);

    // ANOTHER PRESS NAMES ANOTHER PANE, UNDER ANOTHER NAME -- this pane itself included.
    const std::int64_t first = f.picture().subject;
    f.press_pane_row("Info");
    CHECK(f.r.session().inspected.ref == pane_info_ref());
    CHECK(f.picture().subject != first);
    CHECK(f.row_of(">*Info -- ") >= 0);
}

TEST_CASE("INFO-WEAVE: the pane's keys act only after the maker has pressed into it") {
    // ⭐ VD-22: the pane's rows reach it only while it holds the keyboard.
    InfoRig f;
    f.open();
    const std::string resting = f.text();

    // NOT FOCUSED: the arrows are command mode's and the pane does not move.
    f.r.key(input::scan::kDown);
    CHECK(f.text() == resting);

    // FOCUSED: the same key moves the list cursor.
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
          "commits through the owner") {
    InfoRig f;
    f.open();
    f.draft_on("Width");

    // ⭐ A PANE IS ONE KEYBOARD CONTEXT, so while a maker is typing, these two are the only rows
    // this pane declares and every other key arrives as an ordinary `PaneKey` for the line.
    CHECK(f.declared() == kDrafting);
    f.r.text("77");
    CHECK(value_of(f.property_row("Width")).find("77") != std::string::npos);

    const std::string before = f.text();
    f.r.key(input::scan::kEscape);
    // ⚠ THE NOTICE IS CUT TO THE ROOM LIKE EVERY OTHER ROW, and the mark is where it was cut.
    const std::int64_t cancelled = f.row_of("edit cancelled");
    REQUIRE(cancelled == 0); // and a notice takes the pane's first row
    CHECK(f.shown()[0].find("...") != std::string::npos);
    CHECK(f.declared() == kResting);
    CHECK(before != f.text());
    CHECK(f.layouts_width() == "-"); // nothing was written
}

TEST_CASE("INFO-WEAVE: a draft on a value the maker owns is written to the desk") {
    InfoRig f;
    f.open();
    f.draft_holding("Width", "12");
    f.r.key(input::scan::kReturn); // commit

    // THE DESK HOLDS IT, through the setup's own gesture door, and the pane is showing the owner's
    // answer rather than its own draft: the picture arrived on the same drain.
    CHECK(f.declared() == kResting);
    const SetupPane* row = pane_of(f.r.session().setup.active, layouts_ref());
    REQUIRE(row != nullptr);
    CHECK(row->width.mode == pane_unit::kSubcells);
    CHECK(row->width.amount == subs(12));
    CHECK(value_of(f.property_row("Width")) == "12 cells");
    // ...AND THE HOST SAYS WHAT IT WROTE, TO WHICH PANE.
    CHECK(f.r.last_notice().find("committed Width of Layouts = 12 cells") != std::string::npos);
}

TEST_CASE("INFO-WEAVE: a row the screen makes is refused by the pane, in its own words") {
    // THE REFUSAL THAT IS THE PANE'S TO MAKE, because the reason is about the ROW: a resolved
    // value is not authored, so there is nothing to open a draft on.
    InfoRig f;
    f.open();
    f.inspect("Layouts");
    REQUIRE_FALSE(f.picture().properties[f.property_index("Window")].editable);
    f.press_property("Window");
    f.r.key(input::scan::kReturn);
    // THE REFUSAL NAMES THE ROW IT IS ABOUT, on the pane's first row, fitted to its column.
    REQUIRE_MESSAGE(f.row_of("Window is not autho") == 0, f.text());
    CHECK(f.shown()[0].find("...") != std::string::npos);
    // ...AND NO DRAFT OPENED.
    CHECK(f.declared() == kResting);
}

TEST_CASE("INFO-WEAVE: a live draft holds another subject back, and the reason is the maker's") {
    // A LIVE DRAFT IS UNFINISHED WORK another subject would take the rows from, and the pane is
    // the party that knows: refused before anything is asked.
    InfoRig f;
    f.open();
    f.draft_on("Width");
    const std::int64_t named = f.picture().subject;
    f.press_pane_row("Info");
    CHECK(f.text().find("finish the edit first") != std::string::npos);
    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.picture().subject == named);
    CHECK(f.declared() == kDrafting);
}

TEST_CASE("INFO-WEAVE: a room too short for the body invents none of it") {
    // THE BUDGET IS TAKEN BEFORE EITHER LIST IS OFFERED ANYTHING, and a bound that grows
    // when it is exceeded is not a bound.
    InfoRig f;
    f.open(160, 48);
    f.inspect("Layouts");
    const std::int64_t tall = static_cast<std::int64_t>(f.shown().size());
    REQUIRE(tall > 0);

    f.r.extent(160, 22);
    const ui::Rect body = external_body_rect(f.r.session(), f.kind);
    CHECK(static_cast<std::int64_t>(f.shown().size()) <= body.h);
    CHECK(static_cast<std::int64_t>(f.shown().size()) < tall);
    // AND EVERY ROW FITS THE COLUMNS IT WAS GRANTED -- the other half of `judge_content`,
    // which refuses a publication WHOLE when one row is a byte too wide.
    for (const std::string& row_text : f.shown()) {
        CHECK(static_cast<std::int64_t>(row_text.size()) <= body.w);
    }
}

TEST_CASE("INFO-WEAVE: Info may inspect itself, and an edit to its own place is written by the "
          "desk, reseats it and keeps the subject") {
    // ⭐ SELF-INSPECTION. The column that shows a pane's placement can show and edit its own; the
    // write goes through the same door any other pane's does, and the room it moves is this
    // pane's own -- which is not a new subject.
    InfoRig f;
    f.open();
    f.inspect("Info");
    CHECK(f.picture().office == pane::kInfoPaneRole);
    CHECK(f.row_of(">*Info -- ") >= 0);
    const std::int64_t named = f.picture().subject;
    const std::string window = f.picture().properties[f.property_index("Window")].value;

    f.draft_holding("X", "12", "Info");
    f.r.key(input::scan::kReturn);

    CHECK(f.declared() == kResting);
    const SetupPane* row = pane_of(f.r.session().setup.active, pane_info_ref());
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kSubcells);
    CHECK(row->place.x == subs(12));
    // THE DESK RESEATED IT: its resolved window moved, and it still inspects itself.
    CHECK(f.picture().properties[f.property_index("Window")].value != window);
    CHECK(f.picture().subject == named);
    CHECK(f.r.session().inspected.ref == pane_info_ref());
    CHECK(f.r.last_notice().find("committed X of Info = 12 cells") != std::string::npos);
}

TEST_CASE("INFO-WEAVE: the subject is Info's to name: the keys leaving, Escape and a press "
          "elsewhere leave it standing") {
    // ⭐ INDEPENDENT OF SELECTION AND FOCUS. The subject is written by one door, asked by this
    // pane; nothing the maker does elsewhere reaches it.
    InfoRig f;
    f.open();
    f.inspect("Layouts");
    const std::int64_t named = f.picture().subject;

    f.unfocus();                  // a press elsewhere: the keys leave, the selection moves
    f.r.key(input::scan::kEscape); // the host's own Escape, with nothing more specific to do
    f.r.key(input::scan::kDown);   // command mode's arrows
    CHECK(f.r.session().panels.keyboard != f.kind);

    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.picture().subject == named);
    CHECK(f.text().find("PANE Layouts") != std::string::npos);
    const std::int64_t marked = f.pane_row("Layouts");
    REQUIRE(marked >= 0);
    CHECK(f.shown()[static_cast<std::size_t>(marked)][1] == '*');
}

// ============================================================================
// A notice, and the act that spends it
// ============================================================================
//
// A PANE'S NOTICE STANDS UNTIL THE MAKER'S NEXT ACT, AND SPENT MEANS GONE FROM THE ROWS WORKSHOP
// HOLDS (`agents/panes.md`). Both halves are asked of what Workshop admitted and painted, never
// of the pane: a notice cleared in private stands painted until some unrelated grant says the
// rows again, so every "it stands" below is read again after a new room.

TEST_CASE("a press on an Info row while a notice stands names the row painted there, and a full "
          "room keeps its last row under the notice") {
    // THE NOTICE TAKES THE FIRST ROW AND EVERY ROW UNDER IT MOVES DOWN ONE. The pane composes its
    // body without the notice and `finish` puts it in front; the row map counts it the same way.
    InfoRig f;
    f.open(160, 22);
    f.inspect("Layouts");
    f.r.key(input::scan::kTab);    // the keys into the properties, on `Name`
    f.r.key(input::scan::kReturn); // `info.edit` on a row nobody authors: the pane refuses
    REQUIRE_MESSAGE(f.row_of("Name is not autho") == 0, f.text());

    // A FULL ROOM: every granted row is published, and the last is the counted omission.
    const std::vector<std::string> rows = f.shown();
    const ExternalPane* seat = f.r.session().panels.external_pane(f.kind);
    REQUIRE(seat != nullptr);
    REQUIRE(static_cast<std::int64_t>(rows.size()) == seat->rows);
    CHECK(rows.back().rfind("... ", 0) == 0);
    // ...AND NOTHING BLANK UNDER THE PANE LIST: the row above the subject's heading is a pane.
    const std::int64_t heading = f.row_of("PANE Layouts");
    REQUIRE(heading > 1);
    CHECK(rows[static_cast<std::size_t>(heading - 1)].find(" -- ") != std::string::npos);

    // A PRESS ON A PROPERTY PAINTED UNDER THE NOTICE PUTS THE CURSOR ON THAT PROPERTY.
    f.press_property("Summary");
    CHECK(f.row_of(">Summary") >= 0);
    CHECK(f.row_of("Name is not autho") == -1); // ...and it was an act, so the notice is spent

    // A PRESS ON A PANE PAINTED UNDER A NOTICE INSPECTS THAT PANE.
    f.r.key(input::scan::kReturn);
    REQUIRE(f.row_of("Summary is not autho") == 0);
    f.press_pane_row("Info");
    CHECK(f.r.session().inspected.ref == pane_info_ref());
    CHECK(f.row_of(">*Info -- ") >= 0);
}

TEST_CASE("a key the Info draft line does not take is no act: the notice stands through a new "
          "room, and a key it takes spends it") {
    // THE DRAFT'S LINE REFUSES A KEY IT HAS NO MEANING FOR; the refusal is not an act.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    f.press_pane_row("Info"); // the pane's own refusal, beside the live draft
    REQUIRE(f.row_of("finish the edit") == 0);
    const std::string drafted = f.property_row("Width");
    REQUIRE(value_of(drafted) == "77");

    f.r.key(input::scan::kDown); // a key the line has no meaning for
    CHECK(f.row_of("finish the edit") == 0);
    CHECK(f.property_row("Width") == drafted); // the same room, so the same row, byte for byte
    f.regrant();
    CHECK(f.row_of("finish the edit") == 0);
    // ...AND THE DRAFT IS THE DRAFT IT WAS: still open, its text where the maker left it.
    CHECK(f.declared() == kDrafting);
    CHECK(value_of(f.property_row("Width")) == "77");

    f.r.key(input::scan::kLeft); // a key the line takes
    CHECK(f.row_of("finish the edit") == -1);
    CHECK(value_of(f.property_row("Width")) == "77");
}

TEST_CASE("an id the Info pane does not answer to in its mode is no act: a commit resolved before "
          "a cancel, and an id nobody declared, leave the notice standing through a new room") {
    // THE MODE OWNS THE PANE'S ACTIONS, AND A DECLARATION RACES A KEYSTROKE. Escape and Return in
    // one poll resolve against the rows the draft declared -- `info.cancel`, then `info.commit`
    // -- and the pane hears the commit after the cancel closed the draft.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");

    f.enqueue_key(input::scan::kEscape);
    f.enqueue_key(input::scan::kReturn);
    f.settle();
    REQUIRE(f.row_of("edit cancelled") == 0);
    CHECK(f.declared() == kResting);
    f.regrant();
    CHECK(f.row_of("edit cancelled") == 0);

    // AN ID NOBODY DECLARED, SAID BY WORKSHOP'S OWN OFFICE, is the same non-act.
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
    // ...AND NEITHER ONE WROTE ANYTHING.
    CHECK(f.layouts_width() == "-");

    // THE SAME DOOR WITH AN ID THE PANE DOES ANSWER TO IS AN ACT.
    const auto cursor_row = [&f] { // the property row wearing the mark
        for (const std::string& one : f.shown()) {
            if (one.rfind(">", 0) == 0 && one.find(" -- ") == std::string::npos) {
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

TEST_CASE("a press on the pane Info already inspects spends the notice in the rows Workshop holds "
          "while its answer is still on its way, and needs no new picture to say so") {
    // AN ACCEPTED ASK THAT CHANGES NO PICTURE: the press asks the host to inspect what is
    // inspected; the host answers yes and has nothing new to publish.
    InfoRig f;
    f.open();
    f.inspect("Layouts");
    f.r.key(input::scan::kTab);
    f.r.key(input::scan::kReturn); // `info.edit` on `Name`: the pane refuses
    REQUIRE(f.row_of("Name is not autho") == 0);
    const std::size_t pictures = f.r.said_subjects.size();
    const std::int64_t at = f.pane_row("Layouts");
    REQUIRE(at > 0);

    const loom::WeaveId pane_id = info_id(f);
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
            } else if (ev.schema_name == PaneSubjectActed::zen_name) {
                answered = true;
            }
        });
    f.enqueue_press(at);
    for (int turns = 0; turns < 8 && !pressed; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(pressed);
    REQUIRE_FALSE(answered);
    const auto leads_with_it = [&f] { return f.admitted_leads("Name is not autho"); };
    for (int turns = 0; turns < 16 && leads_with_it() && !answered; ++turns) {
        (void)bus.pump_pending();
    }
    CHECK_FALSE(answered);
    CHECK_FALSE(leads_with_it());
    bus.drain_until_idle();
    bus.remove_observer(tap);

    // THE CONVERSATION IS OVER: accepted, the subject where it was, and no picture was said.
    CHECK(answered);
    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.r.said_subjects.size() == pictures);
    CHECK(f.row_of("Name is not autho") == -1);
    f.regrant();
    CHECK(f.row_of("Name is not autho") == -1);

    // A PRESS STILL MEANS WHAT THE PICTURE SAYS.
    f.press_pane_row("Info");
    CHECK(f.r.session().inspected.ref == pane_info_ref());
}

TEST_CASE("an Info act that moves nothing still spends the notice before it, and a refusal the "
          "owner gives a commit stands until the act after it") {
    InfoRig f;
    f.open();
    // THE OWNER'S OWN REFUSAL IS A NOTICE: it stands through a new room, beside the draft.
    f.draft_holding("Width", "abc");
    f.r.key(input::scan::kReturn);
    REQUIRE_MESSAGE(f.row_of("Width: not a whole") == 0, f.text());
    CHECK(f.declared() == kDrafting);
    f.regrant();
    CHECK(f.row_of("Width: not a whole") == 0);
    CHECK(f.layouts_width() == "-");

    // ...UNTIL THE ACT AFTER IT.
    f.r.key(input::scan::kEscape);
    REQUIRE(f.row_of("edit cancelled") == 0);

    // AN ACT THAT MOVES NOTHING: the cursor walked to the top, a refusal said there, and `up`
    // pressed where there is no row above -- declared, applicable, and a no-op that spends it.
    for (int i = 0; i < 12; ++i) {
        f.r.key(input::scan::kUp);
    }
    f.r.key(input::scan::kReturn);
    REQUIRE(f.row_of("Name is not autho") == 0);
    const std::string marked = f.property_row("Name");
    f.r.key(input::scan::kUp);
    CHECK(f.row_of("Name is not autho") == -1);
    CHECK(f.property_row("Name").rfind(">Name", 0) == 0);
    CHECK_FALSE(marked.empty());

    // ...AND THE NEXT PRESS READS THE PICTURE THAT SAYS SO.
    f.press_pane_row("Info");
    CHECK(f.r.session().inspected.ref == pane_info_ref());
}

// ============================================================================
// A draft, a request and an answer
// ============================================================================
//
// THREE LIFETIMES. A draft is the pane's and ends when the maker ends it or when the picture stops
// showing its property; a request is the owner's once it is sent, and closing a draft does not
// take it back; an answer belongs to the ask that asked, and for a commit to the draft that sent
// it. Every case reads the desk itself beside the rows Workshop admitted and painted, because a
// sentence that claims less than happened is the defect here.

TEST_CASE("a press on a pane while an Info draft is live is refused, keeping the draft, its text, "
          "the subject and the desk through a new room, and inspecting resumes once the draft "
          "ends") {
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::size_t pictures = f.r.said_subjects.size();
    const auto draft_stands = [&] {
        CHECK(f.declared() == kDrafting);
        CHECK(value_of(f.property_row("Width")) == "77");
        CHECK(f.r.session().inspected.ref == layouts_ref());
        CHECK(f.layouts_width() == "-");
    };

    // THE PANE ALREADY INSPECTED: refused, and nothing was asked of the host.
    f.press_pane_row("Layouts");
    CHECK(f.row_of("finish the edit first") == 0);
    CHECK(f.admitted_leads("finish the edit first"));
    draft_stands();
    CHECK(f.r.said_subjects.size() == pictures);

    // ANOTHER PANE, PAINTED UNDER THE REFUSAL: refused the same way.
    f.press_pane_row("Info");
    CHECK(f.row_of("finish the edit first") == 0);
    draft_stands();

    // A NEW ROOM IS NOT AN ACT, AND KEEPS THE DRAFT.
    f.regrant();
    CHECK(f.row_of("finish the edit first") == 0);
    draft_stands();

    // THE DRAFT ENDS, NOTHING WAS SENT, AND A PRESS INSPECTS AGAIN.
    f.r.key(input::scan::kEscape);
    REQUIRE(f.row_of("edit cancelled -- unwri") == 0);
    f.press_pane_row("Info");
    CHECK(f.r.session().inspected.ref == pane_info_ref());
    CHECK(f.row_of(">*Info -- ") >= 0);
    CHECK(f.layouts_width() == "-");
}

TEST_CASE("a commit and a cancel resolved in one poll: the cancel says the commit was already "
          "sent, the answer's account takes that sentence's place, and no row says nothing was "
          "written over a write") {
    // ESCAPE ENDS THE DRAFT, AND IT CANNOT END A COMMIT THAT HAS LEFT.
    const auto commit_then_cancel = [](InfoRig& f, const std::string& typed) {
        f.draft_holding("Width", typed);
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
        CHECK(f.declared() == kResting);
    };
    const auto nothing_claims_it_was_not_written = [](InfoRig& f) {
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("nothing was") == std::string::npos);
            CHECK(one.find("cancelled") == std::string::npos);
        }
    };

    {
        // REFUSED: the value stands, and the refusal is the commit's, not a closed field's.
        InfoRig f;
        f.open();
        commit_then_cancel(f, "abc");
        CHECK_MESSAGE(f.row_of("commit refused -- Width") == 0, f.text());
        CHECK(f.layouts_width() == "-");
        CHECK(f.picture().properties[f.property_index("Width")].value == "-");
        f.regrant();
        CHECK(f.row_of("commit refused") == 0);
    }
    {
        // TAKEN: the desk holds the value, the pane says the commit was written, and the host's own
        // band says what it committed.
        InfoRig f;
        f.open();
        commit_then_cancel(f, "77");
        CHECK(f.layouts_width() == "77 cells");
        CHECK(f.picture().properties[f.property_index("Width")].value == "77 cells");
        CHECK(f.row_of("commit written") == 0);
        CHECK(f.r.last_notice().find("committed Width of Layouts = 77 cells") != std::string::npos);
        nothing_claims_it_was_not_written(f);
        f.regrant();
        CHECK(f.row_of("commit written") == 0);
        nothing_claims_it_was_not_written(f);
    }
}

TEST_CASE("an Info commit answered after a newer draft opened on the same field closes, alters and "
          "marks nothing on that draft, and a sentence a later act said stands") {
    // AN OLD ANSWER IS NOT THE NEW DRAFT'S. A commit, a cancel and a new edit delivered in one
    // burst, then a press on another pane: the host answers the first draft's commit while the
    // second draft is open on the same property.
    const auto late_answer = [](const std::string& typed, const std::string& expected) {
        InfoRig f;
        f.open();
        f.draft_holding("Width", typed);
        const std::int64_t other = f.pane_row("Info");
        REQUIRE(other > 0);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_action(pane::kActionCommit);
        f.enqueue_action(pane::kActionCancel);
        f.enqueue_action(pane::kActionEdit);
        f.enqueue_press(other);
        f.settle();
        REQUIRE(tap.answered == 1);

        const auto newer_draft_stands = [&] {
            CHECK(f.declared() == kDrafting);
            CHECK(f.row_of("finish the edit first") == 0);
            const std::string drafted = f.property_row("Width");
            REQUIRE_FALSE(drafted.empty());
            CHECK(drafted.rfind(">Width", 0) == 0);
            CHECK(value_of(drafted) == "-"); // what the newer draft opened with
            for (const std::string& one : f.shown()) {
                INFO("row: ", one);
                CHECK(one.find("commit written") == std::string::npos);
                CHECK(one.find("commit refused") == std::string::npos);
                CHECK(one.find("commit already") == std::string::npos);
                CHECK(one.find("not a whole") == std::string::npos);
            }
            CHECK(f.layouts_width() == expected);
        };
        newer_draft_stands();
        f.regrant();
        newer_draft_stands();

        // ...AND THE NEWER DRAFT'S OWN CANCEL IS TRUE ABOUT IT: it sent nothing.
        f.r.key(input::scan::kEscape);
        CHECK(f.row_of("edit cancelled -- unwri") == 0);
        CHECK(f.layouts_width() == expected);
    };
    SUBCASE("refused") { late_answer("abc", "-"); }
    SUBCASE("taken") { late_answer("77", "77 cells"); }
}

TEST_CASE("an Info draft ended while an earlier draft's commit is still unanswered says the commit "
          "was already sent, even when an act between them asked the host something else, and "
          "that commit's account replaces the sentence") {
    // WHETHER A COMMIT IS UNANSWERED IS WHAT DECIDES WHAT ENDING A DRAFT MAY SAY. A commit and its
    // cancel, a press on the pane already inspected (an inspect of its own), a new draft and its
    // cancel, delivered in one burst.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t layouts = f.pane_row("Layouts");
    REQUIRE(layouts >= 0);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_action(pane::kActionCommit);
    f.enqueue_action(pane::kActionCancel); // the first draft ends, its commit unanswered
    // ...and the Layouts row is pressed on the rows that cancel said, under its one-row sentence.
    f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, layouts + 1, 3}));
    f.enqueue_action(pane::kActionSwitch); // the keys back to the properties
    f.enqueue_action(pane::kActionEdit);
    f.enqueue_action(pane::kActionCancel);
    f.settle();
    REQUIRE(tap.answered == 2);
    CHECK(f.r.session().inspected.ref == layouts_ref()); // the inspect was asked and taken
    CHECK(f.layouts_width() == "77 cells");
    CHECK_MESSAGE(f.row_of("commit written") == 0, f.text());
    for (const std::string& one : f.shown()) {
        INFO("row: ", one);
        CHECK(one.find("cancelled") == std::string::npos);
    }
    f.regrant();
    CHECK(f.row_of("commit written") == 0);
}

TEST_CASE("an inspect asked before an Info draft opened, and answered while it is open, closes "
          "nothing") {
    // AN ACCEPTED ASK THAT IS NOT THE DRAFT'S IS NOT THE DRAFT'S END.
    InfoRig f;
    f.open();
    f.inspect("Layouts");
    f.press_property("Width");  // the property cursor on Width...
    f.r.key(input::scan::kTab); // ...and the keys back in the list
    const std::int64_t at = f.pane_row("Layouts");
    REQUIRE(at >= 0);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, at, 3}));
    f.enqueue_action(pane::kActionSwitch);
    f.enqueue_action(pane::kActionEdit);
    f.settle();
    REQUIRE(tap.answered == 1);
    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.declared() == kDrafting);
    f.r.text("77");
    CHECK(value_of(f.property_row("Width")) == "-77");
}

TEST_CASE("a picture that names other rows abandons the Info draft and says so, even where they "
          "have the same property on the same row, and writes nothing into either desk") {
    // THE SUBJECT IS THE PANE ON ITS DESK. Another desk put live has `Width` of the same pane on
    // the same row, and a commit carried there would write the maker's text into another desk.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t named = f.picture().subject;
    const std::size_t width = f.property_index("Width");

    f.unfocus();
    f.letter(input::scan::kEquals, "="); // `=` in command mode: a new layout, put live
    REQUIRE(layout_count(f.r.session().setup) == 2);
    REQUIRE(f.picture().subject != named);
    REQUIRE(f.picture().properties[width].label == "Width"); // the same property on the same row

    CHECK(f.declared() == kResting);
    CHECK_MESSAGE(f.row_of("edit abandoned -- the i") == 0, f.text());
    CHECK(value_of(f.property_row("Width")) == "-");
    CHECK(layouts_width_default_on_every_desk(f));

    // A COMMIT CANNOT FOLLOW IT THERE: the pane no longer answers to one, and neither desk moves.
    const PaneRig::OfficeAction commit =
        f.r.workshop_action(pane::kInfoPaneRole, pane::kInfoPane, pane::kActionCommit);
    REQUIRE(commit.delivered);
    CHECK(layouts_width_default_on_every_desk(f));
    CHECK(f.row_of("edit abandoned") == 0);
}

TEST_CASE("a clipboard answer asked for by an Info draft that has closed lands in no later draft, "
          "and one asked for by the draft still standing lands in it") {
    // A PASTE BELONGS TO THE DRAFT THAT ASKED (the text-box register's paste law).
    InfoRig f;
    f.open();
    SkinSeat* skin = f.r.mount_skin_seat();
    REQUIRE(skin != nullptr);
    skin->platform = "PASTED";
    f.draft_on("Width");

    // THE DRAFT THAT ASKED STILL STANDS: the text lands in it.
    f.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == 1);
    CHECK(value_of(f.property_row("Width")) == "-PASTED");

    // ASKED, THEN THAT DRAFT CANCELLED AND ANOTHER OPENED ON THE SAME PROPERTY BEFORE THE ANSWER.
    f.enqueue_as_workshop(
        loom::to_value(PaneKey{pane::kInfoPane, input::scan::kV, input::mod::kCtrl}));
    f.enqueue_action(pane::kActionCancel);
    f.enqueue_action(pane::kActionEdit);
    f.settle();
    CHECK(skin->clipboard_reads == 2); // the read really happened, so the absence is measured
    CHECK(f.declared() == kDrafting);
    CHECK(value_of(f.property_row("Width")) == "-");
    CHECK(f.layouts_width() == "-");
}

// ONE COMMIT OUTSTANDING, AND WHAT IT SENT. A commit is the owner's once it has left, and until it
// is answered the pane sends no other: a second commit is declined aloud and the draft goes on
// being edited. The answer is read against what the commit SENT as well as the draft that sent it,
// because typing after Return is an edit that no write covers.

TEST_CASE("a second Info commit in the same poll as the first is not sent: the pane says so, keeps "
          "the text typed between them, sends that text once the first is answered, and a cancel "
          "after the write and a refused retry claims no write away") {
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_key(input::scan::kReturn);
    f.enqueue_key(input::scan::kA, input::mod::kCtrl);
    f.enqueue_text("abc");
    f.enqueue_key(input::scan::kReturn);
    for (int turns = 0; turns < 16 && !f.admitted_leads("commit not sent") && tap.answered == 0;
         ++turns) {
        (void)f.r.bus.pump_pending();
    }
    CHECK(tap.answered == 0);
    CHECK(f.admitted_leads("commit not sent"));
    f.settle();

    // ONE COMMIT LEFT, WITH WHAT THE DRAFT HELD AT ITS RETURN, AND THE DESK TOOK IT.
    CHECK(asks.commits == std::vector<std::string>{"77"});
    CHECK(tap.answered == 1);
    CHECK(f.layouts_width() == "77 cells");
    CHECK(f.picture().properties[f.property_index("Width")].value == "77 cells");
    const auto kept = [&] {
        CHECK(f.declared() == kDrafting);
        CHECK(value_of(f.property_row("Width")) == "abc");
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

    // THE KEPT TEXT IS SENT ONCE THE FIRST IS ANSWERED -- refused in the owner's own words.
    f.r.key(input::scan::kReturn);
    CHECK(asks.commits == std::vector<std::string>{"77", "abc"});
    CHECK(tap.answered == 2);
    CHECK_MESSAGE(f.row_of("Width: not a whole") == 0, f.text());
    CHECK(f.declared() == kDrafting);
    CHECK(f.layouts_width() == "77 cells");

    // ESCAPE ENDS THE DRAFT, DISCARDS WHAT WAS NEVER WRITTEN, AND TAKES NO WRITE AWAY IN WORDS.
    f.r.key(input::scan::kEscape);
    const auto cancelled = [&] {
        CHECK_MESSAGE(f.row_of("edit cancelled -- unwri") == 0, f.text());
        CHECK(f.admitted_leads("edit cancelled -- unwri"));
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("nothing was") == std::string::npos);
        }
        CHECK(f.declared() == kResting);
        CHECK(f.layouts_width() == "77 cells");
        CHECK(f.picture().properties[f.property_index("Width")].value == "77 cells");
    };
    cancelled();
    f.regrant();
    cancelled();
}

TEST_CASE("Return twice over an unchanged Info draft sends one commit, says the second was not "
          "sent while the first is unanswered, and the first's acceptance closes the draft and "
          "retires that sentence") {
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);
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
    CHECK(f.layouts_width() == "77 cells");
    CHECK(f.r.last_notice().find("committed Width of Layouts = 77 cells") != std::string::npos);
    const auto closed = [&] {
        CHECK(f.declared() == kResting);
        CHECK(value_of(f.property_row("Width")) == "77 cells");
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

TEST_CASE("text typed after an Info commit was sent outlives that commit's answer: the draft stays "
          "open with its history, the write is told apart from the unsent text, a refusal is not "
          "said of the newer text, and the newer text commits normally") {
    // AN ACCEPTED WRITE DOES NOT COVER WHAT WAS TYPED AFTER IT WAS SENT.
    SUBCASE("taken") {
        InfoRig f;
        f.open();
        f.draft_holding("Width", "77");
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_text("8");
        f.settle();
        REQUIRE(tap.answered == 1);
        CHECK(heard_before(tap, PaneTextInput::zen_name, PaneSubjectActed::zen_name));
        CHECK(asks.commits == std::vector<std::string>{"77"});
        CHECK(f.layouts_width() == "77 cells");
        const auto kept = [&] {
            CHECK(f.declared() == kDrafting);
            CHECK(value_of(f.property_row("Width")) == "778");
            CHECK_MESSAGE(f.row_of("earlier commit written") == 0, f.text());
            CHECK(f.admitted_leads("earlier commit written"));
        };
        kept();
        f.regrant();
        kept();

        // THE LINE IS STILL THE LINE IT WAS: undo steps back over the typing, redo brings it back.
        f.r.key(input::scan::kZ, input::mod::kCtrl);
        CHECK(value_of(f.property_row("Width")).empty());
        f.r.key(input::scan::kY, input::mod::kCtrl);
        CHECK(value_of(f.property_row("Width")) == "778");

        // ...AND IT COMMITS LIKE ANY DRAFT: taken, unchanged since, so closed.
        f.r.key(input::scan::kReturn);
        CHECK(asks.commits == std::vector<std::string>{"77", "778"});
        CHECK(f.layouts_width() == "778 cells");
        CHECK(f.declared() == kResting);
        CHECK(value_of(f.property_row("Width")) == "778 cells");
    }
    SUBCASE("refused") {
        InfoRig f;
        f.open();
        f.draft_holding("Width", "abc");
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_key(input::scan::kA, input::mod::kCtrl);
        f.enqueue_text("12");
        f.settle();
        REQUIRE(tap.answered == 1);
        CHECK(heard_before(tap, PaneTextInput::zen_name, PaneSubjectActed::zen_name));
        CHECK(asks.commits == std::vector<std::string>{"abc"});
        CHECK(f.layouts_width() == "-");
        const auto kept = [&] {
            CHECK(f.declared() == kDrafting);
            CHECK(value_of(f.property_row("Width")) == "12");
            // THE REFUSAL IS THE COMMIT'S: said as the earlier commit's, never in the owner's
            // bare words over a value it was not about.
            CHECK_MESSAGE(f.row_of("earlier commit refused") == 0, f.text());
            CHECK(f.row_of("Width: not a whole") == -1);
        };
        kept();
        f.regrant();
        kept();

        f.r.key(input::scan::kReturn);
        CHECK(asks.commits == std::vector<std::string>{"abc", "12"});
        CHECK(f.layouts_width() == "12 cells");
        CHECK(f.declared() == kResting);
    }
    SUBCASE("abandoned") {
        // AND A DRAFT THAT OUTLIVED A WRITE, ABANDONED, TAKES NO WRITE AWAY IN WORDS EITHER -- read
        // in a window wide enough for the whole sentence, on both desks, because the right
        // column's width cuts it before the clause that says what was lost.
        InfoRig f;
        f.open();
        // (x = 1 is any place on the desk -- it stepped around the prototype canvas's boot objects
        // until that canvas retired; the rig reads a pane's rows by the region at its body's origin.)
        const auto widen = [](Setup& desk) {
            const Written placed = author_pane_place(desk, pane_info_ref(), subs(1), subs(3));
            REQUIRE_MESSAGE(placed.accepted, placed.refusal);
            const Written sized = author_pane_size(desk, pane_info_ref(),
                                                   PaneSize{pane_unit::kSubcells, subs(120)},
                                                   PaneSize{pane_unit::kSubcells, subs(30)});
            REQUIRE_MESSAGE(sized.accepted, sized.refusal);
        };
        f.unfocus();
        f.letter(input::scan::kEquals, "="); // a second desk, live...
        f.letter(input::scan::kComma, ",");  // ...and back to the first
        REQUIRE(f.r.session().setup.active_at == 0);
        REQUIRE(layout_count(f.r.session().setup) == 2);

        f.draft_holding("Width", "77");
        AnswerTap tap(f.r.bus, info_id(f));
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_text("8");
        f.settle();
        REQUIRE(tap.answered == 1);
        REQUIRE(f.declared() == kDrafting);
        // THE MAKER'S OWN WINDOW FOR THE PANE ON BOTH DESKS, through the setup's authoring doors,
        // and a new room -- which keeps the draft.
        widen(f.r.session().setup.active);
        widen(f.r.session().setup.shelved.front().desk);
        f.r.extent(150, 44);
        const ExternalPane* seat = f.r.session().panels.external_pane(f.kind);
        REQUIRE(seat != nullptr);
        REQUIRE(seat->columns > 90);
        REQUIRE(f.declared() == kDrafting);
        f.unfocus();
        f.letter(input::scan::kPeriod, "."); // the other desk, put live
        CHECK(f.declared() == kResting);
        CHECK_MESSAGE(f.row_of("edit abandoned -- the inspected pane, its desk or its rows "
                               "changed; unwritten changes discarded") == 0,
                      f.text());
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("nothing was") == std::string::npos);
        }
        const SetupPane* first = pane_of(layout_at(f.r.session().setup, 0), layouts_ref());
        REQUIRE(first != nullptr);
        CHECK(first->width.amount == subs(77)); // the write stands on the desk it was typed for
    }
}

TEST_CASE("an Info commit's answer settles the sentence that said a second commit was not sent, "
          "and leaves a sentence a later act said standing") {
    // A SENTENCE ABOUT A PENDING COMMIT BELONGS TO THAT COMMIT, AND ONLY THAT ONE. Two commits and
    // a press on another pane in one burst: the second commit is declined and the press says the
    // draft comes first.
    const auto burst = [](InfoRig& f, bool typed_between) {
        const std::int64_t other = f.pane_row("Info");
        REQUIRE(other > 0);
        f.enqueue_action(pane::kActionCommit);
        if (typed_between) {
            f.enqueue_as_workshop(loom::to_value(PaneTextInput{pane::kInfoPane, "8"}));
        }
        f.enqueue_action(pane::kActionCommit);
        f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, other + 1, 3}));
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
        f.draft_holding("Width", "77");
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        burst(f, false);
        CHECK(asks.commits == std::vector<std::string>{"77"});
        CHECK(tap.answered == 1);
        CHECK(f.layouts_width() == "77 cells");
        const auto stands = [&] {
            CHECK(f.declared() == kResting);
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
        f.draft_holding("Width", "abc");
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        burst(f, false);
        CHECK(asks.commits == std::vector<std::string>{"abc"});
        CHECK(tap.answered == 1);
        CHECK(f.layouts_width() == "-");
        const auto stands = [&] {
            CHECK(f.declared() == kDrafting);
            CHECK(value_of(f.property_row("Width")) == "abc");
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
        f.draft_holding("Width", "77");
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        AnswerTap tap(f.r.bus, info_id(f));
        burst(f, true);
        CHECK(asks.commits == std::vector<std::string>{"77"});
        CHECK(tap.answered == 1);
        CHECK(f.layouts_width() == "77 cells");
        const auto stands = [&] {
            CHECK(f.declared() == kDrafting);
            CHECK(value_of(f.property_row("Width")) == "778");
            CHECK_MESSAGE(f.row_of("finish the edit first") == 0, f.text());
            CHECK(f.admitted_leads("finish the edit first"));
            no_pending_sentence(f);
        };
        stands();
        f.regrant();
        stands();
    }
}

TEST_CASE("a commit from a newer Info draft is not sent while an earlier draft's commit is "
          "unanswered, even with an inspect asked between them, and the earlier commit's account "
          "replaces that sentence without closing or altering the newer draft") {
    // ONE OUTSTANDING COMMIT IS THE PANE'S, NOT ONE DRAFT'S.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t chosen = f.pane_row("Layouts");
    REQUIRE(chosen >= 0);
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);
    AnswerTap tap(f.r.bus, info_id(f));
    f.enqueue_action(pane::kActionCommit);
    f.enqueue_action(pane::kActionCancel); // the first draft ends, its commit unanswered
    // ...the pane already inspected is pressed on the rows that cancel said, under its sentence.
    f.enqueue_as_workshop(loom::to_value(PanePressed{pane::kInfoPane, chosen + 1, 3}));
    f.enqueue_action(pane::kActionSwitch);
    f.enqueue_action(pane::kActionEdit);
    f.enqueue_action(pane::kActionCommit);
    f.settle();

    CHECK(asks.commits == std::vector<std::string>{"77"});
    CHECK(asks.others == 1);
    CHECK(tap.answered == 2);
    CHECK(f.layouts_width() == "77 cells");
    const auto newer_stands = [&] {
        CHECK(f.declared() == kDrafting);
        CHECK(value_of(f.property_row("Width")) == "-");
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
    CHECK(f.layouts_width() == "12 cells");
    CHECK(f.declared() == kResting);
}

// ============================================================================
// INFO-WEAVE — a commit names what it was typed for, and a send Loom refused is not silence
// ============================================================================

TEST_CASE("an Info commit queued behind another desk put live, or another and back, is refused: "
          "neither desk is written and the pane says why") {
    // ⭐ THE RACE, THROUGH ORDINARY INPUT. Return, a press on the band and `=` in one poll: Workshop
    // resolves the Return to the pane's commit and puts a new desk live before the pane has sent it,
    // so the commit arrives naming rows the host no longer holds.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t typed_for = f.picture().subject;
    REQUIRE(typed_for != 0);
    AnswerTap tap(f.r.bus, info_id(f));
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);

    SUBCASE("another desk") {
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_unfocus();
        f.enqueue_key(input::scan::kEquals);
        f.settle();
        CHECK(layout_count(f.r.session().setup) == 2);
        CHECK(f.r.session().setup.active_at == 1);
    }
    SUBCASE("another desk and back") {
        // THE SAME DESK LIVE AGAIN: its rows, labels and values are exactly the ones the draft was
        // typed over, and it is still not the subject the commit named.
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_unfocus();
        f.enqueue_key(input::scan::kEquals);
        f.enqueue_key(input::scan::kComma);
        f.settle();
        CHECK(layout_count(f.r.session().setup) == 2);
        CHECK(f.r.session().setup.active_at == 0);
    }
    // ONE COMMIT LEFT THE PANE, NAMING ITS DRAFT'S ROWS, AND ONE ANSWER CAME BACK.
    REQUIRE(asks.commits == std::vector<std::string>{"77"});
    CHECK(asks.subjects == std::vector<std::int64_t>{typed_for});
    CHECK(tap.answered == 1);
    // NEITHER DESK WAS WRITTEN, AND THE HOST SAYS NOTHING WAS COMMITTED.
    CHECK(layouts_width_default_on_every_desk(f));
    CHECK(f.r.last_notice().find("committed") == std::string::npos);
    CHECK(f.picture().subject != typed_for);
    // THE PANE ABANDONED THE DRAFT WHEN IT SAW THE NEW NAME, AND ITS ROW SAYS WHY THE COMMIT WAS
    // REFUSED -- in what Workshop admitted and in what it painted.
    CHECK(f.declared() == kResting);
    CHECK_MESSAGE(f.row_of("commit refused -- the i") == 0, f.text());
    CHECK(f.admitted_leads("commit refused -- the i"));
    CHECK(value_of(f.property_row("Width")) == "-");
}

TEST_CASE("an Info commit that reaches the owner before another desk is put live is written to "
          "the desk it was typed for") {
    // THE OPPOSITE ORDER IS LEGITIMATE: the commit arrives while its name is still the rows' own.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t typed_for = f.picture().subject;
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);
    f.r.key(input::scan::kReturn); // drained: the commit is answered before anything else happens
    f.enqueue_unfocus();
    f.enqueue_key(input::scan::kEquals);
    f.settle();
    CHECK(asks.subjects == std::vector<std::int64_t>{typed_for});
    REQUIRE(layout_count(f.r.session().setup) == 2);
    const SetupPane* first = pane_of(layout_at(f.r.session().setup, 0), layouts_ref());
    REQUIRE(first != nullptr);
    CHECK(first->width.amount == subs(77));
    CHECK(f.layouts_width() == "-"); // the new desk
    CHECK(f.declared() == kResting);
    for (const std::string& one : f.shown()) {
        INFO("row: ", one);
        CHECK(one.find("refused") == std::string::npos);
    }
}

TEST_CASE("an Info commit queued behind a restore of the desk from its file is refused, and a "
          "refused restore keeps the draft") {
    // ⭐ THE SECOND IDENTITY BOUNDARY. A restore puts the file's desk live in the same place --
    // possibly the very same bytes -- and a draft typed for the desk it replaced is typed for
    // another desk.
    InfoRig f;
    f.open();
    TempDir dir("info-subject-restore");
    f.r.host.setup_path = dir.file("setup.json");

    SUBCASE("the desk's own bytes") {
        f.unfocus();
        f.letter(input::scan::kS, "s"); // save the desk
        REQUIRE_FALSE(f.r.session().notice_is_bad);
        const std::string saved = slurp(f.r.host.setup_path);
        f.draft_holding("Width", "77");
        const std::int64_t typed_for = f.picture().subject;
        AnswerTap tap(f.r.bus, info_id(f));
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_unfocus();
        f.enqueue_key(input::scan::kR);
        f.settle();
        REQUIRE(asks.commits == std::vector<std::string>{"77"});
        CHECK(asks.subjects == std::vector<std::int64_t>{typed_for});
        CHECK(tap.answered == 1);
        CHECK(f.r.last_notice().find("restored setup") != std::string::npos);
        CHECK(f.picture().subject != typed_for);
        CHECK(f.layouts_width() == "-");
        CHECK(slurp(f.r.host.setup_path) == saved);
        CHECK(f.declared() == kResting);
        CHECK_MESSAGE(f.row_of("commit refused -- the i") == 0, f.text());
        CHECK(f.admitted_leads("commit refused -- the i"));
    }
    SUBCASE("a restore the file refuses") {
        {
            std::ofstream bad(f.r.host.setup_path, std::ios::binary);
            bad << "{";
        }
        f.draft_holding("Width", "77");
        const std::int64_t typed_for = f.picture().subject;
        f.unfocus();
        f.letter(input::scan::kR, "r");
        REQUIRE(f.r.session().notice_is_bad);
        // THE DRAFT, ITS TEXT AND ITS NAME STAND, and the next Return is written.
        CHECK(f.picture().subject == typed_for);
        CHECK(f.declared() == kDrafting);
        CHECK(value_of(f.property_row("Width")) == "77");
        f.focus();
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        f.r.key(input::scan::kReturn);
        CHECK(asks.subjects == std::vector<std::int64_t>{typed_for});
        CHECK(f.layouts_width() == "77 cells");
        CHECK(f.declared() == kResting);
    }
}

TEST_CASE("an Info draft outlives a new room and its pane's window moving, and its commit is "
          "written") {
    // THE ORDINARY UPDATES ARE NOT A NEW SUBJECT. Each one re-reads the host's rows and publishes a
    // picture, and none of them changes what the draft's row addresses.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "41");
    const std::int64_t typed_for = f.picture().subject;
    const std::size_t said = f.r.said_subjects.size();
    const std::string window = f.picture().properties[f.property_index("Window")].value;
    f.regrant();
    f.r.extent(154, 46);
    REQUIRE(f.picture().properties[f.property_index("Window")].value != window);
    REQUIRE(f.r.said_subjects.size() > said); // the moved value was said...
    CHECK(f.picture().subject == typed_for);   // ...under the same name
    REQUIRE(f.declared() == kDrafting);
    CHECK(value_of(f.property_row("Width")) == "41");
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);
    f.r.key(input::scan::kReturn);
    CHECK(asks.subjects == std::vector<std::int64_t>{typed_for});
    CHECK(f.layouts_width() == "41 cells");
    CHECK(f.declared() == kResting);
}

TEST_CASE("an Info commit whose image was replaced before its answer is not the successor's: the "
          "successor holds no draft and says nothing of it, the subject stands, and the write "
          "shows as the owner's rows") {
    // ⭐ PROVIDERS CHANGE: THIS PANE'S OWN. A commit sent by one incarnation is answered to that
    // incarnation alone (Loom ANS-03), and a reload in between leaves its successor with no draft
    // (the state keeps the maker's position, never work in flight), no record of the commit and no
    // sentence about it. What the write did is the owner's to show, and the host shows it.
    TempDir copy("info-reload");
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t named = f.picture().subject;
    const std::string image =
        copy.file(("zengine-info-again" +
                   std::filesystem::path(WORKSHOP_SO_INFO_PANE).extension().string())
                      .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_INFO_PANE, image);

    const loom::WeaveId pane_id = info_id(f);
    loom::Switchboard& bus = f.r.bus;
    bool sent = false;
    int answers_delivered = 0;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered) {
            return;
        }
        if (!sent && ev.target == pane_id && ev.schema_name == PaneActionRequested::zen_name) {
            sent = true; // the pane has acted on Return: its commit is queued, not delivered
            bus.stop();
        } else if (ev.target == pane_id && ev.schema_name == PaneSubjectActed::zen_name) {
            ++answers_delivered;
        }
    });
    f.enqueue_key(input::scan::kReturn);
    for (int turns = 0; turns < 16 && !sent; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(sent);
    f.r.enqueue_reload(pane::kInfoPaneStem, image); // behind the commit, ahead of its answer
    bus.drain_until_idle();
    bus.remove_observer(tap);
    REQUIRE(f.r.load_refusals.empty());

    // THE OWNER TOOK THE COMMIT, AND ITS ANSWER REACHED NO INCARNATION OF THIS PANE.
    CHECK(f.layouts_width() == "77 cells");
    CHECK(answers_delivered == 0);
    // THE SUCCESSOR: no draft, no sentence about a commit it never sent, the subject standing.
    CHECK(f.declared() == kResting);
    for (const std::string& one : admitted_and_painted(f)) {
        INFO("row: ", one);
        CHECK(one.find("commit") == std::string::npos);
    }
    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.picture().subject == named);
    CHECK(f.text().find("PANE Layouts") != std::string::npos);
    CHECK(value_of(f.property_row("Width")) == "77 cells");
}

namespace {

/// WHAT THE BUS SAID ABOUT A COMMIT IT REFUSED AT DISPATCH, read off its tap.
struct RefusedAtDispatch {
    std::uint64_t attempt = 0; ///< the refused commit's sequence
    std::string reason;        ///< Loom's safe reason
    int notices = 0;           ///< Loom's notices naming that attempt, delivered to the pane
    int delivered = 0;         ///< commits delivered to anybody in the interval
};

/// LOOM REFUSES THE PANE'S NEXT COMMIT AT DISPATCH. The turn stops where the pane has heard `heard`
/// of Workshop's resolved actions -- its commit queued behind them, not delivered -- and
/// Workshop's weave is killed before that delivery, a real lifecycle change by the host's own
/// authority, so the bus refuses the queued commit and tells its author by that attempt. Workshop
/// is then revived in place from its own snapshot -- the same session -- asks the room who has
/// panes, and grants the pane a room again, so Workshop holds the rows it says.
inline RefusedAtDispatch refuse_next_commit(InfoRig& f, int heard) {
    const loom::WeaveId pane_id = info_id(f);
    loom::Switchboard& bus = f.r.bus;
    RefusedAtDispatch out;
    int actions = 0;
    bool stopped = false;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& ev) {
        if (!stopped && ev.kind == loom::EventKind::Delivered && ev.target == pane_id &&
            ev.schema_name == PaneActionRequested::zen_name && ++actions == heard) {
            stopped = true;
            bus.stop();
        }
        if (ev.schema_name == PaneCommitRequested::zen_name) {
            if (ev.kind == loom::EventKind::Refused && ev.sender == pane_id) {
                out.attempt = ev.seq;
                out.reason = loom::name_of(ev.refusal.reason);
            } else if (ev.kind == loom::EventKind::Delivered) {
                ++out.delivered;
            }
        }
        if (ev.kind == loom::EventKind::Delivered && ev.target == pane_id &&
            ev.schema_name == loom::DispatchRefused::zen_name && ev.payload != nullptr &&
            out.attempt != 0 &&
            loom::from_value<loom::DispatchRefused>(*ev.payload).refused_attempt().seq ==
                out.attempt) {
            ++out.notices;
        }
    });
    for (int turns = 0; turns < 16 && !stopped; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(stopped);
    const std::string bytes = bus.snapshot_bytes(f.r.workshop_id);
    bus.kill(f.r.workshop_id);
    bus.drain_until_idle();
    bus.remove_observer(tap);
    REQUIRE(bus.swap_state(f.r.workshop_id, bytes).revived);
    f.r.ready();
    f.regrant();
    return out;
}

/// AN OFFICE HOLDING `zengine.workshop` WITH NO SUBJECT DOOR. It hears a pane's offer, its declared
/// actions and its rows the way Workshop does, and says Workshop's resolved commit id to the Info
/// pane under that office. With Workshop's weave off the bus, nothing on it declares
/// `PaneCommitRequested`, so a commit meets Loom's seam before anything is queued.
class DoorlessOffice
    : public loom::WeaveBase<DoorlessOffice, SeatState,
                             loom::Accept<PaneOffered, PaneActions, PaneContent, SeatDo>,
                             loom::Emit<PaneActionRequested>> {
public:
    void on(const PaneOffered&, loom::Mail&) {}
    void on(const PaneActions&, loom::Mail&) {}
    void on(const PaneContent& said, loom::Mail& mail) {
        if (!mail.authored_from_role(pane::kInfoPaneRole)) {
            return;
        }
        rows.clear();
        for (const surface::SurfaceTextRow& row : said.rows) {
            rows.push_back(row.text);
        }
    }
    void on(const SeatDo&, loom::Mail& mail) {
        ++state_.said;
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(pane::kInfoPaneRole,
                          PaneActionRequested{pane::kInfoPane, pane::kActionCommit});
    }
    std::vector<std::string> rows; ///< the pane's last rows, as this office was told them
};

/// A STRANGER: an ordinary weave granted the refusal notice's shape, as every loaded image already
/// is. What it sends is its own speech, whatever the payload claims.
class Stranger : public loom::WeaveBase<Stranger, SeatState, loom::Accept<SeatDo>,
                                        loom::Emit<loom::DispatchRefused>> {
public:
    void on(const SeatDo&, loom::Mail&) { ++state_.said; }
};

/// AN INSPECTOR OF ITS OWN, IN ITS OWN OFFICE -- so a case can ask the host's subject doors what no
/// maker's hand on the Info pane can make it ask: a pane nobody has, or the picture on arrival.
class Inspector
    : public loom::WeaveBase<Inspector, SeatState,
                             loom::Accept<PaneSubjectActed, PaneSubjectShown, SeatDo>,
                             loom::Emit<InspectPaneRequested, PaneSubjectRequested>> {
public:
    void on(const SeatDo&, loom::Mail&) {}
    void on(const PaneSubjectActed& said, loom::Mail& mail) {
        if (mail.answers_ask()) {
            answers.push_back(said);
        }
    }
    void on(const PaneSubjectShown& said, loom::Mail& mail) {
        if (mail.answers_ask()) {
            pictures.push_back(said);
        }
    }
    std::vector<PaneSubjectActed> answers;
    std::vector<PaneSubjectShown> pictures;
};

inline constexpr const char* kInspectorOffice = "zengine.test.inspector";

} // namespace

TEST_CASE("an Info commit Loom refuses at dispatch is released: the draft and its text stand, the "
          "next Return is written, and a cancel's promise is replaced") {
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");

    SUBCASE("the draft that sent it stands") {
        f.enqueue_key(input::scan::kReturn);
        const RefusedAtDispatch refused = refuse_next_commit(f, 1);
        CHECK(refused.attempt != 0);
        CHECK(refused.reason == "TargetUnavailable");
        CHECK(refused.notices == 1);
        CHECK(refused.delivered == 0);
        CHECK(f.layouts_width() == "-");
        // THE PANE SAYS WHAT HAPPENED, in the rows Workshop holds and paints, and the draft is
        // exactly what it was.
        CHECK_MESSAGE(f.admitted_leads("commit not delivered --"), f.text());
        CHECK(f.row_of("commit not delivered --") == 0);
        CHECK(f.declared() == kDrafting);
        CHECK(value_of(f.property_row("Width")) == "77");
        // ...IT TAKES MORE TEXT, AND THE NEXT RETURN IS A FRESH COMMIT, NOT ONE HELD BEHIND AN
        // ANSWER THAT CANNOT COME.
        f.r.text("8");
        SubjectAskTap asks(f.r.bus, f.r.workshop_id);
        f.r.key(input::scan::kReturn);
        CHECK(asks.commits == std::vector<std::string>{"778"});
        CHECK(f.layouts_width() == "778 cells");
        CHECK(f.declared() == kResting);
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("commit not sent") == std::string::npos);
        }
    }
    SUBCASE("a draft cancelled while it waited") {
        // Return and Escape: the pane hears the commit and the cancel before the death, so the
        // cancel promised the commit may still be written. Loom's word replaces that promise.
        f.enqueue_key(input::scan::kReturn);
        f.enqueue_key(input::scan::kEscape);
        const RefusedAtDispatch refused = refuse_next_commit(f, 2);
        CHECK(refused.notices == 1);
        CHECK(refused.delivered == 0);
        CHECK(f.layouts_width() == "-");
        CHECK(f.declared() == kResting);
        CHECK_MESSAGE(f.admitted_leads("commit not delivered --"), f.text());
        for (const std::string& one : admitted_and_painted(f)) {
            INFO("row: ", one);
            CHECK(one.find("commit already sent") == std::string::npos);
        }
        // ...AND ESCAPE OVER THE NEXT DRAFT CLAIMS NO COMMIT IS OUTSTANDING.
        f.focus();
        f.r.key(input::scan::kReturn);
        REQUIRE(f.declared() == kDrafting);
        f.r.key(input::scan::kEscape);
        CHECK_MESSAGE(f.row_of("edit cancelled -- unwri") == 0, f.text());
    }
}

TEST_CASE("an Info commit nothing could queue is released at once: the draft stands, the next "
          "commit tries again, and it is written once the door is back") {
    // THE DOOR LEAVES FOR AN INTERVAL: Workshop's weave comes off the bus -- its session untouched
    // -- and an office with no subject door holds `zengine.workshop` meanwhile, so the commit's
    // shape is one this bus has never heard of and Loom's seam refuses it before anything is
    // queued. The pane's ticket is not valid; no answer and no notice can follow.
    InfoRig f;
    f.open();
    f.draft_holding("Width", "77");
    const std::int64_t typed_for = f.picture().subject;
    const loom::WeaveId pane_id = info_id(f);

    std::unique_ptr<loom::Weave> workshop = f.r.take_workshop_off();
    REQUIRE(f.r.bus.resolve_schema(PaneCommitRequested::zen_name,
                                   PaneCommitRequested::zen_version) == nullptr);
    auto held = std::make_unique<DoorlessOffice>();
    DoorlessOffice* office = held.get();
    loom::Grant say;
    say.allow_to_role(PaneActionRequested::zen_name, PaneActionRequested::zen_version,
                      pane::kInfoPaneRole);
    const loom::WeaveId office_id =
        f.r.bus.register_weave(std::move(held), std::move(say), std::string(kWorkshopProvider));
    office->zen_set_self(office_id);

    int seam_refusals = 0;
    int queued = 0;
    std::string reason;
    const loom::ObserverId tap = f.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.schema_name != PaneCommitRequested::zen_name) {
            return;
        }
        if (ev.kind == loom::EventKind::Refused && ev.sender == pane_id) {
            ++seam_refusals;
            reason = loom::name_of(ev.refusal.reason);
        } else {
            ++queued;
        }
    });
    const auto commit_id_said = [&f, office_id] {
        (void)f.r.bus.send(office_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        f.r.bus.drain_until_idle();
    };
    commit_id_said();
    CHECK(seam_refusals == 1);
    CHECK(reason == "SeamUnresolved");
    CHECK(queued == 0);
    REQUIRE_FALSE(office->rows.empty());
    CHECK_MESSAGE(office->rows.front().rfind("commit not submitted --", 0) == 0,
                  office->rows.front());
    // THE RECORD WAS RELEASED: the next commit is attempted again rather than refused as the second
    // of two, which is what an outstanding record would have said.
    commit_id_said();
    CHECK(seam_refusals == 2);
    CHECK(queued == 0);
    CHECK(office->rows.front().rfind("commit not submitted --", 0) == 0);
    f.r.bus.remove_observer(tap);

    // THE DOOR COMES BACK: the office leaves, and the same Workshop weave holds it again.
    REQUIRE(f.r.bus.unregister_weave(office_id) != nullptr);
    f.r.put_workshop_back(std::move(workshop));
    f.regrant();
    CHECK_MESSAGE(f.admitted_leads("commit not submitted --"), f.text());
    CHECK(f.declared() == kDrafting);
    CHECK(value_of(f.property_row("Width")) == "77");
    CHECK(f.layouts_width() == "-");
    SubjectAskTap asks(f.r.bus, f.r.workshop_id);
    f.r.key(input::scan::kReturn);
    CHECK(asks.subjects == std::vector<std::int64_t>{typed_for});
    CHECK(f.layouts_width() == "77 cells");
    CHECK(f.declared() == kResting);
}

TEST_CASE("a refusal notice anyone could send, naming the Info pane's outstanding commit exactly, "
          "settles nothing") {
    // THE PROVENANCE IS THE FACT; THE SHAPE IS SPEECH. A stranger says `zen.DispatchRefused` naming
    // every half the pane matches -- the attempt, the correlation, the shape, its version, the
    // office -- and queues it where it reaches the pane before the owner's answer does.
    InfoRig f;
    f.open();
    auto held = std::make_unique<Stranger>();
    Stranger* stranger = held.get();
    loom::Grant grant;
    grant.allow_to_any(loom::DispatchRefused::zen_name, loom::DispatchRefused::zen_version);
    const loom::WeaveId stranger_id = f.r.bus.register_weave(std::move(held), std::move(grant));
    stranger->zen_set_self(stranger_id);
    const loom::WeaveId pane_id = info_id(f);

    // THE COMMITS THE DOOR RECEIVED, with the attempt and the correlation each carried.
    std::vector<std::uint64_t> seqs;
    std::vector<std::uint64_t> correlations;
    std::vector<std::string> first_rows; ///< every row 0 the pane said, in order
    int forged_delivered = 0;
    bool stopped = false;
    bool stop_armed = false;
    const loom::ObserverId tap = f.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered) {
            return;
        }
        if (ev.schema_name == PaneCommitRequested::zen_name) {
            seqs.push_back(ev.seq);
            correlations.push_back(ev.correlation);
        } else if (ev.target == pane_id && ev.schema_name == loom::DispatchRefused::zen_name &&
                   ev.sender == stranger_id) {
            ++forged_delivered;
        } else if (ev.sender == pane_id && ev.schema_name == PaneContent::zen_name &&
                   ev.payload != nullptr) {
            const PaneContent said = loom::from_value<PaneContent>(*ev.payload);
            if (!said.rows.empty()) {
                first_rows.push_back(said.rows.front().text);
            }
        } else if (stop_armed && !stopped && ev.target == pane_id &&
                   ev.schema_name == PaneActionRequested::zen_name) {
            stopped = true;
            f.r.bus.stop();
        }
    });

    // THE PANE'S CORRELATION IS ITS OWN COUNT: read off a first commit, written, and the next is
    // one more. (The inspect the draft needed counted too, before this one.)
    f.draft_holding("Width", "7");
    f.r.key(input::scan::kReturn);
    REQUIRE(seqs.size() == 1);
    REQUIRE(f.layouts_width() == "7 cells");
    REQUIRE(f.declared() == kResting);
    f.r.key(input::scan::kReturn); // `info.edit` on the same row: a new draft, holding "7 cells"
    REQUIRE(f.declared() == kDrafting);
    for (int i = 0; i < 6; ++i) {
        f.r.key(input::scan::kBackspace);
    }
    f.r.text("8");
    REQUIRE(value_of(f.property_row("Width")) == "78");

    // THE TURN STOPS WHERE THE PANE HAS HEARD ITS COMMIT: the commit is queued, not delivered.
    stop_armed = true;
    f.enqueue_key(input::scan::kReturn);
    for (int turns = 0; turns < 16 && !stopped; ++turns) {
        (void)f.r.bus.pump_pending();
    }
    REQUIRE(stopped);
    REQUIRE(seqs.size() == 1);
    // ITS ATTEMPT IS THE SEQUENCE HANDED OUT JUST BEFORE THE NEXT ONE -- read off a probe now, and
    // checked against the commit's delivery afterwards.
    const loom::Ticket probe = f.r.bus.send(
        stranger_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    REQUIRE(probe.valid());
    loom::DispatchRefused forged;
    forged.attempt = std::to_string(probe.seq - 1);
    forged.role = kWorkshopProvider;
    forged.shape = PaneCommitRequested::zen_name;
    forged.version = PaneCommitRequested::zen_version;
    forged.reason = "TargetUnavailable";
    REQUIRE(f.r.bus
                .send_as(stranger_id, pane_id,
                         loom::Message(loom::to_value(forged), stranger_id, loom::WeaveId{},
                                       correlations.front() + 1))
                .valid());
    // ...AND WORKSHOP'S RESOLVED COMMIT ID AGAIN, queued behind the forgery, ahead of the answer.
    f.enqueue_action(pane::kActionCommit);
    f.settle();
    f.r.bus.remove_observer(tap);

    // THE FORGERY WAS DELIVERED, AND IT NAMED THE COMMIT THE DOOR THEN RECEIVED, EXACTLY.
    CHECK(forged_delivered == 1);
    REQUIRE(seqs.size() == 2);
    CHECK(seqs.back() == probe.seq - 1);
    CHECK(correlations.back() == correlations.front() + 1);
    // IT SETTLED NOTHING: the commit id said meanwhile met the outstanding commit and was not sent,
    // no row ever said the commit was not delivered, and the answer closed the draft.
    const bool not_sent_said =
        std::find_if(first_rows.begin(), first_rows.end(), [](const std::string& row) {
            return row.rfind("commit not sent --", 0) == 0;
        }) != first_rows.end();
    CHECK(not_sent_said);
    for (const std::string& row : first_rows) {
        INFO("row 0: ", row);
        CHECK(row.find("not delivered") == std::string::npos);
    }
    CHECK(f.layouts_width() == "78 cells");
    CHECK(f.declared() == kResting);
    CHECK(f.row_of("commit not sent") == -1);
}

TEST_CASE("an Info inspect Loom refuses at dispatch releases its own record, and the next press "
          "inspects") {
    // THE SAME ACCOUNTING FOR THE OTHER ASK. A press on a pane row asks the host to inspect it; the
    // turn stops where the pane has heard the press -- its inspect queued, not delivered -- and
    // Workshop's weave is killed before that delivery and revived afterwards.
    InfoRig f;
    f.open();
    f.focus();
    const std::int64_t at = f.pane_row("Info");
    REQUIRE(at >= 0);
    const loom::WeaveId pane_id = info_id(f);
    loom::Switchboard& bus = f.r.bus;
    bool stopped = false;
    std::uint64_t attempt = 0;
    std::string reason;
    int notices = 0;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& ev) {
        if (!stopped && ev.kind == loom::EventKind::Delivered && ev.target == pane_id &&
            (ev.schema_name == PanePressed::zen_name)) {
            stopped = true;
            bus.stop();
        }
        if (ev.kind == loom::EventKind::Refused && ev.sender == pane_id &&
            ev.schema_name == InspectPaneRequested::zen_name) {
            attempt = ev.seq;
            reason = loom::name_of(ev.refusal.reason);
        }
        if (ev.kind == loom::EventKind::Delivered && ev.target == pane_id &&
            ev.schema_name == loom::DispatchRefused::zen_name && ev.payload != nullptr &&
            attempt != 0 &&
            loom::from_value<loom::DispatchRefused>(*ev.payload).refused_attempt().seq == attempt) {
            ++notices;
        }
    });
    f.enqueue_press(at);
    for (int turns = 0; turns < 16 && !stopped; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(stopped);
    const std::string bytes = bus.snapshot_bytes(f.r.workshop_id);
    bus.kill(f.r.workshop_id);
    bus.drain_until_idle();
    bus.remove_observer(tap);
    REQUIRE(bus.swap_state(f.r.workshop_id, bytes).revived);
    f.r.ready();
    f.regrant();
    CHECK(attempt != 0);
    CHECK(reason == "TargetUnavailable");
    CHECK(notices == 1);
    CHECK_FALSE(f.r.session().inspected.addressed());
    CHECK_MESSAGE(f.admitted_leads("inspect not delivered -"), f.text());
    // ...AND THE NEXT PRESS IS AN INSPECT OF ITS OWN, answered.
    f.press_pane_row("Info");
    CHECK(f.r.session().inspected.ref == pane_info_ref());
    CHECK(f.row_of("inspect not delivered") == -1);
}

TEST_CASE("a subject naming a pane in neither this build's vocabulary nor this desk is refused in "
          "words with nothing moved, and an inspector that arrives is answered the picture as it "
          "is now") {
    // THE HOST'S TWO SUBJECT DOORS, ASKED BY AN OFFICE OF ITS OWN: a reference nobody has, which
    // no press on the Info pane can name, and the question an arriving inspector asks.
    InfoRig f;
    f.open();
    f.inspect("Layouts");
    const std::int64_t named = f.picture().subject;

    auto held = std::make_unique<Inspector>();
    Inspector* inspector = held.get();
    loom::Grant grant;
    grant.allow_to_role(InspectPaneRequested::zen_name, InspectPaneRequested::zen_version,
                        kWorkshopProvider);
    grant.allow_to_any(PaneSubjectRequested::zen_name, PaneSubjectRequested::zen_version);
    const loom::WeaveId id =
        f.r.bus.register_weave(std::move(held), std::move(grant), std::string(kInspectorOffice));
    inspector->zen_set_self(id);

    (void)f.r.bus.office_send_to_role_as(
        id, kInspectorOffice, kWorkshopProvider,
        loom::Message(loom::to_value(InspectPaneRequested{"zengine.nowhere", "ghost"}), id, id, 1));
    f.settle();
    REQUIRE(inspector->answers.size() == 1);
    CHECK_FALSE(inspector->answers.back().accepted);
    CHECK(inspector->answers.back().refusal.find("in neither this build's vocabulary nor this desk") !=
          std::string::npos);
    CHECK(f.r.session().inspected.ref == layouts_ref());
    CHECK(f.picture().subject == named);

    (void)f.r.bus.office_send_to_role_as(
        id, kInspectorOffice, kWorkshopProvider,
        loom::Message(loom::to_value(PaneSubjectRequested{}), id, id, 2));
    f.settle();
    REQUIRE(inspector->pictures.size() == 1);
    CHECK(inspector->pictures.back().name == "Layouts");
    CHECK(inspector->pictures.back().subject == named);

    // ...AND PERSONAL SPEECH IS ANSWERED BY NOBODY.
    (void)f.r.bus.send_as(id, f.r.workshop_id,
                          loom::Message(loom::to_value(PaneSubjectRequested{}), id, id, 3));
    f.settle();
    CHECK(inspector->pictures.size() == 1);
}

// ============================================================================
// INFO-WEAVE — what the image is not allowed to be
// ============================================================================

TEST_CASE("INFO-WEAVE: the image that inspects a pane holds no desk") {
    // A SOURCE READ, and the reason it is one: "this pane owns no facts" is a claim about what a
    // translation unit NAMES, and only reading the file can keep it.
    std::ifstream in(INFO_PANE_SOURCE);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();
    REQUIRE(source.size() > 4096);

    // THE HOST'S PRESENTATION AND ITS DESK ARE NOT REACHABLE FROM HERE, asked of the INCLUDE.
    for (const char* forbidden : {"#include \"workshop/screen.hpp\"",
                                  "#include \"workshop/panel.hpp\"",
                                  "#include \"workshop/setup.hpp\"",
                                  "#include \"workshop/screen_info", "#include \"ui/"}) {
        INFO("includes ", forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
    // ...AND NO DESK TYPE OR DESK DOOR IS NAMED IN ITS CODE AT ALL: what this image holds is the
    // PICTURE it was shown.
    for (const char* owned : {"SetupPane", "SetupState", "author_pane", "write_pane_axis"}) {
        INFO("names ", owned);
        CHECK(source.find(owned) == std::string::npos);
    }
    // ...AND WHAT IT DOES REACH IS THE PROTOCOL AND THE SEAM, both of which are values.
    CHECK(source.find("workshop/pane_vocabulary.hpp") != std::string::npos);
    CHECK(source.find("workshop/inspection_seam_vocabulary.hpp") != std::string::npos);
    // ...AND THE SHARED TEXT HELPERS RATHER THAN ANOTHER COPY OF THEM.
    CHECK(source.find("workshop/pane_text.hpp") != std::string::npos);
}
