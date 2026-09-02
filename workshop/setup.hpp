// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_HPP
#define ZENGINE_WORKSHOP_SETUP_HPP

// WHAT A MAKER CALLS THE ARRANGEMENT THEY ARE WORKING IN, and the whole of what
// that is allowed to mean.
// Workshop law: agents/workshop/layouts.md (+7 registers; agents/workshop.md routes)

#include "document.hpp" // `doc::kMaxCells` -- the bound an authored cell count already has
#include "surface/vocabulary.hpp" // `kCellSubs` -- the fine lattice authored amounts live on
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

/// How long a setup's human name may be.
inline constexpr std::size_t kMaxSetupNameLen = 32;

/// How long either half of a `PaneRef` may be.
// WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxPaneKeyLen = 64;

/// How many pane references one setup may carry.
// WL-MIG-03 -- agents/workshop/migration.md; WL-SETUP-01 -- agents/workshop/setup-file.md
inline constexpr std::size_t kMaxSetupPanes = 32;

/// How long a RUNTIME pane descriptor's two prose fields may be.
inline constexpr std::size_t kMaxPaneNameLen = 32;
inline constexpr std::size_t kMaxPaneSummaryLen = 64;

/// THE LARGEST DEVICE-PIXEL AMOUNT A PANE MAY BE AUTHORED AT.
// WL-SETUP-06 -- agents/workshop/setup-file.md
inline constexpr std::int64_t kMaxPanePixels = 65536;

// ---- The value ---------------------------------------------------------------

/// WHICH PANE A MAKER MEANT -- durably, and without naming a catalog slot.
// WL-LAYOUT-11 -- agents/workshop/layouts.md; WL-SETUP-01 -- agents/workshop/setup-file.md
struct PaneRef {
    std::string provider;
    std::string pane;

    friend bool operator==(const PaneRef&, const PaneRef&) = default;
};

// ---- THE AUTHORED WINDOW: the smallest difference from a default --------------
// WL-SETUP-01, WL-SETUP-04 -- agents/workshop/setup-file.md

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
} // namespace pane_unit

/// THE AUTHORED LATTICE'S WALLS, in sub-units: the same cell bounds the setup
/// law has always enforced, expressed at the resolution the amounts now carry.
// WL-GEO-06 -- agents/workshop/geometry.md
inline constexpr std::int64_t kPaneSubMin = ui::kMinCells * surface::kCellSubs;
inline constexpr std::int64_t kPaneSubMax = doc::kMaxCells * surface::kCellSubs;

/// WHERE A MAKER PUT A PANE -- one fact, both coordinates.
// WL-PANE-11 -- agents/workshop/panes-and-windows.md; WL-SETUP-03 -- agents/workshop/setup-file.md
struct PanePlace {
    std::int64_t mode = pane_unit::kDefault; ///< `kDefault` or `kSubcells`; never `kPixels`
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
// WL-MAKER-04 -- agents/workshop/maker-pane.md; WL-PANE-12 -- agents/workshop/panes-and-windows.md
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

// ---- THE COMBINED CATALOG: what the picker offers, built-ins and offers -------

/// ONE ROW OF THE COMBINED PICKER POPULATION -- a compile-time kind or a runtime
/// one, said in one shape so the picker, the cursor, the selection and the
/// pointer all read one list.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
struct CatalogRow {
    std::int64_t kind = panel::kBuilder;
    PaneRef ref;
    std::string name;
    std::string summary;
};

/// The one line the picker reads under a maker-made pane's name.
inline constexpr const char* kMakerPaneSummary = "a pane you made -- Pane Creator";

inline std::vector<CatalogRow> combined_catalog(const Panels& panels) {
    std::vector<CatalogRow> rows;
    rows.reserve(kPanelKinds + 1 + panels.runtime.entries.size());
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        rows.push_back(CatalogRow{kPanelCatalog[i].kind,
                                  PaneRef{kPanelCatalog[i].provider, kPanelCatalog[i].pane},
                                  kPanelCatalog[i].name, kPanelCatalog[i].summary});
    }
    // THE MAKER'S OWN PANE, BETWEEN THE BUILT-INS AND THE STRANGERS: Workshop-owned, so it
    // sits with Workshop's rows, and after them because a maker's own row is the one that
    // was not there yesterday. Its name is the definition's and its identity is minted
    // from it -- there is no copy of either here.
    if (panels.maker.open()) {
        rows.push_back(CatalogRow{kMakerPaneKind, maker_pane_ref(panels.maker.definition.name),
                                  panels.maker.definition.name, kMakerPaneSummary});
    }
    for (const RuntimePane& r : panels.runtime.entries) {
        rows.push_back(CatalogRow{r.kind, PaneRef{r.provider, r.pane}, r.name, r.summary});
    }
    return rows;
}

