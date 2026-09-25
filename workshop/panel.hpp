// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANEL_HPP
#define ZENGINE_WORKSHOP_PANEL_HPP

// The panel catalog, the panels open this session, and each open panel's view.
// Workshop law: agents/workshop/maker-pane.md (+10 registers; agents/workshop.md routes)

#include "pane_definition.hpp"
#include "pane_vocabulary.hpp"
#include "pane_canvas_vocabulary.hpp"
#include <zen/switchboard/message.hpp>

#include "builder/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// No kind: a `kind` field before anybody said which, and a pane this build cannot present.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md; WL-FRONT-04 -- agents/workshop/planes.md
inline constexpr std::int64_t kNoPaneKind = -1;

/// The KINDS of panel this Workshop can present.
// WL-CAT-01 -- agents/workshop/catalog.md
namespace panel {
// 0-3 and 5 are unused: a kind is a session-local handle, so renumbering would buy nothing.
/// WORKSHOP'S OWN STANDING IDENTITY, AS A PANE.
// WL-PRESS-05 -- agents/workshop/press-chain.md; WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kLayouts = 4;
} // namespace panel

/// Where a panel kind is presented: one of three named places.
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
namespace placement {
/// The column at the workspace's right edge: fixed width, and reserving nothing.
// WL-GEO-03 -- agents/workshop/geometry.md; WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kSideRegion = 0;
/// Over the workspace, from the canvas's top-left, stacked downwards.
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kOverlayStack = 1;
/// The rows at the top of the canvas: full width, against the top edge, and reserved
/// whether or not anything is in them.
// WL-GEO-03 -- agents/workshop/geometry.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kTopBand = 2;
} // namespace placement

/// WHOSE PANES THE BUILT-INS ARE — the provider/service key every catalog row
/// below carries, and the first half of a durable `PaneRef`.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr const char* kWorkshopProvider = "zengine.workshop";

/// WHOSE PANES THE MAKER-MADE ONES ARE -- the provider half of the durable `PaneRef` a pane
/// created inside Workshop carries (`maker_pane_ref`, setup.hpp), and a namespace this
/// application OWNS.
// WL-MAKER-03 -- agents/workshop/maker-pane.md
inline constexpr const char* kMakerPaneProvider = "zengine.workshop.maker";

/// The Info pane's office, spelled for `default_setup`'s desk and not a catalog row. A literal:
/// this host does not link the weave, and a case checks the two spellings agree.
// WL-INFO-11 -- agents/workshop/info-body.md
inline constexpr const char* kInfoPaneProvider = "zengine.info";
inline constexpr const char* kInfoPaneKey = "info";

/// One entry in the catalog: what a maker sees in the Pane Manager, where the thing
/// they open will be, and WHAT TO CALL IT IN A FILE.
// WL-FOCUS-02 -- agents/workshop/focus.md; WL-SETUP-01 -- agents/workshop/setup-file.md
struct PanelKind {
    std::int64_t kind = kNoPaneKind;
    std::int64_t placed_in = placement::kOverlayStack; ///< which place it is in
    const char* provider = kWorkshopProvider; ///< the durable provider/service key
    const char* pane = "";    ///< the durable pane key, in that provider's namespace
    const char* name = "";    ///< what the Pane Manager lists
    const char* summary = ""; ///< one line, so a maker can tell what they are opening
    /// CAN A PRESS INTO THIS BUILT-IN POINT THE KEYBOARD AT IT?
    // WL-FOCUS-02 -- agents/workshop/focus.md
    bool takes_keyboard = false;
};

/// The built-ins' pane keys in a saved setup, named once because the suite and the file format
/// both spell them.
namespace pane_key {
// Retired keys are spelled only in `pane_migration.hpp`, as facts about files already written.
inline constexpr const char* kLayouts = "layouts";
} // namespace pane_key

/// The catalog: the one pane this host presents itself. Every other pane is offered by an office.
// WL-FOCUS-02 -- agents/workshop/focus.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr PanelKind kPanelCatalog[] = {
    // Layouts holds no state of its own (it is `SetupState`'s), so closing it loses no layout; it
    // takes no keyboard, because its gestures are the pointer's and the keymap's.
    {panel::kLayouts, placement::kTopBand, kWorkshopProvider, pane_key::kLayouts, "Layouts",
     "layout tabs and setup"},
};

