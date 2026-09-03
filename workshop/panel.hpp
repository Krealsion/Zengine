// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANEL_HPP
#define ZENGINE_WORKSHOP_PANEL_HPP

// Workshop's dynamic panels: the catalog of what a maker may open, what is
// currently open, and each open panel's own view of the thing it presents.
// Workshop law: agents/workshop/maker-pane.md (+10 registers; agents/workshop.md routes)

#include "files.hpp"
#include "pane_definition.hpp"

#include "builder/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// The KINDS of panel this Workshop can present.
namespace panel {
inline constexpr std::int64_t kBuilder = 0;
inline constexpr std::int64_t kInfo = 1;
inline constexpr std::int64_t kEditor = 2;
inline constexpr std::int64_t kProjectFiles = 3;
/// WORKSHOP'S OWN STANDING IDENTITY, AS A PANE.
// WL-PRESS-05 -- agents/workshop/press-chain.md; WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kLayouts = 4;
/// THE PANE EDITOR: the built-in whose SUBJECT is an ordinary Workshop pane.
// WL-PED-01 -- agents/workshop/pane-manager.md
inline constexpr std::int64_t kPaneEditor = 5;
} // namespace panel

/// WHERE a panel kind is presented. Three, because Workshop has three places and no
/// more, and each is a NAME for a place this screen already had rather than a
/// coordinate somebody chose.
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
namespace placement {
/// The reserved column beside the workspace: fixed width, against the right
/// edge, and reserved whether or not anything is in it.
// WL-GEO-03 -- agents/workshop/geometry.md; WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kSideRegion = 0;
/// Over the workspace, from the canvas's top-left, stacked downwards — the
/// terminal overlay's mechanism pointed at the other corner.
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr std::int64_t kOverlayStack = 1;
/// The rows at the top of the canvas: full width, against the top edge, and reserved
/// whether or not anything is in them.
// WL-GEO-03 -- agents/workshop/geometry.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kTopBand = 2;
} // namespace placement

/// IS THIS PLACE THE MAKER'S TO AUTHOR?
// WL-PANE-01, WL-PANE-08 -- agents/workshop/panes-and-windows.md
inline constexpr bool place_is_authorable(std::int64_t where) noexcept {
    return where != placement::kSideRegion;
}

/// WHOSE PANES THE BUILT-INS ARE — the provider/service key every catalog row
/// below carries, and the first half of a durable `PaneRef`.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr const char* kWorkshopProvider = "zengine.workshop";

/// WHOSE PANES THE MAKER-MADE ONES ARE -- the provider half of the durable `PaneRef` a pane
/// created inside Workshop carries (`maker_pane_ref`, setup.hpp), and a namespace this
/// application OWNS.
// WL-MAKER-03 -- agents/workshop/maker-pane.md
inline constexpr const char* kMakerPaneProvider = "zengine.workshop.maker";

/// One entry in the catalog: what a maker sees in the picker, where the thing
/// they open will be, and WHAT TO CALL IT IN A FILE.
// WL-FOCUS-02 -- agents/workshop/focus.md; WL-SETUP-01 -- agents/workshop/setup-file.md
struct PanelKind {
    std::int64_t kind = panel::kBuilder;
    std::int64_t placed_in = placement::kOverlayStack; ///< which of the two places it is in
    const char* provider = kWorkshopProvider; ///< the durable provider/service key
    const char* pane = "";    ///< the durable pane key, in that provider's namespace
    const char* name = "";    ///< what the picker lists
    const char* summary = ""; ///< one line, so a maker can tell what they are opening
    /// CAN A PRESS INTO THIS BUILT-IN POINT THE KEYBOARD AT IT?
    // WL-FOCUS-02 -- agents/workshop/focus.md
    bool takes_keyboard = false;
};

/// The pane keys the built-ins are spelled with in a saved setup. Named
/// constants rather than literals in the catalog, because the suite and the
/// file format both have to say them and a typo in one of three copies is a
/// setup that loads as unresolved.
namespace pane_key {
inline constexpr const char* kBuilder = "builder";
inline constexpr const char* kInfo = "info";
inline constexpr const char* kEditor = "editor";
inline constexpr const char* kProjectFiles = "project-files";
inline constexpr const char* kLayouts = "layouts";
inline constexpr const char* kPaneEditor = "pane-editor";
} // namespace pane_key

