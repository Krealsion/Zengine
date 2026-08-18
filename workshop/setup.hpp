// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_HPP
#define ZENGINE_WORKSHOP_SETUP_HPP

// WHAT A MAKER CALLS THE ARRANGEMENT THEY ARE WORKING IN, and the whole of what
// that is allowed to mean in WS-0.
//
// A SETUP IS A NAME AND AN ORDERED LIST OF AUTHORED PANE ROWS. Each row is a
// durable reference plus the SMALLEST DIFFERENCE FROM THE DEVELOPER'S DEFAULT a
// maker has asked for -- where it sits, how wide, how tall, and how far forward
// (WIND-2). It is the third kind of fact this application has, and the only
// interesting thing about it is where its edges are:
//
//   DOCUMENT   the objects a maker authored (vocabulary.hpp, persist.hpp). A
//              setup neither contains nor touches one, and saving or restoring
//              a setup changes no document byte.
//   SETUP      the human name, which panes the maker meant to have open, and the
//              window intent they authored for each. Authored, persisted, and
//              this file.
//   SESSION    the picker's cursor, a Builder panel's copy of a tool's status, a
//              Terminal's draft, a drag, a notice. Belongs to the run.
//   DERIVED    every rectangle a pane resolves to on the current screen. Never
//              written down anywhere, here least of all.
//
// ---- Why a reference is two strings ---------------------------------------
//
// `Panels::open` holds `panel::kBuilder` and `panel::kInfo`, which are indices
// into THIS BUILD'S OWN vocabulary. A file cannot hold one, for two reasons and
// the second is the load-bearing one:
//
//   an ordinal is not durable    renumber the two constants in panel.hpp and
//                                every saved setup silently opens the other
//                                panel. The same argument persist.hpp makes for
//                                writing an extent mode as the WORD `cells`.
//   an ordinal cannot be ABSENT  there is no integer meaning "a pane this build
//                                has never heard of". A setup that could not say
//                                that would have to drop such an entry on load,
//                                which is a saved file quietly editing itself.
//
// So a durable reference is a PROVIDER/SERVICE KEY and a PANE KEY, both text,
// both readable by a person looking at their own file:
//
//     zengine.workshop / info
//     zengine.workshop / builder
//
// The built-ins carry the same shape a later external provider will carry
// (WP-R0), which is the whole reason it is two strings today rather than one:
// EXTERNAL-SAFE DOES NOT MEAN EXTERNAL-NOW, and the cost of being external-safe
// on day one is that today's file needs no migration on the day it stops being
// the only kind.
//
// WHAT `provider` IS NOT is stated on `kWorkshopProvider` in panel.hpp and is
// worth reaching from here too: it is a ROUTE, not an author identity and not
// stamped provenance. Nothing in this file authenticates anything, and nothing
// in this file may come to.
//
// ---- Resolution is FALLIBLE, and internal lookup stays total ---------------
//
// `panel_kind(unknown)` answers with the Builder, and that is correct for its
// callers: they hand it a kind derived from bounded local state (a picker
// cursor, an open panel), so a total function is cheaper than an invariant
// somebody has to maintain. It is exactly wrong for a FILE, where the kind
// arrives as two strings a stranger wrote, and where answering "Builder" to an
// unknown reference would paint a maker's third-party pane as Workshop's build
// tool.
//
// So the totality stays where it was and `resolve_pane` is a SECOND, narrower
// door: it answers with nothing rather than with a fallback, and no placement,
// occupancy, picker or painter path can be reached through it without the
// caller having said what to do about the nothing.
//
// WP-0 WIDENED WHAT THAT DOOR CONSULTS AND NOT WHAT IT PROMISES. It now asks the
// compile-time catalog AND this session's runtime catalog -- the panes some
// office actually offered this run -- and the runtime half is a REQUIRED
// argument, never a default and never a second overload. The reason is the one
// HD-4 wrote down about `first_visible`: a spelling a caller can keep is a
// spelling that stays silently right until the first case that needed the new
// argument, and here that case is a maker's open external pane counted as
// unresolved on the line beneath it. `resolve_builtin_pane` is the narrow
// question under its own name, so the two cannot be reached by accident.
//
// ---- What an external pane added, and what it did NOT ----------------------
//
// A runtime pane is admitted from a LIVE OFFER under the office Loom stamped on
// it (`admit_pane_offer`), held in session state that no file and no document
// sees, and bounded in every field before a byte of it is retained. What did NOT
// change is the persisted grammar: `PaneRef` is the same two strings,
// `check_pane_key` is the same law, `kMaxSetupPanes` is the same number, and
// setup_persist.hpp was not edited. An external reference saves, loads and saves
// again byte-identically whether or not anybody is currently offering it.
//
// ---- What a setup deliberately does not have ------------------------------
//
// No RESOLVED rectangle, no screen extent, no text metric, no pane-instance
// identity, no opaque provider configuration, no focus, no tabs, no docking, no
// layout tree, no plugin manifest, no schema field of its own beyond the file's
// (setup_persist.hpp). Every one of those is either DERIVED from the current
// screen -- and would go stale the moment the window changed -- or a decision
// the first provider that actually needs it should get to make.
//
// WIND-2 ADDED AUTHORED WINDOW INTENT AND NOT RESOLVED GEOMETRY, and the line
// between them is the whole of the phase. What a row carries is what a MAKER
// SAID: a place in canvas cells, a width and a height each in their own unit, and
// a canonical rank saying how far forward this pane sits. What no row carries,
// and what `check_setup` still cannot see, is a `Screen`: a setup legal on a tall
// screen is legal on a short one, a setup legal on a terminal is legal on a
// window, and the rectangle any of it resolves to is computed on every paint by
// screen.hpp and stored nowhere.
//
// SPARSE, SO A DEFAULT STAYS A DEFAULT. Every one of the three geometry fields
// carries a MODE, and the mode `default` means "no override -- the developer's
// answer, whatever it becomes in a later build". A full snapshot would silently
// convert every developer default into a maker decision at the moment of first
// save, which is the same defect W-5 removed from the document's saved flag.

#include "document.hpp" // `doc::kMaxCells` -- the bound an authored cell count already has
#include "pane_vocabulary.hpp"
#include "panel.hpp"
#include "property.hpp"

#include "component/text_box.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The name a fresh Workshop's setup carries. It is a real name and not an
/// empty one, because a setup ALWAYS has a name -- "unnamed" would be a second
/// state for every reader of a name to handle, bought with nothing.
inline constexpr const char* kDefaultSetupName = "Default";

/// The setup file's suggested name. Beside the document's own
/// (`persist::kDefaultDocumentName`), and deliberately a different file: the two
/// artifacts answer different questions and a maker may want one without the
/// other.
inline constexpr const char* kDefaultSetupFileName = "workshop-setup.json";

// ---- The bounds, and why each one is the number it is -----------------------
//
// EVERY ONE OF THESE IS AN INPUT BOUNDARY AND NOT A CAPACITY. They exist because
// a setup can arrive from a file somebody else wrote, and material from a file
// must be refused BEFORE it is copied into the live setup -- the Loom's decoder
// already refuses a payload that would materialise more than its own budget, and
// these say what this application will additionally accept as a setup. None of
// them is a statement about how many panes Workshop can usefully show.

/// How long a setup's human name may be.
///
/// THIRTY-TWO, and it is the same number `doc::kMaxNameLen` uses for an object's
/// label, measured the same way (bytes), because it is the same kind of fact: a
/// name a maker types and then reads back on one line of a screen. The screen is
/// the reason it is not larger -- the setup line at the minimum composition
/// carries the name, the file it is stored in and its saved marker, and a
/// thirty-two-byte name leaves that line fitting whole at 78 cells with the
/// default file name (measured, screen.hpp's `setup_line`).
inline constexpr std::size_t kMaxSetupNameLen = 32;