// WL-CAT-01 -- agents/workshop/catalog.md
inline constexpr std::size_t kPanelKinds = sizeof(kPanelCatalog) / sizeof(kPanelCatalog[0]);

/// The catalog entry for a kind, or the first one: total, because a kind can arrive from a cursor.
inline constexpr const PanelKind& panel_kind(std::int64_t kind) noexcept {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (kPanelCatalog[i].kind == kind) {
            return kPanelCatalog[i];
        }
    }
    return kPanelCatalog[0];
}

/// WHERE THE SESSION-LOCAL KINDS BEGIN, and the whole of how a runtime
/// pane is told from a built-in one.
// WL-MAKER-04 -- agents/workshop/maker-pane.md
// WL-CAT-01 -- agents/workshop/catalog.md
inline constexpr std::int64_t kFirstRuntimeKind = 1024;

/// Is this a session-local runtime kind rather than a compile-time one?
// WL-CAT-01 -- agents/workshop/catalog.md
inline constexpr bool is_runtime_kind(std::int64_t kind) noexcept {
    return kind >= kFirstRuntimeKind;
}

/// THE HANDLE A MAKER-MADE PANE IS PRESENTED UNDER -- a third class of kind beside the
/// compile-time built-ins and the session-minted runtime handles.
// WL-MAKER-03, WL-MAKER-04 -- agents/workshop/maker-pane.md
inline constexpr std::int64_t kMakerPaneKind = 512;

/// Is this the maker-made pane's handle?
inline constexpr bool is_maker_kind(std::int64_t kind) noexcept { return kind == kMakerPaneKind; }

static_assert(kMakerPaneKind < kFirstRuntimeKind,
              "the maker-made pane's handle sits below the runtime range, so no arithmetic can "
              "confuse the two");

/// WHERE THIS KIND IS PRESENTED — the question a painter asks instead of knowing
/// a column.
// WL-MAKER-04 -- agents/workshop/maker-pane.md; WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t placement_of(std::int64_t kind) noexcept {
    if (is_runtime_kind(kind) || is_maker_kind(kind)) {
        return placement::kOverlayStack;
    }
    return panel_kind(kind).placed_in;
}

/// MAY A PRESS INTO THIS KIND POINT THE KEYBOARD AT IT?
// WL-FOCUS-02 -- agents/workshop/focus.md; WL-MAKER-04 -- agents/workshop/maker-pane.md
inline constexpr bool kind_takes_keyboard(std::int64_t kind) noexcept {
    if (is_runtime_kind(kind)) {
        return true;
    }
    if (is_maker_kind(kind)) {
        return false;
    }
    return panel_kind(kind).takes_keyboard;
}

/// How many kinds declare a given place; asked only by the assertions below.
inline constexpr std::size_t kinds_placed_in(std::int64_t where) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (kPanelCatalog[i].placed_in == where) {
            ++n;
        }
    }
    return n;
}

static_assert(kinds_placed_in(placement::kSideRegion) == 0,
              "no built-in kind declares the right column: a desk row names it, and a catalog "
              "row that took it back would be this host deciding where a weave's pane goes");

static_assert(kinds_placed_in(placement::kTopBand) == 1,
              "the top band has room for one pane: a second kind placed there would "
              "resolve to the same bounds and paint over the first");

namespace detail {

/// A `constexpr` walk: `std::strcmp` is not usable in a constant expression on every toolchain.
inline constexpr bool same_key(const char* a, const char* b) noexcept {
    if (a == nullptr || b == nullptr) {
        return a == b;
    }
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

inline constexpr bool blank_key(const char* a) noexcept { return a == nullptr || *a == '\0'; }

} // namespace detail

/// Does every catalog row carry a durable reference at all, and is no two rows'
/// reference the same one?
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr bool every_kind_is_referable() noexcept {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (detail::blank_key(kPanelCatalog[i].provider) ||
            detail::blank_key(kPanelCatalog[i].pane)) {
            return false;
        }
    }
    return true;
}

inline constexpr bool every_reference_is_one_kind() noexcept {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        for (std::size_t j = i + 1; j < kPanelKinds; ++j) {
            if (detail::same_key(kPanelCatalog[i].provider, kPanelCatalog[j].provider) &&
                detail::same_key(kPanelCatalog[i].pane, kPanelCatalog[j].pane)) {
                return false;
            }
        }
    }
    return true;
}

static_assert(every_kind_is_referable(),
              "every panel kind needs a durable provider/pane reference: a kind without one "
              "cannot be named in a saved setup, and nothing at runtime would say so");