/// THE CATALOG. Workshop's own, and complete: a panel that is not here cannot be
/// opened, because the picker is the only door and the picker walks this array.
// WL-FOCUS-02 -- agents/workshop/focus.md
// WL-PED-01 -- agents/workshop/pane-manager.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr PanelKind kPanelCatalog[] = {
    {panel::kBuilder, placement::kOverlayStack, kWorkshopProvider, pane_key::kBuilder, "Builder",
     "build a chosen recipe"},
    {panel::kInfo, placement::kSideRegion, kWorkshopProvider, pane_key::kInfo, "Info",
     "objects and properties"},
    // THE SOURCE EDITOR'S PRESENTATION, AND ONLY ITS PRESENTATION. The document -- the
    // path, the buffer, the saved copy, the dirty answer -- is Session state
    // (`Session::editor`), which is exactly what makes removing, hiding or rearranging
    // this pane unable to lose one byte of unsaved source: a panel is a presentation,
    // and closing one destroys a presentation. Info's own shape, holding a document
    // instead of holding nothing.
    {panel::kEditor, placement::kOverlayStack, kWorkshopProvider, pane_key::kEditor, "Editor",
     "edit a source file", true},
    // THE PROJECT ON DISK, AS A PLACE A MAKER CAN LEAVE OPEN. An ordinary catalog row and
    // nothing more: it opens and closes through the picker, arranges, stacks, hides and
    // rides a saved setup exactly as the three above do, because it is a pane and not a
    // dialog. Deliberately NOT a modal picker -- a browser a maker must reopen every time
    // they want to look at their project is a browser they stop using -- and deliberately
    // not a provider pane, since nothing external offers it.
    //
    // ITS STATE IS `Panels::files`, and none of it is durable. What persists is that the
    // PANE is on the desk, which is the setup's business and arrives for free.
    {panel::kProjectFiles, placement::kOverlayStack, kWorkshopProvider, pane_key::kProjectFiles,
     "Files", "browse and open files", true},
    // WORKSHOP'S OWN STANDING IDENTITY, AS AN ORDINARY ROW. Until this row existed
    // the layout run, the Setup association and the workspace fact were painted by `paint`
    // into a rectangle nothing could name: not in the picker, not in a setup file, not in
    // `occupied_at`, not coverable, and not movable. Nothing about the three facts asked for
    // that -- they were hard-coded by implementation date. What they get here is what every
    // other pane already had, and they spend no authority a pane lacks: pressing a tab calls
    // the same door the key calls.
    //
    // ITS STATE IS THE SETUP'S OWN (`SetupState`), which is why this row carries none: the
    // run, the live position and the association are Workshop-global facts with one owner,
    // and this pane is a PRESENTATION of them exactly as the Editor is a presentation of a
    // document the session holds. Removing it destroys a presentation and no layout.
    //
    // IT DOES NOT TAKE THE KEYBOARD. Its gestures are the pointer's (press a tab, press
    // `+`, press twice to rename, drag to reorder) and the keymap's, and the keymap's reach
    // it wherever the maker is standing -- so a press here is a maker POINTING at their
    // desk's identity, not moving where their typing goes. The name editor is a mode that
    // takes the row while it is open, which is where a typed layout name goes.
    {panel::kLayouts, placement::kTopBand, kWorkshopProvider, pane_key::kLayouts, "Layouts",
     "layout tabs and setup"},
    // THE PANE MANAGER: the name a maker reads. Its durable pane key stays `pane-editor`
    // because a key is a promise to every setup and session file that already names it;
    // what moved is the word, because the surface inventories, places and orders panes and
    // a maker-facing name that claimed to EDIT a pane's inside was claiming a tool that did
    // not exist. The Pane Creator -- the workflow that makes a pane whose inside is authored
    // data -- lives inside this pane (`n`), and is deliberately the narrower name.
    {panel::kPaneEditor, placement::kOverlayStack, kWorkshopProvider, pane_key::kPaneEditor,
     "Pane Manager", "manage a pane", true},
};

inline constexpr std::size_t kPanelKinds = sizeof(kPanelCatalog) / sizeof(kPanelCatalog[0]);

/// The catalog entry for a kind, or the first one. Total, because the kind can
/// arrive from a cursor position and a total function is cheaper than an
/// invariant somebody has to maintain.
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
inline constexpr std::int64_t kFirstRuntimeKind = 1024;

/// Is this a session-local runtime kind rather than a compile-time one?
inline constexpr bool is_runtime_kind(std::int64_t kind) noexcept {
    return kind >= kFirstRuntimeKind;
}

