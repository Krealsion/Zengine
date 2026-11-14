// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// zengine-app-weave, the role `zengine.app`: an app's own weave, its runner and its arranger
// (app/docs/app.md). It asks the host where the app is, reads `app.json`, runs each flow through
// the Flow host and each view through the view host as its role, and arranges the window that
// offers itself: the palette its file names offered and chosen, each view's area placed to hear
// the window's keys, and a line of its own at the window's foot where it says what went wrong.

#include "app/files.hpp"
#include "app/vocabulary.hpp"

#include "activation/activation.hpp"
#include "flow/flow-host/vocabulary.hpp"
#include "graphics/drawing.hpp"
#include "graphics/themes.hpp" // generated: the shipped themes, as palettes
#include "graphics/vocabulary.hpp"
#include "view/description.hpp"
#include "view/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_ended.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace app = zengine::app;
namespace gfx = zengine::graphics;
namespace view = zengine::view;
namespace flow_host = zengine::flow_host;

/// The palette of the shipped theme `app.json` names, if it names one.
std::optional<gfx::PaletteOffered> shipped_palette(const std::string& name) {
    for (gfx::PaletteOffered& p : gfx::themes::shipped()) {
        if (p.palette == name) {
            return std::move(p);
        }
    }
    return std::nullopt;
}

/// The app's own line, at the window's foot: its area, which is its role.
constexpr const char* kStatusArea = app::kAppRole;

struct AppWeaveState {
    std::string status; ///< what went wrong, said on the app's own line; empty when nothing did
    ZEN_EXPOSE();
    ZEN_SHAPE(AppWeaveState, 1, ZEN_FIELD(status));
};

/// One view the app runs, as it stands in the window.
struct Placed {
    std::string view; ///< the view's name, which is its role
    std::string area; ///< the area it draws in, its pane's: `<name>.view`
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t notice_rows = 0;
};

class AppWeave
    : public gfx::With<gfx::DrawingFacet>::Weave<
          AppWeave, AppWeaveState,
          loom::Accept<loom::Activated, app::AppPlaceSaid, gfx::GraphicsReady, gfx::WindowOffered,
                       flow_host::FlowAnswer, view::ViewAnswer, loom::AskEnded>,
          loom::Emit<app::AppPlaceAsked, flow_host::FlowRun, view::ViewRun, gfx::AreaPlaced,
                     gfx::PaletteOffered, gfx::PaletteChosen, gfx::WindowShaped>> {
public:
    using Weave::on;

    AppWeave() : Weave(app::kAppRole, kStatusArea) {}

    /// Started: ask the host where the app is. A weave started again asks again, and runs nothing
    /// twice: the hosts refuse a session that already runs, and say so on its line.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        asked_.clear();
        ask(mail, app::kAppHostRole, app::AppPlaceAsked{}, "where the app is was not told");
    }

    /// Where the app is, in the answer Loom attests to this weave's own ask of the place door: the
    /// app read and run, then arranged.
    void on(const app::AppPlaceSaid& place, loom::Mail& mail) {
        if (!mail.answers_ask() || told_) {
            return;
        }
        told_ = true;
        place_ = place;
        const app::ReadApp read = app::read_app(place.folder + "/" + app::kAppFileName);
        if (!read.ok) {
            say(read.refusal);
            return;
        }
        file_ = read.app;
        read_ = true;
        run(mail);
        arrange(mail);
    }

    /// A graphics directory started, afresh or again: everything is placed with it again.
    void on(const gfx::GraphicsReady&, loom::Mail& mail) {
        if (!mail.authored_from_role(gfx::kGraphicsRole)) {
            return;
        }
        directory_ = true;
        arrange(mail);
    }

    /// A window offered itself, as its role: the first is the app's, and its size places the
    /// app's line at its foot.
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        if (!mail.authored_from_role(std::string(gfx::kWindowRolePrefix) + w.window) ||
            (!window_.window.empty() && w.window != window_.window)) {
            return;
        }
        window_ = w;
        arrange(mail);
    }

    /// The hosts' answers to this weave's own asks: a refusal is said on the app's line, in their
    /// words.
    void on(const flow_host::FlowAnswer& a, loom::Mail& mail) {
        if (mail.answers_ask() && !a.ok) {
            say("the flow '" + a.session + "' did not run: " + a.reason);
        }
    }
    void on(const view::ViewAnswer& a, loom::Mail& mail) {
        if (mail.answers_ask() && !a.ok) {
            say("the view '" + a.session + "' did not run: " + a.reason);
        }
    }

    /// Loom's word that one of this weave's asks ended with no answer, said on the app's line.
    void on(const loom::AskEnded& ended, loom::Mail& mail) {
        if (!mail.ask_ended()) {
            return;
        }
        for (auto at = asked_.begin(); at != asked_.end(); ++at) {
            if (at->first == ended.ended_attempt().seq) {
                say(at->second + ": " + (ended.role.empty() ? ended.target : ended.role) +
                    " never answered (" + loom::stage_name(ended.stage) + ", " +
                    std::string(loom::reason_name(ended.stage)) + ")");
                asked_.erase(at);
                return;
            }
        }
    }

    /// THE APP'S LINE GIVEN ITS ROOM in the app's window: the window holds this weave as an
    /// arranger now, so it is told `app.json`'s title and size, once each incarnation of it.
    void on_room(const gfx::AreaRoom& r, loom::Mail& mail) {
        if (!read_ || r.window != window_.window || shaped_epoch_ == r.epoch) {
            return;
        }
        shaped_epoch_ = r.epoch;
        const app::ZengineAppWindow& w = file_.window;
        if (w.title.empty() && w.width <= 0 && w.height <= 0) {
            return;
        }
        (void)mail.as_role(app::kAppRole)
            .send_to_role(std::string(gfx::kWindowRolePrefix) + r.window,
                          gfx::WindowShaped{r.window, w.title, w.width, w.height});
    }

    void picture(gfx::Sketch& f) const {
        if (!state_.status.empty()) {
            f.text("status", 0, 0, state_.status, gfx::colors::accent);
        }
    }

