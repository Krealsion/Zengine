// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE PANE CREATOR (WUX-14): the first pane that exists because a maker described one.
//
// What this source holds, under the `workshop_panels` entry: the value a maker-made pane's
// interior IS and its law; the ninth durable artifact and what it cannot say; the identity a
// definition earns from its NAME; the region on both faces through the ordinary pane path;
// the Pane Manager's INTERIOR rows and the region mark; the one-open-definition lifecycle
// (dirty refusals at the quit, at the open, at the naming door); relaunch by durable
// reference; and the honest capture of a code-backed pane's interior.
//
// A SECOND SOURCE UNDER THE SAME ENTRY, for QR-13's reason: a suite is not a file, and the
// Pane Manager's own cases already fill one large object. The helpers below are this file's
// own copies of that file's (internal linkage, deliberately) so the two objects share
// nothing but the support header.

#include "workshop_support.hpp"

#include "workshop/pane_definition.hpp"
#include "workshop/pane_definition_persist.hpp"

#include <zen/schema.hpp>

#include <algorithm>
#include <fstream>
#include <set>

namespace {

namespace pdp = pane_definition_persist;

// ---- The Pane Manager, driven by keys -- this file's own copies of the panels suite's ----

ui::Rect editor_cells(const Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds at =
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc);
    REQUIRE(at.open);
    return pane_body_cells(at.rect, sc);
}

void press_into_editor(Live& t) {
    const ui::Rect b = editor_cells(t);
    t.press_canvas(b.x, b.y);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
}

void open_editor(Live& t) {
    open_pane(t, ref_of(panel::kPaneEditor));
    REQUIRE(t.session().panels.has(panel::kPaneEditor));
    press_into_editor(t);
}

std::size_t inventory_index(const Live& t, const PaneRef& ref) {
    const std::vector<CatalogRow> rows =
        inventory_rows(t.session().setup.active, t.session().panels);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].ref == ref) {
            return i;
        }
    }
    FAIL("not in the inventory: ", ref_text(ref));
    return rows.size();
}

void choose_by_keys(Live& t, const PaneRef& ref) {
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    if (t.session().pane_editor.on_rows) {
        t.key(input::scan::kTab);
    }
    REQUIRE_FALSE(t.session().pane_editor.on_rows);
    const std::size_t want = inventory_index(t, ref);
    for (int guard = 0; guard < 64; ++guard) {
        const std::size_t at = t.session().pane_editor.cursor;
        if (at == want) {
            break;
        }
        t.key(at < want ? input::scan::kDown : input::scan::kUp);
    }
    REQUIRE(t.session().pane_editor.cursor == want);
    t.key(input::scan::kReturn);
    REQUIRE(t.session().pane_editor.subject == ref);
}

/// The index of the INTERIOR section row, or the row count when there is none.
std::size_t interior_section(const Live& t) {
    const std::vector<Row>& rows = t.session().pane_editor.rows;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].section() && rows[i].label() == "INTERIOR") {
            return i;
        }
    }
    return rows.size();
}

/// The first row with this label AFTER the INTERIOR section -- the region's own row, told
/// apart from the pane's AUTHORED row of the same name by the section that owns it.
std::size_t region_row_index(const Live& t, const std::string& label) {
    const std::vector<Row>& rows = t.session().pane_editor.rows;
    for (std::size_t i = interior_section(t) + 1; i < rows.size(); ++i) {
        if (rows[i].label() == label) {
            return i;
        }
    }
    return rows.size();
}

const Row* region_row(const Live& t, const std::string& label) {
    const std::size_t at = region_row_index(t, label);
    return at < t.session().pane_editor.rows.size() ? &t.session().pane_editor.rows[at]
                                                     : nullptr;
}

std::string region_value(const Live& t, const std::string& label) {
    const Row* row = region_row(t, label);
    REQUIRE_MESSAGE(row != nullptr, "no INTERIOR row labelled ", label);
    return row->value();
}

/// Put the row cursor on a row BY INDEX, by keys only: Tab into the rows, then step.
void go_to_index(Live& t, std::size_t want) {
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    if (!t.session().pane_editor.on_rows) {
        t.key(input::scan::kTab);
    }
    REQUIRE(t.session().pane_editor.on_rows);
    for (int guard = 0; guard < 96; ++guard) {
        const std::size_t at = t.session().pane_editor.row_cursor;
        if (at == want) {
            return;
        }
        t.key(at < want ? input::scan::kDown : input::scan::kUp);
    }
    FAIL("could not reach row ", want);
}

/// TYPE A VALUE INTO A REGION ROW AND COMMIT IT -- the ordinary draft vocabulary, on the
/// row the INTERIOR section owns.
void type_region_value(Live& t, const std::string& label, const std::string& text) {
    const std::size_t at = region_row_index(t, label);
    REQUIRE(at < t.session().pane_editor.rows.size());
    go_to_index(t, at);
    t.key(input::scan::kReturn);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kDraft);
    REQUIRE(t.session().pane_editor.rows[at].editing());
    for (std::size_t i = 0; i < 300; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : text) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
}

/// The pane's AUTHORED row of this label (the FIRST such row -- before INTERIOR).
std::string pane_value(const Live& t, const std::string& label) {
    for (const Row& r : t.session().pane_editor.rows) {
        if (r.label() == label) {
            return r.value();
        }
    }
    FAIL("no Pane Manager row labelled ", label);
    return {};
}

// ---- The Pane Creator, driven by keys ------------------------------------------------------

const PaneRef kMine = maker_pane_ref("MyPane");

/// PRESS `n` AS THE PLATFORM DELIVERS IT: the key transition and the character it produced,
/// which the binding's swallow eats -- so a name typed afterwards starts with its own first
/// letter and not with the trigger's.
void press_new_pane(Live& t) {
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    t.key(input::scan::kN);
    t.text("n");
}

/// Type characters as the platform delivers them, one `TextEntered` each.
void type_chars(Live& t, const std::string& name) {
    for (const char c : name) {
        t.text(std::string(1, c));
    }
}

