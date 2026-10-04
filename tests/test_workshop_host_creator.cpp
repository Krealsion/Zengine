// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE PANE CREATOR: the first pane that exists because a weaver described one -- the value its
// interior IS and its law, its durable file and what it cannot say, the identity its NAME
// earns, the region on both faces, the inspected INTERIOR rows and the region mark, the
// one-open-definition lifecycle, relaunch by reference, and a code-backed pane's capture. A
// second source of the `workshop_host` entry, split as VM-POP-12 says; its helpers have
// internal linkage, so the two objects share only the support header.

#include "workshop_support.hpp"

#include "workshop/pane_definition.hpp"
#include "workshop/pane_definition_persist.hpp"

#include <zen/schema.hpp>

#include <algorithm>
#include <fstream>
#include <set>

namespace {

namespace pdp = pane_definition_persist;

// ---- The Pane Creator, through the doors an office asks ------------------------------------
// The keys and the name line are the desktop's Pane Manager's, and its own suite drives them;
// this file asks the host's doors as that pane and Info do -- the weaver door for make, save
// and discard (WL-MAKER-11), the inspector's for the subject and its rows (WL-INFO-14).

const PaneRef kMine = weaver_pane_ref("MyPane");

/// MAKE A PANE AND INSPECT IT: the weaver door, as the desktop's Pane Manager asks it, and then
/// the inspector's, as a weaver who opens Info on what they made. Answers what the weaver door
/// said.
WeaverPaneAnswered make_pane(Live& t, const std::string& name) {
    const WeaverPaneAnswered made = hand_weaver(t, weaver_pane_act::kCreate, name);
    REQUIRE_MESSAGE(made.accepted, made.said);
    REQUIRE(t.session().panes.weaver.open());
    REQUIRE(t.session().panes.weaver.definition.name == name);
    const PaneSubjectActed named = hand_inspect(t, weaver_pane_ref(name));
    REQUIRE_MESSAGE(named.accepted, named.refusal);
    return made;
}

/// ...AND SAVE IT, OR PUT IT BACK: the weaver door's other two acts.
WeaverPaneAnswered save_pane(Live& t) { return hand_weaver(t, weaver_pane_act::kSave); }
WeaverPaneAnswered discard_pane(Live& t) { return hand_weaver(t, weaver_pane_act::kDiscard); }

/// The index of the inspected pane's INTERIOR section row, or the row count when there is none.
std::size_t interior_section(const Live& t) {
    const std::vector<Row>& rows = t.session().inspected.rows;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].section() && rows[i].label() == "INTERIOR") {
            return i;
        }
    }
    return rows.size();
}

/// THE REGION'S OWN ROW -- the first with this label AFTER the INTERIOR section, told apart
/// from the pane's AUTHORED row of the same name by the section that owns it.
const Row* region_row(const Live& t, const std::string& label) {
    return subject_row(t.session(), label, "INTERIOR");
}

std::string region_value(const Live& t, const std::string& label) {
    return subject_value(t.session(), label, "INTERIOR");
}

/// WRITE A REGION ROW THROUGH THE COMMIT DOOR, as Info sends a finished draft, and answer what
/// the door said -- a refusal is the asker's to show, in the owner's words, and the band says
/// nothing of it.
PaneSubjectActed type_region_value(Live& t, const std::string& label, const std::string& text) {
    return hand_commit(t, label, text, "INTERIOR");
}

/// The pane's AUTHORED row of this label (the FIRST such row -- before INTERIOR).
std::string pane_value(const Live& t, const std::string& label) {
    return subject_value(t.session(), label);
}

/// The weaver-made pane's interior on this screen, through the ordinary pane path.
PixelRect interior_of(const Live& t) {
    const Screen sc = screen_of(t.session());
    const PaneBounds where =
        bounds_of(t.session().panes, t.session().setup.active, kWeaverPaneKind, sc);
    REQUIRE(where.open);
    REQUIRE_FALSE(where.rect.empty());
    return pane_inside(where.rect, sc).rect;
}

RegionPresentation presentation_of(const Live& t) {
    const WeaverPane& m = t.session().panes.weaver;
    REQUIRE(m.open());
    REQUIRE_FALSE(m.definition.regions.empty());
    return present_region(m.definition.regions.front(), interior_of(t), screen_of(t.session()));
}

/// The weaver-made pane's painted text, off the last frame -- the cell projection.
std::string weaver_pane_text(Live& t) {
    const Screen sc = screen_of(t.session());
    const PaneBounds where =
        bounds_of(t.session().panes, t.session().setup.active, kWeaverPaneKind, sc);
    REQUIRE(where.open);
    const ui::Rect b = cells_covered(where.rect);
    // EVERY CELL LABEL INSIDE THE PANE, top to bottom then left to right -- a region placed
    // inside the interior does not begin at the pane's own column, so the support header's
    // left-column reader would miss it.
    std::vector<surface::SurfaceLabel> inside;
    for (const surface::SurfaceLabel& l : cell_text_of(t.canvases.back())) {
        const std::int64_t lx = surface::cell_of_pixel(l.x), ly = surface::cell_of_pixel(l.y);
        if (lx >= b.x && lx < b.x + b.w && ly >= b.y && ly < b.y + b.h) {
            inside.push_back(l);
        }
    }
    std::sort(inside.begin(), inside.end(),
              [](const surface::SurfaceLabel& a, const surface::SurfaceLabel& c) {
                  return a.y != c.y ? a.y < c.y : a.x < c.x;
              });
    std::string out;
    for (const surface::SurfaceLabel& l : inside) {
        out += l.text;
        out += '\n';
    }
    return out;
}

std::string definition_bytes(const Live& t) {
    return pdp::to_text(t.session().panes.weaver.definition);
}

/// A graphical face: the shipped skin's metric and its device unit.
void sdl_face(Live& t, std::int64_t w = 160, std::int64_t h = 60) {
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(w), cells_px(h), 8, 18, surface::kCanvasCellPx}));
}

/// A character face at the same extent.
void tui_face(Live& t, std::int64_t w = 160, std::int64_t h = 60) {
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(w), cells_px(h), 0, 0, 0}));
}

/// The JSON object keys a serialised value spells -- every `"word":` in the text.
std::set<std::string> json_keys(const std::string& text) {
    std::set<std::string> keys;
    std::size_t at = 0;
    while ((at = text.find('"', at)) != std::string::npos) {
        const std::size_t end = text.find('"', at + 1);
        if (end == std::string::npos) {
            break;
        }
        if (end + 1 < text.size() && text[end + 1] == ':') {
            keys.insert(text.substr(at + 1, end - at - 1));
        }
        at = end + 1;
    }
    return keys;
}

