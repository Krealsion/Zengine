// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANES_HPP
#define ZENGINE_WORKSHOP_PANES_HPP

// The pane catalog, the panes open this session, and each open pane's view.
// Workshop law: agents/workshop/panes-and-windows.md (+9 registers; agents/workshop.md routes)

#include "fingerprint.hpp"
#include "pane_vocabulary.hpp"
#include "pane_settings.hpp"
#include <zen/switchboard/message.hpp>

#include "builder/vocabulary.hpp"
#include "graphics/vocabulary.hpp"

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

/// Where a pane kind is presented by default: one of two named places.
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
namespace placement {
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

/// What one offer asked a pane's body to hold: rows and columns of text, or a canvas body of
/// pixels with rows of text beneath it; all zero for an offer that asked for nothing.
struct PaneOfferedBody {
    std::int64_t rows = 0, columns = 0, width = 0, height = 0, text_rows = 0;

    friend bool operator==(const PaneOfferedBody&, const PaneOfferedBody&) = default;
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
    /// Held as v4 whichever version was declared: a v1 to v3 row is widened at the door with no
    /// causes it did not say and no POSIX key, so admission, legends, dispatch and every reader
    /// read one population.
    std::vector<v4::PaneActionRow> actions;
    /// THE MODE THOSE ROWS ARE IN FORCE FOR, AND WHAT EACH OTHER MODE MAY CAUSE, as declared with
    /// them and replaced with them; empty for a declaration that names none.
    // WL-CAUSE-02 -- agents/workshop/action-causes.md
    std::string mode = {};
    std::vector<ModeCauses> elsewhere = {};
    /// WORKSHOP'S NUMBER FOR THE DECLARATION IN `actions` (`ActionsJudged::declaration`), or 0
    /// when nothing this office declared for the pane is in force. An `ActionsWithdrawn` names it.
    // WL-DESK-06 -- agents/workshop/desktop.md
    std::int64_t declaration = 0;
    std::int64_t preferred_rows = 0;
    std::int64_t preferred_columns = 0;
    /// A canvas body asked for in canvas pixels (`v3::PaneOffered`), and the rows of text beneath
    /// it; zero when the pane asked in text, or not at all.
    std::int64_t preferred_width = 0;
    std::int64_t preferred_height = 0;
    std::int64_t preferred_text_rows = 0;
    /// WHAT THE PANE ASKS FOR NOW, which `fit` reads while `default` keeps the first offer above:
    /// the latest offer's body, which the terminal window follows, and the one `fit` stands on in
    /// the OS window -- the latest, unless that is within one pixel of it on every axis.
    // WL-PANE-17 -- agents/workshop/panes-and-windows.md
    PaneOfferedBody latest = {};
    PaneOfferedBody standing = {};
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

/// AN OPEN EXTERNAL PANE'S VIEW OF THE PANE IT PRESENTS -- what Workshop knows of it beside its
/// catalog row: where its area was last placed, and its chrome.
/// Nothing of what the pane draws: that is the pane's, in its own area. Session, not document.
// WL-PANE-06 -- agents/workshop/panes-and-windows.md
struct ExternalPane {
    std::int64_t kind = kFirstRuntimeKind;
    /// Whether its office draws in a graphics window -- its holder takes `graphics::AreaRoom` --
    /// asked of the live holder before every paint; false while no participant holds the office.
    bool in_window = false;
    /// Whether a participant holds its office now, asked with `in_window`.
    bool held = false;
    /// WHERE ITS AREA WAS LAST PLACED in the desk's pixels, and under how many title rows: a press
    /// a window holds there keeps the area where it was while only the title row its keys bring or
    /// take would move it (WL-FOCUS-11), and the frame draws no title over it meanwhile.
    std::int64_t placed_x = 0, placed_y = 0, placed_w = 0, placed_h = 0;
    std::int64_t placed_titles = 0;
    bool title_waits = false;
    /// The generation the managed opening that seated it admitted (0 if none).
    std::int64_t content_generation = 0;
    /// WHETHER THIS PANE WEARS ITS `?` AND ITS GEAR: its holder takes `PaneDocumentRequested` now,
    /// and the application declares the row that shows a manual. Read again before every paint,
    /// so it is the live holder's answer; nothing of the manual is kept.
    // WL-CHROME-09 -- agents/workshop/chrome.md
    bool manual = false;
};

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
    if (row == nullptr || pane == nullptr) {
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