/// MAKE A PANE BY KEYS: the manager open with the keys in it, `n`, the name, Return.
void make_pane(Live& t, const std::string& name) {
    if (keyboard_context(t.session()) != KeyContext::kPaneEditor) {
        open_editor(t);
    }
    press_new_pane(t);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneNaming);
    type_chars(t,name);
    t.key(input::scan::kReturn);
    REQUIRE_MESSAGE(t.session().panels.maker.open(), t.notice());
    REQUIRE(t.session().panels.maker.definition.name == name);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
}

/// The maker-made pane's interior on this screen, through the ordinary pane path.
FineRect interior_of(const Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds where =
        bounds_of(t.session().panels, t.session().setup.active, kMakerPaneKind, sc);
    REQUIRE(where.open);
    REQUIRE_FALSE(where.rect.empty());
    return pane_inside(where.rect, sc).rect;
}

RegionPresentation presentation_of(const Live& t) {
    const MakerPane& m = t.session().panels.maker;
    REQUIRE(m.open());
    REQUIRE_FALSE(m.definition.regions.empty());
    return present_region(m.definition.regions.front(), interior_of(t), screen_of(t.session()));
}

/// The maker-made pane's painted text, off the last frame -- the cell projection.
std::string maker_pane_text(Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds where =
        bounds_of(t.session().panels, t.session().setup.active, kMakerPaneKind, sc);
    REQUIRE(where.open);
    const ui::Rect b = cells_covered(where.rect);
    // EVERY CELL LABEL INSIDE THE PANE, top to bottom then left to right -- a region placed
    // inside the interior does not begin at the pane's own column, so the support header's
    // left-column reader would miss it.
    std::vector<surface::SurfaceLabel> inside;
    for (const surface::SurfaceLabel& l : cell_text_of(t.canvases.back())) {
        if (l.x >= b.x && l.x < b.x + b.w && l.y >= b.y && l.y < b.y + b.h) {
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
    return pdp::to_text(t.session().panels.maker.definition);
}

/// A graphical face: the shipped skin's metric and its device unit.
void sdl_face(Live& t, std::int64_t w = 160, std::int64_t h = 60) {
    t.publish(loom::to_value(surface::SurfaceExtent{w, h, 8, 18, surface::kCanvasCellPx}));
}

/// A character face at the same extent.
void tui_face(Live& t, std::int64_t w = 160, std::int64_t h = 60) {
    t.publish(loom::to_value(surface::SurfaceExtent{w, h, 0, 0, 0}));
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

/// A DELIBERATE STRANGER HOLDING THE MAKER NAMESPACE AS AN OFFICE, listening for every
/// shape the pane seam sends a provider. It exists so "a maker-made pane is never routed
/// through the provider protocol" is a measurement: if Workshop ever addressed the maker
/// namespace as an office, this is where the sentence would land.
class MakerEars : public loom::WeaveBase<MakerEars, SeenState,
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

MakerEars* mount_maker_ears(Live& t) {
    auto ears = std::make_unique<MakerEars>();
    MakerEars* raw = ears.get();
    loom::Grant grant;
    const loom::WeaveId id =
        t.bus.register_weave(std::move(ears), std::move(grant), std::string(kMakerPaneProvider));
    raw->zen_set_self(id);
    return raw;
}

} // namespace

// ============================================================================
// SC-2 -- the maker-facing name
// ============================================================================

TEST_CASE("WUX-14/SC-2: the WUX-13 surface is the Pane Manager, and its durable key did not move") {
    const PanelKind& k = panel_kind(panel::kPaneEditor);
    CHECK(std::string(k.name) == "Pane Manager");
    // THE KEY IS A PROMISE TO EVERY FILE THAT ALREADY NAMES IT; the word is what moved.
    CHECK(std::string(k.pane) == "pane-editor");
    CHECK(std::string(pane_key::kPaneEditor) == "pane-editor");
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    CHECK(keyboard_context_name(t.session(), KeyContext::kPaneEditor) == "the Pane Manager");
    CHECK(panel_text(t.canvases.back(), editor_cells(t)).find("PANE MANAGER *") !=
          std::string::npos);
    // THE CREATOR'S KEYS ARE THE MANAGER CONTEXT'S OWN ROWS, listed where a maker looks.
    bool new_row = false;
    bool save_row = false;
    bool discard_row = false;
    for (const HotkeyRow& row : hotkeys_rows(t.session())) {
        new_row = new_row || row.text.find("new pane") != std::string::npos;
        save_row = save_row || row.text.find("save pane") != std::string::npos;
        discard_row = discard_row || row.text.find("discard pane edits") != std::string::npos;
    }
    CHECK(new_row);
    CHECK(save_row);
    CHECK(discard_row);
}

// ============================================================================
// SC-6 / SC-7 -- the value and its law
// ============================================================================

TEST_CASE("WUX-14/SC-6: a definition is a name and a list of text regions with stable ids") {
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
    REQUIRE(add_text_region(d, subs(1), subs(3), subs(5), subs(1)).accepted);
    REQUIRE(d.regions.size() == 2);
    CHECK(d.regions[1].id == 2);
    CHECK(d.next_id == 3);
    // ...AND AN ID IS NEVER REUSED: erase the first, mint again, and the mint continues.
    d.regions.erase(d.regions.begin());
    REQUIRE(add_text_region(d, 0, 0, subs(2), subs(2)).accepted);
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
    CHECK(region_of(d, 2)->w == subs(5));
    CHECK(author_region_axis(d, 2, 3, 20).accepted); // finer than a cell is honest intent
    CHECK(region_of(d, 2)->h == 20);
    CHECK_FALSE(author_region_axis(d, 2, 3, kRegionSubMax + 1).accepted);
}

TEST_CASE("WUX-14/SC-7: the whole-definition law refuses what no door could have made") {
    const auto refused = [](PaneDefinition d) {
        const Written w = check_definition(d);
        return !w.accepted ? w.refusal : std::string();
    };
    CHECK(refused(new_definition("")) .find("cannot be empty") != std::string::npos);
    CHECK(refused(new_definition("My Pane")).find("no spaces") != std::string::npos);
    CHECK(refused(new_definition("a/b")).find("`/`") != std::string::npos);
    CHECK(refused(new_definition(std::string(kMaxMakerPaneNameLen + 1, 'p')))
              .find("at most") != std::string::npos);
    CHECK(refused(new_definition(std::string("tab\there"))).find("no spaces") !=
          std::string::npos);
    CHECK(check_definition(new_definition(std::string(kMaxMakerPaneNameLen, 'p'))).accepted);
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
    huge.regions[0].w = kRegionSubMax + 1;
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
// SC-13 -- the ninth durable artifact, and what it cannot say
// ============================================================================

TEST_CASE("WUX-14/SC-13: the pane file round-trips, refuses by number and by shape, and holds "
          "nothing but the definition") {
    PaneDefinition d = new_definition("MyPane");
    REQUIRE(set_region_text(d, 1, "hello from data").accepted);
    // A FINE VALUE NO TERMINAL CAN SAY EXACTLY -- 12 cells and 24 sub-units, the pixel a
    // window authors -- written exactly, quantized by nobody.
    REQUIRE(author_region_axis(d, 1, 0, subs(12) + 24).accepted);
    const std::string text = pdp::to_text(d);
    CHECK(text.find("\"zengine-workshop-pane\"") != std::string::npos);
    CHECK(text.find("\"text\"") != std::string::npos);
    CHECK(text.find("\"600\"") != std::string::npos); // 12 * 48 + 24
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
    maximal.name = std::string(kMaxMakerPaneNameLen, 'p');
    for (std::size_t i = 0; i < kMaxRegions; ++i) {
        REQUIRE(add_text_region(maximal, kRegionSubMax, kRegionSubMax, kRegionSubMax,
                                kRegionSubMax)
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

TEST_CASE("WUX-14/SC-18: the definition and its file are structurally unable to act") {
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
// SC-1 / SC-3 / SC-9 -- a pane from data, on the desk, through the ordinary path
// ============================================================================

TEST_CASE("WUX-14/SC-1+SC-3+SC-9: `n` in the Pane Manager makes a named pane from data, and "
          "it lives on the desk exactly as every other pane does") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "MyPane");
    const Session& s = t.session();
    // THE VALUE: one open definition, one empty text region, minted #1.
    REQUIRE(s.panels.maker.definition.regions.size() == 1);
    CHECK(s.panels.maker.definition.regions[0].id == kFirstRegionId);
    CHECK(s.panels.maker.definition.regions[0].text.empty());
    CHECK(s.panels.maker.dirty()); // never saved: dirty by arithmetic
    CHECK(t.notice().find("Pane Creator: MyPane is on this layout") == 0);
    // THE IDENTITY: minted from the name under Workshop's own namespace, and it resolves.
    CHECK(kMine == PaneRef{kMakerPaneProvider, "MyPane"});
    REQUIRE(resolve_pane(kMine, s.panels).has_value());
    CHECK(*resolve_pane(kMine, s.panels) == kMakerPaneKind);
    CHECK(kind_name(s.panels, kMakerPaneKind) == "MyPane");
    // THE ORDINARY PANE PATH: a setup row, a seat, a rectangle, an occupancy, a state.
    REQUIRE(has_pane(s.setup.active, kMine));
    CHECK(s.panels.has(kMakerPaneKind));
    const Screen sc = screen_of(s);
    const PanelBounds where = bounds_of(s.panels, s.setup.active, kMakerPaneKind, sc);
    REQUIRE(where.open);
    REQUIRE_FALSE(where.rect.empty());
    CHECK(placement_of(kMakerPaneKind) == placement::kOverlayStack);
    const ui::Rect cells = cells_covered(where.rect);
    const Occupancy here = occupied_at(s.panels, s.setup.active, sc, cells.x + 2, cells.y + 2);
    CHECK(here.occupied);
    CHECK(here.kind == kMakerPaneKind);
    CHECK(here.what == "MyPane");
    bool listed = false;
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == kMine) {
            listed = true;
            CHECK(row.kind == kMakerPaneKind);
            CHECK(row.name == "MyPane");
            CHECK(row.summary == kMakerPaneSummary);
            CHECK(pane_state_of(s.panels, s.setup.active, sc, row) == pane_state::kOpen);
        }
    }
    CHECK(listed);
    // THE CREATOR'S SUBJECT: the manager describes the new pane, keys on its text row.
    CHECK(s.pane_editor.subject == kMine);
    CHECK(s.pane_editor.on_rows);
    REQUIRE(s.pane_editor.row_cursor < s.pane_editor.rows.size());
    CHECK(s.pane_editor.rows[s.pane_editor.row_cursor].label() == "Text");
    CHECK(pane_value(t, "Name") == "MyPane");
    CHECK(pane_value(t, "Identity") == "zengine.workshop.maker/MyPane");
    CHECK(pane_value(t, "Provider") == "zengine.workshop.maker (made here -- Pane Creator)");
    CHECK(pane_value(t, "Summary") == kMakerPaneSummary);
    CHECK(pane_value(t, "State") == "open");
    // THE INTERIOR ROWS, in order, under their own section.
    const std::size_t at = interior_section(t);
    REQUIRE(at < s.pane_editor.rows.size());
    const char* const expected[] = {"Region", "Text", "X", "Y", "Width", "Height", "Resolved",
                                    "Shown"};
    REQUIRE(s.pane_editor.rows.size() == at + 1 + sizeof(expected) / sizeof(expected[0]));
    for (std::size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        INFO(i);
        CHECK(s.pane_editor.rows[at + 1 + i].label() == expected[i]);
    }
    CHECK(region_value(t, "Region") == "#1 text -- the Pane Creator's subject");
    CHECK(region_value(t, "X") == "0 cells");
    CHECK(region_value(t, "Width") == "24 cells");
    CHECK(region_value(t, "Height") == "2 cells");
    // THE PANE IS PAINTED, AND THE PICKER'S POPULATION GREW BY ONE ROW AND NOTHING ELSE.
    CHECK(combined_catalog(s.panels).size() == kPanelKinds + 1);
    CHECK(s.panels.runtime.entries.empty());
    CHECK(s.panels.external.empty());
    // A PRESS INTO IT SELECTS IT AND POINTS NO KEYS (it takes none), like Info or Layouts.
    t.press_canvas(cells.x + 2, cells.y + 2);
    CHECK(t.session().panels.selected == kMakerPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    CHECK(t.notice().find("MyPane is here") == 0);
}

TEST_CASE("WUX-14/SC-9: the maker's pane is edited, ordered and removed by the doors every "
          "pane has, and comes back through the session by its reference") {
    TempDir dir("wux14-path");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.host.pane_path = dir.file("pane.json");
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
        make_pane(t, "MyPane");
        // THE PANE'S OWN AUTHORED ROWS -- the manager's, unchanged -- move it.
        const Screen sc = screen_of(t.session());
        const FineRect was = bounds_of(t.session().panels, t.session().setup.active,
                                       kMakerPaneKind, sc)
                                 .rect;
        go_to_index(t, 5); // AUTHORED X, the pane's
        REQUIRE(t.session().pane_editor.rows[5].label() == "X");
        t.key(input::scan::kReturn);
        for (int i = 0; i < 8; ++i) {
            t.key(input::scan::kBackspace);
        }
        type_chars(t,"30");
        t.key(input::scan::kReturn);
        REQUIRE_FALSE(t.session().notice_is_bad);
        const FineRect now = bounds_of(t.session().panels, t.session().setup.active,
                                       kMakerPaneKind, sc)
                                 .rect;
        CHECK(now.x == subs(30));
        CHECK(now.x != was.x);
        CHECK(pane_of(t.session().setup.active, kMine)->place.x == subs(30));
        // ORDER: the arrangement's own door, on the maker's reference.
        t.key(input::scan::kB);
        CHECK(pane_of(t.session().setup.active, kMine)->front == 0);
        // PARTICIPATION: `o` removes it -- the picker's door -- and the definition stands.
        t.key(input::scan::kO);
        CHECK_FALSE(has_pane(t.session().setup.active, kMine));
        CHECK_FALSE(t.session().panels.has(kMakerPaneKind));
        CHECK(t.session().panels.maker.open());
        CHECK(t.session().pane_editor.subject == kMine);
        t.key(input::scan::kO);
        CHECK(has_pane(t.session().setup.active, kMine));
        CHECK(t.session().panels.has(kMakerPaneKind));
        // SAVE, THEN LEAVE STANDING ON THE DESK.
        t.key(input::scan::kS);
        REQUIRE_FALSE(t.session().notice_is_bad);
        CHECK_FALSE(t.session().panels.maker.dirty());
        t.press(90, 35);
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    Live back;
    back.host.session_path = session;
    back.host.pane_path = dir.file("pane.json");
    back.publish(loom::to_value(surface::SurfaceReady{}));
    back.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    CHECK(back.session().panels.maker.open());
    CHECK(has_pane(back.session().setup.active, kMine));
    CHECK(back.session().panels.has(kMakerPaneKind));
    // THE SESSION HOLDS WHERE; THE PANE FILE HOLDS WHAT.
    CHECK(slurp(session).find("\"zengine.workshop.maker\"") != std::string::npos);
    CHECK(slurp(session).find("\"regions\"") == std::string::npos);
}

// ============================================================================
// SC-4 -- identity is minted from the name, never from what happens to be open
// ============================================================================

TEST_CASE("WUX-14/SC-4: a maker pane's identity is its name under Workshop's namespace -- not "
          "a singleton that follows the open file") {
    // ⚔ MUTATION (F1): resolve every maker-namespace reference to whatever definition is
    // open. Then `MyPane`'s row would resolve while `Other` is the open pane; the checks
    // below say it does not.
    TempDir dir("wux14-identity");
    Live t;
    t.host.pane_path = dir.file("pane.json");
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "MyPane");
    t.key(input::scan::kS); // saved, so a second pane may be made
    REQUIRE_FALSE(t.session().notice_is_bad);
    make_pane(t, "Other");
    const Session& s = t.session();
    CHECK(resolve_pane(maker_pane_ref("Other"), s.panels) == kMakerPaneKind);
    CHECK_FALSE(resolve_pane(kMine, s.panels).has_value());
    // MyPane's row on the desk is RETAINED and reads unresolved -- intent, never erased.
    REQUIRE(has_pane(s.setup.active, kMine));
    REQUIRE(has_pane(s.setup.active, maker_pane_ref("Other")));
    const Screen sc = screen_of(s);
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == kMine) {
            CHECK(row.kind == kNoPaneKind);
            CHECK(pane_state_of(s.panels, s.setup.active, sc, row) == pane_state::kUnresolved);
        }
    }
    const std::vector<PaneRef> waiting = unresolved_panes(s.setup.active, s.panels);
    REQUIRE(waiting.size() == 1);
    CHECK(waiting[0] == kMine);
    // A REFERENCE WITH THE NAMESPACE AND ANY OTHER NAME RESOLVES TO NOTHING.
    CHECK_FALSE(resolve_pane(maker_pane_ref("other"), s.panels).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{kMakerPaneProvider, ""}, s.panels).has_value());
    // ...AND NO OFFICE MAY OFFER A PANE IN THE MAKER NAMESPACE.
    RuntimeCatalog cat;
    const Admission refused = admit_pane_offer(cat, kMakerPaneProvider, good_offer());
    CHECK_FALSE(refused.written.accepted);
    CHECK(refused.written.refusal.find("namespace for panes a maker made") != std::string::npos);
    CHECK(cat.entries.empty());
    // THE MANAGER SAYS SO IN ITS OWN ROWS, for the pane whose file is not open.
    choose_by_keys(t, kMine);
    CHECK(pane_value(t, "Provider").find("no open definition is named MyPane") !=
          std::string::npos);
    CHECK(region_value(t, "Interior") == "no open definition is named MyPane -- nothing to show");
}