private:
    void say(std::string words) { state_.status = "[app] " + std::move(words); }

    /// Ask `role` as the app, keeping what to say should no answer come: each ask ends once, in
    /// its answer or Loom's word, so the record holds nothing for ever.
    template <class T>
    void ask(loom::Mail& mail, std::string_view role, const T& asked, std::string unanswered) {
        const loom::Ticket t = mail.as_role(app::kAppRole).ask_role(role, asked);
        if (t.valid()) {
            asked_.emplace_back(t.seq, std::move(unanswered));
        }
    }

    /// Each flow, then each view, as this role: the flow first, so the view's intent has a
    /// listener when it is first said. A file that cannot be read is said and skipped.
    void run(loom::Mail& mail) {
        for (const app::ZengineAppFlow& f : file_.flows) {
            const app::ReadBytes bytes = app::read_bytes(place_.folder + "/" + f.file);
            if (!bytes.ok) {
                say(bytes.refusal);
                continue;
            }
            ask(mail, flow_host::kFlowHostRole, flow_host::FlowRun{f.session, bytes.bytes},
                "the flow '" + f.session + "' did not run");
        }
        for (const app::ZengineAppView& v : file_.views) {
            const app::ReadBytes bytes = app::read_bytes(place_.folder + "/" + v.file);
            if (!bytes.ok) {
                say(bytes.refusal);
                continue;
            }
            const auto described = view::read_description(std::string_view(
                reinterpret_cast<const char*>(bytes.bytes.data()), bytes.bytes.size()));
            if (!described) {
                say("'" + v.file + "' is not a view: " + described.reason);
                continue;
            }
            const view::Description& d = described.description;
            views_.push_back(Placed{d.name, d.name + "." + view::kPane, v.x, v.y, d.width,
                                    d.height, static_cast<std::int64_t>(view::kNoticeRows)});
            ask(mail, view::kViewHostRole, view::ViewRun{v.session, bytes.bytes},
                "the view '" + v.session + "' did not run");
        }
    }

    /// The window as the app arranges it, once the directory, a window and the app's own file are
    /// all known: the palette its file names offered and chosen for every area, then each view's
    /// area at its place, its size and its notice rows in the window's text, hearing the window's
    /// keys and pointer, and the app's line across the window's foot.
    void arrange(loom::Mail& mail) {
        if (!directory_ || window_.window.empty() || !told_) {
            return;
        }
        const auto to_directory = [&mail](const auto& said) {
            (void)mail.as_role(app::kAppRole).send_to_role(gfx::kGraphicsRole, said);
        };
        // The theme app.json names, or, where it names none of the shipped themes or could not be
        // read, the one its window's medium reads best in, so the app's line can still say why.
        const std::string& named = file_.window.theme;
        std::optional<gfx::PaletteOffered> palette = shipped_palette(named);
        if (!palette) {
            palette = shipped_palette(window_.window == gfx::kTuiWindow ? "night" : "day");
            if (read_) {
                say("app.json names the theme '" + named + "'; a window wears night, day " +
                    "or block");
            }
        }
        to_directory(*palette);
        // Each area's palette before its place, so each is introduced once, wearing it.
        for (const Placed& v : views_) {
            to_directory(gfx::PaletteChosen{v.area, palette->palette});
        }
        to_directory(gfx::PaletteChosen{kStatusArea, palette->palette});
        const std::int64_t line = window_.text_line_px > 0 ? window_.text_line_px : gfx::kCellPx;
        for (const Placed& v : views_) {
            gfx::AreaPlaced view{v.area, window_.window, v.view, v.x, v.y, v.w,
                                 v.h + v.notice_rows * line, 1, true, true};
            view.text = true; // a view takes a key and its text, and no release
            to_directory(view);
        }
        to_directory(gfx::AreaPlaced{kStatusArea, window_.window, app::kAppRole, 0,
                                     std::max<std::int64_t>(0, window_.height - line),
                                     window_.width, line, 2, false, false});
    }

    zengine::ActivationCursor activation_;
    bool told_ = false;      ///< the host said where the app is
    bool read_ = false;      ///< app.json was read and admitted
    bool directory_ = false; ///< the graphics directory said it started
    std::int64_t shaped_epoch_ = -1; ///< the window's incarnation this weave last shaped
    app::AppPlaceSaid place_;
    app::ZengineAppFile file_;
    gfx::WindowOffered window_;
    std::vector<Placed> views_;
    /// This incarnation's asks not yet ended with no answer, by attempt: one a flow, a view and
    /// the place, so as many as app.json names.
    std::vector<std::pair<std::uint64_t, std::string>> asked_;
};

} // namespace

ZEN_EXPORT_WEAVE(AppWeave)