/// Replace the FIRST occurrence of `from` in serialised text -- the support header's
/// `forged` for bytes this file already holds, exactly-once on the outer envelope.
std::string forge_first(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    INFO("looking for `", from, "` in: ", text);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

/// THE ONE SPELLING EVERY DOOR USES FOR A PATH. Workshop normalizes what it is handed
/// (`persist::resolved_against`: lexically normal, generic separators) and names the file
/// that way in its notices; a TempDir's native spelling (`C:/a/b` vs `C:` + backslashes) is
/// the same file by different bytes on Windows. The MSVC lane found this; compare spellings.
std::string spelled(const std::string& path) {
    return persist::resolved_against(std::string(), path);
}

std::string file_text(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// A DELIBERATE STRANGER HOLDING THE WEAVER NAMESPACE AS AN OFFICE, listening for every
/// shape the pane seam sends a provider. It exists so "a weaver-made pane is never routed
/// through the provider protocol" is a measurement: if Workshop ever addressed the weaver
/// namespace as an office, this is where the sentence would land.
class WeaverEars : public loom::WeaveBase<WeaverEars, SeenState,
                                         loom::Accept<PaneRoom, PanePressed, PaneKey,
                                                      PaneTextInput, PaneWheel>,
                                         loom::Emit<>> {
public:
    void on(const PaneRoom&, loom::Mail&) { ++state_.frames; }
    void on(const PanePressed&, loom::Mail&) { ++state_.frames; }
    void on(const PaneKey&, loom::Mail&) { ++state_.frames; }
    void on(const PaneTextInput&, loom::Mail&) { ++state_.frames; }
    void on(const PaneWheel&, loom::Mail&) { ++state_.frames; }
    std::int64_t heard() const { return state_.frames; }
};

WeaverEars* mount_weaver_ears(Live& t) {
    auto ears = std::make_unique<WeaverEars>();
    WeaverEars* raw = ears.get();
    loom::Grant grant;
    const loom::WeaveId id =
        t.bus.register_weave(std::move(ears), std::move(grant), std::string(kMakerPaneProvider));
    raw->zen_set_self(id);
    return raw;
}

} // namespace

// ============================================================================
// The value and its law
// ============================================================================

TEST_CASE("a definition is a name and a list of text regions with stable ids") {
    PaneDefinition d = new_definition("MyPane");
    REQUIRE(d.regions.size() == 1);
    CHECK(d.regions[0].id == kFirstRegionId);
    CHECK(d.regions[0].kind == region_kind::kText);
    CHECK(d.regions[0].text.empty());
    CHECK(d.regions[0].x == kNewRegionX);
    CHECK(d.regions[0].y == kNewRegionY);
    CHECK(d.regions[0].w == kNewRegionW);
    CHECK(d.regions[0].h == kNewRegionH);
    CHECK(d.next_id == kFirstRegionId + 1);
    CHECK(check_definition(d).accepted);
    // THE LIST IS THE SHAPE: a second region is a row, minted with the next id.
    REQUIRE(add_text_region(d, cells_px(1), cells_px(3), cells_px(5), cells_px(1)).accepted);
    REQUIRE(d.regions.size() == 2);
    CHECK(d.regions[1].id == 2);
    CHECK(d.next_id == 3);
    // ...AND AN ID IS NEVER REUSED: erase the first, mint again, and the mint continues.
    d.regions.erase(d.regions.begin());
    REQUIRE(add_text_region(d, 0, 0, cells_px(2), cells_px(2)).accepted);
    CHECK(d.regions.back().id == 3);
    CHECK(d.next_id == 4);
    CHECK(check_definition(d).accepted);
    // THE DOORS: text and each axis, refused in words and never clamped.
    CHECK(set_region_text(d, 2, "hello").accepted);
    CHECK(region_of(d, 2)->text == "hello");
    CHECK_FALSE(set_region_text(d, 2, std::string("caf\xC3\xA9")).accepted);
    CHECK(region_of(d, 2)->text == "hello");
    CHECK_FALSE(set_region_text(d, 2, std::string(kMaxRegionTextLen + 1, 'a')).accepted);
    CHECK_FALSE(set_region_text(d, 99, "x").accepted);
    CHECK(author_region_axis(d, 2, 0, 7).accepted);
    CHECK(region_of(d, 2)->x == 7);
    CHECK_FALSE(author_region_axis(d, 2, 0, -1).accepted);
    CHECK(region_of(d, 2)->x == 7);
    CHECK_FALSE(author_region_axis(d, 2, 2, 0).accepted);
    CHECK(region_of(d, 2)->w == cells_px(5));
    CHECK(author_region_axis(d, 2, 3, 20).accepted); // finer than a cell is honest intent
    CHECK(region_of(d, 2)->h == 20);
    CHECK_FALSE(author_region_axis(d, 2, 3, kRegionPxMax + 1).accepted);
}

TEST_CASE("the whole-definition law refuses what no door could have made") {
    const auto refused = [](PaneDefinition d) {
        const Written w = check_definition(d);
        return !w.accepted ? w.refusal : std::string();
    };
    CHECK(refused(new_definition("")) .find("cannot be empty") != std::string::npos);
    CHECK(refused(new_definition("My Pane")).find("no spaces") != std::string::npos);
    CHECK(refused(new_definition("a/b")).find("`/`") != std::string::npos);
    CHECK(refused(new_definition(std::string(kMaxWeaverPaneNameLen + 1, 'p')))
              .find("at most") != std::string::npos);
    CHECK(refused(new_definition(std::string("tab\there"))).find("no spaces") !=
          std::string::npos);
    CHECK(check_definition(new_definition(std::string(kMaxWeaverPaneNameLen, 'p'))).accepted);
    PaneDefinition bad_kind = new_definition("P");
    bad_kind.regions[0].kind = 7;
    CHECK(refused(bad_kind).find("`text`") != std::string::npos);
    PaneDefinition dup = new_definition("P");
    REQUIRE(add_text_region(dup, 0, 0, 1, 1).accepted);
    dup.regions[1].id = dup.regions[0].id;
    CHECK(refused(dup).find("share this id") != std::string::npos);
    PaneDefinition unminted = new_definition("P");
    unminted.regions[0].id = unminted.next_id;
    CHECK(refused(unminted).find("never minted") != std::string::npos);
    PaneDefinition zero = new_definition("P");
    zero.regions[0].id = 0;
    CHECK(refused(zero).find("never minted") != std::string::npos);
    PaneDefinition neg = new_definition("P");
    neg.regions[0].x = -1;
    CHECK(refused(neg).find("negative") != std::string::npos);
    PaneDefinition flat = new_definition("P");
    flat.regions[0].h = 0;
    CHECK(refused(flat).find("positive") != std::string::npos);
    PaneDefinition huge = new_definition("P");
    huge.regions[0].w = kRegionPxMax + 1;
    CHECK(refused(huge).find("at most") != std::string::npos);
    PaneDefinition bytes = new_definition("P");
    bytes.regions[0].text = "\x01";
    CHECK(refused(bytes).find("control") != std::string::npos);
    PaneDefinition many = new_definition("P");
    for (std::size_t i = 1; i < kMaxRegions; ++i) {
        REQUIRE(add_text_region(many, 0, 0, 1, 1).accepted);
    }
    CHECK(check_definition(many).accepted);
    CHECK_FALSE(add_text_region(many, 0, 0, 1, 1).accepted);
    many.regions.push_back(TextRegion{many.next_id - 1, region_kind::kText, 0, 0, 1, 1, ""});
    CHECK(refused(many).find("at most") != std::string::npos);
}

// ============================================================================
// The pane file, and what it cannot say
// ============================================================================

TEST_CASE("the pane file round-trips, refuses by number and by shape, and holds nothing "
          "but the definition") {
    PaneDefinition d = new_definition("MyPane");
    REQUIRE(set_region_text(d, 1, "hello from data").accepted);
    // A VALUE NO TERMINAL CAN SAY EXACTLY -- 12 cells and 6 pixels, which a window authors --
    // written exactly, in the file's own sub-units (four to the pixel), quantized by nobody.
    REQUIRE(author_region_axis(d, 1, 0, cells_px(12) + 6).accepted);
    const std::string text = pdp::to_text(d);
    CHECK(text.find("\"zengine-workshop-pane\"") != std::string::npos);
    CHECK(text.find("\"text\"") != std::string::npos);
    CHECK(text.find("\"600\"") != std::string::npos); // (cells_px(12) + 6) * 4
    // THE KEYS THE FILE SPELLS, EXACTLY -- and not one of them is a medium's fact.
    const std::set<std::string> keys = json_keys(text);
    const std::set<std::string> expected{"zen",     "schema",  "version", "content_id", "fields",
                                         "format",  "format_version", "name",  "next_id",
                                         "regions", "id",      "kind",    "x",          "y",
                                         "width",   "height",  "text"};
    CHECK(keys == expected);
    for (const char* forbidden : {"px", "cell", "pixel", "row", "column", "canvas", "metric",
                                  "callback", "role", "path", "grant", "provider", "operator"}) {
        INFO(forbidden);
        CHECK(text.find(forbidden) == std::string::npos);
    }
    // THE SHAPES ARE THE ONLY THING SERIALISED, and their fields are pinned by name.
    std::vector<std::string> region_fields;
    for (const loom::Field& f : loom::schema_of<pdp::WorkshopPaneRegion>()->fields()) {
        region_fields.push_back(f.name);
    }
    CHECK(region_fields ==
          std::vector<std::string>{"id", "kind", "x", "y", "width", "height", "text"});
    std::vector<std::string> file_fields;
    for (const loom::Field& f : loom::schema_of<pdp::WorkshopPaneDefinition>()->fields()) {
        file_fields.push_back(f.name);
    }
    CHECK(file_fields ==
          std::vector<std::string>{"format", "format_version", "name", "next_id", "regions"});
    // ROUND TRIP: the value, and then the bytes a second time.
    const pdp::LoadedDefinition back = pdp::from_text(text);
    REQUIRE_MESSAGE(back.outcome.accepted, back.outcome.refusal);
    CHECK(back.definition == d);
    CHECK(pdp::to_text(back.definition) == text);
    // REFUSED BY NUMBER, BEFORE THE ROWS: a foreign version is named as a version.
    const std::string v2 = forge_first(text, "\"version\":1", "\"version\":2");
    const pdp::LoadedDefinition other = pdp::from_text(v2);
    CHECK_FALSE(other.outcome.accepted);
    CHECK(other.outcome.refusal.find("version 2") != std::string::npos);
    CHECK(other.definition.regions.empty());
    // ...AND THE FIELD'S OWN NUMBER, for the forgery only a reader of this format makes.
    const pdp::LoadedDefinition forged_field =
        pdp::from_text(forge_first(text, "\"format_version\":\"1\"", "\"format_version\":\"7\""));
    CHECK_FALSE(forged_field.outcome.accepted);
    CHECK(forged_field.outcome.refusal.find("version 7") != std::string::npos);
    // A WRONG FORMAT WORD, AN UNKNOWN FIELD, AN UNKNOWN KIND WORD: each refused whole.
    CHECK_FALSE(pdp::from_text(forge_first(text, "zengine-workshop-pane", "zengine-workshop-setup"))
                    .outcome.accepted);
    CHECK_FALSE(pdp::from_text(forge_first(text, "\"next_id\"", "\"next_id\":\"2\",\"pixels\""))
                    .outcome.accepted);
    const pdp::LoadedDefinition kind =
        pdp::from_text(forge_first(text, "\"kind\":\"text\"", "\"kind\":\"button\""));
    CHECK_FALSE(kind.outcome.accepted);
    CHECK(kind.outcome.refusal.find("`button`") != std::string::npos);
    CHECK(kind.outcome.refusal.find("text") != std::string::npos);
    // THE DEFINITION'S OWN LAW IS THE LAST LAYER, in its own words.
    const pdp::LoadedDefinition law =
        pdp::from_text(forge_first(text, "\"height\":\"96\"", "\"height\":\"0\""));
    CHECK_FALSE(law.outcome.accepted);
    CHECK(law.outcome.refusal.find("positive") != std::string::npos);
    // NOT A DEFINITION AT ALL.
    CHECK_FALSE(pdp::from_text("{").outcome.accepted);
    CHECK_FALSE(pdp::from_text("").outcome.accepted);
    // THE FILE: a safe write, a read with a ceiling, and a file too large to be one.
    TempDir dir("wux14-file");
    const std::string path = dir.file("pane.json");
    REQUIRE(pdp::save_file(path, d).accepted);
    CHECK(slurp(path) == text);
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));
    const pdp::LoadedDefinition read = pdp::load_file(path);
    REQUIRE(read.outcome.accepted);
    CHECK(read.definition == d);
    spillout(path, std::string(pdp::kMaxPaneDefinitionBytes + 1, '{'));
    const pdp::LoadedDefinition big = pdp::load_file(path);
    CHECK_FALSE(big.outcome.accepted);
    CHECK(big.outcome.refusal.find("larger than a Workshop pane definition can be") !=
          std::string::npos);
    CHECK_FALSE(pdp::load_file(dir.file("absent.json")).outcome.accepted);
    // THE CEILING HOLDS A MAXIMAL LEGAL DEFINITION -- a file this build writes is never one
    // it refuses to read.
    PaneDefinition maximal;
    maximal.name = std::string(kMaxWeaverPaneNameLen, 'p');
    for (std::size_t i = 0; i < kMaxRegions; ++i) {
        REQUIRE(add_text_region(maximal, kRegionPxMax, kRegionPxMax, kRegionPxMax,
                                kRegionPxMax)
                    .accepted);
        REQUIRE(set_region_text(maximal, maximal.regions.back().id,
                                std::string(kMaxRegionTextLen, '"'))
                    .accepted);
    }
    REQUIRE(check_definition(maximal).accepted);
    CHECK(pdp::to_text(maximal).size() <= pdp::kMaxPaneDefinitionBytes);
    REQUIRE(pdp::save_file(path, maximal).accepted);
    CHECK(pdp::load_file(path).outcome.accepted);
}

