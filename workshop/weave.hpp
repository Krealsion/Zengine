// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WEAVE_HPP
#define ZENGINE_WORKSHOP_WEAVE_HPP

// Workshop's own weave: the authored document, the session, and the bindings
// from input MOMENTS to maker GESTURES.
//
// WHY IT IS A HEADER (W-4, closing P16). It used to live in workshop.cpp's
// anonymous namespace, where no suite could reach it — so every phase since W-0
// could prove `gesture -> document` and never `message -> gesture`, and the
// binding was the one part of the pointer path nothing witnessed. W-4 rewrote
// what a message CONTAINS, which makes that gap exactly the wrong one to keep:
// the phase's central claim is that a press carries its own position, and the
// only place that claim can be tested end to end is here. Moving the class is
// the whole fix — no framework, no registry, no test hooks. `main()` and the
// host's boot weave stay in the .cpp, because those are the host's job and not
// Workshop's.
//
// WHAT IS AND IS NOT WORKSHOP'S HERE, because several phases turn on it:
//
//   the authored object   a real zengine::ui::Element in the weave's own state:
//                         gated, schema-carrying, poke-inspectable. There is no
//                         shadow model -- the element the maker selects IS the
//                         element the canvas is painted from and the inspector
//                         reads through. W-1 moved the TYPE out to the UI
//                         package; the object is no less Workshop's state for
//                         being spelled in a shared vocabulary.
//   the geometry          NOT Workshop's, since W-1. `ui::resolve` turns the
//                         authored extents into a scene and `ui::hit` says what
//                         is under a cell; this file computes neither, and the
//                         canvas, the inspector and the pointer all read one
//                         scene.
//   the session           selection, workspace extent, drafts, a drag in flight.
//                         Plain members, never state (the Skin's `announced_`
//                         stance).
//   the screen            screen.hpp, pure, pinned by the suite -- and since W-2
//                         that header owns the GESTURES too. This file binds
//                         messages to them and reaches the document through
//                         nothing else, so every maker action the suite drives
//                         is the same one a maker's hand drives.
//
// THE INPUT REALITY, named where a reader will hit it — and W-4 is where it
// finally reads as a description of what works rather than a list of what does
// not. Three reconstructions this file used to perform are GONE, not tidied:
//
//   typing        `character_of(scancode)` is deleted. Characters arrive as
//                 input::TextEntered, from the platform's own keyboard layout,
//                 so `%` and capital letters are ordinary text and Workshop
//                 computes no `Shift+5 -> %` table for anybody.
//   resizing      the four literal keys `, . - =` are deleted. A second
//                 directional gesture is `Shift + hjkl`, because a key event
//                 now says what was held when it happened.
//   pointing      `pointer_x_ / pointer_y_` are deleted. A press carries the
//                 position it happened at, so nothing here remembers where the
//                 pointer was in order to answer where the click landed.

#include "screen.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace zengine::workshop {

/// What the weave needs from the host and cannot get by message: the stop
/// lever. `dir`/`so()` are the host's own boot bookkeeping and are filled in
/// there — kept whole in this header so a suite can construct one without
/// linking the host.
struct HostContext {
    bool quit = false;
    std::function<void()> request_stop;
    std::string dir;

    /// A weave stem, as this platform spells a shared library.
    std::string so(const char* stem) const {
        return dir + "/" + stem +
#if defined(_WIN32)
               ".dll";
#else
               ".so";
#endif
    }
};