/// NO KIND AT ALL -- what a pane this build cannot present answers with.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md; WL-FRONT-04 -- agents/workshop/planes.md
inline constexpr std::int64_t kNoPaneKind = -1;

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

/// How many kinds declare a given place. Only ever asked at compile time, by the
/// assertion under it.
inline constexpr std::size_t kinds_placed_in(std::int64_t where) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (kPanelCatalog[i].placed_in == where) {
            ++n;
        }
    }
    return n;
}

/// THE SIDE REGION HOLDS EXACTLY ONE PANEL, and this line is the whole of that rule.
static_assert(kinds_placed_in(placement::kSideRegion) == 1,
              "the side region has room for one panel: a second kind placed there would "
              "resolve to the same bounds and paint over the first");

/// THE TOP BAND HOLDS EXACTLY ONE PANE, for the side region's reason word for word.
static_assert(kinds_placed_in(placement::kTopBand) == 1,
              "the top band has room for one pane: a second kind placed there would "
              "resolve to the same bounds and paint over the first");

namespace detail {

/// Two catalog keys, compared. A `constexpr` walk rather than `std::strcmp`,
/// which is not usable in a constant expression on every supported toolchain.
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

/// The `+ panel` picker: open or not, and which entry a maker is on.
struct PanelPicker {
    bool open = false;
    std::size_t cursor = 0;
    double wheel_accum = 0.0; /// < fractional wheel notches not yet worth a row
};

/// WHAT A MAKER CALLS THE PICKER — the words on the hint that opens it, so that a
/// sentence about the box on the screen uses the name printed beside the key that
/// put it there. It is here rather than in the catalog because the picker has no
/// catalog row; it is the one presentation that names itself.
inline constexpr const char* kPickerName = "+ panel";

/// WHAT PROJECT REALIZATION IS WAITING ON, RIGHT NOW — a VALUE, derived at every
/// spend and held by nobody.
// WL-ATTN-04 -- agents/workshop/attention.md
struct ProjectFrontier {
    bool waiting = false;     ///< realization is stopped at a row waiting on the maker
    std::string artifact;     ///< the frontier artifact stem; empty when not waiting
    std::size_t blocked = 0;  ///< authored rows behind the frontier, waiting on it
};

/// THE BUILDER PANEL'S VIEW OF THE BUILDER TOOL — a COPY, and session.
///
/// `heard` is the honest distinction between "the tool says it has never built
/// anything" and "the tool has not answered yet", which a panel must not show as
/// the same thing: the first is a fact about the target, the second is a fact
/// about this panel, and only one of them is worth a maker acting on.
///
/// `awaiting` is the ONLY fact in this struct that is genuinely the panel's own,
/// and it exists because the first live run produced a lie without it. Reopening
/// the panel asks the tool, the tool answers with its last outcome, and the
/// screen announced `built zengine-snake -- exit 0` about a build that had
/// finished a minute earlier — because the arrival of a SUCCESS is not the same
/// event as a build succeeding. A panel may report what it WATCHED happen; what
/// it merely learned belongs in the panel's rows and not in an announcement. So
/// this records "I asked and have not been answered", and it is what decides
/// whether an arriving status is news.
///
/// THE ASYNC BUILD MADE IT WORTH MORE, NOT LESS, and widened exactly one thing about it.
/// A build now has a MIDDLE: `asked` and `running` are both conditions a status
/// can arrive in and neither is an ending, so this fact is held across every one
/// of them and released only at a condition the build will not leave —
/// `builder::still_going` is the one place that list is written down. Without
/// the widening, the first intermediate status would clear it and the real
/// ending would arrive as something this panel merely learned. And the case it
/// now covers is the one the first Builder could never reach at all: a panel OPENED while a
/// child is alive is told `running`, shows it, and announces nothing — because it
/// did not watch this build begin.
///
/// Nothing is authored from this struct and nothing is asked through it. Opening
/// the panel sends `builder::StatusRequested` and everything here arrives as the
/// tool's own published answer.
/// ----...AND IT ALSO HOLDS A CATALOG AND A CHOICE -------------------------------
///
/// `known` is what the tool said it can build, arriving once when this panel opens
/// (`builder::RecipeCatalog`), and `chosen` is which of those rows this maker is
/// looking at. THE CHOICE IS GENUINELY THE PANEL'S and is the only other thing here
/// that is: choosing what to build next is a maker's act on a presentation, and a tool
/// that held a selection would be a tool whose next build depended on who had opened a
/// panel last. What the tool holds is what it BUILT; what this holds is what a maker
/// has picked out.
///
/// ⚠ `chosen` IS AN INDEX INTO `known` AND IS BOUNDED AT USE, never at write. The
/// catalog arrives once and does not change during a run, but a panel that trusted an
/// index across a re-ask would be one arrival away from reading past the end.
///
/// `awaiting_realization` IS `awaiting`'s TWIN AND IT IS HELD LONGER, for the reason
/// `awaiting` exists at all. A build ENDS -- at which point `awaiting` is released and
/// the build's outcome is announced -- and the realization of what it produced is still
/// outstanding at that instant. One latch for both would either release too early (and
/// turn realization's answer into a fact this panel merely learned) or too late (and
/// hold the build's own ending back behind it). Two questions, two latches.
/// ⚠ WHICH CATALOG THIS SESSION IS USING IS DELIBERATELY NOT HERE. This struct is
/// destroyed and remade every time the panel is removed and reopened (`close_panel`), and
/// a maker changing recipe catalogs has not changed anything about a PRESENTATION -- so
/// that fact lives on the `Session`, beside the source document, where removing a pane
/// cannot lose it. What lives here is what this panel was TOLD.
struct BuilderPane {
    bool heard = false;
    bool awaiting = false;
    bool awaiting_realization = false;
    builder::BuildStatus shown{};
    builder::RecipeCatalog known{};
    std::size_t chosen = 0;
    /// HAS THE MAKER EXPLICITLY PICKED A RECIPE since the catalog arrived?
    // WL-PROJ-07 -- agents/workshop/project.md
    bool picked = false;
};

/// ONE ROW OF THE SESSION-LOCAL RUNTIME CATALOG: a pane some office
/// offered this run, admitted under that office's stamped authorship.
///
/// IT IS SESSION STATE AND NOTHING ELSE. Not global, not document, not setup, not
/// persisted, and not shared between processes: a fresh Workshop starts with an
/// empty one and earns every row again from a live offer. The `Setup` a maker
/// saves holds the two strings and never a row of this.
///
/// `provider` IS THE OFFICE LOOM STAMPED ON THE OFFER, copied out of
/// `mail.authored_role()` after it passed the same `check_pane_key` law the
/// persisted grammar uses. It is never read from a payload, because the payload
/// has no such field to read (pane_vocabulary.hpp).
struct RuntimePane {
    std::int64_t kind = kFirstRuntimeKind; ///< the session-local handle; see `is_runtime_kind`
    std::string provider;                  ///< the Loom-stamped office that offered it
    std::string pane;                      ///< the pane key, in that office's namespace
    std::string name;                      ///< what the picker lists
    std::string summary;                   ///< one line, beside the name
};

/// HOW MANY CATALOG ROWS THIS SESSION WILL HOLD IN TOTAL -- built-ins included.
///
/// A RUNTIME-CATALOG POLICY CONSTANT, AND DELIBERATELY NOT AN ALIAS OF
/// `kMaxSetupPanes` even though both are thirty-two today. The two answer
/// different questions: that one bounds what a FILE may name and is a promise to
/// a maker's saved bytes, this one bounds what LIVE OFFERS may make this session
/// retain. Spelling one as the other would make a later phase's change to either
/// silently move the other, which is the shape of a bound that stops meaning
/// anything.
///
/// Thirty-two against two built-ins leaves thirty distinct runtime `PaneRef`s. It
/// is four times the tallest picker this composition can show, which is the same
/// argument `kMaxSetupPanes` is chosen by, and it bounds what a chatty or
/// malicious provider can make this session hold to a few kilobytes.
inline constexpr std::size_t kMaxPaneCatalogEntries = 32;

/// THE RUNTIME CATALOG, and the mint for its handles.
///
/// ORDER IS FIRST-ACCEPTED-OFFER ORDER and is never sorted -- not by role, not by
/// name, not by arrival time, not by display text. The combined picker walks the
/// compile-time catalog and then this, so a maker who opens Workshop twice with
/// the same providers sees the same list in the same order, and a provider cannot
/// buy itself the top of the list by choosing a name.
///
/// NOTHING HOLDS A POINTER INTO `entries`. A later offer may grow the vector and
/// reallocate it, so every consumer looks a row up by handle or by reference at
/// the moment it needs one, and `Occupancy` carries a `std::string` copy rather
/// than a `const char*` into a row that may move (screen.hpp).
struct RuntimeCatalog {
    std::vector<RuntimePane> entries;
    /// The next handle to mint. Monotonic within a session; a refreshed offer
    /// keeps the handle it already had, so this advances at most once per
    /// distinct `PaneRef` and is bounded by `kMaxPaneCatalogEntries`.
    std::int64_t next_kind = kFirstRuntimeKind;

