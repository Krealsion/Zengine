// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_HPP
#define ZENGINE_WORKSHOP_SETUP_HPP

// What a maker calls the arrangement they are working in, and what that may mean.
// Workshop law: agents/workshop/layouts.md (+7 registers; agents/workshop.md routes)

#include "lattice.hpp" // `kMaxCells` -- the bound an authored cell count already has
#include "surface/vocabulary.hpp" // `kCellSubs` -- the fine lattice authored amounts live on
#include "pane_vocabulary.hpp"
#include "panel.hpp"
#include "property.hpp"

#include "component/text_box.hpp"
#include "ui/layout.hpp" // `ui::kMinCells` -- the one-cell floor an authored extent has

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The name a fresh Workshop's setup carries: a setup always has a name, so "unnamed" is no state.
inline constexpr const char* kDefaultSetupName = "Default";

/// The setup file's suggested name.
inline constexpr const char* kDefaultSetupFileName = "workshop-setup.json";

// ---- The bounds -------------------------------------------------------------

/// How long a setup's human name may be.
// WL-SETUP-09 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxSetupNameLen = 32;

/// How long either half of a `PaneRef` may be.
// WL-SETUP-01, WL-SETUP-10 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxPaneKeyLen = 64;

/// How many pane references one setup may carry.
// WL-MIG-03 -- agents/workshop/migration.md; WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxSetupPanes = 32;

/// How long a RUNTIME pane descriptor's two prose fields may be.
// WL-CAT-02 -- agents/workshop/catalog.md
inline constexpr std::size_t kMaxPaneNameLen = 32;
// WL-CAT-02 -- agents/workshop/catalog.md
inline constexpr std::size_t kMaxPaneSummaryLen = 64;

/// THE LARGEST DEVICE-PIXEL AMOUNT A PANE MAY BE AUTHORED AT.
// WL-SETUP-06 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kMaxPanePixels = 65536;

// ---- The value ---------------------------------------------------------------

/// WHICH PANE A MAKER MEANT -- durably, and without naming a catalog slot.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct PaneRef {
    std::string provider;
    std::string pane;

    friend bool operator==(const PaneRef&, const PaneRef&) = default;
};

// ---- THE AUTHORED WINDOW: the smallest difference from a default --------------

/// THE UNITS A PANE'S AUTHORED WINDOW MAY BE SAID IN, and there is not a fourth.
// WL-SETUP-03, WL-SETUP-04, WL-SETUP-06 -- agents/workshop/setup-file.md
namespace pane_unit {
inline constexpr std::int64_t kDefault = 0; ///< the developer's answer, whatever it becomes
/// AN ABSOLUTE COUNT OF SUB-CELL UNITS — 1/`surface::kCellSubs` of a canvas cell.
// WL-GEO-06 -- agents/workshop/geometry.md; WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kSubcells = 1;
/// DEVICE PIXELS, declared from the beginning and currently unprojectable.
// WL-SETUP-06 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kPixels = 2;
/// The right column, named: a place a desk asks for, carrying no coordinates. Why a place is a
/// unit: agents/decisions/the-room-is-the-screen.md.
// WL-SETUP-03 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kRightColumn = 3;
} // namespace pane_unit

/// THE AUTHORED LATTICE'S WALLS, in sub-units: the same cell bounds the setup
/// law has always enforced, expressed at the resolution the amounts now carry.
// WL-GEO-06 -- agents/workshop/geometry.md
inline constexpr std::int64_t kPaneSubMin = ui::kMinCells * surface::kCellSubs;
inline constexpr std::int64_t kPaneSubMax = kMaxCells * surface::kCellSubs;

/// WHERE A MAKER PUT A PANE -- one fact, both coordinates.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md; WL-SETUP-03 -- agents/workshop/setup-file.md
struct PanePlace {
    /// `kDefault`, `kSubcells` or `kRightColumn`; never `kPixels`
    std::int64_t mode = pane_unit::kDefault;
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend bool operator==(const PanePlace&, const PanePlace&) = default;
};

/// HOW BIG A MAKER MADE ONE AXIS OF A PANE.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct PaneSize {
    std::int64_t mode = pane_unit::kDefault; ///< `kDefault`, `kSubcells` or `kPixels`
    std::int64_t amount = 0;

    friend bool operator==(const PaneSize&, const PaneSize&) = default;
};

/// ONE ROW OF A SETUP: which pane, and the smallest thing a maker said about its
/// window.
// WL-SETUP-01, WL-SETUP-07 -- agents/workshop/setup-file.md
struct SetupPane {
    PaneRef ref;
    PanePlace place;
    PaneSize width;
    PaneSize height;
    std::int64_t front = 0;

    friend bool operator==(const SetupPane&, const SetupPane&) = default;
};

/// A setup: what a maker calls this arrangement, and which panes it has, in
/// order.
// WL-LAYOUT-01, WL-LAYOUT-06 -- agents/workshop/layouts.md
// WL-PANE-07 -- agents/workshop/panes-and-windows.md
struct Setup {
    std::string name;
    std::vector<SetupPane> panes;

    friend bool operator==(const Setup&, const Setup&) = default;
};

// ---- The reference a built-in kind carries, and the kind a reference names ---

/// THE DURABLE REFERENCE FOR AN INTERNAL KIND.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline PaneRef pane_ref_of(std::int64_t kind) {
    const PanelKind& row = panel_kind(kind);
    return PaneRef{row.provider, row.pane};
}

/// THE DURABLE REFERENCE A MAKER-MADE PANE EARNS FROM ITS NAME, and the whole of how that
/// identity is minted: Workshop's maker namespace, and the definition's own name.
// WL-MAKER-03 -- agents/workshop/maker-pane.md
inline PaneRef maker_pane_ref(const std::string& name) {
    return PaneRef{kMakerPaneProvider, name};
}

/// A maker-made pane's name meets the reference's own key law by construction: the name
/// bound is under the key bound, and the name law refuses every byte the key law refuses.
static_assert(kMaxMakerPaneNameLen <= kMaxPaneKeyLen,
              "a maker-made pane's name is the pane half of its durable reference, so its "
              "bound must sit under the reference's");

/// WHICH INTERNAL KIND THIS REFERENCE NAMES, OR NOTHING.
// WL-MAKER-04 -- agents/workshop/maker-pane.md
inline std::optional<std::int64_t> resolve_builtin_pane(const PaneRef& ref) {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (ref.provider == kPanelCatalog[i].provider && ref.pane == kPanelCatalog[i].pane) {
            return kPanelCatalog[i].kind;
        }
    }
    return std::nullopt;
}