/// The Workshop weave: the authored document, the session, and the bindings.
class WorkshopWeave
    : public loom::WeaveBase<WorkshopWeave, WorkshopDoc,
                             loom::Accept<zengine::input::KeyPressed, zengine::input::TextEntered,
                                          zengine::input::PointerButton,
                                          zengine::input::PointerMoved,
                                          zengine::surface::SurfaceReady>,
                             loom::Emit<zengine::surface::SurfaceCanvas,
                                        zengine::surface::SurfaceText>> {
public:
    explicit WorkshopWeave(HostContext& host) : host_(&host) {
        // The document a maker opens onto. Deliberately boring, and deliberately
        // TWO rectangles sharing nothing but a name pattern -- `panel` and
        // `panel` would be the same object in the old builder, and here they are
        // #1 and #2. The wide one is authored as a SHARE so the very first screen
        // already shows an authored intent and its resolved value side by side.
        doc::add(state_, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60},
                 ui::Extent{ui::kExtentCells, 6});
        doc::add(state_, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14},
                 ui::Extent{ui::kExtentCells, 4});
        session_.selected = state_.elements.empty() ? 0 : state_.elements.front().id;
        rebuild_rows();
    }

    /// A Skin claimed the surface and said hello: give it the whole screen. The
    /// operator weave's precedent, and the only thing Workshop needs in order to
    /// paint for the first time -- so load order decides nothing here either.
    void on(const zengine::surface::SurfaceReady&, loom::Mail& mail) { repaint(mail); }

    /// A key TRANSITION: which key changed, and what was held when it did.
    ///
    /// Ctrl+C is the one key that means the same thing in every mode, and it is
    /// now spelled as what it is. V1 had no modifier vocabulary, so the backends
    /// dressed the courtesy NAME as "Ctrl+C" and this branch trusted a courtesy;
    /// that contract is retired and the modifier is read from the modifier field.
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        if (k.scancode == input::scan::kC && held(k.modifiers, input::mod::kCtrl)) {
            quit();
            return;
        }
        if (editing_row() != nullptr) {
            editing_key(k);
        } else {
            command(k);
        }
        repaint(mail);
    }

    /// TEXT the maker actually entered — the platform's answer, not a guess made
    /// from a key identity. It edits a draft and can do nothing else: in command
    /// mode there is no draft, so text is simply not a command, and the keys that
    /// ARE commands were already delivered as their own transitions.
    ///
    /// This is the whole of P4's typing half. `%` arrives here as "%".
    void on(const zengine::input::TextEntered& t, loom::Mail& mail) {
        Row* row = editing_row();
        if (row == nullptr || t.text.empty()) {
            return;
        }
        row->type(t.text);
        repaint(mail);
    }

    /// A pointer button changed, AND the position it changed at.
    ///
    /// Press: take hold of the size handle if the pointer is on it, otherwise of
    /// whatever object is under it, and select that object. Release: let go.
    /// Between them, every PointerMoved authors a new position or a new size.
    ///
    /// The position comes from the message. W-2 had to reconstruct it from the
    /// last motion event, which is wrong whenever the platform reported no motion
    /// in between -- a console generates none while it lacks focus, so the first
    /// click after refocusing grabbed whatever the pointer had last been seen
    /// over. Nothing here remembers a pointer any more.
    void on(const zengine::input::PointerButton& b, loom::Mail& mail) {
        if (b.button != 1 || !understands(b.space)) {
            return;
        }
        if (b.pressed) {
            const std::int64_t id =
                take_hold(state_, session_, workspace_cell_x(b.x), workspace_cell_y(b.y));
            if (id != 0) {
                const bool sizing = session_.drag.resizing;
                select(id);
                say("holding #" + std::to_string(id) +
                        (sizing ? " -- drag to resize it" : " -- drag to move it"),
                    false);
            } else {
                say("nothing there", false);
            }
        } else if (session_.drag.active) {
            const std::int64_t id = session_.drag.id;
            end_drag(session_);
            say("released #" + std::to_string(id), false);
        }
        repaint(mail);
    }

    /// The pointer moved. Outside a drag this weave now has nothing to do with
    /// it -- which is the clearest measure of what W-4 changed: the entire job
    /// of remembering where the pointer is went away with the reconstruction it
    /// existed to serve.
    void on(const zengine::input::PointerMoved& m, loom::Mail& mail) {
        if (!understands(m.space) || !session_.drag.active) {
            return;
        }
        const bool sizing = session_.drag.resizing;
        const Handled done =
            drag_to(state_, session_, workspace_cell_x(m.x), workspace_cell_y(m.y));
        if (!done.accepted()) {
            // A gesture can still propose something the document refuses, and it
            // must say so rather than have the setter quietly correct it. What it
            // may do -- and does -- is STOP at a boundary before proposing; that
            // is a different event, reported below in different words, with the
            // object actually changed.
            say(done.written.refusal, true);
        } else {
            const ui::Element* e = doc::find(state_, session_.drag.id);
            if (e != nullptr) {
                say(sizing ? size_notice(*e, done) : move_notice(*e, done), false);
            }
        }
        repaint(mail);
    }

    /// The session, for a suite that wants to check where a gesture left things.
    /// Read-only: every change still goes through a message and a gesture.
    const Session& session() const { return session_; }
    const WorkshopDoc& document() const { return state_; }

