// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WEAVE_HPP
#define ZENGINE_WORKSHOP_WEAVE_HPP

// Workshop's own weave: the authored document, the session, and the bindings
// from input MOMENTS to maker GESTURES.
//
// WHY IT IS A HEADER. A weave in the host's anonymous namespace is a weave no
// suite can reach, which would leave `gesture -> document` provable and
// `message -> gesture` not -- and the binding is the one part of the pointer
// path nothing else witnesses. The claim that a press carries its own position
// can only be tested end to end from here. That is the whole reason: no
// framework, no registry, no test hooks. `main()` and the host's boot weave stay
// in the .cpp, because those are the host's job and not Workshop's.
//
// WHAT IS AND IS NOT WORKSHOP'S HERE, because several phases turn on it:
//
//   the authored object   a real zengine::ui::Element in the weave's own state:
//                         gated, schema-carrying, poke-inspectable. There is no
//                         shadow model -- the element the maker selects IS the
//                         element the canvas is painted from and the inspector
//                         reads through. The TYPE is the UI package's; the
//                         object is no less Workshop's state for being spelled
//                         in a shared vocabulary.
//   the geometry          NOT Workshop's. `ui::resolve` turns the authored
//                         extents into a scene and `ui::hit` says what is under
//                         a cell; this file computes neither, and the canvas,
//                         the inspector and the pointer all read one scene.
//   the session           selection, workspace extent, drafts, a drag in flight.
//                         Plain members, never state (the Skin's `announced_`
//                         stance).
//   the screen            screen.hpp, pure, pinned by the suite -- and that
//                         header owns the GESTURES too. This file binds messages
//                         to them and reaches the document through nothing else,
//                         so every maker action the suite drives is the same one
//                         a maker's hand drives.
//
// THE INPUT REALITY, named where a reader will hit it. Three reconstructions
// this file does NOT perform, because the Input vocabulary carries the facts
// that were simultaneously true (README.md#input--the-input-package):
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

// THE DOCUMENT HAS A LIFE LONGER THAN THE PROCESS, and the interesting part of
// that is here rather than in the codec. Two bindings (`^s`, `^o`) and three
// questions with them, each answered in the method that needs it:
//
//   what does Save do about an open editor draft   refuse (see save_document)
//   what happens to the SESSION on a load          it is re-established, not
//                                                  preserved (load_document)
//   how does a maker know whether work is saved    the status line COMPARES the
//                                                  live document with the one on
//                                                  disk (`saved_`), rather than
//                                                  keeping a dirty flag that
//                                                  every write site would have to
//                                                  remember to set

// ONE AUTHORED OBJECT TAKES ITS CONTEXT FROM ANOTHER, and this file is where you
// can see how little that costs. There is no message, no binding, no gesture and
// no session field for it: the relationship is an ordinary editable property
// (`Context` in the inspector), authored through the same Enter/type/Enter a
// width takes and refused through the same one-line notice. The only two places
// it shows here are a move notice that names the frame a position is authored
// IN, and the delete refusal, which arrives from the document with the
// dependents named.
//
// The opening document is deliberately FLAT. A maker's first screen shows two
// independent rectangles; composition is something they do, not something they
// arrive inside, and the simple case is not made more expensive by the
// capability existing.

// THE GRAPHICAL WORKSHOP HAS HANDS, and the measure of it is how little of this
// file that takes. There is no graphical selection, no SDL drag state, no
// graphical hit test and no second gesture path: a click in the SDL window
// reaches `take_hold` and a drag reaches `drag_to`, the same functions the
// terminal's pointer drives, writing through the same document operations with
// the same clamp/refuse law. Two boundaries make that true:
//
//   pointer space   an event is PROJECTED (screen.hpp's `canvas_point_of`) from
//                   whichever medium reported it. Pixels are not refused; they
//                   are converted, once, by the package that knows the
//                   conversion -- docs/reference/pointer-spaces.md. An event
//                   this application cannot place is ignored rather than
//                   mis-placed.
//   close           a native close request arrives as surface truth and reaches
//                   the quit policy `q` already had.