/// WHICH KIND THIS REFERENCE NAMES ON THIS SCREEN, IN THIS RUN, OR NOTHING --
/// asked of the compile-time catalog and of what this session has been offered.
// WL-MAKER-03, WL-MAKER-04 -- agents/workshop/maker-pane.md
inline std::optional<std::int64_t> resolve_pane(const PaneRef& ref, const Panels& panels) {
    const std::optional<std::int64_t> built_in = resolve_builtin_pane(ref);
    if (built_in.has_value()) {
        return built_in;
    }
    if (ref.provider == kMakerPaneProvider) {
        if (panels.maker.open() && panels.maker.definition.name == ref.pane) {
            return kMakerPaneKind;
        }
        return std::nullopt; // the namespace is Workshop's: no office can answer for it
    }
    if (const RuntimePane* row = panels.runtime.find(ref.provider, ref.pane)) {
        return row->kind;
    }
    return std::nullopt;
}

/// Whether this build can currently present the pane this reference names.
inline bool resolvable(const PaneRef& ref, const Panels& panels) {
    return resolve_pane(ref, panels).has_value();
}

// ---- THE COMBINED CATALOG: what the Pane Manager lists, built-ins and offers ---

/// ONE ROW OF THE COMBINED POPULATION -- a compile-time kind or a runtime
/// one, said in one shape so the inventory, the doors, the selection and the
/// pointer all read one list.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
struct CatalogRow {
    std::int64_t kind = kNoPaneKind;
    PaneRef ref;
    std::string name;
    std::string summary;
};

/// The one line a list reads under a maker-made pane's name.
inline constexpr const char* kMakerPaneSummary = "a pane you made -- Pane Creator";

/// THE WHOLE POPULATION A MAKER MAY CHOOSE FROM, in the one order: every compile-time
/// built-in in the catalog's own order, then every admitted runtime pane in
/// first-accepted-offer order. Built as a value rather than walked twice, and cached nowhere.
// WL-CAT-05 -- agents/workshop/catalog.md
inline std::vector<CatalogRow> combined_catalog(const Panels& panels) {
    std::vector<CatalogRow> rows;
    rows.reserve(kPanelKinds + 1 + panels.runtime.entries.size());
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        rows.push_back(CatalogRow{kPanelCatalog[i].kind,
                                  PaneRef{kPanelCatalog[i].provider, kPanelCatalog[i].pane},
                                  kPanelCatalog[i].name, kPanelCatalog[i].summary});
    }
    // The maker's own pane sits between the built-ins and the strangers: Workshop-owned, and the
    // newest. Its name and identity are the definition's; nothing is copied here.
    if (panels.maker.open()) {
        rows.push_back(CatalogRow{kMakerPaneKind, maker_pane_ref(panels.maker.definition.name),
                                  panels.maker.definition.name, kMakerPaneSummary});
    }
    for (const RuntimePane& r : panels.runtime.entries) {
        rows.push_back(CatalogRow{r.kind, PaneRef{r.provider, r.pane}, r.name, r.summary});
    }
    return rows;
}

/// The name a maker reads for a kind, built-in, maker-made or runtime; empty for one none knows.
inline std::string kind_name(const Panels& panels, std::int64_t kind) {
    if (is_runtime_kind(kind)) {
        if (const RuntimePane* row = panels.runtime.of_kind(kind)) {
            return row->name;
        }
        return std::string();
    }
    if (is_maker_kind(kind)) {
        return panels.maker.open() ? panels.maker.definition.name : std::string();
    }
    return std::string(panel_kind(kind).name);
}

/// A reference as a person reads it: `provider/pane`.
inline std::string ref_text(const PaneRef& ref) { return ref.provider + "/" + ref.pane; }

/// A SETUP'S NAME AS ONE QUOTED TOKEN OF MAKER-FACING PROSE.
// WL-TAB-07 -- agents/workshop/tab-run.md
inline std::string quoted_setup_name(const std::string& name) {
    std::string quoted;
    quoted.reserve(name.size() + 2);
    quoted += '"';
    for (const char c : name) {
        if (c == '\\' || c == '"') {
            quoted += '\\';
        }
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

// ---- The law: what this application will accept as a setup -------------------

/// What this application accepts as a setup's human name; a typed name and a loaded one meet
/// this one function.
// WL-SETUP-09 -- agents/workshop/setup-file.md
inline Written check_setup_name(const std::string& name) {
    if (name.empty()) {
        return Written::no("a setup name cannot be empty");
    }
    if (name.size() > kMaxSetupNameLen) {
        return Written::no("a setup name is at most " + std::to_string(kMaxSetupNameLen) +
                           " bytes");
    }
    bool anything = false;
    for (const char c : name) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte == 0x7Fu) {
            return Written::no("a setup name cannot contain control characters");
        }
        if (byte != ' ') {
            anything = true;
        }
    }
    if (!anything) {
        return Written::no("a setup name needs more than spaces in it");
    }
    return Written::ok();
}

/// Either half of a `PaneRef`, judged by shape and never by meaning. A view, so the offer door
/// judges Loom's stamp before anything owns a copy of it.
// WL-SETUP-10 -- agents/workshop/setup-file.md
inline Written check_pane_key(std::string_view key, const char* which) {
    if (key.empty()) {
        return Written::no(std::string("a pane reference's ") + which + " cannot be empty");
    }
    if (key.size() > kMaxPaneKeyLen) {
        return Written::no(std::string("a pane reference's ") + which + " is at most " +
                           std::to_string(kMaxPaneKeyLen) + " bytes");
    }
    for (const char c : key) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return Written::no(std::string("a pane reference's ") + which +
                               " cannot contain spaces or control characters");
        }
    }
    return Written::ok();
}

// WL-SETUP-10 -- agents/workshop/setup-file.md
inline Written check_pane_ref(const PaneRef& ref) {
    const Written provider = check_pane_key(ref.provider, "provider");
    if (!provider.accepted) {
        return provider;
    }
    return check_pane_key(ref.pane, "pane key");
}

// ---- What this application accepts as authored window intent -----------------
// WL-SETUP-03 -- agents/workshop/setup-file.md

/// ONE COORDINATE of an authored place. NEGATIVE IS REFUSED AT THE ROOT.
// WL-ARR-06 -- agents/workshop/arrangement.md; WL-SETUP-03 -- agents/workshop/setup-file.md
inline Written check_pane_place_coord(std::int64_t v) {
    if (v < 0) {
        return Written::no("a pane place cannot be negative");
    }
    if (v > kPaneSubMax) {
        return Written::no("a pane place is at most " + std::to_string(kMaxCells) +
                           " cells");
    }
    return Written::ok();
}