private:
    static bool held(std::int64_t modifiers, std::int64_t which) {
        return (modifiers & which) != 0;
    }

    /// Workshop is a tool made of character cells. A backend reporting pixels is
    /// not speaking about anything this document has, and guessing an equivalence
    /// is exactly the mistake `space` exists to prevent -- so such an event is
    /// ignored rather than mis-placed.
    static bool understands(std::int64_t space) { return space == input::space::kCells; }

    Row* editing_row() {
        for (Row& r : session_.rows) {
            if (r.editing()) {
                return &r;
            }
        }
        return nullptr;
    }

    /// Editing mode, KEY half: the three keys that are editor CONTROLS rather
    /// than text. Commit, cancel, erase -- meanings that belong to Workshop and
    /// that Input deliberately does not know. Everything else a key press might
    /// have meant arrives as TextEntered instead, including `q`, which types a q
    /// here and is the whole reason Ctrl+C is handled above this branch.
    void editing_key(const zengine::input::KeyPressed& k) {
        Row* row = editing_row();
        switch (k.scancode) {
        case input::scan::kReturn: {
            const Commit result = row->commit();
            if (result == Commit::Accepted) {
                say("committed " + row->label() + " = " + row->value(), false);
            } else {
                // Two different failures, and the row already words each one for
                // its own kind: an unparseable draft reads "not <what would have
                // worked>", a refused value carries the setter's own reason. The
                // first live run appended the expected form AGAIN here, which said
                // it twice and then ran off the end of the line -- the notice is
                // one line, so a line's worth is all it may spend.
                (void)result;
                say(row->label() + ": " + row->refusal(), true);
            }
            break;
        }
        case input::scan::kEscape:
            row->cancel();
            say("edit cancelled -- nothing was written", false);
            break;
        case input::scan::kBackspace: row->backspace(); break;
        default: break;
        }
    }

    /// Command mode.
    ///
    /// `hjkl` moves and `Shift+hjkl` resizes, which is one gesture family spelled
    /// two ways rather than two families competing for free keys. W-3 could not
    /// say it: with no modifier on the wire, a second direction gesture cost four
    /// more literal keys (`,` `.` `-` `=`), and the report priced that as P4
    /// arriving a second time. The bill is paid and those four bindings are gone.
    ///
    /// The arrows still step the inspector's rows and `hjkl` still moves, so the
    /// collision W-2 resolved is untouched -- the modifier bought a new gesture,
    /// not a re-argument of an old one.
    void command(const zengine::input::KeyPressed& k) {
        const bool shift = held(k.modifiers, input::mod::kShift);
        switch (k.scancode) {
        case input::scan::kTab: select_next(); break;
        case input::scan::kUp:
            if (session_.cursor > 0) {
                --session_.cursor;
            }
            break;
        case input::scan::kDown:
            if (session_.cursor + 1 < session_.rows.size()) {
                ++session_.cursor;
            }
            break;
        case input::scan::kReturn: begin_edit(); break;
        case input::scan::kN: create_object(); break;
        case input::scan::kD: delete_object(); break;
        case input::scan::kH: shift ? size_by(-1, 0) : move_by(-1, 0); break;
        case input::scan::kJ: shift ? size_by(0, +1) : move_by(0, +1); break;
        case input::scan::kK: shift ? size_by(0, -1) : move_by(0, -1); break;
        case input::scan::kL: shift ? size_by(+1, 0) : move_by(+1, 0); break;
        case input::scan::kLeftBracket: resize_workspace(-4); break;
        case input::scan::kRightBracket: resize_workspace(+4); break;
        case input::scan::kQ: quit(); break;
        default: break;
        }
    }

    /// Make one. The notice names the IDENTITY and not the label, because the
    /// default label is the same word the other objects already carry -- which is
    /// the lesson, arriving at the moment a maker can see it is not a problem.
    void create_object() {
        const std::int64_t id = create(state_, session_);
        say("created #" + std::to_string(id) + " -- a new identity, not a new name", false);
    }

    /// Delete the selected one, and say where the selection went. "Deleted, and
    /// you are now on #2" is one fact; leaving a maker to work out which object
    /// the inspector is suddenly showing is two.
    void delete_object() {
        const std::int64_t was = session_.selected;
        const Written gone = delete_selected(state_, session_);
        if (!gone.accepted) {
            say(gone.refusal, true);
            return;
        }
        say(session_.selected == 0
                ? "deleted #" + std::to_string(was) + " -- the document is empty"
                : "deleted #" + std::to_string(was) + " -- now on #" +
                      std::to_string(session_.selected),
            false);
    }

    /// One cell, through the same document operation a typed X or Y goes through.
    void move_by(std::int64_t ddx, std::int64_t ddy) {
        const Handled moved = nudge(state_, session_, ddx, ddy);
        if (!moved.accepted()) {
            say(moved.written.refusal, true);
            return;
        }
        const ui::Element* e = doc::find(state_, session_.selected);
        if (e != nullptr) {
            say(move_notice(*e, moved), false);
        }
    }

    /// One cell of SIZE, through the same document operation a typed Width or
    /// Height goes through — and through the same projection the pointer uses, so
    /// the two gestures cannot come to hold different opinions about what a
    /// dragged share should become.
    void size_by(std::int64_t dw, std::int64_t dh) {
        const Handled done = grow(state_, session_, dw, dh);
        if (!done.accepted()) {
            say(done.written.refusal, true);
            return;
        }
        const ui::Element* e = doc::find(state_, session_.selected);
        if (e != nullptr) {
            say(size_notice(*e, done), false);
        }
    }

    /// The two notices a direct manipulation produces, in one place so the
    /// pointer and the keyboard cannot describe the same act differently.
    ///
    /// A size notice reports the AUTHORED extents, not the resolved ones: the
    /// whole question W-3 existed to answer is what a maker's hand wrote, and
    /// `71%` is the answer -- `34 x 6 cells` is what the inspector's Resolved
    /// row already says. A boundary is appended in its own words and the notice
    /// stays in the ordinary role, because in this tool the alert role means
    /// exactly one thing: NOTHING WAS WRITTEN. A clamped gesture did write --
    /// the boundary value -- so colouring it as a refusal would erase the
    /// distinction the boundary policy was built to make.
    static std::string edge_of(const Handled& done) {
        return done.clamped() ? " -- " + done.boundary : std::string();
    }
    static std::string move_notice(const ui::Element& e, const Handled& done) {
        return "#" + std::to_string(e.id) + " is at " + std::to_string(e.x) + "," +
               std::to_string(e.y) + edge_of(done);
    }
    static std::string size_notice(const ui::Element& e, const Handled& done) {
        return "#" + std::to_string(e.id) + " is now " + TextForm<ui::Extent>::format(e.width) +
               " x " + TextForm<ui::Extent>::format(e.height) + edge_of(done);
    }

    void begin_edit() {
        if (session_.cursor >= session_.rows.size()) {
            return;
        }
        Row& row = session_.rows[session_.cursor];
        if (!row.editable()) {
            say(row.label() + " is not authored -- it is what the workspace makes of the "
                              "authored value",
                true);
            return;
        }
        row.begin();
        say("editing " + row.label() + " -- enter commits, esc cancels", false);
    }

    /// Resize the workspace: NO authored value changes, and a share visibly
    /// resolves to something else. One keystroke, and the difference between an
    /// authored fact and a resolved one stops being an argument.
    void resize_workspace(std::int64_t delta) {
        std::int64_t want = session_.workspace_w + delta;
        if (want < kWorkspaceMinW) {
            want = kWorkspaceMinW;
        }
        if (want > kWorkspaceW) {
            want = kWorkspaceW;
        }
        session_.workspace_w = want;
        rebuild_rows(); // the resolved row closes over the extent it resolves against
        say("workspace is now " + std::to_string(want) +
                " cells wide -- authored values unchanged",
            false);
    }

    void select_next() {
        if (state_.elements.empty()) {
            return;
        }
        std::size_t at = 0;
        for (std::size_t i = 0; i < state_.elements.size(); ++i) {
            if (state_.elements[i].id == session_.selected) {
                at = i;
                break;
            }
        }
        select(state_.elements[(at + 1) % state_.elements.size()].id);
    }

    void select(std::int64_t id) {
        if (id == session_.selected) {
            return;
        }
        session_.selected = id;
        rebuild_rows();
    }

    /// The rows are rebuilt, never patched. Each one reads through its property
    /// every time it is displayed, so there is no cached value to refresh and no
    /// "refresh the inspector" call anywhere in this file -- the second half of
    /// the old builder's per-row plumbing, also gone.
    void rebuild_rows() { refocus(state_, session_); }

    void say(std::string text, bool bad) {
        session_.notice = std::move(text);
        session_.notice_is_bad = bad;
    }

    void repaint(loom::Mail& mail) {
        mail.publish(paint(state_, session_));
        mail.publish(zengine::surface::SurfaceText{
            zengine::surface::kSlotStatus,
            "[workshop] " + std::to_string(state_.elements.size()) + " objects | " +
                (session_.selected == 0
                     ? std::string("nothing selected")
                     : "selected #" + std::to_string(session_.selected))});
    }

    void quit() {
        host_->quit = true;
        if (host_->request_stop) {
            host_->request_stop();
        }
    }

    HostContext* host_;
    Session session_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_WEAVE_HPP
