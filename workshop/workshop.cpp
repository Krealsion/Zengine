// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// zengine-workshop — the first Workshop surface. One rectangle, selected,
// inspected, edited, refused, moved, resized, typed into.
//
// It is an ordinary Zengine application and holds no privilege snake does not:
// input arrives from the Input package's weave, time from the Timer package's,
// pixels-or-characters from whichever Skin holds `zengine.skin`, and the host
// owns the boot list and nothing else. Workshop paints by PUBLISHING intent — a
// SurfaceCanvas — exactly as the world publishes a SnakeVisual, and it never
// touches the terminal.
//
// THIS FILE IS THE HOST, and only the host: the boot weave, `main()`, the two
// grants, and — since W-5 — the one command-line argument that says which file
// this Workshop's document lives in. Workshop's own weave lives in weave.hpp,
// where a suite can mount it — W-4 moved it there to close P16, so
// `input message -> gesture -> semantic operation` is a chain the tests can walk
// end to end instead of a claim the report has to make.
//
// THE HOST CHOOSES THE PATH AND THE WEAVE USES IT, which is the same division
// the boot list already follows: where things are is the host's business, what
// to do with them is the application's. Workshop still holds no privilege snake
// does not — it opens an ordinary file with an ordinary standard-library call,
// which is a power every program on this machine already has, and it needs no
// grant, no broker and no capability to do it (see the specialness ledger).

#include "weave.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

using namespace zengine::workshop;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace timer = zengine::timer;

/// The boot weave: it asks the Weave Manager to load the packages Workshop runs
/// on, and — the part that matters — it HEARS THE ANSWERS.
///
/// It exists because the first live run of this host had no such weave, and the
/// cost was immediate and instructive. "Failures are values": the Manager answers
/// its asker with zen.Result or zen.Refused. A root send carries no asker, so
/// every one of those answers was addressed to nobody, and a Workshop whose Skin
/// had refused to load looked exactly like a Workshop whose Skin had loaded — a
/// blank terminal and a host that exited saying the bus went quiet. The
/// diagnosis was not in the program at all.
///
/// So the asking is a weave with a grant, the way snake's operator is. It holds
/// the reach to the Manager — which is kernel reach, transitively, and is the
/// dangerous grant — and Workshop's own weave deliberately does not. Two
/// responsibilities, two grants: this one operates, that one authors.
struct BootState {
    std::int64_t answered = 0;
    std::int64_t refused = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BootState, 1, ZEN_FIELD(answered), ZEN_FIELD(refused));
};

class BootWeave : public loom::WeaveBase<BootWeave, BootState,
                                        loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                        loom::Emit<loom::LoadWeave, surface::SurfaceText>> {
public:
    void on(const loom::Result&, loom::Mail&) { ++state_.answered; }
    void on(const loom::Ack&, loom::Mail&) { ++state_.answered; }

    /// A refusal is the only thing here worth saying out loud, and it is said
    /// twice on purpose: as status intent for whoever is painting, and on plain
    /// stdout — because the most likely thing to have been refused is the Skin,
    /// and a message about a missing painter delivered to the painter is not a
    /// message.
    void on(const loom::Refused& r, loom::Mail& mail) {
        ++state_.answered;
        ++state_.refused;
        std::printf("zengine-workshop - boot refused: %s\n", r.reason.c_str());
        std::fflush(stdout);
        mail.publish(surface::SurfaceText{surface::kSlotStatus,
                                          "[workshop] boot refused: " + r.reason});
    }
};

std::string exe_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    std::string path(buf, n);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = '\0';
    std::string path(buf);
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
#endif
}

} // namespace

/// Which file this Workshop saves to and loads from.
///
/// `--document <path>`, defaulted to `workshop.json` in whatever directory the
/// maker started Workshop in. That is the whole of W-5's path story, and the
/// smallness is the point: there is no picker, no recent list, no project
/// concept and no workspace manager, because none of those is needed to prove
/// that a maker can close Workshop and get their work back. An unknown argument
/// is REFUSED rather than ignored — a mistyped flag that silently saved to the
/// default file is exactly the kind of quiet wrong answer persistence makes
/// expensive.
struct Arguments {
    bool ok = true;
    std::string complaint;
    std::string document = zengine::workshop::persist::kDefaultDocumentName;
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--document") {
            if (i + 1 >= argc) {
                args.ok = false;
                args.complaint = "--document needs a path";
                return args;
            }
            args.document = argv[++i];
            continue;
        }
        args.ok = false;
        args.complaint = "unknown argument `" + arg + "`";
        return args;
    }
    if (args.document.empty()) {
        args.ok = false;
        args.complaint = "--document needs a path";
    }
    return args;
}

int main(int argc, char** argv) {
    const Arguments args = parse_arguments(argc, argv);
    if (!args.ok) {
        std::printf("zengine-workshop - %s\nusage: zengine-workshop [--document <path>]\n",
                    args.complaint.c_str());
        return 2;
    }

    // The honest line, in plain scrollback, exactly as snake's host prints it:
    // this host isolates nothing.
    std::printf("zengine-workshop - containment: %s\n", loom::Kernel::containment_note());
    std::printf("zengine-workshop - document: %s\n", args.document.c_str());
    std::fflush(stdout);

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    HostContext host;
    host.dir = exe_dir();
    host.document_path = args.document;
    host.request_stop = [&bus] { bus.stop(); };

    // Workshop's own reach: the right to SPEAK its screen, and nothing else. It
    // commands no lifecycle, loads no weave and reaches no manager -- the boot
    // list below is the host's. A maker tool with a live document does not need
    // the dangerous grant to do its job, and giving it one "because Workshop
    // will eventually need it" is exactly the specialness these phases are
    // supposed to be counting.
    loom::Grant speak;
    speak.allow_to_any(surface::SurfaceCanvas::zen_name, surface::SurfaceCanvas::zen_version);
    speak.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    loom::mount_granted<WorkshopWeave>(bus, std::move(speak), host);

    // The boot weave's reach: the Manager, target-scoped, plus the right to speak
    // a status line. This is the dangerous grant in this process, and it is held
    // by the weave whose whole job is one round of loading.
    loom::Grant operate;
    operate.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    operate.allow_to_any(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version);
    const loom::WeaveId booter = loom::mount_granted<BootWeave>(bus, std::move(operate));

    // Boot, as ordinary LoadWeave commands sent AS the boot weave -- so the
    // Manager's answers come back to something that can hear them, and a refused
    // load is a fact this program can report instead of a silence it exits on.
    // The Skin is first: loading it claims the terminal -- which since W-4
    // includes asking the terminal to report its POINTER, because terminal modes
    // are the Skin's lifetime and always were -- and its hello is what makes
    // Workshop paint.
    const auto boot = [&](const char* stem, const char* role) {
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{stem, host.so(stem), role}),
                                  booter, booter));
    };
    boot("zengine-skin-tui-classic", surface::kSkinRole);
    boot("zengine-input", input::kInputRole);
    boot("zengine-timer", timer::kTimerRole);

    // Everything runs inside pump(): the input weave's own beat keeps the queue
    // alive, the Timer service's nap paces it, and `q` stops the bus. A pump
    // that returns with an empty queue means nothing in this process will ever
    // speak again -- say so and leave rather than spin, snake's stance and for
    // the same reason.
    while (!host.quit) {
        bus.pump();
        if (!host.quit && bus.pending() == 0) {
            std::printf("zengine-workshop - the bus went quiet without a quit "
                        "(no timer service deployed?): exiting.\n");
            break;
        }
    }
    return 0;
}