#include "persist.hpp"
#include "screen.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/terminal/input_lex.hpp> // ONE command grammar, Loom's -- never a second one here
#include <zen/terminal/session.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace zengine::workshop {

/// What the weave needs from the host and cannot get by message: the stop
/// lever, and — since WT-1 — the terminal participant the host mounted.
/// `dir`/`so()` are the host's own boot bookkeeping and are filled in
/// there — kept whole in this header so a suite can construct one without
/// linking the host.
struct HostContext {
    bool quit = false;
    std::function<void()> request_stop;
    std::string dir;

    /// THE TERMINAL PARTICIPANT WORKSHOP PRESENTS — non-owning, and null when the
    /// host mounted none.
    ///
    /// IT ARRIVES THE SAME WAY `request_stop` DOES, and that is the whole of the
    /// composition's wiring: the host owns the bus, mounts an ordinary
    /// `loom::TerminalSession` on it with `host_mount_terminal`, and hands the
    /// weave the pointer it got back. There is no global, no registry, no
    /// singleton and no lookup — a suite constructs a HostContext with whatever
    /// participant it wants, or none, and the weave cannot reach one any other
    /// way.
    ///
    /// A POINTER IS NOT AN IDENTITY AND IT IS NOT AUTHORITY. The bus owns the
    /// participant; holding this changes nothing about who Workshop is. Every
    /// message authored through it leaves through the PARTICIPANT'S own door,
    /// stamped by the bus with the PARTICIPANT'S WeaveId and gated against the
    /// PARTICIPANT'S grant — and `WorkshopWeave`'s own grant is untouched by any
    /// of it. The two identities sit on one screen and stay two, which is the
    /// invariant this whole phase exists to keep.
    ///
    /// LIFETIME: the bus owns it, so a pane can be built and destroyed without
    /// ending the participant, and ending the participant is the host's explicit
    /// act. After that act this pointer must not be used — which is why the pane
    /// holds SNAPSHOTS (`TerminalPane::shown`, taken by value) and never reads a
    /// transcript while painting.
    loom::TerminalSession* terminal = nullptr;