// ============================================================================
// SC-8 -- the frame is the interior, the lattice is fine, and each face is honest
// ============================================================================

TEST_CASE("WUX-14/SC-8: a region is placed relative to the pane's INTERIOR and painted through "
          "the ordinary pane path in cells") {
    // ⚔ MUTATION (F3): resolve the region against the canvas origin instead of the
    // interior's. The stack slot's interior begins one cell in on a terminal and many rows
    // down for a second slot, so the region's resolved place would land elsewhere.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "MyPane");
    type_region_value(t, "Text", "hello from data");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "committed Text = hello from data");
    const FineRect interior = interior_of(t);
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
    CHECK(maker_pane_text(t).find("hello from data") != std::string::npos);
    // MOVE THE REGION INSIDE THE PANE, and it follows the interior's origin, not the canvas's.
    type_region_value(t, "X", "2");
    type_region_value(t, "Y", "1");
    p = presentation_of(t);
    CHECK(p.shown.x == interior.x + subs(2));
    CHECK(p.shown.y == interior.y + subs(1));
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
    CHECK(t.session().panels.maker.definition.regions[0].w == subs(400));
    CHECK(region_value(t, "Resolved").find("(clipped by the pane)") != std::string::npos);
    CHECK(region_value(t, "Width") == "400 cells");
    // MOVING THE PANE MOVES THE REGION WITH IT -- the definition is untouched.
    const std::string before = definition_bytes(t);
    go_to_index(t, 6); // the pane's AUTHORED Y
    REQUIRE(t.session().pane_editor.rows[6].label() == "Y");
    t.key(input::scan::kReturn);
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    type_chars(t,"30");
    t.key(input::scan::kReturn);
    REQUIRE_FALSE(t.session().notice_is_bad);
    const FineRect moved = interior_of(t);
    CHECK(moved.y == subs(30) + kChromeSubs);
    CHECK(presentation_of(t).shown.y == moved.y + subs(1));
    CHECK(definition_bytes(t) == before);
}