/// How long either half of a `PaneRef` may be.
///
/// A key is a ROUTING NAME and not prose: sixty-four bytes holds a
/// reverse-domain provider several levels deep and any pane word a provider
/// would choose, and it bounds what one line of a forged file can push into the
/// active setup. It is deliberately far above anything the catalog uses --
/// `zengine.workshop` is sixteen -- because a setup must be able to retain a
/// reference to a pane THIS BUILD HAS NEVER HEARD OF, and a bound cut to today's
/// catalog would make tomorrow's reference unrepresentable.
inline constexpr std::size_t kMaxPaneKeyLen = 64;

/// How many pane references one setup may carry.
///
/// NOT `kPanelKinds`, emphatically. The catalog's population is a fact about
/// what THIS build can present; the file's limit is a fact about what a setup
/// may REMEMBER, and those are different numbers precisely because an
/// unresolved reference is a legal thing to keep. A bound cut to the catalog
/// would make today's population into tomorrow's file law and would refuse the
/// one case this whole design exists to allow.
///
/// Thirty-two is chosen against the other end: Workshop's honest simultaneous
/// capacity is one to two panes (the side region holds one; an overlay slot past
/// the first does not fit the minimum screen), and the picker's own ceiling is
/// eight kinds. Thirty-two is four times the largest catalog this composition
/// could present and still bounds a file at a few kilobytes.
inline constexpr std::size_t kMaxSetupPanes = 32;

/// How long a RUNTIME pane descriptor's two prose fields may be (WP-0).
///
/// THESE BOUND A LIVE MESSAGE, NOT A FILE, and that is why they are here beside
/// the file's bounds rather than folded into them: a descriptor never reaches the
/// setup, so no saved byte depends on either number, and a later phase may move
/// one without touching the version-1 grammar at all.
///
/// THIRTY-TWO FOR A NAME, for `kMaxSetupNameLen`'s reason measured against a
/// different line: the picker pads a name into a ten-column field and then writes
/// a state column and a summary after it, so a name is a thing a maker reads
/// across one row of the NARROWEST overlay this composition lays out -- 48 cells,
/// at the 78x22 minimum, which is where the bound has to hold (a wider surface
/// gives the slot more, WIND-1, and a bound measured there would be too loose to
/// mean anything). Sixty-four for a summary, the same
/// `kMaxPaneKeyLen` order of magnitude, because a summary is one sentence and the
/// picker fits what it can (`detail::fit` marks the rest). Both are BYTE counts,
/// spent against `std::string::size()`, and their refusals say so (WS-0a).
inline constexpr std::size_t kMaxPaneNameLen = 32;
inline constexpr std::size_t kMaxPaneSummaryLen = 64;

/// THE LARGEST DEVICE-PIXEL AMOUNT A PANE MAY BE AUTHORED AT (WIND-2).
///
/// SIXTY-FIVE THOUSAND FIVE HUNDRED AND THIRTY-SIX, and the three things it is
/// NOT are what make it the right kind of number:
///
///   it is not `doc::kMaxCells * kCanvasCellPx`   a file's validity must not
///                                                depend on a medium's cell
///                                                scale, which is the same
///                                                sentence as "`check_setup`
///                                                never sees a `Screen`". The
///                                                SDL Skin's twelve is one
///                                                Skin's layout number and this
///                                                file may not hold it.
///   it is not a capacity                         no medium is promised to be
///                                                able to show this; the CELL
///                                                bound does the real work at
///                                                projection, and this only has
///                                                to keep the integer safe and
///                                                the file bounded.
///   it is not a screen size                      it is far above one on
///                                                purpose -- eight times the
///                                                row count of an 8K panel --
///                                                so it never becomes the thing
///                                                that decides what a maker may
///                                                author on hardware nobody has
///                                                built yet.
///
/// A power of two, so the one arithmetic a projection would ever do to it (an
/// integer division by a per-axis cell scale) stays nowhere near the end of the
/// number line.
inline constexpr std::int64_t kMaxPanePixels = 65536;

// ---- The value ---------------------------------------------------------------

/// WHICH PANE A MAKER MEANT -- durably, and without naming a catalog slot.
///
/// Two strings, and the pair is the identity: `provider` says whose namespace to
/// read `pane` in. Neither half means anything alone, which is why they are one
/// struct rather than two fields on a setup entry.
///
/// EQUALITY IS BYTE EQUALITY of both halves. There is no normalisation, no case
/// folding and no aliasing here on purpose: a reference this build cannot
/// resolve must come back out of the file exactly as it went in, and a
/// comparison that tidied would be the first step towards a save that rewrote a
/// stranger's entry.
struct PaneRef {
    std::string provider;
    std::string pane;

    friend bool operator==(const PaneRef&, const PaneRef&) = default;
};

// ---- THE AUTHORED WINDOW: the smallest difference from a default (WIND-2) -----
//
// A UNIT IS A MODE BESIDE AN AMOUNT, NEVER A FIELD NAME. `ui::Extent` already
// makes the argument for this shape in its own words -- "It is one property, not
// two. A maker does not author a type and then author a value; they author a
// width" -- and names the historical builder's separate `Width Type` / `Width
// Value` inspector rows as the mistake it exists to refuse.
//
// AND IT IS WORKSHOP'S OWN TYPE, NOT `ui::Extent`. Two measured reasons, either
// of which is sufficient. `kExtentPercent` means *a share of the context frame*,
// and a pane has no context element to take a share of. And `resolve_extent`'s
// first line is `if (e.mode != kExtentPercent) return e.amount;`, so a third mode
// added to `ui::Extent` would resolve AS CELLS in every existing caller --
// silently, in the widening direction, in a shared package whose values also
// arrive off the document's wire.

/// THE UNITS A PANE'S AUTHORED WINDOW MAY BE SAID IN, and there is not a fourth.
///
/// A CLOSED SET, and `default` is a VALUE rather than an absent field. Loom's
/// admission has no optional and refuses an unknown field, so absence cannot be
/// spelled by omitting one; and a magic coordinate is a value a maker could
/// otherwise mean. The precedent is doubled in this repository already --
/// `role::kNone` and `kNoCaret` are both "the absence of one, deliberately not a
/// further member of the set".
///
/// `kDefault == 0` SO A DEFAULT-CONSTRUCTED ROW MEANS "NO OVERRIDE", which is
/// what makes a fresh setup's bytes the smallest spelling of a setup that has
/// never been arranged.
namespace pane_unit {
inline constexpr std::int64_t kDefault = 0; ///< the developer's answer, whatever it becomes
inline constexpr std::int64_t kCells = 1;   ///< an absolute count of canvas cells
/// DEVICE PIXELS, declared from the beginning and currently unprojectable.
///
/// It is a LEGAL authored value on every medium and is refused at PROJECTION on
/// every medium this build has, because no medium publishes a trustworthy
/// per-axis device-pixel scale for a canvas cell -- see `project_pane`
/// (screen.hpp), which owns that refusal and states why the near-misses are
/// traps. Declaring it now rather than later is what makes the refusal branch
/// reachable and tested on day one instead of dead code waiting for a medium.
inline constexpr std::int64_t kPixels = 2;
} // namespace pane_unit

