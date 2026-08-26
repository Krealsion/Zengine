// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANEL_HPP
#define ZENGINE_WORKSHOP_PANEL_HPP

// Workshop's dynamic panels: the catalog of what a maker may open, what is
// currently open, and each open panel's own view of the thing it presents.
//
// A WEAVE MAY PROVIDE A TOOL; A PANEL IS ITS PRESENTATION. That sentence is the
// whole of this file's job, and it is kept true structurally rather than by
// convention:
//
//   the TOOL   is a weave on the bus, mounted by the host, with its own identity
//              and its own grant. It runs whether or not anything is presenting
//              it, and it has never heard of a panel.
//   the PANEL  is a row of this application's furniture. It holds a COPY of what
//              the tool last said, and closing it destroys the copy and nothing
//              else.
//
// So `panel == weave` is not a rule here, and nothing below makes it one: a
// panel kind is an entry in an array of Workshop's own, a tool is a role on the
// bus, and the only thing joining them is that one panel kind happens to know
// which office to ask.
//
// THAT CLAIM HAS NOW BEEN PAID FOR RATHER THAN ASSERTED. BLD-0 predicted that a
// panel over something with no weave behind it — "the document, the object
// list" — would fit this catalog without changing it. `Info` is that panel: it
// presents the OBJECTS and PROPERTIES columns, which are read straight off the
// document and the session, and it reaches no bus at all. What it cost the
// catalog was one row; what it cost this file is one integer and one line in an
// array. There is no `InfoPane` beneath, because Info holds nothing of its own —
// it is the second kind that proves `Panels::builder` is one kind's view rather
// than the first slot of a framework.
//
// THE SESSION IS WHERE THIS LIVES, EMPHATICALLY. Which panels a maker has open
// is not authored content: it does not go in `WorkshopDoc`, it is not saved, and
// it does not survive the process. Panel persistence is a thing a later phase
// can decide to want; making it accidental by putting a vector in the document
// is how it would arrive without anybody deciding.
//
// WHAT IS DELIBERATELY ABSENT, so the absences are decisions and not omissions:
//
//   - no docking and no tabs. A panel kind DECLARES one of the two places this
//     Workshop has (`placement` below) and that is the whole of its say in where
//     it goes. Neither place was chosen for being good — one covers the material
//     a maker is building and the other is a column that was furniture until
//     PNL-0 — and the reports say what using them felt like.
//
//     WIND-2 ENDED THREE OF THIS BULLET'S ABSENCES and left the rest standing, so
//     they are named rather than quietly deleted: dragging, resizing and a saved
//     layout now exist. What did NOT change is the sentence above them — a KIND
//     still declares a place and nothing more. What a maker may then do to the
//     rectangle that place resolves to is authored SETUP intent (setup.hpp) and
//     is resolved by the host (screen.hpp); no field of this file moved, and a
//     panel kind still cannot ask for a coordinate.
//   - no focus framework. There is still no focused panel and no capture: one
//     `if` per mode in the key routing, and a pointer gesture that holds an
//     IDENTITY rather than owning the device. WIND-2 added a SELECTED pane and a
//     canonical front order, which are different things — a selection is a fact
//     about what a maker is arranging and grants no priority outside the mode
//     that arranges it, and the order is authored intent on a setup row rather
//     than a z-stack somebody maintains.
//     What there is: while the picker is open it has the keys, and while a
//     Builder panel is open one key that was previously unbound means something.
//     That is one conditional in `command()`, and it is the same answer the
//     terminal overlay gave to the same question. Info adds no key of its own —
//     the inspector gestures it presents are the ones Workshop already had, and
//     what changed is that they now say so when nothing is showing them.
//   - no multi-instance policy. A kind is either open or it is not; the picker
//     asks for a KIND and there is nowhere for a second instance of one to be
//     named. One live Builder and one live Info are what exists, and a policy
//     about several would be a policy invented ahead of the case that wants it.
//   - no per-panel keybindings, no registry, no plugin surface. The catalog is
//     an array of two integers and two strings, spelled out below.
//
// PRESENCE HAS ONE OWNER, AND IT IS THE PICKER (PNL-0). With one kind, `x`
// could mean "close the Builder" and be unambiguous; with two it would have to
// pick one, and picking would either be a per-panel hotkey or a focus rule —
// both of which this file has declined. So the door that opens a panel is the
// door that removes it:
//
//     closed panel  ->  select  ->  open
//     open panel    ->  select  ->  remove
//
// `x` is unbound again, exactly as it was before BLD-0 bound it. The picker
// SAYS which kinds are open, because a toggle whose current state is invisible
// is a gesture a maker has to guess at.