TEST_CASE("WUX-14/SC-8: one authored fine value, read in pixels on the window and projected "
          "to cells on a terminal, and looking writes nothing back") {
    // ⚔ MUTATION (F4 / F9): a readout, a repaint or a face change that rewrites the
    // authored number to the projected one. The bytes are compared before and after.
    Live t;
    sdl_face(t);
    make_pane(t, "MyPane");
    CHECK(region_value(t, "X") == "0 px");
    CHECK(region_value(t, "Width") == "288 px");
    CHECK(region_value(t, "Height") == "24 px");
    // A PIXEL THAT IS NOT A CELL: 126 px is 10 cells and 24 sub-units.
    type_region_value(t, "X", "126");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "committed X = 126 px");
    CHECK(t.session().panels.maker.definition.regions[0].x == subs(10) + 24);
    CHECK(region_value(t, "X") == "126 px");
    CHECK(region_value(t, "Resolved") == "@126,0 288x24 px");
    CHECK(region_value(t, "Shown") == "1 row x 35 columns, presented in type");
    RegionPresentation p = presentation_of(t);
    CHECK(p.fit.graphical());
    CHECK(p.fit.rows == 1);
    CHECK(p.fit.view.x == surface::px_of_subs(interior_of(t).x) + 126);
    const std::string authored = definition_bytes(t);
    // THE OTHER FACE'S WORD IS REFUSED, NOT CONVERTED.
    type_region_value(t, "Y", "2 cells");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "Y: this face reads px, not cells");
    t.key(input::scan::kEscape);
    CHECK(definition_bytes(t) == authored);
    // THE SAME DESK ON A TERMINAL: the same value, honestly projected, marked.
    tui_face(t);
    CHECK(region_value(t, "X") == "~10 cells (~ projected)");
    CHECK(region_value(t, "Width") == "24 cells");
    CHECK(region_value(t, "Resolved") == "@~10,0 24x2 cells (~ projected)");
    CHECK(region_value(t, "Shown") == "2 rows x 24 columns, presented as cells");
    p = presentation_of(t);
    CHECK_FALSE(p.fit.graphical());
    // THE PUBLISHED REGION CARRIES THE REMAINDER ON THE WIRE, for the cell projection to
    // floor at its own grain and the window to spend as pixels.
    bool carried = false;
    for (const surface::SurfaceTextRegion& r : all_texts(t.canvases.back())) {
        if (r.x == cells_covered(interior_of(t)).x + 10 && r.sub_x == 24) {
            carried = true;
        }
    }
    CHECK(carried);
    CHECK(definition_bytes(t) == authored);
    // AND A TERMINAL'S OWN TYPING AUTHORS CELLS, exactly.
    type_region_value(t, "X", "11 cells");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(t.session().panels.maker.definition.regions[0].x == subs(11));
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

