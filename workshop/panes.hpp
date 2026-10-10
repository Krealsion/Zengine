// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANES_HPP
#define ZENGINE_WORKSHOP_PANES_HPP

// The pane catalog, the panes open this session, and each open pane's view.
// Workshop law: agents/workshop/panes-and-windows.md (+9 registers; agents/workshop.md routes)

#include "fingerprint.hpp"
#include "pane_vocabulary.hpp"
#include "pane_canvas_vocabulary.hpp"
#include "pane_document.hpp"
#include "pane_settings.hpp"
#include <zen/switchboard/message.hpp>

#include "builder/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// No kind: a `kind` field before anybody said which, and a pane this build cannot present.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md; WL-FRONT-04 -- agents/workshop/planes.md
inline constexpr std::int64_t kNoPaneKind = -1;

/// The built-in KINDS: the panes this Workshop presents itself.
// WL-CAT-01 -- agents/workshop/catalog.md
namespace pane_kind {
// 0-3 and 5 are unused: a kind is a session-local handle, so renumbering would buy nothing.
/// WORKSHOP'S OWN STANDING IDENTITY, AS A PANE.
// WL-PRESS-05 -- agents/workshop/press-chain.md; WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kLayouts = 4;
} // namespace pane_kind

/// Where a pane kind is presented: one of three named places.
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

/// The Info pane's office, spelled for `default_setup`'s desk and not a catalog row. A literal:
/// this host does not link the weave, and a case checks the two spellings agree.
// WL-INFO-11 -- agents/workshop/info-body.md
inline constexpr const char* kInfoPaneProvider = "zengine.info";
inline constexpr const char* kInfoPaneKey = "info";

/// One entry in the catalog: what a weaver sees in the Pane Manager, where the thing
/// they open will be, and WHAT TO CALL IT IN A FILE.
// WL-FOCUS-02 -- agents/workshop/focus.md; WL-SETUP-01 -- agents/workshop/setup-file.md
struct BuiltinPane {
    std::int64_t kind = kNoPaneKind;
    std::int64_t placed_in = placement::kOverlayStack; ///< which place it is in
    const char* provider = kWorkshopProvider; ///< the durable provider/service key
    const char* pane = "";    ///< the durable pane key, in that provider's namespace
    const char* name = "";    ///< what the Pane Manager lists
    const char* summary = ""; ///< one line, so a weaver can tell what they are opening
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
inline constexpr BuiltinPane kBuiltinPanes[] = {
    // Layouts holds no state of its own (it is `SetupState`'s), so closing it loses no layout; it
    // takes no keyboard, because its gestures are the pointer's and the keymap's.
    {pane_kind::kLayouts, placement::kTopBand, kWorkshopProvider, pane_key::kLayouts, "Layouts",
     "layout tabs and setup"},
};

// WL-CAT-01 -- agents/workshop/catalog.md
inline constexpr std::size_t kBuiltinPaneCount = sizeof(kBuiltinPanes) / sizeof(kBuiltinPanes[0]);

/// The catalog entry for a kind, or the first one: total, because a kind can arrive from a cursor.
inline constexpr const BuiltinPane& builtin_pane(std::int64_t kind) noexcept {
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        if (kBuiltinPanes[i].kind == kind) {
            return kBuiltinPanes[i];
        }
    }
    return kBuiltinPanes[0];
}

/// WHERE THE SESSION-LOCAL KINDS BEGIN, and the whole of how a runtime
/// pane is told from a built-in one.
// WL-CAT-01 -- agents/workshop/catalog.md
inline constexpr std::int64_t kFirstRuntimeKind = 1024;

/// Is this a session-local runtime kind rather than a compile-time one?
// WL-CAT-01 -- agents/workshop/catalog.md
inline constexpr bool is_runtime_kind(std::int64_t kind) noexcept {
    return kind >= kFirstRuntimeKind;
}

/// WHERE THIS KIND IS PRESENTED — the question a painter asks instead of knowing
/// a column.
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t placement_of(std::int64_t kind) noexcept {
    if (is_runtime_kind(kind)) {
        return placement::kOverlayStack;
    }
    return builtin_pane(kind).placed_in;
}

/// MAY A PRESS INTO THIS KIND POINT THE KEYBOARD AT IT?
// WL-FOCUS-02 -- agents/workshop/focus.md
inline constexpr bool kind_takes_keyboard(std::int64_t kind) noexcept {
    if (is_runtime_kind(kind)) {
        return true;
    }
    return builtin_pane(kind).takes_keyboard;
}