/// A PLACE: default with nothing said, a named place, or an absolute position on the fine
/// lattice.
inline Written check_pane_place(const PanePlace& p) {
    if (p.mode == pane_unit::kDefault) {
        if (p.x != 0 || p.y != 0) {
            return Written::no("a default pane place carries no coordinates");
        }
        return Written::ok();
    }
    // A named place carries no coordinates either: they would be a second answer.
    if (p.mode == pane_unit::kRightColumn) {
        if (p.x != 0 || p.y != 0) {
            return Written::no("a named pane place carries no coordinates");
        }
        return Written::ok();
    }
    if (p.mode != pane_unit::kSubcells) {
        return Written::no("a pane place is default, right-column or subcells");
    }
    const Written x = check_pane_place_coord(p.x);
    if (!x.accepted) {
        return x;
    }
    return check_pane_place_coord(p.y);
}

/// ONE AXIS OF A SIZE: default, a count of sub-cell units, or a count of device
/// pixels.
// WL-SETUP-03, WL-SETUP-06 -- agents/workshop/setup-file.md
inline Written check_pane_size(const PaneSize& s, const char* which) {
    if (s.mode == pane_unit::kDefault) {
        if (s.amount != 0) {
            return Written::no(std::string("a default pane ") + which + " carries no amount");
        }
        return Written::ok();
    }
    if (s.mode == pane_unit::kSubcells) {
        if (s.amount < kPaneSubMin) {
            return Written::no(std::string("a pane ") + which + " is at least " +
                               std::to_string(ui::kMinCells) + " cell");
        }
        if (s.amount > kPaneSubMax) {
            return Written::no(std::string("a pane ") + which + " is at most " +
                               std::to_string(kMaxCells) + " cells");
        }
        return Written::ok();
    }
    if (s.mode == pane_unit::kPixels) {
        if (s.amount < 1) {
            return Written::no(std::string("a pane ") + which + " is at least 1 pixel");
        }
        if (s.amount > kMaxPanePixels) {
            return Written::no(std::string("a pane ") + which + " is at most " +
                               std::to_string(kMaxPanePixels) + " pixels");
        }
        return Written::ok();
    }
    return Written::no(std::string("a pane ") + which + " is default, subcells or pixels");
}

/// EVERY LAW ONE AUTHORED ROW MEETS, minus the two that are about the WHOLE setup
/// (no duplicate reference, and the rank permutation). One function so a typed
/// gesture and a loaded file cannot come to disagree about a row.
inline Written check_setup_pane(const SetupPane& row) {
    const Written legal = check_pane_ref(row.ref);
    if (!legal.accepted) {
        return legal;
    }
    const Written placed = check_pane_place(row.place);
    if (!placed.accepted) {
        return placed;
    }
    const Written wide = check_pane_size(row.width, "width");
    if (!wide.accepted) {
        return wide;
    }
    return check_pane_size(row.height, "height");
}

// ---- Admitting one live offer into the runtime catalog -----------------------
// A descriptor is judged whole and only then copied: an offer wrong in its fourth field leaves
// nothing of its first three behind.

/// A runtime descriptor's prose, its display name or its one-line summary: one owner for both.
// WL-CAT-02 -- agents/workshop/catalog.md
inline Written check_pane_text(const std::string& text, const char* which,
                               std::size_t limit) {
    if (text.empty()) {
        return Written::no(std::string("a pane's ") + which + " cannot be empty");
    }
    if (text.size() > limit) {
        return Written::no(std::string("a pane's ") + which + " is at most " +
                           std::to_string(limit) + " bytes");
    }
    bool anything = false;
    for (const char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte == 0x7Fu) {
            return Written::no(std::string("a pane's ") + which +
                               " cannot contain control characters");
        }
        if (byte != ' ') {
            anything = true;
        }
    }
    if (!anything) {
        return Written::no(std::string("a pane's ") + which + " needs more than spaces in it");
    }
    return Written::ok();
}

/// What admitting an offer did. `refreshed` is kept apart from acceptance: a first acceptance may
/// resolve an authored reference, and a refresh must clear what an open pane showed and ask for its
/// room again.
// WL-CAT-03 -- agents/workshop/catalog.md
struct Admission {
    Written written = Written::ok();
    bool refreshed = false;                  ///< an existing PaneRef, updated in place
    std::int64_t kind = kFirstRuntimeKind;   ///< valid only when `written.accepted`
};

/// Admit one `PaneOffered` under the office Loom stamped on it, `mail.authored_role()`: the shape
/// has no provider field, and `mail.sender()` is a WeaveId, which would make a reloaded provider a
/// different pane. A refresh at capacity is allowed: capacity bounds distinct panes.
// WL-CAT-03 -- agents/workshop/catalog.md
inline Admission admit_pane_offer(RuntimeCatalog& runtime, std::string_view stamped_office,
                                  const PaneOffered& offer, std::int64_t rows = 0,
                                  std::int64_t columns = 0) {
    Admission out;
    if (rows < 0 || columns < 0 || rows > 512 || columns > 512 ||
        ((rows == 0) != (columns == 0))) {
        out.written = Written::no("pane comfort must be 1..512 body rows and columns, or zero/zero");
        return out;
    }
    // The stamp is judged first, as a view, before anything owns a copy. An empty role is
    // personal speech, refused here as well as at the door; Loom bounds no role's length.
    const Written office = check_pane_key(stamped_office, "provider");
    if (!office.accepted) {
        out.written = office;
        return out;
    }
    const Written key = check_pane_key(offer.pane, "pane key");
    if (!key.accepted) {
        out.written = key;
        return out;
    }
    const Written named = check_pane_text(offer.name, "name", kMaxPaneNameLen);
    if (!named.accepted) {
        out.written = named;
        return out;
    }
    const Written said = check_pane_text(offer.summary, "summary", kMaxPaneSummaryLen);
    if (!said.accepted) {
        out.written = said;
        return out;
    }
    // The first owned copy of the office, made only once all four fields have passed.
    const PaneRef ref{std::string(stamped_office), offer.pane};
    if (resolve_builtin_pane(ref).has_value()) {
        out.written = Written::no("`" + ref_text(ref) + "` is a built-in pane");
        return out;
    }
    // The maker namespace is Workshop's own: an offer stamped with it would put a stranger's rows
    // behind a maker's name.
    if (ref.provider == kMakerPaneProvider) {
        out.written = Written::no("`" + ref.provider +
                                  "` is Workshop's namespace for panes a maker made -- no "
                                  "office may offer a pane in it");
        return out;
    }
    // EVERY FIELD HAS PASSED; ONLY NOW IS ANYTHING WRITTEN.
    for (RuntimePane& row : runtime.entries) {
        if (row.provider == ref.provider && row.pane == ref.pane) {
            row.name = offer.name;       // in place: the position and the handle are kept,
            row.summary = offer.summary; // so an open pane stays the pane it was
            out.refreshed = true;
            out.kind = row.kind;
            return out;
        }
    }
    if (kPanelKinds + runtime.entries.size() >= kMaxPaneCatalogEntries) {
        out.written = Written::no("Workshop holds at most " +
                                  std::to_string(kMaxPaneCatalogEntries) +
                                  " panes -- `" + ref_text(ref) + "` was not added");
        return out;
    }
    RuntimePane row;
    row.kind = runtime.next_kind++;
    row.provider = ref.provider;
    row.pane = ref.pane;
    row.name = offer.name;
    row.summary = offer.summary;
    row.preferred_rows = rows;
    row.preferred_columns = columns;
    out.kind = row.kind;
    runtime.entries.push_back(std::move(row));
    return out;
}

