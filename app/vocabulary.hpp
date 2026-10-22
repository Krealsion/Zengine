// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_APP_VOCABULARY_HPP
#define ZENGINE_APP_VOCABULARY_HPP

// The app host's words (app/docs/app.md): the offices it and an app's own weave hold, the two
// asks of the host's place door, and `app.json`, the file that says what an app runs. Portable:
// no kernel, no loader.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::app {

/// The office of the app's own weave (`zengine-app-weave`): it reads `app.json`, runs the app's
/// flows and views, and arranges the window.
inline constexpr const char* kAppRole = "zengine.app";

/// The office of the host's place door: where the app's folder and the artifacts are.
inline constexpr const char* kAppHostRole = "zengine.app.host";

/// The files an app's folder holds, by their names there.
inline constexpr const char* kPlanName = "app-plan.json";
inline constexpr const char* kAppFileName = "app.json";
inline constexpr const char* kAuthorityName = "app-authority.json";

/// The largest `app.json` read, in bytes: far above any app's, so a hostile file does not choose
/// the cost of refusing it.
inline constexpr std::uintmax_t kMaxAppFileBytes = 1u << 16;

/// The most views, and the most flows, one app runs.
inline constexpr std::size_t kMaxAppViews = 8;
inline constexpr std::size_t kMaxAppFlows = 8;

// ---- the place door ----------------------------------------------------------------------

/// Anyone -> `zengine.app.host`: where am I? Answered with `AppPlaceSaid`.
struct AppPlaceAsked {
    ZEN_SHAPE(AppPlaceAsked, 1);
};

/// `zengine.app.host` -> the asker: the app's folder, and the directory the host opens artifacts
/// from, a skin's painter code among them. Facts the host holds; a loaded weave cannot know where
/// it was loaded from.
struct AppPlaceSaid {
    std::string folder;
    std::string artifacts;
    ZEN_SHAPE(AppPlaceSaid, 1, ZEN_FIELD(folder), ZEN_FIELD(artifacts));
};

// ---- app.json ------------------------------------------------------------------------------

/// The app's window as `app.json` writes it. The app's weave reads `skin`, the painter it asks
/// the window to paint its areas with (`day`, `night`, `classic` or `block`); `title`, `width` and
/// `height` are written for the window and not read yet.
struct ZengineAppWindow {
    std::string title;
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::string skin;
    ZEN_SHAPE(ZengineAppWindow, 1, ZEN_FIELD(title), ZEN_FIELD(width), ZEN_FIELD(height),
              ZEN_FIELD(skin));
};

/// One view the app runs: its description's file in the app's folder, the session it runs in, and
/// where its area stands in the window, in pixels.
struct ZengineAppView {
    std::string file;
    std::string session;
    std::int64_t x = 0;
    std::int64_t y = 0;
    ZEN_SHAPE(ZengineAppView, 1, ZEN_FIELD(file), ZEN_FIELD(session), ZEN_FIELD(x), ZEN_FIELD(y));
};

/// One flow the app runs: its project's file in the app's folder (a definition and its state, as
/// Flow's Export writes it), and the session it runs in.
struct ZengineAppFlow {
    std::string file;
    std::string session;
    ZEN_SHAPE(ZengineAppFlow, 1, ZEN_FIELD(file), ZEN_FIELD(session));
};

/// `app.json`: the app's name, its window, and the views and flows it runs, flows first.
struct ZengineAppFile {
    std::string name;
    ZengineAppWindow window;
    std::vector<ZengineAppView> views;
    std::vector<ZengineAppFlow> flows;
    ZEN_SHAPE(ZengineAppFile, 1, ZEN_FIELD(name), ZEN_FIELD(window), ZEN_FIELD(views),
              ZEN_FIELD(flows));
};

} // namespace zengine::app

#endif // ZENGINE_APP_VOCABULARY_HPP