static_assert(every_reference_is_one_kind(),
              "two panel kinds share one durable reference: a saved setup naming it would "
              "resolve to whichever of them the catalog happens to list first");

/// WHAT PROJECT REALIZATION IS WAITING ON, RIGHT NOW — a VALUE, derived at every
/// spend and held by nobody.
// WL-ATTN-04 -- agents/workshop/attention.md
struct ProjectFrontier {
    bool waiting = false;     ///< realization is stopped at a row waiting on the maker
    std::string artifact;     ///< the frontier artifact stem; empty when not waiting
    std::size_t blocked = 0;  ///< authored rows behind the frontier, waiting on it
};

/// One pane an office offered this run: session state, never persisted. `provider` is the office
/// Loom stamped on the offer (`mail.authored_role()`), never a payload field.
struct RuntimePane {
    std::int64_t kind = kFirstRuntimeKind; ///< the session-local handle; see `is_runtime_kind`
    std::string provider;                  ///< the Loom-stamped office that offered it
    std::string pane;                      ///< the pane key, in that office's namespace
    std::string name;                      ///< what the Pane Manager lists
    std::string summary;                   ///< one line, beside the name
    /// The actions this pane declared, as admitted; what is in force is `Keymap::panes`.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    /// Held as v2 whichever version was declared: a v1 row is widened at the door, so admission,
    /// legends and dispatch read one population.
    std::vector<v2::PaneActionRow> actions;
    /// WORKSHOP'S NUMBER FOR THE DECLARATION IN `actions` (`ActionsJudged::declaration`), or 0
    /// when nothing this office declared for the pane is in force. An `ActionsWithdrawn` names it.
    // WL-DESK-06 -- agents/workshop/desktop.md
    std::int64_t declaration = 0;
    std::int64_t preferred_rows = 0;
    std::int64_t preferred_columns = 0;
};

/// Catalog rows this session holds, built-ins included.
// WL-CAT-04 -- agents/workshop/catalog.md
inline constexpr std::size_t kMaxPaneCatalogEntries = 32;

/// The runtime catalog, and the mint for its handles.
// WL-CAT-05 -- agents/workshop/catalog.md
struct RuntimeCatalog {
    std::vector<RuntimePane> entries;
    /// The next handle to mint; a refreshed offer keeps its handle, so this advances once per
    /// distinct `PaneRef`.
    std::int64_t next_kind = kFirstRuntimeKind;

    /// Takes views, so asking about an office not yet judged needs no owned copy of it.
    const RuntimePane* find(std::string_view provider, std::string_view pane) const {
        for (const RuntimePane& r : entries) {
            if (r.provider == provider && r.pane == pane) {
                return &r;
            }
        }
        return nullptr;
    }

    const RuntimePane* of_kind(std::int64_t kind) const {
        for (const RuntimePane& r : entries) {
            if (r.kind == kind) {
                return &r;
            }
        }
        return nullptr;
    }
};

// WL-DESK-14 -- agents/workshop/desktop-presenting.md
/// The picture a press is stamped with: the newest one the medium had been handed when the press
/// was read. A picture becomes the stamp once the host's `PictureFence` has come round behind its
/// canvas twice; delivery is single-threaded FIFO, so every press read earlier keeps the older
/// stamp. Bounded: overflow drops the oldest entry, which only keeps a press stamped older.
struct PictureStamp {
    struct InFlight {
        std::int64_t fence = 0;
        std::int64_t picture = 0;
    };
    static constexpr std::size_t kInFlight = 8;
    std::int64_t aimed = 0;          ///< what a press is stamped with now; 0 = none yet
    std::vector<InFlight> in_flight; ///< handed out, not yet fenced, oldest first

    /// Record `picture` as handed out behind fence `fence`; false (and nothing recorded) when it
    /// is already the newest picture handed out.
    bool hand_out(std::int64_t picture, std::int64_t fence) {
        const std::int64_t newest = in_flight.empty() ? aimed : in_flight.back().picture;
        if (picture == newest) {
            return false;
        }
        if (in_flight.size() >= kInFlight) {
            in_flight.erase(in_flight.begin());
        }
        in_flight.push_back(InFlight{fence, picture});
        return true;
    }
    /// Fence `fence` came round the second time: every picture handed out behind it is aimed at.
    void come_round(std::int64_t fence) {
        std::size_t done = 0;
        while (done < in_flight.size() && in_flight[done].fence <= fence) {
            aimed = in_flight[done].picture;
            ++done;
        }
        in_flight.erase(in_flight.begin(), in_flight.begin() + static_cast<std::ptrdiff_t>(done));
    }
    void forget() {
        aimed = 0;
        in_flight.clear();
    }
};