/// Which admitted pane a `PaneActions` is about, under the office Loom stamped on it: the shape's
/// half of admission. It takes the pane key, not the shape, since both published versions name a
/// pane alike. An office that never offered this key is refused by name. Nothing is written here:
/// the rows are `join_pane_rows`' (keymap.hpp), and the caller commits both or neither.
// WL-KEY-15 -- agents/workshop/keyboard.md
inline Admission admit_pane_actions(const RuntimeCatalog& runtime,
                                    std::string_view stamped_office,
                                    const std::string& pane) {
    Admission out;
    const Written office = check_pane_key(stamped_office, "provider");
    if (!office.accepted) {
        out.written = office;
        return out;
    }
    const Written key = check_pane_key(pane, "pane key");
    if (!key.accepted) {
        out.written = key;
        return out;
    }
    const RuntimePane* row = runtime.find(stamped_office, pane);
    if (row == nullptr) {
        out.written = Written::no("`" + ref_text(PaneRef{std::string(stamped_office), pane}) +
                                  "` is not a pane that office has offered -- its actions "
                                  "were not taken");
        return out;
    }
    out.refreshed = !row->actions.empty();
    out.kind = row->kind;
    return out;
}

/// THE WHOLE-SETUP LAW, asked once on a complete candidate.
/// It judges the name, every row, how many there are, whether any two name the
/// same pane, and whether the ranks are a permutation.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline Written check_setup(const Setup& s) {
    const Written named = check_setup_name(s.name);
    if (!named.accepted) {
        return named;
    }
    if (s.panes.size() > kMaxSetupPanes) {
        return Written::no("a setup holds at most " + std::to_string(kMaxSetupPanes) +
                           " panes -- this one names " + std::to_string(s.panes.size()));
    }
    const std::int64_t n = static_cast<std::int64_t>(s.panes.size());
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        const Written legal = check_setup_pane(s.panes[i]);
        if (!legal.accepted) {
            return legal;
        }
        if (s.panes[i].front < 0 || s.panes[i].front >= n) {
            return Written::no("a pane's front order is 0 to " + std::to_string(n - 1) +
                               " -- `" + ref_text(s.panes[i].ref) + "` says " +
                               std::to_string(s.panes[i].front));
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (s.panes[j].ref == s.panes[i].ref) {
                return Written::no("`" + ref_text(s.panes[i].ref) + "` is named twice");
            }
            if (s.panes[j].front == s.panes[i].front) {
                return Written::no("two panes claim front order " +
                                   std::to_string(s.panes[i].front) + " -- `" +
                                   ref_text(s.panes[j].ref) + "` and `" +
                                   ref_text(s.panes[i].ref) + "`");
            }
        }
    }
    return Written::ok();
}

// ---- Operations on the authored intent ---------------------------------------

/// The answer for a pane the setup does not name.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr std::size_t kNoPaneRow = static_cast<std::size_t>(-1);

/// WHICH ROW OF THE SETUP NAMES THIS PANE, or `kNoPaneRow`. The one lookup, so a selection,
/// a geometry edit and an ordering operation all find a pane the same way -- by the
/// `PaneRef` that IS its identity, never by a runtime kind.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline std::size_t pane_row(const Setup& s, const PaneRef& ref) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        if (s.panes[i].ref == ref) {
            return i;
        }
    }
    return kNoPaneRow;
}

inline bool has_pane(const Setup& s, const PaneRef& ref) {
    return pane_row(s, ref) != kNoPaneRow;
}

/// The authored row for this reference, or nothing. By handle, and nothing may
/// hold one across an edit that could grow `panes`.
inline const SetupPane* pane_of(const Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    return at == kNoPaneRow ? nullptr : &s.panes[at];
}

inline SetupPane* pane_of(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    return at == kNoPaneRow ? nullptr : &s.panes[at];
}

/// Add a reference to the end of the setup's order, or say it was already there.
// WL-PANE-07 -- agents/workshop/panes-and-windows.md; WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool add_pane(Setup& s, const PaneRef& ref) {
    if (has_pane(s, ref)) {
        return false;
    }
    SetupPane row;
    row.ref = ref;
    row.front = static_cast<std::int64_t>(s.panes.size()); // == n-1 after the push
    s.panes.push_back(std::move(row));
    return true;
}

/// Remove a reference, AND CLOSE THE RANKS OVER IT.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool remove_pane(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    const std::int64_t gone = s.panes[at].front;
    s.panes.erase(s.panes.begin() + static_cast<std::ptrdiff_t>(at));
    for (SetupPane& row : s.panes) {
        if (row.front > gone) {
            --row.front;
        }
    }
    return true;
}

// ---- THE CANONICAL FRONT ORDER: five operations, all exact permutations ------
// WL-SETUP-07 -- agents/workshop/setup-file.md

/// The pane that currently sits at this rank, or `kNoPaneRow`. Total, and used by
/// the two step operations to find the neighbour they swap with.
inline std::size_t pane_at_front(const Setup& s, std::int64_t rank) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        if (s.panes[i].front == rank) {
            return i;
        }
    }
    return kNoPaneRow;
}

/// THE AUTHORED ORDER BECOMES THE DEFAULT ORDER: `front[i] = i`, in list order.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline void reset_front(Setup& s) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        s.panes[i].front = static_cast<std::int64_t>(i);
    }
}

/// SEND TO FRONT, or say it is already there.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool send_to_front(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    if (s.panes[at].front == static_cast<std::int64_t>(s.panes.size()) - 1) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    for (SetupPane& row : s.panes) {
        if (row.front > was) {
            --row.front;
        }
    }
    s.panes[at].front = static_cast<std::int64_t>(s.panes.size()) - 1;
    return true;
}

/// Send to back, or say it is already there -- `send_to_front`'s mirror, no-op included.
inline bool send_to_back(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    if (s.panes[at].front == 0) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    for (SetupPane& row : s.panes) {
        if (row.front < was) {
            ++row.front;
        }
    }
    s.panes[at].front = 0;
    return true;
}