/// How many kinds declare a given place; asked only by the assertions below.
inline constexpr std::size_t kinds_placed_in(std::int64_t where) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        if (kBuiltinPanes[i].placed_in == where) {
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
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        if (detail::blank_key(kBuiltinPanes[i].provider) ||
            detail::blank_key(kBuiltinPanes[i].pane)) {
            return false;
        }
    }
    return true;
}

inline constexpr bool every_reference_is_one_kind() noexcept {
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        for (std::size_t j = i + 1; j < kBuiltinPaneCount; ++j) {
            if (detail::same_key(kBuiltinPanes[i].provider, kBuiltinPanes[j].provider) &&
                detail::same_key(kBuiltinPanes[i].pane, kBuiltinPanes[j].pane)) {
                return false;
            }
        }
    }
    return true;
}

static_assert(every_kind_is_referable(),
              "every pane kind needs a durable provider/pane reference: a kind without one "
              "cannot be named in a saved setup, and nothing at runtime would say so");
static_assert(every_reference_is_one_kind(),
              "two pane kinds share one durable reference: a saved setup naming it would "
              "resolve to whichever of them the catalog happens to list first");

/// WHAT PROJECT REALIZATION IS WAITING ON, RIGHT NOW — a VALUE, derived at every
/// spend and held by nobody.
// WL-ATTN-04 -- agents/workshop/attention.md
struct ProjectFrontier {
    bool waiting = false;     ///< realization is stopped at a row waiting on the weaver
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
    /// THE SETTINGS THIS PANE DECLARED, as admitted, and the weave that declared them: they
    /// count while that weave holds the office (`WorkshopWeave::counted_settings`).
    // WL-SETTING-03 -- agents/workshop/settings.md
    std::vector<PaneSettingRow> settings_rows = {};
    loom::WeaveId settings_from{};
    /// THE MANUAL THIS PANE DECLARED, as admitted, and the weave that declared it: it counts while
    /// that weave holds the office, and a re-offer drops it (`WorkshopWeave::counted_document`).
    // WL-DESK-06 -- agents/workshop/desktop.md
    PaneDocumentDeclared document = {};
    loom::WeaveId document_from{};
    std::int64_t preferred_rows = 0;
    std::int64_t preferred_columns = 0;
    /// A canvas body asked for in canvas pixels (`v3::PaneOffered`), and the rows of text beneath
    /// it; zero when the pane asked in text, or not at all.
    std::int64_t preferred_width = 0;
    std::int64_t preferred_height = 0;
    std::int64_t preferred_text_rows = 0;
};

/// Catalog rows this session holds, built-ins included: room for the standard panes, Info's and
/// the view host's views, and Inventory's portable views beside them.
// WL-CAT-04 -- agents/workshop/catalog.md
inline constexpr std::size_t kMaxPaneCatalogEntries = 96;

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

// WL-DESK-14 -- agents/workshop/desktop-presenting.md; WL-READ-07 -- agents/workshop/desk-read.md
/// The picture a press is stamped with: the newest one the medium had been handed when the press
/// was read, aimed at once the host's `PictureFence` has come round behind its canvas twice;
/// delivery is single-threaded FIFO, so every press read earlier keeps the older stamp. Bounded:
/// overflow drops the oldest entry, which only keeps a press stamped older. Each picture carries
/// the fingerprint of what it shows (`picture_fingerprint`), which a reading's stamp names: one
/// handed out under the number already newest replaces that number's fingerprint where it stands.
struct PictureStamp {
    struct InFlight {
        std::int64_t fence = 0;
        std::int64_t picture = 0;
        std::int64_t fingerprint = 0;
    };
    static constexpr std::size_t kInFlight = 8;
    std::int64_t aimed = 0;             ///< what a press is stamped with now; 0 = none yet
    std::int64_t aimed_fingerprint = 0; ///< ...and what that picture shows; 0 = nothing yet
    std::vector<InFlight> in_flight;    ///< handed out, not yet fenced, oldest first

    /// Record `picture`, showing `fingerprint`, as handed out behind fence `fence`; false when it
    /// is already the newest picture handed out, whose fingerprint becomes `fingerprint`.
    bool hand_out(std::int64_t picture, std::int64_t fence, std::int64_t fingerprint = 0) {
        const std::int64_t newest = in_flight.empty() ? aimed : in_flight.back().picture;
        if (picture == newest) {
            (in_flight.empty() ? aimed_fingerprint : in_flight.back().fingerprint) = fingerprint;
            return false;
        }
        if (in_flight.size() >= kInFlight) {
            in_flight.erase(in_flight.begin());
        }
        in_flight.push_back(InFlight{fence, picture, fingerprint});
        return true;
    }
    /// Fence `fence` came round the second time: every picture handed out behind it is aimed at.
    void come_round(std::int64_t fence) {
        std::size_t done = 0;
        while (done < in_flight.size() && in_flight[done].fence <= fence) {
            aimed = in_flight[done].picture;
            aimed_fingerprint = in_flight[done].fingerprint;
            ++done;
        }
        in_flight.erase(in_flight.begin(), in_flight.begin() + static_cast<std::ptrdiff_t>(done));
    }
    void forget() {
        aimed = 0;
        aimed_fingerprint = 0;
        in_flight.clear();
    }
};