#include "builder/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// The KINDS of panel this Workshop can present. Two so far, and they are
/// deliberately unalike: one presents a weave, one presents this application's
/// own state, and nothing in this file can tell them apart.
///
/// A plain integer rather than an enum class, for the reason every other
/// vocabulary constant in this repository is one: these values sit beside
/// canvas roles and input scancodes in code that is read together, and a
/// second numeric convention buys nothing.
namespace panel {
inline constexpr std::int64_t kBuilder = 0;
inline constexpr std::int64_t kInfo = 1;
} // namespace panel

/// WHERE a panel kind is presented. Two, because Workshop has two places and no
/// more, and each is a NAME for a place this screen already had rather than a
/// coordinate somebody chose.
///
/// THIS IS PLACEMENT INTENT, AND THE BOUNDS ARE SOMEWHERE ELSE. A place says
/// which of Workshop's two regions a kind occupies; WHAT RECTANGLE that is
/// depends on the screen's extent and on how many other panels are stacked, and
/// is worked out against a `Screen` by `placement_bounds` in screen.hpp. That
/// is the same authored/resolved split the document itself is under (W-1): the
/// intent is a small constant that can be written down and read, the rectangle
/// is an observation that is recomputed on demand and cached nowhere.
///
/// A place is NOT a docking side, an anchor, a constraint or a layout, and there
/// is no policy here that could put a third kind somewhere neither of these two
/// is. What a third kind gets is the ability to SAY which of the two it wants,
/// instead of a painter quietly knowing a column number.
namespace placement {
/// The reserved column beside the workspace: fixed width, against the right
/// edge, and reserved whether or not anything is in it (screen.hpp says why it
/// stays empty when Info is removed). It has room for ONE panel, and the
/// static_assert under the catalog is what keeps that true rather than hoped.
inline constexpr std::int64_t kSideRegion = 0;
/// Over the workspace, from the canvas's top-left, stacked downwards — the
/// terminal overlay's mechanism pointed at the other corner. It covers the
/// material a maker is building, which is BLD-0's awkwardness and still the
/// evidence a layout phase should be built on.
inline constexpr std::int64_t kOverlayStack = 1;
} // namespace placement

/// WHOSE PANES THE BUILT-INS ARE — the provider/service key every catalog row
/// below carries, and the first half of a durable `PaneRef` (setup.hpp).
///
/// IT IS A ROUTE AND NOT A CREDENTIAL, and the distinction is worth stating here
/// because the string is about to be written into a maker's file where a later
/// reader will meet it with no context. It says WHICH NAMESPACE a pane key is
/// to be read in. It does not say which package author created the pane, which
/// binary is running, that the same author came back after a restart, that a
/// live Loom office answers to this name, or that anything claiming this string
/// is authentic. Nothing in WS-0 checks any of those, because nothing in WS-0
/// can: provenance is an OFFICE's to stamp on a delivery, not a file's to
/// assert about itself.
inline constexpr const char* kWorkshopProvider = "zengine.workshop";