/// SWAP WITH THE PANE IMMEDIATELY IN FRONT, or say there is none.
// WL-SETUP-07 -- agents/workshop/setup-file.md
inline bool raise_one(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    const std::size_t ahead = pane_at_front(s, s.panes[at].front + 1);
    if (ahead == kNoPaneRow) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    s.panes[at].front = s.panes[ahead].front;
    s.panes[ahead].front = was;
    return true;
}

inline bool lower_one(Setup& s, const PaneRef& ref) {
    const std::size_t at = pane_row(s, ref);
    if (at == kNoPaneRow) {
        return false;
    }
    if (s.panes[at].front == 0) {
        return false;
    }
    const std::size_t behind = pane_at_front(s, s.panes[at].front - 1);
    if (behind == kNoPaneRow) {
        return false;
    }
    const std::int64_t was = s.panes[at].front;
    s.panes[at].front = s.panes[behind].front;
    s.panes[behind].front = was;
    return true;
}

// ---- THE GEOMETRY DOORS: what a hand and a key both end at ------------------------------
// WL-SETUP-08 -- agents/workshop/setup-file.md

/// AUTHOR AN ABSOLUTE PLACE. Writes the place and nothing else. `x`/`y` are
/// sub-units, the authored lattice's own resolution.
inline Written author_pane_place(Setup& s, const PaneRef& ref, std::int64_t x,
                                 std::int64_t y) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return Written::no("`" + ref_text(ref) + "` is not in this setup");
    }
    const PanePlace proposed{pane_unit::kSubcells, x, y};
    const Written legal = check_pane_place(proposed);
    if (!legal.accepted) {
        return legal;
    }
    row->place = proposed;
    return Written::ok();
}

/// AUTHOR BOTH SIZE AXES AT ONCE, each in its own unit.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md; WL-SETUP-08 -- agents/workshop/setup-file.md
inline Written author_pane_size(Setup& s, const PaneRef& ref, const PaneSize& width,
                                const PaneSize& height) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return Written::no("`" + ref_text(ref) + "` is not in this setup");
    }
    const Written wide = check_pane_size(width, "width");
    if (!wide.accepted) {
        return wide;
    }
    const Written tall = check_pane_size(height, "height");
    if (!tall.accepted) {
        return tall;
    }
    row->width = width;
    row->height = height;
    return Written::ok();
}

/// ONE AXIS of what a single gesture proposes for a pane's window.
// WL-ARR-06 -- agents/workshop/arrangement.md
struct PaneAxisProposal {
    std::optional<std::int64_t> position;
    std::optional<PaneSize> extent;
    std::int64_t base = 0;
};

/// What authoring a window proposal did: `written` answers for the gesture as a
/// whole, and `place_written` says whether the place moved — the caller owes a
/// reseat (`apply_setup`) exactly then, because an authored place leaves the
/// reactive stack.
struct WindowWritten {
    Written written;
    bool place_written = false;
};

/// AUTHOR WHAT ONE GESTURE PROPOSES FOR A PANE'S WINDOW — per axis.
// WL-ARR-05, WL-ARR-06, WL-ARR-10 -- agents/workshop/arrangement.md
// WL-PED-05 -- agents/workshop/pane-manager.md
inline WindowWritten author_pane_window(Setup& s, const PaneRef& ref,
                                        const PaneAxisProposal& horizontal,
                                        const PaneAxisProposal& vertical) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return WindowWritten{Written::no("`" + ref_text(ref) + "` is not in this setup"),
                             false};
    }
    Written wide = Written::ok();
    if (horizontal.position.has_value()) {
        wide = check_pane_place_coord(*horizontal.position);
    }
    if (wide.accepted && horizontal.extent.has_value()) {
        wide = check_pane_size(*horizontal.extent, "width");
    }
    Written tall = Written::ok();
    if (vertical.position.has_value()) {
        tall = check_pane_place_coord(*vertical.position);
    }
    if (tall.accepted && vertical.extent.has_value()) {
        tall = check_pane_size(*vertical.extent, "height");
    }
    const bool h_asks = horizontal.position.has_value() || horizontal.extent.has_value();
    const bool v_asks = vertical.position.has_value() || vertical.extent.has_value();
    const bool h_lands = h_asks && wide.accepted;
    const bool v_lands = v_asks && tall.accepted;
    if (!h_lands && !v_lands) {
        if (!h_asks && !v_asks) {
            return WindowWritten{Written::ok(), false};
        }
        return WindowWritten{!wide.accepted ? wide : tall, false};
    }
    const bool place_written = (h_lands && horizontal.position.has_value()) ||
                               (v_lands && vertical.position.has_value());
    if (place_written) {
        // A PLACE IS ONE FIELD. The axis that settled a position writes it; the
        // other contributes what it already stood at — its authored coordinate,
        // or the resolved base the caller measured — never a clamped wall.
        const bool authored = row->place.mode == pane_unit::kSubcells;
        const std::int64_t x = h_lands && horizontal.position.has_value()
                                   ? *horizontal.position
                                   : (authored ? row->place.x : horizontal.base);
        const std::int64_t y = v_lands && vertical.position.has_value()
                                   ? *vertical.position
                                   : (authored ? row->place.y : vertical.base);
        row->place = PanePlace{pane_unit::kSubcells, x, y};
    }
    if (h_lands && horizontal.extent.has_value()) {
        row->width = *horizontal.extent;
    }
    if (v_lands && vertical.extent.has_value()) {
        row->height = *vertical.extent;
    }
    return WindowWritten{Written::ok(), place_written};
}

/// The resets: each removes one authored difference and leaves the others.
inline bool reset_pane_place(Setup& s, const PaneRef& ref) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr || row->place.mode == pane_unit::kDefault) {
        return false;
    }
    row->place = PanePlace{};
    return true;
}

inline bool reset_pane_width(Setup& s, const PaneRef& ref) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr || row->width.mode == pane_unit::kDefault) {
        return false;
    }
    row->width = PaneSize{};
    return true;
}

inline bool reset_pane_height(Setup& s, const PaneRef& ref) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr || row->height.mode == pane_unit::kDefault) {
        return false;
    }
    row->height = PaneSize{};
    return true;
}

/// The references this build cannot currently present, IN THE ORDER THE SETUP
/// HOLDS THEM.
// WL-MAKER-04 -- agents/workshop/maker-pane.md
inline std::vector<PaneRef> unresolved_panes(const Setup& s, const Panels& panels) {
    std::vector<PaneRef> out;
    for (const SetupPane& row : s.panes) {
        if (!resolvable(row.ref, panels)) {
            out.push_back(row.ref);
        }
    }
    return out;
}

