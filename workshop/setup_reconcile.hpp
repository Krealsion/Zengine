// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_RECONCILE_HPP
#define ZENGINE_WORKSHOP_SETUP_RECONCILE_HPP

// What the live panes make of a setup: an offer admitted into the runtime catalog, the population
// a weaver chooses from, the setup reconciled onto the open panes, and their depth order. A part
// of `workshop/setup.hpp`, which includes it after the setup value; nothing else includes it.
// Workshop law: agents/workshop/panes-and-windows.md (+4 registers; agents/workshop.md routes)

#include "pane_settings.hpp"
#include "pane_vocabulary.hpp"
#include "panes.hpp"
#include "property.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- Admitting one live offer into the runtime catalog -----------------------
// A descriptor is judged whole and only then copied: an offer wrong in its fourth field leaves
// nothing of its first three behind.

/// How long a RUNTIME pane descriptor's two prose fields may be.
// WL-CAT-02 -- agents/workshop/catalog.md
inline constexpr std::size_t kMaxPaneNameLen = 32;
// WL-CAT-02 -- agents/workshop/catalog.md
inline constexpr std::size_t kMaxPaneSummaryLen = 64;

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

/// A canvas body a pane asked for (`v3::PaneOffered`): pixels, and rows of text beneath them.
struct PaneBody {
    std::int64_t width = 0, height = 0, text_rows = 0;
};

/// Whether an offer is a pixel's wobble of another: two canvas bodies with the same rows of text
/// beneath, within one pixel of each other on both axes. A text offer moves by whole rows and
/// columns, so it is never one.
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
inline bool within_a_pixel(const PaneOfferedBody& a, const PaneOfferedBody& b) {
    const auto by_a_pixel = [](std::int64_t x, std::int64_t y) {
        return x - y <= 1 && y - x <= 1;
    };
    return a.width > 0 && b.width > 0 && a.text_rows == b.text_rows &&
           by_a_pixel(a.width, b.width) && by_a_pixel(a.height, b.height);
}