/// AN OPEN EXTERNAL PANEL'S VIEW OF THE PANE IT PRESENTS -- a COPY, and session.
// WL-ATTN-04 -- agents/workshop/attention.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
struct ExternalPane {
    std::int64_t kind = kFirstRuntimeKind;
    std::int64_t rows = 0;    ///< the last prose rows granted
    std::int64_t columns = 0; ///< ...and the last prose columns
    bool granted = false;     ///< whether any room has been sent for this presentation
    bool heard = false;       ///< whether valid content has ever arrived under it
    bool awaiting = true;     ///< a room is out and no valid content has answered it
    /// Workshop's own sentence about a refused update, never a provider's bytes; empty when none.
    std::string refusal;
    /// ...and why, in the judge's own words.
    // WL-ATTN-04 -- agents/workshop/attention.md
    std::string refusal_why;
    std::vector<surface::SurfaceTextRow> shown;

    /// Where the pane said its caret is, in the body lattice it was granted, and its selection.
    /// Workshop adds its header offset when it merges these; nothing here is a cell or a pixel.
    // WL-CARET-01 -- agents/workshop/pane-caret.md
    std::int64_t caret_row = surface::kNoCaret;
    std::int64_t caret_col = 0;
    std::int64_t sel_begin_row = surface::kNoSelection;
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = surface::kNoSelection;
    std::int64_t sel_end_col = 0;

    /// The generation of the rows admitted here (0 if never said). A projection naming an older
    /// one is refused, so a queued picture of a replaced document cannot repaint its successor.
    std::int64_t content_generation = 0;
    /// The last admitted `v3::PaneContent` number (0 if never numbered). Recorded, not judged, and
    /// not the press stamp: a picture the medium has not been handed is not one a hand aimed at.
    std::int64_t picture = 0;
    /// The press stamp, echoed on `v3::PanePressed` and `PaneButton` so the pane can refuse an
    /// older picture.
    PictureStamp stamp;
    struct Canvas {
        loom::WeaveId owner{};
        std::int64_t grant = 0, x = 0, y = 0, width = 0, height = 0, grain = 0;
        std::int64_t text_advance_px = 0, text_line_px = 0;
        bool graphical = false, heard = false, preview = false;
        PaneCanvasContent content;
    } canvas;
    /// A re-offer (a reloaded image numbers its pictures afresh) or a close: no earlier number
    /// may stamp a press.
    void forget_pictures() {
        picture = 0;
        stamp.forget();
    }

    /// THERE IS NOTHING TO REFUSE ANY MORE -- one door.
    // WL-ATTN-04 -- agents/workshop/attention.md
    void clear_refusal() {
        refusal.clear();
        refusal_why.clear();
    }

    // WL-CARET-02 -- agents/workshop/pane-caret.md
    void clear_caret() {
        caret_row = surface::kNoCaret;
        caret_col = 0;
        sel_begin_row = surface::kNoSelection;
        sel_begin_col = 0;
        sel_end_row = surface::kNoSelection;
        sel_end_col = 0;
    }
};

/// One panel a maker has opened.
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
struct Panel {
    std::int64_t kind = kNoPaneKind;
};

/// Open in a fresh session before any weave speaks; every other pane arrives when offered.
// WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kDefaultPanels[] = {panel::kLayouts};

inline constexpr std::size_t kDefaultPanelCount =
    sizeof(kDefaultPanels) / sizeof(kDefaultPanels[0]);

inline std::vector<Panel> default_panels() {
    std::vector<Panel> open;
    open.reserve(kDefaultPanelCount);
    for (const std::int64_t kind : kDefaultPanels) {
        open.push_back(Panel{kind});
    }
    return open;
}

