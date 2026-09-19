// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Connections pane -- a loadable weave that offers Workshop one pane: the other hosts
// connected to it, as the guest door reports them.
//
// (!) IT DERIVES NOTHING. Every row is the guest door's reading of its own server -- the
// connection's number, its state, what the peer claimed, what the policy established, where it
// came from, and its session on this bus. The door publishes the inventory whole whenever it
// changes and answers a fresh presenter that asks, so this pane holds the last word and nothing
// older; a connection that closed is in the next publication as `closed` once, and gone after
// the door reaps it, which is how a dead session never sits here looking live.
//
// (!) IT GRANTS NOTHING AND DECIDES NOTHING. A row saying `awaiting-decision` is a fact a maker
// can see; the decision is the door's seam and the popup that will make it is later work. This
// pane declares no actions, so it takes no keys and answers no gesture beyond being seated.

#include "connections-pane/vocabulary.hpp"

#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "activation/activation.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::connections_pane;

using ws::GuestConnection;
using ws::GuestConnections;
using ws::GuestConnectionsRequested;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneOffered;
using ws::PaneRoom;

/// WHO THIS PANE IS TALKING TO: the host's office, and the door's, spelled as a stranger
/// spells them.
constexpr const char* kWorkshopRole = "zengine.workshop";

using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::omitted_text;

class ConnectionsPaneWeave
    : public loom::WeaveBase<ConnectionsPaneWeave, pane::ConnectionsPaneState,
                             loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom,
                                          GuestConnections>,
                             loom::Emit<PaneOffered, PaneContent, GuestConnectionsRequested>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
        ask(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kConnectionsPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        say(mail);
    }

    /// THE INVENTORY, SAID BY THE DOOR. Replaced whole; an answer to this pane's own ask and
    /// the door's publication are the same reading and are treated the same.
    void on(const GuestConnections& said, loom::Mail& mail) {
        if (!mail.authored_from_role(ws::kGuestsRole) && !mail.answers_ask()) {
            return; // a stranger's opinion about who is connected is not the door's reading
        }
        known_ = said;
        heard_ = true;
        say(mail);
    }

private:
    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kConnectionsPaneRole)
            .send_to_role(kWorkshopRole,
                          PaneOffered{pane::kConnectionsPane, pane::kConnectionsPaneName,
                                      pane::kConnectionsPaneSummary});
    }

    /// A presenter that just arrived asks the door once, so it need not wait for a change.
    void ask(loom::Mail& mail) {
        (void)mail.as_role(pane::kConnectionsPaneRole)
            .send_to_role(ws::kGuestsRole, GuestConnectionsRequested{});
    }

    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        const auto push = [&out, this](const std::string& text, std::int64_t role) {
            out.push_back(surface::SurfaceTextRow{drawable(fit(text, columns_)), role});
        };
        if (!heard_) {
            push("CONNECTIONS (waiting for the guest door)", surface::role::kMuted);
        } else if (known_.listen.empty()) {
            push("CONNECTIONS -- this Workshop accepts no guests", surface::role::kAccent);
            push("  launch with --guests <file> to listen", surface::role::kMuted);
        } else {
            push("CONNECTIONS -- " + std::to_string(known_.rows.size()) +
                     (known_.rows.size() == 1 ? " connection" : " connections") + " at " +
                     known_.listen,
                 surface::role::kAccent);
            const std::size_t budget = rows_ > 1 ? static_cast<std::size_t>(rows_ - 1) : 0;
            if (known_.rows.empty() && budget > 0) {
                push("  nobody is connected", surface::role::kMuted);
            }
            std::size_t shown = 0;
            for (const GuestConnection& c : known_.rows) {
                if (shown + 1 >= budget && known_.rows.size() - shown > 1) {
                    push("  " + omitted_text(known_.rows.size() - shown, "more"),
                         surface::role::kMuted);
                    break;
                }
                push("  " + row_text(c), role_of(c));
                ++shown;
            }
            if (budget > shown + 1 && (known_.refused > 0 || known_.shed > 0)) {
                push("  refused " + std::to_string(known_.refused) + ", shed " +
                         std::to_string(known_.shed) + " (all time)",
                     surface::role::kMuted);
            }
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        ++state_.said;
        (void)mail.as_role(pane::kConnectionsPaneRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kConnectionsPane, std::move(out)});
    }

    /// ONE ROW: the host's word first, the peer's claim beside it, then where it stands.
    static std::string row_text(const GuestConnection& c) {
        std::string s = "#" + std::to_string(c.connection) + " ";
        if (!c.established.empty()) {
            s += c.established;
            if (!c.claimed.empty() && c.claimed != c.established) {
                s += " (claims '" + c.claimed + "')";
            }
        } else if (!c.claimed.empty()) {
            s += "'" + c.claimed + "' (unverified)";
        } else {
            s += "(unnamed)";
        }
        s += "  " + c.state;
        if (c.session != 0) {
            s += "  weave " + std::to_string(c.session);
        }
        if (!c.peer.empty()) {
            s += "  from " + c.peer;
        }
        if (!c.refusal.empty()) {
            s += "  -- " + c.refusal;
        }
        return s;
    }

    static std::int64_t role_of(const GuestConnection& c) {
        if (c.state == "admitted") {
            return surface::role::kFill;
        }
        if (c.state == "awaiting-decision") {
            return surface::role::kAlert;
        }
        return surface::role::kMuted;
    }

    zengine::ActivationCursor activation_;
    GuestConnections known_;
    bool heard_ = false;
    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(ConnectionsPaneWeave)
