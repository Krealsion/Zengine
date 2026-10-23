// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESKTOP_PANE_DESKTOP_COMMON_HPP
#define ZENGINE_DESKTOP_PANE_DESKTOP_COMMON_HPP

// What the Desktop's three panes and the weave that routes to them share: the offices they speak
// to, the tools a launch points at, the sentences they print, the subject a menu carries, and the
// records of a press, a declaration and a pane's canvas. Private to `zengine-desktop-pane`.
// Workshop law: agents/workshop/desktop.md

#include "workshop/desktop-pane/vocabulary.hpp"

#include "workshop/desktop_seam_vocabulary.hpp"
#include "workshop/pane_menu.hpp"
#include "workshop/pane_seat.hpp"
#include "workshop/pane_text.hpp"

#include "component/list_window.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::desktop_pane {

namespace ws = zengine::workshop;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::pane_text::admissible;
using ws::pane_text::fit;
using ws::pane_text::pad;

/// WHO THIS WEAVE IS TALKING TO -- the host's office, spelled as a literal exactly as every
/// other provider spells it. A provider is a stranger to Workshop's internals and says who it
/// is talking to the way a third party would.
inline constexpr const char* kWorkshopRole = "zengine.workshop";

/// THE TOOL THE TERMINAL LAUNCH POINTS AT, as a `PaneRef`'s two durable halves. (!) THESE ARE
/// SPELLED HERE, IN THE DESKTOP, AND THAT IS THE POINT: which tool `Ctrl+t` opens is an
/// application's choice, so it is written in the weave a weaver replaces rather than in the host
/// they cannot. A desktop that pointed it at a different terminal would change one line here.
inline constexpr const char* kTerminalOffice = "zengine.terminal";
inline constexpr const char* kTerminalPane = "terminal";

/// ...AND THE TOOL `n` POINTS AT, on the same terms: a pane of the weaver's own is a described
/// view, made and saved in the View Builder, and which tool makes one is the application's choice.
inline constexpr const char* kViewBuilderOffice = "zengine.view.builder";
inline constexpr const char* kViewBuilderPane = "view-builder";


/// THE SENTENCE FOR A PRESS THE WINDOW LANDED ON A ROW THIS PANE HAS SINCE REPLACED THERE.
inline constexpr const char* kMovedSentence = "the list moved -- press again";

/// A SENTENCE ANOTHER PARTY WROTE, MADE DRAWABLE: the host refuses a row carrying a byte a canvas
/// cannot draw, and a loader's refusal may quote a file's bytes or break a line. What this
/// pane prints of another's words is one row of printable text.
inline std::string drawable(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        if (b < 0x20 || b == 0x7f) {
            out += ' ';
        } else if (b >= 0x80) {
            // ONE MARK PER MULTI-BYTE SEQUENCE: the lead byte says how long it is.
            out += '?';
            std::size_t more = b >= 0xf0 ? 3 : b >= 0xe0 ? 2 : b >= 0xc0 ? 1 : 0;
            while (more > 0 && i + 1 < text.size() &&
                   (static_cast<unsigned char>(text[i + 1]) & 0xc0) == 0x80) {
                ++i;
                --more;
            }
        } else {
            out += text[i];
        }
    }
    return out;
}

/// THE SEPARATOR INSIDE A MENU SUBJECT: a `PaneRef`'s two halves, or a binding's three, joined
/// by a byte neither a pane key nor a gesture spelling may contain.
inline constexpr char kSubjectSep = '\x1f';

inline std::string join_subject(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += kSubjectSep;
        }
        out += parts[i];
    }
    return out;
}

inline std::vector<std::string> split_subject(const std::string& subject) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (true) {
        const std::size_t sep = subject.find(kSubjectSep, at);
        out.push_back(subject.substr(at, sep == std::string::npos ? std::string::npos : sep - at));
        if (sep == std::string::npos) {
            return out;
        }
        at = sep + 1;
    }
}

/// DID THE HOST SAY THIS? Either as its office, or as Loom's answer to an ask this image sent
/// to that office -- an answer carries answer provenance, not office authorship, and only the
/// office's holder at delivery could have given it.
inline bool from_workshop(const loom::Mail& mail) {
    return mail.authored_from_role(kWorkshopRole) || mail.answers_ask();
}

/// WHOLE NOTCHES OUT OF AN ACCUMULATED WHEEL, the remainder carried.
inline std::int64_t take_notches(double& carried, double dy) {
    return zengine::component::take_wheel_notches(carried, dy);
}

/// DOES THIS NOTICE NAME THE ASK A RECORD IS WAITING ON -- the same queued attempt, under the
/// same correlation, of the shape it sent, to the office it addressed? Every half, because a
/// sequence alone is a number and the rest is what this pane asked (Info's `refused_ask`).
inline bool refused_ask(bool awaiting, loom::Ticket sent, std::uint64_t pending,
                        const loom::DispatchRefused& refused, const loom::Mail& mail,
                        const char* shape, std::uint32_t version, const char* role) {
    const loom::Ticket attempt = refused.refused_attempt();
    return awaiting && sent.valid() && attempt.valid() && attempt.seq == sent.seq &&
           mail.correlation() == pending && refused.shape == shape &&
           refused.version == static_cast<std::int64_t>(version) && refused.role == role &&
           refused.target.empty();
}

/// ASK THE HOST TO OPEN OR FOCUS A PANE, under a number of this image's own, so the answer
/// that comes back can be read against the launch it answers. `launches` is the weave's one
/// counter for every launch, close and toggle it asks for.
inline void launch(loom::Mail& mail, const std::string& office, const std::string& pane_key,
                   std::uint64_t& launches) {
    (void)mail.as_role(kDesktopRole)
        .send_to_role(kWorkshopRole, ws::PaneLaunchRequested{office, pane_key}, ++launches);
}

/// ...AND TO TAKE ONE OFF THE DESK; the host judges whether it is there, because participation
/// is the desk's fact and not this list's copy of it.
inline void close(loom::Mail& mail, const std::string& office, const std::string& pane_key,
                  std::uint64_t& launches) {
    (void)mail.as_role(kDesktopRole)
        .send_to_role(kWorkshopRole, ws::PaneCloseRequested{office, pane_key}, ++launches);
}

/// WHERE A PRESS LANDED in one of this weave's panes: the row and column of the rows drawn there,
/// and whether the keys were already here.
struct PanePress {
    std::string pane;
    std::int64_t row = 0, column = 0;
    bool keys_went_here = false;
    /// The window drew another row under the hand than the pane's newest rows show there
    /// (`WindowPointer::moved`): refused in words, never resolved against the row there now.
    bool moved = false;
};

/// ONE OF THIS IMAGE'S DECLARATIONS: the number the latest attempt went out under, the
/// number Workshop gave the one in force, and what to tell the weaver about it.
struct Declared {
    std::uint64_t attempt = 0;
    std::int64_t in_force = 0;
    std::string word;
};

/// ONE PANE'S AREA: the window that shows it, and the rows and columns it composes for there.
using PaneCanvas = ws::PaneCanvasSeat;

} // namespace zengine::desktop_pane

#endif // ZENGINE_DESKTOP_PANE_DESKTOP_COMMON_HPP