TEST_CASE("the definition and its file are structurally unable to act") {
    // A CLAIM ABOUT WHAT CODE DOES NOT CONTAIN cannot be proved by running it, so it is
    // read off the source: the value and the file may name no bus, no kernel, no grant, no
    // operator, no keymap and no callable. The behavioural half is the live case below.
    for (const char* path : {WORKSHOP_PANE_DEFINITION_HPP, WORKSHOP_PANE_DEFINITION_PERSIST_HPP}) {
        INFO(path);
        const std::string source = file_text(path);
        REQUIRE_FALSE(source.empty());
        for (const char* forbidden :
             {"mail.", "send_to", "publish(", "Kernel", "dlopen", "LoadWeave", "mount(",
              "evaluate(", "Grant", "allow_", "keymap", "Act::", "std::function", "Catalog",
              "operator/", "op::migrate", "Switchboard", "SampleDoor", "PaneRoom",
              "PaneOffered", "role::"}) {
            INFO(forbidden);
            CHECK(source.find(forbidden) == std::string::npos);
        }
    }
}

// ============================================================================
// A pane from data, on the desk, through the ordinary path
// ============================================================================

TEST_CASE("the weaver door makes a named pane from data, and it lives on the desk exactly as every "
          "other pane does") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    const WeaverPaneAnswered made = make_pane(t, "MyPane");
    const Session& s = t.session();
    // THE VALUE: one open definition, one empty text region, minted #1.
    REQUIRE(s.panes.weaver.definition.regions.size() == 1);
    CHECK(s.panes.weaver.definition.regions[0].id == kFirstRegionId);
    CHECK(s.panes.weaver.definition.regions[0].text.empty());
    CHECK(s.panes.weaver.dirty()); // never saved: dirty by arithmetic
    // THE ASKER IS TOLD WHAT THE BAND SAYS, and the sentence says where the region's rows
    // are; the door names nothing inspected.
    CHECK(made.said.find("Pane Creator: MyPane is on this layout") == 0);
    CHECK(made.said.find("inspect it in Info to write its text region") != std::string::npos);
    // THE IDENTITY: minted from the name under Workshop's own namespace, and it resolves.
    CHECK(kMine == PaneRef{kMakerPaneProvider, "MyPane"});
    REQUIRE(resolve_pane(kMine, s.panes).has_value());
    CHECK(*resolve_pane(kMine, s.panes) == kWeaverPaneKind);
    CHECK(kind_name(s.panes, kWeaverPaneKind) == "MyPane");
    // THE ORDINARY PANE PATH: a setup row, a seat, a rectangle, an occupancy, a state.
    REQUIRE(has_pane(s.setup.active, kMine));
    CHECK(s.panes.has(kWeaverPaneKind));
    const Screen sc = screen_of(s);
    const PaneBounds where = bounds_of(s.panes, s.setup.active, kWeaverPaneKind, sc);
    REQUIRE(where.open);
    REQUIRE_FALSE(where.rect.empty());
    CHECK(placement_of(kWeaverPaneKind) == placement::kOverlayStack);
    const ui::Rect cells = cells_covered(where.rect);
    const Occupancy here = occupied_at(s.panes, s.setup.active, sc, cells.x + 2, cells.y + 2);
    CHECK(here.occupied);
    CHECK(here.kind == kWeaverPaneKind);
    CHECK(here.what == "MyPane");
    bool listed = false;
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panes)) {
        if (row.ref == kMine) {
            listed = true;
            CHECK(row.kind == kWeaverPaneKind);
            CHECK(row.name == "MyPane");
            CHECK(row.summary == kWeaverPaneSummary);
            CHECK(pane_state_of(s.panes, s.setup.active, sc, row) == pane_state::kOpen);
        }
    }
    CHECK(listed);
    // THE SUBJECT IS THE INSPECTOR'S, named through its door (`make_pane` asks as Info does);
    // which row a weaver's keys are on is Info's own, in Info's image.
    CHECK(s.inspected.ref == kMine);
    CHECK(pane_value(t, "Name") == "MyPane");
    CHECK(pane_value(t, "Identity") == "zengine.workshop.maker/MyPane");
    CHECK(pane_value(t, "Provider") == "zengine.workshop.maker (made here -- Pane Creator)");
    CHECK(pane_value(t, "Summary") == kWeaverPaneSummary);
    CHECK(pane_value(t, "State") == "open");
    // THE INTERIOR ROWS, in order, under their own section.
    const std::size_t at = interior_section(t);
    REQUIRE(at < s.inspected.rows.size());
    const char* const expected[] = {"Region", "Text", "X", "Y", "Width", "Height", "Resolved",
                                    "Shown"};
    REQUIRE(s.inspected.rows.size() == at + 1 + sizeof(expected) / sizeof(expected[0]));
    for (std::size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        INFO(i);
        CHECK(s.inspected.rows[at + 1 + i].label() == expected[i]);
    }
    CHECK(region_value(t, "Region") == "#1 text -- the Pane Creator made it");
    CHECK(region_value(t, "X") == "0 cells");
    CHECK(region_value(t, "Width") == "24 cells");
    CHECK(region_value(t, "Height") == "2 cells");
    // THE PANE IS PAINTED, AND THE CATALOG GREW BY ONE ROW AND NOTHING ELSE: the built-ins, the
    // two stand-ins every `Live` admits (two runtime rows), and now the weaver's.
    CHECK(combined_catalog(s.panes).size() == kBuiltinPaneCount + 3);
    CHECK(s.panes.runtime.entries.size() == 2);
    CHECK(s.panes.external.empty());
    // A PRESS INTO IT SELECTS IT AND POINTS NO KEYS (it takes none), like Info or Layouts.
    t.press_canvas(cells.x + 2, cells.y + 2);
    CHECK(t.session().panes.selected == kWeaverPaneKind);
    CHECK(t.session().panes.keyboard == kNoPaneKind);
    CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    CHECK(t.notice().find("MyPane is here") == 0);
}

