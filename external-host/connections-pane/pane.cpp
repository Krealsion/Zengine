// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Connections pane: a loadable weave that offers Workshop one pane, the other hosts
// connected to it as the guest door reports them. It derives nothing: every row is the door's
// reading (number, state, what the peer claimed, what policy established, origin, session, the
// admitting row's version and what its powers do not reach here), published whole on each
// change, so a closed connection shows `closed` once and is gone once reaped. It grants and
// decides nothing, and declares no actions: `awaiting-decision` is a fact.
// Pane law: agents/panes.md

#include "external-host/connections-pane/vocabulary.hpp"

#include "workshop/guest_seam_vocabulary.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"

#include "graphics/vocabulary.hpp"

#include "manuals/zengine.connections.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace gfx = zengine::graphics;
namespace ws = zengine::workshop;
namespace pane = zengine::connections_pane;

using ws::v3::GuestConnection;
using ws::v3::GuestConnections;
using ws::v3::GuestConnectionsRequested;
using ws::v2::PaneOffered;

/// THE GUESTS FILE'S WORD for a host whose guests write and build as its weaver does.
constexpr const char* kDevelopmentHost = "development";

using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::omitted_text;

/// The pane's room and its one picture: in a graphics window, and in Workshop on its canvas while
/// it holds one and as prose to a host granting none. Nothing in it is a part: a press is nothing
/// here, and a right press in Workshop is handed back, so Workshop's own pane menu opens where it
/// was made.
using Drawing = ws::PaneDrawingFacet<pane::kConnectionsPane, pane::kConnectionsPaneRole,
                                     zengine::manual::pages::zengine_connections::kManual>;

class ConnectionsPaneWeave
    : public zengine::With<Drawing>::Weave<ConnectionsPaneWeave, pane::ConnectionsPaneState,
                                           loom::Accept<GuestConnections>,
                                           loom::Emit<GuestConnectionsRequested>> {
public:
    PaneOffered offering() const {
        return PaneOffered{pane::kConnectionsPane, pane::kConnectionsPaneName,
                           pane::kConnectionsPaneSummary, 7, 62};
    }

    /// Each activation offers the pane and asks the door.
    void on_activated(loom::Mail& mail) {
        room().announce(*this, mail);
        ask(mail);
    }

    /// The room the window gave the pane's area: the pane says its rows in it. A refused drawing
    /// leaves the last good one showing, and the next reading draws again.
    void on_room(loom::Mail& mail) { say(mail); }

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
    /// A presenter that just arrived asks the door once, so it need not wait for a change, and
    /// at version 3, so each row's powers, losses, role and scopes come with it.
    void ask(loom::Mail& mail) {
        (void)mail.as_role(pane::kConnectionsPaneRole)
            .send_to_role(ws::kGuestsRole, GuestConnectionsRequested{});
    }

    Drawing& room() { return this->template facet<Drawing>(); }
    const Drawing& room() const { return this->template facet<Drawing>(); }

    /// The picture shown, wherever the pane stands, once it holds a room.
    void say(loom::Mail& mail) {
        if (!room().granted() || room().rows() <= 0 || room().columns() <= 0) {
            return;
        }
        ++state_.said;
        room().show(*this, mail);
    }

public:
    /// WHAT THE PANE SHOWS, a line of the room's text each, named for what it says; a connection's
    /// lines stand under `rows/<n>`, the n-th connection said. A row past the room is not drawn.
    void picture(gfx::Sketch& f) const {
        const std::int64_t rows = room().rows(), columns = room().columns();
        const std::int64_t height = room().text().line;
        std::int64_t row = 0;
        const auto line = [&](const std::string& path, const std::string& text,
                              std::int64_t role) {
            if (row < rows) {
                f.text(path, 0, row * height, drawable(fit(text, columns)),
                       gfx::by_role(role));
                ++row;
            }
        };
        if (!heard_) {
            line("waiting", "CONNECTIONS (waiting for the guest door)", gfx::role::kMuted);
            return;
        }
        if (known_.listen.empty()) {
            line("header", "CONNECTIONS -- this Workshop accepts no guests",
                 gfx::role::kAccent);
            line("listen", "  launch with --guests <file> to listen", gfx::role::kMuted);
            return;
        }
        line("header",
             "CONNECTIONS -- " + std::to_string(known_.rows.size()) +
                 (known_.rows.size() == 1 ? " connection" : " connections") + " at " +
                 known_.listen + (development_host() ? " -- a development host" : ""),
             gfx::role::kAccent);
        const std::size_t budget = rows > 1 ? static_cast<std::size_t>(rows - 1) : 0;
        if (known_.rows.empty() && budget > 0) {
            line("nobody", "  nobody is connected", gfx::role::kMuted);
        }
        // A CONNECTION IS SAID WHOLE OR COUNTED: one a guests-file row admitted brings its
        // version line, and its reach line when it names a role or doors; they stand together
        // or not at all.
        std::size_t shown = 0; // connections said
        std::size_t used = 0;  // the lines they and the count took
        for (const GuestConnection& c : known_.rows) {
            const std::size_t need = 1u + (c.version > 0 ? 1u : 0u) + (c.scopes.empty() ? 0u : 1u);
            const bool last = known_.rows.size() - shown == 1;
            if (last ? used + need > budget : used + need >= budget) {
                line("more", "  " + omitted_text(known_.rows.size() - shown, "more"),
                     gfx::role::kMuted);
                ++used;
                break;
            }
            if (shown == 0) {
                f.group("rows", 0, 0);
            }
            const std::string at = "rows/" + std::to_string(shown);
            line(at, "  " + row_text(c), role_of(c));
            if (c.version > 0) {
                line(at + "/version", "    " + version_text(c), role_of(c));
            }
            if (!c.scopes.empty()) {
                line(at + "/scopes", "    " + scopes_text(c), role_of(c));
            }
            used += need;
            ++shown;
        }
        if (budget > used + 1 && (known_.refused > 0 || known_.shed > 0)) {
            line("refused",
                 "  refused " + std::to_string(known_.refused) + ", shed " +
                     std::to_string(known_.shed) + " (all time)",
                 gfx::role::kMuted);
        }
    }

private:

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

    /// BENEATH A CONNECTION A GUESTS-FILE ROW ADMITTED: the file's version, and what the row's
    /// powers do not reach on this host, in the door's own words.
    static std::string version_text(const GuestConnection& c) {
        std::string s = "version " + std::to_string(c.version) + " -- ";
        if (c.losses.empty()) {
            return s + "all its powers reach here";
        }
        s += "not here: ";
        for (std::size_t i = 0; i < c.losses.size(); ++i) {
            s += (i == 0 ? "" : "; ") + c.losses[i];
        }
        return s;
    }

    /// ...AND WHERE IT READS AND POINTS, its role and doors, in the door's own words.
    static std::string scopes_text(const GuestConnection& c) {
        std::string s;
        for (const std::string& scope : c.scopes) s += (s.empty() ? "" : "; ") + scope;
        return s;
    }

    /// Whether the door's reading names a development host: the guests file's `host`, which every
    /// connection a guests-file row admitted carries.
    bool development_host() const {
        for (const GuestConnection& c : known_.rows) {
            if (c.host == kDevelopmentHost) {
                return true;
            }
        }
        return false;
    }

    static std::int64_t role_of(const GuestConnection& c) {
        if (c.state == "admitted") {
            return gfx::role::kFill;
        }
        if (c.state == "awaiting-decision") {
            return gfx::role::kAlert;
        }
        return gfx::role::kMuted;
    }

    GuestConnections known_;
    bool heard_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(ConnectionsPaneWeave)
