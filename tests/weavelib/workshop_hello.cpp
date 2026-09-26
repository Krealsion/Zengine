// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Hello pane provider: a real dynamic weave offering Workshop one read-only pane, and the
// smallest complete witness of the external pane seam (docs/workshop/panes.md). A fixture, not a
// product: loaded only by the Workshop suites, through the real ABI and Kernel. It writes no file,
// starts no process, opens no socket and publishes no canvas: the pane protocol grants a provider
// no ambient authority. Its bus grant is Loom's in-process default, `allow_any`, so the protocol's
// narrowness is a fact about the protocol, not a containment claim about the loader.

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
    /// First breath, and only if Loom says so: `ActivationCursor` (activation/activation.hpp)
    /// wants Loom's lifecycle attestation and a sequence this incarnation has not acted on. A
    /// `zen.Activated` sent by anybody granted the shape is refused and announces nothing -- the
    /// suite's negative control, since a provider announcing on a forged activation would let any
    /// weave make a pane appear in a maker's picker.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    /// Workshop asking who has panes, answered only when Workshop asked: `authored_from_role`, not
    /// `sender()`. The ask is a publication reaching every weave that accepts the shape, so Loom's
    /// stamp on its authorship is what says it was Workshop; a provider answering any
    /// `PaneCatalogRequested` would hand its catalog to whoever asked, even a weave with no office.
    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            ++state_.refused;
            return;
        }
        announce(mail);
    }

    /// Workshop granting this pane its prose budget. The content is formatted from the room it
    /// was given, the fixture's one deliberate design: the suite reads the granted rows and
    /// columns off the canvas a maker would see, so the room contract is observed through the
    /// real presentation, not a test-only hook.
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
