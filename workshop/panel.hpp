// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANEL_HPP
#define ZENGINE_WORKSHOP_PANEL_HPP

// Workshop's dynamic panels: the catalog of what a maker may open, what is
// currently open, and each open panel's own view of the thing it presents.
// Workshop law: agents/workshop/maker-pane.md (+10 registers; agents/workshop.md routes)

#include "pane_definition.hpp"
#include "pane_vocabulary.hpp" // PaneActionRow -- what an offered pane declared it can do

#include "builder/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// NO KIND AT ALL -- what a pane this build cannot present answers with, and what a
/// `kind` field holds before anybody has said which.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md; WL-FRONT-04 -- agents/workshop/planes.md
inline constexpr std::int64_t kNoPaneKind = -1;

/// The KINDS of panel this Workshop can present.
// WL-CAT-01 -- agents/workshop/catalog.md
namespace panel {
// 0 IS RETIRED. It was the Builder panel's kind until that pane became a weave
// (`Zengine/builder-pane/`), for 3's reason exactly and with one consequence 3 did not
// have: this was the value every `kind` field defaulted to, on `Panel`, `PanelKind` and
// `CatalogRow`, because it happened to be zero. Those defaults are `kNoPaneKind` now, which
// is what they always meant -- "nobody has said which yet" -- and was only ever spelled as
// the Builder because the Builder was first in a list.
// 1 IS RETIRED. It was the Info panel's kind until that pane became a weave
// (`Zengine/info-pane/`), for 3's reason exactly. It was also the only kind that ever
// declared `placement::kSideRegion`, which is why `kinds_placed_in` now counts zero there.
inline constexpr std::int64_t kEditor = 2;
// 3 IS RETIRED. It was the project browser's kind until that pane became a weave
// (`Zengine/files/`); the number is left unused rather than reassigned, because a kind is a
// session-local handle and renumbering the two below would buy nothing and move two values
// every case and every catalog walk already agrees on.
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
/// The column at the workspace's right edge: fixed width, against that edge, and RESERVING
/// NOTHING. It was subtracted from the room whether or not anything stood in it, which made
/// it the one place a maker could not author and the one pane whose absence bought them no
/// space. It is an ordinary place now -- the room runs under it, a pane standing here covers
/// room exactly as a stacked panel does, and a maker may move a pane out of it and get the
/// thirty columns back.
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

// `place_is_authorable` IS GONE, AND THE ANSWER IT GAVE IS NOW UNCONDITIONAL. It read
// `where != placement::kSideRegion` -- one place excluded, because the screen reserved that
// column and owned what stood in it. Nothing is reserved, so EVERY place is the maker's to
// author, and a predicate that can only answer true is a question this code no longer has.
// Its four readers said the same sentence four ways (the arrangement admission, the pointer's
// hold, the pane editor's typed geometry, and `project_pane`'s layering of authored intent
// over a resolved rectangle); each of them now simply layers what the maker said. The two laws
// that named it, WL-PANE-01 and WL-PANE-08, name what replaced it instead.

/// WHOSE PANES THE BUILT-INS ARE — the provider/service key every catalog row
/// below carries, and the first half of a durable `PaneRef`.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr const char* kWorkshopProvider = "zengine.workshop";

/// WHOSE PANES THE MAKER-MADE ONES ARE -- the provider half of the durable `PaneRef` a pane
/// created inside Workshop carries (`maker_pane_ref`, setup.hpp), and a namespace this
/// application OWNS.
// WL-MAKER-03 -- agents/workshop/maker-pane.md
inline constexpr const char* kMakerPaneProvider = "zengine.workshop.maker";

/// THE ONE FOREIGN OFFICE THIS HOST SPELLS, and it is spelled for a DESK rather than for
/// furniture: `default_setup` (setup.hpp) names the Info pane so that a fresh Workshop opens
/// with one, the way every saved desk names the panes it wants. It is a literal here rather
/// than `info-pane/vocabulary.hpp`'s constant, for `pane_migration.hpp`'s reason: this host
/// does not link the weave, and a case checks the two spellings against each other.
///
/// ⚠ AND IT IS NOT A CATALOG ROW. Nothing resolves it, nothing places it, and no code path
/// asks whether a pane is this one: `add_pane` copies the reference and `resolve_pane` answers
/// from what this run was offered. A maker who deletes the row gets a Workshop with no Info in
/// it, which is what deleting a desk row should mean.
// WL-INFO-11 -- agents/workshop/info-body.md
inline constexpr const char* kInfoPaneProvider = "zengine.info";
inline constexpr const char* kInfoPaneKey = "info";

/// One entry in the catalog: what a maker sees in the picker, where the thing
/// they open will be, and WHAT TO CALL IT IN A FILE.
// WL-FOCUS-02 -- agents/workshop/focus.md; WL-SETUP-01 -- agents/workshop/setup-file.md
struct PanelKind {
    std::int64_t kind = kNoPaneKind;
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
// `info` IS RETIRED HERE AND LIVES IN `info-pane/vocabulary.hpp` NOW. It stays spelled in
// `pane_migration.hpp`, once, as a historical fact about files already written.
inline constexpr const char* kEditor = "editor";
inline constexpr const char* kLayouts = "layouts";
inline constexpr const char* kPaneEditor = "pane-editor";
} // namespace pane_key

/// THE CATALOG. Workshop's own, and complete: a panel that is not here cannot be
/// opened, because the picker is the only door and the picker walks this array.
// WL-FOCUS-02 -- agents/workshop/focus.md
// WL-PED-01 -- agents/workshop/pane-manager.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
inline constexpr PanelKind kPanelCatalog[] = {
    // THE SOURCE EDITOR'S PRESENTATION, AND ONLY ITS PRESENTATION. The document -- the
    // path, the buffer, the saved copy, the dirty answer -- is Session state
    // (`Session::editor`), which is exactly what makes removing, hiding or rearranging
    // this pane unable to lose one byte of unsaved source: a panel is a presentation,
    // and closing one destroys a presentation. The shape the Info panel had, when it was one:
    // a presentation holding a document instead of holding nothing.
    {panel::kEditor, placement::kOverlayStack, kWorkshopProvider, pane_key::kEditor, "Editor",
     "edit a source file", true},
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

// WL-CAT-01 -- agents/workshop/catalog.md
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

/// NO BUILT-IN KIND DEFAULTS TO THE RIGHT COLUMN ANY MORE, and this line is the whole of that
/// rule. Info was the one that did, and Info is a weave; `placement_of` answers
/// `kOverlayStack` for every runtime kind, so the only thing that can put a pane in the right
/// column now is a DESK saying so (`pane_unit::kRightColumn`), which is a maker's sentence
/// rather than this host's. The place itself stays: it is a rectangle a setup row may name.
///
/// ⚠ AND ZERO IS ASSERTED RATHER THAN THE LINE BEING DELETED. "At most one" would still be
/// true of zero and would go on being true if somebody added a row back; what this says is
/// that the host declares NO furniture at that edge, which is the thing the arc bought and
/// the thing a later catalog row would quietly undo.
static_assert(kinds_placed_in(placement::kSideRegion) == 0,
              "no built-in kind declares the right column: a desk row names it, and a catalog "
              "row that took it back would be this host deciding where a weave's pane goes");

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
// WL-PANE-14 -- agents/workshop/panes-and-windows.md
struct PanelPicker {
    bool open = false;
    std::size_t cursor = 0;
    double wheel_accum = 0.0; /// < fractional wheel notches not yet worth a row
};

/// WHAT A MAKER CALLS THE PICKER — the words on the hint that opens it, so that a
/// sentence about the box on the screen uses the name printed beside the key that
/// put it there. It is here rather than in the catalog because the picker has no
/// catalog row; it is the one presentation that names itself.
// WL-PANE-14 -- agents/workshop/panes-and-windows.md
inline constexpr const char* kPickerName = "+ panel";

/// WHAT PROJECT REALIZATION IS WAITING ON, RIGHT NOW — a VALUE, derived at every
/// spend and held by nobody.
// WL-ATTN-04 -- agents/workshop/attention.md
struct ProjectFrontier {
    bool waiting = false;     ///< realization is stopped at a row waiting on the maker
    std::string artifact;     ///< the frontier artifact stem; empty when not waiting
    std::size_t blocked = 0;  ///< authored rows behind the frontier, waiting on it
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
    /// THE ACTIONS THIS PANE DECLARED, as admitted -- retained here, beside the descriptor
    /// they arrived with, so the maker's keymap file can be applied to them whenever it
    /// loads. What is IN FORCE is `Keymap::panes`, derived from this; a refused
    /// declaration leaves this exactly as it was.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    std::vector<PaneActionRow> actions;
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
// WL-CAT-04 -- agents/workshop/catalog.md
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
// WL-CAT-05 -- agents/workshop/catalog.md
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

    /// WHERE THIS PANE SAID ITS CARET IS, in the BODY lattice it was granted -- and what
    /// it has selected. `surface::kNoCaret` on `caret_row` is the ordinary state: no
    /// caret was ever published, the pane said it has none, or the last one was refused.
    /// Workshop adds its own header offset when it merges these into the region it
    /// assembles; nothing here is ever a cell, a pixel or a region origin.
    // WL-CARET-01 -- agents/workshop/panes-and-windows.md
    std::int64_t caret_row = surface::kNoCaret;
    std::int64_t caret_col = 0;
    std::int64_t sel_begin_row = surface::kNoSelection;
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = surface::kNoSelection;
    std::int64_t sel_end_col = 0;

    /// THERE IS NOTHING TO REFUSE ANY MORE -- one door.
    // WL-ATTN-04 -- agents/workshop/attention.md
    void clear_refusal() {
        refusal.clear();
        refusal_why.clear();
    }

    /// THIS PANE HAS NO CARET -- the state a refusal lands in and the one a pane asks for
    /// by sending `kNoCaret`. One door, so "refused whole" is one call rather than six
    /// assignments somebody can write five of.
    // WL-CARET-02 -- agents/workshop/panes-and-windows.md
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
///
/// It carries a KIND and nothing else. Per-panel view state lives beside the
/// stack rather than inside the instance (`Panels::builder`), because the catalog
/// allows one instance of a kind: a second copy of a tool's status inside each
/// instance would be a shape that only means something once a policy about
/// several instances exists.
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
struct Panel {
    std::int64_t kind = kNoPaneKind;
};

/// THE KINDS A FRESH SESSION HAS OPEN BEFORE ANY WEAVE HAS SPOKEN -- one, now that Info is a
/// weave: a pane this host does not compile cannot be open at construction, and arrives when
/// its office offers it and the desk names it, exactly as Files, the Builder and Attention do.
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

/// Every dynamic panel this session has open, plus the picker and the per-kind
/// views. Session, never document.
struct Panels {
    std::vector<Panel> open = default_panels();
    PanelPicker picker;
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
    /// door — the rule the retired Builder panel's own per-kind view demonstrated,
    /// generalized from one kind to a population. It is the only such view left, and
    /// what it holds is rows a provider SAID rather than facts this host derived.
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

/// WHICH EXTERNAL PANE THE KEYBOARD IS POINTED AT RIGHT NOW, or `kNoPaneKind`.
/// `Panels::keyboard` is a press's MEMORY and this is the ANSWER, resolved fresh at every
/// spend rather than maintained: a pane that stops qualifying stops being the target.
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

/// Open a panel of this kind, or say why not.
///
/// Answers whether anything changed, so the caller can tell a maker the truth
/// either way rather than showing an unchanged screen with no explanation.
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
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
// WL-LAYOUT-07 -- agents/workshop/layouts.md
// WL-PANE-13 -- agents/workshop/panes-and-windows.md
inline bool close_panel(Panels& panels, std::int64_t kind) {
    for (std::size_t i = 0; i < panels.open.size(); ++i) {
        if (panels.open[i].kind == kind) {
            panels.open.erase(panels.open.begin() + static_cast<std::ptrdiff_t>(i));
            // ⭐ NO BUILT-IN HAS A PER-KIND VIEW TO FORGET ANY MORE, and the branch
            // that forgot the last one is gone with it. There used to be exactly one
            // (`panels.builder`, the Builder panel's copy of the tool's status), written
            // as one `if` rather than a virtual `forget()` on a panel base class -- and
            // the pane that needed it is a weave now, which holds its copy in its own
            // image and drops it when Workshop stops granting it a room.
            //
            // INFO HAD NOTHING TO FORGET EITHER, and that was never an omission: it holds
            // no copy of anything, because what it presents is the document and the
            // session, and both of those outlive it and belong to somebody else.
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