/// EVERY PANE A MAKER MAY CHOOSE FROM **OR** HAS ALREADY AUTHORED -- the one inventory,
/// said out loud to whatever presents it (the desktop's Pane Manager) and spent by both doors.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
inline std::vector<CatalogRow> inventory_rows(const Setup& setup, const Panels& panels) {
    std::vector<CatalogRow> rows = combined_catalog(panels);
    for (const SetupPane& row : setup.panes) {
        bool known = false;
        for (const CatalogRow& have : rows) {
            if (have.ref == row.ref) {
                known = true;
                break;
            }
        }
        if (known) {
            continue;
        }
        CatalogRow made;
        made.kind = kNoPaneKind; // this build cannot present it, so it names no kind
        made.ref = row.ref;
        made.name = row.ref.pane;
        made.summary = ref_text(row.ref);
        rows.push_back(std::move(made));
    }
    return rows;
}

/// A fresh Workshop's setup: the default panels, then Info in the right column.
// WL-LAYOUT-03 -- agents/workshop/layouts.md; WL-SETUP-07 -- agents/workshop/setup-file.md
inline Setup default_setup() {
    Setup s;
    s.name = kDefaultSetupName;
    s.panes.reserve(kDefaultPanelCount);
    for (const std::int64_t kind : kDefaultPanels) {
        (void)add_pane(s, pane_ref_of(kind));
    }
    // Info opens at the right edge because this desk row says so: the one place this host names a
    // weave's office, as a row a maker may move or delete like any other. An office that never
    // arrives leaves an unresolved row (`unresolved_panes`).
    const PaneRef info{kInfoPaneProvider, kInfoPaneKey};
    if (add_pane(s, info)) {
        for (SetupPane& row : s.panes) {
            if (row.ref == info) {
                row.place.mode = pane_unit::kRightColumn;
            }
        }
    }
    return s;
}

// ---- Authored intent, reconciled onto resolved presentations ------------------

/// WHAT RECONCILING A SETUP ONTO THE LIVE PANELS ACTUALLY DID.
// WL-PANE-07 -- agents/workshop/panes-and-windows.md
struct Reconciled {
    std::vector<std::int64_t> opened;
    std::vector<std::int64_t> closed;
    std::size_t unresolved = 0;
    /// THE KINDS THIS SCREEN HAD NO ROOM FOR, in setup order.
    // WL-PANE-03 -- agents/workshop/panes-and-windows.md
    std::vector<std::int64_t> waiting;
};

/// HOW MANY OVERLAY SLOTS FIT ABOVE THE BOTTOM BAND -- Workshop's current spatial
/// capacity, as one number.
// WL-PANE-03 -- agents/workshop/panes-and-windows.md
struct StackCapacity {
    std::size_t slots = 0;
    std::int64_t height = 0, width = 0;
    std::int64_t line = 0, column = 0, border = 0, fallback_height = 0, gap = 0;
};

struct PreferredExtent { std::int64_t width = 0, height = 0; };

inline PreferredExtent preferred_extent(const RuntimePane* pane, const StackCapacity& room) {
    if (!pane || !pane->preferred_rows || !room.height) return {};
    return {std::min(room.width, pane->preferred_columns * room.column + 2 * room.border),
            std::min(room.height, (pane->preferred_rows + 1) * room.line + 2 * room.border)};
}

/// WHICH AUTHORED REFERENCES THIS BUILD WOULD PRESENT AT THIS CAPACITY, and which
/// it would not -- resolution and seating, decided together and changing nothing.
// WL-PANE-03, WL-PANE-07 -- agents/workshop/panes-and-windows.md
struct Seating {
    std::vector<std::int64_t> wanted;  ///< resolved and seated, in setup order
    std::vector<std::int64_t> waiting; ///< resolved and out of room, in setup order
    std::size_t unresolved = 0;
};

inline Seating seat_panes(const Setup& setup, const Panels& panels, StackCapacity room) {
    Seating out;
    out.wanted.reserve(setup.panes.size());
    std::size_t stack_used = 0;
    std::int64_t used_height = 0;
    for (const SetupPane& row : setup.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(row.ref, panels);
        if (!kind.has_value()) {
            ++out.unresolved;
            continue;
        }
        // The slot this pane would take, counted as `bounds_of` counts it. A side-region pane takes
        // none and always fits, and so does a pane the maker placed: an authored place is not
        // rationed by the reactive stack, so it never waits for room it never spent.
        if (placement_of(*kind) == placement::kOverlayStack &&
            row.place.mode == pane_unit::kDefault) {
            const auto preferred = preferred_extent(panels.runtime.of_kind(*kind), room);
            const auto height = preferred.height ? preferred.height : room.fallback_height;
            if (room.height ? used_height + height > room.height : stack_used >= room.slots) {
                out.waiting.push_back(*kind);
                continue;
            }
            ++stack_used;
            used_height += height + room.gap;
        }
        out.wanted.push_back(*kind);
    }
    return out;
}

/// THE AUTHORED PANE ORDER, BACK TO FRONT -- the one place `front` is read.
// WL-FRONT-05, WL-FRONT-06 -- agents/workshop/planes.md
inline std::vector<std::int64_t> presentation_order(const Setup& setup, const Panels& panels) {
    struct Ranked {
        std::int64_t front;
        std::int64_t kind;
    };
    std::vector<Ranked> ranked;
    ranked.reserve(panels.open.size());
    std::vector<std::int64_t> unranked;
    for (const Panel& p : panels.open) {
        bool found = false;
        for (const SetupPane& row : setup.panes) {
            const std::optional<std::int64_t> kind = resolve_pane(row.ref, panels);
            if (kind.has_value() && *kind == p.kind) {
                ranked.push_back(Ranked{row.front, p.kind});
                found = true;
                break;
            }
        }
        if (!found) {
            unranked.push_back(p.kind);
        }
    }
    // A selection sort over at most `kMaxSetupPanes` rows; the ranks are distinct, so no tie-break.
    std::vector<std::int64_t> out;
    out.reserve(ranked.size() + unranked.size());
    while (!ranked.empty()) {
        std::size_t least = 0;
        for (std::size_t i = 1; i < ranked.size(); ++i) {
            if (ranked[i].front < ranked[least].front) {
                least = i;
            }
        }
        out.push_back(ranked[least].kind);
        ranked.erase(ranked.begin() + static_cast<std::ptrdiff_t>(least));
    }
    for (const std::int64_t kind : unranked) {
        out.push_back(kind);
    }
    return out;
}