/// Every dynamic panel this session has open, and the per-kind views. Session, never document.
struct Panels {
    std::vector<Panel> open = default_panels();
    /// The panes offered to this run. Here rather than in `Session` because every question that
    /// needs a runtime pane's name or place is already handed a `Panels`.
    RuntimeCatalog runtime;
    /// THE ONE MAKER-MADE PANE THIS RUN HAS OPEN (`pane_definition.hpp`): its durable
    /// name, its authored interior, the file it stands for and the last value that file held.
    // WL-MAKER-01, WL-MAKER-08 -- agents/workshop/maker-pane.md
    MakerPane maker;
    /// Each open external panel's view: made by the open door, destroyed by the close door.
    std::vector<ExternalPane> external;
    /// AUTHORED INTENT THIS SCREEN HAS NO ROOM FOR, as resolved kinds, in setup
    /// order.
    // WL-PANE-03, WL-PANE-10 -- agents/workshop/panes-and-windows.md
    std::vector<std::int64_t> waiting_for_room;
    /// The keyboard's candidate: the pane a maker last pointed the keys at, not the answer.
    // WL-FOCUS-01, WL-FOCUS-03, WL-FOCUS-05 -- agents/workshop/focus.md
    std::int64_t keyboard = kNoPaneKind;

    /// The pane the maker last pressed into: the selection the foreground order lifts.
    // WL-FRONT-04 -- agents/workshop/planes.md
    // WL-CTX-01 -- agents/workshop/contextual.md
    std::int64_t selected = kNoPaneKind;

    // WL-PANE-13 -- agents/workshop/panes-and-windows.md
    bool has(std::int64_t kind) const {
        for (const Panel& p : open) {
            if (p.kind == kind) {
                return true;
            }
        }
        return false;
    }

    bool waiting(std::int64_t kind) const {
        for (const std::int64_t k : waiting_for_room) {
            if (k == kind) {
                return true;
            }
        }
        return false;
    }

    /// The open external panel's view, or nothing -- by handle, because nothing may hold one
    /// across an offer that could grow `external` or `runtime`.
    ExternalPane* external_pane(std::int64_t kind) {
        for (ExternalPane& e : external) {
            if (e.kind == kind) {
                return &e;
            }
        }
        return nullptr;
    }

    const ExternalPane* external_pane(std::int64_t kind) const {
        for (const ExternalPane& e : external) {
            if (e.kind == kind) {
                return &e;
            }
        }
        return nullptr;
    }
};

/// The selected pane right now, or `kNoPaneKind`, resolved like `keyboard_pane`.
// WL-FRONT-04, WL-FRONT-05 -- agents/workshop/planes.md
inline std::int64_t selected_pane(const Panels& panels) noexcept {
    const std::int64_t kind = panels.selected;
    return kind != kNoPaneKind && panels.has(kind) ? kind : kNoPaneKind;
}

/// The external pane the keyboard points at right now, or `kNoPaneKind`, resolved at each spend.
// WL-FOCUS-01, WL-FOCUS-05, WL-FOCUS-10 -- agents/workshop/focus.md
inline std::int64_t keyboard_pane(const Panels& panels) noexcept {
    const std::int64_t kind = panels.keyboard;
    if (!is_runtime_kind(kind) || !panels.has(kind)) {
        return kNoPaneKind;
    }
    const RuntimePane* row = panels.runtime.of_kind(kind);
    const ExternalPane* pane = panels.external_pane(kind);
    if (row == nullptr || pane == nullptr || !pane->granted) {
        return kNoPaneKind;
    }
    return kind;
}

/// Open a panel of this kind; answers whether anything changed.
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
inline bool open_panel(Panels& panels, std::int64_t kind) {
    if (panels.has(kind)) {
        return false;
    }
    panels.open.push_back(Panel{kind});
    // A presentation and its view have one lifetime, so the open door makes the view too.
    if (is_runtime_kind(kind) && panels.external_pane(kind) == nullptr) {
        ExternalPane fresh;
        fresh.kind = kind;
        panels.external.push_back(std::move(fresh));
    }
    return true;
}

/// Close the panel of this kind, and forget what it was showing.
// WL-LAYOUT-07 -- agents/workshop/layouts.md
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
inline bool close_panel(Panels& panels, std::int64_t kind) {
    for (std::size_t i = 0; i < panels.open.size(); ++i) {
        if (panels.open[i].kind == kind) {
            panels.open.erase(panels.open.begin() + static_cast<std::ptrdiff_t>(i));
            for (std::size_t e = 0; e < panels.external.size(); ++e) {
                if (panels.external[e].kind == kind) {
                    panels.external.erase(panels.external.begin() +
                                          static_cast<std::ptrdiff_t>(e));
                    break;
                }
            }
            return true;
        }
    }
    return false;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANEL_HPP