    /// THE LOOKUP TAKES VIEWS, so that asking whether a pane was already
    /// admitted costs no allocation and, more to the point, needs no owned copy of
    /// an office that has not yet been judged. The `PaneContent` door reads Loom's
    /// stamp as a `std::string_view` and asks here with it directly; what it
    /// compares against is the row's OWN string, admitted under `check_pane_key`
    /// and owned by this vector. Views are compared, ownership is not moved, and
    /// nothing here retains the caller's bytes.
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

/// AN OPEN EXTERNAL PANEL'S VIEW OF THE PANE IT PRESENTS -- a COPY, and session.
// WL-ATTN-04 -- agents/workshop/attention.md; WL-PANE-06 -- agents/workshop/panes-and-windows.md
struct ExternalPane {
    std::int64_t kind = kFirstRuntimeKind;
    std::int64_t rows = 0;    ///< the last prose rows granted
    std::int64_t columns = 0; ///< ...and the last prose columns
    bool granted = false;     ///< whether any room has been sent for this presentation
    bool heard = false;       ///< whether valid content has ever arrived under it
    bool awaiting = true;     ///< a room is out and no valid content has answered it
    /// WORKSHOP'S OWN SENTENCE ABOUT A REFUSED UPDATE, bounded and written here
    /// rather than anywhere a provider's bytes could reach. Empty when there is
    /// nothing to refuse.
    std::string refusal;
    /// ...AND WHY, IN THE JUDGE'S OWN WORDS.
    // WL-ATTN-04 -- agents/workshop/attention.md
    std::string refusal_why;
    std::vector<surface::SurfaceTextRow> shown;

