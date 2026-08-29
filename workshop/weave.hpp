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
// that were simultaneously true (docs/reference/input.md):
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
#include "keymap_persist.hpp"
#include "prefs_persist.hpp"
#include "screen.hpp"
#include "session_persist.hpp"
#include "setup_persist.hpp"

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/terminal/input_lex.hpp> // ONE command grammar, Loom's -- never a second one here
#include <zen/terminal/session.hpp>
#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

    /// WHAT PROJECT REALIZATION IS WAITING ON, ANSWERED ALIVE (BLD-2).
    ///
    /// IT ARRIVES THE SAME WAY `request_stop` DOES: the host owns the realization
    /// owner as a local of its own `main`, wires a function that reads the owner's
    /// derived answers (`waiting_on`, `behind`) at the moment of the call, and hands
    /// the weave nothing else — no pointer, no state, no copy. The weave spends it
    /// when it paints the Builder panel and when the maker asks for the frontier,
    /// and holds the answer for exactly the length of that one spend, which is what
    /// keeps the panel's frontier the OWNER's frontier rather than a mirror of it.
    ///
    /// EMPTY IS ORDINARY and means no realization owner is wired — every suite
    /// fixture gets that by default, and the Builder panel then paints exactly as it
    /// did before this seam existed. It is a READING and not a power: nothing a
    /// holder of this function can do starts a build, performs a row, or moves the
    /// frontier by so much as an ask.
    std::function<ProjectFrontier()> frontier;

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

    /// The one file this Workshop's LAST SESSION is written to and read from (WUX-0).
    ///
    /// A THIRD PATH, AND A THIRD PROMISE. `document_path` is what a maker MADE and
    /// `setup_path` is a desk they deliberately NAMED; this is the desk and the window they
    /// happened to be using when they left, written when Workshop quits and read when it
    /// starts, with no gesture at either end. It is the host's to choose (`--session
    /// <path>`, defaulted) for the reasons the other two are, and it is a different file for
    /// the sharpest of them: an automatic save that could land on `setup_path` would rewrite
    /// a maker's explicitly named desk every time they closed the window.
    ///
    /// Empty means no session file was chosen, and startup restores nothing and shutdown
    /// writes nothing -- silently, both times, because a host that did not ask for
    /// continuity is not a host with a problem. That is also what every suite fixture gets
    /// by default, so a case has to opt IN to touching a file.
    std::string session_path;

    /// The one file this Workshop's KEYMAP is read from (KEY-0).
    ///
    /// A FOURTH PATH, AND A SIXTH KIND OF DURABLE FACT: the maker's hand. It is the
    /// host's to choose (`--keymap <path>`, defaulted) for the reasons the other three
    /// are, and a separate file because binding overrides describe neither the work, nor
    /// a desk, nor the desk in use -- see keymap_persist.hpp. Empty means no keymap file
    /// was chosen and the defaults-in-code stand, silently; an ABSENT file at a chosen
    /// path means exactly the same, because deleting the file is how a maker returns to
    /// the defaults. Only a file that EXISTS and cannot be admitted is refused, out loud.
    ///
    /// SINCE WUX-3 THE HOST'S DEFAULT FOR IT IS THE PER-USER CONFIGURATION ROOT, not the
    /// launch directory (user_paths.hpp owns the roots and the precedence; the host owns
    /// calling them). The weave is deliberately ignorant of all of that: it gets one
    /// string, and empty still means exactly what it has always meant here.
    std::string keymap_path;

    /// The one file this Workshop's presentation PREFERENCES live in (WUX-3).
    ///
    /// A FIFTH PATH, AND A SEVENTH DURABLE FACT: the maker's eyes -- see prefs_persist.hpp
    /// for why it is not the keymap's file. The host's to choose (`--prefs <path>`,
    /// defaulted to the per-user configuration root). Empty means no prefs file was
    /// chosen: the defaults stand, a toggle changes the live preference only, and nothing
    /// is read or written -- which is also what `--isolated` resolves it to.
    std::string prefs_path;

    /// ONE HUMAN-READABLE SENTENCE ABOUT THE LEGACY-FILE TRANSITION (WUX-3), or empty.
    ///
    /// The HOST performs the one-time import of pre-WUX-3 local files into the per-user
    /// roots (user_paths.hpp owns the rule) -- it happens before this weave exists, against
    /// paths the weave never learns. What the weave owes the maker is the SENTENCE: a
    /// transition that moved someone's settings must be said where they are looking, so
    /// whatever the host puts here is spoken once on the notice line at startup, beside
    /// the keymap's own word. The host also prints it to its banner; the two audiences
    /// (a terminal launch, a shortcut launch) overlap in neither direction.
    ///
    /// IT CARRIES ONLY WHAT HAPPENED. An import is an EVENT -- it ran once, at
    /// this launch, and converges by existence so it can never run again -- and that is what
    /// belongs on a row whose next sentence replaces it. A file that is still SHADOWED is a
    /// standing condition with a maker action, and travels in `standing_conditions` below.
    std::string transition_note;

    /// WHAT THE HOST ALREADY KNEW WAS TRUE, AND STILL IS.
    ///
    /// The conditions decided BEFORE this weave exists: today, exactly the shadowed legacy
    /// files. The host resolves the per-user roots and runs the one-time import in `main`,
    /// against paths the weave never learns, so the host is the owner of that truth and
    /// hands it over as a condition -- key, compact statement, its own explanation, its own
    /// role -- rather than as a sentence to be joined with somebody else's.
    ///
    /// IT IS NOT A CHANNEL. Nothing reads it back, nothing writes to it after startup, and
    /// the weave copies it once (`take_host_conditions`). A host that has nothing standing
    /// leaves it empty, which is what every suite fixture gets.
    std::vector<Condition> standing_conditions;

    /// AN ARTIFACT STEM, AS THIS PLATFORM SPELLS A SHARED LIBRARY.
    ///
    /// THE ONE RULE, AND IT IS THE HOST'S (LOAD-0). A directory, a separator and a
    /// suffix: that is the whole of what turns `zengine-timer` into a file, and
    /// keeping it here rather than in an authored plan is what makes ONE plan legal
    /// on Linux and on Windows -- no platform matrix, no per-OS field, no `.so` or
    /// `.dll` written down anywhere a person edits, and no package locator.
    ///
    /// IT TAKES A VIEW since LOAD-0, because a stem now arrives as a `std::string`
    /// read out of a file as often as it arrives as a literal. One signature that
    /// serves both is what keeps this the only place either spelling is resolved.
    std::string so(std::string_view stem) const { return so_in(dir, stem); }

    /// THE SAME RULE, AIMED SOMEWHERE ELSE (BLD-1).
    ///
    /// A build recipe may put its product somewhere other than beside this host -- an
    /// existing CMake target lands wherever its own project puts it -- so the Builder
    /// needs the file a stem means in a directory that is not `dir`. It is the same
    /// spelling and it stays in one place: a second copy of the suffix rule is how a
    /// Workshop comes to look for `.so` where CMake wrote `.dll`.
    ///
    /// STATIC, because it is a fact about the platform and not about this host. `so`
    /// above is the ordinary case with the host's own directory filled in.
    static std::string so_in(std::string_view directory, std::string_view stem) {
        return std::string(directory) + "/" + std::string(stem) +
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
                                          zengine::surface::SurfacePlacement,
                                          zengine::surface::SurfaceCloseRequested,
                                          zengine::surface::ClipboardText,
                                          zengine::surface::ClipboardCopy,
                                          zengine::builder::BuildStatus,
                                          zengine::builder::RecipeCatalog,
                                          zengine::workshop::PaneOffered,
                                          zengine::workshop::PaneContent>,
                             loom::Emit<zengine::surface::SurfaceCanvas,
                                        zengine::surface::SurfaceText,
                                        zengine::surface::ClipboardCopy,
                                        zengine::surface::ClipboardTextRequested,
                                        zengine::surface::SurfacePlacementRemembered,
                                        zengine::builder::StatusRequested,
                                        zengine::builder::BuildRequested,
                                        zengine::workshop::PaneCatalogRequested,
                                        zengine::workshop::PaneRoom,
                                        zengine::workshop::PanePressed,
                                        zengine::workshop::PaneKey,
                                        zengine::workshop::PaneTextInput>> {
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

    /// READ THE MAKER'S KEYMAP, OR STAND ON THE DEFAULTS (KEY-0).
    ///
    /// Three quiet endings and two spoken ones: no path chosen and no file present are
    /// both simply the defaults (deleting the file IS resetting the keymap, so an absent
    /// file must never be a complaint); an admitted file is applied and announced; an
    /// admitted file with a known backend gap carries the gap in the announcement; a file
    /// that exists and cannot be admitted is refused in its own words and the defaults
    /// stand -- Workshop does not rewrite, half-apply, or delete a file it could not
    /// understand.
    /// It runs on the FIRST `SurfaceReady`, the session restore's own moment and guard
    /// shape: once per process, before the restore's repaint, so the first band a maker
    /// reads is already projected from their own bindings. A key arriving before any
    /// surface exists is answered by the defaults, exactly as it is answered by the
    /// default desk -- the two files share one startup story.
    void load_keymap() {
        if (keymap_loaded_) {
            return;
        }
        keymap_loaded_ = true;
        if (host_->keymap_path.empty()) {
            return;
        }
        if (!std::filesystem::exists(host_->keymap_path)) {
            return;
        }
        const keymap_persist::LoadedKeymap loaded =
            keymap_persist::load_file(host_->keymap_path);
        if (!loaded.outcome.accepted) {
            // A STANDING WALL, AND IT IS SAID AS ONE. The file is still refused
            // an hour later and every later launch meets the same wall, so this is a
            // CONDITION and not a sentence about a moment -- established under its own key,
            // with the loader's own refusal as the explanation, and retracted by nobody
            // because nothing in a run can make an unreadable file readable.
            keymap_bad_ = true;
            session_.conditions.establish(
                Condition{kKeymapWallKey, "keymap refused -- default bindings stand",
                          loaded.outcome.refusal, surface::role::kAlert, std::string()});
            return;
        }
        session_.keymap = loaded.keymap;
        if (!session_.keymap.authored.empty() || session_.keymap.legend != legend_mode::kDefault) {
            keymap_word_ = "keymap " + host_->keymap_path + " applied -- " +
                           std::to_string(session_.keymap.authored.size()) + " override" +
                           (session_.keymap.authored.size() == 1 ? "" : "s");
            if (!session_.keymap.note.empty()) {
                keymap_word_ += "; " + session_.keymap.note;
            }
        }
    }

    /// READ THE MAKER'S PRESENTATION PREFERENCES, OR STAND ON THE DEFAULTS (WUX-3).
    ///
    /// The keymap's startup story, one file over, with the same three quiet endings (no
    /// path, no file, a file that changes nothing visible) and one spoken one (a file that
    /// exists and cannot be admitted is refused in its own words and the defaults stand).
    /// An applied preference speaks for itself on screen -- hidden titles are visibly
    /// hidden -- so unlike the keymap there is no applied-and-announced sentence.
    ///
    /// A REFUSED FILE IS ALSO A STANDING WALL: `prefs_bad_` keeps every later toggle from
    /// writing, because Workshop does not rewrite, half-apply or delete a file it could
    /// not understand -- and unlike the keymap, THIS file is one Workshop ordinarily
    /// writes, so the discipline needs the flag, not just restraint.
    void load_prefs() {
        if (prefs_loaded_) {
            return;
        }
        prefs_loaded_ = true;
        if (host_->prefs_path.empty()) {
            return;
        }
        if (!std::filesystem::exists(host_->prefs_path)) {
            return;
        }
        const prefs_persist::LoadedPrefs loaded = prefs_persist::load_file(host_->prefs_path);
        if (!loaded.outcome.accepted) {
            // THE KEYMAP WALL'S TWIN, one file over, and this one is load-bearing beyond
            // the sentence: `prefs_bad_` blocks every later write, so a maker who toggles a
            // preference is told again, in different words, at each toggle. The CONDITION
            // is the standing half -- true from this load until the process ends -- and the
            // toggle's refusal stays an event, because it is about the press.
            prefs_bad_ = true;
            session_.conditions.establish(
                Condition{kPrefsWallKey, "preferences refused -- defaults stand",
                          loaded.outcome.refusal, surface::role::kAlert, std::string()});
            return;
        }
        session_.pane_titles = loaded.titles_shown;
    }

    /// Say once what the startup file work DID, on the first surface that can show it --
    /// after the session restore, deliberately, so the sentence that survives on the one
    /// notice line is the one about this launch.
    ///
    /// THIS ROW CARRIES ONLY THE EVENT HALF, and the split is the whole point. A
    /// startup produces two kinds of fact and they used to share one string and one
    /// severity bit:
    ///
    ///     EVENT      `keymap <path> applied -- 3 overrides`, `imported your local keymap
    ///                from ... (the original was left in place)`. True once, about a moment
    ///                that has passed, and replaced by whatever is said next. THIS row.
    ///     CONDITION  a refused keymap file, a refused prefs file, a legacy file that is
    ///                still shadowed. Still true when it is read, an hour later and at the
    ///                next launch. `Session::conditions`, under a key, with a lifetime.
    ///
    /// So the severity bit is gone from here: everything left is something that HAPPENED
    /// and none of it is a wall. `keymap_bad_`/`prefs_bad_` remain what they always were
    /// beyond the sentence -- one blocks nothing, the other blocks every later prefs write.
    void speak_startup_notes(loom::Mail& mail) {
        if (startup_spoken_) {
            return;
        }
        startup_spoken_ = true;
        std::string word;
        for (const std::string* part : {&keymap_word_, &host_->transition_note}) {
            if (part->empty()) {
                continue;
            }
            if (!word.empty()) {
                word += "; ";
            }
            word += *part;
        }
        if (word.empty()) {
            return;
        }
        say(word, false);
        repaint(mail);
    }

    /// TAKE THE CONDITIONS THE HOST ALREADY KNEW.
    ///
    /// A shadowed legacy file is decided before this weave exists -- the host resolves the
    /// per-user roots and runs the one-time import in `main`, against paths the weave never
    /// learns -- so the host is the OWNER of that truth and hands it over rather than being
    /// asked for it. Establishing them here, once, beside the two file loads, is what keeps
    /// the whole standing-condition population arriving through one door on one occasion.
    ///
    /// THE HOST HANDS OVER CONDITIONS AND NOT SENTENCES. A refused file's word and a
    /// shadowed file's word used to be the same kind of thing joined with `"; "`; they are
    /// two kinds of thing now, and the host's own banner still prints both because a
    /// scrollback launch and a shortcut launch overlap in neither direction.
    void take_host_conditions() {
        for (const Condition& c : host_->standing_conditions) {
            session_.conditions.establish(c);
        }
    }

    /// A Skin claimed the surface and said hello: give it the whole screen. The
    /// operator weave's precedent, and the only thing Workshop needs in order to
    /// paint for the first time -- so load order decides nothing here either.
    /// AND IT IS WHERE WORKSHOP ASKS THE ROOM WHO HAS PANES (WP-0).
    ///
    /// DISCOVERY MUST CONVERGE IN BOTH LOAD ORDERS, and this is the half that answers the
    /// awkward one. A provider loaded BEFORE Workshop announces itself on its own attested
    /// activation, and that announcement is addressed to the `zengine.workshop` office --
    /// which, if Workshop is not mounted yet, is held by nobody, so the sentence reaches
    /// nobody and is gone. Nothing is retried, nothing is queued and nothing is buffered;
    /// what happens instead is that Workshop asks once it exists, and every provider that
    /// verified the ask answers again.
    ///
    /// WHY `SurfaceReady` AND NOT AN ACTIVATION. Workshop's weave is mounted IN-PROCESS by
    /// its host, and Loom deliberately does not send `zen.Activated` to a native mount --
    /// so this weave has no first breath to hang anything on, and manufacturing one would
    /// be a fake lifecycle event in the one application that is meant to demonstrate the
    /// real ones. `SurfaceReady` is the startup fact this file has always had: it is the
    /// signal that a medium exists, which is also the first moment a pane could be shown.
    ///
    /// IT IS OFFICE-PUBLISHED, and the publication is the one broadcast in this protocol.
    /// Workshop cannot address it -- knowing which offices have panes is the very thing it
    /// is asking -- so it speaks to everyone, deliberately as `zengine.workshop`, and a
    /// provider verifies that authorship before answering. Answering an unauthenticated
    /// broadcast would let any weave granted the shape harvest a provider's catalog.
    ///
    /// REPETITION IS HARMLESS BY CONSTRUCTION. A medium may say hello more than once (a
    /// Skin replacement does), so this may ask more than once, and a provider re-offers
    /// every time; `admit_pane_offer` refreshes an existing `PaneRef` in place and grows the
    /// catalog by nothing. The protocol needs no de-duplication of its own because identity
    /// does the work.
    void on(const zengine::surface::SurfaceReady&, loom::Mail& mail) {
        (void)mail.as_role(kWorkshopProvider).publish(PaneCatalogRequested{});
        // THE FIRST PICTURE OF A RUN IS WORKSHOP'S FLOOR, AND THAT IS LOAD-BEARING (WUX-0).
        //
        // A medium that has not been told anything has only this picture to size itself
        // from, and whatever it makes of it, it must not be that Workshop can never again be
        // smaller than the window its maker happened to leave open last night. So the run's
        // FIRST statement about how much room it wants is the smallest room it is honest in,
        // and the room it is trying to get BACK is the second -- a want, not a floor.
        // Reversing the two costs a maker the ability to shrink their window, which is a
        // stranger thing to lose to a continuity feature than anything it could have bought.
        load_keymap();
        // The prefs beside it (WUX-3), BEFORE the first paint: the first band and the
        // first pane headers a maker reads are already wearing their own preference.
        load_prefs();
        // ...and whatever the host already knew was standing, so the first picture
        // of the run already carries every condition this launch is going to have.
        take_host_conditions();
        repaint(mail);
        restore_last_session(mail);
        speak_startup_notes(mail);
    }

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
        // THE NORMAL WINDOW'S ROOM FOLLOWS THE SCREEN, EXCEPT WHILE THIS RUN'S MEDIUM SAYS
        // THE WINDOW IS MAXIMIZED (WUX-3). The medium reports placement BEFORE extent on
        // its beat (skin.hpp says why once), so by the time a maximized room arrives the
        // gate is already closed and the remembered normal viewport survives to the save.
        // A run whose medium never reports placement -- every terminal -- never gates, and
        // a maximized flag merely RESTORED from a file must not gate either: that is last
        // run's window, and this run's resizes are this run's to remember.
        if (!(medium_placed_ && session_.place_maximized)) {
            session_.normal_w = session_.screen_w;
            session_.normal_h = session_.screen_h;
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
        // AND THE COMPOSITION IS RECONCILED AGAINST THE ROOM IT NOW HAS (WP-0). A screen
        // that grew may have gained an overlay slot, and one that shrank may have lost the
        // one a panel was standing in -- so this is the second reason a reconcile happens
        // and the only one that is not a maker's gesture. Growth opens an authored pane that
        // was waiting for room; a shrink closes the presentation through the ordinary close
        // door, destroys its cache, and leaves the authored reference exactly where it was.
        // `apply_setup` is the one path either way, so a resize cannot open or close a panel
        // by some route the picker and a restore do not share.
        apply_setup(mail);
        repaint(mail);
    }

    /// THE MEDIUM SAID WHERE ITS WINDOW SITS (WUX-3). Remember it, whole and opaque.
    ///
    /// The second and last message that flows medium -> application here, and deliberately
    /// the dumbest handler in this file: the coordinates are the medium's own desktop
    /// units, which Workshop cannot interpret, cannot validate (its session law says so)
    /// and does not paint -- a moved window changes no pixel of any canvas -- so there is
    /// no adoption, no clamp, no repaint and no reconcile. What it buys is the memory the
    /// next orderly close writes down and the next launch hands back to whichever medium
    /// then holds the surface, plus the one live gate above: `medium_placed_` is what lets
    /// the extent handler tell a maximized room from a normal one.
    void on(const zengine::surface::SurfacePlacement& p, loom::Mail&) {
        medium_placed_ = true;
        session_.placement_known = true;
        session_.place_x = p.x;
        session_.place_y = p.y;
        session_.place_maximized = p.maximized;
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
    /// SINCE KEY-0 THE GESTURE IS RESOLVED THROUGH ONE BINDING TRUTH. The context comes
    /// from `keyboard_context(session_)` -- the routing chain, spelled once, where this
    /// function used to spell it and two hand-kept mirrors spelled it again -- and the
    /// gesture becomes an action identity through `session_.keymap`, exactly the value
    /// every help surface projects. What each action DOES is untouched: every arm below
    /// calls the same owner function it always called, because the keymap resolves names
    /// and can perform nothing.
    ///
    /// FIVE ACTIONS ARE ANSWERED ABOVE EVERY MODE, in the position the old four chords
    /// held. `document.save`/`document.open` are the document's two keys (a maker halfway
    /// through a width still means "save my work"; the draft-open refusal is
    /// save_document's own policy); `workshop.terminal` toggles the pane a maker must be
    /// able to reach from inside anything (its default moved to `ctrl+t` in KEY-0 -- the
    /// old `shift+space` cannot arrive from the POSIX backend at all, and is gone rather
    /// than kept as an invisible alias); `workshop.hotkeys` opens the view that explains
    /// the rest. `workshop.quit` is declared for exactly the contexts where nothing takes
    /// text (TEXT-0's `^c` law, carried by the declaration's kNoText context now): where
    /// text has the keyboard the chord travels the chain and the box's own vocabulary
    /// answers it, and quitting stays one press-elsewhere away.
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        // THE CONTEXT IS RESOLVED ONCE, AT ENTRY, and every decision this turn -- the
        // above-mode arm, the swallow, the chain -- spends the same answer, so a mode a
        // dispatch arm opens cannot change what THIS keystroke meant.
        const KeyContext ctx = keyboard_context(session_);
        // THE SWALLOW BELONGS TO ONE MOMENT: cleared on every key, armed only when this
        // keystroke is consumed as an application binding whose key also enters text
        // (`expected_text_of` derives the owed character from the binding -- the
        // generalization of the three hard-coded `" "`/`"s"`/`"w"` sites KEY-R0 found,
        // and deliberately not a swallow-the-next-text rule: an unmatched or absent
        // expectation eats nothing).
        swallow_text_.clear();
        if (session_.keymap.action_for(ctx, k.scancode, k.modifiers) != Act::kNone) {
            swallow_text_ = expected_text_of(k.scancode, k.modifiers);
        }
        // ONLY THE ROWS DECLARED ABOVE THE MODES ARE ANSWERED HERE. `workshop.quit`'s
        // ordinary `q` row resolves to the same action and still travels the chain --
        // which is what keeps the hotkey view's swallow, and every mode's ownership,
        // ahead of it.
        switch (session_.keymap.above_mode_action(ctx, k.scancode, k.modifiers)) {
        case Act::kQuit: quit(); return;
        case Act::kTerminalToggle:
            toggle_terminal();
            repaint(mail);
            return;
        case Act::kSaveDocument:
            save_document();
            repaint(mail);
            return;
        case Act::kOpenDocument:
            load_document();
            repaint(mail);
            return;
        case Act::kHotkeys:
            toggle_hotkeys();
            repaint(mail);
            return;
        case Act::kAttention:
            toggle_attention();
            repaint(mail);
            return;
        default: break;
        }
        // THE HOTKEY VIEW IS KEYS-MODAL WHILE IT IS OPEN: the five arms above still
        // answer (its own toggle is one of them), and everything else is the view's to
        // spend or swallow -- a maker reading a key list must not be executing it. The
        // context beneath is untouched, which is why `ctx` above still names it.
        if (session_.hotkeys.open) {
            hotkeys_key(k);
            repaint(mail);
            return;
        }
        // THE CHAIN IS `keyboard_context`'S ANSWER NOW (KEY-0). Its order -- and every
        // recorded rationale behind it: the modes that own the keyboard whole, the
        // reachability arguments that are written down anyway, the pressed-into-LAST
        // symmetry that puts a focused pane above a live draft, `keyboard_pane` resolved
        // fresh with nothing to clear -- lives with the resolver, where the paint path
        // and the paste mirror read the same answer. This switch is what remains of four
        // hand-copies of that order: which owner the resolved context names.
        //
        // A COPY ANYWHERE BELOW IS SAID TO THE PROCESS ONCE, HERE (TEXT-0). The component
        // bumps the clipboard's `writes` exactly when a copy or cut took text, so one
        // comparison around the whole chain notices it whichever consumer it happened in —
        // three handlers each publishing would be the fourth-copy accident arriving in the
        // routing. What is published is `ClipboardCopy`: the Skin offers it to the
        // platform's clipboard and every other text-holding participant mirrors it.
        //
        // A PASTE ANYWHERE BELOW IS ASKED FOR ONCE, HERE, THE SAME WAY (QR-11). The
        // component bumps `paste_requests` instead of pasting, because the value a paste
        // means is the clipboard's CURRENT value and only this owner can obtain it — a
        // read performed BECAUSE this paste was requested, never a mirror kept fresh by
        // watching. The same one comparison notices it, `paste_owner_now()` (a derivation
        // of the resolved context since KEY-0, not a second spelling of the chain) says
        // which draft asked, and `begin_clipboard_paste` opens the conversation with the
        // Skin. The text lands a turn later, in that draft or nowhere
        // (`on(ClipboardText)`).
        const std::uint64_t copied_before = session_.clipboard.writes;
        const std::uint64_t pastes_before = session_.clipboard.paste_requests;
        switch (ctx) {
        case KeyContext::kTerminal: terminal_key(k); break;
        case KeyContext::kArrangePane:
        case KeyContext::kArrangeDesk:
        case KeyContext::kArrangeReset: arrange_key(k, mail); break;
        case KeyContext::kNaming: naming_key(k, mail); break;
        case KeyContext::kPicker: picker_key(k, mail); break;
        case KeyContext::kAttention: attention_key(k); break;
        case KeyContext::kContext: context_key(k, mail); break;
        case KeyContext::kPane: external_key(keyboard_pane(), k, mail); break;
        case KeyContext::kDraft: editing_key(k); break;
        default: command(k, mail); break;
        }
        if (session_.clipboard.writes != copied_before) {
            mail.publish(zengine::surface::ClipboardCopy{session_.clipboard.text});
        }
        if (session_.clipboard.paste_requests != pastes_before) {
            begin_clipboard_paste(mail);
        }
        repaint(mail);
    }

    /// Open or close the full hotkey view. The whole of the mode change --
    /// `toggle_terminal`'s own shape, one screen element over.
    void toggle_hotkeys() { session_.hotkeys.open = !session_.hotkeys.open; }

    /// THE VIEW'S OWN KEYS: Escape closes it, and everything else is swallowed -- the
    /// view is modal for exactly as long as it is being read, so a maker cannot execute
    /// a binding while looking it up. The toggle itself is answered above (it is a
    /// global), so both advertised ways out work; the heading's `esc closes` claim and
    /// this branch are one pinned pair, and Escape is deliberately NOT a keymap action:
    /// the view is not a context, and a modal surface's one structural way out must not
    /// be authorable into a lockout.
    void hotkeys_key(const zengine::input::KeyPressed& k) {
        if (k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone) {
            session_.hotkeys.open = false;
        }
    }

    /// Open or close the current-condition view -- `toggle_hotkeys`' own shape,
    /// one surface over. Opening it puts the cursor back on the loudest condition, because
    /// a cursor left where the maker last was would point at whatever happens to be in that
    /// position now: the population is recomputed from live owners and a row is a fact
    /// about the world, not a slot.
    ///
    /// IT IS A MAKER'S GESTURE AND NOTHING ELSE CAN CALL IT. No severity opens this, no
    /// count opens it, and no condition becoming true opens it -- a modal is earned by
    /// required maker intent, never by diagnostic severity, and there is no branch anywhere
    /// in this weave that reaches this function from an arrival.
    void toggle_attention() {
        session_.attention.open = !session_.attention.open;
        session_.attention.cursor = 0;
    }

    /// THE VIEW'S OWN KEYS: move the cursor, hide the condition it is on, close.
    ///
    /// THE CURSOR IS REPAIRED THROUGH THE POPULATION'S OWN OWNER BEFORE ANYTHING INDEXES IT
    /// -- the picker's rule, and this list needs it more, not less: its population is
    /// DERIVED, so a pane recovering or a build finishing can shrink it between two
    /// keystrokes with no gesture in between.
    ///
    /// DISMISSAL IS PRESENTATION-ONLY AND THIS IS THE WHOLE OF IT. It writes one entry in
    /// the view's own set; it does not touch `HeldConditions`, any pane, any build, the
    /// keymap, the prefs, or any file. The condition is still true afterwards, still
    /// returned by `attention_conditions`, still readable by its owner -- and it comes back
    /// on its own the moment its content changes, because the dismissal was scoped to the
    /// statement rather than to the key alone (`AttentionView::hides`).
    void attention_key(const zengine::input::KeyPressed& k) {
        AttentionView& view = session_.attention;
        std::vector<Condition> shown = attention_shown(session_, frontier_now());
        if (view.cursor >= shown.size()) {
            view.cursor = shown.empty() ? 0 : shown.size() - 1;
        }
        switch (session_.keymap.action_for(KeyContext::kAttention, k.scancode, k.modifiers)) {
        case Act::kAttentionUp:
            if (view.cursor > 0) {
                --view.cursor;
            }
            break;
        case Act::kAttentionDown:
            if (view.cursor + 1 < shown.size()) {
                ++view.cursor;
            }
            break;
        case Act::kAttentionDismiss:
            if (view.cursor < shown.size()) {
                const std::string hidden = shown[view.cursor].compact;
                view.dismiss(shown[view.cursor]);
                // AN EVENT, SAID AS ONE. What just happened is that a maker hid a
                // presentation; what remains true is the condition, which is why the
                // sentence is about the gesture and not about the subject.
                say("hidden -- " + hidden + " is still true", false);
            }
            break;
        case Act::kAttentionClose: view.open = false; break;
        default:
            // THE KEY THAT OPENED IT CLOSES IT -- the picker's and the terminal overlay's
            // shared rule, following the OPENER'S effective binding wherever a maker moved
            // it. The toggle is a global, so it is answered above this switch and never
            // reaches here; this arm is what makes a remapped opener still close.
            if (session_.keymap.matches(Act::kAttention, k.scancode, k.modifiers)) {
                view.open = false;
            }
            break;
        }
    }

    // ---- What can I do with this? The contextual-action surface (CTX-0) -------
    //
    // POINTING NAMES A SUBJECT FOR ONE REQUEST. SELECTION IS A STATE A MAKER ENTERED.
    // Opening this surface captures a temporary subject -- a `PaneRef`, an object id, or
    // nothing -- and changes no persistent selection, no keyboard candidate and no focus.
    // OPEN REMEMBERS AN IDENTITY; SPEND RE-ASKS ITS OWNER: a chosen row closes the
    // surface and hands the captured identity to the same owner operation the keyboard
    // reaches, which answers absence and refusal in its own words. The surface owns only
    // its subject and its own interaction mode, and it performs nothing itself.

    /// OPEN ON WHAT IS POINTED AT. The subject resolvers are the pointer route's own --
    /// `occupied_at` for a presentation, `object_at` for the authored material -- asked
    /// once, at the press, for identity ONLY. A presentation that is not an arrangeable
    /// pane (the picker's rectangle) and an empty cell both name the room: a subject with
    /// no identity is a real subject here.
    ///
    /// DELIBERATELY UNWRITTEN: `session_.panels.keyboard`. A LEFT press points the
    /// keyboard at what it lands on; a right press asks a question about it, and asking
    /// about a pane must not steal the keys from wherever the maker was typing.
    void open_context_at(const PointedAt& at) {
        ContextMenu next;
        next.open = true;
        // THE PRESS'S OWN CELL IS THE ANCHOR (ARR-0): the surface opens beside the hand
        // that asked, on both media at the cell grain -- the composition is settled in
        // cells before any metric is consulted, HD-10's own medium-independence rule.
        // The bounds stay derived; only the gesture's place is captured.
        next.anchored = true;
        next.anchor_x = at.cell.x;
        next.anchor_y = at.cell.y;
        const Occupancy here =
            occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
        if (here.occupied) {
            // The setup row that RESOLVES to the pointed presentation -- the durable
            // identity, never the kind handle (`arrange_press`'s own walk). The picker's
            // rectangle resolves to no row and falls through to the room.
            for (const SetupPane& row : session_.setup.active.panes) {
                const std::optional<std::int64_t> named =
                    resolve_pane(row.ref, session_.panels.runtime);
                if (named.has_value() && *named == here.kind) {
                    next.subject = context_subject::kPane;
                    next.pane = row.ref;
                    break;
                }
            }
        } else {
            const std::int64_t id = object_at(state_, session_, workspace_cell_x(at.cell.x),
                                              workspace_cell_y(at.cell.y));
            if (id != 0) {
                next.subject = context_subject::kObject;
                next.object = id;
            }
        }
        session_.context = next;
    }

    /// OPEN BY KEY, on the subject command mode can truthfully name: the selected object
    /// while one resolves, else the room. This is the route that keeps the capability
    /// honest on a medium whose environment never delivers the second button -- and it
    /// deliberately does not reach for a pane: pane management IS the keyboard's road to
    /// the pane vocabulary, `manage.remove` included, and a subject the current state
    /// does not name must not be guessed at.
    void open_context_ambient() {
        ContextMenu next;
        next.open = true;
        if (doc::find(state_, session_.selected) != nullptr) {
            next.subject = context_subject::kObject;
            next.object = session_.selected;
        }
        session_.context = next;
    }

    /// Close it whole: subject, group and cursor go together, so a later open cannot
    /// inherit a stale identity. Silent -- the surface disappearing is the statement.
    void close_context() { session_.context = ContextMenu{}; }

    /// The surface's own keys: the picker's four, plus the opener closing it.
    void context_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        ContextMenu& menu = session_.context;
        const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
        menu.cursor = context_cursor_bound(menu.cursor, rows.size());
        switch (session_.keymap.action_for(KeyContext::kContext, k.scancode, k.modifiers)) {
        case Act::kContextUp:
            if (menu.cursor > 0) {
                --menu.cursor;
            }
            break;
        case Act::kContextDown:
            if (menu.cursor + 1 < rows.size()) {
                ++menu.cursor;
            }
            break;
        case Act::kContextChoose: choose_context_row(mail); break;
        case Act::kContextBack:
            // ESCAPE DOES THE APPROPRIATE SMALLER THING: out of an open group, else out
            // of the surface -- pane management's done/close pair, in a surface whose
            // depth is presentation state rather than a submode.
            if (!menu.group.empty()) {
                leave_context_group();
            } else {
                close_context();
            }
            break;
        default:
            // THE KEY THAT OPENED IT CLOSES IT -- the shared rule, following the
            // opener's effective binding wherever a maker moved it.
            if (session_.keymap.matches(Act::kContextOpen, k.scancode, k.modifiers)) {
                close_context();
            }
            break;
        }
    }

    /// Back out one level, with the cursor landing on the group the maker just left --
    /// what makes backtracking read as returning rather than starting over.
    void leave_context_group() {
        ContextMenu& menu = session_.context;
        const std::string was = menu.group;
        menu.group.clear();
        menu.cursor = 0;
        const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].is_group && was == rows[i].group) {
                menu.cursor = i;
                break;
            }
        }
    }

    /// CHOOSE THE ROW THE CURSOR IS ON. One action whose meaning the row decides
    /// (`picker.choose`'s shape): a group row descends, an action row requests -- and a
    /// request CLOSES THE SURFACE FIRST, because the maker's question is answered the
    /// moment they choose; what remains is the operation, and the operation may open a
    /// mode of its own.
    void choose_context_row(loom::Mail& mail) {
        ContextMenu& menu = session_.context;
        const std::vector<ContextEntry> rows = context_population(menu.subject, menu.group);
        if (menu.cursor >= rows.size()) {
            return; // the belt, not the door
        }
        const ContextEntry chosen = rows[menu.cursor];
        if (chosen.is_group) {
            menu.group = chosen.group;
            menu.cursor = 0;
            return;
        }
        if (chosen.row == nullptr) {
            return; // unreachable: the catalog cross-check is a compile-time assertion
        }
        const ContextMenu spent = menu;
        close_context();
        spend_context_choice(chosen.row->act, spent, mail);
    }

    /// SPEND ONE CHOSEN ACTION against the captured subject. Every arm calls the owner
    /// the keyboard calls: the pane rows go through the one target-taking seam with the
    /// CAPTURED pane, the object row through the explicit-id delete, and the room rows
    /// are the same zero-target owner calls `command()` makes -- duplicated one-line
    /// arms, deliberately, because a zero-target call has no target to drift.
    void spend_context_choice(Act a, const ContextMenu& spent, loom::Mail& mail) {
        switch (a) {
        // -- the pointed pane ---------------------------------------------------------
        case Act::kArrange: enter_arrange_pane(spent.pane); break;
        case Act::kManageFront:
        case Act::kManageBack:
        case Act::kManageRaise:
        case Act::kManageLower:
        case Act::kManageResetPlace:
        case Act::kManageResetWidth:
        case Act::kManageResetHeight:
        case Act::kManageRemove: spend_pane_action(a, spent.pane, mail); break;
        // -- the pointed object -------------------------------------------------------
        case Act::kObjectDelete: context_delete_object(spent.object); break;
        // -- the room -----------------------------------------------------------------
        case Act::kObjectNew: create_object(); break;
        case Act::kPicker: open_picker(); break;
        case Act::kArrangeDesk: open_arrange_desk(); break;
        case Act::kTerminalToggle: toggle_terminal(); break;
        case Act::kAttention: toggle_attention(); break;
        case Act::kHotkeys: toggle_hotkeys(); break;
        case Act::kSaveDocument: save_document(); break;
        case Act::kOpenDocument: load_document(); break;
        case Act::kSetupName: open_setup_name(); break;
        case Act::kSetupRestore: restore_setup(mail); break;
        case Act::kManageResetOrder: reset_front_order(); break;
        default: break;
        }
    }

    /// A BUTTON-1 PRESS WHILE THE SURFACE IS OPEN. Inside the rectangle a press is the
    /// pointer's choose -- the same population index the painter drew, through the
    /// painter's inverse -- and the surface's own furniture consumes a press silently.
    /// OUTSIDE it, the press is spent on dismissal: consumed whole, reaching no pane, no
    /// provider, no object and no keyboard candidate, because a click that closes a menu
    /// must not also operate what it happened to land on.
    void context_press(const PointedAt& at, std::int64_t space, std::int64_t x,
                       std::int64_t y, loom::Mail& mail) {
        const ContextPressAt hit =
            context_press_at(session_, screen_of(session_), space, x, y, at);
        if (!hit.inside) {
            close_context();
            return;
        }
        if (!hit.entry) {
            return;
        }
        session_.context.cursor = hit.index;
        choose_context_row(mail);
    }

    /// ANOTHER PARTICIPANT'S COPY — a pane provider's field, mirrored under the no-echo
    /// rule: the counter is left alone, because `writes` counts what THIS weave's boxes
    /// copied and a mirror that bumped it would republish the fact back at the bus. Since
    /// QR-11 this publication is the ONLY feed the mirror has — the platform's own
    /// clipboard is read at paste time, through the Skin, and never watched — so
    /// `session_.clipboard.text` means exactly "the freshest copy said IN this process",
    /// which is also precisely what a paste falls back to on a medium whose platform
    /// cannot be read (the terminal's standing state).
    void on(const zengine::surface::ClipboardCopy& c, loom::Mail&) {
        session_.clipboard.text = c.text;
    }

    /// THE SKIN'S ANSWER TO A PASTE THIS WEAVE REQUESTED — the one road foreign clipboard
    /// text has into this application, and it is walked only under a maker's paste (QR-11).
    ///
    /// TWO WALLS, THEN A VALIDITY CHECK, and each refusal discards the payload whole.
    /// `answers_ask()` is Loom's own provenance — an unsolicited `ClipboardText`, however
    /// well-formed, is somebody's helpful payload and settles nothing (the stronger bound
    /// exists here because the Skin ANSWERS rather than relays, so it is taken — INTR-1's
    /// rule). The book's correlation-plus-sender match then says WHICH paste this settles.
    /// Last, the draft that asked must still be standing: the same owner, holding the same
    /// draft (`draft_epoch`), because focus and mode changes between request and answer
    /// must not redirect clipboard text into another box, and a draft that ended took its
    /// paste with it.
    ///
    /// THE MIRROR IS UPDATED ONLY WHEN THE PASTE APPLIES. A readable answer for a draft
    /// that no longer exists is dropped entirely — retaining it would keep foreign text in
    /// application state on the strength of an intent that no longer has a home.
    void on(const zengine::surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return; // not Loom's answer to anything this weave asked
        }
        const std::optional<loom::PendingAsk> settled =
            paste_asks_.settle(mail.correlation(), mail.sender());
        if (!settled) {
            return; // not a conversation this weave is waiting on
        }
        const PendingPaste p = take_pending_paste(settled->id);
        component::TextBox* box = nullptr;
        Row* row = nullptr;
        switch (p.owner) {
        case PasteOwner::kNone: return;
        case PasteOwner::kTerminal:
            // The line outlives the pane's visibility (shift+space hides it and keeps the
            // draft), so an open pane is not required — the same DRAFT is.
            box = &session_.terminal.input;
            break;
        case PasteOwner::kNaming:
            box = session_.setup.naming.open ? &session_.setup.naming.line : nullptr;
            break;
        case PasteOwner::kDraft:
            row = editing_row();
            if (row == nullptr || row->label() != p.label || session_.selected != p.object ||
                row->editor().draft_epoch() != p.epoch) {
                row = nullptr; // a different draft is standing (or none); not this paste's
            }
            break;
        }
        if (row != nullptr) {
            if (a.readable) {
                session_.clipboard.text = a.text; // the platform's current truth, asked for
            }
            row->paste(session_.clipboard);
        } else if (box != nullptr && box->draft_epoch() == p.epoch) {
            if (a.readable) {
                session_.clipboard.text = a.text;
            }
            box->paste(session_.clipboard);
            if (p.owner == PasteOwner::kTerminal) {
                refresh_terminal();
            }
        } else {
            return; // the draft that asked is gone; the payload is discarded, silently
        }
        repaint(mail);
    }

    // AN UNANSWERABLE ASK STAYS OPEN, VISIBLY, AND THAT IS DELIBERATE (QR-11). With no
    // Skin holding the role — or a stale one that does not accept the shape — the ask is
    // refused as a tap event and no message returns: Loom has no unanswerability notice,
    // and the book does not pretend otherwise (its own doctrine). The cost is bounded by
    // the book's capacity — after four asks into a void, paste goes quiet for this
    // incarnation — and a process whose Skin cannot answer a clipboard read has no paste
    // to deliver anyway. No `zen.Refused` handler is written here, because nothing sends
    // one for this conversation and a handler would be a wall waiting for rain that
    // cannot fall.

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
        // THE HOTKEY VIEW TAKES NO TEXT AND TYPES NONE, exactly as it spends the keys: a
        // maker reading a key list is not typing anywhere, and the surface beneath must
        // come back untouched when the view closes.
        if (session_.hotkeys.open) {
            return;
        }
        // WHERE A CHARACTER GOES IS THE SAME QUESTION AS WHERE A KEY GOES, and since
        // KEY-0 it is answered by the same resolver instead of by this function's own
        // hand-copy of the chain (the second of the five spellings KEY-R0 measured).
        // Per branch, the standing law is unchanged: a mode that owns the keyboard whole
        // takes the text or deliberately types none (arrangement and the picker are driven
        // by unmodified letters, so every character produced while they are open belongs
        // to a gesture); a focused pane receives the text in exactly the position it
        // receives the keys -- the half that makes `%` reach a provider at all, since
        // Workshop maps no key to any character; a live draft types; and in command mode
        // text is simply not a command.
        switch (keyboard_context(session_)) {
        case KeyContext::kNaming:
            session_.setup.naming.line.type(t.text);
            repaint(mail);
            return;
        case KeyContext::kTerminal:
            // AT THE CARET, WHICH SINCE HD-3 IS NOT ALWAYS THE END. `type` is the only
            // door that moves the text and the caret together, so a keystroke in the
            // middle of a line cannot leave one behind. The line changed, so what could
            // be said next changed with it (HD-2): typing IS the completion gesture.
            session_.terminal.input.type(t.text);
            refresh_terminal();
            repaint(mail);
            return;
        case KeyContext::kArrangePane:
        case KeyContext::kArrangeDesk:
        case KeyContext::kArrangeReset:
        case KeyContext::kPicker:
            return;
        case KeyContext::kPane:
            external_text(keyboard_pane(), t, mail);
            return;
        case KeyContext::kDraft: {
            Row* row = editing_row();
            if (row == nullptr) {
                return; // unreachable while the resolver holds; written anyway
            }
            row->type(t.text);
            repaint(mail);
            return;
        }
        default:
            return; // command mode: text is simply not a command
        }
    }

    /// WHAT A BUTTON-1 RELEASE ENDED — and it is asked, not assumed (WIND-2a).
    ///
    /// `pane` is meaningful only when `pane_held`; `document_id` only when `document`.
    struct GesturesEnded {
        bool document = false;
        std::int64_t document_id = 0;
        bool pane_held = false;
        PaneRef pane;
    };

    /// END EVERY BUTTON-1 GESTURE THIS SESSION IS HOLDING, whatever mode saw the release.
    ///
    /// ONE OWNER, THREE CALLERS, AND THAT IS THE WHOLE OF THE REPAIR (WIND-2a). A gesture
    /// begins under one mode and is released under another: a maker drags a pane, opens the
    /// Terminal over it with shift+space, and lets go. The mode that sees the release is not
    /// the mode that owns the gesture, so every mode that answers a release first has to end
    /// ALL of them -- and WIND-2's Terminal branch ended only the document's, because a
    /// document drag was the only gesture that existed when that branch was written. The
    /// symptom is a stranded `active` flag with the button up and a pane that follows the
    /// pointer afterwards.
    ///
    /// IT SAYS NOTHING. What to tell a maker is the caller's, because the answer genuinely
    /// differs: management names the pane it placed, the ordinary path names the object it
    /// released, and the Terminal branch says nothing at all -- the notice line is not
    /// painted while the pane covers it, so a sentence made there is one nobody can read
    /// that would then reappear, stale, when the pane closes.
    ///
    /// IT IS NOT A CAPTURE FRAMEWORK. There is no focus, no target, no capture stack and no
    /// registry -- two gesture records and one function that clears both.
    GesturesEnded end_held_gestures() {
        GesturesEnded out;
        if (session_.drag.active) {
            out.document = true;
            out.document_id = session_.drag.id;
            end_drag(session_);
        }
        if (session_.pane_drag.active) {
            out.pane_held = true;
            out.pane = session_.pane_drag.pane;
            session_.pane_drag = PaneGesture{};
        }
        // The text-selection drag ends silently and is not reported: the selection it swept
        // is on screen, which is the whole statement (TEXT-0). The selection itself SURVIVES
        // the release — ending the sweep is not unselecting — so only the gesture record is
        // cleared here.
        session_.text_drag = TextDrag{};
        return out;
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
            if (!b.pressed && b.button == 1) {
                // EVERY BUTTON-1 GESTURE, and not only the document's (WIND-2a). A pane
                // move or size begun in pane management is held in a second record, and
                // the overlay used to swallow its release exactly as it once swallowed the
                // document's: `pane_drag.active` stayed true with the button up, and the
                // first bare motion after the pane closed moved a window nobody was
                // holding. Measured -- the pane walked to the pointer.
                (void)end_held_gestures();
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
        // ARRANGEMENT IS A MODE AND IT OWNS THE POINTER WHILE IT IS OPEN (WIND-2) -- the
        // Terminal's own shape, four lines up, for the same reason. While a maker is
        // arranging, every press is about a pane: letting one fall through to the
        // document would begin a drag on an object underneath a pane they are looking at,
        // which is the defect PNL-2 removed from panels in the first place.
        //
        // A SECONDARY PRESS IS THIS STATE'S WAY BACK OUT (ARR-0). The active interaction
        // that can truthfully interpret a secondary press receives first refusal, and
        // leaving is what this one truthfully means by it: the press leaves the
        // arrangement -- whichever scope, the reset prompt included -- and is CONSUMED
        // WHOLE. One consumed gesture performs one interaction transition: no context
        // menu opens from this press, and its release falls to the ordinary path's
        // non-primary drop exactly as every second-button release always has. This is a
        // state-local reading, not a Back command: there is no `right_click_back` action,
        // no keymap row, and the ordinary contextual opener still answers only the
        // presses no active interaction claimed.
        //
        // A RELEASE STILL ENDS A DOCUMENT DRAG THAT BEGAN BEFORE THE MODE DID, and this is
        // the same repair HD-3 made for the pane: entering a mode mid-drag must not swallow
        // the release, or `drag.active` stays true with the button up and the next bare
        // motion drags an object nobody is holding.
        if (session_.arrange.open) {
            const PointedAt where = canvas_point_of(b.space, b.x, b.y);
            if (b.pressed && b.button == 3) {
                close_arrange();
                repaint(mail);
                return;
            }
            if (b.button != 1) {
                return;
            }
            if (!b.pressed) {
                const GesturesEnded done = end_held_gestures();
                if (done.pane_held) {
                    // A RELEASE ENDS THE GESTURE WHEREVER THE HAND LANDS, and it is not asked
                    // where that is -- the position is not part of ending something.
                    say("placed " + ref_text(done.pane) + " -- " + arrange_status(), false);
                    repaint(mail);
                }
                return;
            }
            if (!where.understood) {
                return;
            }
            arrange_press(where);
            repaint(mail);
            return;
        }
        // THE CONTEXTUAL SURFACE HAS FIRST REFUSAL WHILE IT IS OPEN (CTX-0) -- a mode in
        // the two above's family, below both because both existed first and neither can
        // be open at the same time as this one through any current door. A press inside
        // it navigates or chooses; a press outside it dismisses and is CONSUMED, so a
        // click spent on closing a menu cannot also select an object, focus a pane or
        // reach a provider. A further right press re-asks the question about whatever is
        // pointed at now -- opening is re-targeting, not a toggle.
        if (session_.context.open) {
            const PointedAt where = canvas_point_of(b.space, b.x, b.y);
            if (b.pressed && b.button == 3) {
                if (where.understood) {
                    open_context_at(where);
                    repaint(mail);
                }
                return;
            }
            if (b.button != 1) {
                return;
            }
            if (!b.pressed) {
                // A RELEASE STILL ENDS A GESTURE THAT BEGAN BEFORE THE SURFACE OPENED --
                // the Terminal's and management's own repair: a right press can arrive
                // mid-drag, and occluding the release would leave a drag active with the
                // button up.
                (void)end_held_gestures();
                return;
            }
            if (!where.understood) {
                return;
            }
            context_press(where, b.space, b.x, b.y, mail);
            repaint(mail);
            return;
        }
        const PointedAt at = canvas_point_of(b.space, b.x, b.y);
        // A RIGHT PRESS ASKS "WHAT CAN I DO WITH THIS?" (CTX-0). Before this branch a
        // second button meant nothing anywhere in Workshop, so consuming it displaces no
        // behaviour and steals nothing from any provider -- the pane seam cannot say a
        // second button, deliberately, and no `PanePressed` is sent for one. Only a press
        // opens; a release of button 3 falls through to the gate below and is dropped, as
        // every non-primary transition always was.
        if (b.pressed && b.button == 3 && at.understood) {
            open_context_at(at);
            repaint(mail);
            return;
        }
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
            // AND THE OCCUPANCY WALK IS RESOLVED HERE TOO SINCE MSG-0, beside the body and
            // the canvas point, for the reason QR-2 hoisted the body: it is one question
            // about one place, every handler below changes nothing on the path where it
            // declines, and the answer is now needed BEFORE the chain rather than after it.
            // It is the same pure walk `occupied_at` always was -- the picker first, then
            // the panes topmost-first, then nothing -- moved, not changed.
            const Occupancy here =
                occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
            // WHERE THE KEYBOARD GOES IS DECIDED BY THE PRESS ITSELF, IN ONE LINE, BEFORE
            // ANY LAYER ANSWERS IT (MSG-0). Putting it in the routing arms instead would be
            // four decisions -- one per arm, one of them easy to forget -- about a single
            // fact: which presentation did the maker just point at. A press on an external
            // pane points the keyboard there; a press on Workshop's own furniture, on the
            // workspace, or on nothing at all takes it away again.
            //
            // IT IS SET FOR THE WHOLE RECTANGLE, not for the rows inside it. A press on the
            // pane's header or on the padding under its last prose line names no row and
            // sends no `PanePressed` -- and it is still unambiguously a maker pointing at
            // that pane, which is the only question this line asks.
            //
            // AND THE MODES ABOVE NEVER REACH IT. The Terminal and pane management take
            // every press whole, one branch up, so opening either leaves the candidate
            // exactly where it was and closing it hands the keyboard straight back --
            // which is the same "closing it restores every gesture exactly" this file
            // already promises about the pointer.
            session_.panels.keyboard =
                here.occupied && is_runtime_kind(here.kind) ? here.kind : kNoPaneKind;
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
            //
            // (`here` was resolved at the top of this branch since MSG-0 -- one walk, for
            // two questions that are about the same press.)
            // AND AN EXTERNAL PANE IS THE ONE PRESENTATION WHOSE PRESS GOES SOMEWHERE
            // (SEL-0). It is the SAME occupancy answer -- one geometry walk, one topmost
            // rule, the picker still first -- asked one further question: this cell belongs
            // to a pane Workshop did not compile, so the press is that provider's.
            //
            // CONSUMED EITHER WAY, AND DECIDED HERE RATHER THAN THERE. A pane that owns
            // visible room owns pointer refusal for that room, and the refusal is Workshop's
            // to make because Workshop is what knows the room exists. Nothing waits for the
            // provider: there is no reply shape, `external_press` sends and returns, and a
            // press that named no row of the body (the header, the padding under the last
            // prose line, the lattice's edge) is consumed exactly the same and simply
            // travels no further. That is WP-R0's split -- the synchronous half of the
            // question is geometry Workshop already holds, so `consumed` never crosses the
            // wire.
            //
            // AND WORKSHOP SAYS NOTHING, WHICH IS THE ONE PLACE THE RULE ABOVE INVERTS.
            // The sentence three lines up is TRUE of a built-in -- there really is nothing
            // under a Builder to take hold of -- and would be a claim about an OUTCOME here,
            // made before the outcome exists: what a press on a provider's row means is that
            // provider's vocabulary, the answer arrives later as ordinary content, and
            // Workshop cannot name either. So the statement is the pane's to make, in its
            // own rows, and this layer leaves the line alone rather than writing a sentence
            // it would have to guess (INT-R0: a refusal belongs to the deepest layer whose
            // vocabulary contains the reason -- and this one's does not).
            if (here.occupied && is_runtime_kind(here.kind)) {
                external_press(here.kind, b, mail);
            } else if (here.occupied) {
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
        } else {
            // A RELEASE IS NOT ASKED THE SAME QUESTION, and the asymmetry is the
            // reason no capture state exists here. A gesture that began on the
            // workspace owns the pointer until it ends, so its release must end
            // it wherever the maker's hand happens to be -- occluding the
            // release would strand `drag.active` true with the button up, and
            // the next motion would drag an object nobody was holding. The
            // other direction needs nothing at all: a press on a panel starts no
            // drag, so a release after one finds none and does nothing at all.
            // The absence of a drag IS the memory.
            //
            // THROUGH THE SAME OWNER AS EVERY OTHER MODE (WIND-2a), so there is one place
            // that knows what a button-1 release ends and three places that decide what to
            // SAY about it. No pane gesture can reach this branch today -- one is begun
            // only while management is open, which routes above -- and asking the owner
            // rather than a field is what keeps that a fact rather than an assumption.
            const GesturesEnded done = end_held_gestures();
            if (done.document) {
                say("released #" + std::to_string(done.document_id), false);
            }
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
            // THE OVERLAY HAS THE INPUT, and since TEXT-0 one motion matters inside it: a
            // selection drag the mode's own press began on its editable line. The geometry
            // is re-resolved from the CURRENT screen — the same two calls the press spent —
            // and the ROW is deliberately not re-tested: a drag owns the gesture until
            // release (`PaneGesture`'s law), so a hand that wanders off the line keeps
            // sweeping the line by column, which is what makes the selection stable rather
            // than flickering with the pointer's row (SC-form: stable across the region).
            if (session_.text_drag.active &&
                session_.text_drag.place == text_drag_place::kTerminalLine) {
                const Screen sc = screen_of(session_);
                const TerminalInputPlace place = terminal_input_place(sc);
                const ProseAt at =
                    prose_at(m.space, m.x, m.y, place.region_x, place.region_y, place.fit);
                if (at.understood) {
                    session_.terminal.input.drag_to_column(
                        terminal_value_column(place, at.column));
                    refresh_terminal();
                    repaint(mail);
                }
            }
            return;
        }
        // AND ARRANGEMENT OWNS MOTION WHILE IT IS OPEN, for the press's reason. A motion
        // with no pane gesture held does nothing at all: only a PRESS begins one, which is
        // the same sentence this handler already said about the document.
        if (session_.arrange.open) {
            const PointedAt here = canvas_point_of(m.space, m.x, m.y);
            if (!here.understood || !session_.pane_drag.active) {
                return;
            }
            arrange_motion(here.sub.x, here.sub.y, mail);
            repaint(mail);
            return;
        }
        // A SELECTION DRAG ON THE LIVE PROPERTY DRAFT (TEXT-0) — the Terminal branch's twin
        // on the ordinary path, before the document's drag for the same reason the press
        // chain asks the draft first: it is the narrower claim, and the two cannot both be
        // active (a press `info_press` consumed never reached `take_hold`).
        if (session_.text_drag.active &&
            session_.text_drag.place == text_drag_place::kPropertyDraft) {
            Row* row = editing_row();
            const InfoBodyAt where = info_body_at(state_, session_, m.space, m.x, m.y);
            if (row != nullptr && where.present) {
                row->drag_to_column(property_value_column(where.at.column));
                refresh_inspector();
                repaint(mail);
            }
            return;
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
        // ---- THE SECOND ANSWER, ANNOUNCED ON ITS OWN LATCH (BLD-1) ------------
        //
        // Realization settles AFTER the build it followed, so by the time it does,
        // `awaiting` has already been released and the build's own ending announced.
        // This is the other half of the same discipline: a panel that WATCHED a
        // realization begin may report how it ended, and a panel that merely learned
        // about one may not.
        //
        // ⚠ WHEN BOTH SETTLE IN ONE ARRIVAL, THE BUILD'S SENTENCE WINS. A failed build
        // refuses realization in the same breath, and there is one notice line: the
        // maker needs the CAUSE ("BUILD FAILED ... exit 1"), not the consequence
        // ("nothing was offered to the project"), which the panel's own rows carry
        // anyway. The latch is still released, so the derivative refusal is not
        // announced late as though it were news.
        const bool build_news = watching && !zengine::builder::still_going(said.outcome);
        const bool realization_settled =
            pane.awaiting_realization &&
            (said.realization == zengine::builder::realization::kRealized ||
             said.realization == zengine::builder::realization::kRefused);
        if (realization_settled) {
            pane.awaiting_realization = false;
            if (!build_news) {
                if (said.realization == zengine::builder::realization::kRealized) {
                    say("realized " + said.artifact + " -- " + said.realized_detail, false);
                } else {
                    say("NOT REALIZED: " + said.artifact + " -- " + said.realized_detail, true);
                }
                repaint(mail);
                return;
            }
        }
        if (!build_news) {
            repaint(mail);
            return;
        }
        switch (said.outcome) {
        case zengine::builder::outcome::kSucceeded:
            // TWO OUTCOMES, TWO SENTENCES, AND THE SECOND IS NOT SUPPRESSED BY THE
            // FIRST (BLD-1). A maker who asked for BUILD & REALIZE and got a green
            // build has learned half of what they asked about; announcing only that
            // half would be the same conflation the Builder's own two fields exist to
            // prevent. The realization half arrives later, in its own status, and is
            // announced by `on(BuildStatus)` reaching this switch again -- which it
            // does, because a realization answer republishes the tool's picture.
            say("built " + said.artifact + " -- exit 0", false);
            break;
        case zengine::builder::outcome::kFailed:
            say("BUILD FAILED: " + said.recipe + " -- exit " + std::to_string(said.status), true);
            break;
        case zengine::builder::outcome::kNoArtifact:
            say("the build succeeded and produced no `" + said.artifact + "`: " + said.detail,
                true);
            break;
        case zengine::builder::outcome::kNotStarted:
            say("the build never started: " + said.detail, true);
            break;
        case zengine::builder::outcome::kUnknownRecipe:
            say("the Builder refused: " + said.detail, true);
            break;
        default: break;
        }
        repaint(mail);
    }

    /// THE BUILDER TOOL SAID WHAT THIS PROJECT CAN BUILD (BLD-1).
    ///
    /// A SECOND PUBLICATION FROM THE SAME WEAVE, held under the same rule as the
    /// first: only while a panel is presenting it. It arrives once, in answer to the
    /// `StatusRequested` an opening Builder panel sends, and it is the entire reason
    /// this application can offer a maker a CHOICE of what to build without holding one
    /// recipe of its own.
    ///
    /// THE CHOICE IS CLAMPED HERE AND NOWHERE ELSE. A catalog that arrived shorter than
    /// the last one -- a second panel, a re-ask -- must not leave a selection pointing
    /// past its end, and clamping at the arrival is the one place that can be true for
    /// every later reader.
    void on(const zengine::builder::RecipeCatalog& said, loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return;
        }
        BuilderPane& pane = session_.panels.builder;
        pane.known = said;
        if (pane.chosen >= pane.known.recipes.size()) {
            pane.chosen = 0;
            // A CLAMPED SELECTION IS NOT THE MAKER'S ANY MORE (BLD-2): the row their
            // pick named is gone, and 0 is where the panel put them, not where they
            // went. The frontier action must not read it as an explicit choice.
            pane.picked = false;
        }
        repaint(mail);
    }

    // ---- THE EXTERNAL PANE SEAM: an office offers, Workshop grants, an office says (WP-0)
    //
    // TWO DOORS AND THEY ARE DIFFERENT DOORS. Discovery adds a row a maker may choose;
    // content fills a pane a maker has already opened. `PaneContent` never creates a
    // catalog row and `PaneOffered` never becomes a presentation by itself -- a provider
    // cannot put a pane on the screen, only into the list.
    //
    // BOTH ARE AUTHENTICATED BY THE SAME ONE FACT, and it is the phase's whole trust story:
    // `mail.authored_role()` is the office LOOM VERIFIED at the moment the sentence was
    // authored, carried as delivery provenance. It cannot be written by a payload, cannot be
    // chosen by a sender, and is EMPTY for personal speech -- including personal speech from
    // the very weave that currently holds the office. Holding an office is not speaking as
    // one (MSG-07), so a provider that reaches for `mail.send` instead of
    // `mail.as_role(...).send` registers nothing, and that is a refusal rather than a
    // leniency.
    //
    // WHAT IT DOES NOT PROVE, said here because the temptation to read more into it is the
    // failure mode WP-R0 was corrected for: a role is a LIVE, REPLACEMENT-STABLE SERVICE
    // ROUTE on this bus in this process. It is not a package author, not a signature, not a
    // publisher, and not evidence that the same author came back after a restart.

    /// AN OFFICE OFFERS A PANE. Admitted, refreshed, or refused -- and every one of those
    /// is bounded before a byte is retained.
    void on(const PaneOffered& offer, loom::Mail& mail) {
        // READ AS A VIEW AND KEPT AS ONE (WP-0a). The stamp belongs to the delivery
        // being handled and outlives every line below it; nothing here stores it, so
        // no view survives this handler. Making an owned string of it HERE would put
        // the copy before the law -- `admit_pane_offer` is the one place that decides
        // whether these bytes are a provider key at all, and it now gets them
        // unowned.
        const std::string_view office = mail.authored_role();
        if (office.empty()) {
            // PERSONAL SPEECH. Not an error to report to a maker -- an unauthenticated
            // message is not a fact about their arrangement -- and emphatically not a
            // catalog change. `mail.sender()` is deliberately not consulted: it is a
            // WeaveId, so a reloaded provider would be a different pane, and it is not the
            // durable route a saved setup names.
            return;
        }
        const Admission admitted = admit_pane_offer(session_.panels.runtime, office, offer);
        if (!admitted.written.accepted) {
            // THE REFUSAL IS WORKSHOP'S SENTENCE ABOUT ITS OWN LAW and interpolates no
            // field that failed one: `admit_pane_offer` names a `PaneRef` only after both
            // halves have passed `check_pane_key`, so no unvalidated byte reaches the
            // notice line a maker is reading.
            say(admitted.written.refusal, true);
            repaint(mail);
            return;
        }
        if (admitted.refreshed) {
            // A RE-OFFER IS A CORRECTION, AND AN OPEN PANE MUST NOT KEEP ANSWERING WITH THE
            // OLD ONE. The descriptor was updated in place; what this clears is the
            // PRESENTATION's copy, so the pane returns to waiting and the repaint below
            // grants the current room again. Nothing is closed and no catalog position
            // moves -- a provider correcting its own summary is not a reason for a maker's
            // panel to vanish.
            if (ExternalPane* pane = session_.panels.external_pane(admitted.kind)) {
                pane->shown.clear();
                pane->clear_refusal();
                pane->heard = false;
                pane->awaiting = true;
                pane->granted = false;
            }
        }
        // AND THE OFFER MAY RESOLVE AUTHORED INTENT THAT WAS WAITING FOR IT. This is the
        // one path -- the same `apply_setup` the picker and a restore go through -- so a
        // setup naming `third.party/hello` opens the moment that office offers it, without
        // the file having been touched and without a second way to open a panel existing.
        apply_setup(mail);
        repaint(mail);
    }

    /// AN OFFICE SAYS WHAT ITS PANE SAYS. Validated WHOLE against the room this pane was
    /// last granted, and only then copied.
    ///
    /// THE ORDER IS THE CONTRACT: authorship, then identity, then the room, then every row.
    /// Nothing is retained until all four have passed, so an update whose last row is one
    /// column too wide leaves not one of its earlier rows behind.
    ///
    /// WHAT THIS BOUND IS, EXACTLY. The Loom's decoder has already materialised the value
    /// by the time this handler runs -- this is an APPLICATION RETENTION bound and not a
    /// decode-memory bound, and describing it as the latter would be claiming a Loom
    /// property this phase did not build.
    void on(const PaneContent& content, loom::Mail& mail) {
        const std::string_view office = mail.authored_role();
        if (office.empty()) {
            return; // personal speech: no cache, no notice, no catalog change
        }
        // IDENTITY IS ASKED OF WHAT WAS ALREADY ADMITTED, WITH VIEWS (WP-0a). The pair
        // is compared against rows this session accepted under `check_pane_key`, so
        // the question is answered without owning either half and without building a
        // `PaneRef` out of an office no law here has judged. THIS IS NOT A SECOND
        // ADMISSION LAW: a pair that matches no row returns nothing and retains
        // nothing, which is the same answer the built-in-first `resolve_pane` gave --
        // admission refuses a runtime offer that would shadow a built-in, so no
        // built-in reference can be a row here to find.
        const RuntimePane* row = session_.panels.runtime.find(office, content.pane);
        if (row == nullptr) {
            return; // an office speaking about a pane it never offered, or about a built-in
        }
        // THE HANDLE, TAKEN NOW. Nothing holds a pointer into `entries` (panel.hpp),
        // and the row is looked up again by handle at the moment a notice needs it.
        const std::int64_t kind = row->kind;
        ExternalPane* pane = session_.panels.external_pane(kind);
        if (pane == nullptr || !pane->granted) {
            // CONTENT FOR A CLOSED PANE, OR FOR ONE THAT HAS NOT BEEN GRANTED A ROOM YET.
            // Nothing is cached and nothing is opened: a provider cannot make a panel appear
            // by talking about it, which is what keeps discovery and presentation two doors.
            return;
        }
        const Written judged = judge_content(content, *pane);
        if (!judged.accepted) {
            // THE OLD ROWS GO WITH THE REFUSAL. Leaving them would present a previous
            // answer as the current one at the exact moment this pane knows it is not --
            // the `awaiting` distinction the Builder panel established, one provider out.
            pane->shown.clear();
            pane->heard = false;
            pane->awaiting = true;
            pane->refusal = kExternalRefused;
            // AND WHY, KEPT WHERE THE REFUSAL IS. This is Workshop's own sentence
            // about the content and carries none of the refused message; what changed in
            // what changed is where it LIVES. It used to be said on the notice row, which has no
            // lifetime: the pane cleared its refusal on the next valid content and the row
            // kept the refusal sentence for the rest of the process -- resolving a
            // condition could not un-say it. Held beside the refusal it explains, it is
            // gone the moment the refusal is, and the attention projection reads it where
            // it lives rather than being told about it (`attention_conditions`).
            pane->refusal_why = judged.refusal;
            repaint(mail);
            return;
        }
        pane->shown = content.rows; // only the validated rows, and only now
        pane->heard = true;
        pane->awaiting = false;
        pane->clear_refusal();
        repaint(mail);
    }

    /// IS THIS UPDATE INSIDE THE ROOM THIS PANE WAS GRANTED, and is every row of it
    /// something a canvas can carry?
    ///
    /// PURE, AND JUDGED BEFORE ANYTHING IS COPIED. Three rules and no more: the rows fit the
    /// granted count, each row's text fits the granted columns, and each row's text obeys
    /// `SurfaceTextRow`'s existing plain-ASCII contract. The last is not a new text policy --
    /// the cell projection is one cell per BYTE, so a multi-byte sequence is split there and
    /// a control byte would move a terminal's cursor out of the row it was given. Every
    /// first-party publisher already honours it; a provider is the first publisher this
    /// application did not write.
    ///
    /// TRUNCATION IS NOT AN OPTION HERE. A pane showing the first eight rows of a
    /// twelve-row answer, unmarked, presents a partial sentence as the provider's whole one
    /// -- the failure `detail::fit` exists to prevent one row at a time. So the update is
    /// refused as a unit and the pane says so.
    ///
    /// THE ROLE AND THE GROUND ARE NOT JUDGED, deliberately. They are SEMANTIC Surface
    /// values and the Surface package already answers for an unknown one (`ink_for_role`'s
    /// fallback); re-deciding that here would be a second palette policy and the beginning
    /// of a provider theme.
    static Written judge_content(const PaneContent& content, const ExternalPane& pane) {
        if (static_cast<std::int64_t>(content.rows.size()) > pane.rows) {
            return Written::no("sent " + std::to_string(content.rows.size()) +
                               " rows into a pane granted " + std::to_string(pane.rows));
        }
        for (const surface::SurfaceTextRow& row : content.rows) {
            if (static_cast<std::int64_t>(row.text.size()) > pane.columns) {
                return Written::no("sent a row of " + std::to_string(row.text.size()) +
                                   " bytes into a pane granted " +
                                   std::to_string(pane.columns) + " columns");
            }
            for (const char c : row.text) {
                const unsigned char byte = static_cast<unsigned char>(c);
                if (byte < 0x20u || byte >= 0x7Fu) {
                    return Written::no("sent a row carrying a byte a canvas cannot draw");
                }
            }
        }
        return Written::ok();
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

    /// The effective spelling of one action, for this weave's own notices -- the same
    /// `hotkey_text` every screen surface spends, so a hint in the notice line and the
    /// band cannot spell one binding two ways.
    std::string hotkey(Act a) const { return hotkey_text(session_.keymap, a); }

    Row* editing_row() {
        for (Row& r : session_.rows) {
            if (r.editing()) {
                return &r;
            }
        }
        return nullptr;
    }

    // WHERE `editable_text_has_keyboard()` AND THE OLD `paste_owner_now()` CHAIN USED TO
    // BE (TEXT-0, QR-11 -> KEY-0): both were hand-kept mirrors of the routing chain, each
    // annotated "MIRRORS THE CHAIN branch for branch so the two cannot disagree" -- the
    // discipline was real and it was still a discipline, held in three places by hand.
    // `keyboard_context(session_)` (screen.hpp) is the chain now, and both questions are
    // one-line derivations of its answer: `context_takes_text(ctx)` is the `^c` gate
    // (carried by `workshop.quit`'s kNoText declaration context), and the paste owner
    // below reads the same value. Per branch the old answers are unchanged, arm for arm.

    /// Which of this weave's own editable places a consumed paste request came from
    /// (QR-11). `kNone` for every armless branch — a focused runtime pane's paste is the
    /// provider's own conversation with the Skin, and the gesture branches that hold no
    /// box cannot have bumped the counter this answers about.
    enum class PasteOwner : std::uint8_t { kNone, kTerminal, kNaming, kDraft };

    /// WHICH DRAFT WOULD THE CHAIN HAVE HANDED THE CLIPBOARD TO? — `paste_requests`
    /// bumped, so one of the box-holding branches ran; since KEY-0 this is a projection of
    /// the one resolved context rather than a second spelling of the routing, which closes
    /// the way two spellings could deliver a paste to a draft the keys never reached.
    PasteOwner paste_owner_now() {
        switch (keyboard_context(session_)) {
        case KeyContext::kTerminal: return PasteOwner::kTerminal;
        case KeyContext::kNaming: return PasteOwner::kNaming;
        case KeyContext::kDraft: return PasteOwner::kDraft;
        default: return PasteOwner::kNone;
        }
    }

    /// ONE PASTE STILL IN FLIGHT: the conversation (by the book's own id) and the draft it
    /// belongs to. The owner names the box; `epoch` says WHICH draft that box was holding
    /// (`TextBox::draft_epoch` — set/clear bump it, so a submitted line, a cancelled
    /// draft and a reopened editor all read as a different draft); `object`/`label`
    /// identify a property row, whose box is one of many and is rebuilt freely (a carried
    /// draft rides `Row::resume` with its epoch, so an extent change mid-flight does not
    /// orphan the paste).
    struct PendingPaste {
        std::uint64_t ask = 0;
        PasteOwner owner = PasteOwner::kNone;
        std::uint64_t epoch = 0;
        std::int64_t object = 0;
        std::string label;
    };

    /// OPEN THE CLIPBOARD CONVERSATION A CONSUMED PASTE REQUEST ASKED FOR (QR-11). The
    /// ask goes to the Skin's ROLE — the Medium owns the platform clipboard in both
    /// directions — and the book is the asker's own record (`loom::AskBook`): at capacity
    /// the NEW paste is refused and every outstanding one is untouched, the asker's half
    /// of the settlement law. A refused open drops this paste silently; the book's
    /// capacity is real pastes in flight, which one bus turn settles, so reaching it takes
    /// a Skin that never answers.
    void begin_clipboard_paste(loom::Mail& mail) {
        PendingPaste p;
        p.owner = paste_owner_now();
        switch (p.owner) {
        case PasteOwner::kNone: return; // no box of this weave's asked; nothing to do
        case PasteOwner::kTerminal: p.epoch = session_.terminal.input.draft_epoch(); break;
        case PasteOwner::kNaming: p.epoch = session_.setup.naming.line.draft_epoch(); break;
        case PasteOwner::kDraft: {
            Row* row = editing_row();
            if (row == nullptr) {
                return; // unreachable while the mirror holds; written anyway
            }
            p.epoch = row->editor().draft_epoch();
            p.object = session_.selected;
            p.label = row->label();
            break;
        }
        }
        const loom::AskOpened opened = paste_asks_.open_to_role(
            zengine::surface::kSkinRole, zengine::surface::ClipboardTextRequested::zen_name,
            zengine::surface::ClipboardTextRequested::zen_version);
        if (!opened) {
            return; // the book is full: this paste is dropped, the outstanding ones stand
        }
        p.ask = opened.id;
        pending_pastes_.push_back(std::move(p));
        (void)mail.send_to_role(zengine::surface::kSkinRole,
                                zengine::surface::ClipboardTextRequested{}, opened.correlation);
    }

    /// The pending-paste record a settled conversation belongs to, removed from the list
    /// and handed back by value. An id the list does not hold answers the empty record
    /// (`owner == kNone`), which every consumer already treats as "nothing to do".
    PendingPaste take_pending_paste(std::uint64_t ask) {
        for (std::size_t i = 0; i < pending_pastes_.size(); ++i) {
            if (pending_pastes_[i].ask == ask) {
                PendingPaste p = std::move(pending_pastes_[i]);
                pending_pastes_.erase(pending_pastes_.begin() +
                                      static_cast<std::ptrdiff_t>(i));
                return p;
            }
        }
        return PendingPaste{};
    }

    /// Editing mode, KEY half: the three keys that are editor CONTROLS rather
    /// than text. Commit, cancel, erase -- meanings that belong to Workshop and
    /// that Input deliberately does not know. Everything else a key press might
    /// have meant arrives as TextEntered instead, including `q`, which types a q
    /// here and is the whole reason Ctrl+C is handled above this branch.
    void editing_key(const zengine::input::KeyPressed& k) {
        Row* row = editing_row();
        // THE DRAFT'S OWN VOCABULARY FIRST (TEXT-0). One call owns what four switches used
        // to spell separately — the six editing keys, and now selection, clipboard, word
        // movement and history behind them — and a `true` is QR-2's bool: the gesture
        // reached the layer that owns what it means, whether or not anything changed. The
        // component's vocabulary outranks the application keymap INSIDE a text context,
        // deliberately (owner-first refusal): a maker who remaps a draft control onto an
        // editing chord has authored a binding the box will answer first, and the hotkey
        // view shows both rows. What is left below is exactly the policy: what a draft
        // MEANS when a maker commits or abandons it, which the component is deliberately
        // unable to know -- resolved through the keymap since KEY-0, executed here as
        // ever.
        if (row->consume(k.scancode, k.modifiers, session_.clipboard)) {
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kDraft, k.scancode, k.modifiers)) {
        case Act::kDraftCommit: {
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
        case Act::kDraftCancel:
            row->cancel();
            say("edit cancelled -- nothing was written", false);
            break;
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
        const PanelBounds info = bounds_of(session_.panels, session_.setup.active, panel::kInfo, sc);
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
            // ...AND THE PRESS OPENS A SELECTION DRAG (TEXT-0). The press placed the caret,
            // which is the anchor; every motion until release extends from it. The record
            // holds WHICH line and nothing else — the geometry is re-resolved per motion by
            // the same functions this press just spent, `PaneGesture`'s no-live-position law.
            session_.text_drag.active = true;
            session_.text_drag.place = text_drag_place::kPropertyDraft;
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
            say(finish_draft_first(), true);
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
            say(finish_draft_first(), true);
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
            say("terminal closed -- " + hotkey(Act::kTerminalToggle) + " reopens it", false);
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
        // THE LINE'S OWN VOCABULARY FIRST (TEXT-0): the six editing keys this switch used
        // to spell, and selection, clipboard, word movement and history behind them, all
        // through the one component call every consumer now makes. A consumed gesture
        // still reaches `refresh_terminal`, for the reason the caret keys always fell
        // through rather than `return`ing the way Up/Down do: an edit or a caret move
        // changes whether the caret is AT THE END, which is the question the completer is
        // allowed to be asked.
        if (pane.input.consume(k.scancode, k.modifiers, session_.clipboard)) {
            refresh_terminal();
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kTerminal, k.scancode, k.modifiers)) {
        case Act::kTerminalSubmit: submit_terminal_line(); break;
        case Act::kTerminalBack:
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
        case Act::kTerminalUp: move_completion(-1); return;   // the line did not change
        case Act::kTerminalDown: move_completion(+1); return; // ...so nothing is recomputed
        case Act::kTerminalComplete:
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
            const bool had_selection = pane.input.has_selection();
            // THROUGH THE WINDOW THE ROW WAS DRAWN WITH (HD-4). A visible column names
            // `first_visible + offset` of the WHOLE authored line, never the offset alone --
            // that is the one subtraction a horizontal viewport adds to a hit test, and
            // leaving it out is right for exactly as long as no line is long enough to
            // scroll. The offset read here is the one the last repaint resolved, which is
            // the one the maker is looking at.
            pane.input.place(terminal_caret_of_column(place, pane.input, at.column));
            // ...AND THE PRESS OPENS A SELECTION DRAG (TEXT-0), `info_press`'s twin: the
            // caret just placed is the anchor, and motion until release extends from it.
            session_.text_drag.active = true;
            session_.text_drag.place = text_drag_place::kTerminalLine;
            // The caret moving is what changes whether completion may be asked, so a press
            // that moved it has to reach `refresh_terminal` exactly as a caret key does. A
            // press that COLLAPSED a selection changed the picture too, even where the
            // caret stood still — the highlight has to leave the screen (TEXT-0).
            if (pane.input.caret() != was || had_selection) {
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
        // EVERY ARM CALLS THE OPERATION IT ALWAYS CALLED; what changed in KEY-0 is only
        // how a gesture becomes an action. Exact matching split the old `shift ?` pairs
        // into declared siblings (`object.left`/`object.narrower`,
        // `builder.build`/`builder.build-realize`) -- one gesture family spelled two ways
        // remains the design (hjkl's own decision), each half its own remappable row now,
        // and the accidental subset aliases the old per-site tests produced (Ctrl+N
        // created; Alt+Q quit) are gone on purpose.
        switch (session_.keymap.action_for(KeyContext::kCommand, k.scancode, k.modifiers)) {
        case Act::kObjectNext: select_next(); break;
        case Act::kInfoUp: move_cursor(-1); break;
        case Act::kInfoDown: move_cursor(+1); break;
        case Act::kInfoEdit: begin_edit(); break;
        case Act::kObjectNew: create_object(); break;
        case Act::kObjectDelete: delete_object(); break;
        case Act::kObjectLeft: move_by(-1, 0); break;
        case Act::kObjectDown: move_by(0, +1); break;
        case Act::kObjectUp: move_by(0, -1); break;
        case Act::kObjectRight: move_by(+1, 0); break;
        case Act::kObjectNarrower: size_by(-1, 0); break;
        case Act::kObjectTaller: size_by(0, +1); break;
        case Act::kObjectShorter: size_by(0, -1); break;
        case Act::kObjectWider: size_by(+1, 0); break;
        case Act::kWorkspaceNarrower: resize_workspace(-4); break;
        case Act::kWorkspaceWider: resize_workspace(+4); break;
        case Act::kPicker: open_picker(); break;
        // BUILDING AND REALIZING stay two deliberate halves (BLD-1): realizing an
        // artifact is the one Builder gesture that changes what is running, and its
        // default is the chorded sibling of the plain build's.
        case Act::kBuild: build_now(mail, false); break;
        case Act::kBuildRealize: build_now(mail, true); break;
        case Act::kRecipeNext: choose_recipe(+1, mail); break;
        case Act::kRecipeBack: choose_recipe(-1, mail); break;
        case Act::kBuildFrontier: build_frontier(mail); break;
        // THE TWO SETUP GESTURES (WS-0): ordinary maker commands beside `+ panel`,
        // deliberately not another `^`-pair beside the document's.
        case Act::kSetupName: open_setup_name(); break;
        case Act::kSetupRestore: restore_setup(mail); break;
        // ARRANGE THE DESK (WIND-2's mode, rescoped by ARR-0): a printable trigger pays
        // the swallow rule -- armed centrally from the binding since KEY-0 -- and buys a
        // mode whose own keys need no modifier at all (P48).
        case Act::kArrangeDesk: open_arrange_desk(); break;
        // WHAT CAN I DO WITH THIS? -- the contextual-action surface, on the subject
        // command mode can truthfully name (CTX-0).
        case Act::kContextOpen: open_context_ambient(); break;
        // PANE TITLES ARE A PRESENTATION PREFERENCE WITH A KEY (WUX-1). The flip is one
        // session bit; everything it changes on screen -- the arrangeable panes' header
        // rows, the row returned to or taken back from each provider's budget -- follows
        // from the ordinary repaint this keystroke already earns (`refresh_external_rooms`
        // re-grants exactly the rooms whose capacity moved). The notice says which state
        // the toggle landed in, because a maker with no external pane open would otherwise
        // watch nothing change; its second half names the one exception, which is the
        // keyboard-identity law, not a courtesy.
        //
        // AND SINCE WUX-3 THE PREFERENCE IS DURABLE: a toggle is the maker STATING it, so
        // this is the moment it is written -- to the prefs file, whose ordinary home is
        // the per-user configuration root, so the choice follows the maker across launch
        // directories rather than living exactly as long as the run. Three quiet walls:
        // no path chosen means live-only (which is `--isolated`'s promise); a prefs file
        // that stands refused is never overwritten (`prefs_bad_` -- the do-not-rewrite
        // law); and a failed write says so on the same notice, because a preference a
        // maker believes saved and is not is the quiet wrong answer.
        case Act::kPaneTitles: {
            load_prefs(); // a toggle before any surface exists still toggles the truth
            session_.pane_titles = !session_.pane_titles;
            std::string note =
                session_.pane_titles
                    ? "pane titles shown"
                    : "pane titles hidden -- a pane holding the keyboard still shows its own";
            bool bad = false;
            if (!host_->prefs_path.empty()) {
                if (prefs_bad_) {
                    note += "; not saved: " + host_->prefs_path +
                            " could not be read and will not be overwritten";
                    bad = true;
                } else {
                    const Written wrote =
                        prefs_persist::save_file(host_->prefs_path, session_.pane_titles);
                    if (!wrote.accepted) {
                        note += "; not saved: " + wrote.refusal;
                        bad = true;
                    }
                }
            }
            say(note, bad);
            break;
        }
        case Act::kQuit: quit(); break;
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

    /// THE PICKER'S POPULATION — the shared recovery inventory, and there is exactly one of
    /// it (WIND-2a).
    ///
    /// WIND-2 widened the picker's PAINTER to the union of the catalog and everything the
    /// setup names, which is what finally gave an unresolved pane a row. It left the cursor
    /// bound and the Return action reading `combined_catalog`, which is a fact about the
    /// BUILD -- so the rows past the catalog were painted and could not be reached, and a
    /// maker with an unresolved reference in their setup could see the row the phase had
    /// added for them, read its state word, and not remove it without editing the file by
    /// hand. Three owners of one population is how a list comes to disagree with itself
    /// about which index means what; this is the one owner.
    std::vector<CatalogRow> picker_population() const {
        return inventory_rows(session_.setup.active, session_.panels);
    }

    /// Open the `+ panel` picker.
    void open_picker() {
        session_.panels.picker.open = true;
        session_.panels.picker.cursor = 0;
        say("+ panel -- " + hotkey(Act::kPickerUp) + "/" + hotkey(Act::kPickerDown) +
                " chooses, " + hotkey(Act::kPickerChoose) + " opens or removes, " +
                hotkey(Act::kPickerClose) + " or " + hotkey(Act::kPicker) + " cancels",
            false);
    }

    /// The picker's keys. Escape and `p` both close it: the key that opened it
    /// closes it, the terminal overlay's rule, and Escape closes it too because
    /// a maker who has changed their mind should not have to remember which of
    /// the two ways out this particular thing has.
    void picker_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        PanelPicker& picker = session_.panels.picker;
        // THE CURSOR IS REPAIRED THROUGH THE POPULATION'S OWN OWNER, BEFORE ANYTHING INDEXES
        // IT (WIND-2a). The list can shrink under an open picker -- a provider going away
        // takes its runtime rows with it -- and every question below is about a row.
        const std::size_t population = picker_population().size();
        if (picker.cursor >= population) {
            picker.cursor = population == 0 ? 0 : population - 1;
        }
        switch (session_.keymap.action_for(KeyContext::kPicker, k.scancode, k.modifiers)) {
        case Act::kPickerUp:
            if (picker.cursor > 0) {
                --picker.cursor;
            }
            break;
        case Act::kPickerDown:
            // THE BOUND IS THE PAINTED POPULATION (WIND-2a), which since WIND-2 is the
            // shared inventory rather than what this build could present. `kPanelKinds` was
            // the whole list until an office could offer one, and the combined catalog was
            // the whole list until the setup could name something neither half knew.
            if (picker.cursor + 1 < picker_population().size()) {
                ++picker.cursor;
            }
            break;
        case Act::kPickerChoose: choose_panel(mail); break;
        case Act::kPickerClose:
            picker.open = false;
            say("no panel opened or removed", false);
            break;
        default:
            // THE KEY THAT OPENED IT CLOSES IT -- the terminal overlay's rule, and since
            // KEY-0 it follows the OPENER'S effective binding wherever the maker moved
            // it: dispatch consults the same truth the row-0 hint spells, so `p` closes
            // exactly while `p` opens.
            if (session_.keymap.matches(Act::kPicker, k.scancode, k.modifiers)) {
                picker.open = false;
                say("no panel opened or removed", false);
            }
            break;
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
        const std::vector<CatalogRow> rows = picker_population();
        picker.open = false;
        if (picker.cursor >= rows.size()) {
            // THE BELT, NOT THE DOOR. The cursor is bounded where it moves -- through the
            // same owner this list came from -- and a population that shrank under an open
            // picker (a provider going away) is the one way it can be past the end.
            return;
        }
        const CatalogRow chosen = rows[picker.cursor];
        const PaneRef ref = chosen.ref;
        if (remove_pane(session_.setup.active, ref)) {
            // A REMOVAL WORKS ON A WAITING ROW EXACTLY AS ON AN OPEN ONE (WP-0). The maker
            // authored the intent; whether this screen currently has room to seat it is
            // Workshop's problem and not a reason to make the intent unremovable.
            apply_setup(mail);
            // WHAT IT WAS PRESENTING IS UNTOUCHED, and one sentence covers both
            // kinds because it is the same sentence: the Builder tool keeps its
            // target, its history and its running count of asks; the document
            // keeps every object, the selection and the inspector's rows. A
            // panel is a presentation, and removing one removes a presentation.
            say("removed " + chosen.name +
                    " -- " + hotkey(Act::kPicker) + " brings it back; nothing behind it was touched",
                false);
            return;
        }
        // A NEW ROW IS REFUSED BEFORE THE SETUP MOVES IF IT COULD NOT BE SEATED (WP-0).
        // The order is the whole of the guarantee: the capacity question is asked against
        // the setup this gesture WOULD produce, and the active setup is left untouched when
        // the answer is no. Adding first and letting `reconcile` drop it into `waiting`
        // would leave a maker with an authored pane they never saw and did not knowingly
        // author, which is a picker that edits a file behind its own refusal.
        Setup candidate = session_.setup.active;
        (void)add_pane(candidate, ref);
        const Seating trial = seat_panes(candidate, session_.panels.runtime,
                                         stack_capacity(screen_of(session_)));
        for (const std::int64_t k : trial.waiting) {
            if (k == chosen.kind) {
                say("no room for " + chosen.name +
                        " on this screen -- make the window taller, then p again",
                    true);
                return;
            }
        }
        session_.setup.active = std::move(candidate);
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
        say(std::string("opened ") + chosen.name + " -- " + hotkey(Act::kPicker) +
                " removes it",
            false);
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
    /// SINCE WP-0 IT ALSO CARRIES THE SCREEN'S CURRENT CAPACITY, and it is the only
    /// place that number is spent. `stack_capacity(screen_of(session_))` resolves it
    /// from the same `placement_bounds` the painter and the pointer use, so a panel
    /// cannot enter `Panels::open` unless the rectangle it would be drawn in fits above
    /// the setup line. What does not fit is retained as authored intent and named --
    /// never deleted, never remapped, and never given a placeholder.
    ///
    /// AND A NEWLY OPENED EXTERNAL PANE ASKS FOR NOTHING HERE. Its room is resolved on
    /// the repaint path (`refresh_external_rooms`), because the room is a fact about the
    /// screen this frame and not about the moment the panel opened -- and resolving it
    /// twice, once here and once there, is exactly the two-measurers defect HD-1 named.
    void apply_setup(loom::Mail& mail) {
        // MEMBERSHIP-DEPENDENT SESSION STATE FIRST (WIND-2a). This is the one door a setup's
        // membership changes through -- the picker's removal, a restore, a geometry edit
        // that reseats -- so it is the one place that has to notice a selection whose pane
        // is no longer named. Doing it here rather than at each caller is what keeps a
        // fourth caller from being the one that forgets.
        forget_removed_selection();
        const Reconciled done = reconcile(session_.panels, session_.setup.active,
                                          stack_capacity(screen_of(session_)));
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
        // The trigger's own character is swallowed centrally since KEY-0: `on(KeyPressed)`
        // arms the expectation from the consumed binding, whatever gesture the maker
        // authored for `setup.name`, so this door no longer hard-codes an `s`.
        say("name this setup -- " + hotkey(Act::kNamingCommit) + " saves it, " +
                hotkey(Act::kNamingCancel) + " cancels",
            false);
    }

    /// The name editor's keys. Return commits and saves; Escape cancels and
    /// changes nothing; the rest is the ordinary editing of one line, through
    /// the component that owns the text, the caret and the window together.
    void naming_key(const zengine::input::KeyPressed& k, loom::Mail&) {
        SetupNaming& naming = session_.setup.naming;
        // The line's own vocabulary first (TEXT-0) — the third of the four switches the
        // component call collapsed. What stays is the policy pair every consumer keeps to
        // itself: what a committed name MEANS and what abandoning one leaves standing.
        if (naming.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kNaming, k.scancode, k.modifiers)) {
        case Act::kNamingCommit: commit_setup_name(); break;
        case Act::kNamingCancel:
            naming.open = false;
            naming.line.clear();
            say("the setup name is unchanged", false);
            break;
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
            say(legal.refusal + " -- " + hotkey(Act::kNamingCommit) + " tries again, " +
                hotkey(Act::kNamingCancel) + " cancels",
            true);
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
    std::string unresolved_note(const Setup& s) const {
        const std::vector<PaneRef> waiting = unresolved_panes(s, session_.panels.runtime);
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

    // ---- THE LAST SESSION: the desk that comes back on its own (WUX-0) --------
    //
    // TWO FUNCTIONS AND NO GESTURE, and that is the whole of what makes this a different
    // promise from the one above it. Everything else in this section happens because a maker
    // ASKED -- `s` names a desk and writes it, `r` reads one back. These two happen because
    // Workshop started and because Workshop is leaving, and there is no key for either.
    //
    // NEITHER TOUCHES `setup_path`, IN EITHER DIRECTION, which is the property that keeps an
    // automatic save from eating an explicit one. A restored session becomes the ACTIVE
    // setup and nothing else: `on_file` is deliberately left alone, because it is this run's
    // copy of what is in the SETUP file and this run has not read that file. So a restored
    // session still says UNSAVED, meaning exactly what it has always meant here -- "this
    // arrangement has not been written to the setup file" -- and `s` still writes that file,
    // `r` still reads it, and neither has learnt anything about the other.

    /// TAKE BACK THE DESK AND THE ROOM THIS WORKSHOP WAS LAST USED IN.
    ///
    /// A TRANSACTION FOR THE SAME REASON `restore_setup` IS ONE: `session_persist::load_file`
    /// hands back a candidate rather than writing into anything, so a bad field near the end
    /// of the file cannot close a panel before it is met. And a session that cannot be read
    /// costs the desk and nothing else -- the default setup is already live, the document is
    /// untouched, and the file is left exactly as it is. Workshop does not rewrite a file it
    /// could not understand.
    void restore_last_session(loom::Mail& mail) {
        if (restored_) {
            return;
        }
        // ONCE PER PROCESS, and the guard is HERE rather than at the caller because
        // `SurfaceReady` is not a once-per-process fact: a Skin replacement announces itself
        // again, and a maker whose afternoon of arranging was silently thrown back to a file
        // written last night would have met a continuity feature that loses work.
        restored_ = true;
        if (host_->session_path.empty()) {
            return; // no session file was chosen: restore nothing, and say nothing about it
        }
        const session_persist::LoadedSession last =
            session_persist::load_file(host_->session_path);
        if (!last.present) {
            // A FIRST LAUNCH IS NOT AN ERROR and must never be reported as one. It is also
            // the most common way this function ends, so it ends quietly.
            return;
        }
        if (!last.outcome.accepted) {
            say(last.outcome.refusal + " -- opening with the default setup", true);
            return;
        }
        // ---- THE VIEWPORT FIRST, AND THE ORDER IS THE WHOLE OF IT ------------
        //
        // `apply_setup` seats panes against `stack_capacity(screen_of(session_))`, so how
        // much of this desk can be PRESENTED at all is decided by how much room the screen
        // has. Reconciling first and resizing afterwards would seat the desk against a
        // viewport nobody asked for and leave whatever did not fit waiting for room that had
        // in fact been there the whole time.
        if (last.honoured && adopt_screen(session_, last.viewport_w, last.viewport_h,
                                          session_.text_advance_px, session_.text_line_px)) {
            // The restored viewport IS the normal window's room -- the save wrote it from
            // exactly that (WUX-3) -- so the remembered pair starts equal to it rather
            // than waiting for the first extent to arrive.
            session_.normal_w = session_.screen_w;
            session_.normal_h = session_.screen_h;
            // The resolved inspector row closes over the workspace extent, and the workspace
            // extent is exactly what just changed -- `on(SurfaceExtent)`'s reason, said at
            // startup.
            refocus_keeping_draft(state_, session_);
        }
        // ---- THE DESKTOP PLACEMENT, REMEMBERED AND OFFERED BACK (WUX-3) ------------
        //
        // Remembered FIRST -- into the session, so the next save carries it whether or not
        // any medium ever acts on it (a terminal run retains a graphical run's placement
        // rather than erasing it) -- and then OFFERED to whichever medium holds the
        // surface. The offer is a want, not an instruction: the medium can see the
        // displays that exist now and Workshop cannot, so the judgment (restore verbatim,
        // adapt a stranded position, refuse to move blind) is entirely the medium's
        // (`surface::SurfacePlacementRemembered`; the law is `placement_within`). What the
        // medium then reports back through the ordinary placement channel is the truth
        // this session remembers next.
        if (last.placement.known) {
            session_.placement_known = true;
            session_.place_x = last.placement.x;
            session_.place_y = last.placement.y;
            session_.place_maximized = last.placement.maximized;
            mail.send_to_role(zengine::surface::kSkinRole,
                              zengine::surface::SurfacePlacementRemembered{
                                  last.placement.x, last.placement.y,
                                  last.placement.maximized});
        }
        // ---- ...AND THEN THE DESK, INTO THE ROOM IT ASKED FOR -----------------
        session_.setup.active = last.desk;
        apply_setup(mail);
        // AND IT SAYS NOTHING ABOUT UNRESOLVED PANES, WHICH `restore_setup` DOES SAY.
        //
        // MEASURED, ON A REAL WINDOW: at this instant no provider has had a turn. Workshop
        // published `PaneCatalogRequested` a few lines ago and the answers are still in the
        // queue, so EVERY external reference in a restored desk is unresolved right now and
        // resolved a moment later -- the count is a fact about the clock rather than about
        // the desk, and a maker reads it after it has stopped being true. Measured: a desk of
        // three external panes reported all three unresolved, BY NAME, over three panes that
        // were on the screen while the sentence was being read.
        //
        // IT IS NOT A LOST DIAGNOSTIC. The setup line carries the same count LIVE and
        // recomputes it every paint (`setup "..." UNSAVED [| N unresolved]`), and the picker
        // gives an unresolved reference a row of its own. A reference that is genuinely gone
        // is therefore still named, by a surface that is still right an hour later. `r` keeps
        // its note, because a maker who presses it is asking a question at a moment when the
        // catalog has long since been answered.
        std::string said = "reopened your last desk " + quoted_setup_name(last.desk.name) +
                           " -- " + std::to_string(session_.screen_w) + "x" +
                           std::to_string(session_.screen_h) + " cells";
        if (!last.declined.empty()) {
            // AND IT NEVER CLAIMS THE SIZE CAME BACK WHEN IT DID NOT. The desk did; the
            // window did not; a maker is told which, with the value that was declined.
            said += "; " + last.declined;
        }
        say(said, !last.declined.empty());
        // THE SECOND PICTURE OF THE RUN, and the one that asks for the room -- see
        // `on(SurfaceReady)` for why it cannot be the first.
        repaint(mail);
    }

    /// WRITE DOWN THE DESK AND THE ROOM, ON THE WAY OUT.
    ///
    /// WHAT IT SAVES IS AUTHORED WORKSPACE STATE AND NOTHING ELSE: which panes the maker
    /// meant to have, where they put them, how big they made them, which is in front, and
    /// how much room the surface had. No runtime pane, no WeaveId, no catalog row, no loaded
    /// artifact, no bus state, no selection, no half-finished drag and no pane's private
    /// contents. "The last session" is not a snapshot of a running universe; it is the two
    /// facts a maker would otherwise have to reconstruct by hand.
    void save_last_session() {
        if (host_->session_path.empty()) {
            return;
        }
        // THE VIEWPORT WRITTEN IS THE NORMAL WINDOW'S (WUX-3): `normal_w/h` tracks the
        // screen except while this run's medium says the window is maximized, so a
        // maximized close remembers the room a maker actually chose, with the maximized
        // state beside it rather than baked into it. The placement rides along exactly as
        // the last medium reported it -- or exactly as the file already carried it, on a
        // run whose medium had no desktop to report.
        session_persist::Placement place;
        place.known = session_.placement_known;
        place.x = session_.place_x;
        place.y = session_.place_y;
        place.maximized = session_.place_maximized;
        const Written written =
            session_persist::save_file(host_->session_path, session_.setup.active,
                                       session_.normal_w, session_.normal_h, place);
        if (written.accepted) {
            return;
        }
        // WHERE A FAILURE GOES WHEN THE SCREEN IS THE THING LEAVING. The notice is set
        // because nothing here may fail silently and because a suite reads it; stderr is
        // written because a maker will never see that notice -- this runs on the way out,
        // and a complaint about the session, delivered to a surface that is being torn down,
        // is not a complaint. The same argument the SDL medium makes for `complain`.
        say(written.refusal, true);
        std::fprintf(stderr, "zengine-workshop: %s\n", written.refusal.c_str());
        std::fflush(stderr);
    }

    // ---- PANE MANAGEMENT: arrange the windows, and never lose one (WIND-2) ----
    //
    // ONE MODE, FOUR STEPS, AND EVERY GESTURE ENDS AT A SETUP DOOR. The keyboard and the
    // pointer converge on `author_pane_window` (the gesture door, WUX-2a), the four
    // ordering operations and the three resets (setup.hpp) -- there is no second
    // arithmetic and no second refusal, which is the `nudge`/`drag_to` pattern this file
    // has used for a document object since W-2, said about a pane. The door owns the
    // settlement law -- independent axes settle independently; an anchored position+extent
    // pair settles atomically within its axis -- so every gesture inherits it and none may
    // restate it.
    //
    // EDITS COMMIT IMMEDIATELY AND ESCAPE IS NOT A ROLLBACK. Every existing immediate-commit
    // gesture in this application (`nudge`, `grow`, `drag_to`) is reversible only by
    // performing the inverse, and a mode with an Escape key will read as "cancel" to a maker
    // who has not been told otherwise -- so the help says `esc back`, never `esc cancels`,
    // and there is no undo here. Adding one would be an undo for the whole application
    // arriving as a side effect of a window packet.

    /// THE ROWS A MAKER MAY ARRANGE: the shared inventory, restricted to what the setup
    /// names. A catalog pane the setup does not name has no authored row, no window to
    /// arrange, and every operation here would have to refuse -- it is the PICKER's to
    /// offer, because participation is the picker's concern and never arrangement's.
    std::vector<PaneRef> arrangeable() const {
        std::vector<PaneRef> out;
        for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
            if (has_pane(session_.setup.active, row.ref)) {
                out.push_back(row.ref);
            }
        }
        return out;
    }

    /// ARRANGE THE DESK (ARR-0): the global arrangement scope.
    ///
    /// NO PANE IS CHOSEN MERELY BECAUSE THE SCOPE OPENED. The desk is the subject; every
    /// arrangeable pane answers the pointer directly, and the keyboard chooses its own
    /// target by stepping. The old mode opened ON a pane because it was a SELECTOR --
    /// choose, then choose a manipulation -- and that prerequisite is exactly what this
    /// scope retired.
    void open_arrange_desk() {
        PaneArrange& a = session_.arrange;
        a.open = true;
        a.desk = true;
        a.resetting = false;
        a.pane = PaneRef{};
        if (arrangeable().empty()) {
            say("arrange desk -- this setup names no panes; " + hotkey(Act::kPicker) +
                    " opens one, " + hotkey(Act::kManageClose) + " leaves",
                false);
            return;
        }
        say("arrange desk -- drag a pane's body or edges; " + hotkey(Act::kManageNext) +
                " steps, " + hotkey(Act::kManageClose) + " or right-click leaves",
            false);
    }

    /// ARRANGE ONE PANE (ARR-0): the pane-local scope, on an explicit target -- the
    /// context menu's captured subject, or the desk's keyboard target. ADMISSION PRECEDES
    /// BINDING, the rule CTX-0 introduced for Move/Size: a refusal is said in the owner's
    /// words and no state is entered, so a pane that cannot be arranged leaves the maker
    /// exactly where they were.
    void enter_arrange_pane(const PaneRef& ref) {
        const Written ready = arrange_geometry_ready(ref);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        PaneArrange& a = session_.arrange;
        a.open = true;
        a.desk = false;
        a.resetting = false;
        a.pane = ref;
        say("arranging " + ref_text(ref) + " -- drag its body to move, its edges to size; " +
                hotkey(Act::kManageClose) + " or right-click leaves",
            false);
    }

    /// THE ACTIVE SETUP NO LONGER NAMES THE ADDRESSED PANE, SO NOTHING DOES (WIND-2a).
    ///
    /// MEMBERSHIP IS THE LAW AND PRESENTATION IS NOT. A pane that becomes waiting, refused,
    /// covered, off-room or UNRESOLVED stays addressed: every one of those is a pane the
    /// setup still names, and reaching it is the whole of WIND-2's recovery claim. What
    /// clears the address is the reference leaving the setup, which is the one event after
    /// which there is nothing to address.
    ///
    /// THE ONE-PANE SCOPE CLOSES WITH ITS PANE (ARR-0): an interaction bound to exactly
    /// one pane is a state about nothing once that pane is gone, and holding a maker
    /// inside it would make every press a refusal about a thing no longer on the desk.
    /// It closes SILENTLY, because this runs inside `apply_setup` and the operation that
    /// removed the pane has its own sentence on the notice line -- writing over it here
    /// would erase the answer the maker just earned. The desk scope stays open: its
    /// subject is the desk, which is still there.
    void forget_removed_selection() {
        PaneArrange& a = session_.arrange;
        if (!a.addressed() || has_pane(session_.setup.active, a.pane)) {
            return;
        }
        a.pane = PaneRef{};
        session_.pane_drag = PaneGesture{};
        if (a.open && !a.desk) {
            a.open = false;
            a.resetting = false;
        }
    }

    /// Leave the arrangement whole: scope, target and the reset prompt go together, so a
    /// later open cannot inherit a stale address -- the desk deliberately opens on no
    /// pane, and the one-pane scope binds its own.
    void close_arrange() {
        session_.arrange = PaneArrange{};
        session_.pane_drag = PaneGesture{};
        say("left arranging", false);
    }

    /// What a maker reads about the pane the vocabulary addresses. One sentence, spent by
    /// every gesture that succeeds, so the notice line always names the thing that just
    /// moved -- and since the roster panel retired (ARR-0) it carries the pane's STATE
    /// word too, which is how an off-room or unresolved pane stays recoverable by ear:
    /// step to it, read what it is, reset it.
    std::string arrange_status() const {
        const PaneArrange& a = session_.arrange;
        if (!a.addressed()) {
            return "arrange -- no pane addressed";
        }
        const char* state = "";
        for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
            if (row.ref == a.pane) {
                state = pane_state_word(pane_state_of(session_.panels, session_.setup.active,
                                                      screen_of(session_), row));
                break;
            }
        }
        const SetupPane* row = pane_of(session_.setup.active, a.pane);
        return "arrange " + ref_text(a.pane) + " (" + state + ") -- " + pane_window_text(row);
    }

    /// MOVE THE KEYBOARD'S TARGET BY ONE ROW, wrapping. Over `arrangeable()`, so an
    /// unresolved, waiting, off-room, covered or refused pane is reached by exactly the
    /// same keys as a visible one -- which is the recovery invariant spent as a keyboard
    /// path. The desk opens on no target, so the first step lands on the first row.
    void arrange_step(std::int64_t by) {
        const std::vector<PaneRef> rows = arrangeable();
        if (rows.empty()) {
            say("this setup names no panes -- " + hotkey(Act::kPicker) + " opens one", true);
            return;
        }
        PaneArrange& a = session_.arrange;
        if (!a.addressed()) {
            a.pane = by > 0 ? rows.front() : rows.back();
            say(arrange_status(), false);
            return;
        }
        std::size_t at = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i] == a.pane) {
                at = i;
                break;
            }
        }
        const std::size_t n = rows.size();
        const std::size_t next = by > 0 ? (at + 1) % n : (at + n - 1) % n;
        a.pane = rows[next];
        say(arrange_status(), false);
    }

    /// CAN THIS PANE'S GEOMETRY BE AUTHORED RIGHT NOW, and if not, why not.
    ///
    /// TWO REFUSALS, AND THEY ARE DIFFERENT SENTENCES BECAUSE THEY ARE DIFFERENT FACTS. A
    /// side-region pane's place is RESERVED BY THE SCREEN -- `room_w` is what every share of
    /// the workspace resolves against, so moving Info would change the resolved size of
    /// objects in a maker's document, which PNL-0 refuses. A pane with no rectangle right now
    /// has nothing to measure a move or a resize against; its RESET and its ORDER still work,
    /// which is what makes the row a recovery path rather than a dead end.
    ///
    /// THE TARGET IS EXPLICIT SINCE CTX-0. The keyboard passes its addressed pane and the
    /// contextual surface passes its captured subject, so Arrange can CHECK a pointed
    /// pane before anything binds to it -- admission precedes binding, and a refusal
    /// leaves no dead arrangement state behind.
    Written arrange_geometry_ready(const PaneRef& ref) const {
        if (ref.provider.empty()) {
            return Written::no("no pane is addressed -- " + hotkey(Act::kManageNext) +
                               " steps to one");
        }
        // A CAPTURED SUBJECT HAS NO `forget_removed_selection` KEEPING IT FRESH, so absence
        // is answered here, first, in its own words -- falling through would report a
        // removed pane as "has no room on this screen yet": true of the screen, wrong about
        // the cause. For the mode's own selection this branch is unreachable today (the
        // clearing runs inside `apply_setup`), and it is written anyway: belt, not door.
        if (!has_pane(session_.setup.active, ref)) {
            return Written::no(ref_text(ref) + " is no longer in this setup -- " +
                               hotkey(Act::kPicker) + " can bring it back");
        }
        const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels.runtime);
        if (!kind.has_value()) {
            return Written::no(ref_text(ref) +
                               " is unresolved -- its place and size cannot be measured; "
                               "0 resets it and f/b/r/l still order it");
        }
        // A UNIT OUTRANKS A RESERVATION, the same precedence `pane_state_of` spends between
        // a unit and a want of room (WIND-2a). Both sentences are true of a fixed pane
        // sized in pixels, and only one of them tells a maker what to press.
        if (!pane_unit_projectable(pane_of(session_.setup.active, ref))) {
            return Written::no(kind_name(session_.panels, *kind) +
                               " is sized in pixels, which no medium here can project -- "
                               "0 then w or h resets that axis");
        }
        if (placement_of(*kind) == placement::kSideRegion) {
            return Written::no(kind_name(session_.panels, *kind) +
                               " is in the reserved side column -- the screen owns its place");
        }
        const PanelBounds where =
            bounds_of(session_.panels, session_.setup.active, *kind, screen_of(session_));
        if (!where.open) {
            return Written::no(kind_name(session_.panels, *kind) +
                               " has no room on this screen yet -- 0 resets it");
        }
        if (!where.projected) {
            return Written::no(kind_name(session_.panels, *kind) +
                               " is sized in pixels, which no medium here can project -- "
                               "0 then w or h resets that axis");
        }
        if (where.rect.w <= 0 || where.rect.h <= 0) {
            return Written::no(kind_name(session_.panels, *kind) +
                               " is off this screen -- 0 then p resets its place");
        }
        return Written::ok();
    }

    /// THE SELECTED PANE'S BOUNDS, both rectangles. Through `bounds_of`, never a second
    /// arithmetic: what a gesture measures from is what the painter drew.
    ///
    /// BOTH, AND THEY ARE SPENT ON DIFFERENT QUESTIONS (WIND-2a). `rect` is the VISIBLE
    /// intersection with the canvas and owns everything about where a hand MEETS this pane:
    /// painting, occupancy, coverage, where the affordances are drawn, and whether geometry
    /// can be reached at all. `resolved` is what the pane's authored-or-default intent ASKS
    /// for, unclipped, and it is the value a first edit captures.
    ///
    /// WIND-2 measured the first edit from the VISIBLE rectangle, and that is the finding
    /// this repairs: a default pane resolving to 89 cells with four of them on screen
    /// answered one rightward step by authoring FIVE. The maker's one-cell gesture became a
    /// reduction they never asked for, and the number it produced was a fact about the
    /// window rather than about the pane. A resize proposes `base + delta`, so the base has
    /// to be the size the pane actually has.
    PanelBounds managed_bounds() const {
        const std::optional<std::int64_t> kind =
            resolve_pane(session_.arrange.pane, session_.panels.runtime);
        if (!kind.has_value()) {
            return PanelBounds{};
        }
        return bounds_of(session_.panels, session_.setup.active, *kind, screen_of(session_));
    }

    /// THE WINDOW A GESTURE MEASURES FROM: authored where authored, resolved where
    /// reactive — the RESOLVED window, never the visible one (see `managed_bounds`).
    /// One spelling for the keys, the pointer's zero-delta test and the axis bases a
    /// partially-settled write falls back to (WUX-2a); three hand-kept copies of this
    /// authored-or-resolved read is how two gestures come to start from different places.
    FineRect managed_window_base() {
        const SetupPane* row = pane_of(session_.setup.active, session_.arrange.pane);
        FineRect out = managed_bounds().resolved;
        if (row != nullptr && row->place.mode == pane_unit::kSubcells) {
            out.x = row->place.x;
            out.y = row->place.y;
        }
        if (row != nullptr && row->width.mode == pane_unit::kSubcells) {
            out.w = row->width.amount;
        }
        if (row != nullptr && row->height.mode == pane_unit::kSubcells) {
            out.h = row->height.amount;
        }
        return out;
    }

    /// AUTHOR AN ABSOLUTE PLACE. `x`/`y` are the whole proposal, saturated by the caller.
    ///
    /// EACH AXIS IS ITS OWN PROPOSAL (WUX-2a): a coordinate that would leave the canvas is
    /// refused and KEEPS ITS CURRENT VALUE — never clamped to the wall — while the other
    /// axis, an independent fact, still follows the hand. An axis the gesture did not
    /// change proposes nothing, so a step refused on its one moving axis writes nothing at
    /// all and is said as a refusal; in particular it cannot author a reactive place as a
    /// side effect. Only a proposal refused on EVERY axis it moved is refused whole.
    void arrange_place(std::int64_t x, std::int64_t y, loom::Mail& mail) {
        const Written ready = arrange_geometry_ready(session_.arrange.pane);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        const FineRect from = managed_window_base();
        PaneAxisProposal horizontal;
        horizontal.base = from.x;
        if (x != from.x) {
            horizontal.position = x;
        }
        PaneAxisProposal vertical;
        vertical.base = from.y;
        if (y != from.y) {
            vertical.position = y;
        }
        const WindowWritten done = author_pane_window(session_.setup.active,
                                                      session_.arrange.pane, horizontal,
                                                      vertical);
        if (!done.written.accepted) {
            say(done.written.refusal, true);
            return;
        }
        // AND THE SEATING IS RECONCILED, because authoring a place takes the pane OUT of the
        // reactive stack -- it stops spending a tile, and whatever was waiting for one may
        // now have it. Resetting the place puts it back. This is the one door that opens or
        // closes a panel, so a geometry edit cannot produce a screen the setup disagrees with.
        if (done.place_written) {
            apply_setup(mail);
        }
        say(arrange_status(), false);
    }

    /// The place a one-cell nudge proposes: the resolved corner if this pane has no authored
    /// place yet, then the delta. AUTHORING THE CURRENT RESOLVED VALUE FIRST is what makes a
    /// first nudge move the pane by one cell rather than to cell (±1, ±1). The keys stay
    /// CELL-granular on purpose (WUX-2): a key press is a discrete gesture and a cell is its
    /// honest step; what the finer lattice buys it is that a nudge PRESERVES a fine
    /// remainder a pointer authored — plus forty-eight sub-units is still plus one cell.
    void arrange_nudge(std::int64_t dx, std::int64_t dy, loom::Mail& mail) {
        const Written ready = arrange_geometry_ready(session_.arrange.pane);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        // THE RESOLVED CORNER, NEVER THE CLIPPED ONE (WIND-2a) -- see `managed_bounds`.
        const FineRect from = managed_window_base();
        arrange_place(detail::step(from.x, dx * surface::kCellSubs),
                      detail::step(from.y, dy * surface::kCellSubs), mail);
    }

    /// AUTHOR WHAT ONE RESIZE GESTURE PROPOSES — the whole window, in sub-units (WUX-2),
    /// split into its two axes (WUX-2a).
    ///
    /// THE EDGE PRESERVES ITS OPPOSITE ANCHOR (`pane_window_proposal`): pulling the top
    /// edge proposes a new `y` WITH the new height as ONE VERTICAL AXIS, and the pair
    /// lands through `author_pane_window` together or not at all — so a top pull whose
    /// height is illegal does not move the top edge it failed to resize. THE OTHER AXIS IS
    /// AN INDEPENDENT FACT: a corner gesture whose height is illegal still widens the pane
    /// by its legal horizontal transaction, and the blocked axis keeps what it had.
    ///
    /// THE AXES THE EDGE DID NOT NAME KEEP WHAT THEY HAD, mode included -- a member is
    /// proposed exactly when the gesture CHANGED it -- so resizing a width leaves a
    /// default height still reacting to the room, and a right-edge or bottom-edge resize
    /// leaves a default PLACE still reactive: those edges anchor the place by not writing
    /// it at all.
    void arrange_resize(std::int64_t edge, std::int64_t base_x, std::int64_t base_y,
                        std::int64_t base_w, std::int64_t base_h, std::int64_t dx,
                        std::int64_t dy, loom::Mail& mail) {
        const Written ready = arrange_geometry_ready(session_.arrange.pane);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        const PaneWindowProposal want =
            pane_window_proposal(edge, base_x, base_y, base_w, base_h, dx, dy);
        PaneAxisProposal horizontal;
        horizontal.base = base_x;
        if (want.place_moved_x && want.x != base_x) {
            horizontal.position = want.x;
        }
        if (want.w != base_w) {
            horizontal.extent = PaneSize{pane_unit::kSubcells, want.w};
        }
        PaneAxisProposal vertical;
        vertical.base = base_y;
        if (want.place_moved_y && want.y != base_y) {
            vertical.position = want.y;
        }
        if (want.h != base_h) {
            vertical.extent = PaneSize{pane_unit::kSubcells, want.h};
        }
        const WindowWritten done = author_pane_window(session_.setup.active,
                                                      session_.arrange.pane, horizontal,
                                                      vertical);
        if (!done.written.accepted) {
            say(done.written.refusal, true);
            return;
        }
        if (done.place_written) {
            // AUTHORING A PLACE TAKES THE PANE OUT OF THE REACTIVE STACK — `arrange_place`'s
            // own reconciliation, owed here the moment an anchored resize writes one.
            apply_setup(mail);
        } else {
            // A SIZE-ONLY CHANGE CANNOT MOVE A PANE BETWEEN SEATED AND WAITING -- only a
            // PLACE does that -- but the room an external pane was granted may have moved,
            // and `repaint` owns that (`refresh_external_rooms`). Nothing is reconciled here.
            (void)mail;
        }
        say(arrange_status(), false);
    }

    /// The size a one-cell key press proposes: the authored window if there is one, else the
    /// resolved one -- the same "author the current resolved value, then apply the delta"
    /// rule the pointer follows, so the two gestures cannot disagree about where they start.
    /// One press is one CELL of delta, through the same anchored proposal the pointer takes.
    ///
    /// THE KEYBOARD'S ANCHOR IS THE PLACE (ARR-0): a key pull moves the bottom-right
    /// corner, so `pull-right` widens, `pull-left` narrows, and the pane never moves under
    /// a resize key -- the document's `shift+hjkl` family, said about a pane. The other
    /// six anchors are the pointer's, on the handles themselves; a maker who wants a
    /// left-anchored keyboard resize composes it from a pull and a place, each one honest
    /// cell.
    void arrange_grow(std::int64_t dx, std::int64_t dy, loom::Mail& mail) {
        const Written ready = arrange_geometry_ready(session_.arrange.pane);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        // THE RESOLVED WINDOW, NEVER THE VISIBLE ONE (WIND-2a) -- see `managed_bounds`.
        const FineRect base = managed_window_base();
        arrange_resize(pane_edge::kBottomRight, base.x, base.y, base.w, base.h,
                       dx * surface::kCellSubs, dy * surface::kCellSubs, mail);
    }

    /// ONE PANE ACTION, PERFORMED ON AN EXPLICIT TARGET -- the one place a targeted pane
    /// operation is spent, whatever asked for it (CTX-0; `end_held_gestures`' shape: one
    /// owner, several callers, not a framework).
    ///
    /// TWO CALLERS, ONE ARM PER ACTION. `arrange_key` passes its addressed pane; the
    /// contextual surface passes its captured subject. The address is never written as
    /// request transport in either direction, and the operation's own sentences stay here
    /// with the operation (INT-R0: the sentence belongs to the layer whose vocabulary
    /// holds the reason). Mode bookkeeping -- a reset closing its prompt -- stays with
    /// the keyboard caller, because the contextual caller is not in a mode.
    ///
    /// ABSENCE IS ANSWERED FIRST, IN ITS OWN WORDS. Every operation underneath already
    /// answers a reference outside the setup with `false`, but that `false` shares a
    /// sentence with "nothing to do" -- and "is already where that would put it" about a
    /// pane that is GONE is a true bool wearing a wrong sentence. One membership test, one
    /// truthful sentence, for every arm. (For the keyboard caller this is unreachable
    /// today -- `forget_removed_selection` runs inside `apply_setup` -- belt, not door.)
    void spend_pane_action(Act a, const PaneRef& ref, loom::Mail& mail) {
        if (ref.provider.empty()) {
            say("no pane is addressed -- " + hotkey(Act::kManageNext) + " steps to one",
                true);
            return;
        }
        Setup& s = session_.setup.active;
        if (!has_pane(s, ref)) {
            say(ref_text(ref) + " is no longer in this setup -- " + hotkey(Act::kPicker) +
                    " can bring it back",
                true);
            return;
        }
        switch (a) {
        // THE FOUR ORDERING OPERATIONS. Available for EVERY row, including an unresolved
        // one, because the rank is over all authored rows and reordering one writes
        // nothing that seating or placement reads.
        case Act::kManageFront:
        case Act::kManageBack:
        case Act::kManageRaise:
        case Act::kManageLower: {
            bool moved = false;
            const char* what = "";
            switch (a) {
            case Act::kManageFront: moved = send_to_front(s, ref); what = "front-most"; break;
            case Act::kManageBack: moved = send_to_back(s, ref); what = "back-most"; break;
            case Act::kManageRaise: moved = raise_one(s, ref); what = "raised"; break;
            default: moved = lower_one(s, ref); what = "lowered"; break;
            }
            if (!moved) {
                say(ref_text(ref) + " is already where that would put it", true);
                return;
            }
            say(ref_text(ref) + " " + what + " -- " + pane_window_text(pane_of(s, ref)),
                false);
            return;
        }
        // THE PER-PANE RESETS, one per authored dimension. (`order` is the whole setup's
        // and zero-target -- `reset_front_order` below -- because the rank is a
        // permutation over all of it.)
        case Act::kManageResetPlace:
        case Act::kManageResetWidth:
        case Act::kManageResetHeight: {
            bool moved = false;
            const char* what = "";
            if (a == Act::kManageResetPlace) {
                moved = reset_pane_place(s, ref);
                what = "place";
            } else if (a == Act::kManageResetWidth) {
                moved = reset_pane_width(s, ref);
                what = "width";
            } else {
                moved = reset_pane_height(s, ref);
                what = "height";
            }
            if (!moved) {
                say(ref_text(ref) + " already takes the developer's " + what, true);
                return;
            }
            // A PLACE RESET PUTS THE PANE BACK IN THE REACTIVE STACK, so the seating has
            // to be reconciled for the same reason authoring one does.
            apply_setup(mail);
            say(ref_text(ref) + " " + what + " reset -- " +
                    pane_window_text(pane_of(s, ref)),
                false);
            return;
        }
        // REMOVE THIS PANE (CTX-0). The picker's own semantics through the picker's own
        // door: the intent leaves the setup, `apply_setup` is what closes the
        // presentation, and what the pane was presenting is untouched -- a panel is a
        // presentation, and removing one removes a presentation. A removal works on a
        // waiting or unresolved row exactly as on an open one (WP-0's rule).
        case Act::kManageRemove: {
            const std::string name = ref_text(ref);
            if (!remove_pane(s, ref)) {
                say(name + " is no longer in this setup -- " + hotkey(Act::kPicker) +
                        " can bring it back",
                    true);
                return;
            }
            apply_setup(mail);
            say("removed " + name + " -- " + hotkey(Act::kPicker) +
                    " brings it back; nothing behind it was touched",
                false);
            return;
        }
        default: return;
        }
    }

    /// RESET THE WHOLE SETUP'S FRONT ORDER -- zero-target, one owner call and its
    /// sentence, spent by the keyboard's reset prompt and by the room's contextual row.
    void reset_front_order() {
        reset_front(session_.setup.active);
        say("front order reset to the setup's own order", false);
    }

    /// THE ARRANGEMENT KEYS -- one switch for both scopes and the reset prompt (ARR-0).
    /// Each scope is its own keymap context, so `action_for` already answers `kNone` for
    /// a gesture the current scope does not declare (the desk's stepping keys inside the
    /// one-pane scope, every non-reset key inside the prompt), and an arm below cannot
    /// fire out of its scope by construction. The targeted operations go through
    /// `spend_pane_action` with the addressed pane -- the ambient read is this caller's,
    /// the operation is the shared owner's, and the mode bookkeeping stays here.
    void arrange_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        PaneArrange& a = session_.arrange;
        const KeyContext ctx = keyboard_context(session_);
        switch (session_.keymap.action_for(ctx, k.scancode, k.modifiers)) {
        // -- the desk's stepping, and its door into the one-pane scope ------------------
        case Act::kManageNext: arrange_step(+1); break;
        case Act::kManagePrevious: arrange_step(-1); break;
        case Act::kArrange: enter_arrange_pane(a.pane); break;
        // -- moving: arrows place the addressed pane ------------------------------------
        case Act::kManagePlaceLeft: arrange_nudge(-1, 0, mail); break;
        case Act::kManagePlaceRight: arrange_nudge(+1, 0, mail); break;
        case Act::kManagePlaceUp: arrange_nudge(0, -1, mail); break;
        case Act::kManagePlaceDown: arrange_nudge(0, +1, mail); break;
        // -- sizing: shifted arrows pull the extent, anchored at the place --------------
        case Act::kManagePullLeft: arrange_grow(-1, 0, mail); break;
        case Act::kManagePullRight: arrange_grow(+1, 0, mail); break;
        case Act::kManagePullUp: arrange_grow(0, -1, mail); break;
        case Act::kManagePullDown: arrange_grow(0, +1, mail); break;
        // -- ordering and removal, on the addressed pane --------------------------------
        case Act::kManageFront: spend_pane_action(Act::kManageFront, a.pane, mail); break;
        case Act::kManageBack: spend_pane_action(Act::kManageBack, a.pane, mail); break;
        case Act::kManageRaise: spend_pane_action(Act::kManageRaise, a.pane, mail); break;
        case Act::kManageLower: spend_pane_action(Act::kManageLower, a.pane, mail); break;
        case Act::kManageRemove: spend_pane_action(Act::kManageRemove, a.pane, mail); break;
        // -- the reset prompt -----------------------------------------------------------
        case Act::kManageReset:
            a.resetting = true;
            say("reset -- " + hotkey_text(session_.keymap, Act::kManageResetPlace) +
                    " place, " + hotkey_text(session_.keymap, Act::kManageResetWidth) +
                    " width, " + hotkey_text(session_.keymap, Act::kManageResetHeight) +
                    " height, " + hotkey_text(session_.keymap, Act::kManageResetOrder) +
                    " order, " + hotkey_text(session_.keymap, Act::kManageDone) + " back",
                false);
            break;
        // The prompt closes exactly when the reset REACHED its operation (the pre-CTX-0
        // behaviour, preserved: a refusal for want of an addressed pane leaves the maker
        // in the prompt they were in).
        case Act::kManageResetPlace:
            spend_pane_action(Act::kManageResetPlace, a.pane, mail);
            if (a.addressed()) {
                a.resetting = false;
            }
            break;
        case Act::kManageResetWidth:
            spend_pane_action(Act::kManageResetWidth, a.pane, mail);
            if (a.addressed()) {
                a.resetting = false;
            }
            break;
        case Act::kManageResetHeight:
            spend_pane_action(Act::kManageResetHeight, a.pane, mail);
            if (a.addressed()) {
                a.resetting = false;
            }
            break;
        case Act::kManageResetOrder:
            reset_front_order();
            a.resetting = false;
            break;
        case Act::kManageDone:
            a.resetting = false;
            say(arrange_status(), false);
            break;
        // -- leaving --------------------------------------------------------------------
        case Act::kManageClose: close_arrange(); break;
        default: break;
        }
    }

    // ---- The pointer, inside arrangement -------------------------------------
    //
    // ONE PRESS CLAIMS ONE GESTURE UNTIL RELEASE, and the whole of that is the absence of a
    // re-resolution: `arrange_motion` reads `session_.pane_drag.pane` and never asks what is
    // under the pointer now. So crossing another pane, crossing the Terminal's rectangle, and
    // raising something mid-drag all change nothing about who is being moved -- which is
    // `Drag`'s own law, and the reason neither struct holds a position.

    /// TAKE HOLD OF ONE PANE AT A POINTED POSITION -- the edge ring sizes, the body
    /// moves, and a press outside its rectangle is not this pane's. One function for both
    /// scopes, so the desk and the one-pane state cannot come to grab differently.
    /// EVERYTHING HERE IS THE POINTER'S OWN RESOLUTION (WUX-2): the press arrives as a
    /// sub-unit position with the reporting medium's grain, every hit is the aligned-span
    /// law the paint uses, and the grab and the base are captured in sub-units — so the
    /// motion that follows can spend a single pixel of hand.
    bool take_pane_hold(const PaneRef& ref, const PointedAt& at, const Screen& sc) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels.runtime);
        if (!kind.has_value() || placement_of(*kind) != placement::kOverlayStack) {
            return false;
        }
        const PanelBounds mine = bounds_of(session_.panels, session_.setup.active, *kind, sc);
        if (!mine.open || mine.rect.w <= 0 || mine.rect.h <= 0) {
            return false;
        }
        const std::int64_t edge = pane_edge_at(mine.rect, at.sub.x, at.sub.y, at.grain);
        if (edge != kNoPaneEdge) {
            session_.pane_drag = PaneGesture{};
            session_.pane_drag.active = true;
            session_.pane_drag.pane = ref;
            session_.pane_drag.sizing = true;
            session_.pane_drag.edge = edge;
            session_.pane_drag.from_x = at.sub.x;
            session_.pane_drag.from_y = at.sub.y;
            const SetupPane* row = pane_of(session_.setup.active, ref);
            // THE AFFORDANCE IS ON THE VISIBLE BOUNDARY -- that is where the eye and the
            // hand are -- AND THE BASE IS THE RESOLVED WINDOW (WIND-2a): place beside
            // size since WUX-2, because an anchored top or left pull authors both from
            // this one captured rectangle. A hand and a key author from the same numbers,
            // which is the pairing this file has kept since both gestures existed.
            session_.pane_drag.base_x = row != nullptr && row->place.mode == pane_unit::kSubcells
                                            ? row->place.x
                                            : mine.resolved.x;
            session_.pane_drag.base_y = row != nullptr && row->place.mode == pane_unit::kSubcells
                                            ? row->place.y
                                            : mine.resolved.y;
            session_.pane_drag.base_w = row != nullptr && row->width.mode == pane_unit::kSubcells
                                            ? row->width.amount
                                            : mine.resolved.w;
            session_.pane_drag.base_h =
                row != nullptr && row->height.mode == pane_unit::kSubcells
                    ? row->height.amount
                    : mine.resolved.h;
            say(std::string("sizing ") + ref_text(ref) + " by its " + pane_edge_name(edge),
                false);
            return true;
        }
        if (mine.rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
            session_.pane_drag = PaneGesture{};
            session_.pane_drag.active = true;
            session_.pane_drag.pane = ref;
            session_.pane_drag.grab_dx = detail::minus(at.sub.x, mine.rect.x);
            session_.pane_drag.grab_dy = detail::minus(at.sub.y, mine.rect.y);
            say("moving " + ref_text(ref) + " -- drag to place it", false);
            return true;
        }
        return false;
    }

    /// A PRESS WHILE ARRANGING (ARR-0), and the two scopes answer it differently because
    /// they are ABOUT different things.
    ///
    /// THE ONE-PANE SCOPE IS BOUND: every press belongs to the bound pane -- its ring
    /// sizes, its body moves -- and a press anywhere else is CONSUMED with the sentence
    /// naming the state, because an interaction about exactly one pane must not quietly
    /// become an interaction about another. Leaving is one gesture away and the sentence
    /// says which.
    ///
    /// THE DESK IS DIRECT: every arrangeable pane answers, topmost first through the one
    /// presentation order the painter uses, and the pane a press takes hold of becomes
    /// the keyboard's target by that same press -- no selection is a prerequisite of
    /// anything. A pane whose place the screen reserves (the side column) is addressed
    /// and answered in the admission's own words rather than dragged.
    void arrange_press(const PointedAt& at) {
        PaneArrange& a = session_.arrange;
        const Screen sc = screen_of(session_);
        if (!a.desk) {
            if (a.addressed() && take_pane_hold(a.pane, at, sc)) {
                return;
            }
            say("arranging " + ref_text(a.pane) + " -- " + hotkey(Act::kManageClose) +
                    " or right-click leaves",
                false);
            return;
        }
        const std::vector<std::int64_t> order =
            presentation_order(session_.setup.active, session_.panels);
        for (std::size_t i = order.size(); i > 0; --i) {
            const std::int64_t kind = order[i - 1];
            if (!bounds_of(session_.panels, session_.setup.active, kind, sc)
                     .rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
                continue;
            }
            for (const SetupPane& row : session_.setup.active.panes) {
                const std::optional<std::int64_t> named =
                    resolve_pane(row.ref, session_.panels.runtime);
                if (named.has_value() && *named == kind) {
                    a.pane = row.ref;
                    if (!take_pane_hold(row.ref, at, sc)) {
                        // Addressed and not draggable -- the reserved side column. The
                        // admission owns the sentence.
                        const Written why = arrange_geometry_ready(row.ref);
                        say(why.accepted ? arrange_status() : why.refusal, !why.accepted);
                    }
                    return;
                }
            }
        }
        say("nothing to arrange there -- " + hotkey(Act::kManageClose) + " leaves", false);
    }

    /// A MOTION WHILE A PANE GESTURE IS HELD. It targets the pane that CLAIMED THE PRESS,
    /// looked up by its reference, so nothing under the pointer can take the gesture over.
    /// The deltas are sub-units — a one-pixel hand movement is four of them on the shipped
    /// skin, and a terminal's cell is forty-eight — proposed from the captured base, never
    /// accumulated (WUX-2).
    void arrange_motion(std::int64_t sub_x, std::int64_t sub_y, loom::Mail& mail) {
        PaneGesture& g = session_.pane_drag;
        if (!g.active) {
            return;
        }
        // THE TARGET MAY HAVE LEFT THE SETUP UNDER THE HAND -- a picker cannot be open while
        // this mode is, but a restore or a provider going away can -- so the gesture ends
        // safely rather than writing to a row that is no longer there.
        if (!has_pane(session_.setup.active, g.pane)) {
            g = PaneGesture{};
            forget_removed_selection();
            return;
        }
        const PaneRef held = g.pane;
        const PaneRef was_addressed = session_.arrange.pane;
        session_.arrange.pane = held;
        if (g.sizing) {
            arrange_resize(g.edge, g.base_x, g.base_y, g.base_w, g.base_h,
                           detail::minus(sub_x, g.from_x), detail::minus(sub_y, g.from_y),
                           mail);
        } else {
            arrange_place(detail::minus(sub_x, g.grab_dx), detail::minus(sub_y, g.grab_dy),
                          mail);
        }
        if (!has_pane(session_.setup.active, held)) {
            session_.arrange.pane = was_addressed;
        }
    }

    /// ASK FOR A BUILD -- by the name the TOOL gave, never by one of Workshop's.
    ///
    /// This is the sharpest statement of the split that this file makes. Workshop
    /// holds no target, no recipe and no command; the only build it can name is
    /// the one the Builder has already told it about, so a Workshop with a panel
    /// that has not yet heard from the tool cannot ask for anything at all, and
    /// says so.
    ///
    /// ---- ...AND SINCE BLD-1 IT NAMES ONE OF SEVERAL, AND MAY ASK FOR MORE -------
    ///
    /// The name comes from the CATALOG the tool published and the maker's cursor in it,
    /// which is the same sentence one plural out: Workshop still holds no recipe, and
    /// the only builds it can name are the ones the Builder has already told it about.
    ///
    /// `realize` IS THE MAKER'S SECOND INTENTION AND IT TRAVELS WITH THE FIRST. Workshop
    /// does not load anything, cannot load anything, and gains no rule that would let it
    /// -- what it does is say, in one sentence to one office, "build this, and if it
    /// works, offer it to the project". Everything after that belongs to two owners
    /// neither of which is here.
    void build_now(loom::Mail& mail, bool realize) {
        if (!session_.panels.has(panel::kBuilder)) {
            return; // `b` is an unbound key with no Builder panel open, exactly as before
        }
        const BuilderPane& pane = session_.panels.builder;
        if (!pane.heard) {
            say("the Builder has not said what it builds yet -- nothing was asked for", true);
            return;
        }
        if (pane.known.recipes.empty()) {
            say("this project has no build recipes -- nothing was asked for", true);
            return;
        }
        const std::size_t at =
            pane.chosen < pane.known.recipes.size() ? pane.chosen : std::size_t{0};
        const std::string chosen = pane.known.recipes[at].recipe;
        (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                zengine::builder::BuildRequested{chosen, realize});
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
        session_.panels.builder.awaiting_realization = realize;
        say("asked the Builder for `" + chosen + "`" +
                (realize ? " and to realize it" : std::string()) +
                " -- Workshop stays live while it builds",
            false);
    }

    /// MOVE THE MAKER'S CURSOR THROUGH THE RECIPES THE TOOL PUBLISHED (BLD-1).
    ///
    /// PURELY A PRESENTATION MOVE: nothing is sent, nothing is asked, and nothing on
    /// the bus knows it happened. It wraps, because a list of two or three that a maker
    /// is stepping through with one key is a ring and not a scrollbar, and stopping at
    /// the end would need a second key to come back.
    void choose_recipe(int by, loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return; // an unbound key with no Builder panel open, exactly as `b` is
        }
        BuilderPane& pane = session_.panels.builder;
        const std::size_t held = pane.known.recipes.size();
        if (held == 0) {
            say("this project has no build recipes to choose between", true);
            return;
        }
        const std::size_t at = pane.chosen < held ? pane.chosen : std::size_t{0};
        pane.chosen = by < 0 ? (at == 0 ? held - 1 : at - 1) : (at + 1 >= held ? 0 : at + 1);
        // THE ONE WRITER OF `picked` (BLD-2): this gesture is what makes a selection the
        // MAKER's rather than the catalog's order wearing an index. The frontier action
        // reads it when several recipes produce one artifact.
        pane.picked = true;
        say("build recipe: " + pane.known.recipes[pane.chosen].recipe + " -> " +
                pane.known.recipes[pane.chosen].artifact,
            false);
        repaint(mail);
    }

    /// BUILD AND REALIZE THE ROW THE PROJECT IS WAITING ON (BLD-2).
    ///
    /// THE JOIN IS PERFORMED HERE, ONCE, AND IT IS ONE STRING COMPARISON: the frontier
    /// artifact — the realization owner's own answer, read alive this very keystroke —
    /// against the artifact each catalog row already carries. Workshop still holds no
    /// recipe, no plan and no realization state; what this gesture adds is that the
    /// maker no longer performs that comparison in their head across two panes.
    ///
    /// IT SPENDS THE EXISTING ROUTE AND NOTHING ELSE. The one send is `build_now` with
    /// the realize intention — the same `BuildRequested` `Shift+b` says, to the same
    /// office, under the same grant — and everything after that belongs to the owners
    /// it always did: the tool refuses or orders, the runner runs, the tool offers, and
    /// the realization owner decides in its own words. There is no second build path,
    /// no direct load, and no new sentence on the bus.
    ///
    /// ⚠ SEVERAL RECIPES MAY PRODUCE ONE ARTIFACT, AND THAT IS AUTHORED LAW, NOT AN
    /// EDGE CASE (`builder::check_recipes` deduplicates identities and deliberately not
    /// artifacts). When more than one matches, this gesture refuses to choose: the
    /// catalog's order is nobody's intent, so it names the candidates and leaves the
    /// pick to `c` — after which the maker's standing pick, if it produces the
    /// frontier, is spent. `picked` is what tells an explicit pick from `chosen`'s
    /// default of 0, which is an index and not a choice.
    void build_frontier(loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return; // an unbound key with no Builder panel open, exactly as `b` is
        }
        BuilderPane& pane = session_.panels.builder;
        if (!pane.heard) {
            say("the Builder has not said what it builds yet -- nothing was asked for", true);
            return;
        }
        const ProjectFrontier now = frontier_now();
        if (!now.waiting) {
            // THE ABSENCE IS THE OWNER'S OWN ANSWER, read a moment ago — not a status
            // this panel manufactured. A project that is complete, still loading, or
            // was never begun is equally "not waiting", and all three are states in
            // which there is no frontier for this gesture to spend.
            say("this project is not waiting on any artifact -- nothing was asked for", true);
            return;
        }
        std::size_t makers = 0;
        std::size_t match = 0;
        std::string named;
        for (std::size_t i = 0; i < pane.known.recipes.size(); ++i) {
            if (pane.known.recipes[i].artifact != now.artifact) {
                continue;
            }
            ++makers;
            match = i;
            if (!named.empty()) {
                named += ", ";
            }
            named += "`" + pane.known.recipes[i].recipe + "`";
        }
        if (makers == 0) {
            say("no authored recipe produces `" + now.artifact + "` -- nothing was asked for",
                true);
            return;
        }
        if (makers > 1) {
            const bool standing_pick = pane.picked && pane.chosen < pane.known.recipes.size() &&
                                       pane.known.recipes[pane.chosen].artifact == now.artifact;
            if (!standing_pick) {
                say(std::to_string(makers) + " recipes produce `" + now.artifact + "` (" +
                        named + ") -- pick one with c, then f builds and realizes it",
                    true);
                return;
            }
            match = pane.chosen;
        }
        // THE SELECTION MOVES WITH THE GESTURE, VISIBLY: row 1 of the panel now names
        // the recipe this ask is about, and `build_now`'s own notice says it again. A
        // gesture that sent one recipe while the panel showed another would be the
        // cross-referencing this phase exists to end, reintroduced one row up.
        pane.chosen = match;
        build_now(mail, /*realize=*/true);
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
            say(draft->label() + " is still being edited -- " + hotkey(Act::kDraftCommit) +
                    " commits, " + hotkey(Act::kDraftCancel) + " cancels; nothing was saved",
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
        // A DOCUMENT REPLACEMENT IS THE ONE PATH an old object identity can come to
        // alias a different object -- the file restores the mint -- so a captured
        // contextual subject from the old document is dropped at this door, exactly as
        // the selection is re-established rather than preserved (CTX-0). A room or pane
        // subject names nothing the replacement touched and stands.
        if (session_.context.subject == context_subject::kObject) {
            session_.context = ContextMenu{};
        }
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

    /// What deleting THE SELECTED object says, read after the repair: where the selection
    /// went. One spelling for the keyboard's delete and the contextual one, so the two
    /// gestures cannot describe the same act differently -- "deleted, and you are now on
    /// #2" is one fact; leaving a maker to work out which object the inspector is
    /// suddenly showing is two.
    std::string deleted_notice(std::int64_t was) const {
        if (session_.selected == 0) {
            return "deleted #" + std::to_string(was) + " -- the document is empty";
        }
        return "deleted #" + std::to_string(was) + " -- now on #" +
               std::to_string(session_.selected);
    }

    /// Delete the selected one, and say where the selection went.
    void delete_object() {
        const std::int64_t was = session_.selected;
        const Written gone = delete_selected(state_, session_);
        if (!gone.accepted) {
            say(gone.refusal, true);
            return;
        }
        say(deleted_notice(was), false);
    }

    /// DELETE AN EXPLICIT OBJECT -- `delete_selected`'s target-taking sibling (CTX-0),
    /// and `delete_selected` itself is untouched.
    ///
    /// THE NEIGHBOUR/SELECTION REPAIR RUNS EXACTLY WHEN THE DELETED ID IS THE SELECTED
    /// ONE, and never otherwise: in the other branch `session_.selected` still names a
    /// live object (the document refuses to remove anything something else measures
    /// against, so a non-selected deletion cannot change how the selected one resolves),
    /// and perturbing a valid selection would be this gesture selecting something the
    /// maker did not point at. The rows are rebuilt, never patched.
    Written delete_object_at(std::int64_t id) {
        if (id == session_.selected) {
            return delete_selected(state_, session_);
        }
        const Written removed = doc::remove(state_, id);
        if (removed.accepted) {
            rebuild_rows();
        }
        return removed;
    }

    /// THE CONTEXTUAL DELETE: the captured object id, spent through the explicit-id door,
    /// under HD-8's live-draft hold-back. The hold-back is the application's own carve-out
    /// (`actions_press`'s rule, same sentence): deletion rebuilds the inspector rows out
    /// from under a live draft, `doc::remove` knows nothing about drafts, and nobody
    /// downstream would speak -- so the press is held here, before the operation. It is on
    /// the PRESS path, never on paint: the menu still offers the row.
    void context_delete_object(std::int64_t id) {
        if (draft_live(session_)) {
            say(finish_draft_first(), true);
            return;
        }
        const bool was_selected = id == session_.selected;
        const Written gone = delete_object_at(id);
        if (!gone.accepted) {
            say(gone.refusal, true);
            return;
        }
        // The selection moved only when the deleted one WAS it; a deletion that touched
        // no selection must not claim one moved.
        say(was_selected ? deleted_notice(id) : "deleted #" + std::to_string(id), false);
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
        say("the properties are not showing -- " + hotkey(Act::kPicker) +
            " opens the Info panel",
            true);
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
        say("editing " + row.label() + " -- " + hotkey(Act::kDraftCommit) + " commits, " +
                hotkey(Act::kDraftCancel) + " cancels",
            false);
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
            setup_name_columns(screen_of(session_), session_.keymap));
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

    /// GRANT EACH OPEN EXTERNAL PANE THE ROOM IT CURRENTLY HAS -- once per repaint, and
    /// only when the answer has changed (WP-0).
    ///
    /// IT IS CALLED FROM `repaint` AND NEVER FROM `paint`, and the separation is the rule
    /// this file has kept since BLD-0: `paint` is a pure function of document and session
    /// that PUBLISHES a picture, and a painter that sent messages would make the picture a
    /// side effect of describing it. So the room is reconciled on the path that owns `Mail`,
    /// beside `refresh_terminal` and `refresh_inspector`, for the identical reason those two
    /// are there: the answer a maker is looking at must be the answer resolved against the
    /// room the last frame drew.
    ///
    /// THE ROOM IS `external_body_place`'S, WHICH IS `fit_region`'S. One measurer. The
    /// provider is told `rows` and `columns` and nothing else -- no rectangle, no cell, no
    /// pixel, no font, no extent, no medium identity -- so it cannot compute a second layout
    /// and cannot disagree with this one.
    ///
    /// A GRANT IS SENT ON EXACTLY THREE OCCASIONS and no others:
    ///
    ///     the pane first opens                   `granted` is false
    ///     a valid re-offer refreshed it          the handler cleared `granted`
    ///     the resolved rows or columns moved     the comparison below
    ///
    /// So a screen that changed CELLS but not prose capacity says nothing, and a text metric
    /// that changed the capacity says it exactly once. A pane that has no room under its
    /// header at all is granted nothing rather than granted zero -- a budget of zero is a
    /// number somebody downstream would subtract from.
    ///
    /// AND EVERY GRANT CLEARS WHAT CAME BEFORE IT. The cached rows, the refusal, `heard`
    /// and `awaiting` are reset BEFORE the send, so the cache can never hold rows admitted
    /// under a wider room than the one currently in force, and an answer to the previous
    /// room can never be presented as an answer to this one.
    void refresh_external_rooms(loom::Mail& mail) {
        const Screen sc = screen_of(session_);
        for (const Panel& p : session_.panels.open) {
            if (!is_runtime_kind(p.kind)) {
                continue;
            }
            const PanelBounds where = bounds_of(session_.panels, session_.setup.active, p.kind, sc);
            const ExternalBodyPlace body = external_body_place(
                where.rect, sc,
                external_title_rows(session_.panels, p.kind, session_.pane_titles));
            if (!body.present) {
                continue;
            }
            const RuntimePane* row = session_.panels.runtime.of_kind(p.kind);
            ExternalPane* pane = session_.panels.external_pane(p.kind);
            if (row == nullptr || pane == nullptr) {
                continue;
            }
            if (pane->granted && pane->rows == body.rows && pane->columns == body.columns) {
                continue; // the same room: saying so again would be noise a provider must parse
            }
            const std::string office = row->provider;
            const std::string key = row->pane;
            pane->rows = body.rows;
            pane->columns = body.columns;
            pane->granted = true;
            pane->shown.clear();
            pane->clear_refusal();
            pane->heard = false;
            pane->awaiting = true;
            // DELIBERATELY AUTHORED AS `zengine.workshop` AND ADDRESSED TO THE OFFICE THE
            // DESCRIPTOR CAME IN UNDER. The authorship is what lets the provider verify the
            // ask (its side refuses a room from anyone else); the destination is a ROLE
            // rather than a WeaveId, so a provider that was replaced still gets its room.
            (void)mail.as_role(kWorkshopProvider)
                .send_to_role(office, PaneRoom{key, body.rows, body.columns});
        }
    }

    /// TELL A PROVIDER A MAKER PRESSED IN ITS ROOM -- the whole of the input seam (SEL-0).
    ///
    /// WHAT WORKSHOP KNOWS WHEN IT SENDS THIS, EXACTLY AND ONLY: that a primary press
    /// landed on cells this pane occupies, and which row and column of the ROOM IT GRANTED
    /// those cells are. It does not know what that row says, whether the provider shows a
    /// list, whether the row is selectable, whether anything is selected now, whether
    /// anything will change, or whether the provider is even still there. Nothing in this
    /// function reads `ExternalPane::shown`, and nothing may: the moment Workshop looks at a
    /// provider's rows to decide what a press means, the rows have become Workshop's
    /// vocabulary and the seam has stopped being a seam.
    ///
    /// THE POSITION IS RESOLVED FROM THE PAINTER'S OWN RECTANGLE, one call, in
    /// `external_press_at`. A press that names no row of the body sends nothing at all --
    /// it was already consumed by occupancy, and there is no sentence to make of it.
    ///
    /// AUTHORED AS `zengine.workshop` AND ADDRESSED TO THE OFFICE, exactly as the room
    /// grant is and for the same two reasons: the authorship is what lets the provider
    /// refuse a forged press, and the destination is a ROLE so a replaced provider still
    /// hears its own pane's presses.
    ///
    /// NOTHING IS REPAINTED HERE. Workshop's picture did not change -- no selection, no
    /// cache, no room, no notice -- and if the provider answers, that answer arrives as an
    /// ordinary `PaneContent` whose own handler repaints. A repaint on this path would
    /// publish a frame identical to the last one for every press a provider ignores.
    void external_press(std::int64_t kind, const zengine::input::PointerButton& b,
                        loom::Mail& mail) {
        const ExternalPressAt at =
            external_press_at(session_.panels, session_.setup.active, screen_of(session_), kind,
                              session_.pane_titles, b.space, b.x, b.y);
        if (!at.named) {
            return;
        }
        const RuntimePane* row = session_.panels.runtime.of_kind(kind);
        const ExternalPane* pane = session_.panels.external_pane(kind);
        // A ROOM THIS PANE HAS NOT BEEN GRANTED HAS NO LATTICE TO NAME A PLACE IN. `granted`
        // is false for exactly one beat -- between a panel opening and the repaint that
        // grants it -- and a press in that beat would be a position in a room the provider
        // has never been told about.
        if (row == nullptr || pane == nullptr || !pane->granted) {
            return;
        }
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PanePressed{row->pane, at.row, at.column});
    }

    /// WHICH EXTERNAL PANE THE KEYBOARD IS POINTED AT RIGHT NOW, or `kNoPaneKind` (MSG-0).
    ///
    /// THE CANDIDATE IS A PRESS'S MEMORY; THIS IS THE ANSWER. `Panels::keyboard` records
    /// which pane a maker last pressed into and nothing keeps it true afterwards -- a pane
    /// closes, a provider stops resolving, a setup is restored, a window shrinks until
    /// there is no room to grant. Rather than hooking every one of those (four writers for
    /// one fact, and the fifth is the one nobody adds), the target is DERIVED here from
    /// the same three things `external_press` requires before it will send a press: the
    /// panel is open, this build has a runtime kind for it, and a room has been granted.
    ///
    /// A ROOM THAT HAS NOT BEEN GRANTED HAS NO PANE TO TYPE INTO. `granted` is false for
    /// exactly one beat -- between a panel opening and the repaint that grants it -- and
    /// keys in that beat would be keys sent to a provider that has not been told it has a
    /// pane on screen at all.
    ///
    /// SO NOTHING EVER CLEARS IT, and a pane that becomes presentable again is typed into
    /// again with no gesture. That is deliberate rather than lazy: the candidate was a
    /// true statement about a maker's hand when it was written, and it stays true; what
    /// changes is whether there is a pane for it to name.
    ///
    /// THE RESOLUTION ITSELF IS `panel.hpp`'S, because the PAINTER asks it too -- the
    /// pane's header marks the pane that has the keys and the bottom band names it. Two
    /// answers to that question would be a screen that tells a maker they are typing
    /// somewhere the keys do not go.
    std::int64_t keyboard_pane() const {
        return zengine::workshop::keyboard_pane(session_.panels);
    }

    /// TELL A PROVIDER A KEY WENT DOWN WHILE ITS PANE HELD THE KEYBOARD (MSG-0).
    ///
    /// WHAT WORKSHOP KNOWS WHEN IT SENDS THIS, EXACTLY AND ONLY: that a key transition
    /// arrived, and that the pane a maker last pressed into is still on screen with a room.
    /// It does not know what the pane is showing, whether the key means anything there,
    /// whether a field is being edited, or whether the provider will answer. Nothing in
    /// this function reads `ExternalPane::shown` and nothing may -- the moment Workshop
    /// looks at a provider's rows to decide what a key means, the seam has stopped being
    /// one (SEL-0's rule, one gesture further on).
    ///
    /// THE TWO NUMBERS ARE FORWARDED AND NOT TRANSLATED. `scancode` and `modifiers` are
    /// `input::KeyPressed`'s own fields, which are already this application's normalized
    /// answer to "which key"; re-deriving them here would be a second translation table
    /// beside the one each backend already went through.
    ///
    /// AUTHORED AS `zengine.workshop` AND ADDRESSED TO THE OFFICE, exactly as the room
    /// grant and the press are, and for the same two reasons: the authorship is what lets
    /// a provider refuse a forged key, and the destination is a ROLE so a replaced
    /// provider still hears its own pane's keys.
    ///
    /// AND NOTHING IS REPAINTED HERE, for `external_press`'s reason: Workshop's picture did
    /// not change, and a provider that answers repaints through its own `PaneContent`.
    void external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
                      loom::Mail& mail) {
        const RuntimePane* row = session_.panels.runtime.of_kind(kind);
        if (row == nullptr) {
            return;
        }
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneKey{row->pane, k.scancode, k.modifiers});
    }

    /// ...AND THE TEXT THE PLATFORM MADE OF IT. `external_key`'s twin in every respect,
    /// and a separate send because they are separate facts: a key may produce no text and
    /// text may arrive with no key this application can name. Workshop maps no key to any
    /// character here any more than it does anywhere else.
    void external_text(std::int64_t kind, const zengine::input::TextEntered& t,
                       loom::Mail& mail) {
        const RuntimePane* row = session_.panels.runtime.of_kind(kind);
        if (row == nullptr) {
            return;
        }
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneTextInput{row->pane, t.text});
    }

    /// THE PROJECT FRONTIER, READ ALIVE, NOW (BLD-2). One spend, one answer, held for
    /// the length of the expression that asked — the host's own `frontier` view is the
    /// only source, and a fixture that wired none reads "not waiting", which paints the
    /// Builder panel exactly as every pre-BLD-2 case knew it.
    ProjectFrontier frontier_now() const {
        return host_->frontier ? host_->frontier() : ProjectFrontier{};
    }

    void repaint(loom::Mail& mail) {
        refresh_terminal();  // the pane is a snapshot, and a snapshot is only true when taken
        refresh_inspector(); // and a draft's window is only true against the room it has now
        refresh_setup_name(); // ...and so is the name editor's, against the same room
        refresh_external_rooms(mail); // ...and an external pane's room, against the same one
        // THE FRONTIER IS DERIVED HERE, PER PAINT, AND STORED NOWHERE. `paint` stays a
        // pure projection of what it is handed, and what it is handed is this repaint's
        // reading of the living realization owner — never a member, never a field of the
        // session, never yesterday's answer (BLD-2).
        const ProjectFrontier frontier = frontier_now();
        // THE SLOTS GO FIRST, AND THE PICTURE LAST. A slot is a line of text a
        // publisher hands the MEDIUM, and the medium owns what it makes of it — the SDL
        // medium composes the attention slot INTO the picture it draws, so a slot published
        // after the canvas would show one frame late. Ordering them ahead costs nothing
        // anywhere else (a title is set, a terminal row is written) and it is what makes
        // "the compact indicator is current" a fact rather than a race.
        mail.publish(
            zengine::surface::SurfaceText{zengine::surface::kSlotStatus, status_line()});
        // WHAT IS CURRENTLY TRUE AND WORTH A GLANCE, ON THE ONE ALWAYS-VISIBLE SLOT
        // WORKSHOP HAD NEVER SPENT. It is derived at every repaint from live owners and
        // held nowhere, exactly as the canvas is — so a condition that resolved is gone
        // from it because it stopped being returned, and NOBODY had to un-say anything.
        // EMPTY IS THE RETRACTION: a medium clears its presentation of a slot published
        // empty, which is why the disappearance needs no path of its own.
        mail.publish(zengine::surface::SurfaceText{
            zengine::surface::kSlotScore, attention_compact(attention_shown(session_, frontier))});
        mail.publish(paint(state_, session_, host_->setup_path, frontier));
    }

    /// LEAVE -- and write down what was on the desk on the way out (WUX-0).
    ///
    /// THE SAVE IS HERE AND NOT IN A SERVICE, and here is the ONE door: `q`, Ctrl+C and a
    /// close box all arrive at this function, so there is exactly one moment at which a
    /// session becomes durable, and no background writer, no dirty tracking and no timer had
    /// to be invented to find it. What that buys and what it costs are both said out loud:
    /// an ORDERLY close is remembered, and a Workshop that is killed is not.
    void quit() {
        save_last_session();
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
    /// Same sentence, same reason, same two ways out -- spelled from the effective
    /// keymap since KEY-0, like every other gesture this application advertises.
    std::string finish_draft_first() const {
        return "finish the draft first -- " + hotkey(Act::kDraftCommit) + " commits it, " +
               hotkey(Act::kDraftCancel) + " cancels";
    }

    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and
    /// a schema version is 32, so a wider number is REFUSED rather than
    /// truncated -- `send @x Foo 4294967297` must not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    HostContext* host_;
    Session session_;

    /// THE ASKER'S OWN BOOK OF PASTES STILL IN FLIGHT (QR-11), and the drafts each one
    /// belongs to. Plain members, per incarnation — a conversation belongs to the asker
    /// that opened it, and a successor's Skin would answer a dead weave's ask into the
    /// void, which is the correct fate for it. Capacity 4 is real pastes one bus turn
    /// settles; see `begin_clipboard_paste`.
    loom::AskBook paste_asks_{4};
    std::vector<PendingPaste> pending_pastes_;

    /// One moment's worth of memory: the character the gesture's OWN keystroke
    /// produced, which is not text a maker typed. Set by a gesture that opens a
    /// mode which takes text, cleared by the next key or the next text, so it
    /// can never outlive the moment it belongs to.
    ///
    /// Empty means nothing is owed. It holds the character rather than a bare
    /// flag so that a backend which reports the key and NO text cannot make the
    /// next real keystroke disappear.
    std::string swallow_text_;

    /// WHETHER THIS PROCESS HAS ALREADY TRIED TO TAKE BACK ITS LAST SESSION (WUX-0).
    ///
    /// A one-shot, and per PROCESS rather than per surface: `SurfaceReady` arrives again
    /// whenever a Skin is replaced, and a second restore would throw away everything a maker
    /// had arranged since the first. It is set before the file is even opened, so a refusal
    /// is final too -- a session Workshop could not read at startup is not one it should
    /// keep trying to read.
    ///
    /// It is a member of the WEAVE and not of `Session`, because it is not something a maker
    /// is doing and nothing paints it: it is this run's own bookkeeping about a thing that
    /// happens once.
    bool restored_ = false;

    /// What loading the keymap DID, held until the first surface can show it, and this
    /// run's own bookkeeping for the same reason `restored_` is. Empty means there is
    /// nothing that happened worth saying: no path, no file, or a file with no authored
    /// difference. A file that could not be admitted says nothing HERE -- that
    /// is a standing wall and it is a condition (`kKeymapWallKey`).
    bool keymap_loaded_ = false;
    std::string keymap_word_;
    bool keymap_bad_ = false;
    bool startup_spoken_ = false; ///< the one combined startup sentence has been said

    /// What loading the PREFS file produced (WUX-3), the keymap's own bookkeeping one file
    /// over. `prefs_bad_` is load-bearing beyond any sentence: a file that exists and could
    /// not be admitted is never overwritten, so a toggle while it stands changes the live
    /// preference and deliberately writes nothing (KEY-0's do-not-rewrite law, applied to
    /// the file Workshop itself writes). There is no `prefs_word_` beside it: a prefs file
    /// that APPLIED speaks for itself on screen (hidden titles are visibly hidden), and one
    /// that was refused is a condition, so this file has no event to announce at all.
    bool prefs_loaded_ = false;
    bool prefs_bad_ = false;

    /// WHETHER THIS RUN'S MEDIUM HAS REPORTED A DESKTOP PLACEMENT (WUX-3) -- the gate that
    /// keeps `Session::normal_w/h` honest. A restored `place_maximized` from LAST run's
    /// file must not stop THIS run's viewport tracking (a terminal run restoring a
    /// maximized session hears no placements and must keep remembering its own resizes),
    /// so the gate is "this run's medium said maximized", which needs both this flag and
    /// the session's current state. A weave member for `restored_`'s reason: it is this
    /// run's bookkeeping about its medium, not anything a maker does or a paint shows.
    bool medium_placed_ = false;

    /// The document as it is ON DISK, or an empty one when nothing has been
    /// written yet. Session, emphatically: it is a copy kept so the status line
    /// can answer "is this saved" by comparing rather than by trusting a flag.
    WorkshopDoc saved_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_WEAVE_HPP