/// The NAME a maker reads for a kind that may be a runtime one -- the catalog's
/// own for a built-in, the offered descriptor's for a runtime pane, and empty for
/// a kind neither knows. A COPY, for `CatalogRow`'s reason.
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

/// A reference as a person reads it, for a notice or a status line: the two
/// halves with a slash between them, which is how this phase's prose spells one.
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
/// THE LENGTH IS A BYTE COUNT, AND THE REFUSAL SAYS SO.
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
/// `check_setup_name`'s reason: `kMaxPaneKeyLen` is spent against
/// `size()`, and a key is a routing name a provider may write in any script the
/// Loom's UTF-8 gate accepts.
///
/// IT JUDGES A `std::string_view`, and that is the whole of what the offer door
/// changed about this law -- the empty test, the byte bound and the byte walk are
/// the ones the setup law left here. Taking a view is what lets the offer door apply this
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

// ---- What this application accepts as authored window intent -----------------
// WL-SETUP-03 -- agents/workshop/setup-file.md

/// ONE COORDINATE of an authored place. NEGATIVE IS REFUSED AT THE ROOT.
// WL-ARR-06 -- agents/workshop/arrangement.md; WL-SETUP-03 -- agents/workshop/setup-file.md
inline Written check_pane_place_coord(std::int64_t v) {
    if (v < 0) {
        return Written::no("a pane place cannot be negative");
    }
    if (v > kPaneSubMax) {
        return Written::no("a pane place is at most " + std::to_string(doc::kMaxCells) +
                           " cells");
    }
    return Written::ok();
}