/// Admit one `PaneOffered` under the office Loom stamped on it, `mail.authored_role()`: the shape
/// has no provider field, and `mail.sender()` is a WeaveId, which would make a reloaded provider a
/// different pane. A refresh at capacity is allowed: capacity bounds distinct panes.
// WL-CAT-03 -- agents/workshop/catalog.md
inline Admission admit_pane_offer(RuntimeCatalog& runtime, std::string_view stamped_office,
                                  const PaneOffered& offer, std::int64_t rows = 0,
                                  std::int64_t columns = 0, PaneBody body = {}) {
    Admission out;
    if (rows < 0 || columns < 0 || rows > kMaxPaneComfort || columns > kMaxPaneComfort ||
        ((rows == 0) != (columns == 0))) {
        out.written = Written::no("pane comfort must be 1.." + std::to_string(kMaxPaneComfort) +
                                  " body rows and columns, or zero/zero");
        return out;
    }
    if (body.width < 0 || body.height < 0 || body.width > kMaxPaneBodyPx ||
        body.height > kMaxPaneBodyPx || ((body.width == 0) != (body.height == 0)) ||
        body.text_rows < 0 || body.text_rows > kMaxPaneComfort ||
        (body.width == 0 && body.text_rows != 0)) {
        out.written = Written::no("a pane body must be 1.." + std::to_string(kMaxPaneBodyPx) +
                                  " pixels wide and tall with 0.." +
                                  std::to_string(kMaxPaneComfort) +
                                  " rows of text beneath, or zero/zero");
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
    // EVERY FIELD HAS PASSED; ONLY NOW IS ANYTHING WRITTEN.
    const PaneOfferedBody asked{rows, columns, body.width, body.height, body.text_rows};
    for (RuntimePane& row : runtime.entries) {
        if (row.provider == ref.provider && row.pane == ref.pane) {
            row.name = offer.name;       // in place: the position and the handle are kept,
            row.summary = offer.summary; // so an open pane stays the pane it was
            // THE FIRST OFFER STANDS for `default`; `fit` takes this one, and in the OS window
            // stays where it stands while this is within a pixel of it.
            row.latest = asked;
            if (!within_a_pixel(asked, row.standing)) {
                row.standing = asked;
            }
            out.refreshed = true;
            out.kind = row.kind;
            return out;
        }
    }
    if (kBuiltinPaneCount + runtime.entries.size() >= kMaxPaneCatalogEntries) {
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
    row.preferred_width = body.width;
    row.preferred_height = body.height;
    row.preferred_text_rows = body.text_rows;
    row.latest = asked;
    row.standing = asked;
    out.kind = row.kind;
    runtime.entries.push_back(std::move(row));
    return out;
}

/// Which admitted pane a `PaneActions` is about, under the office Loom stamped on it: the shape's
/// half of admission. It takes the pane key, not the shape, since both published versions name a
/// pane alike. An office that never offered this key is refused by name. Nothing is written here:
/// the rows are `join_pane_rows`' (keymap_join.hpp), and the caller commits both or neither.
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

/// WHICH ADMITTED PANE A `PaneSettingsDeclared` IS ABOUT, of either version, under the office Loom
/// stamped on it, and whether its settings are sound by that version's rules (each version's
/// `pane_settings_declared_problem`): the declaration judged whole. An office that never offered the
/// key is refused by name. Nothing is written here; the caller writes the settings or nothing.
// WL-SETTING-03 -- agents/workshop/settings.md
template <class Declared>
inline Admission admit_pane_settings(const RuntimeCatalog& runtime, std::string_view stamped_office,
                                     const Declared& declared) {
    Admission out;
    const Written office = check_pane_key(stamped_office, "provider");
    if (!office.accepted) {
        out.written = office;
        return out;
    }
    const Written key = check_pane_key(declared.pane, "pane key");
    if (!key.accepted) {
        out.written = key;
        return out;
    }
    const RuntimePane* row = runtime.find(stamped_office, declared.pane);
    if (row == nullptr) {
        out.written = Written::no("`" + ref_text(PaneRef{std::string(stamped_office),
                                                         declared.pane}) +
                                  "` is not a pane that office has offered -- its settings "
                                  "were not taken");
        return out;
    }
    const std::string wrong = pane_settings_declared_problem(declared);
    if (!wrong.empty()) {
        out.written = Written::no(row->name + " @" + row->provider + ": its settings were not "
                                  "taken -- " + wrong);
        return out;
    }
    out.kind = row->kind;
    return out;
}

/// SETTINGS AS A SENTENCE LISTS THEM: `legend off, step 3`.
inline std::string settings_said(const std::vector<PaneSetting>& settings) {
    std::string out;
    for (const PaneSetting& one : settings) {
        out += (out.empty() ? "" : ", ") + one.key + " " + setting_value_text(one);
    }
    return out;
}

/// THE CLAUSE A CLOSE SENTENCE ADDS FOR THE SETTINGS IT DISCARDED WITH THE ROW: empty for none.
// WL-SETTING-06 -- agents/workshop/settings.md
inline std::string discarded_settings(const std::vector<PaneSetting>& gone) {
    return gone.empty() ? std::string() : " and its settings here (" + settings_said(gone) + ")";
}

/// ...AND FOR THE KEYS THIS LAYOUT LOCKED ON THE ROW, gone with it: empty for none.
// WL-SETTING-06 -- agents/workshop/settings.md
inline std::string discarded_locks(const std::vector<std::string>& keys) {
    std::string out;
    for (const std::string& key : keys) {
        out += (out.empty() ? "" : ", ") + key;
    }
    return keys.empty() ? std::string() : " and its locks here (" + out + ")";
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

/// THE WHOLE POPULATION A WEAVER MAY CHOOSE FROM, in the one order: every compile-time
/// built-in in the catalog's own order, then every admitted runtime pane in
/// first-accepted-offer order. Built as a value rather than walked twice, and cached nowhere.
// WL-CAT-05 -- agents/workshop/catalog.md
inline std::vector<CatalogRow> combined_catalog(const Panes& panes) {
    std::vector<CatalogRow> rows;
    rows.reserve(kBuiltinPaneCount + panes.runtime.entries.size());
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        rows.push_back(CatalogRow{kBuiltinPanes[i].kind,
                                  PaneRef{kBuiltinPanes[i].provider, kBuiltinPanes[i].pane},
                                  kBuiltinPanes[i].name, kBuiltinPanes[i].summary});
    }
    for (const RuntimePane& r : panes.runtime.entries) {
        rows.push_back(CatalogRow{r.kind, PaneRef{r.provider, r.pane}, r.name, r.summary});
    }
    return rows;
}

/// The name a weaver reads for a kind, built-in or runtime; empty for one none knows.
inline std::string kind_name(const Panes& panes, std::int64_t kind) {
    if (is_runtime_kind(kind)) {
        if (const RuntimePane* row = panes.runtime.of_kind(kind)) {
            return row->name;
        }
        return std::string();
    }
    return std::string(builtin_pane(kind).name);
}

/// The references this build cannot currently present, IN THE ORDER THE SETUP
/// HOLDS THEM.
inline std::vector<PaneRef> unresolved_panes(const Setup& s, const Panes& panes) {
    std::vector<PaneRef> out;
    for (const SetupPane& row : s.panes) {
        if (!resolvable(row.ref, panes)) {
            out.push_back(row.ref);
        }
    }
    return out;
}

/// EVERY PANE A WEAVER MAY CHOOSE FROM **OR** HAS ALREADY AUTHORED -- the one inventory,
/// said out loud to whatever presents it (the desktop's Pane Manager) and spent by both doors.
// WL-PANE-12 -- agents/workshop/panes-and-windows.md
inline std::vector<CatalogRow> inventory_rows(const Setup& setup, const Panes& panes) {
    std::vector<CatalogRow> rows = combined_catalog(panes);
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

// ---- Authored intent, reconciled onto resolved presentations ------------------

/// WHAT RECONCILING A SETUP ONTO THE LIVE PANES ACTUALLY DID.
// WL-PANE-07 -- agents/workshop/panes-and-windows.md
struct Reconciled {
    std::vector<std::int64_t> opened;
    std::vector<std::int64_t> closed;
    std::size_t unresolved = 0;
};

/// THE ROOM THE OVERLAY COLUMN HAS ON THIS SCREEN, and the measures a pane in it is sized by:
/// how many panes of the fallback height one pass down the column holds, the room's extent, and
/// the medium's text and chrome.
// WL-PANE-03 -- agents/workshop/panes-and-windows.md
struct StackCapacity {
    std::size_t slots = 0;
    std::int64_t height = 0, width = 0;
    std::int64_t line = 0, column = 0, border = 0, fallback_height = 0, gap = 0;
    /// A canvas body's own measures: the pane's edge on each side, its one header row, one row of
    /// the medium's text, and the device unit a body is laid out on -- all canvas pixels.
    std::int64_t edge = 0, header = 0, text_row = 0, grain = 1;
};

struct PreferredExtent { std::int64_t width = 0, height = 0; };

/// The outer extent that grants a pane the body it asked for: text rows and columns at the
/// medium's metric, or a canvas body of exactly its pixels -- rounded up to the device unit where
/// the medium cannot say a pixel -- with its text rows beneath; never more than the room.
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
inline PreferredExtent offered_extent(const PaneOfferedBody& asked, const StackCapacity& room) {
    if (!room.height) return {};
    if (asked.width) {
        const auto up = [&room](std::int64_t px) {
            const std::int64_t g = room.grain > 0 ? room.grain : 1;
            return (px + g - 1) / g * g;
        };
        return {std::min(room.width, up(asked.width) + 2 * room.edge),
                std::min(room.height, up(asked.height) + asked.text_rows * room.text_row +
                                          room.header + 2 * room.edge)};
    }
    if (!asked.rows) return {};
    return {std::min(room.width, asked.columns * room.column + 2 * room.border),
            std::min(room.height, (asked.rows + 1) * room.line + 2 * room.border)};
}

/// The extent `default` reads: the pane's first accepted offer, which a refresh cannot move.
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
inline PreferredExtent preferred_extent(const RuntimePane* pane, const StackCapacity& room) {
    if (!pane) return {};
    return offered_extent(PaneOfferedBody{pane->preferred_rows, pane->preferred_columns,
                                          pane->preferred_width, pane->preferred_height,
                                          pane->preferred_text_rows},
                          room);
}

/// The offer `fit` reads in this room: the latest where the medium cannot say a pixel, and in the
/// OS window the one it stands on, which a re-offer within a pixel of it does not move.
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
inline const PaneOfferedBody& fit_offer(const RuntimePane& pane, const StackCapacity& room) {
    return room.grain == 1 ? pane.standing : pane.latest;
}

/// The extent `fit` reads (`fit_offer`).
// WL-PANE-17 -- agents/workshop/panes-and-windows.md
inline PreferredExtent fit_extent(const RuntimePane* pane, const StackCapacity& room) {
    if (!pane) return {};
    return offered_extent(fit_offer(*pane, room), room);
}

/// WHICH AUTHORED REFERENCES THIS BUILD PRESENTS, and how many it cannot -- resolution alone,
/// changing nothing. Every resolved reference is seated: the stack is not rationed, since its
/// column begins again at its top when a pane would pass its floor (`bounds_of`).
// WL-PANE-03, WL-PANE-07 -- agents/workshop/panes-and-windows.md
struct Seating {
    std::vector<std::int64_t> wanted; ///< resolved, in setup order
    std::size_t unresolved = 0;
};

inline Seating seat_panes(const Setup& setup, const Panes& panes) {
    Seating out;
    out.wanted.reserve(setup.panes.size());
    for (const SetupPane& row : setup.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(row.ref, panes);
        if (!kind.has_value()) {
            ++out.unresolved;
            continue;
        }
        out.wanted.push_back(*kind);
    }
    return out;
}

/// THE AUTHORED PANE ORDER, BACK TO FRONT -- the one place `front` is read.
// WL-FRONT-05, WL-FRONT-06 -- agents/workshop/planes.md
inline std::vector<std::int64_t> presentation_order(const Setup& setup, const Panes& panes) {
    struct Ranked {
        std::int64_t front;
        std::int64_t kind;
    };
    std::vector<Ranked> ranked;
    ranked.reserve(panes.open.size());
    std::vector<std::int64_t> unranked;
    for (const OpenPane& p : panes.open) {
        bool found = false;
        for (const SetupPane& row : setup.panes) {
            const std::optional<std::int64_t> kind = resolve_pane(row.ref, panes);
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
                                                      const Panes& panes) {
    std::vector<std::int64_t> order = presentation_order(setup, panes);
    const std::int64_t lifted = selected_pane(panes);
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

/// MAKE THE OPEN PANES BE WHAT THE SETUP SAYS -- the one path, and the only thing in this
/// application that opens or closes a pane on a setup's behalf. Three cases, deliberately
/// distinguished, in setup order; an unresolved reference is counted.
// WL-PANE-07 -- agents/workshop/panes-and-windows.md
inline Reconciled reconcile(Panes& panes, const Setup& setup) {
    Reconciled done;
    const Seating seating = seat_panes(setup, panes);
    const std::vector<std::int64_t>& wanted = seating.wanted;
    done.unresolved = seating.unresolved;

    const auto wants = [&wanted](std::int64_t kind) {
        for (const std::int64_t k : wanted) {
            if (k == kind) {
                return true;
            }
        }
        return false;
    };

    // Close first, through the close door, over a copy: `close_kind` erases from `panes.open` and
    // forgets the kind's view.
    const std::vector<OpenPane> before = panes.open;
    for (const OpenPane& p : before) {
        if (!wants(p.kind)) {
            if (close_kind(panes, p.kind)) {
                done.closed.push_back(p.kind);
            }
        }
    }

    // Then assign what remains in the setup's order: `open_kind` appends, so an already-open kind
    // would keep its old position.
    std::vector<OpenPane> now;
    now.reserve(wanted.size());
    for (const std::int64_t kind : wanted) {
        bool was_open = false;
        for (const OpenPane& p : before) {
            if (p.kind == kind) {
                was_open = true;
                break;
            }
        }
        if (!was_open) {
            done.opened.push_back(kind);
            // ...so a newly opened external pane gets its view here, as `open_kind` would give it.
            if (is_runtime_kind(kind) && panes.external_pane(kind) == nullptr) {
                ExternalPane fresh;
                fresh.kind = kind;
                panes.external.push_back(std::move(fresh));
            }
        }
        now.push_back(OpenPane{kind});
    }
    panes.open = std::move(now);
    return done;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SETUP_RECONCILE_HPP