/// WHERE A MAKER PUT A PANE -- one fact, both coordinates.
///
/// ONE MODE FOR THE PAIR, because `doc::move` proves a position is one operation:
/// "a diagonal drag into the top-left corner would slide the object down the edge
/// while reporting a refusal". A width and a height are two values under one
/// rule; an x and a y are one value.
///
/// ABSOLUTE CANVAS CELLS, NOT AN OFFSET FROM THE DEVELOPER'S PLACEMENT. An offset
/// is authored against a default a later build may change, so the same saved
/// bytes would silently mean a different place in the next version. Absolute
/// intent is what `ui::Element` already authors, and RESETTING is what gives back
/// "wherever the default puts it".
struct PanePlace {
    std::int64_t mode = pane_unit::kDefault; ///< `kDefault` or `kCells`; never `kPixels`
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend bool operator==(const PanePlace&, const PanePlace&) = default;
};

/// HOW BIG A MAKER MADE ONE AXIS OF A PANE.
///
/// PER AXIS, AND IT MUST BE. A maker who says "as wide as the pane needs, as tall
/// as the screen allows" is saying two things, and one mode for both would make
/// the narrower of the two intents unsayable.
struct PaneSize {
    std::int64_t mode = pane_unit::kDefault; ///< `kDefault`, `kCells` or `kPixels`
    std::int64_t amount = 0;

    friend bool operator==(const PaneSize&, const PaneSize&) = default;
};

/// ONE ROW OF A SETUP: which pane, and the smallest thing a maker said about its
/// window.
///
/// THE OVERRIDE LIVES BESIDE THE REFERENCE, in the same row, and that is the
/// choice the whole grammar rests on. A separate map keyed by `PaneRef` would
/// create a SECOND POPULATION naming references, and `check_setup` would then owe
/// orphan detection, missing-member detection, and a repair rule for a file
/// carrying both -- a whole second validity surface bought for nothing.
///
/// `front` IS A CANONICAL RANK AND NOT AN ACCUMULATING COUNTER. Over a setup's
/// `n` rows the set of ranks is exactly `{0 .. n-1}`: 0 is back-most, `n-1` is
/// front-most, and there is no tie and therefore no secondary key. A permutation
/// of `0..n-1` is UNIQUE for a given order, so one semantic order has exactly one
/// persisted spelling and reset writes bytes identical to a setup that was never
/// reordered. An accumulating `max + 1` would be an operation TRACE rather than an
/// order: alternating "send A to front" and "send B to front" produces the same two
/// semantic orders forever while the integers grow without bound, so a legal
/// gesture would eventually fail on a setup for which a bounded spelling always
/// existed.
///
/// THE RANK IS OVER ALL AUTHORED ROWS, INCLUDING UNRESOLVED ONES, and that is
/// load-bearing. The presented order is this permutation RESTRICTED to what this
/// build seated, and a restriction of a total order is a total order -- so a pane
/// that stops resolving keeps its exact place for free, and gets it back when it
/// resolves again, with no byte of the file changing either way.
///
/// A DEFAULT-CONSTRUCTED ROW CARRIES `front = 0`, WHICH IS VALID ONLY FOR A
/// ONE-ROW SETUP. That is the one wart of this representation and it is named
/// rather than hidden: `check_setup` judges a completed candidate and never
/// repairs one, so every producer of a row -- `add_pane`, `default_setup`, and any
/// test constructing a candidate by hand -- assigns the rank deliberately.
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
///
/// ORDER IS PRESERVED AND IT IS MEANING, not tidiness -- the same claim
/// `persist.hpp` makes about the objects in a document, and for a smaller but
/// real version of the same reason. An open panel placed in the overlay stack and
/// carrying NO authored place takes the next SLOT in that stack, counted over the
/// open list in order (`bounds_of`), so the order of this list is the order the
/// reactive panes appear down the screen. The list is never sorted, because a save
/// that tidied would silently choose a stacking a maker did not.
///
/// THE LIST'S ORDER IS NOT THE PAINT ORDER (WIND-2). `front` is, and the
/// separation is the whole reason the rank exists: `seat_panes`, `reconcile` and
/// `bounds_of` all read the LIST, so reordering it would move a reactive pane's
/// rectangle -- and "raising a pane must not move it" is then not a discipline
/// somebody keeps but the absence of a write.
///
/// AN EMPTY PANE LIST IS A LEGAL SETUP. "I want nothing open" is a thing a maker
/// can mean and a thing this application can present -- the picker removes the
/// last panel already -- so refusing to save it would make one reachable
/// arrangement unnameable.
struct Setup {
    std::string name;
    std::vector<SetupPane> panes;

    friend bool operator==(const Setup&, const Setup&) = default;
};

// ---- The reference a built-in kind carries, and the kind a reference names ---

/// THE DURABLE REFERENCE FOR AN INTERNAL KIND. Total, for exactly the reason
/// `panel_kind` is total and with exactly the same callers: every one of them
/// derives the kind from bounded local state -- a catalog row the picker's
/// cursor is on, or a panel already open -- so there is no unknown kind for this
/// to have an opinion about.
///
/// IT IS NOT THE FALLIBLE DIRECTION. The direction that meets a stranger's
/// bytes is `resolve_pane` below, and it is the one that answers with nothing.
inline PaneRef pane_ref_of(std::int64_t kind) {
    const PanelKind& row = panel_kind(kind);
    return PaneRef{row.provider, row.pane};
}

/// WHICH INTERNAL KIND THIS REFERENCE NAMES, OR NOTHING.
///
/// The one fallible boundary, and the only door through which text from a file
/// becomes a panel. An unknown provider and an unknown pane key are the same
/// answer -- nothing -- because a caller has the same thing to do about both,
/// and because a finer answer would invite a reader to believe Workshop knows
/// something about the provider it does not (see kWorkshopProvider: silence
/// proves unresolved, never unavailable).
///
/// AN UNKNOWN REFERENCE NEVER BECOMES THE BUILDER. That is the whole of what
/// this function is for; `panel_kind`'s fall-through is correct where it is and
/// would be a lie here.
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
///
/// THE RUNTIME CATALOG IS A REQUIRED ARGUMENT AND IT IS NOT DEFAULTED (WP-0),
/// which is HD-4's rule about `first_visible` applied to a resolution instead of
/// to a window: a default would let a production caller keep the one-argument
/// spelling and be silently right until the first external offer, and the symptom
/// would be a maker's visibly open pane counted as `1 unresolved` on the setup
/// line beneath it. When the compiler stopped every such site, that was the
/// parameter doing its job. `resolve_builtin_pane` above is the narrower question
/// and answers it under a different name, so neither can be reached by accident.
///
/// BUILT-INS FIRST, and it is not merely an ordering. Admission refuses an offer
/// whose `PaneRef` would shadow a compile-time row (`admit_pane_offer`), so no
/// runtime entry can ever match here -- this order is what that refusal would
/// otherwise have to be trusted to have enforced, said a second time in the one
/// place a shadow would do damage.
inline std::optional<std::int64_t> resolve_pane(const PaneRef& ref,
                                                const RuntimeCatalog& runtime) {
    const std::optional<std::int64_t> built_in = resolve_builtin_pane(ref);
    if (built_in.has_value()) {
        return built_in;
    }
    if (const RuntimePane* row = runtime.find(ref.provider, ref.pane)) {
        return row->kind;
    }
    return std::nullopt;
}

/// Whether this build can currently present the pane this reference names.
inline bool resolvable(const PaneRef& ref, const RuntimeCatalog& runtime) {
    return resolve_pane(ref, runtime).has_value();
}

// ---- THE COMBINED CATALOG: what the picker offers, built-ins and offers -------