    /// THERE IS NOTHING TO REFUSE ANY MORE -- one door.
    // WL-ATTN-04 -- agents/workshop/attention.md
    void clear_refusal() {
        refusal.clear();
        refusal_why.clear();
    }
};

/// One panel a maker has opened.
///
/// It carries a KIND and nothing else. Per-panel view state lives beside the
/// stack rather than inside the instance (`Panels::builder`), because the catalog
/// allows one instance of a kind: a second copy of a tool's status inside each
/// instance would be a shape that only means something once a policy about
/// several instances exists.
struct Panel {
    std::int64_t kind = panel::kBuilder;
};

// WL-TAB-01 -- agents/workshop/tab-run.md
inline constexpr std::int64_t kDefaultPanels[] = {panel::kInfo, panel::kLayouts};

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

/// Every dynamic panel this session has open, plus the picker and the per-kind
/// views. Session, never document.
struct Panels {
    std::vector<Panel> open = default_panels();
    PanelPicker picker;
    BuilderPane builder;
    /// WHAT THE PROJECT BROWSER IS CURRENTLY SHOWING.
    // WL-FILES-01 -- agents/workshop/files.md
    FilesPane files;
    /// THE PANES OFFERED TO THIS RUN, beside the compile-time ones. It
    /// lives here rather than in `Session` for one measured reason: every
    /// presentation question that has to know a runtime pane's NAME or its PLACE
    /// -- the picker's rows, `occupied_at`'s answer, `bounds_of`'s slot -- is
    /// already handed a `Panels`, so putting the catalog anywhere else would have
    /// added a parameter to each of them and given a caller a chance to forget it.
    RuntimeCatalog runtime;
    /// THE ONE MAKER-MADE PANE THIS RUN HAS OPEN (`pane_definition.hpp`): its durable
    /// name, its authored interior, the file it stands for and the last value that file held.
    // WL-MAKER-01, WL-MAKER-08 -- agents/workshop/maker-pane.md
    MakerPane maker;
    /// The per-pane view of each OPEN external panel: its granted room, its copy
    /// of what the provider last said, and whether it is waiting. One entry per
    /// open external kind, created by the open door and destroyed by the close
    /// door — the `BuilderPane` rule, for a population rather than for one kind.
    std::vector<ExternalPane> external;
    /// AUTHORED INTENT THIS SCREEN HAS NO ROOM FOR, as resolved kinds, in setup
    /// order.
    // WL-PANE-03, WL-PANE-10 -- agents/workshop/panes-and-windows.md
    std::vector<std::int64_t> waiting_for_room;
    /// WHICH KEYBOARD-TAKING PANE A MAKER LAST POINTED THE KEYS AT -- an external
    /// pane, or the built-in Editor -- the keyboard's CANDIDATE, and emphatically not
    /// its answer.
    // WL-FOCUS-01, WL-FOCUS-03, WL-FOCUS-05 -- agents/workshop/focus.md
    std::int64_t keyboard = kNoPaneKind;

