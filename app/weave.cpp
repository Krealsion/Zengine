// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-app-weave, the office `zengine.app`: an app's own weave, its runner and its arranger
// (app/docs/app.md). It asks the host where the app is, reads `app.json`, runs each flow through
// the Flow host and each view through the view host as its office, and arranges the window that
// offers itself: the skin offered by the host's path and chosen, each view's area placed to hear
// the window's keys, and a line of its own at the window's foot where it says what went wrong.

#include "app/files.hpp"
#include "app/vocabulary.hpp"

#include "activation/activation.hpp"
#include "flow/flow-host/vocabulary.hpp"
#include "graphics/drawing.hpp"
#include "graphics/vocabulary.hpp"
#include "view/description.hpp"
#include "view/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace app = zengine::app;
namespace gfx = zengine::graphics;
namespace view = zengine::view;
namespace flow_host = zengine::flow_host;

/// The platform's loadable-library suffix, which a skin's painter code is spelled with in the
/// host's artifact directory.
constexpr const char* kLibrarySuffix =
#if defined(_WIN32)
    ".dll";
#elif defined(__APPLE__)
    ".dylib";
#else
    ".so";
#endif

/// The painters a window opens by path, by the word `app.json` names one with.
constexpr const char* kSkins[] = {"day", "night", "classic", "block"};

/// The app's own line, at the window's foot: its area, which is its office.
constexpr const char* kStatusArea = app::kAppRole;

struct AppWeaveState {
    std::string status; ///< what went wrong, said on the app's own line; empty when nothing did
    ZEN_EXPOSE();
    ZEN_SHAPE(AppWeaveState, 1, ZEN_FIELD(status));
};

/// One view the app runs, as it stands in the window.
struct Placed {
    std::string area; ///< the view's name, which is its office and its area
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t notice_rows = 0;
};

class AppWeave
    : public gfx::With<gfx::DrawingFacet>::Weave<
          AppWeave, AppWeaveState,
          loom::Accept<loom::Activated, app::AppPlaceSaid, gfx::GraphicsReady, gfx::WindowOffered,
                       flow_host::FlowAnswer, view::ViewAnswer>,
          loom::Emit<app::AppPlaceAsked, flow_host::FlowRun, view::ViewRun, gfx::AreaPlaced,
                     gfx::SkinOffered, gfx::SkinChosen, gfx::WindowShaped>> {
public:
    using Weave::on;

    AppWeave() : Weave(app::kAppRole, kStatusArea) {}

    /// Started: ask the host where the app is. A weave started again asks again, and runs nothing
    /// twice: the hosts refuse a session that already runs, and say so on its line.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        (void)mail.as_role(app::kAppRole).send_to_role(app::kAppHostRole, app::AppPlaceAsked{});
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

    /// A window offered itself, as its office: the first is the app's, and its size places the
    /// app's line at its foot.
    void on(const gfx::WindowOffered& w, loom::Mail& mail) {
        if (!mail.authored_from_role(std::string(gfx::kWindowOfficePrefix) + w.window) ||
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
            .send_to_role(std::string(gfx::kWindowOfficePrefix) + r.window,
                          gfx::WindowShaped{r.window, w.title, w.width, w.height});
    }

    void picture(gfx::Sketch& f) const {
        if (!state_.status.empty()) {
            f.text("status", 0, 0, state_.status, gfx::by_role(gfx::role::kAccent));
        }
    }

private:
    void say(std::string words) { state_.status = "[app] " + std::move(words); }

    /// Each flow, then each view, as this office: the flow first, so the view's intent has a
    /// listener when it is first said. A file that cannot be read is said and skipped.
    void run(loom::Mail& mail) {
        for (const app::ZengineAppFlow& f : file_.flows) {
            const app::ReadBytes bytes = app::read_bytes(place_.folder + "/" + f.file);
            if (!bytes.ok) {
                say(bytes.refusal);
                continue;
            }
            (void)mail.as_role(app::kAppRole)
                .send_to_role(flow_host::kFlowHostRole, flow_host::FlowRun{f.session, bytes.bytes});
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
            views_.push_back(Placed{d.name, v.x, v.y, d.width, d.height,
                                    static_cast<std::int64_t>(view::kNoticeRows)});
            (void)mail.as_role(app::kAppRole)
                .send_to_role(view::kViewHostRole, view::ViewRun{v.session, bytes.bytes});
        }
    }

    /// The window as the app arranges it, once the directory, a window and the app's own file are
    /// all known: its skin offered by the host's path, each view's area at its place, its size and
    /// its notice rows in the window's text, hearing the window's keys; the app's line across the
    /// window's foot; and the skin chosen for each.
    void arrange(loom::Mail& mail) {
        if (!directory_ || window_.window.empty() || !told_) {
            return;
        }
        const auto to_directory = [&mail](const auto& said) {
            (void)mail.as_role(app::kAppRole).send_to_role(gfx::kGraphicsRole, said);
        };
        // The skin app.json names, or, where it names none a window paints with or could not be
        // read, the window's own kind of painter, so the app's line can still say why.
        const std::string& named = file_.window.skin;
        const bool known = std::find(std::begin(kSkins), std::end(kSkins), named) != std::end(kSkins);
        const std::string skin =
            known ? named : (window_.window == gfx::kTerminalWindow ? "classic" : "day");
        if (read_ && !known) {
            say("app.json names the skin '" + named + "'; a window paints with day, night, " +
                "classic or block");
        }
        to_directory(
            gfx::SkinOffered{skin, place_.artifacts + "/zengine-paint-" + skin + kLibrarySuffix});
        const std::int64_t line = window_.text_line_px > 0 ? window_.text_line_px : gfx::kCellPx;
        std::vector<std::string> areas;
        for (const Placed& v : views_) {
            gfx::AreaPlaced placed{v.area, window_.window, v.area, v.x, v.y, v.w,
                                   v.h + v.notice_rows * line, 1, true};
            placed.pointer = false;
            to_directory(placed);
            areas.push_back(v.area);
        }
        to_directory(gfx::AreaPlaced{kStatusArea, window_.window, app::kAppRole, 0,
                                     std::max<std::int64_t>(0, window_.height - line),
                                     window_.width, line, 2, false, false});
        areas.push_back(kStatusArea);
        for (const std::string& area : areas) {
            to_directory(gfx::SkinChosen{area, skin});
        }
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
};

} // namespace

ZEN_EXPORT_WEAVE(AppWeave)