TEST_CASE("WUX-14/SC-8: a region too small for the face is the face's own answer, and the "
          "authored value is not rewritten to fit") {
    Live t;
    sdl_face(t);
    make_pane(t, "MyPane");
    type_region_value(t, "Text", "small");
    // ONE CELL TALL holds no row of an 18-pixel line: the cell projection, honestly named.
    type_region_value(t, "Height", "12");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(region_value(t, "Shown") == "1 row x 24 columns, presented as cells");
    CHECK(region_value(t, "Height") == "12 px");
    CHECK(t.session().panels.maker.definition.regions[0].h == subs(1));
    // THREE PIXELS TALL covers no cell and no row: nothing is drawn, and it says so.
    type_region_value(t, "Height", "3");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(region_value(t, "Shown") == "no room -- nothing of it is drawn on this face");
    CHECK(t.session().panels.maker.definition.regions[0].h == 12);
    CHECK(region_value(t, "Height") == "3 px");
    // ...AND A TERMINAL SAYS THE SAME THING IN ITS OWN GRAIN, marking what it cannot say.
    tui_face(t);
    CHECK(region_value(t, "Height") == "~0 cells (~ projected)");
    CHECK(region_value(t, "Shown") == "no room -- nothing of it is drawn on this face");
    CHECK(t.session().panels.maker.definition.regions[0].h == 12);
}

// ============================================================================
// SC-10 / SC-11 -- the region mark, and the rows as the one door
// ============================================================================

