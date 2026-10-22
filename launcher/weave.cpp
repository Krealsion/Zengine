// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The loadable weave `zengine-launcher`, office `zengine.launcher`: starts an app or a demo from
// a catalog the configuration fixed, detached, and keeps nothing of it. Asked only by the View
// Builder and Files, which ask Workshop first whether the hand that pressed may.
// Reference: launcher/docs/zengine.launcher.md, its manual, compiled in and answered.

#include "launcher/catalog.hpp"
#include "launcher/configured.hpp"
#include "launcher/start.hpp"
#include "launcher/vocabulary.hpp"

#include "activation/activation.hpp"
#include "app/program.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.launcher.hpp" // this office's manual, compiled in from its page
#include "workshop/pane_seam_vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <iterator>
#include <string>

namespace {

namespace launcher = zengine::launcher;
namespace ws = zengine::workshop;

/// The offices that may ask for a start: each asks Workshop first for the hand that pressed.
constexpr const char* kAskers[] = {"zengine.view.builder", "zengine.files"};

struct LauncherState {
    ZEN_SHAPE(LauncherState, 1);
};

/// The catalog as this build was configured, the app host beside the running program.
launcher::Catalog configured_catalog() {
    namespace c = launcher::configured;
    launcher::Catalog out{zengine::app::app_program(), c::kPython, c::kDemoScript, c::kBuild,
                          c::kLoomPrefix, {}};
    for (const auto& [name, dir] : c::demo_setups()) {
        out.demos.push_back(launcher::DemoSetup{name, dir});
    }
    return out;
}

class Launcher
    : public zengine::With<
          zengine::manual::ManualFacet<zengine::manual::pages::zengine_launcher::kManual>>::
          Weave<Launcher, LauncherState,
                loom::Accept<launcher::Launch, ws::ProjectRoot, loom::Activated>,
                loom::Emit<launcher::Launched, ws::ProjectRootRequested>> {
public:
    /// Started: ask where the project is, which every app and demo starts inside.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (activation_.accept(mail, a)) {
            (void)mail.as_role(launcher::kLauncherRole)
                .send_to_role(ws::kProjectRole, ws::ProjectRootRequested{});
        }
    }

    /// Where the project is, in the answer to this weave's own ask.
    void on(const ws::ProjectRoot& said, loom::Mail& mail) {
        if (mail.answers_ask()) {
            project_ = said.project_dir;
            known_ = true;
        }
    }

    /// A START ASKED: judged against the catalog, then started detached and kept by nobody; the
    /// answer names its process, or why nothing started.
    void on(const launcher::Launch& asked, loom::Mail& mail) {
        launcher::Launched answer{false, asked.kind, asked.name, 0, {}};
        const bool asker = std::any_of(std::begin(kAskers), std::end(kAskers),
                                       [&mail](const char* office) {
                                           return mail.authored_from_role(office);
                                       });
        if (!asker) {
            answer.reason = "only the View Builder and Files ask the launcher";
        } else if (!known_) {
            answer.reason = "the launcher does not know the project yet; ask again";
        } else {
            const launcher::LaunchJudged judged =
                launcher::judge_launch(asked, project_, configured_catalog());
            if (!judged.ok) {
                answer.reason = judged.refusal;
            } else {
                const launcher::Started started = launcher::start_detached(judged.plan);
                answer.ok = started.ok;
                answer.pid = started.pid;
                answer.reason = started.ok ? "started " + asked.kind + " '" + asked.name +
                                                 "', process " + std::to_string(started.pid)
                                           : started.trouble;
            }
        }
        (void)mail.answer(answer);
    }

private:
    zengine::ActivationCursor activation_;
    std::string project_;
    bool known_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(Launcher)