    /// WHICH PANE THE MAKER LAST PRESSED INTO -- the SELECTED pane, and the
    /// identity the desk's effective foreground order is lifted by.
    // WL-FRONT-04 -- agents/workshop/planes.md
    // WL-CTX-01 -- agents/workshop/contextual.md
    std::int64_t selected = kNoPaneKind;

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

    /// The open external panel's view, or nothing. Const and mutable doors, both
    /// by handle, because nothing may hold one across an offer that could grow
    /// `external` or `runtime`.
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

/// WHICH PANE IS SELECTED RIGHT NOW, or `kNoPaneKind` -- `keyboard_pane`'s twin,
/// and resolved by the same rule for the same reason.
// WL-FRONT-04, WL-FRONT-05 -- agents/workshop/planes.md
inline std::int64_t selected_pane(const Panels& panels) noexcept {
    const std::int64_t kind = panels.selected;
    return kind != kNoPaneKind && panels.has(kind) ? kind : kNoPaneKind;
}

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

/// Open a panel of this kind, or say why not.
///
/// Answers whether anything changed, so the caller can tell a maker the truth
/// either way rather than showing an unchanged screen with no explanation.
inline bool open_panel(Panels& panels, std::int64_t kind) {
    if (panels.has(kind)) {
        return false;
    }
    panels.open.push_back(Panel{kind});
    // AND AN EXTERNAL PANE GETS ITS VIEW BY THE SAME ACT, for the reason
    // the Builder's is forgotten by the closing one: a presentation and its copy
    // of what it presents have one lifetime, and two doors would eventually be
    // walked through in the wrong order. A fresh view is AWAITING with no room
    // granted, which is exactly true -- nothing has been asked for yet.
    if (is_runtime_kind(kind) && panels.external_pane(kind) == nullptr) {
        ExternalPane fresh;
        fresh.kind = kind;
        panels.external.push_back(std::move(fresh));
    }
    return true;
}

/// Close the panel of this kind, and forget what it was showing.
// WL-FILES-05 -- agents/workshop/files.md; WL-LAYOUT-07 -- agents/workshop/layouts.md
inline bool close_panel(Panels& panels, std::int64_t kind) {
    for (std::size_t i = 0; i < panels.open.size(); ++i) {
        if (panels.open[i].kind == kind) {
            panels.open.erase(panels.open.begin() + static_cast<std::ptrdiff_t>(i));
            // The per-kind view, forgotten by the same act. One `if` rather than
            // a virtual `forget()` on a panel base class: one kind exists, and
            // the shape of the second one is not knowledge this phase has.
            if (kind == panel::kBuilder) {
                panels.builder = BuilderPane{};
            }
            // AND INFO HAS NOTHING TO FORGET, which is not an omission here but
            // the whole shape of the second kind: it holds no copy of anything,
            // because what it presents is the document and the session, and both
            // of those outlive it and belong to somebody else. A panel with no
            // state of its own is the case that proves the branch above is one
            // kind's business rather than a slot in a framework.
            //
            // THE EDITOR HAS NOTHING TO FORGET EITHER, AND THAT ABSENCE IS LOAD-BEARING:
            // the source document -- path, buffer, unsaved edits, caret, viewport --
            // is `Session::editor`, so closing this presentation can lose none of it
            // and reopening the pane shows the same document exactly where it was.
            // A dirty buffer disappearing because a pane was removed is the defect
            // this placement exists to make unsayable.
            //
            // AND AN EXTERNAL PANE FORGETS EVERYTHING IT WAS SHOWING:
            // its granted room, its copy of the provider's rows, whether it had
            // heard, whether it was waiting, and any refusal. What it does NOT
            // touch is the provider weave, its office, its semantic state, or
            // its row in the runtime catalog -- closing a presentation sends no
            // unload and retracts no offer, so the same pane reopens from the
            // catalog and asks for room again. The Builder's rule, whole.
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