TEST_CASE("WUX-14/SC-10: the Pane Creator marks the region it is editing on the pane itself, "
          "from the same resolution, and writes nothing") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
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
    choose_by_keys(t, ref_of(panel::kLayouts));
    CHECK(creator_subject_region(t.session()) == nullptr);
    bool still = false;
    for (const surface::SurfaceRect& r : all_rects(t.canvases.back())) {
        still = still || (r.x == mark.x && r.y == mark.y && r.role == kRegionMark && r.w == mark.w);
    }
    CHECK_FALSE(still);
    // ...AND BACK, IT RETURNS -- and removing the manager removes it with nothing to clear.
    choose_by_keys(t, kMine);
    CHECK(creator_subject_region(t.session()) != nullptr);
    choose_by_keys(t, ref_of(panel::kPaneEditor));
    t.key(input::scan::kO);
    CHECK_FALSE(t.session().panels.has(panel::kPaneEditor));
    CHECK(creator_subject_region(t.session()) == nullptr);
    CHECK(definition_bytes(t) == before);
}

TEST_CASE("WUX-14/SC-11: Text and the four numbers are edited through the definition's doors, "
          "refused in words, and clamped never") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "MyPane");
    const std::string before = definition_bytes(t);
    type_region_value(t, "X", "-");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "X: a region has no default to reset to -- type a whole number of cells");
    t.key(input::scan::kEscape);
    type_region_value(t, "Width", "0");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "Width: a region width must be positive");
    t.key(input::scan::kEscape);
    type_region_value(t, "Y", "abc");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("Y: not a whole number of cells") == 0);
    t.key(input::scan::kEscape);
    type_region_value(t, "Height", "-4");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "Height: a region height must be positive");
    t.key(input::scan::kEscape);
    type_region_value(t, "X", "5000");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "X: a region place is at most 4096 cells");
    t.key(input::scan::kEscape);
    CHECK(definition_bytes(t) == before);
    // TEXT: plain ASCII, judged at the door; a byte the media cannot draw is refused whole.
    type_region_value(t, "Text", "plain words");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(t.session().panels.maker.definition.regions[0].text == "plain words");
    const std::size_t at = region_row_index(t, "Text");
    go_to_index(t, at);
    t.key(input::scan::kReturn);
    t.text("\xC3\xA9");
    t.key(input::scan::kReturn);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "Text: a text region holds plain ASCII with no control characters");
    CHECK(t.session().panels.maker.definition.regions[0].text == "plain words");
    t.key(input::scan::kEscape);
    // THE AUTHORED ROWS OF THE PANE AND THE REGION SHARE LABELS AND NOT DOORS: the pane's X
    // is the setup's, the region's X is the definition's.
    type_region_value(t, "X", "3");
    CHECK(t.session().panels.maker.definition.regions[0].x == subs(3));
    CHECK(pane_of(t.session().setup.active, kMine)->place == PanePlace{});
    CHECK(pane_value(t, "X") == "-");
    CHECK(region_value(t, "X") == "3 cells");
}

// ============================================================================
// SC-12 -- a code-backed pane is a capture, never a decomposition
// ============================================================================

TEST_CASE("WUX-14/SC-12: a code-backed subject's interior is a read-only capture, and an "
          "unresolved one is nothing to inspect") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    choose_by_keys(t, ref_of(panel::kLayouts));
    const std::size_t at = interior_section(t);
    REQUIRE(at + 2 == t.session().pane_editor.rows.size());
    CHECK(t.session().pane_editor.rows[at + 1].label() == "Interior");
    CHECK_FALSE(t.session().pane_editor.rows[at + 1].editable());
    const std::string capture = region_value(t, "Interior");
    CHECK(capture.find("code-backed -- body @") == 0);
    CHECK(capture.find("as cells; no authored interior") != std::string::npos);
    CHECK(region_row(t, "Text") == nullptr);
    // THE CAPTURE IS THE RESOLVED BODY, from the same place the painter resolves.
    const Screen sc = screen_of(t.session());
    const PanelProsePlace place = panel_prose_place(
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc).rect, sc);
    CHECK(capture.find(fine_rect_text(place.inside, 0)) != std::string::npos);
    CHECK(capture.find(std::to_string(place.rows) + " rows x ") != std::string::npos);
    // A CLOSED PANE: not presented, and said so.
    choose_by_keys(t, ref_of(panel::kBuilder));
    CHECK(region_value(t, "Interior") == "code-backed -- not presented; no authored interior");
    // AN UNRESOLVED STRANGER: nothing to inspect, and no pretence.
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    choose_by_keys(t, stranger());
    CHECK(region_value(t, "Interior") == "unresolved -- nothing to inspect");
}

// ============================================================================
// SC-14 / SC-15 -- the one-open-definition lifecycle
// ============================================================================

TEST_CASE("WUX-14/SC-14: dirty pane truth refuses the quit, a second new pane and a replacing "
          "open until the maker saves or discards") {
    // ⚔ MUTATION (F6): a quit, a naming or an open that proceeds over a dirty definition.
    TempDir dir("wux14-dirty");
    const std::string path = dir.file("pane.json");
    {
        PaneDefinition other = new_definition("FromDisk");
        REQUIRE(pdp::save_file(path, other).accepted);
    }
    Live t;
    t.host.pane_path = path;
    t.host.session_path = dir.file("session.json");
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "MyPane");
    REQUIRE(t.session().panels.maker.dirty());
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
    // A SECOND NEW PANE.
    press_into_editor(t);
    press_new_pane(t);
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    CHECK(t.notice().find("no new pane was started") != std::string::npos);
    // A REPLACING OPEN -- the startup load, arriving after a maker has already made one.
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.session().panels.maker.definition.name == "MyPane");
    CHECK(t.notice().find("nothing was opened") != std::string::npos);
    CHECK(t.session().panels.maker.dirty());
    // SAVE IS THE WAY OUT: the file is written, the pane is clean, the quit proceeds.
    t.key(input::scan::kS);
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "saved pane MyPane to " + spelled(path));
    CHECK_FALSE(t.session().panels.maker.dirty());
    CHECK(pdp::load_file(path).definition.name == "MyPane");
    t.press(90, 35);
    t.key(input::scan::kQ);
    CHECK(t.host.quit);
    CHECK(std::filesystem::exists(dir.file("session.json")));
}