/// ONE ROW OF THE COMBINED PICKER POPULATION -- a compile-time kind or a runtime
/// one, said in one shape so the picker, the cursor, the selection and the
/// pointer all read one list.
///
/// IT CARRIES COPIES AND NOT VIEWS, deliberately: a runtime row's strings live in
/// a vector that a later offer may reallocate, and a `const char*` taken out of
/// one is a dangling pointer waiting for a repaint. The built-in half's `const
/// char*`s are static and would have been safe; making both halves the same shape
/// is what stops a reader having to know which half they are holding.
struct CatalogRow {
    std::int64_t kind = panel::kBuilder;
    PaneRef ref;
    std::string name;
    std::string summary;
};

/// THE WHOLE POPULATION A MAKER MAY CHOOSE FROM, in the one order:
///
///     every compile-time built-in, in the catalog's own order
///     then every admitted runtime pane, in first-accepted-offer order
///
/// BUILT AS A VALUE RATHER THAN WALKED TWICE. Two loops over two arrays is how
/// the picker's painter, its cursor bound and its selection come to disagree
/// about which row index means what -- the exact class of defect PNL-1 removed
/// from placement by resolving it in one path. It is at most
/// `kMaxPaneCatalogEntries` small rows and it is derived on demand and cached
/// nowhere, which is the same discipline `Screen` and `workspace_scene()` are
/// under.
inline std::vector<CatalogRow> combined_catalog(const Panels& panels) {
    std::vector<CatalogRow> rows;
    rows.reserve(kPanelKinds + panels.runtime.entries.size());
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        rows.push_back(CatalogRow{kPanelCatalog[i].kind,
                                  PaneRef{kPanelCatalog[i].provider, kPanelCatalog[i].pane},
                                  kPanelCatalog[i].name, kPanelCatalog[i].summary});
    }
    for (const RuntimePane& r : panels.runtime.entries) {
        rows.push_back(CatalogRow{r.kind, PaneRef{r.provider, r.pane}, r.name, r.summary});
    }
    return rows;
}

/// The NAME a maker reads for a kind that may be a runtime one -- the catalog's
/// own for a built-in, the offered descriptor's for a runtime pane, and empty for
/// a kind neither knows. A COPY, for `CatalogRow`'s reason.
///
/// IT IS NOT `panel_kind(kind).name`, and that is the whole of why it exists: the
/// total lookup answers `Builder` for anything it does not recognise, which is
/// correct for its own bounded callers and is a lie about a pane some office
/// offered.
inline std::string kind_name(const Panels& panels, std::int64_t kind) {
    if (is_runtime_kind(kind)) {
        if (const RuntimePane* row = panels.runtime.of_kind(kind)) {
            return row->name;
        }
        return std::string();
    }
    return std::string(panel_kind(kind).name);
}

/// A reference as a person reads it, for a notice or a status line: the two
/// halves with a slash between them, which is how this phase's prose spells one.
inline std::string ref_text(const PaneRef& ref) { return ref.provider + "/" + ref.pane; }

/// A SETUP'S NAME AS ONE QUOTED TOKEN OF MAKER-FACING PROSE (WS-0a).
///
/// AUTHORED NAMES STAY AUTHORED; THE SENTENCE THAT QUOTES ONE OWNS THE ESCAPING.
/// Nothing here mutates a setup, nothing here reaches a file, and no string this
/// returns is ever written back into a `Setup` -- it is a spelling, built at the
/// moment a sentence is and discarded with it.
///
/// WHY IT EXISTS. `check_setup_name` accepts ordinary punctuation, `"` and `\`
/// included, and it is right to: `Ops" run` is a name a maker may mean, and
/// narrowing the law after version 1 shipped would make a file that accepted
/// WS-0 wrote unreadable to the build that reads it next. The BYTES were never
/// the problem -- a control character is already refused, so no name can move a
/// terminal's cursor out of the line it was given. The SENTENCE was: three
/// callers spelled this token by hand as a quote, the name, and a quote, so a
/// name carrying a quote could manufacture the very delimiter a maker uses to
/// tell an identity from the status beside it, and a setup honestly named
/// `Ops" UNSAVED | decoy` read back as
///
///     setup "Ops" UNSAVED | decoy" saved | <path> | s name/save  r restore
///
/// THE OWNER HOLDS THE QUOTES, not only the escaping, and that is the
/// load-bearing half of the shape: a caller handed an escaped INTERIOR could
/// still improvise its own boundary around it, which is the same defect one
/// indirection further away. There is one implementation, and the setup status
/// line, the save notice and the restore notice all spend it.
///
/// ONE PASS, WITH THE ESCAPE WRITTEN BEFORE THE BYTE IT ESCAPES, so a backslash
/// this function inserts is never itself examined -- which is what makes an
/// authored backslash followed by an authored quote come back as two backslashes
/// and then an escaped quote, rather than collapsing into a spelling that some
/// other authored name also has. The substitution is INJECTIVE on purpose:
/// rendering a quote as an apostrophe would be shorter and would make two names
/// a maker can tell apart present as one.
///
/// NO POLICY OF ANY OTHER KIND. No locale, no normalisation, no case folding, no
/// width, no JSON. Two bytes mean something to this sentence; every other
/// accepted byte is itself.
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
//
// ONE LAW, REACHED TWO WAYS -- W-5's discipline, applied to the second artifact
// this application persists. The name a maker types in the one-line editor and
// the name a forged file carries meet the SAME function, because two spellings
// of one rule is how a typed name and a loaded one come to disagree about what
// is legal.

/// What this application accepts as a setup's human name.
///
/// FOUR RULES AND NO MORE. It must be there; it must not be only spaces; it must
/// carry no control character; and it must be short enough to read on one line.
///
/// SPACES ARE ALLOWED INSIDE IT, because `Morning build` is a name a maker
/// means, and the reason all-spaces is refused is not tidiness -- a name that
/// renders as nothing would leave the setup line saying `setup ""` and the maker
/// unable to tell a named setup from an unnamed one.
///
/// CONTROL CHARACTERS ARE REFUSED RATHER THAN RENDERED SAFE, and that is the
/// deliberate half. A maker cannot type one; only a forged file can carry one;
/// and what it would do is move the terminal's cursor out of the line this name
/// was given. Refusing names the field and leaves the live setup untouched,
/// which is strictly more useful than a silent substitution a maker would then
/// have to discover.
///
/// WHAT IS NOT HERE: any Unicode policy. Valid UTF-8 is already the Loom gate's
/// answer on the way in from a file, and the platform's own answer on the way in
/// from a keyboard; normalisation, width, case and script are questions this
/// phase has no consumer for and would get wrong by guessing.
///
/// THE LENGTH IS A BYTE COUNT, AND THE REFUSAL SAYS SO (WS-0a).
/// `std::string::size()` is the whole of the measurement, so a nine-character
/// name written in four-byte UTF-8 is thirty-six bytes and this law refuses it --
/// and telling that maker they had exceeded thirty-two CHARACTERS would be a
/// false sentence about a true refusal. Saying `bytes` corrects the wording and
/// is emphatically not a new text policy: nothing here counts a code point, a
/// grapheme or a cell, which is the same absence the paragraph above declares.
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