TEST_CASE("the weaver's pane is edited, ordered and removed by the doors every pane has, and comes "
          "back through the session by its reference") {
    TempDir dir("wux14-path");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.host.pane_path = dir.file("pane.json");
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
        make_pane(t, "MyPane");
        // THE PANE'S OWN AUTHORED ROWS -- the inspected subject's, through the commit door --
        // move it.
        const Screen sc = screen_of(t.session());
        const PixelRect was = bounds_of(t.session().panes, t.session().setup.active,
                                       kWeaverPaneKind, sc)
                                 .rect;
        const PaneSubjectActed wrote = hand_commit(t, "X", "30"); // AUTHORED X, the pane's
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        const PixelRect now = bounds_of(t.session().panes, t.session().setup.active,
                                       kWeaverPaneKind, sc)
                                 .rect;
        CHECK(now.x == cells_px(30));
        CHECK(now.x != was.x);
        CHECK(pane_of(t.session().setup.active, kMine)->place.x == cells_px(30));
        // ORDER: the arrangement's own door, on the weaver's reference.
        enter_arrange_desk(t);
        select_pane(t, kMine);
        t.key(input::scan::kB);
        CHECK(pane_of(t.session().setup.active, kMine)->front == 0);
        for (int i = 0; i < 3 && t.session().arrange.open; ++i) {
            t.key(input::scan::kEscape);
        }
        REQUIRE_FALSE(t.session().arrange.open);
        // PARTICIPATION: the close door removes it, and the definition stands.
        REQUIRE(hand_close(t, kMine).closed);
        CHECK_FALSE(has_pane(t.session().setup.active, kMine));
        CHECK_FALSE(t.session().panes.has(kWeaverPaneKind));
        CHECK(t.session().panes.weaver.open());
        CHECK(t.session().inspected.ref == kMine);
        REQUIRE(hand_launch(t, kMine).refusal.empty());
        CHECK(has_pane(t.session().setup.active, kMine));
        CHECK(t.session().panes.has(kWeaverPaneKind));
        // SAVE, THEN LEAVE STANDING ON THE DESK.
        REQUIRE(save_pane(t).accepted);
        CHECK_FALSE(t.session().panes.weaver.dirty());
        t.press(90, 35);
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    Live back;
    back.host.session_path = session;
    back.host.pane_path = dir.file("pane.json");
    back.publish(loom::to_value(surface::SurfaceReady{}));
    back.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    CHECK(back.session().panes.weaver.open());
    CHECK(has_pane(back.session().setup.active, kMine));
    CHECK(back.session().panes.has(kWeaverPaneKind));
    // THE SESSION HOLDS WHERE; THE PANE FILE HOLDS WHAT.
    CHECK(slurp(session).find("\"zengine.workshop.maker\"") != std::string::npos);
    CHECK(slurp(session).find("\"regions\"") == std::string::npos);
}

// ============================================================================
// Identity is minted from the name, never from what happens to be open
// ============================================================================

TEST_CASE("a weaver pane's identity is its name under Workshop's namespace -- not a singleton that "
          "follows the open file") {
    // ⚔ MUTATION: resolve every weaver-namespace reference to whatever definition is
    // open. Then `MyPane`'s row would resolve while `Other` is the open pane; the checks
    // below say it does not.
    TempDir dir("wux14-identity");
    Live t;
    t.host.pane_path = dir.file("pane.json");
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "MyPane");
    REQUIRE(save_pane(t).accepted); // saved, so a second pane may be made
    make_pane(t, "Other");
    const Session& s = t.session();
    CHECK(resolve_pane(weaver_pane_ref("Other"), s.panes) == kWeaverPaneKind);
    CHECK_FALSE(resolve_pane(kMine, s.panes).has_value());
    // MyPane's row on the desk is RETAINED and reads unresolved -- intent, never erased.
    REQUIRE(has_pane(s.setup.active, kMine));
    REQUIRE(has_pane(s.setup.active, weaver_pane_ref("Other")));
    const Screen sc = screen_of(s);
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panes)) {
        if (row.ref == kMine) {
            CHECK(row.kind == kNoPaneKind);
            CHECK(pane_state_of(s.panes, s.setup.active, sc, row) == pane_state::kUnresolved);
        }
    }
    // TWO ROWS WAIT, AND ONLY ONE OF THEM IS THIS CASE'S. The other is the desk's Info
    // weave, which no office in this rig offers -- every layout here carries it, so a bare
    // count would be a claim about the rig's load plan rather than about the weaver pane.
    const std::vector<PaneRef> waiting = unresolved_panes(s.setup.active, s.panes);
    REQUIRE(waiting.size() == 2);
    CHECK(waiting[0] == info_ref());
    CHECK(waiting[1] == kMine);
    // A REFERENCE WITH THE NAMESPACE AND ANY OTHER NAME RESOLVES TO NOTHING.
    CHECK_FALSE(resolve_pane(weaver_pane_ref("other"), s.panes).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{kMakerPaneProvider, ""}, s.panes).has_value());
    // ...AND NO OFFICE MAY OFFER A PANE IN THE WEAVER NAMESPACE.
    RuntimeCatalog cat;
    const Admission refused = admit_pane_offer(cat, kMakerPaneProvider, good_offer());
    CHECK_FALSE(refused.written.accepted);
    CHECK(refused.written.refusal.find("namespace for panes a weaver made") != std::string::npos);
    CHECK(cat.entries.empty());
    // THE SUBJECT'S ROWS SAY SO, for the pane whose file is not open.
    REQUIRE(hand_inspect(t, kMine).accepted);
    CHECK(pane_value(t, "Provider").find("no open definition is named MyPane") !=
          std::string::npos);
    CHECK(region_value(t, "Interior") == "no open definition is named MyPane -- nothing to show");
}

// ============================================================================
// The frame is the interior, the lattice is fine, and each face is honest
// ============================================================================

