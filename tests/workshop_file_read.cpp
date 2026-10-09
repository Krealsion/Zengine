// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Workshop's own readers over one file, for a case in another language: `setup <path>` reads it
// as the setup file is read, `session <path>` as the session is, the session history's conversions
// mounted. It prints what was read, one fact a line, or why the file was refused; it exits 0 when
// the file was read, 1 when it was refused and 2 when it was called wrongly.

#include "operator/catalog.hpp"
#include "workshop/session_history.hpp"
#include "workshop/session_persist.hpp"
#include "workshop/setup_persist.hpp"

#include <cstdio>
#include <string>

namespace ws = zengine::workshop;

namespace {

/// A desk as lines: its name and rows, and every setting a row keeps, in its kind's spelling.
void say_desk(const char* which, const ws::Setup& desk) {
    std::printf("%s %s, %zu panes\n", which, desk.name.c_str(), desk.panes.size());
    for (const ws::SetupPane& row : desk.panes) {
        for (const ws::PaneSetting& s : row.settings) {
            std::printf("setting %s/%s %s %s `%s`\n", row.ref.provider.c_str(),
                        row.ref.pane.c_str(), s.key.c_str(),
                        ws::setting_kind_word(ws::setting_kind(s)),
                        ws::setting_value_text(s).c_str());
        }
    }
}

int usage() {
    std::fprintf(stderr, "usage: zengine-workshop-file-read setup|session <path>\n");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        return usage();
    }
    const std::string kind = argv[1];
    const std::string path = argv[2];
    if (kind == "setup") {
        const ws::setup_persist::LoadedSetup read = ws::setup_persist::load_file(path);
        if (!read.outcome.accepted) {
            std::printf("refused: %s\n", read.outcome.refusal.c_str());
            return 1;
        }
        std::printf("read setup\n");
        say_desk("desk", read.setup);
        return 0;
    }
    if (kind == "session") {
        zengine::op::Catalog conversions;
        const zengine::op::MountReport mounted =
            conversions.mount("session-history", ws::session_history::conversions());
        if (!mounted) {
            std::printf("refused: the session history did not mount: %s\n",
                        mounted.reason.c_str());
            return 2;
        }
        const ws::session_persist::LoadedSession read =
            ws::session_persist::load_file(path, &conversions);
        if (!read.present) {
            std::printf("refused: no session file at %s\n", path.c_str());
            return 1;
        }
        if (!read.outcome.accepted) {
            std::printf("refused: %s\n", read.outcome.refusal.c_str());
            return 1;
        }
        std::printf("read session\n");
        std::printf("viewport %lld %lld\n", static_cast<long long>(read.viewport_w),
                    static_cast<long long>(read.viewport_h));
        std::printf("active %zu\n", read.active);
        for (const ws::Layout& layout : read.layouts) {
            say_desk("layout", layout.desk);
            std::printf("link %s\n", layout.link.path.c_str());
        }
        return 0;
    }
    return usage();
}
