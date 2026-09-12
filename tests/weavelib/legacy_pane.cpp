// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A PANE PROVIDER BUILT AGAINST THE PUBLISHED PROTOCOL ALONE -- a REAL dynamic weave, its own
// image, compiled from the pane-action protocol exactly as `84b6bc0` published it
// (`legacy_pane_protocol.hpp`) and from nothing in `workshop/`. It is what a pane written before
// ownership existed IS to this host: an artifact nobody rebuilt.
//
// WHAT IT PROVES, AND WHERE. Loaded through the supported plan path with the current host, its
// offer is admitted, its version-one declaration is joined (widened at the host's door to the
// host's own row type with an empty `supersedes`), a maker's key reaches it as the resolved id,
// and `^s` there is still the object document's -- beside the current Editor, which declares
// version two and owns that action. Reloaded in place through the same path, it keeps its id
// and its state and declares again. The suites that drive it are `test_workshop_panes_actions.cpp`
// (the real Workshop) and `test_workshop_load.cpp` (the real reload).
//
// ⚠ IT MUST NOT INCLUDE THE CURRENT PANE VOCABULARY, and a case reads this file to say so. The
// whole claim is that the shapes it derives come from a header that never saw version two; an
// include of `workshop/pane_vocabulary.hpp` here would make the image a build of the current
// protocol wearing an old name.
//
// IT IS A FIXTURE AND NOT A PRODUCT, on the Hello pane's terms: built by `tests/`, loaded by
// two suites, named in no host's boot list, and granted `allow_any()` by the loader like every
// in-process image.

#include "legacy_pane_protocol.hpp"

#include "activation/activation.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>

namespace {

namespace surface = zengine::surface;
namespace input = zengine::input;
using legacy_protocol::PaneActionRequested;
using legacy_protocol::PaneActionRow;
using legacy_protocol::PaneActions;
using legacy_protocol::PaneCatalogRequested;
using legacy_protocol::PaneContent;
using legacy_protocol::PaneOffered;
using legacy_protocol::PaneRoom;

/// THIS PROVIDER'S OFFICE -- the durable half of the `PaneRef` a maker's setup names it by.
constexpr const char* kOffice = "zengine.test.legacy";

/// The office Workshop holds, as a string: a provider is a stranger to Workshop's internals.
constexpr const char* kWorkshopRole = "zengine.workshop";

constexpr const char* kPaneKey = "old";
constexpr const char* kPaneName = "Old";
constexpr const char* kPaneSummary = "a pane from before ownership";

/// WHAT THIS IMAGE COUNTS -- and a reload keeps, which is how a case tells the reloaded
/// incarnation from a freshly constructed one: `acted` goes 1, 2 across the swap.
struct LegacyState {
    std::int64_t offers = 0;
    std::int64_t rooms = 0;
    std::int64_t acted = 0;
    std::string last_action;
    ZEN_EXPOSE();
    ZEN_SHAPE(LegacyState, 1, ZEN_FIELD(offers), ZEN_FIELD(rooms), ZEN_FIELD(acted),
              ZEN_FIELD(last_action));
};

class LegacyPaneWeave
    : public loom::WeaveBase<LegacyPaneWeave, LegacyState,
                             loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom,
                                          PaneActionRequested>,
                             loom::Emit<PaneOffered, PaneActions, PaneContent>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != kPaneKey) {
            return;
        }
        ++state_.rooms;
        rows_ = room.rows;
        columns_ = room.columns;
        say(mail);
    }

    /// A MAKER PRESSED ONE OF THIS PANE'S ROWS' KEYS, and the host resolved it to the id this
    /// image declared -- version one's whole promise, kept for an image nobody rebuilt.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != kPaneKey) {
            return;
        }
        ++state_.acted;
        state_.last_action = asked.id;
        say(mail);
    }

private:
    /// One offer and one version-one declaration, authored as this office.
    void announce(loom::Mail& mail) {
        ++state_.offers;
        (void)mail.as_role(kOffice).send_to_role(kWorkshopRole,
                                                 PaneOffered{kPaneKey, kPaneName, kPaneSummary});
        PaneActions actions;
        actions.pane = kPaneKey;
        actions.rows.push_back(PaneActionRow{"old.up", "row up", input::scan::kUp, 0});
        actions.rows.push_back(PaneActionRow{"old.mark", "mark", input::scan::kM, 0});
        (void)mail.as_role(kOffice).send_to_role(kWorkshopRole, actions);
    }

    /// Two rows: what this pane is, and what it was last asked to do -- so a case reads the
    /// dispatch off the presentation a maker would see.
    void say(loom::Mail& mail) {
        if (rows_ <= 0) {
            return;
        }
        PaneContent said;
        said.pane = kPaneKey;
        said.rows.push_back(surface::SurfaceTextRow{
            fitted("an old pane -- " + std::to_string(rows_) + "x" + std::to_string(columns_)),
            surface::role::kFill});
        if (rows_ > 1) {
            said.rows.push_back(surface::SurfaceTextRow{
                fitted(state_.acted == 0 ? std::string("acted 0")
                                         : "acted " + std::to_string(state_.acted) + ": " +
                                               state_.last_action),
                surface::role::kMuted});
        }
        (void)mail.as_role(kOffice).send_to_role(kWorkshopRole, said);
    }

    std::string fitted(std::string text) const {
        if (columns_ <= 0) {
            return std::string();
        }
        if (static_cast<std::int64_t>(text.size()) > columns_) {
            text.resize(static_cast<std::size_t>(columns_));
        }
        return text;
    }

    zengine::ActivationCursor activation_;
    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
};

} // namespace

ZEN_EXPORT_WEAVE(LegacyPaneWeave)