TEST_CASE("a region is placed relative to the pane's INTERIOR and painted through the ordinary "
          "pane path in cells") {
    // ⚔ MUTATION: resolve the region against the canvas origin instead of the
    // interior's. The stack slot's interior begins one cell in on a terminal and many rows
    // down for a second slot, so the region's resolved place would land elsewhere.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "MyPane");
    REQUIRE(type_region_value(t, "Text", "hello from data").accepted);
    CHECK(t.notice() == "committed Text of MyPane = hello from data");
    const PixelRect interior = interior_of(t);
    CHECK(interior.x > 0);
    CHECK(interior.y > 0);
    RegionPresentation p = presentation_of(t);
    REQUIRE(p.present);
    CHECK(p.shown.x == interior.x);
    CHECK(p.shown.y == interior.y);
    CHECK_FALSE(p.clipped);
    CHECK(region_value(t, "Resolved") == "@0,0 24x2 cells");
    CHECK(region_value(t, "Shown") == "2 rows x 24 columns, presented as cells");
    // THE TEXT IS ON THE PANE, at the interior's cell, off the published canvas.
    const ui::Rect cells = cells_covered(interior);
    const std::vector<surface::SurfaceTextRegion> found =
        regions_at(t.canvases.back(), cells.x, cells.y);
    bool said = false;
    for (const surface::SurfaceTextRegion& r : found) {
        for (const surface::SurfaceTextRow& row : r.rows) {
            said = said || row.text == "hello from data";
        }
    }
    CHECK(said);
    CHECK(weaver_pane_text(t).find("hello from data") != std::string::npos);
    // MOVE THE REGION INSIDE THE PANE, and it follows the interior's origin, not the canvas's.
    type_region_value(t, "X", "2");
    type_region_value(t, "Y", "1");
    p = presentation_of(t);
    CHECK(p.shown.x == interior.x + cells_px(2));
    CHECK(p.shown.y == interior.y + cells_px(1));
    // ONE `kGroundOwn` REGION AT THE NEW CELL -- the pane's own; the creator's mark writes a
    // second, `kGroundBeneath`, region at the same place on a later plane, deliberately.
    std::size_t own = 0;
    for (const surface::SurfaceTextRegion& r :
         regions_at(t.canvases.back(), cells.x + 2, cells.y + 1)) {
        own += r.ground == surface::kGroundOwn ? 1 : 0;
    }
    CHECK(own == 1);
    CHECK(region_value(t, "Resolved") == "@2,1 24x2 cells");
    // A REGION AUTHORED PAST THE PANE IS LEGAL INTENT, CLIPPED AT PRESENTATION.
    type_region_value(t, "Width", "400");
    p = presentation_of(t);
    CHECK(p.clipped);
    CHECK(surface::add_cells(p.shown.x, p.shown.w) == surface::add_cells(interior.x, interior.w));
    CHECK(t.session().panes.weaver.definition.regions[0].w == cells_px(400));
    CHECK(region_value(t, "Resolved").find("(clipped by the pane)") != std::string::npos);
    CHECK(region_value(t, "Width") == "400 cells");
    // MOVING THE PANE MOVES THE REGION WITH IT -- the definition is untouched.
    const std::string before = definition_bytes(t);
    REQUIRE(hand_commit(t, "Y", "30").accepted); // the pane's AUTHORED Y
    const PixelRect moved = interior_of(t);
    CHECK(moved.y == cells_px(30) + kChromePx);
    CHECK(presentation_of(t).shown.y == moved.y + cells_px(1));
    CHECK(definition_bytes(t) == before);
}

TEST_CASE("one authored fine value, read in pixels on the window and projected to cells on a "
          "terminal, and looking writes nothing back") {
    // ⚔ MUTATION: a readout, a repaint or a face change that rewrites the
    // authored number to the projected one. The bytes are compared before and after.
    Live t;
    sdl_face(t);
    make_pane(t, "MyPane");
    CHECK(region_value(t, "X") == "0 px");
    CHECK(region_value(t, "Width") == "288 px");
    CHECK(region_value(t, "Height") == "24 px");
    // A PIXEL THAT IS NOT A CELL: 126 px is 10 cells and 6 pixels.
    REQUIRE(type_region_value(t, "X", "126").accepted);
    CHECK(t.notice() == "committed X of MyPane = 126 px");
    CHECK(t.session().panes.weaver.definition.regions[0].x == cells_px(10) + 6);
    CHECK(region_value(t, "X") == "126 px");
    CHECK(region_value(t, "Resolved") == "@126,0 288x24 px");
    CHECK(region_value(t, "Shown") == "1 row x 35 columns, presented in type");
    RegionPresentation p = presentation_of(t);
    CHECK(p.fit.graphical());
    CHECK(p.fit.rows == 1);
    CHECK(p.fit.view.x == (interior_of(t).x) + 126);
    const std::string authored = definition_bytes(t);
    // THE OTHER FACE'S WORD IS REFUSED, NOT CONVERTED -- to the asker, in the owner's words.
    const PaneSubjectActed other = type_region_value(t, "Y", "2 cells");
    CHECK_FALSE(other.accepted);
    CHECK(other.refusal == "Y: this face reads px, not cells");
    CHECK(definition_bytes(t) == authored);
    // THE SAME DESK ON A TERMINAL: the same value, honestly projected, marked.
    tui_face(t);
    CHECK(region_value(t, "X") == "~10 cells (~ projected)");
    CHECK(region_value(t, "Width") == "24 cells");
    CHECK(region_value(t, "Resolved") == "@~10,0 24x2 cells (~ projected)");
    CHECK(region_value(t, "Shown") == "2 rows x 24 columns, presented as cells");
    p = presentation_of(t);
    CHECK_FALSE(p.fit.graphical());
    // THE PUBLISHED REGION IS AT ITS PIXEL ON THE WIRE, for the cell projection to floor at
    // its own grain and the window to draw where it is.
    bool carried = false;
    for (const surface::SurfaceTextRegion& r : all_texts(t.canvases.back())) {
        if (r.x == interior_of(t).x + cells_px(10) + 6) {
            carried = true;
        }
    }
    CHECK(carried);
    CHECK(definition_bytes(t) == authored);
    // AND A TERMINAL'S OWN TYPING AUTHORS CELLS, exactly.
    REQUIRE(type_region_value(t, "X", "11 cells").accepted);
    CHECK(t.session().panes.weaver.definition.regions[0].x == cells_px(11));
    CHECK(region_value(t, "X") == "11 cells");
    sdl_face(t);
    CHECK(region_value(t, "X") == "132 px");
    // LOOKING NEVER AUTHORS: faces, extents and repaints leave the bytes identical.
    const std::string again = definition_bytes(t);
    for (int i = 0; i < 3; ++i) {
        tui_face(t, 100 + 10 * i, 40 + 2 * i);
        (void)region_value(t, "Resolved");
        (void)region_value(t, "Shown");
        sdl_face(t, 120 + 10 * i, 50 + 2 * i);
        (void)region_value(t, "Resolved");
        (void)region_value(t, "Shown");
    }
    CHECK(definition_bytes(t) == again);
}

TEST_CASE("a region too small for the face is the face's own answer, and the authored value is not "
          "rewritten to fit") {
    Live t;
    sdl_face(t);
    make_pane(t, "MyPane");
    type_region_value(t, "Text", "small");
    // ONE CELL TALL holds no row of an 18-pixel line: the cell projection, honestly named.
    REQUIRE(type_region_value(t, "Height", "12").accepted);
    CHECK(region_value(t, "Shown") == "1 row x 24 columns, presented as cells");
    CHECK(region_value(t, "Height") == "12 px");
    CHECK(t.session().panes.weaver.definition.regions[0].h == cells_px(1));
    // THREE PIXELS TALL covers no cell and no row: nothing is drawn, and it says so.
    REQUIRE(type_region_value(t, "Height", "3").accepted);
    CHECK(region_value(t, "Shown") == "no room -- nothing of it is drawn on this face");
    CHECK(t.session().panes.weaver.definition.regions[0].h == 3);
    CHECK(region_value(t, "Height") == "3 px");
    // ...AND A TERMINAL SAYS THE SAME THING IN ITS OWN GRAIN, marking what it cannot say.
    tui_face(t);
    CHECK(region_value(t, "Height") == "~0 cells (~ projected)");
    CHECK(region_value(t, "Shown") == "no room -- nothing of it is drawn on this face");
    CHECK(t.session().panes.weaver.definition.regions[0].h == 3);
}

