// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EDITOR_OFFICE_SEAM_HPP
#define ZENGINE_EDITOR_OFFICE_SEAM_HPP

// What both implementations of the Editor's office keep and say alike: the records of their
// conversations with the opening office and with Workshop -- an open relayed through the old door,
// a switch's hold, a copy carried out, a dropped location opened -- the steps those take, and the
// sentences they end in; and the graphics window that shows the Editor (`EditorWindow`).
// Header-only, and read by the two Editor weaves alone (`editor-pane/`, `neovim-editor/`); each
// keeps its own document and speaks as its own vocabulary names it.
// Workshop law: agents/workshop/editor.md, agents/workshop/editor-transfers.md, agents/workshop/neovim-transfers.md

#include "editor/source-transfer/command_line.hpp"
#include "editor/source-transfer/cpp.hpp"
#include "editor/source-transfer/material.hpp"
#include "graphics/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_carry.hpp"
#include "workshop/pane_drawing.hpp"
#include "workshop/pane_operation.hpp"
#include "workshop/pane_seam_vocabulary.hpp"
#include "workshop/pane_seat.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/persist.hpp"

#include <zen/weave.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::editor_office {

namespace st = zengine::source_transfer;
namespace ws = zengine::workshop;

/// The office Workshop holds, named as a string rather than through `workshop/panes.hpp`: an
/// Editor is a stranger to Workshop's internals.
inline constexpr const char* kWorkshopRole = "zengine.workshop";

/// One column of every document row the text may not use: the caret's own, so a caret after the
/// end of a full row stays inside the room (WL-EDIT-08).
inline constexpr std::int64_t kCaretCols = 1;

/// How many opens one Editor relays through the old door at once.
inline constexpr std::size_t kMaxRelays = 4;

/// The subject a dropped command's menu is told apart by.
inline constexpr const char* kDropSubject = "drop";

// ---- The records of the office's conversations ---------------------------------------------

/// An open relayed through the old door: the requester's kept answer right, and the Editor's own
/// conversation with the opening office (its correlation and exact attempt), so the answer or the
/// bus's refusal finds its requester. Bounded by `kMaxRelays`, and not reload state: the rights die
/// with the incarnation that kept them (ANS-02), and an answer to a predecessor's relay is refused.
struct Relay {
    std::string path;
    std::uint64_t correlation = 0;
    loom::Ticket attempt{};
    loom::DeferredAnswer answer;
};

/// A SWITCH HOLDING THE EDITOR STILL at its boundary: which operation, and how many inputs it
/// refused meanwhile. Not reload state -- a reload is not a switch, and the hold belongs to the
/// incarnation the coordinator is talking to.
struct Holding {
    bool active = false;
    std::int64_t op = 0;
    std::int64_t refused = 0;
};

/// WHAT A HAND IS TAKING: the selection's text, or this file's location.
enum class Take : std::uint8_t { Selection, Location };

/// ONE ACQUISITION IN FLIGHT: Workshop's approval for the gesture, then the carry itself.
struct Pickup {
    enum class Stage : std::uint8_t { Idle, Permission, Carry };
    Stage stage = Stage::Idle;
    std::uint64_t ask = 0;
    std::uint64_t gesture = 0;
    bool drag = false;
    loom::Bytes bytes;
    std::string label;
    std::string what;
    loom::Ticket ticket{};
};

/// A DROPPED LOCATION: Workshop's approval for the gesture, then the managed open. `snapshot` is
/// the document as it stood when the open was asked, in the measure its Editor tells "nothing
/// moved since" by: the standard Editor's mark, Neovim's changedtick.
template <class SnapshotT>
struct LocateOf {
    enum class Stage : std::uint8_t { Idle, Permission, Opening };
    Stage stage = Stage::Idle;
    std::uint64_t ask = 0;
    loom::Ticket ticket{};
    st::SourceLocation loc;
    std::optional<st::SourceLocationContext> ctx;
    bool same_path = false;
    SnapshotT snapshot{};
};

// ---- Pure helpers --------------------------------------------------------------------------