/// What this application accepts as either half of a `PaneRef`.
///
/// Deliberately narrow about SHAPE and deliberately silent about MEANING: a key
/// must be present, short enough to be a name rather than a payload, and free of
/// whitespace and control characters so that `provider/pane` remains one legible
/// token in a notice. Whether the key names anything is `resolve_pane`'s
/// question and is not an error.
///
/// ITS LENGTH IS A BYTE COUNT TOO, and its refusal says so for
/// `check_setup_name`'s reason (WS-0a): `kMaxPaneKeyLen` is spent against
/// `size()`, and a key is a routing name a provider may write in any script the
/// Loom's UTF-8 gate accepts.
///
/// IT JUDGES A `std::string_view` (WP-0a), and that is the whole of what WP-0a
/// changed about this law -- the empty test, the byte bound and the byte walk are
/// the ones WS-0a left here. Taking a view is what lets the offer door apply this
/// bound to Loom's stamp BEFORE anything owns a copy of it: a checker that took an
/// owned string would have made the copy the precondition of the check that
/// decides whether the copy is allowed. An owned `std::string` caller converts and
/// still meets exactly one law -- there is no second checker to drift from this
/// one, and `check_pane_ref` and the persisted grammar reach it unchanged.
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

inline Written check_pane_ref(const PaneRef& ref) {
    const Written provider = check_pane_key(ref.provider, "provider");
    if (!provider.accepted) {
        return provider;
    }
    return check_pane_key(ref.pane, "pane key");
}

// ---- What this application accepts as authored window intent (WIND-2) --------
//
// THE UNUSED NUMBERS OF A `default` MUST BE ZERO, and that is one rule with one
// reason: there is then exactly ONE smallest canonical spelling of absent intent.
// A file carrying `{"mode":"default","x":7,"y":3}` would round-trip a number that
// means nothing, and the first reader to wonder what it meant would be right to.
//
// NEITHER LAW SEES A `Screen`. A cell coordinate past the right or bottom edge of
// the current canvas is VALID AUTHORED INTENT and is clipped at projection -- the
// document already allows exactly this ("the canvas clips, and a maker dragging
// something half out of view has not made a mistake"), and a second, stricter law
// for a pane would be the same situation answered twice.

/// A PLACE: default with nothing said, or an absolute canvas cell.
inline Written check_pane_place(const PanePlace& p) {
    if (p.mode == pane_unit::kDefault) {
        if (p.x != 0 || p.y != 0) {
            return Written::no("a default pane place carries no coordinates");
        }
        return Written::ok();
    }
    if (p.mode != pane_unit::kCells) {
        return Written::no("a pane place is either default or cells");
    }
    // NEGATIVE IS REFUSED AT THE ROOT, `doc::check_coord`'s own rule and its own
    // reason: the canvas has no cells there.
    if (p.x < 0 || p.y < 0) {
        return Written::no("a pane place cannot be negative");
    }
    if (p.x > doc::kMaxCells || p.y > doc::kMaxCells) {
        return Written::no("a pane place is at most " + std::to_string(doc::kMaxCells) +
                           " cells");
    }
    return Written::ok();
}