/// One entry in the catalog: what a maker sees in the picker, where the thing
/// they open will be, and WHAT TO CALL IT IN A FILE.
///
/// THE DURABLE REFERENCE IS A FIELD OF THE CATALOG ROW (WS-0), beside the
/// internal kind rather than in a table next to it. That is the whole of how
/// `PaneRef <-> PanelKind` is kept from drifting: there is one array, so there
/// is nothing for a second array to disagree with. A third kind declares its
/// provider and its pane key in the same braces it declares its place in, and a
/// row that forgot to is caught by the assertions under the catalog rather than
/// by a maker whose saved setup came back empty.
///
/// `provider`/`pane` are DURABLE and `kind` is not: the integer is this build's
/// index into its own vocabulary and may be renumbered by an edit to the two
/// lines above, while the two strings are a promise to a file somebody owns.
/// That is the same split `persist.hpp` makes about an extent mode, made about
/// an identity instead of about a word.
struct PanelKind {
    std::int64_t kind = panel::kBuilder;
    std::int64_t placed_in = placement::kOverlayStack; ///< which of the two places it is in
    const char* provider = kWorkshopProvider; ///< the durable provider/service key
    const char* pane = "";    ///< the durable pane key, in that provider's namespace
    const char* name = "";    ///< what the picker lists
    const char* summary = ""; ///< one line, so a maker can tell what they are opening
};

/// The pane keys the built-ins are spelled with in a saved setup. Named
/// constants rather than literals in the catalog, because the suite and the
/// file format both have to say them and a typo in one of three copies is a
/// setup that loads as unresolved.
namespace pane_key {
inline constexpr const char* kBuilder = "builder";
inline constexpr const char* kInfo = "info";
} // namespace pane_key