/// AN OPEN EXTERNAL PANE'S VIEW OF THE PANE IT PRESENTS -- a COPY, and session.
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
    /// ...and the canvas room grant a refused picture was drawn for, so a refusal of the picture a
    /// new room asked for is told from one made before it; 0 when none.
    std::int64_t refused_grant = 0;
    std::vector<surface::SurfaceTextRow> shown;
    /// The parts of those rows the pane names, admitted with them and gone with them.
    std::vector<PaneRowPart> parts;

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
    /// THE SETTINGS LAST HANDED TO THE OFFICE'S HOLDER for this presentation, and that holder.
    /// Nothing until the first hand-off, which is not the empty list a hand-off may carry.
    // WL-SETTING-04 -- agents/workshop/settings.md
    std::optional<std::vector<PaneSetting>> settings_heard;
    loom::WeaveId settings_holder{};
    struct Canvas {
        loom::WeaveId owner{};
        std::int64_t grant = 0, x = 0, y = 0, width = 0, height = 0, grain = 0;
        std::int64_t text_advance_px = 0, text_line_px = 0;
        bool graphical = false, heard = false, preview = false;
        /// The holder speaks only the earlier canvas doors, in sub-units: its room, pointer and
        /// hover cross times `kPaneCanvasLegacySubs`, and its picture is read at their floor.
        bool legacy = false;
        /// A held press keeps this room from the title row its keys bring or take (WL-FOCUS-11):
        /// until the press ends the picture stands here, where it was drawn, painted and read
        /// alike, and the pane's prose room waits with it.
        bool title_waits = false;
        /// The title rows this room was granted under (`external_title_rows`): a held press keeps
        /// the room only while the pane's title rows are no longer these and its place still
        /// gives it under them, so a pane that moved keeps nothing.
        std::int64_t title_rows = 0;
        /// The picture last admitted, in the current form, with the parts it names.
        v5::PaneCanvasContent content;
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
        refused_grant = 0;
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

// WL-READ-07 -- agents/workshop/desk-read.md
/// WHAT AN OPEN EXTERNAL PANE SHOWS, AS A FINGERPRINT: what its office sent and Workshop holds --
/// a picture's rects, labels, runs and parts, never its pane, room grant or number; or prose rows,
/// their parts and the caret -- whether it has said anything yet, Workshop's own words in place of
/// an update it refused and whether that refusal is the current room's. The same picture sent
/// again is the same fingerprint, whatever it was numbered.
inline std::int64_t picture_fingerprint(const ExternalPane& pane) {
    Fingerprint f;
    const ExternalPane::Canvas& c = pane.canvas;
    if (c.heard || c.preview) {
        f.number(static_cast<std::int64_t>(c.content.rects.size()));
        for (const PaneCanvasRect& r : c.content.rects) {
            for (const std::int64_t n : {r.x, r.y, r.w, r.h, r.role}) f.number(n);
        }
        f.number(static_cast<std::int64_t>(c.content.labels.size()));
        for (const PaneCanvasLabel& l : c.content.labels) {
            f.number(l.x);
            f.number(l.y);
            f.bytes(l.text);
            f.number(l.role);
        }
        f.number(static_cast<std::int64_t>(c.content.texts.size()));
        for (const v2::PaneCanvasText& t : c.content.texts) {
            f.number(t.x);
            f.number(t.y);
            f.bytes(t.text);
            for (const std::int64_t n :
                 {t.role, t.caret_col, t.sel_begin_col, t.sel_end_col,
                  static_cast<std::int64_t>(t.padded), t.background}) {
                f.number(n);
            }
        }
        f.number(static_cast<std::int64_t>(c.content.parts.size()));
        for (const PaneCanvasPart& p : c.content.parts) {
            f.bytes(p.name);
            for (const std::int64_t n : {p.x, p.y, p.w, p.h}) f.number(n);
        }
    } else {
        f.number(static_cast<std::int64_t>(pane.shown.size()));
        for (const surface::SurfaceTextRow& row : pane.shown) {
            f.bytes(row.text);
            f.number(row.role);
            f.number(row.background);
        }
        f.number(static_cast<std::int64_t>(pane.parts.size()));
        for (const PaneRowPart& p : pane.parts) {
            f.bytes(p.name);
            for (const std::int64_t n : {p.row, p.column, p.columns}) f.number(n);
        }
        for (const std::int64_t n : {pane.caret_row, pane.caret_col, pane.sel_begin_row,
                                     pane.sel_begin_col, pane.sel_end_row, pane.sel_end_col}) {
            f.number(n);
        }
    }
    // ...and what Workshop paints of it: "(waiting for the provider)" until it is heard, and the
    // refusal's sentence, never its reason, which Attention says.
    f.number(pane.heard ? 1 : 0);
    f.bytes(pane.refusal);
    f.number(pane.refused_grant != 0 && pane.refused_grant == c.grant ? 1 : 0);
    return f.value();
}

/// One pane a weaver has opened.
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
struct OpenPane {
    std::int64_t kind = kNoPaneKind;
};

/// Open in a fresh session before any weave speaks; every other pane arrives when offered.
// WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kDefaultPanes[] = {pane_kind::kLayouts};

inline constexpr std::size_t kDefaultPaneCount =
    sizeof(kDefaultPanes) / sizeof(kDefaultPanes[0]);

inline std::vector<OpenPane> default_panes() {
    std::vector<OpenPane> open;
    open.reserve(kDefaultPaneCount);
    for (const std::int64_t kind : kDefaultPanes) {
        open.push_back(OpenPane{kind});
    }
    return open;
}

/// Every dynamic pane this session has open, and the per-kind views. Session, never document.
struct Panes {
    std::vector<OpenPane> open = default_panes();
    /// The panes offered to this run. Here rather than in `Session` because every question that
    /// needs a runtime pane's name or place is already handed a `Panes`.
    RuntimeCatalog runtime;
    /// Each open external pane's view: made by the open door, destroyed by the close door.
    std::vector<ExternalPane> external;
    /// The keyboard's candidate: the pane a weaver last pointed the keys at, not the answer.
    // WL-FOCUS-01, WL-FOCUS-03, WL-FOCUS-05 -- agents/workshop/focus.md
    std::int64_t keyboard = kNoPaneKind;

    /// The pane the weaver last pressed into: the selection the foreground order lifts.
    // WL-FRONT-04 -- agents/workshop/planes.md
    // WL-CTX-01 -- agents/workshop/contextual.md
    std::int64_t selected = kNoPaneKind;

    // WL-PANE-13 -- agents/workshop/panes-and-windows.md
    bool has(std::int64_t kind) const {
        for (const OpenPane& p : open) {
            if (p.kind == kind) {
                return true;
            }
        }
        return false;
    }

    /// The open external pane's view, or nothing -- by handle, because nothing may hold one
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
inline std::int64_t selected_pane(const Panes& panes) noexcept {
    const std::int64_t kind = panes.selected;
    return kind != kNoPaneKind && panes.has(kind) ? kind : kNoPaneKind;
}

/// The external pane the keyboard points at right now, or `kNoPaneKind`, resolved at each spend.
// WL-FOCUS-01, WL-FOCUS-05, WL-FOCUS-10 -- agents/workshop/focus.md
inline std::int64_t keyboard_pane(const Panes& panes) noexcept {
    const std::int64_t kind = panes.keyboard;
    if (!is_runtime_kind(kind) || !panes.has(kind)) {
        return kNoPaneKind;
    }
    const RuntimePane* row = panes.runtime.of_kind(kind);
    const ExternalPane* pane = panes.external_pane(kind);
    if (row == nullptr || pane == nullptr || !pane->granted) {
        return kNoPaneKind;
    }
    return kind;
}

/// Open a pane of this kind; answers whether anything changed.
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
inline bool open_kind(Panes& panes, std::int64_t kind) {
    if (panes.has(kind)) {
        return false;
    }
    panes.open.push_back(OpenPane{kind});
    // A presentation and its view have one lifetime, so the open door makes the view too.
    if (is_runtime_kind(kind) && panes.external_pane(kind) == nullptr) {
        ExternalPane fresh;
        fresh.kind = kind;
        panes.external.push_back(std::move(fresh));
    }
    return true;
}

/// Close the pane of this kind, and forget what it was showing.
// WL-LAYOUT-07 -- agents/workshop/layouts.md
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
inline bool close_kind(Panes& panes, std::int64_t kind) {
    for (std::size_t i = 0; i < panes.open.size(); ++i) {
        if (panes.open[i].kind == kind) {
            panes.open.erase(panes.open.begin() + static_cast<std::ptrdiff_t>(i));
            for (std::size_t e = 0; e < panes.external.size(); ++e) {
                if (panes.external[e].kind == kind) {
                    panes.external.erase(panes.external.begin() +
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

#endif // ZENGINE_WORKSHOP_PANES_HPP