/// ONE AXIS OF A SIZE: default, a count of cells, or a count of device pixels.
///
/// `pixels` IS ACCEPTED HERE AND REFUSED AT PROJECTION, and the two are not in
/// tension -- they are `reconcile`'s existing law with a unit substituted for a
/// height. A setup legal on a tall screen is legal on a short one; a setup legal
/// on a window is legal on a terminal. What changes with the medium is which
/// authored intent can be PRESENTED, never which can be held.
inline Written check_pane_size(const PaneSize& s, const char* which) {
    if (s.mode == pane_unit::kDefault) {
        if (s.amount != 0) {
            return Written::no(std::string("a default pane ") + which + " carries no amount");
        }
        return Written::ok();
    }
    if (s.mode == pane_unit::kCells) {
        if (s.amount < ui::kMinCells) {
            return Written::no(std::string("a pane ") + which + " is at least " +
                               std::to_string(ui::kMinCells) + " cell");
        }
        if (s.amount > doc::kMaxCells) {
            return Written::no(std::string("a pane ") + which + " is at most " +
                               std::to_string(doc::kMaxCells) + " cells");
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
    return Written::no(std::string("a pane ") + which + " is default, cells or pixels");
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

// ---- ADMITTING ONE LIVE OFFER INTO THE RUNTIME CATALOG (WP-0) ----------------
//
// EVERYTHING BELOW BOUNDS MATERIAL BEFORE IT IS RETAINED, and the ordering is the
// contract rather than an implementation detail: a descriptor is judged WHOLE and
// only then copied, so an offer that is wrong in its fourth field leaves nothing
// of its first three behind. The Loom's decoder has already refused a payload
// that would materialise more than its own budget; these say what THIS
// application will additionally hold on to, and Surface's clipping is not one of
// them -- a Skin cutting a row at a viewport edge happens long after the bytes
// are in this session's memory.

/// What this application accepts as a runtime descriptor's prose -- a pane's
/// display name or its one-line summary.
///
/// ONE OWNER FOR BOTH, because they are one kind of fact: a short line a maker
/// reads in the picker, arriving from a party this build has never met. The
/// rules are `check_setup_name`'s, minus the one that does not apply -- it must
/// be there, it must be more than spaces, it must carry no control byte, and it
/// must be short enough to read. A name that rendered as nothing would leave a
/// picker row that a maker cannot tell from a blank line; a control byte would
/// move a terminal's cursor out of the row it was given, which is precisely what
/// a forged offer would try.
///
/// THE LENGTH IS A BYTE COUNT AND THE REFUSAL SAYS SO (WS-0a). Nothing here
/// counts a code point, a grapheme or a cell.
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

/// WHAT ADMITTING AN OFFER DID: whether it was accepted, whether it was the first
/// time this `PaneRef` was seen, and which kind now presents it.
///
/// `refreshed` is separate from `accepted` because the two lead somewhere
/// different: a first acceptance may make an authored-but-unresolved setup
/// reference resolve, and a refresh must clear whatever an already-open pane was
/// showing and ask for its room again.
struct Admission {
    Written written = Written::ok();
    bool refreshed = false;                  ///< an existing PaneRef, updated in place
    std::int64_t kind = kFirstRuntimeKind;   ///< valid only when `written.accepted`
};

/// ADMIT ONE `PaneOffered`, UNDER THE OFFICE LOOM STAMPED ON IT.
///
/// `stamped_office` is `mail.authored_role()` and nothing else. This function
/// cannot be given a provider from a payload because `PaneOffered` has no such
/// field; the caller's only other option would be `mail.sender()`, which is a
/// WeaveId rather than a durable route and would make a reloaded provider a
/// different pane.
///
/// ATOMIC, IN BOTH DIRECTIONS:
///
///   a first offer that is invalid       adds no row and no byte
///   a refresh that is invalid           leaves the last accepted descriptor whole
///   a first offer at capacity           is refused, visibly, and changes nothing
///   a refresh at capacity               is still allowed -- capacity bounds how many
///                                       DISTINCT panes are held, not how often a
///                                       provider may correct itself
///
/// A RUNTIME OFFER MAY NOT SHADOW A BUILT-IN. `zengine.workshop/info` offered by
/// some other office is a different `PaneRef` and is admitted normally; offered
/// by whoever holds `zengine.workshop` it names the row this build compiled in,
/// and letting a live message move that row would make the picker's first two
/// entries a thing a message could rewrite.
///
/// TWO OFFICES OFFERING ONE PANE KEY ARE TWO PANES. The `PaneRef` is the pair, so
/// `a.tools/hello` and `b.tools/hello` are two rows, two handles and two
/// presentations, and neither office can refresh or overwrite the other's.
inline Admission admit_pane_offer(RuntimeCatalog& runtime, std::string_view stamped_office,
                                  const PaneOffered& offer) {
    Admission out;
    // THE STAMP IS JUDGED FIRST AND AS A `std::string_view`, before anything owns a
    // copy of it -- the view goes straight into `check_pane_key`, and WP-0a is
    // exactly the phase in which that sentence became true of the statement under
    // it rather than only of the paragraph over it. An empty authored role is
    // personal speech -- Loom writes the field only for a verified office
    // authorship -- and it is refused here as well as at the door, because this
    // function must be safe to call with whatever a caller read off a delivery.
    //
    // AN OFFICE LOOM PRESERVES IS NOT AN OFFICE THIS APPLICATION WILL HOLD. The
    // substrate imposes no bound on a role's length and proves it does not
    // (Loom's `R2E-0a/v6` carries a role past two hundred bytes whole), so a
    // provider's stamp is a stranger's bytes until this line has judged them.
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
    // THE FIRST APPLICATION-OWNED COPY OF THE OFFICE, and it is made here rather
    // than at the top: every one of the four fields has now passed its law, so this
    // is the earliest line at which a copy of any of them could be retained -- and
    // the last at which one is still cheap to abandon. The two refusals below still
    // need the pair to NAME, and both name it from bytes this function has judged.
    const PaneRef ref{std::string(stamped_office), offer.pane};
    if (resolve_builtin_pane(ref).has_value()) {
        out.written = Written::no("`" + ref_text(ref) + "` is a built-in pane");
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
    out.kind = row.kind;
    runtime.entries.push_back(std::move(row));
    return out;
}

/// THE WHOLE-SETUP LAW, asked once on a complete candidate.
///
/// It judges the name, every row, how many there are, whether any two name the
/// same pane, and whether the ranks are a permutation. A DUPLICATE IS REFUSED
/// rather than collapsed: a kind is open or it is not (there is no multi-instance
/// policy to make a second entry mean anything), so a file naming one twice is a
/// file whose author believed something this application does not do, and silently
/// keeping one of the two would hide that.
///
/// THE RANK LAW IS THE DUPLICATE LAW'S OWN SHAPE, ASKED OF A NUMBER (WIND-2):
/// every `front` in `[0, n)`, and no two equal. Those two together are exactly
/// "the set of ranks is `{0..n-1}`", because `n` values drawn from `n` slots with
/// no repeat leave no gap. It is bounded structurally rather than by a chosen
/// ceiling -- the bound is the setup's own size -- and it is medium-independent
/// for the same reason every other law here is: this function never sees a
/// `Screen` and could not be given one without inverting the include order.
///
/// IT JUDGES A COMPLETED CANDIDATE AND NEVER REPAIRS ONE. A public aggregate is
/// not valid by construction, so a producer that appends a row assigns its rank;
/// see `add_pane` and `default_setup`.
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

/// WHICH ROW OF THE SETUP NAMES THIS PANE, or `kNoPaneRow`.
///
/// The one lookup, so a selection, a geometry edit and an ordering operation all
/// find a pane the same way -- by the `PaneRef` that IS its identity, never by a
/// newly minted one and never by a runtime kind, which is a resolved view of the
/// identity rather than the identity itself.
inline constexpr std::size_t kNoPaneRow = static_cast<std::size_t>(-1);

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
///
/// THE END, because that is where `open_panel` has always put a newly opened
/// panel and therefore what a maker has always seen: the panel the picker just
/// opened takes the last slot of the stack. Making the authored order agree with
/// the resolved order a maker was already watching is the whole of the choice.
///
/// AND IT TAKES THE FRONT-MOST RANK, `n-1` AFTER GROWTH (WIND-2), for the same
/// sentence said about depth rather than about a slot: the panel a maker just
/// opened is the one they are already looking at, so it goes in front of what was
/// there. Every other row's rank is untouched and the result is still a
/// permutation, because `n-1` is exactly the value the old set of `0..n-2` did not
/// contain.
///
/// IT AUTHORS NO GEOMETRY. A new row is `default` in place, width and height --
/// which is to say the developer's answer, reactive, and following a later build's
/// change of mind.
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
///
/// Every rank above the removed one comes down by one, which is what keeps the
/// remaining `n-1` rows a permutation of `0..n-2`. Without it the set would carry
/// a hole, `check_setup` would refuse the setup the removal produced, and the
/// refusal would be Workshop's own.
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
//
// EVERY ONE OF THESE REWRITES AT MOST `n` RANKS AND LEAVES A PERMUTATION, and
// none of them writes anything `seat_panes` or `bounds_of` reads. That is why
// "changing the order cannot move, resize, mount, unmount, reseat or regrant a
// pane" is not a rule somebody maintains: it is the ABSENCE OF A WRITE.
//
// NOTHING ACCUMULATES. Ten thousand alternating "send A to front" / "send B to
// front" operations leave every rank inside `0..n-1`, so no operation can ever
// become unavailable and there is no renormalisation threshold, no epoch and no
// history. `front`/`back` are `O(n)`; `raise`/`lower` touch exactly two rows.

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
///
/// It REMOVES the authored difference rather than disguising a default as one --
/// the identity permutation is byte-for-byte what a setup that was never reordered
/// carries, which is what makes this a reset in the same sense resetting a place
/// is one.
inline void reset_front(Setup& s) {
    for (std::size_t i = 0; i < s.panes.size(); ++i) {
        s.panes[i].front = static_cast<std::int64_t>(i);
    }
}

/// SEND TO FRONT, or say it is already there.
///
/// THE NO-OP IS ANSWERED `false`, exactly as `raise_one`'s is, and the two answers are
/// different things to tell a maker: "you are already there" is an answer, and a gesture that
/// reports success and changes nothing is not. Without this the arithmetic below would
/// silently succeed at the front -- every rank above `n-1` is none of them -- and a maker
/// pressing `f` twice would get two identical confirmations of one act.
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
///
/// A no-op at the front is answered `false` rather than silently accepted,
/// because the two are different things to tell a maker: "you are already there"
/// is an answer, and a gesture that reports success and changes nothing is not.
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

// ---- THE GEOMETRY DOORS: what a hand and a key both end at (WIND-2) -----------
//
// TWO DOORS, AND BOTH ARE ATOMIC. `doc::move` writes a position and never an
// extent; `doc::resize` checks BOTH extents before writing EITHER, because "a
// diagonal resize whose height is illegal would narrow the object AND report a
// refusal". Both laws carry here verbatim: moving a pane freezes neither size
// axis, resizing one axis freezes neither the place nor the other axis, and a
// refused proposal writes nothing at all.
//
// THEY TAKE THE WHOLE PROPOSED VALUE AND NOT A DELTA. Saturating the arithmetic is
// the CALLER's job and is done before the proposal is made (`screen.hpp`'s
// `detail::step`), so nothing here can be handed a number that overflowed on the
// way in and nothing here has to guess what a maker meant by one.

/// AUTHOR AN ABSOLUTE PLACE. Writes the place and nothing else.
inline Written author_pane_place(Setup& s, const PaneRef& ref, std::int64_t x,
                                 std::int64_t y) {
    SetupPane* row = pane_of(s, ref);
    if (row == nullptr) {
        return Written::no("`" + ref_text(ref) + "` is not in this setup");
    }
    const PanePlace proposed{pane_unit::kCells, x, y};
    const Written legal = check_pane_place(proposed);
    if (!legal.accepted) {
        return legal;
    }
    row->place = proposed;
    return Written::ok();
}

/// AUTHOR BOTH SIZE AXES AT ONCE, each in its own unit.
///
/// ONE OPERATION AND NOT TWO, for `doc::resize`'s reason: a maker pulling a CORNER
/// proposes a size, not a width and then a height. A caller changing one axis
/// passes the row's current value for the other, which is what makes "resizing one
/// axis writes only that axis" true through a door that takes both.
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

/// THE RESETS. Each removes ONE authored difference and leaves every other
/// untouched, which is what "reset each authored dimension independently" means
/// and is why there are three of them rather than one.
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
/// HOLDS THEM. Asked by the status line and by the notice a restore leaves, so
/// that "unresolved" is a thing a maker can be told the identity of rather than
/// only the count of.
/// THE RUNTIME CATALOG IS REQUIRED HERE FOR THE SHARPEST OF THE REASONS (WP-0).
/// This is the function the setup line and the restore notice ask, so a defaulted
/// or built-in-only spelling would put `1 unresolved` on the row beneath a pane a
/// maker can see. An admitted offer makes its reference resolve, and this says so.
inline std::vector<PaneRef> unresolved_panes(const Setup& s, const RuntimeCatalog& runtime) {
    std::vector<PaneRef> out;
    for (const SetupPane& row : s.panes) {
        if (!resolvable(row.ref, runtime)) {
            out.push_back(row.ref);
        }
    }
    return out;
}

/// EVERY PANE A MAKER MAY CHOOSE FROM **OR** HAS ALREADY AUTHORED -- the one
/// inventory, and the population both the picker and pane management spend
/// (WIND-2).
///
/// THE UNION, AND THE SECOND HALF IS THE RECOVERY INVARIANT. `combined_catalog`
/// answers "what could this build present", which is a fact about the build and
/// says nothing about a reference the setup names and this build cannot resolve --
/// so before WIND-2 an unresolved pane had a count on the setup line and NO ROW
/// ANYWHERE. A maker could not reach it, could not see it and could not remove it
/// without editing the file. Every pane the setup names now has exactly one row,
/// whatever its state, and that is what makes "every pane the setup names is
/// represented in a surface whose contents do not depend on whether the pane can
/// be seen" true rather than aspirational.
///
/// ORDER: THE CATALOG'S, THEN THE SETUP'S. Built-ins in the catalog's own order,
/// then admitted runtime panes in first-accepted-offer order, then whatever the
/// setup names that neither half knew about, in setup order. Deliberately NOT the
/// front order: a list a provider could buy the top of is a list a provider owns,
/// and a maker looking for a pane looks in the place it has always been.
///
/// AN UNRESOLVED ROW WEARS ITS OWN REFERENCE. The pane key is its name -- it is
/// what a person would call it -- and its whole `provider/pane` identity is the
/// summary, because that is the string a maker compares against the file when they
/// are working out whether they have a typo or a pane they have not installed.
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

/// WHAT A FRESH WORKSHOP'S SETUP IS -- derived from `kDefaultPanels`, which is
/// the ONE place "a fresh Workshop shows Info" is decided (panel.hpp). This
/// function holds no opinion of its own about which panes those are, which is
/// what makes it impossible for it to drift from `default_panels()`.
///
/// IT GOES THROUGH `add_pane` RATHER THAN THROUGH `push_back` (WIND-2), and that
/// is the whole of what the canonical rank cost this function: the door assigns
/// the identity permutation as it appends, so a fresh setup is valid by the same
/// act that builds it and there is no second place where "the ranks are `0..n-1`"
/// has to be remembered.
inline Setup default_setup() {
    Setup s;
    s.name = kDefaultSetupName;
    s.panes.reserve(kDefaultPanelCount);
    for (const std::int64_t kind : kDefaultPanels) {
        (void)add_pane(s, pane_ref_of(kind));
    }
    return s;
}

// ---- Authored intent, reconciled onto resolved presentations ------------------

/// WHAT RECONCILING A SETUP ONTO THE LIVE PANELS ACTUALLY DID.
///
/// The kinds that were closed and are now open, the kinds that were open and are
/// now closed, and how many authored references this build could not present.
/// It is a REPORT and not a plan: `reconcile` has already done the work by the
/// time a caller reads one. What the caller does with it is the part that needs
/// a bus -- a newly opened Builder asks its tool what it is -- and that is the
/// weave's, which is why this struct exists rather than a `Mail&` parameter on a
/// function that otherwise touches nothing but presentation.
struct Reconciled {
    std::vector<std::int64_t> opened;
    std::vector<std::int64_t> closed;
    std::size_t unresolved = 0;
    /// THE KINDS THIS SCREEN HAD NO ROOM FOR (WP-0), in setup order -- resolved,
    /// known, and not presented. A fourth outcome and not a flavour of the other
    /// three: the reference is not unresolved (this build knows exactly what it
    /// would draw), it is not closed (the maker authored it and it is still
    /// authored), and it is not opened.
    std::vector<std::int64_t> waiting;
};

/// HOW MANY OVERLAY SLOTS FIT ABOVE THE BOTTOM BAND -- Workshop's current spatial
/// capacity, as one number.
///
/// A `std::size_t` RATHER THAN A `Screen`, and the choice is what keeps this file
/// where it is: `Screen` lives in screen.hpp, which includes this one, so a
/// reconcile that took a screen would invert the include order. What reconcile
/// actually needs is not a screen -- it is the answer to "is there a slot n", and
/// that answer is one integer that the placement path resolves. screen.hpp
/// computes it (`stack_slots_that_fit`) from the same `placement_bounds` the
/// painter and the pointer use, so there is no second geometry here and could not
/// be: this file has no rectangle in it at all.
///
/// IT IS NOT DEFAULTED, for `resolve_pane`'s reason. A default would be the
/// composition's capacity guessed by whoever forgot to pass one.
struct StackCapacity {
    std::size_t slots = 0;
};

/// MAKE THE OPEN PANELS BE WHAT THE SETUP SAYS -- the one path, and the only
/// thing in this application that opens or closes a panel on a setup's behalf.
///
/// THREE CASES, DELIBERATELY DISTINGUISHED (WS-0 §9), because they are three
/// different things to do with a presentation that already exists:
///
///   open before, open after    the presentation is LEFT ALONE. `Panels::builder`
///                              is not touched, so a Builder panel that was
///                              showing a status goes on showing it and no
///                              second refresh is asked for. A reconcile that
///                              rebuilt every presentation would make restoring
///                              a setup you are already in a visible event.
///   open before, closed after  `close_panel` -- the existing door, so the
///                              per-kind view is forgotten by the same act that
///                              removes the panel, exactly as the picker's
///                              removal does. The TOOL is untouched: nothing
///                              here reaches a bus.
///   closed before, open after  opened, and NAMED in `opened` so the caller can
///                              perform whatever asking that kind does on open.
///
/// UNRESOLVED REFERENCES ARE COUNTED AND OTHERWISE IGNORED. They stay in the
/// setup -- this function takes it by const reference and could not remove one
/// if it wanted to -- and they produce no panel, no placeholder, no slot and no
/// message. That last is the point: a placeholder would have to be painted by
/// somebody, and the only kind available to paint it with is the Builder.
/// AND SINCE WP-0 IT ALSO REFUSES WHAT THE SCREEN CANNOT HOLD, which is the
/// FOURTH case and the one an external pane made reachable:
///
///   resolved, and it fits    presented -- `opened`, or left alone if already open
///   resolved, no room        NOT presented, retained in `waiting`, and the
///                            authored reference is untouched. It is not deleted,
///                            not remapped, not given a fake panel and not given
///                            an off-screen placeholder.
///   unresolved               counted, as before
///
/// AUTHORED VALIDITY DOES NOT DEPEND ON EXTENT. A setup legal on a tall screen is
/// legal on a short one -- `check_setup` never sees a `Screen` and this function
/// takes the setup by const reference, so neither could delete a reference if it
/// wanted to. What changes with the room is which references are PRESENTED, which
/// is exactly the authored/resolved split W-1 established, said about a pane.
///
/// CAPACITY IS SPENT IN SETUP ORDER, first come first served. That is what makes
/// the answer stable: a maker who authored `A, B` on a screen with room for one
/// sees A, and growing the screen adds B beneath it rather than rearranging both.
/// WHICH AUTHORED REFERENCES THIS BUILD WOULD PRESENT AT THIS CAPACITY, and which
/// it would not -- resolution and seating, decided together and changing nothing.
///
/// IT IS PURE, AND THAT IS WHY IT EXISTS SEPARATELY (WP-0). Two parties ask it:
/// `reconcile`, which then performs the answer, and the PICKER, which must know
/// whether a row it is about to add could be seated BEFORE it edits the active
/// setup. A picker that added first and read `waiting` afterwards would have
/// authored a pane the maker never saw; a picker that computed seating its own
/// way would be a second copy of this arithmetic, which is the defect PNL-1
/// removed from placement and HD-3 from the caret.
struct Seating {
    std::vector<std::int64_t> wanted;  ///< resolved and seated, in setup order
    std::vector<std::int64_t> waiting; ///< resolved and out of room, in setup order
    std::size_t unresolved = 0;
};

inline Seating seat_panes(const Setup& setup, const RuntimeCatalog& runtime,
                          StackCapacity room) {
    Seating out;
    out.wanted.reserve(setup.panes.size());
    std::size_t stack_used = 0;
    for (const SetupPane& row : setup.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(row.ref, runtime);
        if (!kind.has_value()) {
            ++out.unresolved;
            continue;
        }
        // THE SLOT THIS PANE WOULD TAKE, counted the way `bounds_of` counts it --
        // over the panels actually placed in the stack, in order -- because that
        // is the number the rectangle is resolved from. A side-region pane takes
        // no slot and always fits: its rectangle ends exactly where the workspace
        // does, asserted in screen.hpp against the minimum composition.
        //
        // AND ONLY A REACTIVE PANE SPENDS ONE (WIND-2). A pane the maker PLACED
        // has a rectangle because they said so, and asking the stack's slot
        // arithmetic whether there is "room" for it is asking the wrong question --
        // the tiles it is rationing are not the tiles that pane is standing on. So
        // an authored place takes no slot, and it also cannot be made to WAIT by a
        // capacity it never spent, which narrows `waiting` to exactly what the word
        // has always meant here: the reactive default ran out of tiles.
        if (placement_of(*kind) == placement::kOverlayStack &&
            row.place.mode == pane_unit::kDefault) {
            if (stack_used >= room.slots) {
                out.waiting.push_back(*kind);
                continue;
            }
            ++stack_used;
        }
        out.wanted.push_back(*kind);
    }
    return out;
}

/// THE PRESENTED PANES, BACK TO FRONT -- the one presentation-order helper, and
/// the ONLY thing in this application that reads `front` (WIND-2).
///
/// Paint walks the answer ASCENDING (later is drawn over) and the pointer walks it
/// DESCENDING (topmost answers first), which is `ui::Scene`'s stated law --
/// authored order is paint order, said once -- with the authored order now being
/// the rank rather than the list.
///
/// IT REORDERS NOTHING. `panels.open` keeps the setup order `reconcile` assigned
/// it, because that order is what `bounds_of` counts a reactive slot over; this
/// function returns KINDS and the caller walks them. A sorted `panels.open` would
/// move a reactive pane's rectangle the moment a maker raised something, which is
/// exactly the side effect the rank exists to make impossible.
///
/// A RESTRICTION OF A TOTAL ORDER. The ranks are a permutation over ALL authored
/// rows; the seated panes are a subset; so the presented order has no tie in it and
/// needs no secondary key. A seated kind with no authored row cannot arise from
/// `reconcile` -- it derives `open` from the setup -- and is answered anyway, at the
/// front and in open order, because a total function is cheaper here than an
/// invariant somebody has to maintain.
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
            const std::optional<std::int64_t> kind = resolve_pane(row.ref, panels.runtime);
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
    // A selection sort over at most `kMaxSetupPanes` rows, so no `<algorithm>` and
    // no comparator: the ranks are distinct, so "the smallest remaining" is one
    // pane and the result is deterministic without a tie-break rule existing.
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

inline Reconciled reconcile(Panels& panels, const Setup& setup, StackCapacity room) {
    Reconciled done;
    const Seating seating = seat_panes(setup, panels.runtime, room);
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

    // CLOSE FIRST, THROUGH THE EXISTING DOOR. `panels.open` is copied because
    // `close_panel` erases from it, and the per-kind view has to be forgotten by
    // the same call the picker uses -- a loop that rebuilt the vector directly
    // would leave a removed Builder's copied status alive beside no Builder.
    const std::vector<Panel> before = panels.open;
    for (const Panel& p : before) {
        if (!wants(p.kind)) {
            if (close_panel(panels, p.kind)) {
                done.closed.push_back(p.kind);
            }
        }
    }

    // ...THEN PUT WHAT REMAINS INTO THE SETUP'S ORDER. Assigning the vector
    // rather than opening one by one is what keeps the order authored rather
    // than incidental: `open_panel` appends, so a kind already open would have
    // kept whatever position it had.
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
            // AND A NEWLY OPENED EXTERNAL PANE GETS ITS VIEW HERE, because this
            // loop assigns `panels.open` wholesale rather than calling
            // `open_panel` -- which is the very thing that keeps the authored
            // ORDER (open_panel appends). One line rather than a restructure, and
            // it says the same sentence `open_panel` says: a presentation and its
            // copy of what it presents begin together.
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

/// THE ONE-LINE SETUP-NAME EDITOR: open or not, and the line being typed.
///
/// A MODE, like the picker and like the terminal overlay, and not a panel: it
/// has no catalog row, nothing presents it, and it closes the moment it has been
/// used. It is reachable only from command mode, which is exactly the state in
/// which no inspector row is being edited and the picker is closed -- so the
/// three cannot be live at once, and the routing in `on(KeyPressed)` says so in
/// its order rather than leaving it to that argument.
///
/// THE LINE IS A `component::TextBox` (HD-5) and not a `std::string`: the text,
/// the caret in it and which part of it a one-line editor can show are one fact,
/// and this is the third consumer of the component that fact earned. Nothing
/// bespoke was written for it.
struct SetupNaming {
    bool open = false;
    component::TextBox line;
};

/// THE SETUP A SESSION IS SHOWING, THE ONE IN ITS FILE, AND THE EDITOR OVER IT.
///
/// SAVED IS COMPUTED, NEVER FLAGGED -- the discipline W-5 established for the
/// document, applied to the second artifact. `on_file` is a COPY of the setup as
/// it was last successfully written or read, so `saved()` is a comparison and
/// cannot drift from the thing it describes. A dirty flag would need a hand at
/// every place a pane is added or removed and would be wrong the first time one
/// was missed -- and this phase adds three such places.
///
/// AN UNSAVED FRESH SESSION IS STRUCTURAL. `on_file` starts default-constructed,
/// with an EMPTY NAME, and `check_setup_name` refuses an empty name -- so no
/// setup a maker or a file can produce is ever equal to it. A fresh Workshop
/// therefore says UNSAVED because its setup has genuinely never been written,
/// which is the same sentence the document's own status has always said and for
/// the same reason.
struct SetupState {
    Setup active = default_setup();
    Setup on_file;
    SetupNaming naming;

    bool saved() const { return active == on_file; }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETUP_HPP