/// THE CATALOG. Workshop's own, and complete: a panel that is not here cannot be
/// opened, because the picker is the only door and the picker walks this array.
///
/// It is a constant rather than a registry, and that is the honest shape of what
/// exists: a registry would be a mechanism for parties unknown to add entries,
/// and there are no such parties. When there are, this becomes the thing they
/// add to, and the picker below does not change.
///
/// A KIND'S PLACE IS ONE OF THE FOUR THINGS WRITTEN DOWN HERE (PNL-1), beside
/// its name and its one-line summary, because that is what it is: a fact about
/// the kind, known before anything is open, changed only by editing this array.
/// It is not per-instance state and not authored by a maker. A maker CAN move a
/// panel since WIND-2 — but what they move is not this: this is the DEVELOPER'S
/// DEFAULT, the answer a pane takes when its maker has said nothing, and the
/// maker's override lives on their own setup row and is laid over the rectangle
/// this place resolves to. A coordinate stored per open panel would still be a
/// field with one possible value, and it would now also be a second owner of a
/// question the setup already answers.
inline constexpr PanelKind kPanelCatalog[] = {
    {panel::kBuilder, placement::kOverlayStack, kWorkshopProvider, pane_key::kBuilder, "Builder",
     "build one known target"},
    {panel::kInfo, placement::kSideRegion, kWorkshopProvider, pane_key::kInfo, "Info",
     "objects and properties"},
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

/// WHERE THE SESSION-LOCAL KINDS BEGIN (WP-0), and the whole of how a runtime
/// pane is told from a built-in one.
///
/// A RUNTIME KIND IS A HANDLE AND NOT AN IDENTITY. The durable identity of an
/// external pane is its `PaneRef` -- the office Loom stamped, plus the pane key
/// that office offered -- and that is what a setup file holds. This integer is
/// what `Panels::open`, `bounds_of` and `occupied_at` carry in the same field
/// the two built-ins carry `panel::kBuilder` and `panel::kInfo` in, so the whole
/// presentation path needs no second vocabulary. It is minted by this session,
/// spent by this session, and never written to a file, never read off a message
/// and never compared across processes.
///
/// THE GAP IS DELIBERATE AND SO IS ITS SIZE. The built-ins are 0 and 1 and this
/// build's own vocabulary may grow; starting a thousand above leaves no arithmetic
/// by which a runtime handle and a future built-in could collide, and the
/// predicate below is the one place either question is asked.
inline constexpr std::int64_t kFirstRuntimeKind = 1024;

/// Is this a session-local runtime kind rather than a compile-time one?
inline constexpr bool is_runtime_kind(std::int64_t kind) noexcept {
    return kind >= kFirstRuntimeKind;
}

/// NO KIND AT ALL -- what a pane this build cannot present answers with (WIND-2).
///
/// NEGATIVE, for `role::kNone`'s and `kNoCaret`'s reason, which is the sharpest
/// one available here: every kind this build can name is non-negative by
/// construction (the built-ins are 0 and 1, runtime handles start a thousand
/// above), so an absence CANNOT collide with a kind anybody meant. Zero would
/// have been `panel::kBuilder`, which is the exact lie `resolve_pane` is fallible
/// to prevent -- a maker's unresolved third-party reference presented as
/// Workshop's own build tool.
///
/// It is spent by the one inventory row an unresolved authored reference gets
/// (`inventory_rows`, setup.hpp). Nothing paints it, nothing places it, nothing
/// opens it.
inline constexpr std::int64_t kNoPaneKind = -1;

/// WHERE THIS KIND IS PRESENTED — the question a painter asks instead of knowing
/// a column. Total, for the same reason `panel_kind` is.
///
/// AN EXTERNAL PANE IS PLACED BY WORKSHOP AND ASKS FOR NOTHING (WP-0), and this
/// branch is where that is decided rather than negotiated: `PaneOffered` carries
/// no placement field, so every runtime kind goes in the overlay stack. The
/// branch is here rather than left to `panel_kind`'s fall-through on purpose --
/// that fall-through answers with `kPanelCatalog[0]`, which is the BUILDER, and
/// a runtime pane silently inheriting the Builder's row is exactly the lie
/// `resolve_pane` was made fallible to prevent one layer up. `panel_kind` stays
/// total for its own bounded built-in callers and never sees a runtime kind.
inline constexpr std::int64_t placement_of(std::int64_t kind) noexcept {
    if (is_runtime_kind(kind)) {
        return placement::kOverlayStack;
    }
    return panel_kind(kind).placed_in;
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

/// THE SIDE REGION HOLDS EXACTLY ONE PANEL, and this line is the whole of that
/// rule. It is here rather than in a comment because the failure it prevents is
/// silent: two kinds declaring `kSideRegion` resolve to the SAME rectangle, so
/// they would paint over each other in a column a maker reads as one thing, and
/// nothing at runtime would say which one they were looking at.
///
/// A third kind is meant to be cheap — a catalog row and a painter — and this is
/// the one place where it is not free: a third kind that wants the side region
/// needs the layout question answered first (how the column is shared), and it
/// finds that out from a compiler rather than from a screen. The overlay stack
/// carries no such assertion, because stacking is what it is FOR; what it has
/// instead is a measured limit on how many slots fit, in screen.hpp.
static_assert(kinds_placed_in(placement::kSideRegion) == 1,
              "the side region has room for one panel: a second kind placed there would "
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
///
/// Only ever asked at compile time, by the two assertions under them, and they
/// are here for the reason every other `static_assert` in this file is: THE
/// FAILURE THEY PREVENT IS SILENT. A row with an empty pane key resolves from
/// no file and is saved into a setup as an empty string; two rows sharing a
/// reference make one of them unreachable through a saved setup and the other
/// one arbitrary. Neither says anything at runtime — a maker's setup simply
/// comes back missing a panel — and both are one editing mistake away from a
/// third author who is otherwise only asked to fill in six braces.
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
///
/// It is a MODE and not a panel. It has no instance, nothing presents it, and it
/// closes the moment it has been used — so it is not in the catalog and cannot
/// be opened from itself.
struct PanelPicker {
    bool open = false;
    std::size_t cursor = 0;
};

/// WHAT A MAKER CALLS THE PICKER — the words on the hint that opens it, so that a
/// sentence about the box on the screen uses the name printed beside the key that
/// put it there. It is here rather than in the catalog because the picker has no
/// catalog row; it is the one presentation that names itself.
inline constexpr const char* kPickerName = "+ panel";

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
/// ASYNC-1 MADE IT WORTH MORE, NOT LESS, and widened exactly one thing about it.
/// A build now has a MIDDLE: `asked` and `running` are both conditions a status
/// can arrive in and neither is an ending, so this fact is held across every one
/// of them and released only at a condition the build will not leave —
/// `builder::still_going` is the one place that list is written down. Without
/// the widening, the first intermediate status would clear it and the real
/// ending would arrive as something this panel merely learned. And the case it
/// now covers is the one BLD-0 could never reach at all: a panel OPENED while a
/// child is alive is told `running`, shows it, and announces nothing — because it
/// did not watch this build begin.
///
/// Nothing is authored from this struct and nothing is asked through it. Opening
/// the panel sends `builder::StatusRequested` and everything here arrives as the
/// tool's own published answer.
/// ---- ...AND SINCE BLD-1 IT ALSO HOLDS A CATALOG AND A CHOICE ------------------
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
struct BuilderPane {
    bool heard = false;
    bool awaiting = false;
    bool awaiting_realization = false;
    builder::BuildStatus shown{};
    builder::RecipeCatalog known{};
    std::size_t chosen = 0;
};

/// ONE ROW OF THE SESSION-LOCAL RUNTIME CATALOG (WP-0): a pane some office
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

    /// THE LOOKUP TAKES VIEWS (WP-0a), so that asking whether a pane was already
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
///
/// The Builder pane's shape, one provider further out, and the two facts that
/// are genuinely this panel's own are the same two: `heard` distinguishes "the
/// provider says nothing" from "the provider has not answered yet", and
/// `awaiting` records that a room was granted and no valid content has arrived
/// since. A pane whose provider has gone quiet reads WAITING; it never reads
/// unavailable, because Loom gives Workshop no participant-visible unload
/// notification and silence is not evidence of one.
///
/// `rows`/`columns` are the LAST ROOM GRANTED, kept so a re-grant can be told
/// from a repeat: a screen that changed cells but not prose capacity sends
/// nothing, and a metric change that moved the capacity sends exactly once.
///
/// `shown` NEVER EXCEEDS THE ROOM IT WAS ADMITTED UNDER. Every row in it passed
/// the row count, the column width and the plain-ASCII contract at the moment it
/// arrived, and a new grant clears it before the new room is sent -- so a shrink
/// cannot leave yesterday's wider rows sitting in a narrower budget.
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
    std::vector<surface::SurfaceTextRow> shown;
};

/// One panel a maker has opened.
///
/// It carries a KIND and nothing else. Per-panel view state lives beside the
/// stack rather than inside the instance (`Panels::builder`), because BLD-0
/// allows one instance of a kind: a second copy of a tool's status inside each
/// instance would be a shape that only means something once a policy about
/// several instances exists.
struct Panel {
    std::int64_t kind = panel::kBuilder;
};

/// WHAT A FRESH SESSION HAS OPEN. Info, and nothing else.
///
/// It is a function with a name rather than a brace-initialiser on the member
/// below, because "Workshop boots with the properties showing" is a DECISION and
/// a decision should be somewhere a reader can find it and change it. Before
/// PNL-0 it was not a decision at all — the OBJECTS and PROPERTIES columns were
/// structural furniture that `paint` drew unconditionally, and the only way to
/// not have them was to edit `paint`.
///
/// It is also why every existing Workshop case still measures the screen it
/// always measured: a default-constructed `Session` has Info open, so the
/// migration changed where those two columns are painted from and not whether
/// they are painted.
///
/// WS-0 GAVE THE SAME DECISION A SECOND READER, and this array is what stops
/// the two from drifting. A fresh Workshop now has an authored SETUP as well as
/// an open panel list, and "a fresh Workshop shows Info" is one sentence that
/// both of them have to say: `default_panels()` below turns this array into
/// open presentations, and `default_setup()` (setup.hpp) turns the SAME array
/// into the authored references a fresh setup carries. Neither is free to say
/// something else, because neither of them holds the answer -- this line does.
inline constexpr std::int64_t kDefaultPanels[] = {panel::kInfo};

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
    /// THE PANES OFFERED TO THIS RUN, beside the compile-time ones (WP-0). It
    /// lives here rather than in `Session` for one measured reason: every
    /// presentation question that has to know a runtime pane's NAME or its PLACE
    /// -- the picker's rows, `occupied_at`'s answer, `bounds_of`'s slot -- is
    /// already handed a `Panels`, so putting the catalog anywhere else would have
    /// added a parameter to each of them and given a caller a chance to forget it.
    RuntimeCatalog runtime;
    /// The per-pane view of each OPEN external panel: its granted room, its copy
    /// of what the provider last said, and whether it is waiting. One entry per
    /// open external kind, created by the open door and destroyed by the close
    /// door — the `BuilderPane` rule, for a population rather than for one kind.
    std::vector<ExternalPane> external;
    /// AUTHORED INTENT THIS SCREEN HAS NO ROOM FOR, as resolved kinds, in setup
    /// order. `open`'s twin and derived by the same one path: `reconcile` is the
    /// only writer, and it runs on every change to the setup and on every change
    /// of extent, so this cannot describe a screen that has since moved.
    ///
    /// WAITING IS NOT UNRESOLVED AND NOT CLOSED. The reference resolves, this
    /// build knows exactly what it would present, and the current composition
    /// has nowhere to put it. Saying `closed` would invite a maker to press the
    /// picker again; saying `unresolved` would blame the provider for the screen.
    std::vector<std::int64_t> waiting_for_room;
    /// WHICH EXTERNAL PANE A MAKER LAST PRESSED INTO -- the keyboard's CANDIDATE,
    /// and emphatically not its answer (MSG-0).
    ///
    /// IT IS RESOLVED AT EVERY SPEND AND REMEMBERED BY NOBODY, which is
    /// `bounds_of`'s discipline applied to a focus: `keyboard_pane` (weave.hpp)
    /// asks whether this kind is still an open external pane holding a granted
    /// room, and answers `kNoPaneKind` when it is not. So a pane that is closed,
    /// removed from the setup, left unresolved by a provider that went away, or
    /// pushed off the screen stops receiving keys with nothing to clear and no
    /// notification owed to anybody -- and if the same kind comes back, so does
    /// the keyboard, because the candidate was never a lie in the first place.
    ///
    /// THE ONLY WRITER IS A PRESS. `on(PointerButton)` sets it from the occupancy
    /// walk it already performs, before any layer answers the press, so there is
    /// one decision rather than one per routing arm; a press Workshop resolves
    /// that lands anywhere else -- another panel, the workspace, nothing at all --
    /// clears it by the same line. Modes that own the pointer whole (the Terminal,
    /// pane management) never reach that line, which is why closing one hands the
    /// keyboard back exactly where it was.
    ///
    /// SESSION, and not even that: it is not in the setup, not in the document,
    /// not persisted and not restored. `kNoPaneKind` is where every session starts.
    std::int64_t keyboard = kNoPaneKind;

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

/// WHICH EXTERNAL PANE THE KEYBOARD IS POINTED AT RIGHT NOW, or `kNoPaneKind` (MSG-0).
///
/// `Panels::keyboard` is a press's MEMORY and this is the ANSWER, resolved fresh at
/// every spend rather than maintained: a pane that is closed, removed from the setup,
/// left unresolved by a provider that went away, or granted no room stops being the
/// target with nothing to clear and no notification owed to anybody. The three
/// conditions are exactly what `external_press` already requires before it will send
/// a press, so a pane that can be pressed is a pane that can be typed into and there
/// is no fourth state between them.
///
/// IT IS HERE, AND NOT IN THE WEAVE, BECAUSE TWO PARTIES ASK IT. The router asks in
/// order to route a key; the PAINTER asks in order to say so -- the pane's header
/// marks it and the bottom band names it. A second copy of this resolution would be a
/// screen that says a maker is typing into one pane while the keys go to another,
/// which is the worst shape this defect could take.
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
    // AND AN EXTERNAL PANE GETS ITS VIEW BY THE SAME ACT (WP-0), for the reason
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
///
/// THE TOOL IS UNTOUCHED — nothing here reaches the bus, and the weave whose
/// facts this panel was showing goes on being asked, answered and counted by
/// whoever else is talking to it.
///
/// THE PANEL'S COPY IS DESTROYED WITH THE PANEL, and that is the deliberate
/// half. Keeping it would make a reopened panel look instantly informed while
/// showing something that might be minutes stale, and it would hide the property
/// this whole split exists to keep: a reopened panel asks the tool and the TOOL
/// answers with its own running total, so `builds` comes back as 3 rather than
/// as 1. A panel that owned the state could not produce that number, which is
/// what makes it evidence rather than decoration.
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
            // AND AN EXTERNAL PANE FORGETS EVERYTHING IT WAS SHOWING (WP-0):
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