/// A PATH THAT KEEPS ITS END. Whole when it fits; otherwise `...` and the last `width - 3`
/// bytes, so the file's own name survives before its directories do; a width too small
/// for even that keeps the last bytes alone.
inline std::string tail_of_path(const std::string& path, std::int64_t width) {
    if (width <= 0) {
        return std::string();
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (path.size() <= room) {
        return path;
    }
    if (room <= 4) {
        return path.substr(path.size() - room);
    }
    return "..." + path.substr(path.size() - (room - 3));
}

/// A PATH'S LAST PART, the name a carried copy goes by.
inline std::string file_name(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// Whether two claims name one document as it stood: its path, epoch, convention, content,
/// dirtiness and the operation that opened it.
inline bool same_identity(const ws::EditorDocument& a, const ws::EditorDocument& b) {
    return a.path == b.path && a.doc_epoch == b.doc_epoch && a.convention == b.convention &&
           a.content_revision == b.content_revision && a.dirty == b.dirty &&
           a.opened_by == b.opened_by;
}

/// THE WEAVER'S WORDS FOR AN OFFER THE BUS REFUSED: the document the Editor claims moved since
/// the operation bound it (an edit, a paste), the operation was superseded, or a participant was
/// replaced. The diagnostic name stays in parentheses.
inline std::string offer_refusal(const std::string& path, loom::JointRefusal why) {
    switch (why) {
    case loom::JointRefusal::StaleRevision:
    case loom::JointRefusal::ParticipantChanged:
    case loom::JointRefusal::WrongState:
    case loom::JointRefusal::Cancelled:
        return "the document changed while opening " + path + " -- try again";
    default:
        return "the opening was no longer arranged (" + std::string(loom::name_of(why)) +
               ") -- " + path + " was not prepared";
    }
}

/// A REFUSED PREPARATION, as one value: the operation, no, and the sentence.
inline ws::v3::SourcePrepared not_prepared(std::int64_t op, std::string refusal) {
    ws::v3::SourcePrepared refused;
    refused.op = op;
    refused.ok = false;
    refused.refusal = std::move(refusal);
    return refused;
}

// ---- The old door, relayed (WL-OPEN-07) ----------------------------------------------------

/// THE OLD DOOR: an Editor does not arrange the desk, so an open asked of it is asked of the
/// opening office in the Editor's own conversation, as `office`, the requester's answer right kept
/// in `relays`. An unauthored ask is dropped. Refused now, in words -- never silence, and never a
/// document-only success: the opening office itself (it would be asking the Editor to ask it
/// back), a switch holding the Editor still (counted in `holding`), a full relay table, and an ask
/// that queued nothing.
inline void relay_open_source(const ws::OpenSourceRequested& asked, loom::Mail& mail,
                              const char* office, Holding& holding, std::vector<Relay>& relays,
                              std::uint64_t& asked_seq) {
    if (mail.authored_role().empty()) {
        return;
    }
    if (mail.authored_from_role(ws::kOpeningRole)) {
        (void)mail.answer(ws::SourceOpened{
            false, "the opening office asks the Editor to prepare a source, not to open "
                   "one -- nothing was relayed for " +
                       asked.path});
        return;
    }
    if (holding.active) {
        ++holding.refused;
        (void)mail.answer(ws::SourceOpened{false, "the Editor is being switched -- " + asked.path +
                                                      " was not opened; open it again once the "
                                                      "switch has settled"});
        return;
    }
    if (relays.size() >= kMaxRelays) {
        (void)mail.answer(ws::SourceOpened{
            false, "too many opens are already being relayed through the Editor -- " +
                       asked.path + " was not opened; try again"});
        return;
    }
    const std::uint64_t correlation = ++asked_seq;
    const loom::Ticket attempt =
        mail.as_role(office).send_to_role(ws::kOpeningRole, ws::OpenSourceRequested{asked.path},
                                          correlation);
    if (!attempt.valid()) {
        (void)mail.answer(ws::SourceOpened{
            false, "nothing was queued: the Editor could not ask the opening office to show " +
                       asked.path});
        return;
    }
    Relay relay;
    relay.path = asked.path;
    relay.correlation = correlation;
    relay.attempt = attempt;
    relay.answer = mail.defer_answer();
    relays.push_back(std::move(relay));
}

/// The opening office's answer to a relay, matched by the Editor's own correlation and spent into
/// the requester's kept right. An answer to nothing relayed is dropped; a right whose requester
/// was replaced spends into nobody, which the bus refuses.
inline void settle_relay(const ws::SourceOpened& said, loom::Mail& mail,
                         std::vector<Relay>& relays) {
    if (!mail.answers_ask()) {
        return;
    }
    for (auto it = relays.begin(); it != relays.end(); ++it) {
        if (it->correlation == mail.correlation()) {
            (void)loom::answer_deferred(it->answer, mail,
                                        ws::SourceOpened{said.accepted, said.refusal});
            relays.erase(it);
            return;
        }
    }
}

/// THE BUS'S WORD THAT AN ASK NEVER ARRIVED, matched by its exact attempt: a relay's requester is
/// told the opening office could not be reached; a pickup's or a dropped location's failure is
/// said through `notice(text, bad)` and its record released. Delivered silence stays pending.
/// True when the pane has a notice to say.
template <class SnapshotT, class NoticeFn>
bool settle_dispatch_refused(const loom::DispatchRefused& refused, loom::Mail& mail,
                             std::vector<Relay>& relays, Pickup& pickup,
                             LocateOf<SnapshotT>& locate, NoticeFn&& notice) {
    if (!mail.dispatch_refused()) {
        return false;
    }
    const loom::Ticket attempt = refused.refused_attempt();
    if (!attempt.valid()) {
        return false;
    }
    for (auto it = relays.begin(); it != relays.end(); ++it) {
        if (it->attempt.seq == attempt.seq) {
            (void)loom::answer_deferred(
                it->answer, mail,
                ws::SourceOpened{false, std::string(ws::kOpeningRole) + " could not be reached (" +
                                            refused.reason + ") -- " + it->path +
                                            " was not opened"});
            relays.erase(it);
            return false;
        }
    }
    if (pickup.stage != Pickup::Stage::Idle && pickup.ticket.valid() &&
        attempt.seq == pickup.ticket.seq) {
        pickup = Pickup{};
        notice("nothing was carried -- Workshop could not be asked (" + refused.reason + ")", true);
        return true;
    }
    if (locate.stage != LocateOf<SnapshotT>::Stage::Idle && locate.ticket.valid() &&
        attempt.seq == locate.ticket.seq) {
        const std::string path = locate.loc.path;
        locate = LocateOf<SnapshotT>{};
        notice("nothing was opened -- " + path + " could not be asked for (" + refused.reason + ")",
               true);
        return true;
    }
    return false;
}

// ---- A copy carried out, and a dropped location opened: Workshop approves the gesture first --

/// ONE ACQUISITION BEGUN: the pair's bytes kept in `pickup`, and Workshop asked, as `office` for
/// `pane`, to approve a carry for the gesture that caused it; the carry follows the approval
/// (`answer_operation`). A refused pair, a pickup already in flight and an ask that queued
/// nothing are said through `notice(text, bad)`, and nothing is kept.
template <class NoticeFn>
void begin_pickup(const st::Pair& pair, bool drag, std::uint64_t gesture, const std::string& label,
                  std::string what, loom::Mail& mail, const char* office, const char* pane,
                  std::uint64_t& asked_seq, Pickup& pickup, NoticeFn&& notice) {
    if (!pair.ok) {
        notice("nothing was carried -- " + pair.refusal, true);
        return;
    }
    if (pickup.stage != Pickup::Stage::Idle) {
        notice("nothing was carried -- a copy is already on its way to Workshop", true);
        return;
    }
    pickup.stage = Pickup::Stage::Permission;
    pickup.ask = ++asked_seq;
    pickup.gesture = gesture;
    pickup.drag = drag;
    pickup.bytes.assign(pair.bytes.begin(), pair.bytes.end());
    pickup.label = label.substr(0, ws::kMaxCarryLabelBytes);
    pickup.what = std::move(what);
    pickup.ticket = mail.as_role(office).send_to_role(
        kWorkshopRole,
        ws::PaneOperationRequested{pane, kWorkshopRole, ws::PaneValueCarryRequested::zen_name,
                                   ws::PaneValueCarryRequested::zen_version,
                                   static_cast<std::int64_t>(gesture)},
        pickup.ask);
    if (!pickup.ticket.valid()) {
        pickup = Pickup{};
        notice("nothing was carried -- Workshop could not be asked", true);
    }
}

/// WHY A DROPPED LOCATION CANNOT BE ASKED FOR, before either Editor judges anything of its own:
/// it names no absolute path, or another is still being opened. Empty when it can.
template <class SnapshotT>
std::string location_refusal(const st::SourceLocation& loc, const LocateOf<SnapshotT>& locate) {
    if (loc.path.empty() || !std::filesystem::path(loc.path).is_absolute()) {
        return "nothing was opened -- this location names no absolute path; edit its path in Info";
    }
    if (locate.stage != LocateOf<SnapshotT>::Stage::Idle) {
        return "nothing was opened -- a dropped location is still being opened";
    }
    return std::string();
}

/// A DROPPED LOCATION ASKED FOR: Workshop is asked, as `office` for `pane`, whether this gesture's
/// actor may open a file; the managed open follows the approval (`answer_operation`). Its path is
/// kept as both Editors spell one -- absolute, lexically normal, forward slashes. An ask that
/// queued nothing is said through `notice(text, bad)`, and nothing is kept.
template <class SnapshotT, class NoticeFn>
void ask_location(const st::Material& m, loom::Mail& mail, const char* office, const char* pane,
                  std::uint64_t& asked_seq, LocateOf<SnapshotT>& locate, NoticeFn&& notice) {
    locate.stage = LocateOf<SnapshotT>::Stage::Permission;
    locate.loc = m.location;
    locate.loc.path = ws::persist::resolved_against(std::string(), m.location.path);
    locate.ctx = m.location_context;
    locate.ask = ++asked_seq;
    locate.ticket = mail.as_role(office).send_to_role(
        kWorkshopRole,
        ws::PaneOperationRequested{pane, ws::kOpeningRole, ws::OpenSourceRequested::zen_name,
                                   ws::OpenSourceRequested::zen_version,
                                   static_cast<std::int64_t>(mail.correlation())},
        locate.ask);
    if (!locate.ticket.valid()) {
        locate = LocateOf<SnapshotT>{};
        notice("nothing was opened -- Workshop could not be asked", true);
    }
}

/// WORKSHOP'S WORD ON AN ASK THE EDITOR MADE FOR A GESTURE, Loom's answer to its own ask or
/// nothing: an approved pickup's copy is handed to Workshop, and an approved location is asked of
/// the opening office as `office`, once `stamp(locate)` has written what the open is judged
/// against when it settles. A refusal, or an ask that queued nothing, is said through
/// `notice(text, bad)` and its record released. True when the pane has a notice to say.
template <class SnapshotT, class StampFn, class NoticeFn>
bool answer_operation(const ws::PaneOperationAnswered& answer, loom::Mail& mail,
                      const char* office, const char* pane, std::uint64_t& asked_seq,
                      Pickup& pickup, LocateOf<SnapshotT>& locate, StampFn&& stamp,
                      NoticeFn&& notice) {
    if (!mail.answers_ask()) {
        return false;
    }
    if (pickup.stage == Pickup::Stage::Permission && mail.correlation() == pickup.ask) {
        if (!answer.allowed) {
            pickup = Pickup{};
            notice("nothing was carried -- " + answer.reason, true);
            return true;
        }
        pickup.stage = Pickup::Stage::Carry;
        pickup.ticket = mail.as_role(office).send_to_role(
            kWorkshopRole, ws::PaneValueCarryRequested{pane, pickup.label, pickup.bytes, pickup.drag},
            pickup.gesture);
        if (!pickup.ticket.valid()) {
            pickup = Pickup{};
            notice("nothing was carried -- the copy could not be handed to Workshop", true);
            return true;
        }
        return false;
    }
    if (locate.stage == LocateOf<SnapshotT>::Stage::Permission && mail.correlation() == locate.ask) {
        if (!answer.allowed) {
            locate = LocateOf<SnapshotT>{};
            notice("nothing was opened -- " + answer.reason, true);
            return true;
        }
        // THE OPEN IS THE MANAGED ONE, asked as the Editor's office (the relay's own route): the
        // unsaved-work floor, the desk and a refusal's attribution are the opening's, unchanged.
        locate.stage = LocateOf<SnapshotT>::Stage::Opening;
        stamp(locate);
        locate.ask = ++asked_seq;
        locate.ticket = mail.as_role(office).send_to_role(
            ws::kOpeningRole, ws::OpenSourceRequested{locate.loc.path}, locate.ask);
        if (!locate.ticket.valid()) {
            const std::string path = locate.loc.path;
            locate = LocateOf<SnapshotT>{};
            notice("nothing was opened -- the Editor could not ask the opening office for " + path,
                   true);
            return true;
        }
    }
    return false;
}

/// WORKSHOP TOOK THE COPY, OR SAID WHY NOT: the pickup is released, and a refusal -- or, for a copy
/// a menu or a key carried rather than a drag, what the hand now holds -- is said through
/// `notice(text, bad)`, `unchanged` naming what the copy left as it was. False when this answers
/// no pickup in flight.
template <class NoticeFn>
bool settle_carry(const ws::PaneCarryAnswered& answer, loom::Mail& mail, Pickup& pickup,
                  const std::string& unchanged, NoticeFn&& notice) {
    if (pickup.stage != Pickup::Stage::Carry || !mail.answers_ask() ||
        mail.correlation() != pickup.gesture) {
        return false;
    }
    const Pickup done = std::move(pickup);
    pickup = Pickup{};
    if (!answer.carried) {
        notice("nothing was carried -- " + answer.reason, true);
    } else if (!done.drag) {
        notice("carrying a copy of " + done.what + " -- click a receiving pane, or Escape; " +
                   unchanged,
               false);
    }
    return true;
}

// ---- What a settled location and an insertion say ------------------------------------------

/// THE PROJECT ROOTS A DROPPED LOCATION IS SAID AGAINST: the one it was saved under, and this
/// run's. `elsewhere` when both are known and differ -- a location never follows its name to
/// another root.
struct Roots {
    std::string saved;
    std::string here;
    bool elsewhere = false;
};

inline Roots roots_of(const std::optional<st::SourceLocationContext>& ctx, std::string here) {
    Roots r;
    r.saved = ctx ? ctx->project_root : std::string();
    r.here = std::move(here);
    r.elsewhere = !r.saved.empty() && !r.here.empty() && r.saved != r.here;
    return r;
}

/// WHAT A DROPPED LOCATION'S REFUSED OPEN SAYS: the opening's own sentence, and -- saved under
/// another root -- both roots and how to rebind it.
inline std::string open_refused_words(const std::string& path, const std::string& refusal,
                                      const Roots& roots) {
    std::string why = "could not open " + path + ": " + refusal;
    if (roots.elsewhere) {
        why += " -- it was saved under " + roots.saved + ", and this run's project is " + roots.here +
               "; a location never follows its name to another root (edit its path in Info to rebind it)";
    }
    return why;
}

/// WHAT A DROPPED LOCATION'S OPEN SAYS FIRST: the file shown, and the other root it was saved
/// under, if it was.
inline std::string opened_words(const std::string& shown, const Roots& roots) {
    return "opened " + shown +
           (roots.elsewhere ? " (saved under another project root, " + roots.saved + ")" : std::string());
}

/// WHAT A COMMAND'S TERMINAL LINE, PUT IN AS TEXT, SAYS AFTER ITS PLACE: that nothing was sent,
/// the fields it still lacks, a destination it marks that the value never named, and the
/// material's own note on its address.
inline std::string terminal_line_after(const st::TerminalLine& t, const st::Material& m) {
    std::string after = "; text only -- nothing was sent";
    if (!t.missing.empty()) {
        after += "; INCOMPLETE: ";
        for (std::size_t i = 0; i < t.missing.size(); ++i) {
            after += (i > 0 ? ", " : "") + t.missing[i];
        }
        after += t.missing.size() == 1 ? " is not set" : " are not set";
    }
    if (!t.address_supplied) {
        after += "; <address> marks a destination this value never named";
    }
    if (!m.address_note.empty()) {
        after += " (" + m.address_note + ")";
    }
    return after;
}

/// WHAT GENERATED C++ SAYS OF ITSELF: the function and what it builds, `aside` after them, the
/// includes it still needs or that they are already here, and how many required fields it leaves
/// to fill.
inline std::string generated_cpp_words(const st::GeneratedCpp& g, const std::string& what,
                                       const std::string& aside) {
    std::string said = "generated " + g.function + "() for " + what + aside + "; ";
    if (g.missing_includes.empty()) {
        said += "its includes are already here";
    } else {
        said += "add #include";
        for (std::size_t i = 0; i < g.missing_includes.size(); ++i) {
            said += (i > 0 ? " and " : " ") + g.missing_includes[i];
        }
    }
    if (!g.holes.empty()) {
        said += "; INCOMPLETE until you fill " + std::to_string(g.holes.size()) + " required field" +
                (g.holes.size() == 1 ? "" : "s");
    }
    return said;
}

// ---- The graphics window that shows the Editor ---------------------------------------------

/// THE GRAPHICS WINDOW THAT SHOWS THE EDITOR, as the weave holding the office keeps it: the window
/// drawing the area the office names (`ws::WindowDrawing`), the room it gives, and where a press
/// on each named part of the rows last drawn there lands. Transient, as every room is: an Editor
/// reloaded or admitted by a switch asks for it again.
class EditorWindow {
public:
    explicit EditorWindow(const char* office) : office_(office), window_(office, office) {}

    const std::string& area() const noexcept { return window_.area(); }
    /// A graphics window shows the area.
    bool in_window() const noexcept { return window_.shown(); }
    /// The office a paste is asked of: the window showing the area, else the Skin.
    std::string clipboard_office() const { return window_.clipboard_office(); }
    /// The window's room in rows and columns: a line of its text and an advance of it.
    std::int64_t rows() const noexcept { return window_.rows_room().rows(); }
    std::int64_t columns() const noexcept { return window_.rows_room().columns(); }

    /// THE ROOM ASKED OF THE GRAPHICS DIRECTORY, as the office, while no window has said it.
    void ask_room(loom::Mail& mail) const {
        if (!in_window()) {
            (void)mail.as_role(office_).send_to_role(graphics::kGraphicsRole,
                                                     graphics::RoomAsked{area()});
        }
    }

    /// THE ROOM, said by the window that shows the area as that window's office: true when the
    /// window, its incarnation, its size or its measure changed or the area is empty, so the window
    /// holds none of the drawing and the next goes whole.
    bool room(const graphics::AreaRoom& r, const loom::Mail& mail) { return window_.room(r, mail); }

    /// A DRAWING THE WINDOW REFUSED leaves what it shows as it was: a refused change is followed at
    /// once by the rows last drawn, whole; a refused whole drawing, which the window would refuse
    /// again, by nothing until the rows change (`ws::WindowDrawing::refused`). False for another
    /// area or speaker.
    bool refused(const graphics::DrawnRefused& r, loom::Mail& mail) {
        return window_.refused(r, mail);
    }

    /// A PRESS ON A PART IN THE WINDOW, asked by the window that shows the area as its office: the
    /// place of the part `asked.action` names in the rows last drawn there. None for another area
    /// or speaker, or an action those rows name no part by.
    std::optional<ws::RowCell> part(const graphics::ActionRequested& asked,
                                    const loom::Mail& mail) const {
        if (!window_.from_window(asked.area, mail)) {
            return std::nullopt;
        }
        const auto at = places_.find(asked.action);
        return at == places_.end() ? std::nullopt : std::optional<ws::RowCell>(at->second);
    }

    /// WHERE A POINT OF THE AREA FALLS ON THE ROWS DRAWN THERE: a drop's place.
    ws::RowCell cell_at(std::int64_t x, std::int64_t y) const {
        return ws::window_cell(x, y, rows(), columns(), window_.text());
    }

    /// The room a trial says the area will have, in the rows and columns the Editor composes for:
    /// a line of the window's text and an advance of it, a cell each where text is a cell.
    static ws::RowsRoom room_of(const ws::OpeningRoom& r) {
        const bool type = r.cell_px > 0 && r.text_advance_px > 0 && r.text_line_px > 0;
        return ws::RowsRoom{r.width, r.height,
                            type ? graphics::TextMeasure{r.text_advance_px, r.text_line_px}
                                 : graphics::TextMeasure{},
                            !type};
    }

    /// A POINTER MOMENT THE WINDOW SHOWING THE AREA GAVE IT, as that window's office, read onto
    /// the rows drawn there (`ws::WindowDrawing::pointer`). None for another area or speaker.
    template <class Moment>
    std::optional<ws::WindowPointer> pointer(const Moment& moment, const loom::Mail& mail) const {
        return window_.pointer(moment, mail, rows(), columns());
    }
    std::optional<ws::WindowPointer> pointer(const graphics::AreaHoldEnded& ended,
                                             const loom::Mail& mail) const {
        return window_.pointer(ended, mail);
    }

    /// THE EDITOR'S ROWS, DRAWN in the window that shows the area, as many as its room holds, the
    /// caret and the selection standing in them (`ws::rows_drawing`): what changed there since the
    /// drawing last sent, or the whole after a new room or a refused change.
    void draw(loom::Mail& mail, const std::vector<ws::TextRow>& said,
              const std::vector<ws::RowPart>& parts, const ws::RowsCaret& caret) {
        if (!in_window() || rows() <= 0 || columns() <= 0) {
            return;
        }
        ws::RowsDrawing drawn =
            ws::rows_drawing(said, parts, window_.rows_room(), columns(), caret, rows());
        places_ = std::move(drawn.places);
        window_.draw(drawn.sketch, mail);
    }

private:
    const char* office_;
    ws::WindowDrawing window_;
    /// Where a press in the window reaches each named part of the rows last drawn there.
    std::map<std::string, ws::RowCell> places_;
};

} // namespace zengine::editor_office

#endif // ZENGINE_EDITOR_OFFICE_SEAM_HPP
