// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_APP_FILES_HPP
#define ZENGINE_APP_FILES_HPP

// Reading an app's own files (app/docs/app.md): `app.json` through the compat codec and its
// shape, as the load plan is read, and a view's or a flow's bytes, each bounded. A file `app.json`
// names is a plain name in the app's folder, never a path out of it.

#include "app/vocabulary.hpp"

#include "maker/files.hpp"

#include <zen/admission.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace zengine::app {

/// What reading `app.json` came to: the app, or why not, in words.
struct ReadApp {
    bool ok = false;
    std::string refusal;
    ZengineAppFile app;
};

/// What reading one of the app's files came to: its bytes, or why not.
struct ReadBytes {
    bool ok = false;
    std::string refusal;
    loom::Bytes bytes;
};

/// A name `app.json` may give a file: a plain name in the app's folder. Empty when it is one,
/// else why not.
inline std::string plain_name(const std::string& name) {
    if (name.empty() || name == "." || name == "..") {
        return "names no file";
    }
    if (name.find_first_of("/\\:") != std::string::npos) {
        return "is a path, and an app's files stand in its own folder";
    }
    return {};
}

/// `app.json`'s text, admitted against its shape and its rules: a name, at most `kMaxAppViews`
/// views and `kMaxAppFlows` flows, each file a plain name and each session 1 to 128 bytes.
inline ReadApp app_from_text(std::string_view text) {
    ReadApp out;
    const loom::Admission admitted = loom::admit(loom::compat::parse(text),
                                                 loom::schema_of<ZengineAppFile>(),
                                                 loom::Report::FirstError);
    if (!admitted.ok()) {
        out.refusal = "app.json is not an app: " + admitted.first_error().message();
        return out;
    }
    ZengineAppFile app = loom::from_value<ZengineAppFile>(admitted.value());
    const auto refuse = [&out](std::string why) {
        out.refusal = "app.json " + std::move(why);
        return out;
    };
    if (app.name.empty()) {
        return refuse("names no app");
    }
    if (app.views.size() > kMaxAppViews || app.flows.size() > kMaxAppFlows) {
        return refuse("runs more than " + std::to_string(kMaxAppViews) + " views or " +
                      std::to_string(kMaxAppFlows) + " flows");
    }
    const auto session = [](const std::string& s) { return !s.empty() && s.size() <= 128; };
    for (const ZengineAppView& v : app.views) {
        if (const std::string why = plain_name(v.file); !why.empty()) {
            return refuse("view file '" + v.file + "' " + why);
        }
        if (!session(v.session)) {
            return refuse("view '" + v.file + "' names a session of 1 to 128 bytes");
        }
    }
    for (const ZengineAppFlow& f : app.flows) {
        if (const std::string why = plain_name(f.file); !why.empty()) {
            return refuse("flow file '" + f.file + "' " + why);
        }
        if (!session(f.session)) {
            return refuse("flow '" + f.file + "' names a session of 1 to 128 bytes");
        }
    }
    out.ok = true;
    out.app = std::move(app);
    return out;
}

inline ReadApp read_app(const std::string& path) {
    const maker::FileBytes read = maker::read_file(path, kMaxAppFileBytes);
    if (!read) {
        ReadApp out;
        out.refusal = read.reason;
        return out;
    }
    return app_from_text(read.bytes);
}

/// One of the app's files, whole, within a maker file's bound.
inline ReadBytes read_bytes(const std::string& path) {
    ReadBytes out;
    const maker::FileBytes read = maker::read_file(path);
    if (!read) {
        out.refusal = read.reason;
        return out;
    }
    out.ok = true;
    out.bytes.assign(read.bytes.begin(), read.bytes.end());
    return out;
}

} // namespace zengine::app

#endif // ZENGINE_APP_FILES_HPP
