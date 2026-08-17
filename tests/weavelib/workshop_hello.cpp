// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Hello pane provider — a REAL dynamic weave that offers Workshop one
// read-only pane, and the whole of WP-0's external witness.
//
// IT IS A FIXTURE AND NOT A PRODUCT. It is built by `tests/`, loaded only by the
// Workshop suite, and named in no host's boot list. Workshop ships two panes and
// this is not one of them; what it exists to prove is that the SEAM works through
// the real ABI, the real Kernel and a real `.so`/`.dll`, rather than through a
// registration callback a test could reach for and production could not.
//
// WHAT IT DELIBERATELY DOES NOT DO. It writes no file, starts no process, opens
// no socket, holds no timer, publishes no canvas, and asks Workshop for nothing.
// The four shapes below are its entire vocabulary, which is what makes the pane
// protocol's own reach legible: a pane grants a provider no ambient authority at
// all.
//
// AND ITS BUS GRANT IS NOT NARROW, WHICH IS REPORTED RATHER THAN HIDDEN. A weave
// loaded normally in-process through the current Loom receives `Grant{}.allow_any()`
// by default, so this fixture is trusted test code sharing this process's memory.
// The pane protocol containing only offer-and-content messages is a fact about the
// PROTOCOL; it is not a containment claim about the loader, and WP-0 does not make
// one.

#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace {

namespace surface = zengine::surface;
using zengine::workshop::PaneCatalogRequested;
using zengine::workshop::PaneContent;
using zengine::workshop::PaneOffered;
using zengine::workshop::PaneRoom;

/// THIS PROVIDER'S OFFICE — the durable half of the `PaneRef` a maker's setup
/// will name it by. Hard-coded, because a fixture that could be configured into
/// several identities would be testing its own configuration rather than the seam.
constexpr const char* kHelloRole = "zengine.test.workshop-hello";

/// The office Workshop holds. Named here rather than reached through
/// `workshop/panel.hpp` on purpose: a provider is a stranger to Workshop's
/// internals and must be able to say who it is talking to with a string, which is
/// exactly what a real third party would have.
constexpr const char* kWorkshopRole = "zengine.workshop";

constexpr const char* kPaneKey = "hello";
constexpr const char* kPaneName = "Hello";
constexpr const char* kPaneSummary = "a bounded external greeting";

struct HelloState {
    /// How many rooms this provider has been granted — the number that makes a
    /// duplicate `PaneRoom` visible if one is ever sent.
    std::int64_t rooms = 0;
    std::int64_t offers = 0;
    std::int64_t refused = 0; ///< asks that were not authored by the Workshop office
    ZEN_EXPOSE();
    ZEN_SHAPE(HelloState, 1, ZEN_FIELD(rooms), ZEN_FIELD(offers), ZEN_FIELD(refused));
};

class HelloPaneWeave
    : public loom::WeaveBase<HelloPaneWeave, HelloState,
                             loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom>,
                             loom::Emit<PaneOffered, PaneContent>> {
public:
    /// FIRST BREATH, AND ONLY IF LOOM SAYS SO. `ActivationCursor` owns both halves
    /// of that sentence (activation/activation.hpp): the lifecycle attestation must
    /// be Loom's, and the sequence must be one this incarnation has not already
    /// acted on. An ordinary `zen.Activated` sent by anybody granted the shape is
    /// refused here and announces nothing — which is the negative control the suite
    /// spends, because a provider that announced on a forged activation would let
    /// any weave make a pane appear in a maker's picker.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    /// WORKSHOP ASKING WHO HAS PANES. Answered only when Workshop actually asked.
    ///
    /// `authored_from_role` AND NOT `sender()`. The ask arrives as a PUBLICATION,
    /// so it reaches every weave that accepts the shape and there is no addressing
    /// to read intent from; what says it was Workshop is Loom's stamp on the
    /// authorship. A provider that answered any arriving `PaneCatalogRequested`
    /// would hand its catalog to whoever asked for it, including a weave with no
    /// office at all.
    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        announce(mail);
    }

    /// WORKSHOP GRANTING THIS PANE ITS PROSE BUDGET.
    ///
    /// THE CONTENT IS FORMATTED FROM THE ROOM IT WAS ACTUALLY GIVEN, and that is
    /// the fixture's one piece of deliberate design: the suite can read the granted
    /// rows and columns off the canvas a maker would see, so the room contract is
    /// observed through the real presentation rather than through a test-only hook
    /// bolted onto production.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return; // a forged room grants nothing and produces no content
        }
        if (room.pane != kPaneKey) {
            return; // a room for a pane this provider does not have
        }
        ++state_.rooms;
        PaneContent said;
        said.pane = kPaneKey;
        // Row one names the room, which is what makes the budget legible on screen.
        said.rows.push_back(surface::SurfaceTextRow{
            fitted("hello -- " + std::to_string(room.rows) + "x" + std::to_string(room.columns),
                   room.columns),
            surface::role::kFill});
        // ...and the remaining rows are filled to the row budget exactly, so a suite
        // reading the pane can tell "this provider spent its whole grant" from "this
        // provider had nothing more to say".
        for (std::int64_t i = 1; i < room.rows; ++i) {
            said.rows.push_back(surface::SurfaceTextRow{
                fitted("row " + std::to_string(i), room.columns), surface::role::kMuted});
        }
        // DELIBERATELY AS THIS OFFICE. `mail.send_to_role(...)` would be PERSONAL
        // speech from a weave that happens to hold the office, and Workshop refuses
        // it — holding is never speaking-for (MSG-07).
        (void)mail.as_role(kHelloRole).send_to_role(kWorkshopRole, said);
    }

private:
    /// One offer, authored as this office and addressed to the Workshop office.
    ///
    /// DIRECTED RATHER THAN PUBLISHED. Workshop is the only party this concerns,
    /// and a broadcast catalog entry would be an announcement to a room that did
    /// not ask. The catalog REQUEST is the one publication in this protocol, and it
    /// is published only because Workshop does not yet know whose door to knock on.
    void announce(loom::Mail& mail) {
        ++state_.offers;
        (void)mail.as_role(kHelloRole)
            .send_to_role(kWorkshopRole, PaneOffered{kPaneKey, kPaneName, kPaneSummary});
    }

    /// A row cut to the granted columns. The provider owns what its rows SAY and
    /// owes Workshop rows that fit; Workshop refuses an over-wide row rather than
    /// truncating it, so a provider that does not measure loses its whole update.
    static std::string fitted(std::string text, std::int64_t columns) {
        if (columns <= 0) {
            return std::string();
        }
        if (static_cast<std::int64_t>(text.size()) > columns) {
            text.resize(static_cast<std::size_t>(columns));
        }
        return text;
    }

    zengine::ActivationCursor activation_;
};

} // namespace

ZEN_EXPORT_WEAVE(HelloPaneWeave)