TEST_CASE("WUX-14/SC-14: the discard door puts a saved pane back to its file, and closes a pane "
          "that was never saved while keeping its row") {
    TempDir dir("wux14-discard");
    Live t;
    t.host.pane_path = dir.file("pane.json");
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "MyPane");
    // NEVER SAVED: the discard closes it whole; the row is intent and stays.
    t.key(input::scan::kD, input::mod::kCtrl);
    CHECK_FALSE(t.session().panels.maker.open());
    CHECK(t.notice().find("never saved") != std::string::npos);
    CHECK(has_pane(t.session().setup.active, kMine));
    CHECK_FALSE(t.session().panels.has(kMakerPaneKind));
    CHECK_FALSE(resolve_pane(kMine, t.session().panels).has_value());
    CHECK(t.session().pane_editor.subject == kMine); // the subject stands, honestly unresolved
    CHECK(region_value(t, "Interior") == "no open definition is named MyPane -- nothing to show");
    // SAVED, THEN EDITED: the discard is the file's value again.
    make_pane(t, "Again");
    type_region_value(t, "Text", "kept");
    t.key(input::scan::kS);
    REQUIRE_FALSE(t.session().notice_is_bad);
    type_region_value(t, "Text", "lost");
    CHECK(t.session().panels.maker.dirty());
    t.key(input::scan::kD, input::mod::kCtrl);
    CHECK_FALSE(t.session().panels.maker.dirty());
    CHECK(t.session().panels.maker.definition.regions[0].text == "kept");
    CHECK(t.notice().find("back to what") != std::string::npos);
    t.key(input::scan::kD, input::mod::kCtrl);
    CHECK(t.notice().find("nothing to discard") != std::string::npos);
    // SAVE WITH NO FILE: a pane made in a run with no pane path is refused in words.
    Live nowhere;
    nowhere.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(nowhere, "Floating");
    CHECK(nowhere.notice().find("(no pane file this run)") != std::string::npos);
    nowhere.key(input::scan::kS);
    CHECK(nowhere.session().notice_is_bad);
    CHECK(nowhere.notice() == "no pane file -- start Workshop with --pane <path>");
    CHECK(nowhere.session().panels.maker.dirty());
}

TEST_CASE("WUX-14/SC-15: a malformed file cannot replace a live definition, and a refused file "
          "is never written over") {
    // ⚔ MUTATION (F5): an open that installs fields of a candidate before the whole has
    // been judged, or a save that writes over bytes this run could not read.
    TempDir dir("wux14-malformed");
    const std::string path = dir.file("pane.json");
    Live t;
    t.host.pane_path = path;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    make_pane(t, "Live");
    type_region_value(t, "Text", "good");
    t.key(input::scan::kS);
    REQUIRE_FALSE(t.session().notice_is_bad);
    const std::string good = definition_bytes(t);
    // THE FILE IS REPLACED BY A FORGERY BEHIND WORKSHOP'S BACK, then the open door runs.
    spillout(path, forge_first(slurp(path), "\"kind\":\"text\"", "\"kind\":\"button\""));
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("`button`") != std::string::npos);
    CHECK(definition_bytes(t) == good);
    CHECK(t.session().panels.maker.definition.name == "Live");
    CHECK(t.session().panels.maker.definition.regions[0].text == "good");
    CHECK_FALSE(t.session().panels.maker.dirty());
    // THE WALL STANDS, and it is load-bearing: this run's pane may not overwrite those bytes.
    bool walled = false;
    for (const Condition& c : t.conditions()) {
        walled = walled || c.key == kPaneWallKey;
    }
    CHECK(walled);
    const std::string forged_bytes = slurp(path);
    press_into_editor(t);
    type_region_value(t, "Text", "changed");
    t.key(input::scan::kS);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("will not be written over") != std::string::npos);
    CHECK(slurp(path) == forged_bytes);
    // A MALFORMED FILE WITH NO LIVE DEFINITION LEAVES NONE, and says why, at startup.
    Live fresh;
    fresh.host.pane_path = path;
    fresh.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK_FALSE(fresh.session().panels.maker.open());
    CHECK(fresh.session().notice_is_bad);
    bool fresh_wall = false;
    for (const Condition& c : fresh.conditions()) {
        fresh_wall = fresh_wall || c.key == kPaneWallKey;
    }
    CHECK(fresh_wall);
}

// ============================================================================
// SC-16 / SC-17 -- relaunch by durable reference; an absent definition keeps the row
// ============================================================================

TEST_CASE("WUX-14/SC-16+SC-17: save, quit, relaunch -- the same pane returns on the same layout "
          "by its reference; remove the file and the row is kept unresolved") {
    // ⚔ MUTATION (F7): a session that carries the interior, or a restore that drops the row
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
        type_region_value(t, "Text", "hello from data");
        type_region_value(t, "X", "126"); // a pixel that is not a cell
        type_region_value(t, "Y", "6");
        t.key(input::scan::kS);
        REQUIRE_FALSE(t.session().notice_is_bad);
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
        REQUIRE(s.panels.maker.open());
        CHECK(s.panels.maker.definition.name == "MyPane");
        CHECK(s.panels.maker.definition.regions[0].text == "hello from data");
        CHECK(s.panels.maker.definition.regions[0].x == subs(10) + 24);
        CHECK(s.panels.maker.definition.regions[0].y == 24);
        CHECK_FALSE(s.panels.maker.dirty());
        CHECK(has_pane(s.setup.active, kMine));
        CHECK(s.panels.has(kMakerPaneKind));
        const RegionPresentation p = presentation_of(back);
        CHECK(p.present);
        CHECK(p.fit.graphical());
        CHECK(p.fit.view.x == surface::px_of_subs(interior_of(back).x) + 126);
        // THE SAME FILE ON A TERMINAL: the same pane, the same identity, cells and `~`.
        tui_face(back);
        CHECK(back.session().panels.has(kMakerPaneKind));
        CHECK(maker_pane_text(back).find("hello from data") != std::string::npos);
        // THE MANAGER CAME BACK WITH THE DESK -- press into it rather than opening it twice.
        REQUIRE(back.session().panels.has(panel::kPaneEditor));
        press_into_editor(back);
        choose_by_keys(back, kMine);
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
    CHECK_FALSE(gone.session().panels.maker.open());
    REQUIRE(has_pane(gone.session().setup.active, kMine));
    CHECK_FALSE(gone.session().panels.has(kMakerPaneKind));
    const std::vector<PaneRef> waiting =
        unresolved_panes(gone.session().setup.active, gone.session().panels);
    REQUIRE(waiting.size() == 1);
    CHECK(waiting[0] == kMine);
    CHECK(setup_rest_text(gone.session().setup, gone.session().panels, gone.session().keymap)
              .find("1 unresolved") != std::string::npos);
    // ...AND LEAVING WRITES THE ROW BACK UNCHANGED.
    gone.press(90, 35);
    gone.key(input::scan::kQ);
    REQUIRE(gone.host.quit);
    CHECK(slurp(session).find("\"MyPane\"") != std::string::npos);
}