// ============================================================================
// The region mark, and the rows as the one door
// ============================================================================

TEST_CASE("the Pane Creator marks the region it is editing on the pane itself, from the same "
          "resolution, and writes nothing") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "MyPane");
    type_region_value(t, "Text", "marked");
    const std::string before = definition_bytes(t);
    const RegionPresentation p = presentation_of(t);
    const surface::SurfaceRect mark = wire_rect_of(p.shown, kRegionMark);
    // THE MARK IS A LATER PLANE: an accent rectangle at exactly the region's resolved bounds,
    // and the text written OVER it on somebody else's ground.
    const surface::SurfaceCanvas& c = t.canvases.back();
    bool marked = false;
    bool written_over = false;
    std::size_t pane_plane = c.layers.size();
    std::size_t mark_plane = c.layers.size();
    for (std::size_t i = 0; i < c.layers.size(); ++i) {
        for (const surface::SurfaceRect& r : c.layers[i].rects) {
            if (r.x == mark.x && r.y == mark.y && r.w == mark.w && r.h == mark.h &&
                r.role == kRegionMark) {
                marked = true;
                mark_plane = i;
            }
        }
        for (const surface::SurfaceTextRegion& r : c.layers[i].texts) {
            if (r.x == mark.x && r.y == mark.y) {
                if (r.ground == surface::kGroundBeneath && !r.rows.empty() &&
                    r.rows[0].text == "marked") {
                    written_over = true;
                } else if (r.ground == surface::kGroundOwn) {
                    pane_plane = i;
                }
            }
        }
    }
    CHECK(marked);
    CHECK(written_over);
    CHECK(pane_plane < mark_plane);
    CHECK(definition_bytes(t) == before);
    // ANOTHER SUBJECT, NO MARK: the mark is derived from the subject, held nowhere.
    REQUIRE(hand_inspect(t, ref_of(pane_kind::kLayouts)).accepted);
    CHECK(creator_subject_region(t.session()) == nullptr);
    bool still = false;
    for (const surface::SurfaceRect& r : all_rects(t.canvases.back())) {
        still = still || (r.x == mark.x && r.y == mark.y && r.role == kRegionMark && r.w == mark.w);
    }
    CHECK_FALSE(still);
    // ...AND BACK, IT RETURNS -- and closing the pane takes the mark off with nothing to clear:
    // the subject is the inspector's to move, and the host holds no mark to forget.
    REQUIRE(hand_inspect(t, kMine).accepted);
    CHECK(creator_subject_region(t.session()) != nullptr);
    REQUIRE(hand_close(t, kMine).closed);
    bool left = false;
    for (const surface::SurfaceRect& r : all_rects(t.canvases.back())) {
        left = left || r.role == kRegionMark;
    }
    CHECK_FALSE(left);
    CHECK(definition_bytes(t) == before);
}

TEST_CASE("Text and the four numbers are edited through the definition's doors, refused in words, "
          "and clamped never") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "MyPane");
    const std::string before = definition_bytes(t);
    // REFUSED TO THE ASKER, IN THE OWNER'S WORDS -- and the band says nothing, because the
    // draft that was refused is the inspector's own line, which keeps it.
    const std::string notice = t.notice();
    const auto refused = [&t](const char* label, const std::string& text) {
        const PaneSubjectActed said = type_region_value(t, label, text);
        CHECK_FALSE(said.accepted);
        return said.refusal;
    };
    CHECK(refused("X", "-") ==
          "X: a region has no default to reset to -- type a whole number of cells");
    CHECK(refused("Width", "0") == "Width: a region width must be positive");
    CHECK(refused("Y", "abc").find("Y: not a whole number of cells") == 0);
    CHECK(refused("Height", "-4") == "Height: a region height must be positive");
    CHECK(refused("X", "5000") == "X: a region place is at most 4096 cells");
    CHECK(t.notice() == notice);
    CHECK(definition_bytes(t) == before);
    // TEXT: plain ASCII, judged at the door; a byte the media cannot draw is refused whole.
    REQUIRE(type_region_value(t, "Text", "plain words").accepted);
    CHECK(t.session().panes.weaver.definition.regions[0].text == "plain words");
    CHECK(refused("Text", "plain words\xC3\xA9") ==
          "Text: a text region holds plain ASCII with no control characters");
    CHECK(t.session().panes.weaver.definition.regions[0].text == "plain words");
    // THE AUTHORED ROWS OF THE PANE AND THE REGION SHARE LABELS AND NOT DOORS: the pane's X
    // is the setup's, the region's X is the definition's.
    type_region_value(t, "X", "3");
    CHECK(t.session().panes.weaver.definition.regions[0].x == cells_px(3));
    CHECK(pane_of(t.session().setup.active, kMine)->place == PanePlace{});
    CHECK(pane_value(t, "X") == "-");
    CHECK(region_value(t, "X") == "3 cells");
}

// ============================================================================
// A code-backed pane is a capture, never a decomposition
// ============================================================================

TEST_CASE("a code-backed subject's interior is a read-only capture, and an unresolved one is "
          "nothing to inspect") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    REQUIRE(hand_inspect(t, ref_of(pane_kind::kLayouts)).accepted);
    const std::size_t at = interior_section(t);
    REQUIRE(at + 2 == t.session().inspected.rows.size());
    CHECK(t.session().inspected.rows[at + 1].label() == "Interior");
    CHECK_FALSE(t.session().inspected.rows[at + 1].editable());
    const std::string capture = region_value(t, "Interior");
    CHECK(capture.find("code-backed -- body @") == 0);
    CHECK(capture.find("as cells; no authored interior") != std::string::npos);
    CHECK(region_row(t, "Text") == nullptr);
    // THE CAPTURE IS THE RESOLVED BODY, from the same place the painter resolves.
    const Screen sc = screen_of(t.session());
    const ProsePlace place = prose_place(
        bounds_of(t.session().panes, t.session().setup.active, pane_kind::kLayouts, sc).rect, sc);
    CHECK(capture.find(pixel_rect_text(place.inside, 0)) != std::string::npos);
    CHECK(capture.find(std::to_string(place.rows) + " rows x ") != std::string::npos);
    // A CLOSED PANE: not presented, and said so -- and the one closed pane a fresh desk has
    // is the runtime stand-in, whose interior is its provider's.
    REQUIRE(hand_inspect(t, ref_of(stock::kKind)).accepted);
    CHECK(region_value(t, "Interior") == "a provider's own -- not presented; no authored interior");
    // AN UNRESOLVED STRANGER: nothing to inspect, and no pretence.
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    REQUIRE(hand_inspect(t, stranger()).accepted);
    CHECK(region_value(t, "Interior") == "unresolved -- nothing to inspect");
}

// ============================================================================
// The one-open-definition lifecycle
// ============================================================================