/// WHAT PANE IS EFFECTIVELY IN FRONT RIGHT NOW -- the authored order with the selected
/// pane lifted to the end of it.
// WL-FRONT-01, WL-FRONT-05, WL-FRONT-06 -- agents/workshop/planes.md
inline std::vector<std::int64_t> effective_pane_order(const Setup& setup,
                                                      const Panels& panels) {
    std::vector<std::int64_t> order = presentation_order(setup, panels);
    const std::int64_t lifted = selected_pane(panels);
    if (lifted == kNoPaneKind) {
        return order;
    }
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] != lifted) {
            continue;
        }
        // Rotate, do not swap: a swap would exchange two panes' depths into an order nobody wrote.
        order.erase(order.begin() + static_cast<std::ptrdiff_t>(i));
        order.push_back(lifted);
        break;
    }
    return order;
}

/// MAKE THE OPEN PANELS BE WHAT THE SETUP SAYS -- the one path, and the only thing in this
/// application that opens or closes a panel on a setup's behalf. Three cases, deliberately
/// distinguished, and capacity spent in setup order; an unresolved reference is counted.
// WL-PANE-07 -- agents/workshop/panes-and-windows.md
inline Reconciled reconcile(Panels& panels, const Setup& setup, StackCapacity room) {
    Reconciled done;
    const Seating seating = seat_panes(setup, panels, room);
    const std::vector<std::int64_t>& wanted = seating.wanted;
    done.unresolved = seating.unresolved;
    done.waiting = seating.waiting;
    panels.waiting_for_room = done.waiting;

    const auto wants = [&wanted](std::int64_t kind) {
        for (const std::int64_t k : wanted) {
            if (k == kind) {
                return true;
            }
        }
        return false;
    };

    // Close first, through the close door, over a copy: `close_panel` erases from `panels.open` and
    // forgets the kind's view.
    const std::vector<Panel> before = panels.open;
    for (const Panel& p : before) {
        if (!wants(p.kind)) {
            if (close_panel(panels, p.kind)) {
                done.closed.push_back(p.kind);
            }
        }
    }

    // Then assign what remains in the setup's order: `open_panel` appends, so an already-open kind
    // would keep its old position.
    std::vector<Panel> now;
    now.reserve(wanted.size());
    for (const std::int64_t kind : wanted) {
        bool was_open = false;
        for (const Panel& p : before) {
            if (p.kind == kind) {
                was_open = true;
                break;
            }
        }
        if (!was_open) {
            done.opened.push_back(kind);
            // ...so a newly opened external pane gets its view here, as `open_panel` would give it.
            if (is_runtime_kind(kind) && panels.external_pane(kind) == nullptr) {
                ExternalPane fresh;
                fresh.kind = kind;
                panels.external.push_back(std::move(fresh));
            }
        }
        now.push_back(Panel{kind});
    }
    panels.open = std::move(now);
    return done;
}

// ---- The session's side of it -------------------------------------------------

/// THE ONE-LINE LAYOUT-NAME EDITOR: open or not, which layout it is naming, and
/// the line being typed.
// WL-LAYOUT-10 -- agents/workshop/layouts.md; WL-TEXT-01 -- agents/workshop/text-box.md
struct LayoutNaming {
    bool open = false;
    std::size_t at = 0;
    component::TextBox line;
};

/// A LAYOUT'S OPTIONAL RELATIONSHIP TO ONE STANDALONE SETUP ARTIFACT.
// WL-LAYOUT-01, WL-LAYOUT-02, WL-LAYOUT-11 -- agents/workshop/layouts.md
struct SetupLink {
    std::string path;
    Setup known;

    friend bool operator==(const SetupLink& a, const SetupLink& b) {
        return a.path == b.path && a.known == b.known;
    }
    friend bool operator!=(const SetupLink& a, const SetupLink& b) { return !(a == b); }
};

/// What the active layout's status says: derived at every composition, stored nowhere.
namespace setup_link {
inline constexpr std::int64_t kNone = 0;     ///< no artifact is associated
inline constexpr std::int64_t kCurrent = 1;  ///< the desk equals the last known value
inline constexpr std::int64_t kModified = 2; ///< it is associated and differs from it
} // namespace setup_link

/// WHICH OF THE THREE THIS LAYOUT IS. Pure, total, and the ONE place the question
/// is decided.
// WL-LAYOUT-02 -- agents/workshop/layouts.md
inline std::int64_t link_status(const Setup& desk, const SetupLink& link) noexcept {
    if (link.path.empty()) {
        return setup_link::kNone;
    }
    return desk == link.known ? setup_link::kCurrent : setup_link::kModified;
}

/// ONE LAYOUT AS THE SHELF AND THE RUN HOLD IT: the desk, and the artifact it is
/// associated with.
// WL-LAYOUT-01 -- agents/workshop/layouts.md
struct Layout {
    Setup desk;
    SetupLink link;

    friend bool operator==(const Layout& a, const Layout& b) {
        return a.desk == b.desk && a.link == b.link;
    }
    friend bool operator!=(const Layout& a, const Layout& b) { return !(a == b); }
};

/// THE LAYOUTS THIS WORKSHOP IS HOLDING, WHICH ONE IS LIVE, AND THE EDITOR OVER
/// ITS NAME.
// WL-LAYOUT-01, WL-LAYOUT-03 -- agents/workshop/layouts.md
struct SetupState {
    Setup active = default_setup();
    SetupLink active_link;
    LayoutNaming naming;
    std::vector<Layout> shelved;
    std::size_t active_at = 0;
    /// How many times another desk has been put live (a switch, a restored run, a new or removed
    /// live layout, a desk restored from its file); never persisted. How a reader that named "this
    /// pane on this desk" learns the desk is another one (`InspectedPane::desk`).
    // WL-INFO-14 -- agents/workshop/info-body.md
    std::uint64_t put_live = 0;
};

/// How many layouts this Workshop holds, the active one included: never zero, as `active` is a
/// value.
inline std::size_t layout_count(const SetupState& s) noexcept { return s.shelved.size() + 1; }

/// Where position `at` sits on the shelf, for a position that is not the live one: the one place a
/// run index becomes a shelf index.
inline std::size_t shelf_index(const SetupState& s, std::size_t at) noexcept {
    return at < s.active_at ? at : at - 1;
}

/// The desk at position `at` in the maker's order. Out of range answers the active layout: every
/// caller holds a position from this same run.
inline const Setup& layout_at(const SetupState& s, std::size_t at) noexcept {
    if (at == s.active_at || at >= layout_count(s)) {
        return s.active;
    }
    return s.shelved[shelf_index(s, at)].desk;
}

/// ...AND ITS SETUP ASSOCIATION, by the same rule and for the same reason.
inline const SetupLink& link_at(const SetupState& s, std::size_t at) noexcept {
    if (at == s.active_at || at >= layout_count(s)) {
        return s.active_link;
    }
    return s.shelved[shelf_index(s, at)].link;
}

