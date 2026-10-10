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
#include "workshop/pane_canvas_rows.hpp"
#include "workshop/pane_menu.hpp"
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

using ws::v2::GuestConnection;
using ws::v2::GuestConnections;
using ws::v2::GuestConnectionsRequested;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::v2::PaneOffered;
using ws::PaneRoom;

/// WHO THIS PANE IS TALKING TO: the host's office, and the door's, spelled as a stranger
/// spells them.
constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE GUESTS FILE'S WORD for a host whose guests write and build as its weaver does.
constexpr const char* kDevelopmentHost = "development";

using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::omitted_text;

class ConnectionsPaneWeave
    : public loom::WeaveBase<ConnectionsPaneWeave, pane::ConnectionsPaneState,
                             loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom,
                                          ws::PaneCanvasRoom, ws::PaneCanvasPointer,
                                          ws::PaneCanvasRejected, GuestConnections>,
                             loom::Emit<PaneOffered, PaneContent, ws::v5::PaneCanvasContent,
                                        ws::PanePassRequested, GuestConnectionsRequested>> {
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
        prose_rows_ = room.rows;
        prose_columns_ = room.columns;
        granted_ = true;
        fit_room();
        say(mail);
    }

    /// THE PANE'S OWN CANVAS: while it holds a room there it draws its rows as its picture, and
    /// says them as prose only to a host granting none.
    void on(const ws::PaneCanvasRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kConnectionsPane) {
            return;
        }
        canvas_ = room;
        granted_ = true;
        fit_room();
        say(mail);
    }

    /// A press means nothing here; a right press is handed back, so Workshop's own pane menu
    /// opens where it was made.
    void on(const ws::PaneCanvasPointer& press, loom::Mail& mail) {
        if (mail.authored_from_role(kWorkshopRole) && press.pane == pane::kConnectionsPane &&
            press.grant == canvas_.grant && press.phase == ws::canvas_pointer::kPress &&
            press.button == 3) {
            (void)ws::pane_menu::pass_back(mail, pane::kConnectionsPaneRole,
                                           pane::kConnectionsPane);
        }
    }

    /// A refused picture leaves the last good one showing, and the next reading draws again.
    void on(const ws::PaneCanvasRejected&, loom::Mail&) {}

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
                                      pane::kConnectionsPaneSummary, 7, 62});
    }

    /// A presenter that just arrived asks the door once, so it need not wait for a change, and
    /// at version 2, so each row's powers and losses come with it.
    void ask(loom::Mail& mail) {
        (void)mail.as_role(pane::kConnectionsPaneRole)
            .send_to_role(ws::kGuestsRole, GuestConnectionsRequested{});
    }

    bool on_canvas() const {
        return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0;
    }

    /// The rows and columns the pane composes for: its canvas's lattice while it holds one.
    void fit_room() {
        const ws::CanvasRows lattice = ws::canvas_rows(canvas_);
        rows_ = on_canvas() ? lattice.rows : prose_rows_;
        columns_ = on_canvas() ? lattice.columns : prose_columns_;
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
                     known_.listen + (development_host() ? " -- a development host" : ""),
                 surface::role::kAccent);
            const std::size_t budget = rows_ > 1 ? static_cast<std::size_t>(rows_ - 1) : 0;
            if (known_.rows.empty() && budget > 0) {
                push("  nobody is connected", surface::role::kMuted);
            }
            // A CONNECTION IS SAID WHOLE OR COUNTED: one a guests-file row admitted brings its
            // version line, and the two stand together or not at all.
            std::size_t shown = 0; // connections said
            std::size_t used = 0;  // the lines they and the count took
            for (const GuestConnection& c : known_.rows) {
                const std::size_t need = c.version > 0 ? 2 : 1;
                const bool last = known_.rows.size() - shown == 1;
                if (last ? used + need > budget : used + need >= budget) {
                    push("  " + omitted_text(known_.rows.size() - shown, "more"),
                         surface::role::kMuted);
                    ++used;
                    break;
                }
                push("  " + row_text(c), role_of(c));
                if (c.version > 0) {
                    push("    " + version_text(c), role_of(c));
                }
                used += need;
                ++shown;
            }
            if (budget > used + 1 && (known_.refused > 0 || known_.shed > 0)) {
                push("  refused " + std::to_string(known_.refused) + ", shed " +
                         std::to_string(known_.shed) + " (all time)",
                     surface::role::kMuted);
            }
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        ++state_.said;
        if (on_canvas()) {
            (void)mail.as_role(pane::kConnectionsPaneRole)
                .send_to_role(kWorkshopRole,
                              ws::rows_picture(canvas_, pictures_.next(canvas_, 0), out));
            return;
        }
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
    std::int64_t rows_ = 0; ///< the room composed for: the canvas lattice's, else the prose room's
    std::int64_t columns_ = 0;
    std::int64_t prose_rows_ = 0, prose_columns_ = 0;
    ws::PaneCanvasRoom canvas_;
    ws::CanvasPictures pictures_;
    bool granted_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(ConnectionsPaneWeave)