TEST_CASE("dirty pane truth refuses the quit, a second new pane and a replacing open until the "
          "weaver saves or discards") {
    // ⚔ MUTATION: a quit, a naming or an open that proceeds over a dirty definition.
    TempDir dir("wux14-dirty");
    const std::string path = dir.file("pane.json");
    {
        PaneDefinition other = new_definition("FromDisk");
        REQUIRE(pdp::save_file(path, other).accepted);
    }
    Live t;
    t.host.pane_path = path;
    t.host.session_path = dir.file("session.json");
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "MyPane");
    REQUIRE(t.session().panes.weaver.dirty());
    // THE QUIT, FROM ALL THREE DOORS.
    t.press(90, 35);
    t.key(input::scan::kQ);
    CHECK_FALSE(t.host.quit);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("pane MyPane has unsaved changes") == 0);
    CHECK(t.notice().find("Workshop stays open") != std::string::npos);
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(t.host.quit);
    t.close_requested();
    CHECK_FALSE(t.host.quit);
    CHECK_FALSE(std::filesystem::exists(dir.file("session.json")));
    // A SECOND NEW PANE, asked at the weaver door: refused, and nothing is made.
    const WeaverPaneAnswered another = hand_weaver(t, weaver_pane_act::kCreate, "Second");
    CHECK_FALSE(another.accepted);
    CHECK(another.said.find("pane MyPane has unsaved changes") == 0);
    CHECK(another.said.find("nothing was made") != std::string::npos);
    CHECK_FALSE(has_pane(t.session().setup.active, weaver_pane_ref("Second")));
    // A REPLACING OPEN -- the startup load, arriving after a weaver has already made one.
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.session().panes.weaver.definition.name == "MyPane");
    CHECK(t.notice().find("nothing was opened") != std::string::npos);
    CHECK(t.session().panes.weaver.dirty());
    // SAVE IS THE WAY OUT: the file is written, the pane is clean, the quit proceeds.
    const WeaverPaneAnswered saved = save_pane(t);
    REQUIRE_MESSAGE(saved.accepted, saved.said);
    CHECK(saved.said == "saved pane MyPane to " + spelled(path));
    CHECK(t.notice() == saved.said);
    CHECK_FALSE(t.session().panes.weaver.dirty());
    CHECK(pdp::load_file(path).definition.name == "MyPane");
    t.press(90, 35);
    t.key(input::scan::kQ);
    CHECK(t.host.quit);
    CHECK(std::filesystem::exists(dir.file("session.json")));
}

TEST_CASE("the discard door puts a saved pane back to its file, and closes a pane that was never "
          "saved while keeping its row") {
    TempDir dir("wux14-discard");
    Live t;
    t.host.pane_path = dir.file("pane.json");
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "MyPane");
    // NEVER SAVED: the discard closes it whole; the row is intent and stays.
    REQUIRE(discard_pane(t).accepted);
    CHECK_FALSE(t.session().panes.weaver.open());
    CHECK(t.notice().find("never saved") != std::string::npos);
    CHECK(has_pane(t.session().setup.active, kMine));
    CHECK_FALSE(t.session().panes.has(kWeaverPaneKind));
    CHECK_FALSE(resolve_pane(kMine, t.session().panes).has_value());
    CHECK(t.session().inspected.ref == kMine); // the subject stands, honestly unresolved
    CHECK(region_value(t, "Interior") == "no open definition is named MyPane -- nothing to show");
    // SAVED, THEN EDITED: the discard is the file's value again.
    make_pane(t, "Again");
    REQUIRE(type_region_value(t, "Text", "kept").accepted);
    REQUIRE(save_pane(t).accepted);
    REQUIRE(type_region_value(t, "Text", "lost").accepted);
    CHECK(t.session().panes.weaver.dirty());
    REQUIRE(discard_pane(t).accepted);
    CHECK_FALSE(t.session().panes.weaver.dirty());
    CHECK(t.session().panes.weaver.definition.regions[0].text == "kept");
    CHECK(t.notice().find("back to what") != std::string::npos);
    CHECK(discard_pane(t).said.find("nothing to discard") != std::string::npos);
    // SAVE WITH NO FILE: a pane made in a run with no pane path is refused in words.
    Live nowhere;
    nowhere.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    CHECK(make_pane(nowhere, "Floating").said.find("(no pane file this run)") !=
          std::string::npos);
    const WeaverPaneAnswered unsaved = save_pane(nowhere);
    CHECK_FALSE(unsaved.accepted);
    CHECK(nowhere.session().notice_is_bad);
    CHECK(unsaved.said == "no pane file -- start Workshop with --pane <path>");
    CHECK(nowhere.session().panes.weaver.dirty());
}

TEST_CASE("a malformed file cannot replace a live definition, and a refused file is "
          "never written over") {
    // ⚔ MUTATION: an open that installs fields of a candidate before the whole has
    // been judged, or a save that writes over bytes this run could not read.
    TempDir dir("wux14-malformed");
    const std::string path = dir.file("pane.json");
    Live t;
    t.host.pane_path = path;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    make_pane(t, "Live");
    REQUIRE(type_region_value(t, "Text", "good").accepted);
    REQUIRE(save_pane(t).accepted);
    const std::string good = definition_bytes(t);
    // THE FILE IS REPLACED BY A FORGERY BEHIND WORKSHOP'S BACK, then the open door runs.
    spillout(path, forge_first(slurp(path), "\"kind\":\"text\"", "\"kind\":\"button\""));
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("`button`") != std::string::npos);
    CHECK(definition_bytes(t) == good);
    CHECK(t.session().panes.weaver.definition.name == "Live");
    CHECK(t.session().panes.weaver.definition.regions[0].text == "good");
    CHECK_FALSE(t.session().panes.weaver.dirty());
    // THE WALL STANDS, and it is load-bearing: this run's pane may not overwrite those bytes.
    bool walled = false;
    for (const Condition& c : t.conditions()) {
        walled = walled || c.key == kPaneWallKey;
    }
    CHECK(walled);
    const std::string forged_bytes = slurp(path);
    REQUIRE(type_region_value(t, "Text", "changed").accepted);
    const WeaverPaneAnswered walled_save = save_pane(t);
    CHECK_FALSE(walled_save.accepted);
    CHECK(t.session().notice_is_bad);
    CHECK(walled_save.said.find("will not be written over") != std::string::npos);
    CHECK(slurp(path) == forged_bytes);
    // A MALFORMED FILE WITH NO LIVE DEFINITION LEAVES NONE, and says why, at startup.
    Live fresh;
    fresh.host.pane_path = path;
    fresh.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK_FALSE(fresh.session().panes.weaver.open());
    CHECK(fresh.session().notice_is_bad);
    bool fresh_wall = false;
    for (const Condition& c : fresh.conditions()) {
        fresh_wall = fresh_wall || c.key == kPaneWallKey;
    }
    CHECK(fresh_wall);
}

// ============================================================================
// Relaunch by durable reference; an absent definition keeps the row
// ============================================================================