/// THE MOST LAYOUTS ONE RUN KEEPS.
// WL-LAYOUT-08 -- agents/workshop/layouts.md
inline constexpr std::size_t kMaxLayouts = 8;

/// MAKE THE LAYOUT AT POSITION `to` THE LIVE ONE -- the whole of a switch's value
/// half, and the only thing in this application that changes which layout is
/// active.
// WL-LAYOUT-03, WL-LAYOUT-05 -- agents/workshop/layouts.md
inline bool activate_layout(SetupState& s, std::size_t to) {
    if (to >= layout_count(s) || to == s.active_at) {
        return false;
    }
    s.shelved.insert(s.shelved.begin() + static_cast<std::ptrdiff_t>(s.active_at),
                     Layout{std::move(s.active), std::move(s.active_link)});
    s.active = std::move(s.shelved[to].desk);
    s.active_link = std::move(s.shelved[to].link);
    s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(to));
    s.active_at = to;
    ++s.put_live;
    return true;
}

/// THE MAKER'S ORDERED RUN, WITH THE LIVE ONE PUT BACK WHERE IT SITS.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline std::vector<Layout> layout_run(const SetupState& s) {
    std::vector<Layout> run = s.shelved;
    // Total over `active_at`, as `layout_at` is.
    const std::size_t at = s.active_at <= s.shelved.size() ? s.active_at : s.shelved.size();
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(at), Layout{s.active, s.active_link});
    return run;
}

/// TAKE AN ORDERED RUN AND LIFT ONE OF IT LIVE -- the direction a
/// restored session travels, and the only other place the run's spelling is made.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline bool install_layout_run(SetupState& s, std::vector<Layout> run, std::size_t active) {
    if (run.empty() || active >= run.size()) {
        return false;
    }
    s.active = std::move(run[active].desk);
    s.active_link = std::move(run[active].link);
    run.erase(run.begin() + static_cast<std::ptrdiff_t>(active));
    s.shelved = std::move(run);
    s.active_at = active;
    ++s.put_live;
    return true;
}

/// ONE MORE LAYOUT: A FRESH BLANK DESK, APPENDED, AND LIVE.
// WL-LAYOUT-03 -- agents/workshop/layouts.md
inline bool add_layout(SetupState& s, std::size_t ceiling = kMaxLayouts) {
    if (layout_count(s) >= ceiling) {
        return false;
    }
    s.shelved.insert(s.shelved.begin() + static_cast<std::ptrdiff_t>(s.active_at),
                     Layout{std::move(s.active), std::move(s.active_link)});
    s.active = default_setup();
    s.active_link = SetupLink{};
    s.active_at = s.shelved.size();
    ++s.put_live;
    return true;
}

/// COPY THE LAYOUT AT `at`, PUT THE COPY DIRECTLY AFTER IT, AND STAND ON THE COPY.
// WL-LAYOUT-04 -- agents/workshop/layouts.md
inline bool duplicate_layout(SetupState& s, std::size_t at, std::size_t ceiling = kMaxLayouts) {
    if (at >= layout_count(s) || layout_count(s) >= ceiling) {
        return false;
    }
    std::vector<Layout> run = layout_run(s);
    const std::size_t copy_at = at + 1;
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(copy_at),
               Layout{run[at].desk, SetupLink{}});
    return install_layout_run(s, std::move(run), copy_at);
}

/// DISCARD THE LAYOUT AT `at`.
// WL-LAYOUT-03 -- agents/workshop/layouts.md
inline bool remove_layout(SetupState& s, std::size_t at) {
    if (s.shelved.empty() || at >= layout_count(s)) {
        return false;
    }
    if (at != s.active_at) {
        s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(shelf_index(s, at)));
        if (at < s.active_at) {
            --s.active_at;
        }
        return true;
    }
    const std::size_t take =
        s.active_at < s.shelved.size() ? s.active_at : s.shelved.size() - 1;
    s.active = std::move(s.shelved[take].desk);
    s.active_link = std::move(s.shelved[take].link);
    s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(take));
    s.active_at = take;
    ++s.put_live;
    return true;
}

/// MOVE THE LAYOUT AT `from` TO POSITION `to` IN THE MAKER'S ORDER.
// WL-LAYOUT-04 -- agents/workshop/layouts.md; WL-TAB-11 -- agents/workshop/tab-run.md
inline bool move_layout(SetupState& s, std::size_t from, std::size_t to) {
    const std::size_t n = layout_count(s);
    if (from >= n || to >= n || from == to) {
        return false;
    }
    std::vector<Layout> run = layout_run(s);
    Layout moved = std::move(run[from]);
    run.erase(run.begin() + static_cast<std::ptrdiff_t>(from));
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    // Where the live layout landed, computed from the erase and the insert: two layouts may be
    // equal, so a search could find the wrong one.
    std::size_t live = s.active_at;
    if (live == from) {
        live = to;
    } else {
        if (live > from) {
            --live;
        }
        if (live >= to) {
            ++live;
        }
    }
    return install_layout_run(s, std::move(run), live);
}

/// GIVE THE LAYOUT AT `at` A NEW NAME -- the whole of a rename's value
/// half, and the only thing in this application that writes a layout's name
/// without writing a file.
// WL-LAYOUT-04, WL-LAYOUT-10 -- agents/workshop/layouts.md
inline bool rename_layout(SetupState& s, std::size_t at, std::string name) {
    if (at >= layout_count(s)) {
        return false;
    }
    if (at == s.active_at) {
        s.active.name = std::move(name);
        return true;
    }
    s.shelved[shelf_index(s, at)].desk.name = std::move(name);
    return true;
}

/// TEACH EVERY LAYOUT ASSOCIATED WITH `path` WHAT THAT ARTIFACT NOW HOLDS.
// WL-LAYOUT-11 -- agents/workshop/layouts.md
inline void adopt_known_setup(SetupState& s, const std::string& path, const Setup& known) {
    if (path.empty()) {
        return;
    }
    if (s.active_link.path == path) {
        s.active_link.known = known;
    }
    for (Layout& shelved : s.shelved) {
        if (shelved.link.path == path) {
            shelved.link.known = known;
        }
    }
}

/// THE POSITION ONE STEP ALONG THE RUN, WRAPPING -- the keyboard's whole
/// traversal law, over the ENTIRE population rather than over whatever the band
/// had room to paint. `by` is +1 or -1.
inline std::size_t layout_step(const SetupState& s, std::int64_t by) noexcept {
    const std::size_t n = layout_count(s);
    const std::size_t at = s.active_at < n ? s.active_at : 0;
    if (n <= 1) {
        return at;
    }
    return by < 0 ? (at == 0 ? n - 1 : at - 1) : (at + 1 == n ? 0 : at + 1);
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETUP_HPP
