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
#include "setup_persist.hpp"

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

    /// The one file this Workshop's SETUP saves to and restores from (WS-0).
    ///
    /// A SECOND PATH BESIDE THE DOCUMENT'S, AND NOT A PROJECT. It is the host's
    /// to choose (`--setup <path>`, defaulted) for the same reasons the
    /// document's is, and it is a different file for the reason setup_persist.hpp
    /// gives: the same document is worth opening in two arrangements and the
    /// same arrangement is worth using over two documents, and a single
    /// container would make both unsayable.
    ///
    /// WORKSHOP MANAGES ONE ACTIVE PATH. Several setup files may exist because a
    /// maker can launch with a different one; there is no catalog, no recent
    /// list, no picker and no import. Empty means no setup file was chosen, and
    /// naming/restoring say so rather than guessing one.
    std::string setup_path;

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
                                          zengine::surface::SurfaceCloseRequested,
                                          zengine::builder::BuildStatus>,
                             loom::Emit<zengine::surface::SurfaceCanvas,
                                        zengine::surface::SurfaceText,
                                        zengine::builder::StatusRequested,
                                        zengine::builder::BuildRequested>> {
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
        if (!adopt_screen(session_, e.width, e.height, e.text_advance_px, e.text_line_px)) {
            return;
        }
        // THE ROWS ARE REBUILT AND A LIVE DRAFT IS CARRIED ACROSS (HD-5). The resolved row
        // closes over the extent it resolves against, so the rebuild is not optional -- but
        // this is the ONE rebuild that happens for a reason having nothing to do with the
        // maker. A window dragged is not a gesture aimed at the inspector, and until HD-5
        // measured it on the pristine tree it silently threw away whatever was half-typed
        // into a property, its refusal and the cursor with it. Every OTHER caller of
        // `rebuild_rows` follows a change of selection or of document, where dropping the
        // draft is the right answer and carrying it would put it on a different object.
        refocus_keeping_draft(state_, session_);
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
        // THE SWALLOW IS A STRING SINCE WS-0, and the widening is the whole of what a SECOND
        // printable trigger cost. `s` opens a one-line name editor exactly as Shift+Space
        // opens the pane, and the same two facts arrive: the key transition, and the `s` the
        // platform's layout made of it. One flag hard-coded to `" "` could not have covered
        // both, and a second flag beside it would have been the same mechanism written twice.
        // Cleared here, on every key, for the reason it always was: it belongs to one moment.
        swallow_text_ = toggling ? " " : "";
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
        // FOUR MODES NOW, and the order is the priority. The overlay is what a
        // maker most recently asked for, so while it is open it has the keys --
        // an open inspector draft is not cancelled, not committed and not
        // touched, and is still there when the pane closes.
        //
        // THE PICKER SITS SECOND, AND IT CANNOT COLLIDE WITH THE THIRD. It is
        // reachable only from command mode, and command mode is exactly the
        // state in which no row is being edited -- so "picker open" and "a draft
        // is live" cannot both be true. The order is written anyway rather than
        // left to that argument: an ordering that depends on a reachability
        // proof is one refactor away from being wrong silently, and this costs
        // one line. It is NOT a focus framework: there is no focused panel, no
        // z-order and no capture -- one `if` per mode, the same shape the
        // overlay's was.
        //
        // FIVE MODES SINCE WS-0, and the new one sits SECOND for the picker's own reason.
        // The setup-name editor is reachable only from command mode -- `s` is a command --
        // so "naming" and "a draft is live" cannot both be true, and neither can "naming"
        // and "the picker is open". The order is written anyway rather than left to that
        // argument, because an ordering that depends on a reachability proof is one refactor
        // away from being wrong silently, and this costs one line. It is still not a focus
        // framework: one `if` per mode, and nothing captures anything.
        if (session_.terminal.open) {
            terminal_key(k);
        } else if (session_.setup.naming.open) {
            naming_key(k, mail);
        } else if (session_.panels.picker.open) {
            picker_key(k, mail);
        } else if (editing_row() != nullptr) {
            editing_key(k);
        } else {
            command(k, mail);
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
        if (!swallow_text_.empty()) {
            const std::string owed = swallow_text_;
            swallow_text_.clear();
            if (same_keystroke(t.text, owed)) {
                return; // the character the trigger produced belongs to the trigger
            }
        }
        if (t.text.empty()) {
            return;
        }
        // THE NAME EDITOR TAKES TEXT WHEREVER THE KEYS GO. It sits under the overlay for the
        // same reason an inspector draft does -- the pane is what a maker most recently asked
        // for -- and above everything else, because while it is open there is nothing else
        // for a character to mean.
        if (!session_.terminal.open && session_.setup.naming.open) {
            session_.setup.naming.line.type(t.text);
            repaint(mail);
            return;
        }
        // The overlay is where typing goes while it is open. Same rule as the
        // keys, same reason, and the inspector draft underneath is untouched.
        if (session_.terminal.open) {
            // AT THE CARET, WHICH SINCE HD-3 IS NOT ALWAYS THE END. `type` is the only door
            // that moves the text and the caret together, so a keystroke in the middle of a
            // line cannot leave one behind.
            session_.terminal.input.type(t.text);
            // The line changed, so what could be said next changed with it (HD-2).
            // Typing IS the completion gesture -- there is no second key that
            // summons the list, because a maker who has to ask for discovery has to
            // know discovery is there.
            refresh_terminal();
            repaint(mail);
            return;
        }
        // THE PICKER TAKES NO TEXT AND TYPES NONE. It is chosen from with arrow
        // keys, so every character produced while it is open belongs to nothing
        // -- and the `p` that opened it produces one. Without this the picker
        // would be unreachable from an open draft anyway (there can be none),
        // but the rule is written where a reader looks for it rather than left
        // as a consequence of the branch below.
        if (session_.panels.picker.open) {
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
    /// THE WHOLE ROUTING RULE, in the order it is written below (PNL-2):
    ///
    ///     the terminal overlay, while it is open -- it has the pointer entirely
    ///     a visible panel, where a press lands inside its resolved bounds
    ///     the workspace and the document underneath
    ///
    /// The first is a MODE and the second is a PLACE, and the difference is the
    /// whole design: the overlay takes every pointer event anywhere, because
    /// while it is open it is what the maker is doing; a panel takes only the
    /// presses that land on it, because a maker with a panel open is still
    /// working in the workspace beside it. Neither is a focus object, a capture,
    /// a z-order or a widget tree -- one `if` each, and the same shape the
    /// keyboard's four modes already have.
    ///
    /// The position comes from the message. Reconstructing it from the last
    /// motion event is wrong whenever the platform reported no motion in between
    /// -- a console generates none while it lacks focus, so the first click after
    /// refocusing would grab whatever the pointer had last been seen over.
    /// Nothing here remembers a pointer.
    void on(const zengine::input::PointerButton& b, loom::Mail& mail) {
        // WHILE THE OVERLAY IS OPEN THE WORKSPACE GETS NOTHING, and that half is
        // unchanged since PNL-2: the pane covers the bottom-right of the screen,
        // workspace included, so a press there would take hold of an object the
        // maker cannot see -- and a press just outside it would move the document
        // out from under a mode they are typing in. One sentence covers both:
        // while the terminal is open, the terminal has the input. There is no
        // focus object, no capture and no z-order; closing it restores every
        // gesture exactly.
        //
        // WHAT HD-3 CHANGED IS THAT THE TERMINAL NOW DOES SOMETHING WITH IT --
        // and only inside itself. The mode is still a MODE: it takes every
        // pointer event anywhere, and `terminal_press` decides whether one of the
        // regions the Terminal OWNS wants it. A press that lands on none of them
        // is still consumed by the mode rather than falling through, which is the
        // whole of what stops a click on the pane's empty middle from selecting
        // an object behind it. That is the first PLACE-WITHIN-A-MODE this
        // application has, and it is a bounds test against the Terminal's own
        // regions rather than a widget registry: nothing below is an entity,
        // nothing has an identity, and closing the pane removes all of it because
        // there is nothing to remove.
        if (session_.terminal.open) {
            // A RELEASE STILL ENDS A DRAG THAT BEGAN ON THE WORKSPACE, and this is a
            // repair rather than a new rule (PNL-2's own: "a gesture that began on the
            // workspace owns the pointer until it ends, so its release must end it
            // wherever the maker's hand happens to be"). Opening the pane mid-drag used
            // to swallow the release, leaving `drag.active` true with the button up --
            // after which closing the pane made the next bare motion drag an object
            // nobody was holding. Occluding a release is the one thing this file already
            // knew not to do; the overlay was doing it by arriving first.
            //
            // SILENTLY, WHICH IS THE ONE PLACE THIS FILE'S "SAY SO" RULE DOES NOT APPLY.
            // `toggle_terminal` already wrote down why: the notice line is not painted
            // while the pane covers it, so a sentence made now is one nobody can read and
            // that would then reappear, stale, when the pane closes -- and closing writes
            // its own notice over it anyway. The gesture is ended; there is nobody to
            // tell.
            if (!b.pressed && b.button == 1 && session_.drag.active) {
                end_drag(session_);
                return;
            }
            // AND THE BOOL BELOW IS NOT THE PRESS-CHAIN'S BOOL, which is why it is given a
            // name here (QR-2). The three handlers under `if (b.pressed)` answer whether they
            // CONSUMED the press, and the chain stops on a true. `terminal_press` answers
            // whether anything CHANGED, and the answer does not decide anything about routing:
            // the mode consumed the press the moment `session_.terminal.open` was true, three
            // lines up, and a press that lands on none of the pane's regions is consumed there
            // just the same. Two different questions, two bools, and unifying them would put a
            // repaint decision on the routing path or a routing decision on the repaint path.
            if (b.pressed && b.button == 1) {
                const bool repaint_needed = terminal_press(b);
                if (repaint_needed) {
                    repaint(mail);
                }
            }
            return;
        }
        const PointedAt at = canvas_point_of(b.space, b.x, b.y);
        if (b.button != 1 || !at.understood) {
            return;
        }
        if (b.pressed) {
            // TRUE MEANS CONSUMED: STOP ROUTING. FALSE MEANS NOT CONSUMED: CARRY ON (QR-2).
            // That is the whole meaning of the three bools below, and it is the only meaning
            // any of them has -- not "something changed", not "the act succeeded", not "the
            // press was accepted". A layer that consumes may refuse in its own words, may say
            // nothing at all, and may leave every fact in this application exactly as it found
            // it; what it may not do is let a press it owns be answered by the layer around
            // it. A consumed press does not have to change anything -- it only has to have
            // reached the layer that owns what the press means.
            //
            // AND THE BODY IS RESOLVED ONCE, HERE, beside the canvas point above it (QR-2).
            // The three handlers under it are three questions about ONE place, and they used
            // to resolve it separately -- the same six lines three times, and up to three
            // resolutions of one body for one press. Holding it across the chain is safe for a
            // reason worth writing down rather than assuming: every one of the three changes
            // nothing on the paths where it declines, so a handler that says "not mine" has
            // not moved the picture the next handler is about to ask about.
            const InfoBodyAt where = info_body_at(state_, session_, b.space, b.x, b.y);
            // THE ACTIVE PROPERTY EDITOR IS ASKED FIRST, and it is a PLACE inside a panel
            // rather than a mode (HD-5). The order is the same one the pane and the
            // completion list already have: the innermost thing that owns the pointer where
            // it landed answers before the thing around it, and a press it declines falls
            // through to the panel's own answer unchanged.
            //
            // IT SAYS NOTHING, AND IT CONSUMES WHETHER OR NOT THE CARET MOVED. Every other
            // press on this panel writes the notice line, because a press that changed nothing
            // and said nothing would leave the previous gesture's sentence sitting beside a
            // maker who has just done something else. A press on the draft's own row is the
            // one press where that argument runs the other way: the caret IS the statement, it
            // is on screen, and a sentence repeating it would push the refusal a maker may
            // still need to read off the line for a gesture they can already see the result
            // of. That argument never depended on the caret MOVING -- a maker who presses
            // where the caret already is has aimed at the draft and hit it, and the caret is
            // still the answer (QR-2).
            if (info_press(where)) {
                repaint(mail);
                return;
            }
            // THEN THE ACTION CONTROLS (HD-8). Same order, same reason, and the three runs of
            // the body cannot fight over a press: the footer, the object list and a live
            // draft's own row are disjoint runs of ONE row budget, which is the property HD-7
            // bought by making the body one region rather than two. So this ordering is
            // written down for the same reason HD-5's four modes are -- an ordering that rests
            // on a disjointness proof is one refactor from being silently wrong -- and not
            // because two of these could otherwise both answer.
            if (actions_press(where)) {
                repaint(mail);
                return;
            }
            // AND THEN THE OBJECT LIST, for the same reason and in the same order (HD-7): the
            // innermost thing that owns the pointer where it landed answers before the thing
            // around it, and a press it declines falls through to the panel's own answer
            // unchanged. It is asked SECOND because a live draft's own row is the narrower
            // claim -- and `objects_press` refuses outright while any draft is live, so the
            // two can never both want the same press.
            if (objects_press(where)) {
                repaint(mail);
                return;
            }
            // A VISIBLE PANEL OCCUPIES POINTER SPACE (PNL-2), and this is the
            // whole of it: the press is asked what it landed on before the
            // document is asked anything, and a press that landed on a panel
            // never reaches `take_hold` -- so it cannot select, cannot begin a
            // move and cannot begin a resize, because all three are that one
            // call. The question names no kind and knows no coordinate; it is
            // the same `bounds_of` the painter used for the same panel.
            //
            // IT SAYS SO RATHER THAN GOING QUIET. Every other press writes the
            // notice line, so a press that changed nothing and said nothing
            // would leave the previous gesture's sentence sitting beside a
            // maker who has just done something else -- a stale statement,
            // which is the one thing this tool is arranged against. It is also
            // the only way a maker learns that the panel is a thing rather than
            // a picture, since `[ Build ]` is not clickable yet.
            const Occupancy here =
                occupied_at(session_.panels, screen_of(session_), at.cell.x, at.cell.y);
            if (here.occupied) {
                say(std::string(here.what) + " is here -- nothing under it can be taken hold of",
                    false);
            } else {
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
            }
        } else if (session_.drag.active) {
            // A RELEASE IS NOT ASKED THE SAME QUESTION, and the asymmetry is the
            // reason no capture state exists here. A gesture that began on the
            // workspace owns the pointer until it ends, so its release must end
            // it wherever the maker's hand happens to be -- occluding the
            // release would strand `drag.active` true with the button up, and
            // the next motion would drag an object nobody was holding. The
            // other direction needs nothing at all: a press on a panel starts no
            // drag, so a release after one finds none and does nothing at all.
            // The absence of a drag IS the memory.
            const std::int64_t id = session_.drag.id;
            end_drag(session_);
            say("released #" + std::to_string(id), false);
        }
        repaint(mail);
    }

    /// The pointer moved. Outside a drag this weave has nothing to do with it:
    /// the job of remembering where the pointer is went away with the
    /// reconstruction it existed to serve.
    ///
    /// A PANEL DOES NOT OCCLUDE MOTION, and that is a decision rather than an
    /// omission (PNL-2). Only a press can begin a gesture, so a motion that
    /// matters here belongs to a drag that began on the workspace -- and
    /// stopping that drag at a panel's edge would CLAMP the document: an object
    /// could not be dragged to a cell a maker is entitled to put it at merely
    /// because something is currently drawn over that cell. That is a panel's
    /// presence becoming visible in the picture of the document, which is the
    /// same rule that keeps the vacated Info column empty (screen.hpp). The
    /// object goes where the hand puts it, and the panel goes on covering it.
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

    /// THE BUILDER TOOL SAID WHAT IT IS.
    ///
    /// A publication from a weave this application does not own, presented by a
    /// panel this application does. Everything a Builder panel shows arrives
    /// here and nowhere else -- Workshop computes no build fact, holds no
    /// target of its own, and cannot ask for a build it was not first told
    /// about (see `build_now`).
    ///
    /// IT IS KEPT ONLY WHILE A PANEL IS PRESENTING IT. With no Builder panel
    /// open there is nothing to put it on, and keeping a copy against the
    /// possibility of one being opened later is precisely how a presentation
    /// quietly becomes a second owner of somebody else's facts. The tool goes on
    /// saying what it is to whoever else is listening; this application stops
    /// listening in the only sense it can -- it stops remembering.
    void on(const zengine::builder::BuildStatus& said, loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return;
        }
        BuilderPane& pane = session_.panels.builder;
        // WAS THIS PANEL WATCHING? The first live run got this wrong and the
        // screen said so: reopening the panel asks the tool, the tool answers
        // with the outcome of a build that finished a minute ago, and the notice
        // line announced `built zengine-snake -- exit 0` as though it had just
        // happened. LEARNING a fact and WITNESSING an event are different, and
        // only the second is news. So the announcement below is made only for a
        // build this panel asked for and has not yet been answered about.
        //
        // ASYNC-1 MADE THAT DISTINCTION WORTH MORE, NOT LESS. A build now has a
        // middle, so a panel opened while one is running is TOLD "running" and
        // must announce nothing -- it did not watch this build begin, and the
        // arrival of a status is not the arrival of an event. `awaiting` is
        // therefore held across every intermediate condition and released only
        // when the build reaches one it will not leave: `still_going` is the one
        // place that list is written down.
        const bool watching = pane.awaiting;
        pane.heard = true;
        pane.shown = said;
        if (!zengine::builder::still_going(said.outcome)) {
            pane.awaiting = false;
        }
        if (!watching || zengine::builder::still_going(said.outcome)) {
            repaint(mail);
            return;
        }
        switch (said.outcome) {
        case zengine::builder::outcome::kSucceeded:
            say("built " + said.target + " -- exit 0", false);
            break;
        case zengine::builder::outcome::kFailed:
            say("BUILD FAILED: " + said.target + " -- exit " + std::to_string(said.status), true);
            break;
        case zengine::builder::outcome::kNotStarted:
            say("the build never started: " + said.detail, true);
            break;
        case zengine::builder::outcome::kUnknownTarget:
            say("the Builder refused: " + said.detail, true);
            break;
        default: break;
        }
        repaint(mail);
    }

    /// The session, for a suite that wants to check where a gesture left things.
    /// Read-only: every change still goes through a message and a gesture.
    const Session& session() const { return session_; }
    const WorkshopDoc& document() const { return state_; }

private:
    /// IS THIS THE CHARACTER THAT KEY PRODUCED?
    ///
    /// Byte equality, and one deliberate widening: a single ASCII LETTER matches
    /// in either case. A trigger says which KEY changed; what character the
    /// platform's layout made of it is a second fact, and Shift or a caps lock
    /// makes it the capital. `s` opening the name editor and then typing an `S`
    /// into it is the same defect as it typing an `s`, so both are owed.
    ///
    /// Nothing broader: this is not a case-folding rule for text, it is a
    /// question about ONE keystroke that has already happened.
    static bool same_keystroke(const std::string& text, const std::string& owed) {
        if (text == owed) {
            return true;
        }
        if (text.size() != 1 || owed.size() != 1) {
            return false;
        }
        const auto lower = [](char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        };
        const char a = lower(text[0]);
        return a >= 'a' && a <= 'z' && a == lower(owed[0]);
    }

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
        // THE EDITING KEYS THE SECOND CONSUMER EARNED (HD-5). Until this phase a property
        // draft could only be appended to and backspaced from, so a typo six characters back
        // cost six deletions and six retypes -- reproduced before the change, on a draft of
        // `hellp world`, where Left, Right, Home, End and Delete were every one of them
        // `default: break`.
        //
        // ALL FIVE WERE UNBOUND IN THIS MODE, source-traced exactly as HD-2's three and
        // HD-3's three were: `editing_key`'s switch had Return, Escape and Backspace, and
        // everything else fell through. Up/Down step the INSPECTOR's rows and Tab selects the
        // next OBJECT -- both in COMMAND mode, which is a different mode and is not reachable
        // while a draft is live. So no gesture anywhere changed meaning.
        //
        // They are the same five the Terminal binds, spelled the same way, because they now
        // reach the same implementation: `Row` forwards to the `component::TextBox` it owns,
        // and `TerminalPane` calls the same six methods on the one it owns.
        case input::scan::kLeft: row->left(); break;
        case input::scan::kRight: row->right(); break;
        case input::scan::kHome: row->home(); break;
        case input::scan::kEnd: row->end(); break;
        case input::scan::kDelete: row->erase_forward(); break;
        default: break;
        }
    }

    /// THE ONE PLACE THE PROPERTY DRAFT'S HORIZONTAL WINDOW IS RECONCILED (HD-5).
    ///
    /// `refresh_terminal`'s argument, one editor over, and it is called from the same place
    /// for the same reason: once per repaint, BEFORE anything is painted and before the next
    /// press is mapped, so the window a press is answered with is the window the maker is
    /// looking at. Four things must move it -- a keystroke, a press, opening a draft on a
    /// value longer than the row, and a RESIZE that changed nothing but the room -- and only
    /// the first three are edits, so a hook on the edits alone would have missed the fourth.
    ///
    /// ONLY THE EDITING ROW HAS A WINDOW TO RECONCILE, and at most one row is ever editing:
    /// `begin_edit` is reachable only from command mode, which is precisely the state in
    /// which no row is being edited. So this is one pass over eight rows doing nothing, plus
    /// four integer comparisons on the one that matters. Nothing is pooled and no row is
    /// given presentation state it is not using.
    ///
    /// A CLOSED PANEL IS NOT A ZERO CAPACITY. `bounds_of` answers with an empty rectangle for
    /// a panel nobody has open and `info_body_place` refuses it, so the reconcile is
    /// skipped rather than run against no room -- the draft is untouched, and the next
    /// repaint after the panel comes back resolves the window against the room it then has.
    ///
    /// SINCE HD-6 THE CAPACITY IS THE BODY'S, NOT THE ROW'S, and it is the same number for
    /// every row: the body is one region and `value_columns` is what any of its rows has left
    /// after the mark, the name and the caret's own column. A resize therefore reconciles the
    /// horizontal window of a live draft and the vertical window of the body from ONE resolved
    /// place, which is the whole of what "one resize reconciles all of it" costs here.
    void refresh_inspector() {
        const Screen sc = screen_of(session_);
        const PanelBounds info = bounds_of(session_.panels, panel::kInfo, sc);
        if (!info.open) {
            return;
        }
        for (std::size_t i = 0; i < session_.rows.size(); ++i) {
            if (!session_.rows[i].editing()) {
                continue;
            }
            const InfoBodyPlace body = info_body_place(info.rect, sc, state_, session_);
            if (body.present) {
                session_.rows[i].keep_caret_visible(body.value_columns);
            }
            return;
        }
    }

    /// A PRESS INSIDE THE ACTIVE PROPERTY EDITOR, and nothing else (HD-5).
    ///
    /// The pipeline, in the order §10 asks for it and with no step reconstructed from a
    /// coarser one:
    ///
    ///     the raw pointer fact (its own space, its own numbers)
    ///         -> the resolved Info panel body       info_body_place
    ///         -> a row and a column of ITS prose    prose_at
    ///         -> a semantic property row            property_at_prose_row
    ///         -> a column of that row's VALUE       property_value_column
    ///         -> a byte of the WHOLE draft          TextBox::position_at_column
    ///         -> the caret                          Row::place
    ///
    /// THE VERTICAL HALF IS HD-6'S, and it is the half a bounded body made necessary: a prose
    /// row is no longer the property's own index, because the body may be showing rows 4..7 of
    /// eight with a `... 4 earlier` marker spending the first of them. `property_at_prose_row`
    /// is the inverse of the function the painter positioned the caret with, so there is no
    /// second copy of the window arithmetic to go one row out once the body has scrolled.
    ///
    /// AND IT IS STILL NOT ROUNDED TO A WORKSHOP CELL. A graphical body row is 18 device
    /// pixels tall against a 12-pixel cell, so a press resolved through cells would name the
    /// wrong property for two thirds of the body. `prose_at` divides by the resolution the
    /// rows were DRAWN with -- `fit.line_px` -- which is the same fit the painter spent.
    ///
    /// THE RAW PIXEL IS USED AS A RAW PIXEL. `prose_at` branches on the `space` the backend
    /// stamped, exactly as the Terminal's press does: a window's pixel is divided by the
    /// resolution the row was drawn with, and a cell medium's position is already a
    /// character and takes the other route entirely.
    ///
    /// IT IS A PLACE, NOT A MODE, AND IT BEGINS NOTHING. A press that lands on an editable
    /// value which is NOT being edited is not a request to start editing it: that would have
    /// to decide what happens to a draft already live somewhere else, which is a semantic
    /// this phase has no measurement for and did not invent. `Return` opens a draft, and this
    /// only moves the insertion point inside the one that is open. A press anywhere else on
    /// the panel is answered by the panel exactly as it was.
    ///
    /// **TRUE MEANS CONSUMED — STOP ROUTING. FALSE MEANS NOT CONSUMED — CARRY ON (QR-2).**
    /// It used to answer whether the CARET MOVED, and the two agree for exactly as long as
    /// every press that lands on the draft also moves it -- which is to say until a maker
    /// presses where the caret already is. Measured on the pristine tree: that press fell
    /// through this handler, through the controls, through the object list, and was answered
    /// by the panel with `Info is here -- nothing under it can be taken hold of`, over a
    /// notice the maker was still reading. A press that reached the layer that owns what it
    /// means is that layer's, whether or not anything moved.
    bool info_press(const InfoBodyAt& where) {
        if (!where.present) {
            return false;
        }
        for (std::size_t i = 0; i < session_.rows.size(); ++i) {
            Row& row = session_.rows[i];
            if (!row.editing()) {
                continue;
            }
            if (!property_row_hit(where.body, i, where.at.column, where.at.row)) {
                return false; // on the panel, but not on the draft's row: not consumed
            }
            // THROUGH THE WINDOW THE ROW WAS DRAWN WITH. A visible column names
            // `first_visible + offset` of the WHOLE draft, never the offset alone -- the one
            // subtraction a horizontal window adds to a hit test, and the one that is right
            // to leave out for exactly as long as no value is long enough to scroll. The
            // component holds the offset the last repaint resolved, which is the one the
            // maker is looking at.
            //
            // AND THE ROW'S OWN PROSE OFFSET COMES OFF FIRST (HD-6). A body row carries the
            // mark and the property's name before the value, exactly as the pane's row
            // carries `> ` before the command, so a pressed column is a column of the ROW and
            // the value's column is that minus what the name spent. `property_value_column`
            // is the one subtraction and it is the inverse of the one
            // `property_caret_column` added.
            row.place(row.editor().position_at_column(property_value_column(where.at.column)));
            return true; // consumed: the press was on the draft's own row
        }
        return false; // no draft is live, so this panel has no editor to press
    }

    /// A PRESS ON AN ACTION CONTROL PERFORMS THE ACT THE CONTROL NAMES (HD-8).
    ///
    /// THE GEOMETRY IS THE PAINTER'S, one run down from the two lists and with the same
    /// pipeline, no step reconstructed from a coarser one:
    ///
    ///     the raw pointer fact (its own space, its own numbers)
    ///         -> the resolved Info panel body       info_body_place
    ///         -> a row and a column of ITS prose    prose_at
    ///         -> a control of the footer            action_press_at
    ///         -> availability                       action_availability
    ///         -> the SAME operation `n` and `d` call
    ///
    /// `action_press_at` inverts `prose_row_of_action`, the function the painter placed the
    /// control with, so what a maker aims at is what answers. It is never rounded to a
    /// Workshop cell, for the reason the other two presses are not: a graphical body row is
    /// eighteen device pixels against a twelve-pixel cell.
    ///
    /// **IT INVENTS NO SECOND PATH INTO THE DOCUMENT.** `create_object()` and
    /// `delete_object()` are the operations `command()` binds `n` and `d` to, called here
    /// unchanged -- so the pointer and the keyboard cannot come to create differently, delete
    /// differently, select differently afterwards, or describe what they did in different
    /// words. There is no copy of the create algorithm here and there is no registry, no
    /// command id, no callback and no action bus: this function is a switch over two indices
    /// of a table, which is what two controls actually cost.
    ///
    /// THE TWO REFUSALS ARE HANDLED DIFFERENTLY AND THAT IS THE POINT (`Availability`'s own
    /// comment carries the argument). A live draft is refused HERE, because the operations
    /// know nothing about one and would rebuild the inspector's rows out from under it. No
    /// target is passed THROUGH, because `doc::remove` already refuses it, changes nothing,
    /// and says so in the document's own words -- and a second sentence for one state is the
    /// thing this file spends `move_notice` and `kNoDocumentFile` avoiding.
    ///
    /// AND A PRESS ON A CONTROL IS CONSUMED WHATEVER IT DECIDES — **true means consumed, stop
    /// routing; false means not consumed, carry on** (QR-2, naming what this already did).
    /// Returning false for an unavailable control would drop the press through to the object
    /// list and then to the panel's occupancy answer, which would put a sentence about the
    /// panel on the notice line in place of the reason the maker actually needs. One gesture,
    /// one owner. This is the handler that was already answering the routing question the
    /// chain asks, which is why nothing about it changes here beyond where its place comes
    /// from.
    bool actions_press(const InfoBodyAt& where) {
        if (!where.present) {
            return false;
        }
        const std::size_t which = action_press_at(where.body, where.at.column, where.at.row);
        if (which == kNoAction) {
            return false; // a list row, the heading, a spare row, or off the body entirely
        }
        if (action_availability(which, state_, session_) == Availability::kDraftLive) {
            say(kFinishDraftFirst, true);
            return true; // consumed, and refused in this application's own words
        }
        if (which == kActionCreate) {
            create_object();
        } else {
            delete_object();
        }
        return true; // consumed, whatever the document then made of it
    }

    /// A PRESS ON A VISIBLE OBJECT NAME SELECTS THAT OBJECT — in command mode, and only there
    /// (HD-7).
    ///
    /// THE GEOMETRY IS THE PAINTER'S, with no step reconstructed from a coarser one and no
    /// second copy of the window arithmetic:
    ///
    ///     the raw pointer fact (its own space, its own numbers)
    ///         -> the resolved Info panel body       info_body_place
    ///         -> a row and a column of ITS prose    prose_at
    ///         -> a visible object                   object_press_at
    ///         -> the document's own identity        select
    ///
    /// `object_press_at` inverts `prose_row_of_object`, the same function the painter placed
    /// the name with, so the row a maker sees IS the row the press names -- including when the
    /// list has scrolled and an `... N earlier` marker is spending the body's first row, which
    /// is the case a second copy of the arithmetic would get wrong. It is never rounded to a
    /// Workshop cell: a graphical name row is eighteen device pixels tall against a
    /// twelve-pixel cell, so a cell-rounded press names the wrong object for most of the list.
    ///
    /// THE MODE LAW, STATED ONCE AND PINNED: **while a property draft is live, a press on the
    /// object list changes no selection and says so.** Changing objects rebuilds the inspector
    /// rows, which is exactly what a live draft cannot survive -- and the three answers a
    /// press could give instead (commit it, cancel it, carry it to a different object's `Name`)
    /// are three different sentences about a maker's unfinished work that nothing has measured
    /// a preference between. HD-6 refused the mirror of this question (a press does not BEGIN
    /// an edit) for the same reason. `Esc` cancels, `Return` commits, and then the list is
    /// live again; the notice says which.
    ///
    /// AND IT SELECTS, WHICH IS ALL IT DOES. It does not open the Builder, begin a drag, take
    /// hold, rename, or start editing a property -- `select` is the same call `Tab` makes, so
    /// a pointer and a key reach the document through one door.
    ///
    /// **TRUE MEANS CONSUMED — STOP ROUTING. FALSE MEANS NOT CONSUMED — CARRY ON (QR-2).**
    /// AND ONE OF THE FALSES BELOW IS DELIBERATE RATHER THAN GEOMETRIC: a press on the row of
    /// the object that is ALREADY selected lands squarely on this list and is still not
    /// consumed, because there is nothing here for it to do and the sentence a maker should
    /// get is the panel's -- the same one every other press on this rectangle gets. That is
    /// the shape `info_press` had by accident and this one has on purpose, and the difference
    /// is exactly why one bit could not be repaired by copying the other site: one of them was
    /// answering the wrong question and the other was answering this one with a considered
    /// `no`. Naming the bit does not merge them; it makes the deliberate `no` legible as a
    /// choice instead of leaving it indistinguishable from a defect.
    bool objects_press(const InfoBodyAt& where) {
        if (!where.present) {
            return false;
        }
        const std::size_t which = object_press_at(where.body, where.at.column, where.at.row);
        if (which == kNoObject || which >= state_.elements.size()) {
            return false; // a marker, the heading, a property row, a spare row, or off the body
        }
        if (draft_live(session_)) {
            say(kFinishDraftFirst, true);
            return true; // consumed: nothing moved, and the reason is on the notice line
        }
        const std::int64_t id = state_.elements[which].id;
        if (id == session_.selected) {
            // NOT CONSUMED, DELIBERATELY. The press is on this list and this list has nothing
            // to do with it; letting it through is how a maker gets the panel's answer rather
            // than silence.
            return false;
        }
        select(id);
        say("selected #" + std::to_string(id), false);
        return true; // consumed
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

    /// Editing mode for the command line: the keys that are controls rather than
    /// text, exactly as the inspector's editor has.
    ///
    /// Escape CLEARS THE LINE AND DOES NOT CLOSE THE PANE. One gesture opens and
    /// closes this thing, and giving Escape a second way out would mean a maker
    /// who wanted to abandon a half-typed command sometimes lost the pane too.
    ///
    /// SINCE HD-2 IT HAS ONE MORE JOB BEFORE THAT ONE: dismissing the completion
    /// list. The ordering is what a maker means by pressing it -- the list is the
    /// most recent thing that appeared, so the first Escape puts it away and the
    /// second abandons the line. Neither ever closes the pane, which is still the
    /// property that makes this key safe to press.
    ///
    /// THE THREE NEW KEYS WERE ALL UNBOUND IN THIS MODE, source-traced before they
    /// were taken: `terminal_key`'s switch had exactly Return, Backspace and
    /// Escape, and everything else fell through `default: break`. Up/Down step the
    /// INSPECTOR's rows and Tab selects the next OBJECT -- both in command mode,
    /// which is a different mode and is not reachable while the pane is open. So
    /// no gesture changed meaning anywhere.
    void terminal_key(const zengine::input::KeyPressed& k) {
        TerminalPane& pane = session_.terminal;
        switch (k.scancode) {
        case input::scan::kReturn: submit_terminal_line(); break;
        case input::scan::kBackspace: pane.input.backspace(); break;
        // THE CARET KEYS (HD-3). All three were `default: break` before this phase --
        // source-traced, exactly as HD-2 traced its three. Left/Right are bound in COMMAND
        // mode to nothing at all (that mode's directional gestures are `hjkl` and the
        // up/down arrows), and this mode is not reachable from it, so no gesture anywhere
        // changed meaning.
        //
        // THEY DO NOT `return` THE WAY Up/Down DO, and the difference is the phase. Up/Down
        // move a selection and skip the rebuild because the line did not change; a caret
        // move does not change the line either, but it changes whether the caret is AT THE
        // END -- which is the question the completer is allowed to be asked (see
        // `refresh_terminal`). Falling through is what makes the list appear and disappear
        // as the caret leaves and returns to the end.
        case input::scan::kLeft: pane.input.left(); break;
        case input::scan::kRight: pane.input.right(); break;
        case input::scan::kDelete: pane.input.erase_forward(); break;
        case input::scan::kHome: pane.input.home(); break;
        case input::scan::kEnd: pane.input.end(); break;
        case input::scan::kEscape:
            if (completion_selectable()) {
                // THE LIST GOES AWAY AND THE LINE IS UNTOUCHED. A maker who wanted
                // the line gone presses it again; a maker who wanted only the list
                // gone has not lost the word they were half-way through.
                pane.dismissed = true;
                pane.dismissed_at = pane.completion.slot;
                pane.asked = false;
            } else {
                // ABANDONING THE LINE ABANDONS THE DISMISSAL WITH IT. The dismissal was
                // made against a word; there is no longer a word, so keeping it would
                // leave the list hidden for the whole of the next command with nothing
                // on screen to explain why. (Measured: after Escape-Escape the next
                // three characters produced no list at all.)
                pane.input.clear(); // ...and the caret with it: `clear` moves both
                pane.dismissed = false;
            }
            break;
        case input::scan::kUp: move_completion(-1); return;   // the line did not change
        case input::scan::kDown: move_completion(+1); return; // ...so nothing is recomputed
        case input::scan::kTab:
            // ONE KEY, ONE MEANING: "help me here". With a list on screen that is
            // taking the selected candidate; with nothing on screen it is asking
            // for one, which is the only gesture discovery needs because every
            // other entry point is ordinary typing.
            if (completion_selectable()) {
                accept_completion();
            } else {
                pane.asked = true;
                pane.dismissed = false;
            }
            break;
        default: break;
        }
        refresh_terminal();
    }

    /// A PRESS INSIDE THE TERMINAL MODE — the first place-within-a-mode (HD-3).
    ///
    /// Answers whether anything CHANGED, so the caller repaints for a press that did
    /// something and stays quiet for one that landed on the pane's furniture. It never
    /// answers "not mine": the mode consumes every press either way, which is what makes
    /// click-through impossible without a z-order service to prevent it.
    ///
    /// **SO THIS BOOL IS NOT THE PRESS CHAIN'S BOOL, AND IT MUST NOT BE UNIFIED WITH IT**
    /// (QR-2). `info_press`, `actions_press` and `objects_press` answer *did I consume this
    /// press* and the chain stops on a true; this answers *is a repaint owed*, and consumption
    /// was already decided one layer up by the MODE. A `false` here means "consumed by the
    /// terminal, and nothing moved" -- the opposite of what a `false` means in the chain. Two
    /// questions that happen to have two answers each are not one question, and the caller
    /// names the result `repaint_needed` so the difference is visible at the only place both
    /// kinds of bool are in view.
    ///
    /// THE ORDER IS THE PAINTER'S ORDER, BACKWARDS, and that is the whole of the arbitration:
    /// `paint_terminal` pushes the pane and then the completion list, and painter's order
    /// across `texts` is list order, so the list is on top -- therefore the list is asked
    /// first. Two regions, one rule, and no z-order object to hold it.
    ///
    /// IT CONSUMES THE PLACEMENT THE PAINTER RESOLVED, never a second interpretation of it.
    /// `completion_place` and `terminal_input_place` are each called with the same `Screen`
    /// the painter uses, which is what makes "click the row you can see" true rather than
    /// approximately true after the list has scrolled.
    bool terminal_press(const zengine::input::PointerButton& b) {
        TerminalPane& pane = session_.terminal;
        const Screen sc = screen_of(session_);

        // THE COMPLETION LIST, IF IT IS ON SCREEN. Its own condition is `paint_terminal`'s,
        // read from the same two flags, so a list a maker cannot see cannot be clicked.
        if (pane.completion.open && !pane.dismissed) {
            const CompletionPlace place =
                completion_place(sc, pane.completion.candidates.size() + 1 /*the heading*/);
            if (place.visible) {
                const surface::RegionFit fit =
                    surface::fit_region(place.x, place.y, place.w, place.h, sc.text_advance_px,
                                        sc.text_line_px);
                const ProseAt at = prose_at(b.space, b.x, b.y, place.x, place.y, fit);
                if (at.understood && at.column >= 0 && at.column <= fit.columns &&
                    at.row >= 0 && at.row < static_cast<std::int64_t>(place.rows)) {
                    // ROW 0 IS THE HEADING and is not a candidate. A press on it is a press
                    // on the list -- consumed, changing nothing -- rather than a press that
                    // falls through to the input line underneath, which is not underneath
                    // it at all.
                    if (at.row >= 1) {
                        // THE SAME WINDOW THE ROWS WERE DRAWN WITH. `completion_first_shown`
                        // is the one answer to "which candidate is the first visible row",
                        // and it is read here rather than recomputed.
                        const std::size_t first =
                            completion_first_shown(pane.completion.selected, place.rows);
                        const std::size_t at_index =
                            first + static_cast<std::size_t>(at.row - 1);
                        if (at_index < pane.completion.candidates.size()) {
                            // ONE SELECTION, WHICHEVER HAND MOVED IT. There is no
                            // pointer-selected state beside the keyboard's: this writes the
                            // field Up/Down write, so the row a click chooses is a row Tab
                            // then accepts and the renderer cannot tell which happened.
                            pane.completion.selected = at_index;
                            return true;
                        }
                    }
                    return false; // on the list, on nothing choosable
                }
            }
        }

        // THE EDITABLE LINE. One region -- the pane's own -- and one row of it.
        const TerminalInputPlace place = terminal_input_place(sc);
        const ProseAt at = prose_at(b.space, b.x, b.y, place.region_x, place.region_y,
                                    place.fit);
        if (at.understood && terminal_input_hit(place, at.column, at.row)) {
            const std::size_t was = pane.input.caret();
            // THROUGH THE WINDOW THE ROW WAS DRAWN WITH (HD-4). A visible column names
            // `first_visible + offset` of the WHOLE authored line, never the offset alone --
            // that is the one subtraction a horizontal viewport adds to a hit test, and
            // leaving it out is right for exactly as long as no line is long enough to
            // scroll. The offset read here is the one the last repaint resolved, which is
            // the one the maker is looking at.
            pane.input.place(terminal_caret_of_column(place, pane.input, at.column));
            // The caret moving is what changes whether completion may be asked, so a press
            // that moved it has to reach `refresh_terminal` exactly as a caret key does.
            if (pane.input.caret() != was) {
                refresh_terminal();
                return true;
            }
            return false;
        }
        return false; // inside the mode, on none of its regions: consumed, and nothing moved
    }

    /// IS THERE A LIST ON SCREEN WITH SOMETHING IN IT TO CHOOSE?
    ///
    /// The three completion keys all ask this one question, and the "something to
    /// choose" half is what keeps Escape's old meaning intact. A HEADING-ONLY list
    /// -- "no shape here begins with that" -- is a real and useful answer, and it is
    /// also transient: it goes away the moment the prefix changes, so a maker never
    /// needs a gesture to be rid of it and Escape can go on meaning "clear the line"
    /// there, exactly as it always did. A list with candidates in it is a chooser
    /// sitting over the record, which is a thing a maker may genuinely want gone
    /// while keeping what they have typed.
    ///
    /// Asked rather than assumed, because "there is something to say" and "a list is
    /// showing" are different: a dismissed list still has candidates and is not there.
    bool completion_selectable() const {
        const TerminalPane& pane = session_.terminal;
        return pane.open && pane.completion.open && !pane.dismissed &&
               !pane.completion.candidates.empty();
    }

    /// Move the selection, and stop at the ends.
    ///
    /// It does not wrap. A list that wrapped would answer Up on the first row by
    /// jumping to the last, which in a windowed list scrolls the whole thing out
    /// from under the maker's eye -- and the gesture that recovers from it is the
    /// one they just pressed.
    void move_completion(int by) {
        if (!completion_selectable()) {
            return;
        }
        Completion& comp = session_.terminal.completion;
        const std::size_t last = comp.candidates.size() - 1;
        if (by < 0) {
            comp.selected = comp.selected == 0 ? 0 : comp.selected - 1;
        } else {
            comp.selected = comp.selected >= last ? last : comp.selected + 1;
        }
    }

    /// TAKE THE SELECTED CANDIDATE INTO THE LINE.
    ///
    /// The edit is an ordinary end-of-line edit, which is what makes it compatible
    /// with the caret this pane actually has: the token being completed is always
    /// the LAST one, and the caret is always at the end, so accepting is "drop
    /// what has been typed of this token, append what it was going to be". No
    /// cursor position is needed, none is invented, and the trailing `_` still
    /// sits exactly where the next keystroke lands.
    ///
    /// THE SEPARATOR COMES FROM THE CANDIDATE, not from here. `insert` carries the
    /// trailing space where the grammar wants one and carries none after `field=`,
    /// where a value follows immediately -- so acceptance can neither duplicate a
    /// separator nor swallow one.
    ///
    /// HD-3 CHANGED WHERE THE CARET ENDS UP AND NOTHING ELSE, because there is now
    /// somewhere else it could be. It ends at the end of the inserted result, which is
    /// where a maker's next keystroke belongs -- and it is not an arbitrary choice: a
    /// candidate is offered only when the caret is at the end (see `refresh_terminal`), so
    /// "the end of the insert" and "the end of the line" are the same place, and leaving
    /// the caret anywhere else would put it inside bytes the accept had just replaced.
    void accept_completion() {
        if (!completion_selectable()) {
            return;
        }
        TerminalPane& pane = session_.terminal;
        const Completion& comp = pane.completion;
        const Candidate& c = comp.candidates[comp.selected];
        // `partial` is a TOKEN of this very line, so it can never be longer than the
        // line -- `tokenize` drops quote characters, which only ever makes a token
        // shorter than the text it came from, and a quoted token is refused by the
        // completer outright. The `min` is written anyway rather than as an `if`,
        // because the alternative to clamping is appending without stripping, which
        // is a doubled word on a line nobody could explain.
        const std::size_t typed =
            comp.partial.size() < pane.input.size() ? comp.partial.size() : pane.input.size();
        std::string line = pane.input.text();
        line.resize(line.size() - typed);
        line += c.insert;
        const std::size_t at = line.size();
        pane.input.set(std::move(line), at);
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
    /// SUBMISSION DOES NOT DEPEND ON WHERE THE CARET IS (HD-3). The parser receives the
    /// whole line exactly as authored -- Return is not "submit up to the caret", it is
    /// "submit this line", which is the reading that keeps a mis-typed middle repairable
    /// without the repair changing what gets sent.
    void submit_terminal_line() {
        const std::string line = session_.terminal.input.text();
        session_.terminal.input.clear();
        // A SUBMITTED LINE ENDS BOTH PIECES OF COMPLETION STATE. Escape said "not for
        // this word" and Tab said "show me anyway"; the next line is neither, and a
        // maker who pressed one of them once should not find its effect still in
        // force three commands later.
        session_.terminal.dismissed = false;
        session_.terminal.asked = false;
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
        // THE VERB TABLE IS THE COMPLETER'S TOO (HD-2, complete.hpp). It used to be
        // two string literals in the condition below, which was one answer while
        // one thing asked the question; a list a maker can be SHOWN is a second
        // asker, and two lists of two verbs is how the third verb gets learned by
        // only one of them.
        const TerminalVerb* verb =
            tok.empty() ? nullptr : terminal_verb(tok[0].text);
        loom::Address to;
        std::uint64_t version = 0;
        if (verb != nullptr && tok.size() >= 4 && loom::parse_address(tok[1].text, to) &&
            loom::parse_u64(tok[3].text, version) && version <= kMaxVersion) {
            std::vector<loom::Arg> args;
            for (std::size_t i = 4; i < tok.size(); ++i) {
                args.push_back(loom::lex_arg(tok[i]));
            }
            const loom::TerminalResult r =
                verb->ask ? me.ask(to, tok[2].text, static_cast<std::uint32_t>(version), args)
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
        const Screen sc = screen_of(session_);
        // THE ONE PLACE THE INPUT LINE'S HORIZONTAL WINDOW IS RECONCILED (HD-4).
        //
        // It is here because this function runs on EVERY repaint -- which is the property
        // HD-2 met as a trap and this needs as a guarantee. The window has to follow the
        // caret after a keystroke, after a press, after accepting a candidate AND after a
        // resize that changed nothing but the room; the first three are edits and the fourth
        // is not, so a hook on the edits would have missed exactly the witness §19 asks for.
        // Running it once per repaint answers all four with no special case, and it answers
        // them BEFORE `paint` and before the next press is mapped, so the picture and the
        // hit test are resolved against the same window.
        //
        // ABOVE THE `attached` RETURN ON PURPOSE: `paint_terminal` draws the pane whenever it
        // is OPEN, and `terminal_key` edits the line on the same condition, so a maker can
        // type into a pane with no participant mounted and must still be able to see where
        // they are. The capacity is `terminal_input_place`'s -- the same answer the painter
        // and the press consume, never a second count of the columns.
        pane.input.keep_caret_visible(terminal_input_place(sc).columns);
        pane.attached = host_->terminal != nullptr;
        pane.id = pane.attached ? host_->terminal->id() : loom::WeaveId{};
        pane.shown.clear();
        pane.earlier = 0;
        pane.dropped = 0;
        // TAKEN BEFORE THE CLEAR, because the clear is what this function does to
        // everything derived and the selection is the one thing that is not. See the
        // note below `complete_line` for what happens when these four are read after it.
        const LineSlot was_slot = pane.completion.slot;
        const std::string was_partial = pane.completion.partial;
        const bool was_open = pane.completion.open;
        const std::size_t was_selected = pane.completion.selected;
        pane.completion = Completion{};
        if (!pane.attached || !pane.open) {
            pane.dismissed = false; // a closed pane has no half-typed word to remember one for
            return;                 // nothing is painted from it, so nothing is copied
        }
        // WHAT COULD BE SAID NEXT, RECOMPUTED WITH THE LINE. It is derived from the
        // input and the participant's vocabulary and from nothing else, so it is
        // rebuilt rather than patched -- which is the same reason `shown` is a fresh
        // copy every time and not a list somebody maintains.
        //
        // AND IT AUTHORS NOTHING. `complete_line` takes the participant by const
        // reference; every method it reaches (`vocabulary()`, `describe()`,
        // `compose()`) is const, and the only path that authors goes through the
        // participant's own channel, which a const reference cannot touch. This is
        // the one call in this file that runs on every keystroke, and it is the one
        // that must never send.
        //
        // AND IT IS ASKED ABOUT THE END OF THE LINE, WHICH IS WHERE THE CARET HAS TO BE
        // (HD-3). HD-2's completer rests on an assumption that was free when the caret could
        // not move: the token being completed is the LAST one, so accepting is "drop what
        // has been typed of this token, append what it was going to be". With a caret in the
        // middle that edit would delete everything after it. The two honest repairs are to
        // teach the completer about a token under an arbitrary caret -- a second parser, on
        // a phase about carets and pointers -- or to say plainly that completion follows the
        // end of the line. HD-3 says it plainly.
        //
        // AND IT SAYS IT OUT LOUD RATHER THAN GOING QUIET, which is HD-2's own measured rule
        // arriving from a new direction: three different silences would otherwise render
        // identically, and a maker who moves the caret and watches the list vanish cannot
        // tell "not here" from "broken". So the list becomes a heading with no candidates in
        // it -- exactly the shape `send * s` already produces -- and a heading-only list is
        // transient and takes no gesture to dismiss.
        if (pane.input.at_end()) {
            pane.completion = complete_line(*host_->terminal, pane.input.text());
        } else {
            pane.completion.open = true;
            pane.completion.slot = read_command_line(pane.input.text()).slot;
            pane.completion.heading =
                "completion follows the END of the line -- this caret is inside it";
        }
        // THE SELECTION SURVIVES A REPAINT AND NOT A CHANGE OF QUESTION.
        //
        // This function runs on every repaint, not only when the line changes -- the pane
        // is a snapshot and a snapshot is only true when taken -- so a freshly computed
        // Completion arrives with `selected` at zero every time. Without this the arrow
        // keys appeared to do nothing at all: the move landed, the repaint immediately
        // after it undid the move, and the next Tab took the first candidate. (Found in
        // the live SDL run and reproduced headlessly, which is what said it was never the
        // driver -- and then reproduced a SECOND time, because the first repair read the
        // previous selection AFTER this function had already cleared it. A rebuild-from-
        // scratch function has exactly one place a survivor can be read, and it is the
        // top.)
        //
        // The question is the SLOT and the PARTIAL together. Same question, same
        // selection; a different word or a different part of the line is a new list and
        // starts at the top. Clamped either way, because a list can shrink under an
        // unchanged partial -- name a field and the field that was selected leaves.
        if (was_open && pane.completion.open && pane.completion.slot == was_slot &&
            pane.completion.partial == was_partial && !pane.completion.candidates.empty()) {
            const std::size_t last = pane.completion.candidates.size() - 1;
            pane.completion.selected = was_selected < last ? was_selected : last;
        }
        // A DISMISSAL BELONGS TO THE PART OF THE LINE IT WAS MADE IN. Moving on to
        // the next word is a new question, so the list comes back for it.
        if (pane.dismissed && pane.completion.slot != pane.dismissed_at) {
            pane.dismissed = false;
        }
        // AN UNTOUCHED LINE ASKS NOTHING. See `TerminalPane::asked`: the line is
        // empty immediately after a submit, and a list there covers the answer the
        // pane just gave. Typing is the gesture; Tab is the way to ask anyway.
        if (!pane.input.empty()) {
            pane.asked = false;
        } else if (!pane.asked) {
            pane.completion = Completion{};
        }
        // AN EMPTY LINE HAS ITS CARET AT THE END BY CONSTRUCTION, so the branch above and
        // the caret rule cannot disagree about it -- there is no position in an empty string
        // that is not both 0 and the end.
        // AS MANY ENTRIES AS THIS PANE CAN SHOW WHOLE, which is no longer the same as "as
        // many entries as it has rows": since G-2 a line too long for the pane WRAPS rather
        // than being cut, so one entry can cost several rows. `entries_that_fit` is the one
        // place that arithmetic lives, and `paint_terminal` carries out the same choice with
        // the same call -- two answers here would be a pane whose omission marker lied.
        //
        // A row apiece is the floor, so `tail(rows)` is always at least as many entries as
        // can possibly fit, and the fitting only ever takes fewer. `sc` is the one resolved
        // at the top of this function -- the same screen the input line's window was
        // reconciled against, because two screens in one refresh is two answers.
        const loom::Transcript& record = host_->terminal->transcript();
        std::vector<loom::TranscriptEntry> newest = record.tail(sc.terminal_rows);
        const std::size_t fits = entries_that_fit(newest, sc.terminal_cols, sc.terminal_rows);
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
    ///
    /// `p` opens the picker and `b` asks the open Builder for a build. Both were
    /// unbound before BLD-0, so no existing gesture changed meaning, and with no
    /// Builder panel open `b` does exactly what it did before, which is nothing.
    ///
    /// `x` IS UNBOUND AGAIN, and that is PNL-0 answering the question BLD-0 wrote
    /// down and declined: whose `x` is it? With a second panel kind the answer
    /// would have to be either a per-panel binding or a focus rule, and this file
    /// has refused both. So presence moved wholly to the picker -- one door,
    /// which opens what is closed and removes what is open -- and the key that
    /// used to close the Builder means nothing again.
    ///
    /// THE INSPECTOR'S KEYS NOW ANSWER FOR THEIR PANEL. `up`, `down` and Return
    /// drive rows that only the Info panel shows, so with Info removed they say
    /// so instead of quietly working on something invisible. That is not a focus
    /// rule: it is the same sentence `b` says by doing nothing, said out loud
    /// because unlike `b` these keys used to do something.
    void command(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        const bool shift = held(k.modifiers, input::mod::kShift);
        switch (k.scancode) {
        case input::scan::kTab: select_next(); break;
        case input::scan::kUp: move_cursor(-1); break;
        case input::scan::kDown: move_cursor(+1); break;
        case input::scan::kReturn: begin_edit(); break;
        case input::scan::kN: create_object(); break;
        case input::scan::kD: delete_object(); break;
        case input::scan::kH: shift ? size_by(-1, 0) : move_by(-1, 0); break;
        case input::scan::kJ: shift ? size_by(0, +1) : move_by(0, +1); break;
        case input::scan::kK: shift ? size_by(0, -1) : move_by(0, -1); break;
        case input::scan::kL: shift ? size_by(+1, 0) : move_by(+1, 0); break;
        case input::scan::kLeftBracket: resize_workspace(-4); break;
        case input::scan::kRightBracket: resize_workspace(+4); break;
        case input::scan::kP: open_picker(); break;
        case input::scan::kB: build_now(mail); break;
        // THE TWO SETUP GESTURES (WS-0). They are commands rather than another `^`-pair
        // beside the document's, and that is the visible half of the separation: `^s`/`^o`
        // are the DOCUMENT's two keys and mean the same thing in every mode, while naming and
        // restoring a setup are ordinary maker gestures that belong beside `p`. Both were
        // unbound before this phase, so nothing a maker knew changed meaning.
        case input::scan::kS: open_setup_name(); break;
        case input::scan::kR: restore_setup(mail); break;
        case input::scan::kQ: quit(); break;
        default: break;
        }
    }

    // ---- The dynamic panels --------------------------------------------------
    //
    // A WEAVE MAY PROVIDE A TOOL; A PANEL IS ITS PRESENTATION. Everything below
    // is presentation: it opens and closes rows of this application's furniture,
    // and the two places it touches the bus are ordinary sends to an office,
    // authored as Workshop, gated against Workshop's own grant. Workshop gained
    // exactly two new things it may SAY -- ask the Builder what it is, and ask
    // it to build the target it just named -- and nothing it may DO.
    //
    // AND PNL-0 ADDED A SECOND PANEL KIND WITHOUT ADDING A THIRD THING. `Info`
    // opens, presents and closes through exactly the machinery below, and the
    // only line in this whole section that knows a bus exists is still the one
    // `if (chosen.kind == panel::kBuilder)` in `choose_panel`. That is the
    // clearest evidence available that the panel seam is not a weave seam: the
    // second kind arrived and the grant did not move.

    /// Open the `+ panel` picker.
    void open_picker() {
        session_.panels.picker.open = true;
        session_.panels.picker.cursor = 0;
        say("+ panel -- up/down chooses, enter opens or removes, esc or p cancels", false);
    }

    /// The picker's keys. Escape and `p` both close it: the key that opened it
    /// closes it, the terminal overlay's rule, and Escape closes it too because
    /// a maker who has changed their mind should not have to remember which of
    /// the two ways out this particular thing has.
    void picker_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        PanelPicker& picker = session_.panels.picker;
        switch (k.scancode) {
        case input::scan::kUp:
            if (picker.cursor > 0) {
                --picker.cursor;
            }
            break;
        case input::scan::kDown:
            if (picker.cursor + 1 < kPanelKinds) {
                ++picker.cursor;
            }
            break;
        case input::scan::kReturn: choose_panel(mail); break;
        case input::scan::kEscape:
        case input::scan::kP:
            picker.open = false;
            say("no panel opened or removed", false);
            break;
        default: break;
        }
    }

    /// OPEN THE KIND THE CURSOR IS ON, OR REMOVE IT. The picker is the one owner
    /// of panel presence, and this is the whole of that ownership.
    ///
    ///     closed panel  ->  select  ->  open
    ///     open panel    ->  select  ->  remove
    ///
    /// BLD-0 REFUSED THE SECOND SELECTION with a sentence -- `Builder is already
    /// open` -- because with one kind, `x` could say "remove" unambiguously and
    /// the picker had nothing to add. The second kind took that away: `x` would
    /// have had to choose a panel, and choosing means either a per-panel binding
    /// or a focused panel, and both are frameworks this Workshop has declined to
    /// grow. Selecting an open kind was already the gesture a maker reached for
    /// and it was already spelled `p`, so the refusal became the removal and one
    /// key went back to being unbound. That is the smallest layer this could be
    /// resolved at: no new gesture, no new mode, no new state, one branch.
    ///
    /// IT IS STILL NOT A MULTI-INSTANCE POLICY. A kind is present or absent;
    /// there is no second Builder for this to have an opinion about.
    ///
    /// NO DRAFT CAN BE ORPHANED BY A REMOVAL, and the reason is a reachability
    /// one, so it is written where a reader would otherwise have to reconstruct
    /// it: the picker is reachable only from command mode, command mode is by
    /// definition the state in which no inspector row is being edited, and the
    /// key routing in `on(KeyPressed)` puts editing ahead of command. So a maker
    /// cannot be part-way through typing a value into a row and remove the panel
    /// showing it. Nothing here guards against that, because nothing can reach
    /// it -- and if the routing ever changes, this paragraph is the thing that
    /// stops being true, which is why it names the routing rather than the fact.
    /// THE PICKER NOW MOVES THE INTENT, AND THE INTENT MOVES THE PANELS (WS-0).
    /// One line changed shape and it is the phase's coherence claim: this
    /// gesture used to call `open_panel`/`close_panel` directly, which would
    /// have left the active setup describing an arrangement the screen had
    /// stopped showing the moment a maker pressed `p`. So the picker edits
    /// `setup.active` and `apply_setup` is the only thing that opens or closes
    /// anything -- the same path a restore goes through, so the two cannot
    /// diverge about what "Builder is open" costs.
    ///
    /// A REMOVAL AND AN OPEN ARE STILL THE SAME TWO CASES a maker sees; what
    /// changed is which value they are asked of.
    void choose_panel(loom::Mail& mail) {
        PanelPicker& picker = session_.panels.picker;
        const PanelKind& chosen = kPanelCatalog[picker.cursor];
        picker.open = false;
        const PaneRef ref = pane_ref_of(chosen.kind);
        if (remove_pane(session_.setup.active, ref)) {
            apply_setup(mail);
            // WHAT IT WAS PRESENTING IS UNTOUCHED, and one sentence covers both
            // kinds because it is the same sentence: the Builder tool keeps its
            // target, its history and its running count of asks; the document
            // keeps every object, the selection and the inspector's rows. A
            // panel is a presentation, and removing one removes a presentation.
            say(std::string("removed ") + chosen.name +
                    " -- p brings it back; nothing behind it was touched",
                false);
            return;
        }
        (void)add_pane(session_.setup.active, ref);
        // AND THE PANEL ASKS THE TOOL WHAT IT IS -- inside `apply_setup`, for
        // every kind it newly opened. A presentation that was handed its
        // subject's facts by whoever built it would be showing the builder's
        // opinion; this one shows the tool's answer, and shows nothing until it
        // has one.
        //
        // INFO ASKS NOBODY, and the absence of a second branch there is the
        // phase's structural claim, unchanged: opening it sends no message,
        // touches no role and needs no weave mounted anywhere. A Workshop
        // hosting no tools at all opens Info and it works.
        apply_setup(mail);
        say(std::string("opened ") + chosen.name + " -- p removes it", false);
    }

    // ---- The setup: name it, save it, restore it ------------------------------
    //
    // A SETUP IS AUTHORED CONFIGURATION AND THE DOCUMENT IS AUTHORED CONTENT, and
    // everything below keeps them apart by the strongest means available: a
    // different value, a different law, a different file, a different format
    // identity and a different pair of gestures. Nothing here reads, writes,
    // normalises or dirties `state_`, and `save_document`/`load_document` above
    // are untouched.

    /// MAKE THE OPEN PANELS BE WHAT THE ACTIVE SETUP SAYS -- the one owner, and
    /// the only thing in this file that opens or closes a panel.
    ///
    /// The presentation half is `reconcile` (setup.hpp), which is pure and takes
    /// no bus. What it hands back is what it CHANGED, and this is where that
    /// becomes speech: a kind that was closed and is now open performs whatever
    /// asking that kind does on open, which for the Builder is the same
    /// `StatusRequested` opening it through the picker has always sent, and for
    /// Info is nothing at all.
    void apply_setup(loom::Mail& mail) {
        const Reconciled done = reconcile(session_.panels, session_.setup.active);
        for (const std::int64_t kind : done.opened) {
            if (kind == panel::kBuilder) {
                (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                        zengine::builder::StatusRequested{});
            }
        }
    }

    /// OPEN THE ONE-LINE SETUP-NAME EDITOR, on the name the setup already has.
    ///
    /// IT OPENS ON THE CURRENT NAME RATHER THAN ON NOTHING, because the common
    /// gesture is "save this again" and retyping `Morning build` to do it would
    /// make the shortest path the least likely one. Enter commits and saves;
    /// Escape leaves the name exactly as it was.
    ///
    /// AND IT SWALLOWS ITS OWN `s`. The key transition and the character it
    /// produced are two facts that were simultaneously true and both arrive --
    /// the trap WG-0 measured and named -- so without this the gesture that
    /// opened the editor would also type an `s` into the name it opened.
    void open_setup_name() {
        if (host_->setup_path.empty()) {
            say(kNoSetupFile, true);
            return;
        }
        SetupNaming& naming = session_.setup.naming;
        naming.open = true;
        naming.line.set(session_.setup.active.name, session_.setup.active.name.size());
        swallow_text_ = "s";
        say("name this setup -- enter saves it, esc cancels", false);
    }

    /// The name editor's keys. Return commits and saves; Escape cancels and
    /// changes nothing; the rest is the ordinary editing of one line, through
    /// the component that owns the text, the caret and the window together.
    void naming_key(const zengine::input::KeyPressed& k, loom::Mail&) {
        SetupNaming& naming = session_.setup.naming;
        switch (k.scancode) {
        case input::scan::kReturn: commit_setup_name(); break;
        case input::scan::kEscape:
            naming.open = false;
            naming.line.clear();
            say("the setup name is unchanged", false);
            break;
        case input::scan::kBackspace: naming.line.backspace(); break;
        case input::scan::kDelete: naming.line.erase_forward(); break;
        case input::scan::kLeft: naming.line.left(); break;
        case input::scan::kRight: naming.line.right(); break;
        case input::scan::kHome: naming.line.home(); break;
        case input::scan::kEnd: naming.line.end(); break;
        default: break;
        }
    }

    /// TAKE THE TYPED NAME AND WRITE THE SETUP.
    ///
    /// The name meets `check_setup_name` -- the SAME function a file's name
    /// meets -- and a refusal leaves the editor open with the text still in it,
    /// so a maker fixes what they typed rather than retyping it. Nothing is
    /// written and the active setup's name does not move until the whole thing
    /// is legal AND the file has been replaced.
    void commit_setup_name() {
        SetupNaming& naming = session_.setup.naming;
        const std::string wanted = naming.line.text();
        const Written legal = check_setup_name(wanted);
        if (!legal.accepted) {
            say(legal.refusal + " -- enter tries again, esc cancels", true);
            return;
        }
        Setup candidate = session_.setup.active;
        candidate.name = wanted;
        const Written whole = check_setup(candidate);
        if (!whole.accepted) {
            say(whole.refusal, true);
            return;
        }
        const Written written = setup_persist::save_file(host_->setup_path, candidate);
        if (!written.accepted) {
            // THE LAST GOOD SETUP FILE IS INTACT and so is the live one: the
            // writer never opened the destination, and this function has not
            // assigned anything yet. The editor stays open over the name the
            // maker was trying to save.
            say(written.refusal, true);
            return;
        }
        session_.setup.active = candidate;
        // What is on disk is now what is in memory. A COPY, never a flag --
        // `SetupState::saved()` compares, so it cannot drift.
        session_.setup.on_file = candidate;
        naming.open = false;
        naming.line.clear();
        say("saved setup " + quoted_setup_name(candidate.name) + " to " + host_->setup_path +
                unresolved_note(candidate),
            false);
    }

    /// RESTORE THE SETUP IN THE SELECTED FILE.
    ///
    /// A TRANSACTION, and structurally so: `setup_persist::load_file` RETURNS a
    /// candidate rather than writing into anything, so there is no path here by
    /// which a panel closes before a bad field near the end of the file has been
    /// met. A refusal costs a maker the notice and nothing else -- the active
    /// setup, the open panels, the Builder panel's copied status and the document
    /// are all exactly as they were.
    ///
    /// AN UNRESOLVED REFERENCE IS NOT A FAILURE. It loads, it stays in the setup,
    /// it is counted, it is named, and it is saved again unchanged. A malformed
    /// candidate is a failure; a reference to a pane this build has never heard
    /// of is a setup that means more than this build can show.
    ///
    /// NO DRAFT AND NO HALF-FINISHED NAME CAN BE ORPHANED BY THIS, and the
    /// reason is reachability rather than a guard: `r` is a command, command
    /// mode is by definition the state in which no inspector row is being edited,
    /// and the key routing puts the name editor and the picker ahead of it. So a
    /// maker cannot be part-way through typing anything when this runs.
    void restore_setup(loom::Mail& mail) {
        if (host_->setup_path.empty()) {
            say(kNoSetupFile, true);
            return;
        }
        const setup_persist::LoadedSetup loaded = setup_persist::load_file(host_->setup_path);
        if (!loaded.outcome.accepted) {
            say(loaded.outcome.refusal, true);
            return;
        }
        session_.setup.active = loaded.setup;
        session_.setup.on_file = loaded.setup;
        apply_setup(mail);
        say("restored setup " + quoted_setup_name(loaded.setup.name) + " from " +
                host_->setup_path + unresolved_note(loaded.setup),
            false);
    }

    /// WHAT TO SAY ABOUT THE PANES THIS BUILD COULD NOT PRESENT -- nothing when
    /// there are none, and the first one BY NAME when there are.
    ///
    /// `unresolved`, and never `unavailable`. Workshop knows one thing here: it
    /// has no catalog row for this reference. It does not know whether whoever
    /// could present it exists, is loading, has been unloaded, or was never
    /// installed -- and silence is not evidence of absence. Naming the reference
    /// is what lets a maker tell a typo from a pane they have not installed yet.
    static std::string unresolved_note(const Setup& s) {
        const std::vector<PaneRef> waiting = unresolved_panes(s);
        if (waiting.empty()) {
            return {};
        }
        std::string note = " -- " + std::to_string(waiting.size()) +
                           (waiting.size() == 1 ? " pane" : " panes") + " unresolved: " +
                           ref_text(waiting.front());
        if (waiting.size() > 1) {
            note += ", ...";
        }
        return note;
    }

    /// ASK FOR A BUILD -- by the name the TOOL gave, never by one of Workshop's.
    ///
    /// This is the sharpest statement of the split that this file makes. Workshop
    /// holds no target, no recipe and no command; the only build it can name is
    /// the one the Builder has already told it about, so a Workshop with a panel
    /// that has not yet heard from the tool cannot ask for anything at all, and
    /// says so.
    void build_now(loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return; // `b` is an unbound key with no Builder panel open, exactly as before
        }
        const BuilderPane& pane = session_.panels.builder;
        if (!pane.heard || pane.shown.target.empty()) {
            say("the Builder has not said what it builds yet -- nothing was asked for", true);
            return;
        }
        (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                zengine::builder::BuildRequested{pane.shown.target});
        // I ASKED. Workshop's own fact, recorded before anything is dispatched,
        // and the thing that decides whether the answer will be news to this
        // panel.
        //
        // THE SENTENCE CHANGED WITH ASYNC-1 AND THE CHANGE IS THE PHASE. It used
        // to say `the screen waits until it is done`, which was true and was the
        // measured cost of a runner that built inside its own handler. It is now
        // false: the runner starts a child, keeps it, and comes back to it on an
        // ordinary beat, so every other delivery in this program goes on
        // happening. Leaving the old words in place would have been the one kind
        // of stale comment this repository treats as a defect -- a sentence a
        // maker reads on the screen.
        session_.panels.builder.awaiting = true;
        say("asked the Builder for `" + pane.shown.target +
                "` -- Workshop stays live while it builds",
            false);
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

    /// IS THE INSPECTOR ON THE SCREEN AT ALL? Since PNL-0 the rows are shown by a
    /// panel a maker may remove, and `Session::rows` goes on existing when they
    /// do -- correctly, because the rows are a fact about the SELECTION and the
    /// selection is not the panel's. What must not go on happening is a gesture
    /// over them.
    bool inspector_shown() const { return session_.panels.has(panel::kInfo); }

    /// True if the gesture the caller is about to perform has nothing on screen
    /// to perform it on, HAVING SAID SO. A silent no-op would be the worse half
    /// of both available answers: these keys did something before Info could be
    /// removed, so a maker pressing one and seeing nothing has been given no way
    /// to tell a removed panel from a broken tool -- the same argument the empty
    /// OBJECTS list already won.
    bool inspector_absent() {
        if (inspector_shown()) {
            return false;
        }
        say("the properties are not showing -- p opens the Info panel", true);
        return true;
    }

    /// Step the inspector's cursor, when there is an inspector to step it in.
    void move_cursor(std::int64_t delta) {
        if (inspector_absent()) {
            return;
        }
        if (delta < 0) {
            if (session_.cursor > 0) {
                --session_.cursor;
            }
        } else if (session_.cursor + 1 < session_.rows.size()) {
            ++session_.cursor;
        }
    }

    /// BEGIN AN EDIT -- and refuse to begin one nobody can see.
    ///
    /// This is the guard that matters most of the three, because the state it
    /// prevents is a trap rather than a confusion: a draft opened with Info
    /// removed would put Workshop into editing mode, where `p` types a `p`
    /// instead of opening the picker, so the maker could not reopen the panel to
    /// find what they were editing -- and `^s` would then refuse to save, naming
    /// a row that is not on the screen.
    void begin_edit() {
        if (inspector_absent()) {
            return;
        }
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

    /// KEEP THE NAME EDITOR'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW -- the
    /// same call `refresh_inspector` makes for a property draft and the terminal
    /// makes for its command line, against the same one measurer
    /// (`setup_name_columns`). A surface that got narrower while a maker was
    /// typing must not leave the caret drawn off the end of its own row (HD-4).
    void refresh_setup_name() {
        if (!session_.setup.naming.open) {
            return;
        }
        session_.setup.naming.line.keep_caret_visible(
            setup_name_columns(screen_of(session_)));
    }

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
        refresh_terminal();  // the pane is a snapshot, and a snapshot is only true when taken
        refresh_inspector(); // and a draft's window is only true against the room it has now
        refresh_setup_name(); // ...and so is the name editor's, against the same room
        mail.publish(paint(state_, session_, host_->setup_path));
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

    /// The same sentence for the OTHER file, and it is a different sentence
    /// rather than a shared one because a maker who has a document file and no
    /// setup file must be told which of the two they are missing.
    static constexpr const char* kNoSetupFile =
        "no setup file -- start Workshop with --setup <path>";

    /// What to say to a gesture that would have changed the document or the
    /// selection out from under a live property draft. HD-7 wrote it for a press
    /// on the object list; HD-8's action controls are the second gesture to meet
    /// the same wall, which is the duplication that turns a literal into a name.
    /// Same sentence, same reason, same two ways out.
    static constexpr const char* kFinishDraftFirst =
        "finish the draft first -- enter commits it, esc cancels";

    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and
    /// a schema version is 32, so a wider number is REFUSED rather than
    /// truncated -- `send @x Foo 4294967297` must not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    HostContext* host_;
    Session session_;

    /// One moment's worth of memory: the character the gesture's OWN keystroke
    /// produced, which is not text a maker typed. Set by a gesture that opens a
    /// mode which takes text, cleared by the next key or the next text, so it
    /// can never outlive the moment it belongs to.
    ///
    /// Empty means nothing is owed. It holds the character rather than a bare
    /// flag so that a backend which reports the key and NO text cannot make the
    /// next real keystroke disappear.
    std::string swallow_text_;

    /// The document as it is ON DISK, or an empty one when nothing has been
    /// written yet. Session, emphatically: it is a copy kept so the status line
    /// can answer "is this saved" by comparing rather than by trusting a flag.
    WorkshopDoc saved_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_WEAVE_HPP