TEST_CASE("save, quit, relaunch -- the same pane returns on the same layout by its reference; "
          "remove the file and the row is kept unresolved") {
    // ⚔ MUTATION: a session that carries the interior, or a restore that drops the row
    // when the definition is absent. The session bytes are read; the row is checked.
    TempDir dir("wux14-relaunch");
    const std::string pane = dir.file("workshop-pane.json");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.pane_path = pane;
        t.host.session_path = session;
        t.publish(loom::to_value(surface::SurfaceReady{}));
        sdl_face(t);
        make_pane(t, "MyPane");
        REQUIRE(type_region_value(t, "Text", "hello from data").accepted);
        REQUIRE(type_region_value(t, "X", "126").accepted); // a pixel that is not a cell
        REQUIRE(type_region_value(t, "Y", "6").accepted);
        REQUIRE(save_pane(t).accepted);
        t.press(90, 35);
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    const std::string file_after_save = slurp(pane);
    CHECK(file_after_save.find("hello from data") != std::string::npos);
    CHECK(file_after_save.find("\"504\"") != std::string::npos);
    // THE SESSION NAMES THE PANE AND HOLDS NOT ONE BYTE OF ITS INSIDE.
    const std::string session_bytes = slurp(session);
    CHECK(session_bytes.find("\"zengine.workshop.maker\"") != std::string::npos);
    CHECK(session_bytes.find("\"MyPane\"") != std::string::npos);
    CHECK(session_bytes.find("hello from data") == std::string::npos);
    CHECK(session_bytes.find("\"regions\"") == std::string::npos);
    {
        Live back;
        back.host.pane_path = pane;
        back.host.session_path = session;
        back.publish(loom::to_value(surface::SurfaceReady{}));
        sdl_face(back);
        const Session& s = back.session();
        REQUIRE(s.panes.weaver.open());
        CHECK(s.panes.weaver.definition.name == "MyPane");
        CHECK(s.panes.weaver.definition.regions[0].text == "hello from data");
        CHECK(s.panes.weaver.definition.regions[0].x == cells_px(10) + 6);
        CHECK(s.panes.weaver.definition.regions[0].y == 6);
        CHECK_FALSE(s.panes.weaver.dirty());
        CHECK(has_pane(s.setup.active, kMine));
        CHECK(s.panes.has(kWeaverPaneKind));
        const RegionPresentation p = presentation_of(back);
        CHECK(p.present);
        CHECK(p.fit.graphical());
        CHECK(p.fit.view.x == (interior_of(back).x) + 126);
        // THE SAME FILE ON A TERMINAL: the same pane, the same identity, cells and `~`.
        tui_face(back);
        CHECK(back.session().panes.has(kWeaverPaneKind));
        CHECK(weaver_pane_text(back).find("hello from data") != std::string::npos);
        // THE SUBJECT DID NOT COME BACK, AND IS NOT OWED: an inspector's subject is its own and
        // no session holds it. Named again through the inspector's door, the same rows read the
        // same file.
        CHECK_FALSE(back.session().inspected.addressed());
        REQUIRE(hand_inspect(back, kMine).accepted);
        CHECK(region_value(back, "X") == "~10 cells (~ projected)");
        CHECK(region_value(back, "Y") == "~0 cells (~ projected)");
        CHECK(region_value(back, "Text") == "hello from data");
        CHECK(slurp(pane) == file_after_save);
        back.press(90, 35);
        back.key(input::scan::kQ);
        REQUIRE(back.host.quit);
        CHECK(slurp(pane) == file_after_save);
    }
    // THE DEFINITION GOES MISSING: the layout keeps its row and says what it cannot show.
    std::filesystem::remove(pane);
    Live gone;
    gone.host.pane_path = pane;
    gone.host.session_path = session;
    gone.publish(loom::to_value(surface::SurfaceReady{}));
    tui_face(gone);
    CHECK_FALSE(gone.session().panes.weaver.open());
    REQUIRE(has_pane(gone.session().setup.active, kMine));
    CHECK_FALSE(gone.session().panes.has(kWeaverPaneKind));
    // TWO ROWS WAIT: this case's pane, and the desk's Info weave, which no office in this
    // rig offers. The band says so with the count it actually has.
    const std::vector<PaneRef> waiting =
        unresolved_panes(gone.session().setup.active, gone.session().panes);
    REQUIRE(waiting.size() == 2);
    CHECK(waiting[0] == info_ref());
    CHECK(waiting[1] == kMine);
    CHECK(setup_rest_text(gone.session().setup, gone.session().panes, gone.session().keymap)
              .find("2 unresolved") != std::string::npos);
    // ...AND LEAVING WRITES THE ROW BACK UNCHANGED.
    gone.press(90, 35);
    gone.key(input::scan::kQ);
    REQUIRE(gone.host.quit);
    CHECK(slurp(session).find("\"MyPane\"") != std::string::npos);
}

// ============================================================================
// Loading presents and may not act
// ============================================================================

TEST_CASE("loading a definition mounts nothing, offers nothing and sends nothing through "
          "the provider seam") {
    // ⚔ MUTATION: route the weaver's pane through the external protocol. A stranger
    // holding the weaver namespace as an office is listening; it must hear nothing.
    TempDir dir("wux14-authority");
    const std::string pane = dir.file("pane.json");
    {
        PaneDefinition d = new_definition("MyPane");
        REQUIRE(set_region_text(d, 1, "quiet").accepted);
        REQUIRE(pdp::save_file(pane, d).accepted);
    }
    Live t;
    t.host.pane_path = pane;
    WeaverEars* ears = mount_weaver_ears(t);
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    REQUIRE(t.session().panes.weaver.open());
    open_pane(t, kMine);
    REQUIRE(t.session().panes.has(kWeaverPaneKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect cells = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, kWeaverPaneKind, sc).rect);
    t.press_canvas(cells.x + 2, cells.y + 2);
    t.key(input::scan::kX);
    t.text("x");
    t.wheel_canvas(1.0, cells.x + 2, cells.y + 2);
    for (int i = 0; i < 3; ++i) {
        t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132 + i), cells_px(46), 0, 0}));
    }
    CHECK(ears->heard() == 0);
    CHECK(t.session().panes.runtime.entries.size() == 2); // the two stand-ins `Live` admits
    CHECK(t.session().panes.external.empty());
    CHECK(t.session().panes.keyboard == kNoPaneKind);
}

// ============================================================================
// The naming prompt, waiting for room, and what did not move
// ============================================================================

TEST_CASE("the weaver door refuses a bad name in words and makes nothing") {
    // THE NAME'S LAW IS THE HOST'S, AND SO ARE ITS WORDS. The prompt that keeps a refused name
    // for correcting is the desktop Pane Manager's own line, and its suite drives it; what the
    // host owes that line is here: a refusal in words, and nothing made.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    const std::size_t rows_before = t.session().setup.active.panes.size();
    const WeaverPaneAnswered spaced = hand_weaver(t, weaver_pane_act::kCreate, "My Pane");
    CHECK_FALSE(spaced.accepted);
    CHECK(spaced.said.find("no spaces") != std::string::npos);
    CHECK(t.session().notice_is_bad);
    CHECK_FALSE(t.session().panes.weaver.open());
    const WeaverPaneAnswered empty = hand_weaver(t, weaver_pane_act::kCreate, "");
    CHECK_FALSE(empty.accepted);
    CHECK(empty.said.find("cannot be empty") != std::string::npos);
    CHECK_FALSE(t.session().panes.weaver.open());
    // NOTHING WAS MADE: no row on the desk, and no definition.
    CHECK(t.session().setup.active.panes.size() == rows_before);
    // ...AND AN ACT THE DOOR DOES NOT HAVE IS REFUSED IN WORDS TOO.
    const WeaverPaneAnswered odd = hand_weaver(t, 9);
    CHECK_FALSE(odd.accepted);
    CHECK(odd.said.find("make, save and discard are the three") != std::string::npos);
}

TEST_CASE("at the minimum composition a new pane lands waiting, is still the subject, and "
          "is still editable") {
    Live t; // 78x22: one overlay slot, and the stand-in is standing in it
    open_pane(t, ref_of(stock::kKind));
    const WeaverPaneAnswered made = make_pane(t, "MyPane");
    const Session& s = t.session();
    CHECK(made.said.find("waiting for room") != std::string::npos);
    REQUIRE(has_pane(s.setup.active, kMine));
    CHECK_FALSE(s.panes.has(kWeaverPaneKind));
    CHECK(s.panes.waiting(kWeaverPaneKind));
    const Screen sc = screen_of(s);
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panes)) {
        if (row.ref == kMine) {
            CHECK(pane_state_of(s.panes, s.setup.active, sc, row) == pane_state::kWaiting);
        }
    }
    CHECK(s.inspected.ref == kMine);
    REQUIRE(type_region_value(t, "Text", "typed while waiting").accepted);
    CHECK(t.session().panes.weaver.definition.regions[0].text == "typed while waiting");
    CHECK(region_value(t, "Resolved") == "- (the pane is not presented, or the region lies outside it)");
    CHECK(region_value(t, "Shown") == "no room -- nothing of it is drawn on this face");
    // A TALLER WINDOW SEATS IT, with the text already in it.
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    CHECK(t.session().panes.has(kWeaverPaneKind));
    CHECK(weaver_pane_text(t).find("typed while waiting") != std::string::npos);
}

TEST_CASE("a run with no weaver pane is the run it always was") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{cells_px(132), cells_px(46), 0, 0}));
    const Session& s = t.session();
    CHECK_FALSE(s.panes.weaver.open());
    CHECK_FALSE(s.panes.weaver.dirty());
    CHECK(combined_catalog(s.panes).size() == kBuiltinPaneCount + 2); // ...plus the two stand-ins
    // THE INVENTORY IS ONE LONGER THAN THE CATALOG, and the extra row is not a weaver pane:
    // it is the desk's Info weave, authored by `default_setup` and offered by nobody here.
    CHECK(inventory_rows(s.setup.active, s.panes).size() == kBuiltinPaneCount + 3);
    CHECK(has_pane(s.setup.active, info_ref()));
    CHECK_FALSE(resolve_pane(kMine, s.panes).has_value());
    CHECK(kind_name(s.panes, kWeaverPaneKind).empty());
    // THE QUIT IS UNGUARDED BY A PANE NOBODY MADE.
    t.key(input::scan::kQ);
    CHECK(t.host.quit);
}
