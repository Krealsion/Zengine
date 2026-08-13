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
//   - no docking, no tabs, no dragging, no resizing, no saved layout. A panel
//     appears where this file says panels appear. The placement is chosen for
//     being the simplest thing the current geometry supports, not for being
//     good, and the report says what using it felt like.
//   - no focus framework. There is no focused panel, no z-order and no capture.
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
//     an array of two strings and an integer, spelled out below.
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

#include <cstddef>
#include <cstdint>
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

/// One entry in the catalog: what a maker sees in the picker.
struct PanelKind {
    std::int64_t kind = panel::kBuilder;
    const char* name = "";    ///< what the picker lists
    const char* summary = ""; ///< one line, so a maker can tell what they are opening
};

/// THE CATALOG. Workshop's own, and complete: a panel that is not here cannot be
/// opened, because the picker is the only door and the picker walks this array.
///
/// It is a constant rather than a registry, and that is the honest shape of what
/// exists: a registry would be a mechanism for parties unknown to add entries,
/// and there are no such parties. When there are, this becomes the thing they
/// add to, and the picker below does not change.
inline constexpr PanelKind kPanelCatalog[] = {
    {panel::kBuilder, "Builder", "build one known target"},
    {panel::kInfo, "Info", "objects and properties"},
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

/// The `+ panel` picker: open or not, and which entry a maker is on.
///
/// It is a MODE and not a panel. It has no instance, nothing presents it, and it
/// closes the moment it has been used — so it is not in the catalog and cannot
/// be opened from itself.
struct PanelPicker {
    bool open = false;
    std::size_t cursor = 0;
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
/// Nothing is authored from this struct and nothing is asked through it. Opening
/// the panel sends `builder::StatusRequested` and everything here arrives as the
/// tool's own published answer.
struct BuilderPane {
    bool heard = false;
    bool awaiting = false;
    builder::BuildStatus shown{};
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
inline std::vector<Panel> default_panels() { return {Panel{panel::kInfo}}; }

/// Every dynamic panel this session has open, plus the picker and the per-kind
/// views. Session, never document.
struct Panels {
    std::vector<Panel> open = default_panels();
    PanelPicker picker;
    BuilderPane builder;

    bool has(std::int64_t kind) const {
        for (const Panel& p : open) {
            if (p.kind == kind) {
                return true;
            }
        }
        return false;
    }
};

/// Open a panel of this kind, or say why not.
///
/// Answers whether anything changed, so the caller can tell a maker the truth
/// either way rather than showing an unchanged screen with no explanation.
inline bool open_panel(Panels& panels, std::int64_t kind) {
    if (panels.has(kind)) {
        return false;
    }
    panels.open.push_back(Panel{kind});
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
            return true;
        }
    }
    return false;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANEL_HPP