/// A PLACE: default with nothing said, or an absolute position on the fine lattice.
inline Written check_pane_place(const PanePlace& p) {
    if (p.mode == pane_unit::kDefault) {
        if (p.x != 0 || p.y != 0) {
            return Written::no("a default pane place carries no coordinates");
        }
        return Written::ok();
    }
    if (p.mode != pane_unit::kSubcells) {
        return Written::no("a pane place is either default or subcells");
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
        // The same one-cell floor and kMaxCells ceiling as ever, on the fine
        // lattice — a pane's smallest authorable extent is still exactly one cell.
        if (s.amount < kPaneSubMin) {
            return Written::no(std::string("a pane ") + which + " is at least " +
                               std::to_string(ui::kMinCells) + " cell");
        }
        if (s.amount > kPaneSubMax) {
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

// ---- ADMITTING ONE LIVE OFFER INTO THE RUNTIME CATALOG -----------------------
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
/// THE LENGTH IS A BYTE COUNT AND THE REFUSAL SAYS SO. Nothing here
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
    // copy of it -- the view goes straight into `check_pane_key`, and admitting an offer is
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
    // THE MAKER NAMESPACE IS WORKSHOP'S OWN, and no office may speak in it: a pane a maker
    // made is presented by Workshop from authored data, and an offer stamped with its
    // namespace would put a stranger's rows behind a maker's own name.
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
    out.kind = row.kind;
    runtime.entries.push_back(std::move(row));
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

/// WHICH ROW OF THE SETUP NAMES THIS PANE, or `kNoPaneRow`.
// WL-SETUP-01 -- agents/workshop/setup-file.md
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
// WL-PANE-07 -- agents/workshop/panes-and-windows.md; WL-SETUP-07 -- agents/workshop/setup-file.md

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
// WL-ARR-06 -- agents/workshop/arrangement.md; WL-SETUP-08 -- agents/workshop/setup-file.md

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

/// EVERY PANE A MAKER MAY CHOOSE FROM **OR** HAS ALREADY AUTHORED -- the one
/// inventory, and the population both the picker and pane management spend.
// WL-PED-03, WL-PED-04 -- agents/workshop/pane-manager.md
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

/// WHAT A FRESH WORKSHOP'S SETUP IS -- derived from `kDefaultPanels`, which is
/// the ONE place "a fresh Workshop shows Info" is decided (panel.hpp).
// WL-LAYOUT-03 -- agents/workshop/layouts.md; WL-SETUP-07 -- agents/workshop/setup-file.md
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
};

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
    for (const SetupPane& row : setup.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(row.ref, panels);
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
        // AND ONLY A REACTIVE PANE SPENDS ONE. A pane the maker PLACED
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
        // ROTATE, DO NOT SWAP. Everything behind the lifted pane keeps its authored
        // order relative to everything else -- a swap would exchange two panes' depths
        // and leave the desk describing an arrangement nobody authored.
        order.erase(order.begin() + static_cast<std::ptrdiff_t>(i));
        order.push_back(lifted);
        break;
    }
    return order;
}

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

/// WHAT THE ACTIVE LAYOUT'S TOP-ROW STATUS SAYS -- three answers, DERIVED at every
/// composition and stored nowhere (a condition is read off a live owner, never remembered).
/// owner, never remembered).
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
// WL-LAYOUT-01, WL-LAYOUT-02, WL-LAYOUT-03 -- agents/workshop/layouts.md
struct SetupState {
    Setup active = default_setup();
    SetupLink active_link;
    LayoutNaming naming;
    std::vector<Layout> shelved;
    std::size_t active_at = 0;
};

/// HOW MANY LAYOUTS THIS WORKSHOP IS HOLDING, the active one included.
///
/// NEVER ZERO, structurally: `active` is a value rather than a pointer, so the
/// floor is the type's and not a rule somebody keeps.
inline std::size_t layout_count(const SetupState& s) noexcept { return s.shelved.size() + 1; }

/// WHERE POSITION `at` SITS ON THE SHELF, for the positions that are not the live
/// one. The one place a run index becomes a shelf index, so the readers below
/// cannot come to spell it differently.
inline std::size_t shelf_index(const SetupState& s, std::size_t at) noexcept {
    return at < s.active_at ? at : at - 1;
}

/// THE DESK AT POSITION `at` IN THE MAKER'S ORDER -- a read, and the one place the
/// run's spelling is undone. Out of range answers the active layout, for
/// `bounds_of`'s reason: every caller of this already has a position it got from
/// this same run, and a second refusal shape would be a state to keep true.
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
    return true;
}

/// THE MAKER'S ORDERED RUN, WITH THE LIVE ONE PUT BACK WHERE IT SITS.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline std::vector<Layout> layout_run(const SetupState& s) {
    std::vector<Layout> run = s.shelved;
    // TOTAL OVER `active_at`, for `layout_at`'s reason exactly: nothing in this
    // file can produce a position past the shelf, and a second refusal shape
    // would be a state somebody has to keep true.
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
    // WHERE THE LIVE ELEMENT ENDED UP, COMPUTED FROM THE ERASE AND THE INSERT it
    // just went through -- never searched for, because two layouts may hold equal
    // values and a search would find whichever came first.
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