// ============================================================================
// SC-18 -- loading presents and may not act
// ============================================================================

TEST_CASE("WUX-14/SC-18: loading a definition mounts nothing, offers nothing and sends nothing "
          "through the provider seam") {
    // ⚔ MUTATION (F8): route the maker's pane through the external protocol. A stranger
    // holding the maker namespace as an office is listening; it must hear nothing.
    TempDir dir("wux14-authority");
    const std::string pane = dir.file("pane.json");
    {
        PaneDefinition d = new_definition("MyPane");
        REQUIRE(set_region_text(d, 1, "quiet").accepted);
        REQUIRE(pdp::save_file(pane, d).accepted);
    }
    Live t;
    t.host.pane_path = pane;
    MakerEars* ears = mount_maker_ears(t);
    t.publish(loom::to_value(surface::SurfaceReady{}));
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    REQUIRE(t.session().panels.maker.open());
    open_pane(t, kMine);
    REQUIRE(t.session().panels.has(kMakerPaneKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect cells = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, kMakerPaneKind, sc).rect);
    t.press_canvas(cells.x + 2, cells.y + 2);
    t.key(input::scan::kX);
    t.text("x");
    t.wheel_canvas(1.0, cells.x + 2, cells.y + 2);
    for (int i = 0; i < 3; ++i) {
        t.publish(loom::to_value(surface::SurfaceExtent{132 + i, 46, 0, 0}));
    }
    CHECK(ears->heard() == 0);
    CHECK(t.session().panels.runtime.entries.empty());
    CHECK(t.session().panels.external.empty());
    CHECK(t.session().panels.keyboard == kNoPaneKind);
}

// ============================================================================
// The naming prompt, waiting for room, and what did not move
// ============================================================================

TEST_CASE("WUX-14: the name prompt refuses a bad name in words and keeps it, cancels cleanly, and "
          "swallows its own trigger") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    press_new_pane(t);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneNaming);
    CHECK(t.session().pane_naming.line.text().empty()); // the trigger's own `n` was swallowed
    CHECK(keyboard_context_name(t.session(), KeyContext::kPaneNaming) == "naming a new pane");
    CHECK(panel_text(t.canvases.back(), editor_cells(t)).find(kPaneNamePrompt) !=
          std::string::npos);
    type_chars(t,"My Pane");
    t.key(input::scan::kReturn);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("no spaces") != std::string::npos);
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneNaming);
    CHECK(t.session().pane_naming.line.text() == "My Pane");
    CHECK_FALSE(t.session().panels.maker.open());
    for (int i = 0; i < 7; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.key(input::scan::kReturn);
    CHECK(t.notice().find("cannot be empty") != std::string::npos);
    t.key(input::scan::kEscape);
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    CHECK(t.notice() == "no pane was made");
    CHECK_FALSE(t.session().panels.maker.open());
    CHECK_FALSE(t.session().pane_naming.open);
    // THE PROMPT'S KEYS ARE LISTED IN ITS OWN CONTEXT.
    press_new_pane(t);
    bool make_row = false;
    bool box_group = false;
    for (const HotkeyRow& row : hotkeys_rows(t.session())) {
        make_row = make_row || row.text.find("make the pane") != std::string::npos;
        box_group = box_group || row.text.find("text box's own keys") != std::string::npos;
    }
    CHECK(make_row);
    CHECK(box_group);
    t.key(input::scan::kEscape);
}

TEST_CASE("WUX-14: at the minimum composition a new pane lands waiting, is still the subject, and "
          "is still editable") {
    Live t; // 78x22: one overlay slot, and the Pane Manager is standing in it
    make_pane(t, "MyPane");
    const Session& s = t.session();
    CHECK(t.notice().find("waiting for room") != std::string::npos);
    REQUIRE(has_pane(s.setup.active, kMine));
    CHECK_FALSE(s.panels.has(kMakerPaneKind));
    CHECK(s.panels.waiting(kMakerPaneKind));
    const Screen sc = screen_of(s);
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == kMine) {
            CHECK(pane_state_of(s.panels, s.setup.active, sc, row) == pane_state::kWaiting);
        }
    }
    CHECK(s.pane_editor.subject == kMine);
    type_region_value(t, "Text", "typed while waiting");
    REQUIRE_FALSE(t.session().notice_is_bad);
    CHECK(t.session().panels.maker.definition.regions[0].text == "typed while waiting");
    CHECK(region_value(t, "Resolved") == "- (the pane is not presented, or the region lies outside it)");
    CHECK(region_value(t, "Shown") == "no room -- nothing of it is drawn on this face");
    // A TALLER WINDOW SEATS IT, with the text already in it.
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    CHECK(t.session().panels.has(kMakerPaneKind));
    CHECK(maker_pane_text(t).find("typed while waiting") != std::string::npos);
}

TEST_CASE("WUX-14/SC-19: a run with no maker pane is the run it always was") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const Session& s = t.session();
    CHECK_FALSE(s.panels.maker.open());
    CHECK_FALSE(s.panels.maker.dirty());
    CHECK(combined_catalog(s.panels).size() == kPanelKinds);
    CHECK(inventory_rows(s.setup.active, s.panels).size() == kPanelKinds);
    CHECK_FALSE(resolve_pane(kMine, s.panels).has_value());
    CHECK(kind_name(s.panels, kMakerPaneKind).empty());
    // THE QUIT IS UNGUARDED BY A PANE NOBODY MADE.
    t.key(input::scan::kQ);
    CHECK(t.host.quit);
}