    /// The one file this Workshop saves to and loads from.
    ///
    /// It is the host's to choose (`--document <path>`, defaulted) and it is
    /// SESSION, not document: a file cannot sensibly contain its own location,
    /// and the same document opened from two places is the same document. There
    /// is deliberately no recent-files list, no picker and no project concept —
    /// one path is the smallest thing that lets a maker close Workshop and come
    /// back to their work.
    ///
    /// Empty means no document file was chosen, and save/load say so rather
    /// than guessing one.
    std::string document_path;

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
                                          zengine::surface::SurfaceReady,
                                          zengine::surface::SurfaceExtent,
                                          zengine::surface::SurfaceCloseRequested>,
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
        open_on_first();
    }

    /// A Skin claimed the surface and said hello: give it the whole screen. The
    /// operator weave's precedent, and the only thing Workshop needs in order to
    /// paint for the first time -- so load order decides nothing here either.
    void on(const zengine::surface::SurfaceReady&, loom::Mail& mail) { repaint(mail); }

    /// THE SURFACE SAID HOW MUCH ROOM IT HAS. Take it, and lay the screen out again.
    ///
    /// This is the one message that flows medium -> application in this whole tool, and it is
    /// the whole of G-2's plumbing: a Skin measures its drawable, Workshop believes it, and a
    /// larger window becomes a larger Workshop instead of a larger picture of a small one.
    /// A medium with no opinion -- the terminal Skins -- never sends it, so a terminal run is
    /// exactly the run it was before, at the minimum extent.
    ///
    /// IT IS TAKEN, NOT OBEYED. `adopt_screen` clamps into what this composition is honest
    /// on, so an extent smaller than the minimum leaves the screen alone and lets the medium
    /// clip, and an absurd one (this arrives as a `ZEN_SHAPE` off the bus, whose fields are
    /// whatever the sender put in them) is bounded before any arithmetic touches it.
    ///
    /// IT REPAINTS ONLY ON A REAL CHANGE. The Skin already guards its own publishing, so this
    /// second guard is not redundancy for its own sake -- it is what makes the clamps above
    /// safe to state: two different extents that both clamp to the minimum are one screen,
    /// and a maker dragging a window edge across that boundary should not see it flicker.
    ///
    /// The rows are rebuilt because the `Resolved` row closes over the workspace extent, and
    /// the workspace extent is exactly what just changed.
    void on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail) {
        if (!adopt_screen(session_, e.width, e.height)) {
            return;
        }
        rebuild_rows();
        repaint(mail);
    }

    /// THE SURFACE WAS ASKED TO CLOSE -- by the window manager, the close box,
    /// the platform. Workshop applies the quit policy it already has.
    ///
    /// It is the SAME policy `q` and Ctrl+C reach, deliberately: a close box is a
    /// new way for the request to ARRIVE, not a new thing for it to mean. In
    /// particular it does not ask about unsaved work -- the status line says
    /// `UNSAVED` and `^o` can already discard authored work without a
    /// confirmation. Making the close box the one gesture that argues back would
    /// be answering a product question nobody has asked, in whichever phase
    /// happened to add the button.
    ///
    /// It is not a key. Nothing here reads a scancode, and no backend
    /// synthesized one: a native close request and a maker pressing `q` are
    /// different events that this application chooses to answer the same way,
    /// and the choice is visible precisely because they arrive separately.
    void on(const zengine::surface::SurfaceCloseRequested&, loom::Mail&) { quit(); }

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
        // SHIFT+SPACE OPENS AND CLOSES THE TERMINAL, above every mode, because a
        // maker who cannot reach the pane from inside a half-typed width has a
        // pane with a trapdoor rather than a toggle.
        //
        // AND IT SWALLOWS ITS OWN TEXT. A key transition and the character it
        // produced are two facts that were simultaneously true, and both arrive:
        // the SDL and Win32 backends report Shift+Space as `KeyPressed{Space,
        // shift}` AND `TextEntered{" "}`. Without this the gesture that closed
        // the pane would also have typed a space into the line it closed over.
        // The flag is set here and cleared by the very next key OR the very next
        // text, so it cannot outlive the moment it belongs to and eat a space a
        // maker meant -- which a lingering one-shot flag would do on a backend
        // that reports the key and no text at all.
        const bool toggling =
            k.scancode == input::scan::kSpace && held(k.modifiers, input::mod::kShift);
        swallow_toggle_text_ = toggling;
        if (toggling) {
            toggle_terminal();
            repaint(mail);
            return;
        }
        // Save and open are the two commands that mean the same thing in every
        // mode, so they sit beside Ctrl+C rather than inside `command()`: a
        // maker halfway through typing a width still means "save my work" when
        // they press ^s, and what Workshop does about the half-typed width is
        // save_document's answer, not a reason to make the key unreachable.
        //
        // BOTH ARE TRUTHFUL ON BOTH BACKENDS, and ^s in particular is a claim
        // worth source-tracing rather than assuming. Ctrl+S is byte 0x13, which
        // is XOFF: on a terminal that still has flow control it never reaches
        // the application at all. The Input weave's TerminalReader clears IXON
        // when it takes raw mode (input.cpp), so the byte arrives, and the
        // parser reads 1..26 as Ctrl+letter with the modifier MEASURED rather
        // than inferred. On the Win32 console it is VK_S with the control-key
        // state the record already carries. Neither backend was changed.
        if (held(k.modifiers, input::mod::kCtrl)) {
            if (k.scancode == input::scan::kS) {
                save_document();
                repaint(mail);
                return;
            }
            if (k.scancode == input::scan::kO) {
                load_document();
                repaint(mail);
                return;
            }
        }
        // THREE MODES NOW, and the order is the priority. The overlay is what a
        // maker most recently asked for, so while it is open it has the keys --
        // an open inspector draft is not cancelled, not committed and not
        // touched, and is still there when the pane closes.
        if (session_.terminal.open) {
            terminal_key(k);
        } else if (editing_row() != nullptr) {
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
    /// Workshop maps no key to any character. `%` arrives here as "%".
    void on(const zengine::input::TextEntered& t, loom::Mail& mail) {
        if (swallow_toggle_text_) {
            swallow_toggle_text_ = false;
            if (t.text == " ") {
                return; // the space Shift+Space produced belongs to the toggle
            }
        }
        if (t.text.empty()) {
            return;
        }
        // The overlay is where typing goes while it is open. Same rule as the
        // keys, same reason, and the inspector draft underneath is untouched.
        if (session_.terminal.open) {
            session_.terminal.input += t.text;
            repaint(mail);
            return;
        }
        Row* row = editing_row();
        if (row == nullptr) {
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
    /// The position comes from the message. Reconstructing it from the last
    /// motion event is wrong whenever the platform reported no motion in between
    /// -- a console generates none while it lacks focus, so the first click after
    /// refocusing would grab whatever the pointer had last been seen over.
    /// Nothing here remembers a pointer.
    void on(const zengine::input::PointerButton& b, loom::Mail& mail) {
        // WHILE THE OVERLAY IS OPEN THE POINTER DOES NOTHING, and this is one
        // rule rather than a focus framework. The pane covers the bottom-right
        // of the screen, workspace included, so a press there would take hold of
        // an object the maker cannot see -- and a press just outside it would
        // move the document out from under a mode they are typing in. One
        // sentence covers both: while the terminal is open, the terminal has the
        // input. There is no focus object, no hit test against the pane, no
        // capture and no z-order; closing it restores every gesture exactly.
        if (session_.terminal.open) {
            return;
        }
        const PointedAt at = canvas_point_of(b.space, b.x, b.y);
        if (b.button != 1 || !at.understood) {
            return;
        }
        if (b.pressed) {
            const std::int64_t id = take_hold(state_, session_, workspace_cell_x(at.cell.x),
                                              workspace_cell_y(at.cell.y));
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

    /// The pointer moved. Outside a drag this weave has nothing to do with it:
    /// the job of remembering where the pointer is went away with the
    /// reconstruction it existed to serve.
    void on(const zengine::input::PointerMoved& m, loom::Mail& mail) {
        if (session_.terminal.open) {
            return; // the same rule as the press above: the overlay has the input
        }
        const PointedAt at = canvas_point_of(m.space, m.x, m.y);
        if (!at.understood || !session_.drag.active) {
            return;
        }
        const bool sizing = session_.drag.resizing;
        const Handled done = drag_to(state_, session_, workspace_cell_x(at.cell.x),
                                     workspace_cell_y(at.cell.y));
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

    // ---- The terminal overlay ------------------------------------------------
    //
    // WORKSHOP PRESENTS A PARTICIPANT; IT DOES NOT BECOME ONE. Everything below
    // reads a snapshot or calls an ordinary method on the participant the host
    // mounted. Nothing here sends a message as Workshop, and nothing here can:
    // `WorkshopWeave`'s own grant carries `SurfaceCanvas` and `SurfaceText` and
    // nothing else, and it has no bus to speak through outside a handler anyway.
    // The participant's door is the participant's.

    /// Open or close the pane. The whole of the mode change.
    void toggle_terminal() {
        session_.terminal.open = !session_.terminal.open;
        if (!session_.terminal.open) {
            // Said on the way OUT and not on the way in, because the notice line
            // is not painted while the pane covers it -- an "opened" notice would
            // be a sentence nobody could read that then reappeared, stale, at the
            // moment it stopped being true. The pane's own header says how to
            // close it, which is the fact a maker needs while it is open.
            say("terminal closed -- shift+space reopens it", false);
        }
        refresh_terminal();
    }

    /// Editing mode for the command line: the three keys that are controls
    /// rather than text, exactly as the inspector's editor has.
    ///
    /// Escape CLEARS THE LINE AND DOES NOT CLOSE THE PANE. One gesture opens and
    /// closes this thing, and giving Escape a second way out would mean a maker
    /// who wanted to abandon a half-typed command sometimes lost the pane too.
    void terminal_key(const zengine::input::KeyPressed& k) {
        switch (k.scancode) {
        case input::scan::kReturn: submit_terminal_line(); break;
        case input::scan::kBackspace: erase_one_character(session_.terminal.input); break;
        case input::scan::kEscape: session_.terminal.input.clear(); break;
        default: break;
        }
        refresh_terminal();
    }

    /// AUTHOR ONE LINE THROUGH THE PARTICIPANT'S OWN DOOR.
    ///
    /// The grammar is LOOM'S, not Workshop's: `tokenize`, `parse_address` and
    /// `lex_arg` all come from <zen/terminal/input_lex.hpp>, which is the same
    /// parser the standalone terminal uses. That is the whole reason WT-1 lifted
    /// `parse_address` out of that REPL's anonymous namespace -- a second author
    /// of one grammar is two grammars, and the day `#12` grows a second form only
    /// one of them would learn it.
    ///
    /// TWO VERBS, and the smallness is deliberate: `send` and `ask` are the two
    /// acts an ordinary participant has, and every other thing the standalone
    /// REPL offers is either a renderer (`show`, `log`), a convenience over these
    /// two (`request`, `approve`), or a HOST power no participant holds
    /// (`weaves`, `tap`, `notify`). A pane that grew those would be growing a
    /// second terminal, which is the one thing this phase is not allowed to do.
    ///
    /// The line is recorded on the participant BEFORE it is understood, so a
    /// command that turns out to be nonsense is still part of that participant's
    /// own chronology -- a record of effects with no causes is not a session.
    void submit_terminal_line() {
        const std::string line = session_.terminal.input;
        session_.terminal.input.clear();
        if (line.empty()) {
            return;
        }
        if (host_->terminal == nullptr) {
            // Nothing to author through, and nowhere to record the attempt: the
            // participant IS the transcript. So the tool's own notice line says
            // it, and it is visible the moment the pane closes.
            say("no terminal participant is mounted on this bus -- nothing was authored", true);
            return;
        }
        loom::TerminalSession& me = *host_->terminal;
        me.record_command(line);

        const std::vector<loom::Token> tok = loom::tokenize(line);
        const std::string verb = tok.empty() ? std::string() : tok[0].text;
        loom::Address to;
        std::uint64_t version = 0;
        if ((verb == "send" || verb == "ask") && tok.size() >= 4 &&
            loom::parse_address(tok[1].text, to) && loom::parse_u64(tok[3].text, version) &&
            version <= kMaxVersion) {
            std::vector<loom::Arg> args;
            for (std::size_t i = 4; i < tok.size(); ++i) {
                args.push_back(loom::lex_arg(tok[i]));
            }
            const loom::TerminalResult r =
                verb == "ask" ? me.ask(to, tok[2].text, static_cast<std::uint32_t>(version), args)
                              : me.send(to, tok[2].text, static_cast<std::uint32_t>(version), args);
            if (!r) {
                // A LOCAL refusal, and it is recorded as this participant's own
                // notice rather than dressed up as an answer: nothing was
                // authored, so nothing was denied by anybody. The core already
                // words each outcome; repeating it here in different words would
                // be a second vocabulary for one fact.
                me.record_notice(std::string(loom::name_of(r.outcome)) +
                                 (r.detail.empty() ? "" : ": " + r.detail));
            }
            return;
        }
        // THE WHOLE SENTENCE, at whatever length it takes to be a complete one. It is
        // recorded on the participant, unshortened, exactly as every other entry is: the pane
        // wraps it across as many of its own rows as it needs (`detail::wrap`), so the length
        // of this string is a question about the GRAMMAR and never about the furniture. Before
        // G-2 it was fitted into one 56-cell row and a maker asking how to send a message got
        // `this pane speaks two verbs: \`send <addr> <Shape> <ver...` -- the answer truncated
        // at exactly the point it started being an answer.
        me.record_notice("this pane speaks two verbs, and `ask` takes the same form as `send`: "
                         "send <addr> <Shape> <version> [args] -- an address is #12 for one "
                         "weave, @office for whoever holds a role, or * for everyone");
    }

    /// Take the pane's snapshot of the participant.
    ///
    /// BY VALUE, EVERY TIME, and never a retained reference into the transcript.
    /// `Transcript::entries()`/`tail()` return copies precisely so a presentation
    /// may hold the result across anything at all, including the destruction of
    /// the participant it came from. So the canvas Workshop publishes cannot
    /// contain a read of a freed transcript no matter when a Skin paints it, and
    /// the only pointer in this composition is dereferenced here, inside a
    /// handler, on the same thread the host pumps.
    void refresh_terminal() {
        TerminalPane& pane = session_.terminal;
        pane.attached = host_->terminal != nullptr;
        pane.id = pane.attached ? host_->terminal->id() : loom::WeaveId{};
        pane.shown.clear();
        pane.earlier = 0;
        pane.dropped = 0;
        if (!pane.attached || !pane.open) {
            return; // nothing is painted from it, so nothing is copied
        }
        // AS MANY ENTRIES AS THIS PANE CAN SHOW WHOLE, which is no longer the same as "as
        // many entries as it has rows": since G-2 a line too long for the pane WRAPS rather
        // than being cut, so one entry can cost several rows. `entries_that_fit` is the one
        // place that arithmetic lives, and `paint_terminal` carries out the same choice with
        // the same call -- two answers here would be a pane whose omission marker lied.
        //
        // A row apiece is the floor, so `tail(rows)` is always at least as many entries as
        // can possibly fit, and the fitting only ever takes fewer.
        const Screen sc = screen_of(session_);
        const loom::Transcript& record = host_->terminal->transcript();
        std::vector<loom::TranscriptEntry> newest = record.tail(sc.terminal_rows);
        const std::size_t fits = entries_that_fit(newest, sc.terminal_w, sc.terminal_rows);
        newest.erase(newest.begin(),
                     newest.end() - static_cast<std::ptrdiff_t>(fits));
        pane.shown = std::move(newest);
        pane.earlier = record.size() - pane.shown.size();
        pane.dropped = record.evicted();
    }

    /// Command mode.
    ///
    /// `hjkl` moves and `Shift+hjkl` resizes, which is one gesture family spelled
    /// two ways rather than two families competing for free keys. It is spellable
    /// only because the wire carries the modifiers held at the transition; with
    /// no modifier vocabulary a second directional gesture costs four more
    /// literal keys (`,` `.` `-` `=`), and those four bindings do not exist.
    ///
    /// The arrows step the inspector's rows and `hjkl` moves, which is why
    /// neither pair had to be re-argued: the modifier bought a new gesture, not a
    /// second meaning for an old key.
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

    // ---- Save and open -------------------------------------------------------

    /// Write the document to its file.
    ///
    /// THE DRAFT POLICY, and it is a refusal. If a row is open with an
    /// uncommitted draft, nothing is saved and the notice says which row. The
    /// two alternatives were weighed against how this tool already behaves:
    ///
    ///   save the committed value quietly   would write the OLD width while a
    ///                                      NEW one is on the screen with a
    ///                                      cursor after it. The file would then
    ///                                      disagree with what the maker is
    ///                                      looking at, and nothing would say so.
    ///   commit the draft for them          is auto-commit. Workshop has spent
    ///                                      five phases keeping "the draft is
    ///                                      not the property" true; a save that
    ///                                      writes a value the maker never
    ///                                      confirmed would end that for a
    ///                                      keystroke's convenience.
    ///
    /// So it refuses, in the alert role, which in this tool means exactly one
    /// thing: NOTHING WAS WRITTEN -- true of the document and now also of the
    /// file. Enter commits and Escape cancels; both are one key and both are on
    /// the help line.
    void save_document() {
        if (host_->document_path.empty()) {
            say(kNoDocumentFile, true);
            return;
        }
        const Row* draft = editing_row();
        if (draft != nullptr) {
            say(draft->label() + " is still being edited -- enter commits, esc cancels; "
                                 "nothing was saved",
                true);
            return;
        }
        const Written written = persist::save_file(host_->document_path, state_);
        if (!written.accepted) {
            say(written.refusal, true);
            return;
        }
        // What is on disk is now what is in memory. Recorded as a COPY of the
        // document rather than as a flag, so "saved" cannot drift from the truth
        // it describes -- see WorkshopDoc's operator==.
        saved_ = state_;
        say("saved " + host_->document_path, false);
    }

    /// Replace the document with the one in its file.
    ///
    /// A LOAD IS NOT A MERGE and it is not an import: the document that was here
    /// is gone, identities and all, and the one in the file takes its place with
    /// ITS identities. `persist::load_file` is a transaction -- the live document
    /// is untouched unless the whole candidate is legal -- so everything below
    /// runs only on success.
    ///
    /// THE SESSION IS RE-ESTABLISHED, NOT PRESERVED, and that distinction is the
    /// whole of what a load costs the session. Every session fact points at the
    /// document that is gone:
    ///
    ///   the drag       held an identity and an offset from an object that may
    ///                  not exist. It is cancelled, so a pointer already down
    ///                  cannot drag a new object it never grabbed.
    ///   the drafts     lived in rows bound to the old objects. Rebuilding the
    ///                  inspector ends them.
    ///   the selection  is the sharp one. KEEPING the old id would silently
    ///                  alias whatever new object happened to carry that number
    ///                  -- the same identity confusion the whole arc is arranged
    ///                  to prevent, arriving through the back door. So the
    ///                  selection is re-established by the SAME rule that opens
    ///                  a fresh Workshop: the first object, or none.
    ///
    /// What is NOT reset is the workspace extent, and that is deliberate: it is
    /// a fact about the window this document is being looked at through, not
    /// about the document. Loading a file under a different workspace is exactly
    /// how a maker sees that a share was authored rather than resolved.
    void load_document() {
        if (host_->document_path.empty()) {
            say(kNoDocumentFile, true);
            return;
        }
        const Written read = persist::load_file(host_->document_path, state_);
        if (!read.accepted) {
            // The document, the selection, the drag and any draft are all
            // exactly as they were. A failed load costs a maker nothing but the
            // notice.
            say(read.refusal, true);
            return;
        }
        end_drag(session_);
        open_on_first();
        saved_ = state_;
        say("loaded " + host_->document_path + " -- " + std::to_string(state_.elements.size()) +
                " objects",
            false);
    }

    /// Open onto the first object, or onto none. The rule a fresh Workshop uses
    /// and the rule a load uses, written once so a loaded document and a new one
    /// cannot come to open differently.
    void open_on_first() {
        session_.selected = state_.elements.empty() ? 0 : state_.elements.front().id;
        rebuild_rows();
    }

    /// Make one. The notice names the IDENTITY and not the label, because the
    /// default label is the same word the other objects already carry -- which is
    /// the lesson, arriving at the moment a maker can see it is not a problem.
    void create_object() {
        const std::int64_t id = create(state_, session_);
        if (id == 0) {
            // The mint is spent. Unreachable by pressing `n`; reachable in one
            // line of a loaded file, which is why this gesture has an answer
            // rather than an overflow.
            say("this document has no identity left to give -- nothing was created", true);
            return;
        }
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
    /// whole question a resize notice answers is what a maker's hand wrote, and
    /// `71%` is that -- `34 x 6 cells` is what the inspector's Resolved
    /// row already says. A boundary is appended in its own words and the notice
    /// stays in the ordinary role, because in this tool the alert role means
    /// exactly one thing: NOTHING WAS WRITTEN. A clamped gesture did write --
    /// the boundary value -- so colouring it as a refusal would erase the
    /// distinction the boundary policy was built to make.
    static std::string edge_of(const Handled& done) {
        return done.clamped() ? " -- " + done.boundary : std::string();
    }
    /// A move notice names the AUTHORED position and, when there is one, the
    /// frame that position is authored IN. `#2 is at 2,1 in #1` is one fact; a
    /// bare `#2 is at 2,1` beside a rectangle visibly nowhere near cell 2,1
    /// would be two, and a maker would have to work out which. Nothing here
    /// reports the resolved position: that is what the picture already is.
    static std::string move_notice(const ui::Element& e, const Handled& done) {
        const std::string where = e.context == ui::kRootContext
                                      ? std::string()
                                      : " in #" + std::to_string(e.context);
        return "#" + std::to_string(e.id) + " is at " + std::to_string(e.x) + "," +
               std::to_string(e.y) + where + edge_of(done);
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
        // The ceiling is THIS SCREEN'S room, not a constant: since G-2 a surface can offer
        // more of it, and a `]` that stopped at 48 cells on a window with room for eighty
        // would be the tool refusing space it had already been given.
        const std::int64_t room = screen_of(session_).room_w;
        std::int64_t want = session_.workspace_w + delta;
        if (want < kWorkspaceMinW) {
            want = kWorkspaceMinW;
        }
        if (want > room) {
            want = room;
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

    /// The status line: how many objects, which one is selected, WHICH FILE, and
    /// whether it matches.
    ///
    /// The file half is not decoration. Once work survives a process, the first
    /// thing a maker needs to know is whether the thing in front of them is the
    /// thing on disk, and the second is which file that is. `unsaved` is
    /// computed by comparing, so it is right by construction: a fresh Workshop
    /// says `unsaved` because its opening document has genuinely never been
    /// written, and a document edited and then edited BACK says `saved`, because
    /// it is.
    std::string status_line() const {
        std::string line =
            "[workshop] " + std::to_string(state_.elements.size()) + " objects | " +
            (session_.selected == 0 ? std::string("nothing selected")
                                    : "selected #" + std::to_string(session_.selected));
        if (!host_->document_path.empty()) {
            line += " | " + host_->document_path + (state_ == saved_ ? " saved" : " UNSAVED");
        }
        return line;
    }

    void repaint(loom::Mail& mail) {
        refresh_terminal(); // the pane is a snapshot, and a snapshot is only true when taken
        mail.publish(paint(state_, session_));
        mail.publish(
            zengine::surface::SurfaceText{zengine::surface::kSlotStatus, status_line()});
    }

    void quit() {
        host_->quit = true;
        if (host_->request_stop) {
            host_->request_stop();
        }
    }

    /// What to say when there is no file to save to or load from. One sentence,
    /// in one place, because a maker who meets it twice should not have to
    /// wonder whether they met two different problems.
    static constexpr const char* kNoDocumentFile =
        "no document file -- start Workshop with --document <path>";

    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and
    /// a schema version is 32, so a wider number is REFUSED rather than
    /// truncated -- `send @x Foo 4294967297` must not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    HostContext* host_;
    Session session_;

    /// One moment's worth of memory: the toggle's own keystroke produced a
    /// space, and that space is not text a maker typed. Set by the toggle,
    /// cleared by the next key or the next text, so it can never outlive its
    /// moment.
    bool swallow_toggle_text_ = false;

    /// The document as it is ON DISK, or an empty one when nothing has been
    /// written yet. Session, emphatically: it is a copy kept so the status line
    /// can answer "is this saved" by comparing rather than by trusting a flag.
    WorkshopDoc saved_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_WEAVE_HPP
