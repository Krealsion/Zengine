// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WEAVE_HPP
#define ZENGINE_WORKSHOP_WEAVE_HPP

// Workshop's own weave: the authored document, the session, and the bindings
// from input MOMENTS to maker GESTURES.
// Workshop law: agents/workshop/session.md (+23 registers; agents/workshop.md routes)




#include "persist.hpp"
#include "filesystem_roots.hpp" // which roots this system reports, asked at the gesture
#include "interaction_time.hpp" // what monotonic time it is, and nothing else
#include "keymap_persist.hpp"
#include "marks_persist.hpp"    // the places a maker said they want back
#include "pane_definition_persist.hpp" // the pane a maker made, as its own project file
#include "prefs_persist.hpp"
#include "screen.hpp"
#include "session_persist.hpp"
#include "setup_persist.hpp"

#include "input/vocabulary.hpp"
#include "operator/catalog.hpp" // the conversions this run has, looked up at a load
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
/// lever, and the terminal participant the host mounted.
/// `dir`/`so()` are the host's own boot bookkeeping and are filled in
/// there — kept whole in this header so a suite can construct one without
/// linking the host.
struct HostContext {
    bool quit = false;
    std::function<void()> request_stop;
    std::string dir;

    /// THE PROJECT THIS WORKSHOP WAS LAUNCHED INTO -- the directory the process was
    /// started from, captured ONCE by the host and never recomputed.
    // WL-FILES-02 -- agents/workshop/files.md; WL-PROJ-01 -- agents/workshop/project.md
    std::string project_dir;

    /// THE TERMINAL PARTICIPANT WORKSHOP PRESENTS — non-owning, and null when the
    /// host mounted none.
    // WL-TERM-02 -- agents/workshop/terminal.md
    loom::TerminalSession* terminal = nullptr;

    /// WHAT PROJECT REALIZATION IS WAITING ON, ANSWERED ALIVE.
    // WL-ATTN-04 -- agents/workshop/attention.md
    std::function<ProjectFrontier()> frontier;

    /// WHAT MONOTONIC TIME IT IS, ANSWERED BY THE HOST -- `frontier`'s seam exactly.
    // WL-PTR-01 -- agents/workshop/pointer.md
    std::function<std::int64_t()> interaction_now;

    /// WHAT THE AUTHORED RECIPE CATALOG SAYS ABOUT ONE RECIPE'S SOURCE, answered by the
    /// HOST.
    // WL-EDIT-05 -- agents/workshop/editor.md; WL-PROJ-02 -- agents/workshop/project.md
    struct RecipeSource {
        bool known = false; ///< the id names an authored recipe of this project
        std::string kind;   ///< `single_source` or `cmake_target`, the file's own words
        std::string source; ///< the one authored source file; empty when the kind has none
    };

    /// `frontier`'s exact seam, one catalog over: the host wires a function over its own
    /// authored recipes, the weave spends it at the moment of the gesture and stores
    /// nothing.
    // WL-EDIT-05 -- agents/workshop/editor.md
    std::function<RecipeSource(const std::string&)> recipe_source;

    /// WHAT A MAKER'S CHOICE OF AUTHORED CATALOG ANSWERED.
    // WL-PROJ-05, WL-PROJ-09 -- agents/workshop/project.md
    struct RecipeSwap {
        bool accepted = false;   ///< the candidate became this session's catalog
        std::string refusal;     ///< the owner's own words; empty exactly when accepted
        std::string path;        ///< the authored catalog IN FORCE after this request
        std::size_t recipes = 0; ///< how many that catalog holds
    };

    /// USE THIS AUTHORED FILE AS THE CURRENT RECIPE CATALOG.
    // WL-PROJ-04, WL-PROJ-05 -- agents/workshop/project.md
    std::function<RecipeSwap(const std::string&)> use_recipes;

    /// The one file this Workshop saves to and loads from.
    // WL-SESSION-01 -- agents/workshop/session.md
    std::string document_path;

    /// The one file this Workshop's SETUP saves to and restores from.
    // WL-LAYOUT-10 -- agents/workshop/layouts.md; WL-SESSION-01 -- agents/workshop/session.md
    std::string setup_path;

    /// The one file this Workshop's LAST SESSION is written to and read from.
    // WL-SESSION-01, WL-SESSION-04, WL-SESSION-13 -- agents/workshop/session.md
    std::string session_path;

    /// The one file this Workshop's LOCATION MARKS live in.
    // WL-FILES-08 -- agents/workshop/files.md; WL-SESSION-01 -- agents/workshop/session.md
    std::string marks_path;

    /// The one file this Workshop's open PANE DEFINITION is read from and written to.
    // WL-MAKER-08 -- agents/workshop/maker-pane.md
    // WL-SESSION-01 -- agents/workshop/session.md
    std::string pane_path;

    /// The one file this Workshop's KEYMAP is read from.
    // WL-KEY-07 -- agents/workshop/keyboard.md; WL-SESSION-01 -- agents/workshop/session.md
    std::string keymap_path;

    /// The one file this Workshop's presentation PREFERENCES live in.
    // WL-FOCUS-11 -- agents/workshop/focus.md; WL-SESSION-01 -- agents/workshop/session.md
    std::string prefs_path;

    /// WHICH CONVERSIONS THIS RUN ACTUALLY HAS, or nothing.
    // WL-MIG-08 -- agents/workshop/migration.md
    const op::Catalog* conversions = nullptr;

    /// ONE HUMAN-READABLE SENTENCE ABOUT THE LEGACY-FILE TRANSITION, or empty.
    // WL-ATTN-02 -- agents/workshop/attention.md; WL-SESSION-03 -- agents/workshop/session.md
    std::string transition_note;

    /// WHAT THE HOST ALREADY KNEW WAS TRUE, AND STILL IS.
    // WL-ATTN-01 -- agents/workshop/attention.md
    std::vector<Condition> standing_conditions;

    /// AN ARTIFACT STEM, AS THIS PLATFORM SPELLS A SHARED LIBRARY.
    ///
    /// THE ONE RULE, AND IT IS THE HOST'S. A directory, a separator and a
    /// suffix: that is the whole of what turns `zengine-timer` into a file, and
    /// keeping it here rather than in an authored plan is what makes ONE plan legal
    /// on Linux and on Windows -- no platform matrix, no per-OS field, no `.so` or
    /// `.dll` written down anywhere a person edits, and no package locator.
    ///
    /// IT TAKES A VIEW because a stem now arrives as a `std::string`
    /// read out of a file as often as it arrives as a literal. One signature that
    /// serves both is what keeps this the only place either spelling is resolved.
    std::string so(std::string_view stem) const { return so_in(dir, stem); }

    /// THE SAME RULE, AIMED SOMEWHERE ELSE.
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
                                          zengine::input::PointerWheel,
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
                                        zengine::workshop::PaneTextInput,
                                        zengine::workshop::PaneWheel>> {
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

    /// READ THE MAKER'S KEYMAP, OR STAND ON THE DEFAULTS.
    // WL-KEY-07, WL-KEY-08 -- agents/workshop/keyboard.md
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

    /// READ THE MAKER'S PRESENTATION PREFERENCES, OR STAND ON THE DEFAULTS.
    // WL-FOCUS-11 -- agents/workshop/focus.md
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
    // WL-ATTN-02 -- agents/workshop/attention.md
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
    // WL-ATTN-01, WL-ATTN-02 -- agents/workshop/attention.md
    void take_host_conditions() {
        for (const Condition& c : host_->standing_conditions) {
            session_.conditions.establish(c);
        }
    }

    /// A Skin claimed the surface and said hello: give it the whole screen. The
    /// operator weave's precedent, and the only thing Workshop needs in order to
    /// paint for the first time -- so load order decides nothing here either.
    /// AND IT IS WHERE WORKSHOP ASKS THE ROOM WHO HAS PANES.
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
        // THE FIRST PICTURE OF A RUN IS WORKSHOP'S FLOOR, AND THAT IS LOAD-BEARING.
        //
        // A medium that has not been told anything has only this picture to size itself
        // from, and whatever it makes of it, it must not be that Workshop can never again be
        // smaller than the window its maker happened to leave open last night. So the run's
        // FIRST statement about how much room it wants is the smallest room it is honest in,
        // and the room it is trying to get BACK is the second -- a want, not a floor.
        // Reversing the two costs a maker the ability to shrink their window, which is a
        // stranger thing to lose to a continuity feature than anything it could have bought.
        load_keymap();
        // The prefs beside it, BEFORE the first paint: the first band and the
        // first pane headers a maker reads are already wearing their own preference.
        load_prefs();
        //...and the maker's own places, so a marks file this run cannot read is a
        // condition the FIRST picture already carries rather than one discovered whenever
        // the browser happens to open. It has its own once-guard, so a run that reaches the
        // browser before any surface exists reads them there instead, exactly once.
        load_marks();
        //...AND THE MAKER'S OWN PANE, BEFORE THE SESSION IS TAKEN BACK -- the
        // ordering is the whole of the relaunch story. The session's desks name a
        // maker-made pane by its durable reference, and `apply_setup` seats a reference
        // only if it resolves at that moment; a definition opened after the restore would
        // leave the pane a maker saved yesterday counted unresolved on the very row it
        // came back on. So identity is established first, from its own file, through the
        // one open door; the session then finds it exactly as it finds a built-in.
        load_pane_definition(mail);
        // ...and whatever the host already knew was standing, so the first picture
        // of the run already carries every condition this launch is going to have.
        take_host_conditions();
        repaint(mail);
        restore_last_session(mail);
        speak_startup_notes(mail);
    }

    /// READ THE PROJECT'S PANE DEFINITION, OR STAND ON NONE.
    // WL-MAKER-08, WL-MAKER-09 -- agents/workshop/maker-pane.md
    void load_pane_definition(loom::Mail& mail) {
        if (pane_loaded_) {
            return;
        }
        pane_loaded_ = true;
        const std::string path = host_pane_path();
        if (path.empty()) {
            return;
        }
        if (!std::filesystem::exists(path)) {
            return;
        }
        open_maker_pane(path, mail);
    }

    /// THE HOST'S PANE PATH IN THE ONE SPELLING THE DOORS COMPARE.
    // WL-MAKER-08 -- agents/workshop/maker-pane.md
    std::string host_pane_path() const {
        if (host_->pane_path.empty()) {
            return std::string();
        }
        return persist::resolved_against(host_->project_dir, host_->pane_path);
    }

    /// THE SURFACE SAID HOW MUCH ROOM IT HAS. Take it, and lay the screen out again.
    // WL-DOC-17 -- agents/workshop/document.md; WL-GEO-08 -- agents/workshop/geometry.md
    void on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail) {
        if (!adopt_screen(session_, e.width, e.height, e.text_advance_px, e.text_line_px,
                          e.cell_px)) {
            return;
        }
        // THE NORMAL WINDOW'S ROOM FOLLOWS THE SCREEN, EXCEPT WHILE THIS RUN'S MEDIUM SAYS
        // THE WINDOW IS MAXIMIZED. The medium reports placement BEFORE extent on
        // its beat (skin.hpp says why once), so by the time a maximized room arrives the
        // gate is already closed and the remembered normal viewport survives to the save.
        // A run whose medium never reports placement -- every terminal -- never gates, and
        // a maximized flag merely RESTORED from a file must not gate either: that is last
        // run's window, and this run's resizes are this run's to remember.
        if (!(medium_placed_ && session_.place_maximized)) {
            session_.normal_w = session_.screen_w;
            session_.normal_h = session_.screen_h;
        }
        // THE ROWS ARE REBUILT AND A LIVE DRAFT IS CARRIED ACROSS. The resolved row
        // closes over the extent it resolves against, so the rebuild is not optional -- but
        // this is the ONE rebuild that happens for a reason having nothing to do with the
        // maker. A window dragged is not a gesture aimed at the inspector, and earlier
        // measured it on the pristine tree it silently threw away whatever was half-typed
        // into a property, its refusal and the cursor with it. Every OTHER caller of
        // `rebuild_rows` follows a change of selection or of document, where dropping the
        // draft is the right answer and carrying it would put it on a different object.
        refocus_keeping_draft(state_, session_);
        // AND THE COMPOSITION IS RECONCILED AGAINST THE ROOM IT NOW HAS. A screen
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

    /// THE MEDIUM SAID WHERE ITS WINDOW SITS. Remember it, whole and opaque.
    // WL-SESSION-08, WL-SESSION-09 -- agents/workshop/session.md
    void on(const zengine::surface::SurfacePlacement& p, loom::Mail&) {
        medium_placed_ = true;
        session_.placement_known = true;
        session_.place_x = p.x;
        session_.place_y = p.y;
        session_.place_maximized = p.maximized;
    }

    /// THE SURFACE WAS ASKED TO CLOSE -- by the window manager, the close box,
    /// the platform. Workshop applies the quit policy it already has.
    // WL-SESSION-13 -- agents/workshop/session.md
    void on(const zengine::surface::SurfaceCloseRequested&, loom::Mail&) { quit(); }

    /// A key TRANSITION: which key changed, and what was held when it did.
    // WL-KEY-03, WL-KEY-05, WL-KEY-12 -- agents/workshop/keyboard.md
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        // THE CONTEXT IS RESOLVED ONCE, AT ENTRY, and every decision this turn -- the
        // above-mode arm, the swallow, the chain -- spends the same answer, so a mode a
        // dispatch arm opens cannot change what THIS keystroke meant.
        const KeyContext ctx = keyboard_context(session_);
        // THE SWALLOW BELONGS TO ONE MOMENT: cleared on every key, armed only when this
        // keystroke is consumed as an application binding whose key also enters text
        // (`expected_text_of` derives the owed character from the binding -- the
        // generalization of the three hard-coded `" "`/`"s"`/`"w"` sites the keymap research found,
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
        case Act::kQuit:
            // A quit REFUSED for unsaved source says so on the notice line, which has to
            // be painted to be read; a quit that proceeded publishes one last unchanged
            // frame on its way out, which costs nothing anybody sees.
            quit();
            repaint(mail);
            return;
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
        // THE CHAIN IS `keyboard_context`'S ANSWER NOW. Its order -- and every
        // recorded rationale behind it: the modes that own the keyboard whole, the
        // reachability arguments that are written down anyway, the pressed-into-LAST
        // symmetry that puts a focused pane above a live draft, `keyboard_pane` resolved
        // fresh with nothing to clear -- lives with the resolver, where the paint path
        // and the paste mirror read the same answer. This switch is what remains of four
        // hand-copies of that order: which owner the resolved context names.
        //
        // A COPY ANYWHERE BELOW IS SAID TO THE PROCESS ONCE, HERE. The component
        // bumps the clipboard's `writes` exactly when a copy or cut took text, so one
        // comparison around the whole chain notices it whichever consumer it happened in —
        // three handlers each publishing would be the fourth-copy accident arriving in the
        // routing. What is published is `ClipboardCopy`: the Skin offers it to the
        // platform's clipboard and every other text-holding participant mirrors it.
        //
        // A PASTE ANYWHERE BELOW IS ASKED FOR ONCE, HERE, THE SAME WAY. The
        // component bumps `paste_requests` instead of pasting, because the value a paste
        // means is the clipboard's CURRENT value and only this owner can obtain it — a
        // read performed BECAUSE this paste was requested, never a mirror kept fresh by
        // watching. The same one comparison notices it, `paste_owner_now()` (a derivation
        // of the resolved context not a second spelling of the chain) says
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
        case KeyContext::kPaneNaming: pane_naming_key(k, mail); break;
        case KeyContext::kPicker: picker_key(k, mail); break;
        case KeyContext::kAttention: attention_key(k); break;
        case KeyContext::kContext: context_key(k, mail); break;
        case KeyContext::kPane: external_key(keyboard_pane(), k, mail); break;
        case KeyContext::kEditor: editor_key(k); break;
        case KeyContext::kFiles: files_key(k, mail); break;
        case KeyContext::kPaneEditor: pane_editor_key(k, mail); break;
        case KeyContext::kDraft: editing_key(k, mail); break;
        default: command(k, mail); break;
        }
        // ESCAPE'S FINAL MEANING IS TO PUT THE SELECTED PANE DOWN. It is asked
        // LAST, after the resolved context has had the key: every mode, overlay and draft
        // answers Escape with a row of its own (`picker.close`, `draft.cancel`,
        // `manage.close`, `context.back`, `attention.close`, `naming.cancel`,
        // `terminal.back`), the hotkey view took it further up -- and a bare Escape that
        // arrives here in a context where the keys are held by a LIST or by NOTHING, with
        // no binding claiming it, is an Escape nothing more specific owned. Then, if a pane
        // is selected, the selection is the layer it sheds: the press-elsewhere gesture's
        // own two lines, spent without needing an unoccupied pixel to press. Nothing else
        // moves -- no pane closes, no rank, no geometry, no Pane Editor subject, no
        // provider state, no file.
        //
        // A PLACE A MAKER TYPES INTO KEEPS ESCAPE WHILE IT HOLDS THE KEYS
        // (`escape_may_shed_selection`). The source editor's Escape is a pinned no-op
        // -- a maker's habitual Esc must not hand the next `d` to command mode. A
        // focused external pane has already been sent the key and Workshop cannot see
        // whether it spent it: the seam carries no `consumed`, by design, and the
        // shipped Composer does spend it (its form goes back to its catalog and the maker
        // keeps typing). So the pane keeps its keys, and the way out of either is
        // the way in: press a pane that takes no text -- every desk has one, Layouts --
        // or the workspace, then Escape.
        //
        // IT IS DELIBERATELY NOT A KEYMAP ACTION, the hotkey view's own reason: a recovery
        // gesture must not be authorable into a lockout. A maker who binds Escape to an
        // action in one of these contexts has said what Escape means there; their binding
        // answered above and this line does not.
        if (k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone &&
            escape_may_shed_selection(ctx) &&
            session_.keymap.action_for(ctx, k.scancode, k.modifiers) == Act::kNone &&
            session_.panels.selected != kNoPaneKind) {
            unselect_pane();
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

    /// THE VIEW'S OWN KEYS: Escape closes it, and everything else is swallowed.
    // WL-KEY-11 -- agents/workshop/keyboard.md
    void hotkeys_key(const zengine::input::KeyPressed& k) {
        if (k.scancode == input::scan::kEscape && k.modifiers == input::mod::kNone) {
            session_.hotkeys.open = false;
        }
    }

    /// Open or close the current-condition view -- `toggle_hotkeys`' own shape,
    /// one surface over.
    // WL-ATTN-09, WL-ATTN-10 -- agents/workshop/attention.md
    void toggle_attention() {
        session_.attention.open = !session_.attention.open;
        session_.attention.cursor = 0;
    }

    /// THE VIEW'S OWN KEYS: move the cursor, hide the condition it is on, close.
    // WL-ATTN-08, WL-ATTN-09 -- agents/workshop/attention.md
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

    // ---- What can I do with this? The contextual-action surface ---------------
    // WL-CTX-01 -- agents/workshop/contextual.md

    /// OPEN ON WHAT IS POINTED AT.
    // WL-CTX-01 -- agents/workshop/contextual.md
    void open_context_at(const PointedAt& at) {
        ContextMenu next;
        next.open = true;
        // THE PRESS'S OWN CELL IS THE ANCHOR: the surface opens beside the hand
        // that asked, on both media at the cell grain -- the composition is settled in
        // cells before any metric is consulted, own medium-independence rule.
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
                    resolve_pane(row.ref, session_.panels);
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

    /// OPEN ON A PAINTED LAYOUT TAB -- the same surface, on the one subject the band owns.
    // WL-TAB-12 -- agents/workshop/tab-run.md
    void open_context_on_layout(const PointedAt& at, std::size_t layout) {
        ContextMenu next;
        next.open = true;
        next.anchored = true;
        next.anchor_x = at.cell.x;
        next.anchor_y = at.cell.y;
        next.subject = context_subject::kLayout;
        next.layout = layout;
        session_.context = next;
    }

    /// OPEN BY KEY, on the subject command mode can truthfully name: the selected object
    /// while one resolves, else the room.
    // WL-CTX-01 -- agents/workshop/contextual.md
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

    /// CHOOSE THE ROW THE CURSOR IS ON.
    // WL-CTX-08 -- agents/workshop/contextual.md
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

    /// SPEND ONE CHOSEN ACTION against the captured subject.
    // WL-CTX-01, WL-CTX-02, WL-CTX-07, WL-CTX-08 -- agents/workshop/contextual.md
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
        // -- the pointed LAYOUT TAB ---------------------------------------------------
        //
        // EVERY ONE OF THESE TAKES THE CAPTURED POSITION and none of them switches first.
        // The subject is the tab the press named; the owner re-asks the run about it at
        // spend, exactly as the pane rows re-ask about a `PaneRef`, so a run that changed
        // while the menu was open refuses rather than acting on whoever moved into that
        // slot.
        case Act::kLayoutRename: open_layout_rename(spent.layout); break;
        case Act::kLayoutDuplicate: duplicate_layout(spent.layout, mail); break;
        case Act::kLayoutMoveLeft: shift_layout(spent.layout, -1); break;
        case Act::kLayoutMoveRight: shift_layout(spent.layout, +1); break;
        case Act::kLayoutRemove: drop_layout(spent.layout, mail); break;
        // -- the room -----------------------------------------------------------------
        case Act::kObjectNew: create_object(); break;
        case Act::kPicker: open_picker(); break;
        case Act::kArrangeDesk: open_arrange_desk(); break;
        case Act::kTerminalToggle: toggle_terminal(); break;
        case Act::kAttention: toggle_attention(); break;
        case Act::kHotkeys: toggle_hotkeys(); break;
        case Act::kSaveDocument: save_document(); break;
        case Act::kOpenDocument: load_document(); break;
        case Act::kSetupSave: save_setup(); break;
        case Act::kSetupRestore: restore_setup(mail); break;
        case Act::kManageResetOrder: reset_front_order(); break;
        default: break;
        }
    }

    /// A BUTTON-1 PRESS WHILE THE SURFACE IS OPEN.
    // WL-CTX-08 -- agents/workshop/contextual.md
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

    /// ANOTHER PARTICIPANT'S COPY — a pane provider's field, mirrored under the no-echo rule.
    // WL-TEXT-08 -- agents/workshop/text-box.md
    void on(const zengine::surface::ClipboardCopy& c, loom::Mail&) {
        session_.clipboard.text = c.text;
    }

    /// THE SKIN'S ANSWER TO A PASTE THIS WEAVE REQUESTED — the one road foreign clipboard
    /// text has into this application, and it is walked only under a maker's paste.
    // WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
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
        case PasteOwner::kEditor: {
            // THE EDITOR'S SETTLEMENT PINS THE WHOLE POSITION, not just the draft: the
            // request recorded which document was open and exactly where it stood, and
            // the answer applies there or nowhere. A replaced or closed document strands
            // the payload silently (the dead draft's own fate); a document that MOVED --
            // any edit, any caret or selection change between request and answer -- gets
            // a sentence instead of a paste, because relocating the text to wherever the
            // caret is now would be answering a question the maker no longer asked.
            EditorState& e = session_.editor;
            if (!e.open_document() || e.doc_epoch != p.editor_doc) {
                return; // the document that asked is gone; discarded, silently
            }
            if (e.buffer.revision() != p.editor_revision) {
                say("the paste answer arrived after the source moved -- nothing was "
                    "pasted; paste again",
                    true);
                repaint(mail);
                return;
            }
            if (a.readable) {
                session_.clipboard.text = a.text; // the platform's current truth, asked for
            }
            if (session_.clipboard.text.empty()) {
                repaint(mail);
                return; // an empty clipboard pastes nothing, the component's own law
            }
            const PasteableSource judged = pasteable_source(session_.clipboard.text);
            if (!judged.representable) {
                say("the clipboard holds bytes outside plain ASCII, which this editor "
                    "cannot carry truthfully -- nothing was pasted",
                    true);
                repaint(mail);
                return;
            }
            e.buffer.paste_lines(judged.lines);
            e.follow_caret = true;
            repaint(mail);
            return;
        }
        case PasteOwner::kTerminal:
            // The line outlives the pane's visibility (shift+space hides it and keeps the
            // draft), so an open pane is not required — the same DRAFT is.
            box = &session_.terminal.input;
            break;
        case PasteOwner::kNaming:
            box = naming_line();
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


    /// TEXT the maker actually entered — the platform's answer, not a guess made
    /// from a key identity.
    // WL-KEY-03 -- agents/workshop/keyboard.md
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
        // the keymap it is answered by the same resolver instead of by this function's own
        // hand-copy of the chain (the second of the five spellings the research measured).
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
        case KeyContext::kPaneNaming:
            session_.pane_naming.line.type(t.text);
            repaint(mail);
            return;
        case KeyContext::kTerminal:
            // AT THE CARET, WHICH IS NOT ALWAYS THE END. `type` is the only
            // door that moves the text and the caret together, so a keystroke in the
            // middle of a line cannot leave one behind. The line changed, so what could
            // be said next changed with it: typing IS the completion gesture.
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
        case KeyContext::kEditor:
            editor_text(t.text);
            repaint(mail);
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

    /// WHAT A BUTTON-1 RELEASE ENDED — and it is asked, not assumed.
    ///
    /// `pane` is meaningful only when `pane_held`; `document_id` only when `document`.
    struct GesturesEnded {
        bool document = false;
        std::int64_t document_id = 0;
        bool pane_held = false;
        PaneRef pane;
    };

    /// END EVERY BUTTON-1 GESTURE THIS SESSION IS HOLDING, whatever mode saw the release.
    // WL-ARR-02 -- agents/workshop/arrangement.md; WL-TAB-11 -- agents/workshop/tab-run.md
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
        // is on screen, which is the whole statement. The selection itself SURVIVES
        // the release — ending the sweep is not unselecting — so only the gesture record is
        // cleared here.
        session_.text_drag = TextDrag{};
        //...and so does the tab drag. The run's new order is on screen and the
        // moves were already narrated one step at a time, so a release has nothing to add;
        // what it must do is end the gesture, wherever the hand happens to be, for this
        // function's whole stated reason.
        session_.tab_drag = LayoutTabDrag{};
        return out;
    }

    /// A pointer button changed, AND the position it changed at.
    // WL-FOCUS-03 -- agents/workshop/focus.md; WL-PRESS-04 -- agents/workshop/press-chain.md
    void on(const zengine::input::PointerButton& b, loom::Mail& mail) {
        // WHILE THE OVERLAY IS OPEN THE WORKSPACE GETS NOTHING, and that half is
        // unchanged: the pane covers the bottom-right of the screen,
        // workspace included, so a press there would take hold of an object the
        // maker cannot see -- and a press just outside it would move the document
        // out from under a mode they are typing in. One sentence covers both:
        // while the terminal is open, the terminal has the input. There is no
        // focus object, no capture and no z-order; closing it restores every
        // gesture exactly.
        //
        // WHAT CHANGED IS THAT THE TERMINAL NOW DOES SOMETHING WITH IT --
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
            // repair rather than a new rule (own: "a gesture that began on the
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
                // EVERY BUTTON-1 GESTURE, and not only the document's. A pane
                // move or size begun in pane management is held in a second record, and
                // the overlay used to swallow its release exactly as it once swallowed the
                // document's: `pane_drag.active` stayed true with the button up, and the
                // first bare motion after the pane closed moved a window nobody was
                // holding. Measured -- the pane walked to the pointer.
                (void)end_held_gestures();
                return;
            }
            // AND THE BOOL BELOW IS NOT THE PRESS-CHAIN'S BOOL, which is why it is given a
            // name here. The three handlers under `if (b.pressed)` answer whether they
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
        // ARRANGEMENT IS A MODE AND IT OWNS THE POINTER WHILE IT IS OPEN -- the
        // Terminal's own shape, four lines up, for the same reason. While a maker is
        // arranging, every press is about a pane: letting one fall through to the
        // document would begin a drag on an object underneath a pane they are looking at,
        // which is the defect occupancy removed from panels in the first place.
        //
        // A SECONDARY PRESS IS THIS STATE'S WAY BACK OUT. The active interaction
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
        // the same repair made for the pane: entering a mode mid-drag must not swallow
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
        // THE CONTEXTUAL SURFACE HAS FIRST REFUSAL WHILE IT IS OPEN -- a mode in
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
        // A RIGHT PRESS ASKS "WHAT CAN I DO WITH THIS?". Before this branch a
        // second button meant nothing anywhere in Workshop, so consuming it displaces no
        // behaviour and steals nothing from any provider -- the pane seam cannot say a
        // second button, deliberately, and no `PanePressed` is sent for one. Only a press
        // opens; a release of button 3 falls through to the gate below and is dropped, as
        // every non-primary transition always was.
        if (b.pressed && b.button == 3 && at.understood) {
            //...AND A TAB IS A SUBJECT IT CAN NAME -- BEHIND OCCUPANCY.
            // The tab inverse is asked only once the ordinary walk has answered that the
            // Layouts pane owns this point, so the menu's subject is the tab under the hand
            // when the tabs are what is under the hand, and is whatever pane a maker put in
            // FRONT of them when it is not. Until this phase the question was asked first
            // and globally, which made a covered tab nameable through the pane covering it.
            // A right press on the create affordance names the room, not a layout: `+` is
            // an action rather than a thing, so there is nothing to ask about it.
            const Occupancy owner =
                occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
            const LayoutTabPress tab =
                owner.occupied && owner.kind == panel::kLayouts
                    ? band_tab_at(session_, screen_of(session_), b.space, b.x, b.y)
                    : LayoutTabPress{};
            if (tab.hit && !tab.create) {
                open_context_on_layout(at, tab.at);
            } else {
                open_context_at(at);
            }
            repaint(mail);
            return;
        }
        if (b.button != 1 || !at.understood) {
            return;
        }
        if (b.pressed) {
            // TRUE MEANS CONSUMED: STOP ROUTING. FALSE MEANS NOT CONSUMED: CARRY ON.
            // That is the whole meaning of the three bools below, and it is the only meaning
            // any of them has -- not "something changed", not "the act succeeded", not "the
            // press was accepted". A layer that consumes may refuse in its own words, may say
            // nothing at all, and may leave every fact in this application exactly as it found
            // it; what it may not do is let a press it owns be answered by the layer around
            // it. A consumed press does not have to change anything -- it only has to have
            // reached the layer that owns what the press means.
            //
            // AND THE BODY IS RESOLVED ONCE, HERE, beside the canvas point above it.
            // The three handlers under it are three questions about ONE place, and they used
            // to resolve it separately -- the same six lines three times, and up to three
            // resolutions of one body for one press. Holding it across the chain is safe for a
            // reason worth writing down rather than assuming: every one of the three changes
            // nothing on the paths where it declines, so a handler that says "not mine" has
            // not moved the picture the next handler is about to ask about.
            const InfoBodyAt where = info_body_at(state_, session_, b.space, b.x, b.y);
            // AND THE OCCUPANCY WALK IS RESOLVED HERE TOO beside the body and
            // the canvas point, for the reason the body was hoisted: it is one question
            // about one place, every handler below changes nothing on the path where it
            // declines, and the answer is now needed BEFORE the chain rather than after it.
            // It is the same pure walk `occupied_at` always was -- the picker first, then
            // the panes topmost-first, then nothing -- moved, not changed.
            const Occupancy here =
                occupied_at(session_.panels, session_.setup.active, screen_of(session_), at);
            // WHERE THE KEYBOARD GOES IS DECIDED BY THE PRESS ITSELF, IN ONE LINE, BEFORE
            // ANY LAYER ANSWERS IT. Putting it in the routing arms instead would be
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
            //
            // A BUILT-IN CAN BE A CANDIDATE WHEN ITS CATALOG ROW SAYS SO. The Editor was
            // the first -- a body a maker types into, so a press there points the keys at
            // their source exactly as a press into an external pane points them at a
            // provider -- and Project Files is the second, a list with a cursor and
            // gestures of its own. At two, the distinction stopped being something this
            // line should know: it is a fact about a KIND, so it is declared on the kind
            // (`PanelKind::takes_keyboard`) and read here. Every other built-in still
            // clears the candidate, and this is still not a focus framework -- one
            // declaration moved, nothing registered.
            //
            // ⚠ THE PRIOR ANSWER IS READ BEFORE IT IS OVERWRITTEN, because one arm below
            // needs it: Project Files activates a row only when the pane ALREADY had the
            // keys, and this line is what makes that untrue a moment later. Reading it
            // afterwards would make every first press look like a press in a pane the
            // maker was already working in.
            const bool files_had_keyboard = files_has_keyboard(session_);
            // WHICH PANE THE MAKER JUST POINTED AT -- ONE READING, TWO FACTS.
            // Selection is the wider of the two and the keyboard candidate is DERIVED
            // from it through the declared candidacy, rather than the occupancy being
            // tested twice: two reads of one press is how the desk comes to think one
            // pane is in front while the keys go to another. A press that lands on the
            // workspace, on the picker, on the screen's own furniture or on nothing
            // clears both by these same two lines.
            session_.panels.selected = here.occupied ? here.kind : kNoPaneKind;
            session_.panels.keyboard =
                session_.panels.selected != kNoPaneKind &&
                        kind_takes_keyboard(session_.panels.selected)
                    ? session_.panels.selected
                    : kNoPaneKind;
            // A VISIBLE PANEL OCCUPIES POINTER SPACE, and this is the
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
            // (`here` was resolved at the top of this branch -- one walk, for
            // two questions that are about the same press.)
            // AND AN EXTERNAL PANE IS THE ONE PRESENTATION WHOSE PRESS GOES SOMEWHERE
            //. It is the SAME occupancy answer -- one geometry walk, one topmost
            // rule, the picker still first -- asked one further question: this cell belongs
            // to a pane Workshop did not compile, so the press is that provider's.
            //
            // CONSUMED EITHER WAY, AND DECIDED HERE RATHER THAN THERE. A pane that owns
            // visible room owns pointer refusal for that room, and the refusal is Workshop's
            // to make because Workshop is what knows the room exists. Nothing waits for the
            // provider: there is no reply shape, `external_press` sends and returns, and a
            // press that named no row of the body (the header, the padding under the last
            // prose line, the lattice's edge) is consumed exactly the same and simply
            // travels no further. That is split -- the synchronous half of the
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
            // it would have to guess (a refusal belongs to the deepest layer whose
            // vocabulary contains the reason -- and this one's does not).
            if (here.occupied && is_runtime_kind(here.kind)) {
                external_press(here.kind, b, mail);
            } else if (here.occupied && here.kind == panel::kEditor) {
                // A PRESS INTO THE EDITOR PLACES THE CARET AND BEGINS A SELECTION SWEEP
                // -- the draft's and the Terminal line's own press, over a document. It
                // says nothing: the caret is the statement, and the candidate line above
                // already pointed the keys here for the whole rectangle. A press on the
                // header or past the body's rows moves nothing and is consumed exactly
                // as an external pane's header press is.
                editor_press(b);
            } else if (here.occupied && here.kind == panel::kProjectFiles) {
                // AND A PRESS INTO THE PROJECT BROWSER SELECTS A ROW -- the editor's arm,
                // over a list. A press on the header or past the last row moves nothing
                // and is consumed exactly as the editor's is.
                files_press(b, files_had_keyboard, mail);
            } else if (here.occupied && here.kind == panel::kPaneEditor) {
                // AND A PRESS INTO THE PANE EDITOR -- Files' arm, one pane over:
                // a pane row chooses the SUBJECT, a field row moves the row cursor, the
                // live draft's own row places the caret, and the heading or the padding
                // is consumed as a focus statement. The selection line above has already
                // made this pane the selected one; nothing in here reads that fact.
                pane_editor_press(b, b.modifiers);
            } else if (here.occupied && here.kind == panel::kInfo &&
                       (info_press(where, b.modifiers) || actions_press(where) ||
                        objects_press(where))) {
                // THE INFO PANEL'S OWN THREE INVERSES, BEHIND THE OWNERSHIP DECISION LIKE
                // EVERY OTHER PANE'S. They are unchanged -- same order, same
                // disjointness, same `true means consumed` -- and what changed is only
                // WHERE they are asked. Until this phase all three ran BEFORE the occupancy
                // walk and never consulted the effective order, so a pane authored over the
                // side column and ranked in FRONT of Info still lost its presses on Info's
                // control cells to Info: see-here, press-there, at the same boundary the
                // top band had it. Nothing here is a new routing layer; three questions
                // moved down into the arm that already knew which pane owns the point.
                //
                // THE ACTIVE PROPERTY EDITOR IS ASKED FIRST, and it is a PLACE inside a
                // panel rather than a mode: the innermost thing that owns the
                // pointer where it landed answers before the thing around it, and a press
                // it declines falls through unchanged. It says nothing and consumes whether
                // or not the caret moved -- the caret IS the statement, and a sentence
                // repeating it would push off the line a refusal the maker may still need
                //.
                //
                // THEN THE ACTION CONTROLS, then the OBJECT LIST. The three
                // runs of the body cannot fight over a press -- the footer, the object list
                // and a live draft's own row are disjoint runs of ONE row budget, which is
                // what making the body one region bought -- so this ordering is
                // written down because an ordering resting on a disjointness proof is one
                // refactor from being silently wrong, not because two of them could answer.
                //
                // ⚠ THE SHORT CIRCUIT IS THE CHAIN, and it is exact: `||` stops at the
                // first `true`, and each of the three changes nothing on the path where it
                // declines -- which is the property that let the body be hoisted in the
                // first place. A press none of them owns falls to Info's own sentence
                // below, which is what it always did.
                repaint(mail);
                return;
            } else if (here.occupied && here.kind == panel::kLayouts &&
                       layouts_press(b, mail)) {
                // AND THE LAYOUTS PANE'S OWN INVERSE -- the tabs, `+`, the rename
                // second press and the reorder drag, asked ONLY once the ordinary walk has
                // said this point is that pane's. It is `files_press`' position exactly.
                // The inverse itself is still specialised to Layouts and still rule
                // end to end (the spans come from `band_status`' own composition); what is
                // gone is the coordinate exception that used to ask it first, above every
                // pane, from a rectangle nothing else could name.
                repaint(mail);
                return;
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
            // THROUGH THE SAME OWNER AS EVERY OTHER MODE, so there is one place
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
    // WL-PANE-05 -- agents/workshop/panes-and-windows.md
    void on(const zengine::input::PointerMoved& m, loom::Mail& mail) {
        // ---- READING PAST AN ELLIPSIS, BEFORE ANYTHING ELSE THIS MOTION MEANS ------------
        //
        // IT IS A POINTING AND NOT A GESTURE, which is why it is resolved here rather than in
        // one of the branches below: nothing is held, nothing is claimed, and the answer is a
        // pure function of where the pointer is right now (`reveal_for`) compared with what
        // the session was already showing. A motion that changes neither costs no repaint,
        // which is what keeps a hand crossing the screen from republishing the canvas.
        //
        // A MODE THAT OWNS THE POINTER OWNS THIS TOO. While the Terminal, an arrangement
        // scope or the contextual surface is open, a motion is theirs -- and so is the
        // absence of a reveal, because a row a maker cannot point at is not a row they are
        // pointing at.
        //
        // AND A HELD GESTURE IS NOT A HOVER. A hand sweeping a selection, moving an object or
        // sizing a pane is doing something with the pointer; scrolling a row underneath it
        // would be a second meaning for one motion.
        const bool pointer_is_spent = session_.terminal.open || session_.arrange.open ||
                                      session_.context.open || session_.hotkeys.open ||
                                      session_.attention.open || session_.text_drag.active ||
                                      session_.drag.active || session_.pane_drag.active ||
                                      session_.tab_drag.active;
        const Revealed want =
            pointer_is_spent ? Revealed{}
                             : reveal_for(state_, session_, m.space, m.x, m.y);
        if (!want.same_as(session_.reveal)) {
            session_.reveal = want;
            repaint(mail);
        }
        // ---- CARRYING A LAYOUT TAB ALONG THE RUN -----------------------------------------
        //
        // THE HAND IS HOLDING THE LIVE LAYOUT, because the press that began this made that
        // tab live. So a motion asks the same inverse the press asked -- against the run as
        // it is painted RIGHT NOW, which has already reordered under any earlier step of
        // this same drag -- and moves the live layout to whatever tab it is over.
        //
        // NOTHING IS CACHED AND NOTHING IS RECONCILED. `move_layout` changes order and only
        // order; no desk is replaced, so there is no `apply_setup` and no provider hears a
        // thing. A motion that is over no tab, over the create affordance or over the live
        // tab's own span moves nothing -- which is what makes dragging past the end of the
        // run rest rather than wrap.
        if (session_.tab_drag.active) {
            const LayoutTabPress over =
                band_tab_at(session_, screen_of(session_), m.space, m.x, m.y);
            if (over.hit && !over.create &&
                move_layout(session_.setup, session_.setup.active_at, over.at)) {
                repaint(mail);
            }
            return;
        }
        if (session_.terminal.open) {
            // THE OVERLAY HAS THE INPUT, and one motion matters inside it: a
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
        // A SELECTION DRAG ON THE PANE EDITOR'S LIVE DRAFT -- the property draft's
        // twin below, resolved through the Pane Editor's own body.
        if (session_.text_drag.active &&
            session_.text_drag.place == text_drag_place::kPaneEditorDraft) {
            Row* row = pane_editor_editing_row();
            const PaneEditorAt where = pane_editor_at(session_, m.space, m.x, m.y);
            if (row != nullptr && where.present) {
                row->drag_to_column(property_value_column(where.at.column));
                refresh_inspector();
                repaint(mail);
            }
            return;
        }
        // A SELECTION DRAG ON THE LIVE PROPERTY DRAFT — the Terminal branch's twin
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
        // A SELECTION DRAG IN THE SOURCE EDITOR — the third editable place, and the first
        // where the ROW is meaningful mid-drag: a document has many. The geometry is
        // re-resolved per motion through the same resolution the press spent; a hand past
        // the body's top or bottom edge steps the caret one row further per motion (the
        // component's leftward-step law turned vertical), and the follow flag then pulls
        // the viewport after it -- deterministic, minimal, and enough to sweep a
        // selection out of the window a motion at a time.
        if (session_.text_drag.active &&
            session_.text_drag.place == text_drag_place::kEditorBody &&
            session_.editor.open_document()) {
            const Screen sc = screen_of(session_);
            const ExternalBodyPlace body = editor_body(session_, sc);
            if (!body.present) {
                return;
            }
            const ProseAt at = prose_at(m.space, m.x, m.y, body.region_x, body.region_y,
                                        body.fit);
            if (!at.understood) {
                return;
            }
            EditorState& e = session_.editor;
            const std::int64_t brow = at.row - body.header_rows;
            std::size_t target;
            if (brow < 0) {
                target = e.first_row > 0 ? e.first_row - 1 : 0;
            } else if (brow >= body.rows) {
                target = e.first_row + static_cast<std::size_t>(body.rows);
            } else {
                target = e.first_row + static_cast<std::size_t>(brow);
            }
            e.buffer.drag_to(target,
                             at.column < 0 ? std::int64_t{-1} : e.first_col + at.column);
            e.follow_caret = true;
            repaint(mail);
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

    /// THE WHEEL TURNED.
    // WL-EDIT-10 -- agents/workshop/editor.md
    void on(const zengine::input::PointerWheel& w, loom::Mail& mail) {
        if (session_.terminal.open || session_.arrange.open || session_.context.open) {
            return;
        }
        const Screen sc = screen_of(session_);
        const PointedAt at = canvas_point_of(w.space, w.x, w.y);
        if (!at.understood) {
            return;
        }
        // The TOPMOST presentation under the wheel decides -- a pane in front owns its
        // own cells, and scrolling something under somebody else's pane is the
        // imaginary-reach this test refuses. The picker answers first inside the walk,
        // exactly as it does for a press, and it is the one occupant with no kind.
        const Occupancy here =
            occupied_at(session_.panels, session_.setup.active, sc, at);
        if (!here.occupied) {
            return;
        }
        if (here.kind == kNoKind) {
            picker_wheel(w, mail);
            return;
        }
        if (is_runtime_kind(here.kind)) {
            external_wheel(here.kind, w, mail);
            return;
        }
        if (here.kind == panel::kProjectFiles) {
            files_wheel(w, sc, mail);
            return;
        }
        if (here.kind == panel::kPaneEditor) {
            pane_editor_wheel(w, mail);
            return;
        }
        if (here.kind != panel::kEditor || !session_.editor.open_document()) {
            return;
        }
        if (!over_editor_body(session_, sc, w.space, w.x, w.y)) {
            return;
        }
        EditorState& e = session_.editor;
        const std::int64_t lines = spend_wheel(e.wheel_accum, w.dy, kEditorWheelLines);
        if (lines == 0) {
            return;
        }
        const ExternalBodyPlace body = editor_body(session_, sc);
        if (!body.present) {
            return;
        }
        const std::size_t rows = static_cast<std::size_t>(body.rows);
        const std::size_t total = e.buffer.line_count();
        const std::size_t furthest = total > rows ? total - rows : 0;
        std::size_t first = e.first_row;
        if (lines > 0) {
            const std::size_t up = static_cast<std::size_t>(lines);
            first = first > up ? first - up : 0;
        } else {
            first += static_cast<std::size_t>(-lines);
        }
        if (first > furthest) {
            first = furthest;
        }
        if (first == e.first_row) {
            return; // already at the edge: nothing moved, nothing repaints
        }
        e.first_row = first;
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
        // THE ASYNC BUILD MADE THAT DISTINCTION WORTH MORE, NOT LESS. A build now has a
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
        // ---- THE SECOND ANSWER, ANNOUNCED ON ITS OWN LATCH --------------------
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
        // A FINISHED BUILD IS THE ONE THING THIS APPLICATION ALREADY KNOWS ABOUT THAT
        // CHANGES THE PROJECT ON DISK, so it is the one message that earns the browser a
        // fresh listing -- which is how Project Files stays truthful with no watcher, no
        // poll and no timer, and without one byte added to any protocol.
        //
        // IT IS GATED ON `build_news` AND NOT ON THE ARRIVAL. The tool republishes its
        // whole picture on every transition and again whenever a panel opens, so
        // "a status arrived" is not "a build finished": scanning on arrival would walk
        // the directory for a build that ended before this pane existed. `build_news` is
        // the fact this weave already derives for exactly that distinction -- a build
        // this session watched, which has now reached an outcome it will not leave.
        files_build_settled();
        switch (said.outcome) {
        case zengine::builder::outcome::kSucceeded:
            // TWO OUTCOMES, TWO SENTENCES, AND THE SECOND IS NOT SUPPRESSED BY THE
            // FIRST. A maker who asked for BUILD & REALIZE and got a green
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

    /// THE BUILDER TOOL SAID WHAT THIS PROJECT CAN BUILD.
    // WL-PROJ-07 -- agents/workshop/project.md
    void on(const zengine::builder::RecipeCatalog& said, loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return;
        }
        BuilderPane& pane = session_.panels.builder;
        // THE IDENTITY IS TAKEN BEFORE THE CATALOG MOVES, because it is the only thing
        // about the old catalog worth carrying across. An empty string is the honest
        // absence -- nothing was selected, or nothing was there to select.
        const std::string was = pane.chosen < pane.known.recipes.size()
                                    ? pane.known.recipes[pane.chosen].recipe
                                    : std::string();
        pane.known = said;
        std::size_t now = pane.known.recipes.size(); // = not found
        for (std::size_t i = 0; i < pane.known.recipes.size(); ++i) {
            if (!was.empty() && pane.known.recipes[i].recipe == was) {
                now = i;
                break;
            }
        }
        if (now < pane.known.recipes.size()) {
            // THE SAME RECIPE, WHEREVER IT NOW SITS. `picked` is untouched: whether this
            // selection was the maker's explicit act or the catalog's own first row is a
            // fact about how it was made, and a reordering does not change it.
            pane.chosen = now;
            repaint(mail);
            return;
        }
        pane.chosen = 0;
        // A SELECTION THAT NO LONGER NAMES ANYTHING IS NOT THE MAKER'S ANY MORE:
        // the recipe their pick named is gone, and 0 is where the panel put them, not
        // where they went. The frontier action must not read it as an explicit choice.
        pane.picked = false;
        repaint(mail);
    }

    // ---- THE EXTERNAL PANE SEAM: an office offers, Workshop grants, an office says
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
    // failure mode a research pass was corrected for: a role is a LIVE, REPLACEMENT-STABLE SERVICE
    // ROUTE on this bus in this process. It is not a package author, not a signature, not a
    // publisher, and not evidence that the same author came back after a restart.

    /// AN OFFICE OFFERS A PANE. Admitted, refreshed, or refused -- and every one of those
    /// is bounded before a byte is retained.
    void on(const PaneOffered& offer, loom::Mail& mail) {
        // READ AS A VIEW AND KEPT AS ONE. The stamp belongs to the delivery
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
        // IDENTITY IS ASKED OF WHAT WAS ALREADY ADMITTED, WITH VIEWS. The pair
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
    // WL-KEY-12 -- agents/workshop/keyboard.md
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

    /// THE PANE EDITOR'S LIVE DRAFT, if any -- asked by its own name where the
    /// caller already knows which inspector it is standing in.
    Row* pane_editor_editing_row() {
        for (Row& r : session_.pane_editor.rows) {
            if (r.editing()) {
                return &r;
            }
        }
        return nullptr;
    }

    /// THE DRAFT UNDER THE KEYS.
    // WL-PED-07 -- agents/workshop/pane-manager.md
    Row* editing_row() {
        if (pane_editor_has_keyboard(session_)) {
            if (Row* mine = pane_editor_editing_row()) {
                return mine;
            }
        }
        for (Row& r : session_.rows) {
            if (r.editing()) {
                return &r;
            }
        }
        return nullptr;
    }


    /// Which of this weave's own editable places a consumed paste request came from
    /// `kNone` for every armless branch.
    // WL-TEXT-09 -- agents/workshop/text-box.md
    enum class PasteOwner : std::uint8_t { kNone, kTerminal, kNaming, kDraft, kEditor };

    /// THE ONE-LINE NAME EDITOR THAT IS OPEN, or nothing -- the layout's or the Pane
    /// Creator's.
    // WL-MAKER-11 -- agents/workshop/maker-pane.md; WL-TEXT-09 -- agents/workshop/text-box.md
    component::TextBox* naming_line() {
        if (session_.setup.naming.open) {
            return &session_.setup.naming.line;
        }
        if (session_.pane_naming.open) {
            return &session_.pane_naming.line;
        }
        return nullptr;
    }

    /// WHICH DRAFT WOULD THE CHAIN HAVE HANDED THE CLIPBOARD TO? A projection of the one
    /// resolved context rather than a second spelling of the routing, which closes the way
    /// two spellings could deliver a paste to a draft the keys never reached.
    // WL-KEY-03 -- agents/workshop/keyboard.md; WL-TEXT-09 -- agents/workshop/text-box.md
    PasteOwner paste_owner_now() {
        switch (keyboard_context(session_)) {
        case KeyContext::kTerminal: return PasteOwner::kTerminal;
        case KeyContext::kNaming:
        case KeyContext::kPaneNaming: return PasteOwner::kNaming;
        case KeyContext::kDraft: return PasteOwner::kDraft;
        case KeyContext::kEditor: return PasteOwner::kEditor;
        default: return PasteOwner::kNone;
        }
    }

    /// ONE PASTE STILL IN FLIGHT: the conversation (by the book's own id) and the draft it
    /// belongs to.
    // WL-EDIT-11 -- agents/workshop/editor.md; WL-TEXT-09 -- agents/workshop/text-box.md
    struct PendingPaste {
        std::uint64_t ask = 0;
        PasteOwner owner = PasteOwner::kNone;
        std::uint64_t epoch = 0;
        std::int64_t object = 0;
        std::string label;
        /// THE SOURCE EDITOR'S OWN IDENTITY PAIR, meaningful only for `kEditor`: which
        /// document was open, and exactly where it stood.
        // WL-EDIT-11 -- agents/workshop/editor.md
        std::uint64_t editor_doc = 0;
        std::uint64_t editor_revision = 0;
    };

    /// OPEN THE CLIPBOARD CONVERSATION A CONSUMED PASTE REQUEST ASKED FOR.
    // WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
    void begin_clipboard_paste(loom::Mail& mail) {
        PendingPaste p;
        p.owner = paste_owner_now();
        switch (p.owner) {
        case PasteOwner::kNone: return; // no box of this weave's asked; nothing to do
        case PasteOwner::kTerminal: p.epoch = session_.terminal.input.draft_epoch(); break;
        case PasteOwner::kNaming: {
            const component::TextBox* line = naming_line();
            if (line == nullptr) {
                return; // unreachable while the resolver holds; written anyway
            }
            p.epoch = line->draft_epoch();
            break;
        }
        case PasteOwner::kEditor:
            p.editor_doc = session_.editor.doc_epoch;
            p.editor_revision = session_.editor.buffer.revision();
            break;
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
    /// than text.
    // WL-TEXT-02 -- agents/workshop/text-box.md
    void editing_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        Row* row = editing_row();
        // A PANE EDITOR ROW'S COMMIT OWES A RESEAT. Its write closure spent the
        // setup door; what a place write also changes is the SEATING -- an authored place
        // leaves the reactive stack and every reactive pane below it moves up a slot --
        // and `apply_setup` is the one path that reconciles it, exactly as it is for the
        // arrangement's `arrange_place`. Asked once, here, for every accepted commit, so no
        // write closure has to know which of the four axes it was.
        const bool pane_row = row != nullptr && pane_editor_has_keyboard(session_) &&
                              pane_editor_editing_row() == row;
        // THE DRAFT'S OWN VOCABULARY FIRST. One call owns what four switches used
        // to spell separately — the six editing keys, and now selection, clipboard, word
        // movement and history behind them — and a `true` is bool: the gesture
        // reached the layer that owns what it means, whether or not anything changed. The
        // component's vocabulary outranks the application keymap INSIDE a text context,
        // deliberately (owner-first refusal): a maker who remaps a draft control onto an
        // editing chord has authored a binding the box will answer first, and the hotkey
        // view shows both rows. What is left below is exactly the policy: what a draft
        // MEANS when a maker commits or abandons it, which the component is deliberately
        // unable to know -- resolved through the keymap executed here as
        // ever.
        if (row->consume(k.scancode, k.modifiers, session_.clipboard)) {
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kDraft, k.scancode, k.modifiers)) {
        case Act::kDraftCommit: {
            const Commit result = row->commit();
            if (result == Commit::Accepted) {
                if (pane_row) {
                    apply_setup(mail);
                }
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

    /// THE ONE PLACE THE PROPERTY DRAFT'S HORIZONTAL WINDOW IS RECONCILED.
    // WL-INFO-01, WL-INFO-05, WL-INFO-06 -- agents/workshop/info-body.md
    void refresh_inspector() {
        const Screen sc = screen_of(session_);
        // THE PANE EDITOR'S DRAFT FIRST, against ITS body's capacity -- the same
        // one measurer, one pane over; a closed Pane Editor is skipped, not a zero.
        const PanelBounds editor =
            bounds_of(session_.panels, session_.setup.active, panel::kPaneEditor, sc);
        if (editor.open) {
            if (Row* mine = pane_editor_editing_row()) {
                const PaneEditorBodyPlace body = pane_editor_body(session_, sc, editor.rect);
                if (body.present) {
                    mine->keep_caret_visible(body.value_columns);
                }
            }
        }
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

    /// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK, AND IF SO SELECT THE WORD.
    // WL-PTR-02, WL-PTR-03 -- agents/workshop/pointer.md
    bool press_selects_word(std::int64_t modifiers, std::int64_t place,
                            component::TextBox& box, std::size_t at) {
        if (modifiers != zengine::input::mod::kNone) {
            session_.click = ClickMemory{};
            return false;
        }
        const component::WordSpan word = box.word_at(at);
        const std::uint64_t epoch = box.draft_epoch();
        const std::int64_t now = interaction_now();
        if (doubles_a_click(session_.click, place, epoch, word, now)) {
            // AND THE ARMING IS SPENT. A third press in the same place is an ordinary press
            // again: there is no triple-click in this application, and an arming that
            // survived its own gesture would make every press after a double-click select
            // the word once more.
            session_.click = ClickMemory{};
            return box.select_word_at(at);
        }
        session_.click = click_landed(place, epoch, word, now);
        return false;
    }

    /// The same question for a property row, which keeps its draft behind its own invariant
    /// -- so the mutation goes through `Row`'s door and the reading through `editor()`.
    bool press_selects_word(std::int64_t modifiers, Row& row, std::size_t at) {
        if (modifiers != zengine::input::mod::kNone) {
            session_.click = ClickMemory{};
            return false;
        }
        const component::WordSpan word = row.editor().word_at(at);
        const std::uint64_t epoch = row.editor().draft_epoch();
        const std::int64_t now = interaction_now();
        if (doubles_a_click(session_.click, text_drag_place::kPropertyDraft, epoch, word,
                            now)) {
            session_.click = ClickMemory{};
            return row.select_word_at(at);
        }
        session_.click = click_landed(text_drag_place::kPropertyDraft, epoch, word, now);
        return false;
    }

    /// A PRESS INSIDE THE ACTIVE PROPERTY EDITOR, and nothing else: the raw pointer fact,
    /// the resolved Info body, a prose row and column, a semantic property row, a column of
    /// its value, a byte of the draft, the caret. It is a place, not a mode, and begins nothing.
    // WL-PRESS-01, WL-PRESS-04 -- agents/workshop/press-chain.md
    // WL-PTR-02 -- agents/workshop/pointer.md
    // WL-INFO-01 -- agents/workshop/info-body.md
    bool info_press(const InfoBodyAt& where, std::int64_t modifiers) {
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
            // AND THE ROW'S OWN PROSE OFFSET COMES OFF FIRST. A body row carries the
            // mark and the property's name before the value, exactly as the pane's row
            // carries `> ` before the command, so a pressed column is a column of the ROW and
            // the value's column is that minus what the name spent. `property_value_column`
            // is the one subtraction and it is the inverse of the one
            // `property_caret_column` added.
            const std::size_t target =
                row.editor().position_at_column(property_value_column(where.at.column));
            //...AND A SECOND PRESS IN THE SAME WORD SELECTS IT. The first press is
            // still an ordinary press and still places the caret; only the second one means
            // something else, and it means it in the component's own word vocabulary.
            if (!press_selects_word(modifiers, row, target)) {
                row.place(target);
            }
            //...AND THE PRESS OPENS A SELECTION DRAG. The press placed the caret,
            // which is the anchor; every motion until release extends from it. The record
            // holds WHICH line and nothing else — the geometry is re-resolved per motion by
            // the same functions this press just spent, `PaneGesture`'s no-live-position law.
            session_.text_drag.active = true;
            session_.text_drag.place = text_drag_place::kPropertyDraft;
            return true; // consumed: the press was on the draft's own row
        }
        return false; // no draft is live, so this panel has no editor to press
    }

    /// A PRESS ON AN ACTION CONTROL PERFORMS THE ACT THE CONTROL NAMES.
    // WL-CTRL-03, WL-CTRL-05 -- agents/workshop/info-controls.md
    // WL-PRESS-01 -- agents/workshop/press-chain.md
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

    /// A PRESS ON A VISIBLE OBJECT NAME SELECTS THAT OBJECT — in command mode, and only there.
    // WL-INFO-01, WL-INFO-09 -- agents/workshop/info-body.md
    // WL-PRESS-01, WL-PRESS-02 -- agents/workshop/press-chain.md
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

    /// Open or close the pane. The whole of the mode change.
    // WL-TERM-01 -- agents/workshop/terminal.md
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
    // WL-TERM-01, WL-TERM-05 -- agents/workshop/terminal.md
    // WL-TEXT-02, WL-TEXT-04 -- agents/workshop/text-box.md
    void terminal_key(const zengine::input::KeyPressed& k) {
        TerminalPane& pane = session_.terminal;
        // THE LINE'S OWN VOCABULARY FIRST: the six editing keys this switch used
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

    /// A PRESS INSIDE THE TERMINAL MODE — the first place-within-a-mode.
    // WL-TERM-01, WL-TERM-09 -- agents/workshop/terminal.md
    // WL-GEO-01 -- agents/workshop/geometry.md
    // WL-PTR-02 -- agents/workshop/pointer.md
    // WL-PRESS-02 -- agents/workshop/press-chain.md
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
            // THROUGH THE WINDOW THE ROW WAS DRAWN WITH. A visible column names
            // `first_visible + offset` of the WHOLE authored line, never the offset alone --
            // that is the one subtraction a horizontal viewport adds to a hit test, and
            // leaving it out is right for exactly as long as no line is long enough to
            // scroll. The offset read here is the one the last repaint resolved, which is
            // the one the maker is looking at.
            const std::size_t target = terminal_caret_of_column(place, pane.input, at.column);
            //...AND A SECOND PRESS IN THE SAME WORD SELECTS IT, `info_press`'s twin
            // over the other instance of one component: the first press still places the
            // caret and still means exactly what it always did.
            const bool word = press_selects_word(b.modifiers, text_drag_place::kTerminalLine,
                                                 pane.input, target);
            if (!word) {
                pane.input.place(target);
            }
            //...AND THE PRESS OPENS A SELECTION DRAG, `info_press`'s twin: the
            // caret just placed is the anchor, and motion until release extends from it. A
            // press that selected a WORD opens one too -- the anchor is the word's start, so
            // dragging from it extends the selection rather than replacing it, which is what
            // the component's own anchor/caret split already meant.
            session_.text_drag.active = true;
            session_.text_drag.place = text_drag_place::kTerminalLine;
            // The caret moving is what changes whether completion may be asked, so a press
            // that moved it has to reach `refresh_terminal` exactly as a caret key does. A
            // press that COLLAPSED a selection changed the picture too, even where the
            // caret stood still — the highlight has to leave the screen.
            if (word || pane.input.caret() != was || had_selection) {
                refresh_terminal();
                return true;
            }
            return false;
        }
        return false; // inside the mode, on none of its regions: consumed, and nothing moved
    }

    /// IS THERE A LIST ON SCREEN WITH SOMETHING IN IT TO CHOOSE?
    // WL-TERM-05 -- agents/workshop/terminal.md
    bool completion_selectable() const {
        const TerminalPane& pane = session_.terminal;
        return pane.open && pane.completion.open && !pane.dismissed &&
               !pane.completion.candidates.empty();
    }

    /// Move the selection, and stop at the ends.
    // WL-TERM-05 -- agents/workshop/terminal.md
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
    // WL-TERM-05 -- agents/workshop/terminal.md
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
    // WL-TERM-02, WL-TERM-04 -- agents/workshop/terminal.md
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
        // THE VERB TABLE IS THE COMPLETER'S TOO (complete.hpp). It used to be
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
        // wrapping it was fitted into one 56-cell row and a maker asking how to send a message got
        // `this pane speaks two verbs: \`send <addr> <Shape> <ver...` -- the answer truncated
        // at exactly the point it started being an answer.
        me.record_notice("this pane speaks two verbs, and `ask` takes the same form as `send`: "
                         "send <addr> <Shape> <version> [args] -- an address is #12 for one "
                         "weave, @office for whoever holds a role, or * for everyone");
    }

    /// Take the pane's snapshot of the participant.
    // WL-TERM-03, WL-TERM-08 -- agents/workshop/terminal.md
    // WL-TEXT-04 -- agents/workshop/text-box.md
    void refresh_terminal() {
        TerminalPane& pane = session_.terminal;
        const Screen sc = screen_of(session_);
        // THE ONE PLACE THE INPUT LINE'S HORIZONTAL WINDOW IS RECONCILED.
        //
        // It is here because this function runs on EVERY repaint -- which is the property
        // completion met as a trap and this needs as a guarantee. The window has to follow the
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
        //. completer rests on an assumption that was free when the caret could
        // not move: the token being completed is the LAST one, so accepting is "drop what
        // has been typed of this token, append what it was going to be". With a caret in the
        // middle that edit would delete everything after it. The two honest repairs are to
        // teach the completer about a token under an arbitrary caret -- a second parser, on
        // a phase about carets and pointers -- or to say plainly that completion follows the
        // end of the line.
        //
        // AND IT SAYS IT OUT LOUD RATHER THAN GOING QUIET, which is own measured rule
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
        // many entries as it has rows": a line too long for the pane WRAPS rather
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
    // WL-KEY-01 -- agents/workshop/keyboard.md; WL-PANE-12 -- agents/workshop/panes-and-windows.md
    void command(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        // EVERY ARM CALLS THE OPERATION IT ALWAYS CALLED; what the keymap changed is only
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
        // BUILDING AND REALIZING stay two deliberate halves: realizing an
        // artifact is the one Builder gesture that changes what is running, and its
        // default is the chorded sibling of the plain build's.
        case Act::kBuild: build_now(mail, false); break;
        case Act::kBuildRealize: build_now(mail, true); break;
        case Act::kRecipeNext: choose_recipe(+1, mail); break;
        case Act::kRecipeBack: choose_recipe(-1, mail); break;
        case Act::kBuildFrontier: build_frontier(mail); break;
        // EDIT THE SOURCE the chosen recipe names -- the Builder-owned door into the
        // source editor, unbound without a Builder panel exactly as `b` is.
        case Act::kEditSource: edit_source(mail); break;
        // The editor's deliberate discard, reachable from command mode too so the quit
        // refusal names a gesture that works where the maker is standing.
        case Act::kEditorDiscard: discard_source_edits(); break;
        // THE TWO SETUP GESTURES: ordinary maker commands beside `+ panel`,
        // deliberately not another `^`-pair beside the document's. they are
        // both FILE operations and nothing else -- `s` writes, `r` reads, and naming a
        // layout is `layout.rename`'s.
        case Act::kSetupSave: save_setup(); break;
        case Act::kSetupRestore: restore_setup(mail); break;
        // THE LAYOUT SHELF: four ordinary command-mode gestures over the run of
        // desk arrangements this Workshop is holding. Stepping is over the WHOLE
        // population, painted or not, which is what keeps the band's derived tab window a
        // presentation rather than a bound on what a maker can reach.
        //
        // THE FOUR THAT TAKE A POSITION REACH THE KEYBOARD ONLY THROUGH THE LIVE LAYOUT,
        // and that is deliberate: `^w` closes the layout a maker is standing on, and the
        // rest are the contextual menu's, on the tab a maker pointed at. A keyboard with
        // no captured subject can truthfully name one layout, which is the live one --
        // `open_context_ambient`'s own rule about panes, one surface over.
        case Act::kLayoutNext: step_layout(+1, mail); break;
        case Act::kLayoutPrevious: step_layout(-1, mail); break;
        case Act::kLayoutNew: new_layout(mail); break;
        case Act::kLayoutRemove: drop_layout(session_.setup.active_at, mail); break;
        case Act::kLayoutRename: open_layout_rename(session_.setup.active_at); break;
        case Act::kLayoutDuplicate:
            duplicate_layout(session_.setup.active_at, mail);
            break;
        case Act::kLayoutMoveLeft: shift_layout(session_.setup.active_at, -1); break;
        case Act::kLayoutMoveRight: shift_layout(session_.setup.active_at, +1); break;
        // ARRANGE THE DESK (a mode, rescoped to the desk): a printable trigger pays
        // the swallow rule -- armed centrally from the binding -- and buys a
        // mode whose own keys need no modifier at all (P48).
        case Act::kArrangeDesk: open_arrange_desk(); break;
        // WHAT CAN I DO WITH THIS? -- the contextual-action surface, on the subject
        // command mode can truthfully name.
        case Act::kContextOpen: open_context_ambient(); break;
        // PANE TITLES ARE A PRESENTATION PREFERENCE WITH A KEY. The flip is one
        // session bit; everything it changes on screen -- the arrangeable panes' header
        // rows, the row returned to or taken back from each provider's budget -- follows
        // from the ordinary repaint this keystroke already earns (`refresh_external_rooms`
        // re-grants exactly the rooms whose capacity moved). The notice says which state
        // the toggle landed in, because a maker with no external pane open would otherwise
        // watch nothing change; its second half names the one exception, which is the
        // keyboard-identity law, not a courtesy.
        //
        // AND THE PREFERENCE IS DURABLE: a toggle is the maker STATING it, so
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
    // WL-PANE-12 -- agents/workshop/panes-and-windows.md

    /// THE PICKER'S POPULATION — the shared recovery inventory, and there is exactly one of
    /// it.
    // WL-PANE-12 -- agents/workshop/panes-and-windows.md
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

    /// STEP THE PICKER'S CURSOR, BOUNDED BY THE PAINTED POPULATION.
    // WL-PANE-12 -- agents/workshop/panes-and-windows.md
    void picker_move(std::int64_t by) {
        PanelPicker& picker = session_.panels.picker;
        const std::size_t total = picker_population().size();
        if (total == 0) {
            picker.cursor = 0;
            return;
        }
        if (picker.cursor >= total) {
            picker.cursor = total - 1;
        }
        if (by < 0) {
            const std::size_t up = static_cast<std::size_t>(-by);
            picker.cursor = picker.cursor > up ? picker.cursor - up : 0;
        } else {
            const std::size_t down = static_cast<std::size_t>(by);
            picker.cursor = picker.cursor + down < total ? picker.cursor + down : total - 1;
        }
    }

    /// THE WHEEL OVER THE PICKER MOVES ITS CURSOR.
    // WL-EDIT-10 -- agents/workshop/editor.md
    void picker_wheel(const zengine::input::PointerWheel& w, loom::Mail& mail) {
        PanelPicker& picker = session_.panels.picker;
        const std::int64_t rows = spend_wheel(picker.wheel_accum, w.dy, kListWheelRows);
        if (rows == 0) {
            return;
        }
        const std::size_t was = picker.cursor;
        picker_move(-rows);
        if (picker.cursor == was) {
            return; // already at the edge: nothing moved, nothing repaints
        }
        repaint(mail);
    }

    /// The picker's keys. Escape and `p` both close it: the key that opened it
    /// closes it, the terminal overlay's rule, and Escape closes it too because
    /// a maker who has changed their mind should not have to remember which of
    /// the two ways out this particular thing has.
    void picker_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        PanelPicker& picker = session_.panels.picker;
        // THE CURSOR IS REPAIRED THROUGH THE POPULATION'S OWN OWNER, BEFORE ANYTHING INDEXES
        // IT. The list can shrink under an open picker -- a provider going away
        // takes its runtime rows with it -- and every question below is about a row.
        const std::size_t population = picker_population().size();
        if (picker.cursor >= population) {
            picker.cursor = population == 0 ? 0 : population - 1;
        }
        switch (session_.keymap.action_for(KeyContext::kPicker, k.scancode, k.modifiers)) {
        case Act::kPickerUp: picker_move(-1); break;
        case Act::kPickerDown: picker_move(+1); break;
        case Act::kPickerChoose: choose_panel(mail); break;
        case Act::kPickerClose:
            picker.open = false;
            say("no panel opened or removed", false);
            break;
        default:
            // THE KEY THAT OPENED IT CLOSES IT -- the terminal overlay's rule, and since
            // the keymap it follows the OPENER'S effective binding wherever the maker moved
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
    // WL-PED-05 -- agents/workshop/pane-manager.md
    // WL-PANE-12 -- agents/workshop/panes-and-windows.md
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
        toggle_participation(rows[picker.cursor], hotkey(Act::kPicker), mail);
    }

    /// OPEN A CLOSED PANE, OR REMOVE AN OPEN ONE -- the picker's own two cases, as the ONE
    /// membership door two consumers spend.
    // WL-PED-05 -- agents/workshop/pane-manager.md
    // WL-PANE-12 -- agents/workshop/panes-and-windows.md
    void toggle_participation(const CatalogRow& chosen, const std::string& again,
                              loom::Mail& mail) {
        const PaneRef ref = chosen.ref;
        if (remove_pane(session_.setup.active, ref)) {
            // A REMOVAL WORKS ON A WAITING ROW EXACTLY AS ON AN OPEN ONE. The maker
            // authored the intent; whether this screen currently has room to seat it is
            // Workshop's problem and not a reason to make the intent unremovable.
            apply_setup(mail);
            // WHAT IT WAS PRESENTING IS UNTOUCHED, and one sentence covers both
            // kinds because it is the same sentence: the Builder tool keeps its
            // target, its history and its running count of asks; the document
            // keeps every object, the selection and the inspector's rows. A
            // panel is a presentation, and removing one removes a presentation.
            say("removed " + chosen.name + " -- " + again +
                    " brings it back; nothing behind it was touched",
                false);
            return;
        }
        // A NEW ROW IS REFUSED BEFORE THE SETUP MOVES IF IT COULD NOT BE SEATED.
        // The order is the whole of the guarantee: the capacity question is asked against
        // the setup this gesture WOULD produce, and the active setup is left untouched when
        // the answer is no. Adding first and letting `reconcile` drop it into `waiting`
        // would leave a maker with an authored pane they never saw and did not knowingly
        // author, which is a picker that edits a file behind its own refusal.
        Setup candidate = session_.setup.active;
        (void)add_pane(candidate, ref);
        const Seating trial = seat_panes(candidate, session_.panels,
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
        say(std::string("opened ") + chosen.name + " -- " + again + " removes it",
            false);
    }

    // ---- The setup: name it, save it, restore it ------------------------------

    /// MAKE THE OPEN PANELS BE WHAT THE ACTIVE SETUP SAYS -- the one owner, and
    /// the only thing in this file that opens or closes a panel.
    // WL-LAYOUT-05, WL-LAYOUT-07 -- agents/workshop/layouts.md
    // WL-ARR-03 -- agents/workshop/arrangement.md
    // WL-MAKER-09 -- agents/workshop/maker-pane.md
    // WL-PED-05 -- agents/workshop/pane-manager.md
    // WL-SESSION-12 -- agents/workshop/session.md
    void apply_setup(loom::Mail& mail) {
        // MEMBERSHIP-DEPENDENT SESSION STATE FIRST. This is the one door a setup's
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
            } else if (kind == panel::kProjectFiles) {
                // A BROWSER THAT HAS JUST BECOME PRESENT LOOKS. This is the Builder's own
                // arm one kind over -- a pane that opens asks its subject what is true now
                // -- and it is where "the listing is a snapshot" becomes usable: reopening
                // the pane is a maker's way of asking for a fresh one, and it costs the
                // same walk as pressing refresh.
                files_refresh();
            }
        }
    }

    /// OPEN THE ONE-LINE NAME EDITOR ON THE LAYOUT AT `at`.
    // WL-CTX-07 -- agents/workshop/contextual.md
    // WL-LAYOUT-10 -- agents/workshop/layouts.md
    void open_layout_rename(std::size_t at) {
        if (at >= layout_count(session_.setup)) {
            return; // the belt: a captured position the run no longer holds
        }
        LayoutNaming& naming = session_.setup.naming;
        naming.open = true;
        naming.at = at;
        const std::string& name = layout_at(session_.setup, at).name;
        naming.line.set(name, name.size());
        say("rename this layout -- " + hotkey(Act::kNamingCommit) + " renames it, " +
                hotkey(Act::kNamingCancel) + " cancels",
            false);
    }

    /// The name editor's keys. Return commits the rename; Escape cancels and
    /// changes nothing; the rest is the ordinary editing of one line, through
    /// the component that owns the text, the caret and the window together.
    void naming_key(const zengine::input::KeyPressed& k, loom::Mail&) {
        LayoutNaming& naming = session_.setup.naming;
        // The line's own vocabulary first — the third of the four switches the
        // component call collapsed. What stays is the policy pair every consumer keeps to
        // itself: what a committed name MEANS and what abandoning one leaves standing.
        if (naming.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kNaming, k.scancode, k.modifiers)) {
        case Act::kNamingCommit: commit_layout_rename(); break;
        case Act::kNamingCancel:
            close_naming();
            say("the layout name is unchanged", false);
            break;
        default: break;
        }
    }

    /// Close the editor whole: open, subject and line together, so a later open
    /// cannot inherit a stale position or a stale draft (`close_context`'s rule).
    void close_naming() {
        session_.setup.naming = LayoutNaming{};
    }

    /// TAKE THE TYPED NAME AND RENAME THE LAYOUT.
    // WL-LAYOUT-04, WL-LAYOUT-10 -- agents/workshop/layouts.md
    void commit_layout_rename() {
        LayoutNaming& naming = session_.setup.naming;
        const std::size_t at = naming.at;
        const std::string wanted = naming.line.text();
        const Written legal = check_setup_name(wanted);
        if (!legal.accepted) {
            say(legal.refusal + " -- " + hotkey(Act::kNamingCommit) + " tries again, " +
                    hotkey(Act::kNamingCancel) + " cancels",
                true);
            return;
        }
        if (at >= layout_count(session_.setup)) {
            close_naming();
            say("that layout is no longer here -- nothing was renamed", true);
            return;
        }
        Setup candidate = layout_at(session_.setup, at);
        candidate.name = wanted;
        const Written whole = check_setup(candidate);
        if (!whole.accepted) {
            say(whole.refusal, true);
            return;
        }
        rename_layout(session_.setup, at, wanted);
        close_naming();
        // NO `apply_setup`. A name is the one authored field of a desk that no
        // presentation reads: which panes participate, where they are and how big
        // they are have not moved, so there is nothing to reconcile. What DOES
        // change is the row this layout is painted on and, where the layout is
        // associated, whether it still matches its artifact -- both derived at the
        // next composition, from the value that just moved.
        say("renamed layout " + quoted_setup_name(wanted) + link_note(at), false);
    }

    /// WHAT TO SAY ABOUT A LAYOUT'S SETUP ASSOCIATION AFTER AN OPERATION
    /// -- nothing when there is none, and the artifact plus the verdict when there
    /// is.
    // WL-LAYOUT-02 -- agents/workshop/layouts.md
    std::string link_note(std::size_t at) const {
        const SetupLink& link = link_at(session_.setup, at);
        const std::int64_t status = link_status(layout_at(session_.setup, at), link);
        if (status == setup_link::kNone) {
            return {};
        }
        return std::string(" -- ") + link.path + " is " +
               (status == setup_link::kCurrent ? kSetupLinkCurrent : kSetupLinkModified);
    }

    /// WHICH ARTIFACT `s` AND `r` ACT ON FOR THE LIVE LAYOUT: its own
    /// association where it has one, and the host's configured setup path where it
    /// does not.
    // WL-LAYOUT-10 -- agents/workshop/layouts.md
    const std::string& setup_artifact() const {
        return session_.setup.active_link.path.empty() ? host_->setup_path
                                                       : session_.setup.active_link.path;
    }

    /// WRITE THE LIVE LAYOUT'S DESK TO ITS SETUP ARTIFACT (`s`).
    // WL-LAYOUT-09, WL-LAYOUT-10, WL-LAYOUT-11 -- agents/workshop/layouts.md
    void save_setup() {
        const std::string path = setup_artifact();
        if (path.empty()) {
            say(kNoSetupFile, true);
            return;
        }
        const Setup& desk = session_.setup.active;
        const Written whole = check_setup(desk);
        if (!whole.accepted) {
            say(whole.refusal, true);
            return;
        }
        const Written written = setup_persist::save_file(path, desk);
        if (!written.accepted) {
            // THE LAST GOOD SETUP FILE IS INTACT and so is every association: the
            // writer never opened the destination, and nothing below this line has
            // run.
            say(written.refusal, true);
            return;
        }
        session_.setup.active_link.path = path;
        adopt_known_setup(session_.setup, path, desk);
        say("saved setup " + quoted_setup_name(desk.name) + " to " + path +
                unresolved_note(desk),
            false);
    }

    /// RESTORE THE LIVE LAYOUT FROM ITS SETUP ARTIFACT (`r`).
    // WL-LAYOUT-09, WL-LAYOUT-10, WL-LAYOUT-11 -- agents/workshop/layouts.md
    void restore_setup(loom::Mail& mail) {
        const std::string path = setup_artifact();
        if (path.empty()) {
            say(kNoSetupFile, true);
            return;
        }
        const setup_persist::LoadedSetup loaded = setup_persist::load_file(path);
        if (!loaded.outcome.accepted) {
            say(loaded.outcome.refusal, true);
            return;
        }
        session_.setup.active = loaded.setup;
        session_.setup.active_link.path = path;
        adopt_known_setup(session_.setup, path, loaded.setup);
        apply_setup(mail);
        say("restored setup " + quoted_setup_name(loaded.setup.name) + " from " + path +
                unresolved_note(loaded.setup),
            false);
    }

    // ---- THE LAYOUT SHELF: several desks, one of them live --------------------

    /// WHICH LAYOUT IS LIVE AND WHERE IT SITS IN THE RUN, as one sentence.
    std::string layout_note() const {
        return "layout " + quoted_setup_name(session_.setup.active.name) + " -- " +
               std::to_string(session_.setup.active_at + 1) + " of " +
               std::to_string(layout_count(session_.setup));
    }

    /// MAKE THE LAYOUT AT `to` LIVE -- the one operation every switching gesture spends,
    /// keyboard and pointer alike, so a tab press and a stepping key cannot come to mean
    /// two different transactions.
    void switch_layout(std::size_t to, loom::Mail& mail) {
        if (!activate_layout(session_.setup, to)) {
            // NOTHING MOVED: the position is the live layout's own, or is not a layout.
            // Said rather than silent, because a maker who pressed the tab they are
            // already on has aimed at something and is owed the row's own answer.
            say(layout_note() + link_note(session_.setup.active_at), false);
            return;
        }
        apply_setup(mail);
        say(layout_note() + link_note(session_.setup.active_at) +
                unresolved_note(session_.setup.active),
            false);
    }

    /// STEP ONE ALONG THE RUN, wrapping -- over the WHOLE population, including the
    /// layouts the band's tab window had no room to paint. `by` is +1 or -1.
    void step_layout(std::int64_t by, loom::Mail& mail) {
        if (layout_count(session_.setup) <= 1) {
            say("this is the only layout -- " + hotkey(Act::kLayoutNew) + " makes another",
                false);
            return;
        }
        switch_layout(layout_step(session_.setup, by), mail);
    }

    /// What to say when the run is already as long as one Workshop keeps. One sentence,
    /// because both doors that can hit the ceiling deserve the same words.
    std::string layout_ceiling_note() const {
        return "that is the most layouts one Workshop keeps (" +
               std::to_string(kMaxLayouts) + ") -- " + hotkey(Act::kLayoutRemove) +
               " drops this one";
    }

    /// ONE MORE LAYOUT: A FRESH BLANK DESK, APPENDED, AND LIVE.
    // WL-LAYOUT-03 -- agents/workshop/layouts.md
    void new_layout(loom::Mail& mail) {
        if (!add_layout(session_.setup)) {
            say(layout_ceiling_note(), true);
            return;
        }
        apply_setup(mail);
        say("new " + layout_note() + " -- an empty desk", false);
    }

    /// COPY THE LAYOUT AT `at`, INSERT THE COPY AFTER IT, AND STAND ON THE COPY.
    // WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-04 -- agents/workshop/layouts.md
    void duplicate_layout(std::size_t at, loom::Mail& mail) {
        if (at >= layout_count(session_.setup)) {
            return; // the belt: a captured position the run no longer holds
        }
        if (!::zengine::workshop::duplicate_layout(session_.setup, at)) {
            say(layout_ceiling_note(), true);
            return;
        }
        apply_setup(mail);
        say("duplicated " + layout_note() + " -- the desk was copied, its setup file was not",
            false);
    }

    /// DROP THE LAYOUT AT `at` AND, WHERE IT WAS THE LIVE ONE, STAND ON A NEIGHBOUR.
    // WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-03 -- agents/workshop/layouts.md
    void drop_layout(std::size_t at, loom::Mail& mail) {
        if (at >= layout_count(session_.setup)) {
            return; // the belt: a captured position the run no longer holds
        }
        const bool was_live = at == session_.setup.active_at;
        const std::string gone = quoted_setup_name(layout_at(session_.setup, at).name);
        if (!remove_layout(session_.setup, at)) {
            say("this is the only layout -- Workshop always has one desk", true);
            return;
        }
        if (was_live) {
            apply_setup(mail);
        }
        say("removed layout " + gone + " -- now on " + layout_note(), false);
    }

    /// MOVE THE LAYOUT AT `at` ONE STEP ALONG THE MAKER'S ORDER.
    // WL-CTX-07 -- agents/workshop/contextual.md; WL-LAYOUT-04 -- agents/workshop/layouts.md
    void shift_layout(std::size_t at, std::int64_t by) {
        const std::size_t n = layout_count(session_.setup);
        if (at >= n) {
            return; // the belt: a captured position the run no longer holds
        }
        if ((by < 0 && at == 0) || (by > 0 && at + 1 == n)) {
            say("layout " + quoted_setup_name(layout_at(session_.setup, at).name) +
                    " is already at the " + (by < 0 ? "start" : "end") + " of the run",
                false);
            return;
        }
        const std::size_t to = by < 0 ? at - 1 : at + 1;
        const std::string moved = quoted_setup_name(layout_at(session_.setup, at).name);
        if (!move_layout(session_.setup, at, to)) {
            return;
        }
        say("moved layout " + moved + " to " + std::to_string(to + 1) + " of " +
                std::to_string(n),
            false);
    }

    /// WHAT TO SAY ABOUT THE PANES THIS BUILD COULD NOT PRESENT -- nothing when
    /// there are none, and the first one BY NAME when there are.
    // WL-PANE-10 -- agents/workshop/panes-and-windows.md
    std::string unresolved_note(const Setup& s) const {
        const std::vector<PaneRef> waiting = unresolved_panes(s, session_.panels);
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

    // ---- THE LAST SESSION: the desk that comes back on its own ----------------

    /// TAKE BACK THE DESK AND THE ROOM THIS WORKSHOP WAS LAST USED IN.
    // WL-SESSION-11, WL-SESSION-12, WL-SESSION-14, WL-SESSION-16, WL-SESSION-17 -- agents/workshop/session.md
    // WL-MAKER-09 -- agents/workshop/maker-pane.md
    // WL-MIG-10 -- agents/workshop/migration.md
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
        //...AND WHATEVER CONVERSIONS THIS RUN HAPPENS TO HAVE, which is a reading
        // taken at this instant and not a capability this weave holds: an older session file
        // is brought forward exactly when a live conversion says so, by the same catalog
        // that answers every other operator question in this process, and is refused in
        // words when nothing does.
        const session_persist::LoadedSession last =
            session_persist::load_file(host_->session_path, host_->conversions);
        if (!last.present) {
            // A FIRST LAUNCH IS NOT AN ERROR and must never be reported as one. It is also
            // the most common way this function ends, so it ends quietly.
            return;
        }
        if (!last.outcome.accepted) {
            // ⚠ AND THIS RUN WILL NOT WRITE OVER IT, which is the marks file's own
            // law taken for a sharper reason. Restraint on the READ path was always here --
            // Workshop does not rewrite a file it could not understand -- but the session is
            // a file Workshop WRITES on its way out, so without this flag an orderly close
            // would replace bytes this run could not read with this run's default desk.
            //
            // IT COSTS ALMOST NOTHING AND IT BUYS BACK A WHOLE VINTAGE. the most
            // likely reason a session is refused is that the conversion for it is not mounted
            // in THIS arrangement -- a condition a maker fixes by adding a row to a plan, in
            // a minute, on a file that has to still be there when they do.
            session_refused_ = true;
            say(last.outcome.refusal + " -- opening with the default setup", true);
            // THE NOTICE IS THE EVENT; THE CONDITION IS WHAT IS STILL TRUE. The sentence
            // above is about this launch and the next thing said replaces it; that this
            // Workshop is keeping no session, over a file that is still on disk, is true all
            // run and has a maker action -- which is what makes it a condition
            // (`kSessionWallKey`, the keymap/prefs/marks walls' own shape).
            session_.conditions.establish(
                Condition{kSessionWallKey, "session refused -- this run keeps no session",
                          last.outcome.refusal +
                              " (the file is left exactly as it is, and this run will not "
                              "write over it)",
                          surface::role::kAlert, std::string()});
            return;
        }
        // ---- THE VIEWPORT FIRST, AND THE ORDER IS THE WHOLE OF IT ------------
        //
        // `apply_setup` seats panes against `stack_capacity(screen_of(session_))`, so how
        // much of this desk can be PRESENTED at all is decided by how much room the screen
        // has. Reconciling first and resizing afterwards would seat the desk against a
        // viewport nobody asked for and leave whatever did not fit waiting for room that had
        // in fact been there the whole time.
        // THE MEDIUM'S OWN FACTS ARE HANDED BACK UNCHANGED -- the face metric and
        // the canvas's device unit. A restore replaces the ROOM, which is the
        // only thing the file remembers; what the medium said about its own units is this
        // run's and must survive the call rather than be reset to the character reading.
        if (last.honoured && adopt_screen(session_, last.viewport_w, last.viewport_h,
                                          session_.text_advance_px, session_.text_line_px,
                                          session_.cell_px)) {
            // The restored viewport IS the normal window's room -- the save wrote it from
            // exactly that -- so the remembered pair starts equal to it rather
            // than waiting for the first extent to arrive.
            session_.normal_w = session_.screen_w;
            session_.normal_h = session_.screen_h;
            // The resolved inspector row closes over the workspace extent, and the workspace
            // extent is exactly what just changed -- `on(SurfaceExtent)`'s reason, said at
            // startup.
            refocus_keeping_draft(state_, session_);
        }
        // ---- THE DESKTOP PLACEMENT, REMEMBERED AND OFFERED BACK --------------------
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
        // ---- ...AND THEN THE DESKS, INTO THE ROOM THEY ASKED FOR ---------------
        //
        // THE WHOLE RUN COMES BACK AND EXACTLY ONE OF IT IS LIFTED LIVE.
        // `install_layout_run` is `layout_run`'s inverse and lives beside it in
        // `setup.hpp`, so this weave never touches `shelved` or `active_at` by hand and
        // there is no second spelling of the lift to drift. The layouts that are not live
        // are VALUES: no panel is opened for one, no provider hears about one, and nothing
        // is reconciled against one -- which is why `apply_setup` below is still the one
        // membership door and still sees exactly one desk.
        //
        // IT CANNOT REFUSE HERE, and the reason is where the law is: an admitted session's
        // run is non-empty and its position is in range, because `session_persist` proved
        // both before this value existed. The bool is the TYPE's floor for callers that
        // have not.
        install_layout_run(session_.setup, last.layouts, last.active);
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
        // AND IT SAYS HOW MANY CAME BACK WHEN MORE THAN ONE DID. One layout is
        // the sentence this has always been and every migrated session is one, so the
        // clause appears exactly when there is more to say. It COUNTS FROM ONE because it
        // is prose about tabs a maker is looking at; the file's own `active` is a position
        // and is spoken from zero where a maker is reading the file (`session_persist`'s
        // refusals).
        std::string said =
            "reopened your last desk " +
            quoted_setup_name(session_.setup.active.name);
        if (last.layouts.size() > 1) {
            said += " (" + std::to_string(last.active + 1) + " of " +
                    std::to_string(last.layouts.size()) + " layouts)";
        }
        said += " -- " + std::to_string(session_.screen_w) + "x" +
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
    // WL-SESSION-13, WL-SESSION-15, WL-SESSION-16 -- agents/workshop/session.md
    // WL-MIG-10 -- agents/workshop/migration.md
    void save_last_session() {
        if (host_->session_path.empty() || session_refused_) {
            return;
        }
        // THE VIEWPORT WRITTEN IS THE NORMAL WINDOW'S: `normal_w/h` tracks the
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
        // THE WHOLE RUN, IN MAKER ORDER, WITH THE POSITION THEY ARE ACTUALLY STANDING IN
        //. `layout_run` puts the lifted value back where it sits and answers with
        // a NEW vector, so saving cannot reorder what it is saving; and the position is
        // `active_at` rather than the end of the run, because `layout.new` leaving a maker
        // on the last layout is a habit and not a law.
        const Written written = session_persist::save_file(
            host_->session_path, layout_run(session_.setup), session_.setup.active_at,
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

    // ---- PANE MANAGEMENT: arrange the windows, and never lose one -------------

    /// THE ROWS A MAKER MAY ARRANGE: the shared inventory, restricted to what the setup
    /// names.
    // WL-ARR-08 -- agents/workshop/arrangement.md
    // WL-PANE-12 -- agents/workshop/panes-and-windows.md
    std::vector<PaneRef> arrangeable() const {
        std::vector<PaneRef> out;
        for (const CatalogRow& row : inventory_rows(session_.setup.active, session_.panels)) {
            if (has_pane(session_.setup.active, row.ref)) {
                out.push_back(row.ref);
            }
        }
        return out;
    }

    /// ARRANGE THE DESK: the global arrangement scope.
    // WL-ARR-07 -- agents/workshop/arrangement.md
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

    /// ARRANGE ONE PANE: the pane-local scope, on an explicit target -- the
    /// context menu's captured subject, or the desk's keyboard target. ADMISSION PRECEDES
    // WL-ARR-07, WL-ARR-09 -- agents/workshop/arrangement.md
    // WL-CTX-02 -- agents/workshop/contextual.md
    // WL-FRONT-04 -- agents/workshop/planes.md
    // WL-PRESS-06 -- agents/workshop/press-chain.md
    void enter_arrange_pane(const PaneRef& ref) {
        const Written ready = arrange_geometry_ready(ref);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        // ARRANGING A PANE IS CHOOSING IT, AND IT SPENDS THE SELECTION THAT ALREADY
        // EXISTS. A maker who says "arrange this one" has identified the thing they are
        // working with as surely as a press into it does, so the same `Panels::selected`
        // truth an ordinary press writes is written here -- and everything the lift derives
        // from that truth follows with nothing added: the selected chrome, the temporary
        // foreground lift in `effective_pane_order`, the paint order, the hit order. There
        // is no arrangement-specific z-order, no second foreground fact and no `front` rank
        // touched; `manage.front` is still the only way to say "and I mean this
        // permanently", and a save straight after this writes the desk it always would.
        //
        // AFTER ADMISSION, NEVER BEFORE. A pane that cannot be arranged leaves the maker
        // exactly where they were (rule for this door), and that has to include the
        // selection: a refusal that had silently re-selected something would have moved the
        // desk while saying it changed nothing.
        //
        // THE KEYBOARD CANDIDATE IS NOT TOUCHED, and the two are deliberately not collapsed
        // merely because they happen together. Arrangement is a keyboard CONTEXT of its own
        // (`KeyContext::kArrangePane`), sitting above any pane's claim on the keys; where
        // those keys go when the maker LEAVES is a separate fact with a separate owner, and
        // an entrance that quietly re-pointed it would hand the keyboard somewhere new for
        // reasons the maker never stated.
        //
        // ...AND THE RESOLUTION IS FRESH, `bounds_of`'s discipline: admission proved this
        // reference names a pane a moment ago, and asking again is cheaper than carrying an
        // answer that could have been a different pane's.
        const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
        if (kind.has_value()) {
            session_.panels.selected = *kind;
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

    /// THE ACTIVE SETUP NO LONGER NAMES THE ADDRESSED PANE, SO NOTHING DOES.
    // WL-ARR-03 -- agents/workshop/arrangement.md; WL-PED-03 -- agents/workshop/pane-manager.md
    void forget_removed_selection() {
        // ⚠ THE PANE EDITOR'S SUBJECT IS DELIBERATELY NOT REPAIRED HERE. The
        // arrangement's address is a claim about a pane ON THE DESK, so a removal ends it;
        // the editor's subject is an IDENTITY a maker asked to be described, and a pane
        // that just left the layout is exactly the pane that now reads `closed -- open it`
        // -- clearing it would make "remove, look, reopen" impossible from the one surface
        // built for it. Its one clearing rule is `repair_pane_editor_subject`.
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
    /// moved -- and since the roster panel retired it carries the pane's STATE
    // WL-GEO-11, WL-GEO-12 -- agents/workshop/geometry.md
    // WL-ARR-09 -- agents/workshop/arrangement.md
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
        std::string text = "arrange " + ref_text(a.pane) + " (" + state + ") -- " +
                           pane_window_text(row, session_.cell_px);
        if (pane_window_partly_default(row)) {
            const FineRect now = managed_bounds().resolved;
            if (now.w > 0 && now.h > 0) {
                text += " -- now " + fine_rect_text(now, session_.cell_px);
            }
        }
        return text;
    }

    /// MOVE THE KEYBOARD'S TARGET BY ONE ROW, wrapping.
    // WL-ARR-08 -- agents/workshop/arrangement.md
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
    // WL-ARR-07 -- agents/workshop/arrangement.md
    // WL-CTX-02 -- agents/workshop/contextual.md
    // WL-PANE-08 -- agents/workshop/panes-and-windows.md
    // WL-SETUP-06 -- agents/workshop/setup-file.md
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
        const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
        if (!kind.has_value()) {
            return Written::no(ref_text(ref) +
                               " is unresolved -- its place and size cannot be measured; "
                               "0 resets it and f/b/r/l still order it");
        }
        // A UNIT OUTRANKS A RESERVATION, the same precedence `pane_state_of` spends between
        // a unit and a want of room. Both sentences are true of a fixed pane
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
    // WL-ARR-04 -- agents/workshop/arrangement.md; WL-GEO-12 -- agents/workshop/geometry.md
    PanelBounds managed_bounds() const {
        const std::optional<std::int64_t> kind =
            resolve_pane(session_.arrange.pane, session_.panels);
        if (!kind.has_value()) {
            return PanelBounds{};
        }
        return bounds_of(session_.panels, session_.setup.active, *kind, screen_of(session_));
    }

    /// THE WINDOW A GESTURE MEASURES FROM: authored where authored, resolved where
    /// reactive — the RESOLVED window, never the visible one (see `managed_bounds`).
    // WL-ARR-04 -- agents/workshop/arrangement.md; WL-PED-05 -- agents/workshop/pane-manager.md
    FineRect managed_window_base() {
        // ONE READING FOR THE HAND AND FOR THE TYPED VALUE: `pane_window_base`
        // (screen.hpp) is this function's old body, quarried out so the Pane Editor's
        // per-axis writes measure the axis they did not type from the same window the
        // arrangement's gestures measure from.
        return pane_window_base(session_, session_.arrange.pane);
    }

    /// AUTHOR AN ABSOLUTE PLACE. `x`/`y` are the whole proposal, saturated by the caller.
    // WL-ARR-06 -- agents/workshop/arrangement.md
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
    /// place yet, then the delta.
    // WL-ARR-08 -- agents/workshop/arrangement.md
    void arrange_nudge(std::int64_t dx, std::int64_t dy, loom::Mail& mail) {
        const Written ready = arrange_geometry_ready(session_.arrange.pane);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        // THE RESOLVED CORNER, NEVER THE CLIPPED ONE -- see `managed_bounds`.
        const FineRect from = managed_window_base();
        arrange_place(detail::step(from.x, dx * surface::kCellSubs),
                      detail::step(from.y, dy * surface::kCellSubs), mail);
    }

    /// AUTHOR WHAT ONE RESIZE GESTURE PROPOSES — the whole window, in sub-units, split into its two axes.
    // WL-ARR-05, WL-ARR-06 -- agents/workshop/arrangement.md
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
    // WL-ARR-08, WL-ARR-10 -- agents/workshop/arrangement.md
    void arrange_grow(std::int64_t dx, std::int64_t dy, loom::Mail& mail) {
        const Written ready = arrange_geometry_ready(session_.arrange.pane);
        if (!ready.accepted) {
            say(ready.refusal, true);
            return;
        }
        // THE RESOLVED WINDOW, NEVER THE VISIBLE ONE -- see `managed_bounds`.
        const FineRect base = managed_window_base();
        arrange_resize(pane_edge::kBottomRight, base.x, base.y, base.w, base.h,
                       dx * surface::kCellSubs, dy * surface::kCellSubs, mail);
    }

    /// ONE PANE ACTION, PERFORMED ON AN EXPLICIT TARGET -- the one place a targeted pane
    /// operation is spent, whatever asked for it.
    // WL-CTX-07 -- agents/workshop/contextual.md; WL-PED-05 -- agents/workshop/pane-manager.md
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
            say(ref_text(ref) + " " + what + " -- " +
                    pane_window_text(pane_of(s, ref), session_.cell_px),
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
                    pane_window_text(pane_of(s, ref), session_.cell_px),
                false);
            return;
        }
        // REMOVE THIS PANE. The picker's own semantics through the picker's own
        // door: the intent leaves the setup, `apply_setup` is what closes the
        // presentation, and what the pane was presenting is untouched -- a panel is a
        // presentation, and removing one removes a presentation. A removal works on a
        // waiting or unresolved row exactly as on an open one (rule).
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

    /// THE ARRANGEMENT KEYS -- one switch for both scopes and the reset prompt.
    // WL-ARR-08 -- agents/workshop/arrangement.md
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
        // -- and the coarse step, on both axes at once -----------------------------------
        //
        // THE SAME FUNCTION, A BIGGER DELTA. There is no second geometry owner here and
        // deliberately no second proposal: `arrange_grow` is the door a shifted arrow
        // already goes through, anchored bottom-right, so a coarse step cannot move the
        // pane, cannot move any other pane, and meets the identical per-axis settlement --
        // a shrink that would take the width below one cell keeps the width and still
        // shortens the height, refuse-never-clamp, per axis.
        case Act::kManageGrow:
            arrange_grow(+kCoarseStepCells, +kCoarseStepCells, mail);
            break;
        case Act::kManageShrink:
            arrange_grow(-kCoarseStepCells, -kCoarseStepCells, mail);
            break;
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
        // The prompt closes exactly when the reset REACHED its operation (the earlier
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
    // WL-ARR-01 -- agents/workshop/arrangement.md

    /// TAKE HOLD OF ONE PANE AT A POINTED POSITION -- the edge ring sizes, the body
    /// moves, and a press outside its rectangle is not this pane's. One function for both
    /// scopes, so the desk and the one-pane state cannot come to grab differently.
    // WL-ARR-01, WL-ARR-07 -- agents/workshop/arrangement.md
    // WL-PANE-01 -- agents/workshop/panes-and-windows.md
    bool take_pane_hold(const PaneRef& ref, const PointedAt& at, const Screen& sc) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, session_.panels);
        // A HAND MAY TAKE HOLD OF ANY PANE WHOSE PLACE IS THE MAKER'S TO AUTHOR.
        // This named the overlay stack while the stack was the only such place; saying it
        // as the exclusion (`place_is_authorable` -- the side column is the screen's) is
        // what keeps the hand and the KEYS agreeing, since the arrangement admission has
        // always refused by that same sentence.
        if (!kind.has_value() || !place_is_authorable(placement_of(*kind))) {
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
            // hand are -- AND THE BASE IS THE RESOLVED WINDOW: place beside
            // size because an anchored top or left pull authors both from
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

    /// A PRESS WHILE ARRANGING, and the two scopes answer it differently because
    /// they are ABOUT different things.
    // WL-ARR-07 -- agents/workshop/arrangement.md; WL-FRONT-05 -- agents/workshop/planes.md
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
            effective_pane_order(session_.setup.active, session_.panels);
        for (std::size_t i = order.size(); i > 0; --i) {
            const std::int64_t kind = order[i - 1];
            if (!bounds_of(session_.panels, session_.setup.active, kind, sc)
                     .rect.contains_at(at.sub.x, at.sub.y, at.grain)) {
                continue;
            }
            for (const SetupPane& row : session_.setup.active.panes) {
                const std::optional<std::int64_t> named =
                    resolve_pane(row.ref, session_.panels);
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
    // WL-ARR-01 -- agents/workshop/arrangement.md
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
    /// ----...AND IT NAMES ONE OF SEVERAL, AND MAY ASK FOR MORE --------------------
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
        // THE SENTENCE CHANGED WITH THE ASYNC BUILD AND THE CHANGE IS THE POINT. It used
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

    /// MOVE THE MAKER'S CURSOR THROUGH THE RECIPES THE TOOL PUBLISHED.
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
        // THE ONE WRITER OF `picked`: this gesture is what makes a selection the
        // MAKER's rather than the catalog's order wearing an index. The frontier action
        // reads it when several recipes produce one artifact.
        pane.picked = true;
        say("build recipe: " + pane.known.recipes[pane.chosen].recipe + " -> " +
                pane.known.recipes[pane.chosen].artifact,
            false);
        repaint(mail);
    }

    /// BUILD AND REALIZE THE ROW THE PROJECT IS WAITING ON.
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

    // ---- THE SOURCE EDITOR: choose source, edit, save, and never lose a byte ----------

    /// The editor's keys: the buffer's own vocabulary first, then the editor's policy.
    // WL-EDIT-02, WL-EDIT-09 -- agents/workshop/editor.md
    void editor_key(const zengine::input::KeyPressed& k) {
        EditorState& e = session_.editor;
        if (e.buffer.consume(k.scancode, k.modifiers, session_.clipboard)) {
            e.follow_caret = true;
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kEditor, k.scancode, k.modifiers)) {
        case Act::kEditorSave: save_source(); break;
        case Act::kEditorNewline:
            e.buffer.newline();
            e.follow_caret = true;
            break;
        case Act::kEditorTab:
            // A TAB BYTE, PRESERVED AS ONE -- the byte policy's insertion half. It
            // arrives as a key rather than as text because no backend delivers a
            // control byte as entered text (input's own law).
            e.buffer.type("\t");
            e.follow_caret = true;
            break;
        case Act::kEditorDiscard: discard_source_edits(); break;
        default: break;
        }
    }

    /// Text the maker typed into the source.
    // WL-EDIT-07 -- agents/workshop/editor.md
    void editor_text(const std::string& text) {
        if (text.empty()) {
            return;
        }
        if (!source_text_ok(text)) {
            say("that text holds bytes outside plain ASCII, which this editor cannot "
                "carry truthfully -- nothing was inserted",
                true);
            return;
        }
        session_.editor.buffer.type(text);
        session_.editor.follow_caret = true;
    }

    /// A press in the editor's body places the caret and begins the selection sweep.
    // WL-EDIT-08, WL-EDIT-12 -- agents/workshop/editor.md
    void editor_press(const zengine::input::PointerButton& b) {
        EditorState& e = session_.editor;
        if (!e.open_document()) {
            return; // consumed: a press into the empty editor is only a focus statement
        }
        const EditorPressAt at =
            editor_press_at(session_, screen_of(session_), b.space, b.x, b.y);
        if (!at.named) {
            return; // the header, or the strip below the last prose row: consumed, still
        }
        const std::size_t row = e.first_row + static_cast<std::size_t>(at.row);
        const std::size_t target =
            row < e.buffer.line_count() ? row : e.buffer.line_count() - 1;
        e.buffer.place(target,
                       byte_of_visual_col(e.buffer.line(target), e.first_col + at.column));
        session_.text_drag.active = true;
        session_.text_drag.place = text_drag_place::kEditorBody;
        e.follow_caret = true;
    }

    /// THE ONE PLACE THE EDITOR'S VIEWPORT IS RECONCILED -- `refresh_terminal`'s
    /// argument, two dimensions instead of one, on the same once-per-repaint path.
    void refresh_editor() { reconcile_editor_view(session_); }

    // ---- The filesystem browser ----------------------------------------------
    // WL-FILES-02 -- agents/workshop/files.md

    /// The directory the browser is showing. A FIELD READ: after the seed below, nothing
    /// here derives a location from the project anchor, which is what makes browsing
    /// structurally unable to move it.
    std::string files_dir() const { return session_.panels.files.current_dir; }

    /// GENERATE THIS RUN'S ORIGIN AND READ ITS DURABLE MARKS -- once, at the moment
    /// navigation first needs either.
    // WL-FILES-02 -- agents/workshop/files.md
    void ensure_marks() {
        if (session_.marks.settled) {
            return;
        }
        session_.marks.settled = true;
        session_.marks.origin = admit_location(host_->project_dir);
        load_marks(); // has its own once-guard: startup may have run it already
        if (session_.panels.files.current_dir.empty()) {
            session_.panels.files.current_dir = session_.marks.origin;
        }
    }

    /// READ THE MAKER'S OWN PLACES, OR STAND ON NONE.
    // WL-FILES-08 -- agents/workshop/files.md
    void load_marks() {
        if (marks_loaded_) {
            return;
        }
        marks_loaded_ = true;
        if (host_->marks_path.empty() || !std::filesystem::exists(host_->marks_path)) {
            return;
        }
        const marks_persist::LoadedMarks loaded = marks_persist::load_file(host_->marks_path);
        if (!loaded.outcome.accepted) {
            marks_refused_ = true;
            session_.conditions.establish(Condition{
                kMarksWallKey, "marks refused -- this run remembers no places",
                loaded.outcome.refusal + " (the file is left exactly as it is; fix or "
                                         "delete it)",
                surface::role::kAlert, std::string()});
            return;
        }
        session_.marks.maker = loaded.maker;
        if (!loaded.skipped.empty()) {
            // A SECOND STANDING CONDITION, AND A QUIETER ONE. The file was read and most of
            // it is in force; what is standing is that part of it is not, and that the next
            // mark this maker makes will write the list WITHOUT those rows. Saying it once
            // as an event would be saying it exactly where nobody could act on it.
            session_.conditions.establish(
                Condition{kMarksSkippedKey, "some marks could not be used",
                          loaded.skipped + " (fix the file before marking anything else, or "
                                           "those rows are dropped by the next save)",
                          surface::role::kAccent, std::string()});
        }
    }

    /// WRITE THE MAKER'S PLACES BACK. Empty path = no persistence, silently, exactly as it
    /// is for every other durable fact this weave holds.
    // WL-FILES-08 -- agents/workshop/files.md
    void save_marks() {
        if (host_->marks_path.empty() || marks_refused_) {
            return;
        }
        const Written done =
            marks_persist::save_file(host_->marks_path, session_.marks.maker);
        if (!done.accepted) {
            say("could not write your marks: " + done.refusal, true);
        }
    }

    /// TAKE A FRESH LISTING OF WHERE THE MAKER IS STANDING.
    // WL-FILES-12 -- agents/workshop/files.md
    void files_refresh() {
        ensure_marks();
        FilesPane& pane = session_.panels.files;
        const std::string dir = files_dir();
        if (dir.empty()) {
            pane.listing = Listing{};
            pane.listing.refusal =
                "this run began nowhere -- Workshop could not tell where it was launched "
                "from, so there is no origin to browse from";
            pane.cursor = 0;
            return;
        }
        pane.listing = enumerate_directory(dir);
        pane.cursor = 0;
        pane.wheel_accum = 0.0;
    }

    /// A BUILD THIS SESSION WATCHED HAS FINISHED, so what is on disk may have changed --
    /// take a fresh listing if the browser is open, and put the maker back where they
    /// were.
    // WL-FILES-12 -- agents/workshop/files.md
    void files_build_settled() {
        if (!session_.panels.has(panel::kProjectFiles)) {
            return;
        }
        const FileRow* row = row_at(session_.panels.files.listing, session_.panels.files.cursor);
        const std::string was = row != nullptr ? row->name : std::string();
        files_refresh();
        if (!was.empty()) {
            files_point_at(was);
        }
    }

    /// Put the cursor on a named row if this listing has one -- the ONE place a refresh
    /// does not send it home, because going UP has an answer to the question "which row
    /// did I come from" and landing at the top of a long directory would throw it away.
    void files_point_at(const std::string& name) {
        const FilesPane& pane = session_.panels.files;
        for (std::size_t i = 0; i < pane.listing.rows.size(); ++i) {
            if (pane.listing.rows[i].name == name) {
                session_.panels.files.cursor = i;
                return;
            }
        }
    }

    /// MOVE THE CURSOR BY `by` ROWS, bounded at both ends. Bounded at USE, because the
    /// listing under it is replaced wholesale by every refresh.
    void files_move(std::int64_t by) {
        FilesPane& pane = session_.panels.files;
        const std::size_t total = pane.listing.rows.size();
        if (total == 0) {
            pane.cursor = 0;
            return;
        }
        std::int64_t at = static_cast<std::int64_t>(pane.cursor < total ? pane.cursor : 0) + by;
        if (at < 0) {
            at = 0;
        }
        if (at >= static_cast<std::int64_t>(total)) {
            at = static_cast<std::int64_t>(total) - 1;
        }
        pane.cursor = static_cast<std::size_t>(at);
    }

    /// GO UP ONE LEXICAL DIRECTORY.
    // WL-FILES-03 -- agents/workshop/files.md
    void files_parent() {
        ensure_marks();
        FilesPane& pane = session_.panels.files;
        if (pane.current_dir.empty()) {
            say("there is nowhere to go up from -- this run began nowhere", true);
            return;
        }
        const std::string up = parent_location(pane.current_dir);
        if (up.empty()) {
            say(pane.current_dir + " is the top of this filesystem -- there is nothing "
                                   "above it to go to",
                false);
            return;
        }
        const std::filesystem::path was(pane.current_dir);
        const AdmittedName leaf = admit_filename(was.filename());
        pane.current_dir = up;
        files_refresh();
        if (leaf.exact) {
            files_point_at(leaf.name);
        }
        files_say_where();
    }

    /// Where the maker is, for a notice: the absolute location, which is the
    /// only unambiguous answer -- a relative spelling would need a base, and the base a
    /// browser used to have (the project) is exactly the thing it may now be nowhere near.
    std::string files_where() const {
        const std::string& dir = session_.panels.files.current_dir;
        return dir.empty() ? std::string("nowhere") : dir;
    }

    /// SAY WHERE THE MAKER NOW IS, and why this place is one they might have meant.
    // WL-FILES-07 -- agents/workshop/files.md
    void files_say_where() {
        const std::string where = files_where();
        const std::string why =
            provenance_words(session_.marks.provenance(session_.panels.files.current_dir));
        say(why.empty() ? "in " + where : "in " + where + " (" + why + ")", false);
    }

    /// ACT ON THE ROW THE CURSOR IS ON -- enter a directory, or hand a file to the one
    /// editor door.
    // WL-FILES-04, WL-FILES-10, WL-FILES-11 -- agents/workshop/files.md
    // WL-FOCUS-04 -- agents/workshop/focus.md
    void files_open(loom::Mail& mail) {
        ensure_marks();
        FilesPane& pane = session_.panels.files;
        const FileRow* row = row_at(pane.listing, pane.cursor);
        if (row == nullptr) {
            say("no row is selected -- nothing was opened", true);
            return;
        }
        if (!row->openable) {
            say("`" + shown_name(row->name) +
                    "` has bytes this Workshop cannot carry in a path -- it is shown so you "
                    "know it is there, and cannot be opened from here",
                true);
            return;
        }
        const std::string dir = files_dir();
        if (dir.empty()) {
            say("this run began nowhere -- there is no location to act in", true);
            return;
        }
        if (row->directory) {
            // THE SAME NORMALIZATION EVERY OTHER SPELLING IN THIS APPLICATION GOES
            // THROUGH (`persist::resolved_against`), so entering a directory at a
            // filesystem root cannot produce the doubled separator that would name a
            // different root on POSIX.
            const std::string into =
                admit_location(persist::resolved_against(dir, row->name));
            if (into.empty()) {
                say("`" + shown_name(row->name) +
                        "` cannot be reached from here in a path this Workshop can carry",
                    true);
                return;
            }
            pane.current_dir = into;
            files_refresh();
            files_say_where();
            return;
        }
        open_source(persist::resolved_against(dir, row->name), mail);
    }

    /// MARK, OR UNMARK, THE LOCATION THE BROWSER IS SHOWING.
    // WL-FILES-05 -- agents/workshop/files.md
    void files_mark() {
        ensure_marks();
        const std::string where = session_.panels.files.current_dir;
        if (where.empty()) {
            say("there is nowhere to mark -- this run began nowhere", true);
            return;
        }
        const bool removed = session_.marks.forget(where);
        if (!removed) {
            session_.marks.remember(where);
        }
        save_marks();
        say((removed ? "no longer marked: " : "marked: ") + where, false);
    }

    /// GO TO THE NEXT (or previous) PLACE WORTH RETURNING TO.
    // WL-FILES-06 -- agents/workshop/files.md
    void files_jump_mark(std::int64_t by) {
        ensure_marks();
        const std::vector<MarkedPlace> stops =
            session_.marks.destinations(host_filesystem_roots());
        if (stops.empty()) {
            say("there is nowhere to jump to -- no origin, no marks, and this system "
                "reports no filesystem roots",
                true);
            return;
        }
        const std::int64_t total = static_cast<std::int64_t>(stops.size());
        std::int64_t at = by > 0 ? -1 : 0;
        for (std::int64_t i = 0; i < total; ++i) {
            if (stops[static_cast<std::size_t>(i)].path == session_.panels.files.current_dir) {
                at = i;
                break;
            }
        }
        const std::int64_t to = ((at + by) % total + total) % total;
        const MarkedPlace& went = stops[static_cast<std::size_t>(to)];
        session_.panels.files.current_dir = went.path;
        files_refresh();
        const std::string why = provenance_words(went.from);
        say("at " + went.path + (why.empty() ? std::string() : " (" + why + ")"), false);
    }

    /// USE THE FILE THE CURSOR IS ON AS THIS SESSION'S RECIPE CATALOG.
    // WL-FILES-11 -- agents/workshop/files.md; WL-PROJ-05 -- agents/workshop/project.md
    void files_use_recipes(loom::Mail& mail) {
        ensure_marks();
        FilesPane& pane = session_.panels.files;
        const FileRow* row = row_at(pane.listing, pane.cursor);
        if (row == nullptr) {
            say("no row is selected -- the recipes in force are unchanged", true);
            return;
        }
        if (!row->openable) {
            say("`" + shown_name(row->name) +
                    "` has bytes this Workshop cannot carry in a path -- the recipes in "
                    "force are unchanged",
                true);
            return;
        }
        if (row->directory) {
            say("`" + shown_name(row->name) +
                    "` is a directory -- a recipe catalog is one authored file",
                true);
            return;
        }
        if (!host_->use_recipes) {
            say("this host cannot change recipe catalogs -- the recipes in force are "
                "unchanged",
                true);
            return;
        }
        const std::string dir = files_dir();
        if (dir.empty()) {
            say("this run began nowhere -- the recipes in force are unchanged", true);
            return;
        }
        const HostContext::RecipeSwap done =
            host_->use_recipes(persist::resolved_against(dir, row->name));
        if (!done.accepted) {
            // BOTH HALVES, IN ONE SENTENCE, AND IN THE ORDER THAT SURVIVES THE CUT. What
            // went wrong and what is still running are both owed here -- a refusal that
            // named only the first would leave a maker guessing whether they had just lost
            // the catalog they were using. The notice row is `detail::fit`-cut at the
            // band's width, so the two SHORT fixed statements go first and the two long
            // variable ones -- the owner's own sentence, then the path -- take the tail in
            // that order. MEASURED (the live witness): with the reason first, the reassuring
            // half was exactly the half that elided.
            say("not a recipe catalog -- the recipes in force are unchanged: " +
                    done.refusal + "; still using " +
                    (done.path.empty() ? std::string("no catalog") : done.path),
                true);
            return;
        }
        // THE SESSION LEARNS THAT ITS CATALOG HAS MOVED, AND TO WHAT. It is a projection
        // for the screen and never a second owner: no recipe is copied here, and every
        // consumer goes on reading the one owner exactly as before.
        //
        // ⚠ IT IS THE OWNER'S OWN ABSOLUTE PATH, AND IT IS READ BACK RATHER THAN
        // RECOMPOSED. It used to be a spelling relative to the Files root, which was
        // unambiguous only while that root WAS the project -- and a browser that can stand
        // anywhere makes a based spelling with no stated base a wrong-looking name for the
        // right file. `done.path` is what the catalog owner is holding after the attempt,
        // so the projection cannot name a different file from the one in force, and the
        // panel's column fits it with the tail intact (`detail::fit_path`) rather than
        // losing the half that says which catalog this is.
        session_.recipes_moved_to = done.path;
        // ...AND THE BUILDER IS ASKED TO SAY WHAT IT IS, through the same message an
        // opening panel has always sent. The tool reads the owner's views, so it already
        // holds the new catalog; what it has not done is SAY so, and this is the existing
        // sentence for that. No observer, no subscription, no second recipe event.
        (void)mail.send_to_role(zengine::builder::kBuilderRole,
                                zengine::builder::StatusRequested{});
        say("build recipes: " + done.path + " (" + std::to_string(done.recipes) +
                (done.recipes == 1 ? " recipe)" : " recipes)"),
            false);
        repaint(mail);
    }

    /// THE BROWSER'S KEYS -- nine verbs, every one of them a keymap row, so a maker who
    /// remapped them gets their own bindings here and on every help surface.
    void files_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        switch (session_.keymap.action_for(KeyContext::kFiles, k.scancode, k.modifiers)) {
        case Act::kFilesUp: files_move(-1); break;
        case Act::kFilesDown: files_move(1); break;
        case Act::kFilesOpen: files_open(mail); break;
        case Act::kFilesParent: files_parent(); break;
        case Act::kFilesRefresh:
            files_refresh();
            say("listed " + files_where() + " again", false);
            break;
        case Act::kFilesUseRecipes: files_use_recipes(mail); break;
        case Act::kFilesMark: files_mark(); break;
        case Act::kFilesNextMark: files_jump_mark(1); break;
        case Act::kFilesPreviousMark: files_jump_mark(-1); break;
        default: break; // an unbound key in this pane means nothing, and says nothing
        }
    }

    /// THE WHEEL OVER THE BROWSER'S BODY MOVES THE CURSOR, and the window follows it.
    // WL-EDIT-10 -- agents/workshop/editor.md
    void files_wheel(const zengine::input::PointerWheel& w, const Screen& sc,
                     loom::Mail& mail) {
        if (!over_files_body(session_, sc, w.space, w.x, w.y)) {
            return;
        }
        FilesPane& pane = session_.panels.files;
        if (!pane.listing.known || pane.listing.rows.empty()) {
            return;
        }
        const std::int64_t rows = spend_wheel(pane.wheel_accum, w.dy, kFilesWheelRows);
        if (rows == 0) {
            return;
        }
        const std::size_t was = pane.cursor;
        files_move(-rows);
        if (pane.cursor == was) {
            return; // already at the edge: nothing moved, nothing repaints
        }
        repaint(mail);
    }

    /// A PRESS INSIDE THE LAYOUTS PANE -- the tab run's own inverse, and the whole
    /// of what the top band's two global pointer arms became.
    // WL-PRESS-05 -- agents/workshop/press-chain.md; WL-TAB-09 -- agents/workshop/tab-run.md
    bool layouts_press(const zengine::input::PointerButton& b, loom::Mail& mail) {
        const LayoutTabPress tab =
            band_tab_at(session_, screen_of(session_), b.space, b.x, b.y);
        if (!tab.hit) {
            return false;
        }
        if (tab.create) {
            // THE `+` IS THE POINTER'S SPELLING OF `layout.new` AND NOTHING MORE:
            // the same door, the same ceiling, the same refusal in the same words. It arms
            // no double-click and begins no drag -- it is not a tab.
            session_.tab_click = TabClickMemory{};
            new_layout(mail);
            return true;
        }
        // A SECOND PRESS ON THE SAME TAB RENAMES IT, and the first one has already
        // made that tab live -- which is why the editor's subject and the switch cannot
        // disagree. `press_selects_word`'s discipline exactly: the completing press SPENDS
        // the arming, so there is no triple-click, and a first press is an ordinary switch
        // with an arming left beside it.
        const std::int64_t now = interaction_now();
        if (doubles_a_tab_click(session_.tab_click, tab.at, now)) {
            session_.tab_click = TabClickMemory{};
            open_layout_rename(tab.at);
            return true;
        }
        session_.tab_click = TabClickMemory{true, tab.at, now};
        // AND THE PRESS TAKES HOLD OF THE TAB. A press that becomes a drag
        // reorders; a press that does not is exactly the switch it always was, because a
        // drag that never moved lands the layout back where it started. The record holds no
        // position: the switch below has just made this tab the live one, so what is being
        // carried is always `setup.active_at`.
        session_.tab_drag.active = true;
        switch_layout(tab.at, mail);
        return true;
    }

    /// A PRESS IN THE BROWSER'S BODY: the first press on a row SELECTS it, and a press on the
    /// row that is already selected ACTIVATES it -- only in a pane that already held the keys,
    /// so no single press can replace what is open. Double-click is not what this is.
    // WL-FOCUS-04 -- agents/workshop/focus.md
    void files_press(const zengine::input::PointerButton& b, bool had_keyboard,
                     loom::Mail& mail) {
        FilesPane& pane = session_.panels.files;
        if (!pane.listing.known) {
            return; // consumed: a press into a browser with nothing listed is a focus statement
        }
        const Screen sc = screen_of(session_);
        const FilesPressAt at = files_press_at(session_, sc, b.space, b.x, b.y);
        if (!at.named) {
            return; // the header, or the strip below the last row: consumed, still
        }
        const ExternalBodyPlace body = files_body(session_, sc);
        std::size_t which = 0;
        if (!files_row_of_body_row(pane, body.rows, at.row, which)) {
            return; // a marker row or blank space names no entry, and invents none
        }
        if (had_keyboard && which == pane.cursor) {
            files_open(mail);
            return;
        }
        pane.cursor = which;
    }

    // ---- THE PANE EDITOR: a pane as a subject ------------------------------------------------

    /// A FRESH VIEW OF THE SUBJECT, TAKEN AT A GESTURE.
    // WL-PED-03 -- agents/workshop/pane-manager.md
    void repair_pane_editor_subject() {
        PaneEditor& ed = session_.pane_editor;
        if (!ed.addressed() || pane_editor_subject_row(session_).has_value()) {
            return;
        }
        const std::string was = ref_text(ed.subject);
        ed.subject = PaneRef{};
        ed.rows.clear();
        ed.row_cursor = 0;
        ed.on_rows = false;
        say("the Pane Manager's subject " + was +
                " is in neither this build's vocabulary nor this layout -- subject cleared",
            true);
    }

    /// MAKE THIS PANE THE PANE EDITOR'S SUBJECT -- the one writer of `PaneEditor::subject`.
    // WL-PED-02 -- agents/workshop/pane-manager.md
    void choose_subject(const PaneRef& ref) {
        PaneEditor& ed = session_.pane_editor;
        ed.subject = ref;
        ed.rows = pane_editor_rows(session_);
        ed.row_cursor = first_editable(ed.rows);
        const std::optional<CatalogRow> row = pane_editor_subject_row(session_);
        std::string name = ref_text(ref);
        std::string state;
        if (row) {
            name = row->kind == kNoPaneKind ? ref_text(ref) : row->name;
            state = pane_state_word(pane_state_of(session_.panels, session_.setup.active,
                                                  screen_of(session_), *row));
        }
        say("Pane Manager: " + name + (state.empty() ? "" : " (" + state + ")") + " -- " +
                hotkey(Act::kPaneEditorSwitch) + " reaches its rows",
            false);
    }

    /// STEP THE CURSOR OF WHICHEVER LIST THE KEYS ARE IN, bounded, and over a section
    /// heading without stopping on it -- a heading is a boundary, not a row a maker edits.
    void pane_editor_move(std::int64_t by) {
        pane_editor_move_in(session_.pane_editor.on_rows, by);
    }

    /// STEP ONE OF THE TWO LISTS' CURSORS -- the subject's rows (`rows`) or the PANES list
    /// -- by one, bounded.
    // WL-PED-08 -- agents/workshop/pane-manager.md
    void pane_editor_move_in(bool rows, std::int64_t by) {
        PaneEditor& ed = session_.pane_editor;
        if (!rows) {
            const std::size_t total = picker_population().size();
            if (total == 0) {
                return;
            }
            if (ed.cursor >= total) {
                ed.cursor = total - 1;
            }
            if (by < 0 && ed.cursor > 0) {
                --ed.cursor;
            } else if (by > 0 && ed.cursor + 1 < total) {
                ++ed.cursor;
            }
            return;
        }
        const std::size_t total = ed.rows.size();
        if (total == 0) {
            return;
        }
        std::size_t at = ed.row_cursor < total ? ed.row_cursor : total - 1;
        while (true) {
            if (by < 0) {
                if (at == 0) {
                    return;
                }
                --at;
            } else {
                if (at + 1 >= total) {
                    return;
                }
                ++at;
            }
            if (!ed.rows[at].section()) {
                ed.row_cursor = at;
                return;
            }
        }
    }

    /// THE WHEEL OVER THE PANE EDITOR MOVES THE LIST UNDER THE POINTER.
    // WL-EDIT-10 -- agents/workshop/editor.md; WL-PED-08 -- agents/workshop/pane-manager.md
    void pane_editor_wheel(const zengine::input::PointerWheel& w, loom::Mail& mail) {
        repair_pane_editor_subject();
        const PaneEditorAt where = pane_editor_at(session_, w.space, w.x, w.y);
        if (!where.present) {
            return;
        }
        PaneEditor& ed = session_.pane_editor;
        const std::int64_t rows = spend_wheel(ed.wheel_accum, w.dy, kListWheelRows);
        if (rows == 0) {
            return;
        }
        const bool on_fields =
            where.at.row >= static_cast<std::int64_t>(where.body.panes_rows);
        const std::size_t was = on_fields ? ed.row_cursor : ed.cursor;
        const std::int64_t steps = rows < 0 ? -rows : rows;
        for (std::int64_t n = 0; n < steps; ++n) {
            pane_editor_move_in(on_fields, rows > 0 ? -1 : +1);
        }
        if ((on_fields ? ed.row_cursor : ed.cursor) == was) {
            return; // already at the edge: nothing moved, nothing repaints
        }
        repaint(mail);
    }

    /// MOVE THE KEYS BETWEEN THE PANES LIST AND THE SUBJECT'S ROWS.
    void pane_editor_switch() {
        PaneEditor& ed = session_.pane_editor;
        if (!ed.on_rows && ed.rows.empty()) {
            say("the Pane Manager has no subject -- " + hotkey(Act::kPaneEditorChoose) +
                    " on a pane in its list chooses one",
                true);
            return;
        }
        ed.on_rows = !ed.on_rows;
    }

    /// THE ONE RETURN: on the PANES list it chooses the subject; on the rows it opens a
    /// draft, or says why the row under the cursor is not the maker's to author (the Info
    /// panel's `begin_edit` sentence, one inspector over).
    void pane_editor_choose() {
        PaneEditor& ed = session_.pane_editor;
        if (!ed.on_rows) {
            const std::vector<CatalogRow> rows = picker_population();
            if (ed.cursor >= rows.size()) {
                return; // the belt: a population that shrank under the cursor
            }
            if (pane_editor_draft_live(session_)) {
                say(finish_draft_first(), true); // a new subject would drop the draft
                return;
            }
            choose_subject(rows[ed.cursor].ref);
            return;
        }
        if (ed.row_cursor >= ed.rows.size()) {
            return;
        }
        Row& row = ed.rows[ed.row_cursor];
        if (!row.editable()) {
            say(row.label() + " is not authored -- it is what the screen makes of the "
                              "authored value",
                true);
            return;
        }
        row.begin();
        say("editing " + row.label() + " -- " + hotkey(Act::kDraftCommit) + " commits, " +
                hotkey(Act::kDraftCancel) + " cancels, `-` resets it",
            false);
    }

    /// OPEN THE SUBJECT IF IT IS CLOSED, REMOVE IT IF IT IS OPEN -- through the picker's own
    /// door, on the row the picker itself would act on.
    void pane_editor_toggle(loom::Mail& mail) {
        const std::optional<CatalogRow> row = pane_editor_subject_row(session_);
        if (!row) {
            say("the Pane Manager has no subject -- " + hotkey(Act::kPaneEditorChoose) +
                    " on a pane in its list chooses one",
                true);
            return;
        }
        if (pane_editor_draft_live(session_)) {
            say(finish_draft_first(), true);
            return;
        }
        toggle_participation(*row, hotkey(Act::kPaneEditorOpen), mail);
    }

    /// THE PANE EDITOR'S KEYS: a list with a cursor and one gesture on the row it is on.
    void pane_editor_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        repair_pane_editor_subject();
        const PaneRef subject = session_.pane_editor.subject;
        const auto order = [&](Act a) {
            if (subject.provider.empty()) {
                say("the Pane Manager has no subject -- " + hotkey(Act::kPaneEditorChoose) +
                        " on a pane in its list chooses one",
                    true);
                return;
            }
            spend_pane_action(a, subject, mail);
        };
        switch (session_.keymap.action_for(KeyContext::kPaneEditor, k.scancode, k.modifiers)) {
        case Act::kPaneEditorUp: pane_editor_move(-1); break;
        case Act::kPaneEditorDown: pane_editor_move(+1); break;
        case Act::kPaneEditorSwitch: pane_editor_switch(); break;
        case Act::kPaneEditorChoose: pane_editor_choose(); break;
        case Act::kPaneEditorOpen: pane_editor_toggle(mail); break;
        case Act::kPaneEditorFront: order(Act::kManageFront); break;
        case Act::kPaneEditorBack: order(Act::kManageBack); break;
        case Act::kPaneEditorRaise: order(Act::kManageRaise); break;
        case Act::kPaneEditorLower: order(Act::kManageLower); break;
        // THE PANE CREATOR'S THREE: make a pane, keep it, put it back.
        case Act::kPaneCreatorNew: open_pane_naming(); break;
        case Act::kPaneCreatorSave: save_maker_pane(); break;
        case Act::kPaneCreatorDiscard: discard_maker_pane_edits(mail); break;
        default: break; // an unbound key in this pane means nothing, and says nothing
        }
    }

    // ---- THE PANE CREATOR: a pane made of authored data ---------------------------------------

    /// REBUILD THE PANE MANAGER'S SUBJECT ROWS WITHOUT CHANGING THE SUBJECT.
    // WL-PED-04 -- agents/workshop/pane-manager.md
    void rebuild_subject_rows() {
        PaneEditor& ed = session_.pane_editor;
        if (!ed.addressed()) {
            return;
        }
        ed.rows = pane_editor_rows(session_);
        if (ed.row_cursor >= ed.rows.size()) {
            ed.row_cursor = first_editable(ed.rows);
        }
    }

    /// The sentence a dirty definition refuses with, naming the two ways out where the maker
    /// is standing. One spelling, spent by the open door, the naming door and the quit guard.
    std::string maker_pane_dirty_sentence(const char* consequence) const {
        const MakerPane& m = session_.panels.maker;
        const std::string name = m.definition.open() ? m.definition.name : m.saved.name;
        return "pane " + name + " has unsaved changes -- " + hotkey(Act::kPaneCreatorSave) +
               " in the Pane Manager saves it, " + hotkey(Act::kPaneCreatorDiscard) +
               " discards them; " + consequence;
    }

    /// THE ONE OPEN DOOR: a pane-definition file becomes the run's open definition, or
    /// nothing moves.
    // WL-MAKER-08 -- agents/workshop/maker-pane.md
    void open_maker_pane(const std::string& requested, loom::Mail& mail) {
        const std::string path = persist::resolved_against(host_->project_dir, requested);
        MakerPane& m = session_.panels.maker;
        if (m.dirty()) {
            say(maker_pane_dirty_sentence("nothing was opened"), true);
            return;
        }
        const pane_definition_persist::LoadedDefinition loaded =
            pane_definition_persist::load_file(path);
        if (!loaded.outcome.accepted) {
            if (path == host_pane_path()) {
                pane_refused_ = true;
                session_.conditions.establish(Condition{
                    kPaneWallKey, "pane definition refused -- this run will not write over it",
                    loaded.outcome.refusal +
                        " (the file is left exactly as it is; fix it, or start Workshop with "
                        "--pane <another path>)",
                    surface::role::kAlert, std::string()});
            }
            say(path + ": " + loaded.outcome.refusal, true);
            return;
        }
        m.path = path;
        m.definition = loaded.definition;
        m.saved = loaded.definition;
        rebuild_subject_rows();
        // AND THE DESK RE-SEATS, because a reference that was unresolved a moment ago may
        // resolve now -- the same reconcile a provider's late offer earns.
        apply_setup(mail);
        say("opened pane " + m.definition.name + " from " + path, false);
    }

    /// MAKE A PANE FROM A NAME -- the Pane Creator's own act.
    // WL-MAKER-08, WL-MAKER-11 -- agents/workshop/maker-pane.md
    bool new_maker_pane(const std::string& name, loom::Mail& mail) {
        MakerPane& m = session_.panels.maker;
        if (m.dirty()) {
            say(maker_pane_dirty_sentence("nothing was made"), true);
            return false;
        }
        const Written legal = check_maker_pane_name(name);
        if (!legal.accepted) {
            say(legal.refusal + " -- " + hotkey(Act::kPaneNamingCommit) + " tries again, " +
                    hotkey(Act::kPaneNamingCancel) + " cancels",
                true);
            return false;
        }
        const PaneRef ref = maker_pane_ref(name);
        m.definition = new_definition(name);
        m.saved = PaneDefinition{};
        m.path = host_pane_path();
        (void)add_pane(session_.setup.active, ref);
        const Seating trial = seat_panes(session_.setup.active, session_.panels,
                                         stack_capacity(screen_of(session_)));
        bool waiting = false;
        for (const std::int64_t k : trial.waiting) {
            if (is_maker_kind(k)) {
                waiting = true;
            }
        }
        apply_setup(mail);
        // THE CREATOR'S SUBJECT IS THE NEW PANE'S REGION: the manager's subject through its
        // one writer, then the keys onto the region's own rows, on the text first.
        choose_subject(ref);
        PaneEditor& ed = session_.pane_editor;
        ed.on_rows = true;
        for (std::size_t i = 0; i < ed.rows.size(); ++i) {
            if (ed.rows[i].label() == "Text") {
                ed.row_cursor = i;
                break;
            }
        }
        std::string said = "Pane Creator: " + name + " is on this layout";
        if (waiting) {
            said += " (waiting for room -- make the window taller)";
        }
        said += " -- its text region's rows are under INTERIOR; " +
                hotkey(Act::kPaneCreatorSave) + " saves it";
        if (m.path.empty()) {
            said += " (no pane file this run)";
        }
        say(said, false);
        return true;
    }

    /// OPEN THE PANE CREATOR'S NAME PROMPT -- `n` inside the Pane Manager. A dirty
    /// definition is refused HERE, before a name is typed, so a maker is not asked for a
    /// word they cannot use. The trigger's own character is swallowed by the binding.
    void open_pane_naming() {
        if (session_.panels.maker.dirty()) {
            say(maker_pane_dirty_sentence("no new pane was started"), true);
            return;
        }
        session_.pane_naming.open = true;
        session_.pane_naming.line.set(std::string(), 0);
        say("new pane -- type its name; " + hotkey(Act::kPaneNamingCommit) + " makes it, " +
                hotkey(Act::kPaneNamingCancel) + " cancels",
            false);
    }

    /// The name prompt's keys: the line's own vocabulary first, then make or cancel.
    void pane_naming_key(const zengine::input::KeyPressed& k, loom::Mail& mail) {
        PaneNaming& naming = session_.pane_naming;
        if (naming.line.consume(k.scancode, k.modifiers, session_.clipboard)) {
            return;
        }
        switch (session_.keymap.action_for(KeyContext::kPaneNaming, k.scancode, k.modifiers)) {
        case Act::kPaneNamingCommit:
            if (new_maker_pane(naming.line.text(), mail)) {
                close_pane_naming();
            }
            break;
        case Act::kPaneNamingCancel:
            close_pane_naming();
            say("no pane was made", false);
            break;
        default: break;
        }
    }

    /// Close the prompt whole, so a later open cannot inherit a stale draft.
    void close_pane_naming() { session_.pane_naming = PaneNaming{}; }

    /// WRITE THE OPEN DEFINITION TO ITS FILE -- the one save door, through the family's
    /// safe write.
    // WL-MAKER-08 -- agents/workshop/maker-pane.md
    void save_maker_pane() {
        MakerPane& m = session_.panels.maker;
        if (!m.open()) {
            say("no pane is open -- " + hotkey(Act::kPaneCreatorNew) +
                    " in the Pane Manager makes one",
                true);
            return;
        }
        if (m.path.empty()) {
            say(kNoPaneFile, true);
            return;
        }
        if (pane_refused_ && m.path == host_pane_path()) {
            say("the pane file " + m.path +
                    " was refused at startup and will not be written over -- fix it, or start "
                    "Workshop with --pane <another path>",
                true);
            return;
        }
        if (const Row* draft = pane_editor_editing_row()) {
            say(draft->label() + " is still being edited -- " + hotkey(Act::kDraftCommit) +
                    " commits, " + hotkey(Act::kDraftCancel) + " cancels; nothing was saved",
                true);
            return;
        }
        const Written written = pane_definition_persist::save_file(m.path, m.definition);
        if (!written.accepted) {
            say(written.refusal, true);
            return;
        }
        m.saved = m.definition;
        say("saved pane " + m.definition.name + " to " + m.path, false);
    }

    /// THE ONE DELIBERATE DISCARD DOOR: put the definition back to what its file holds.
    // WL-MAKER-08 -- agents/workshop/maker-pane.md
    void discard_maker_pane_edits(loom::Mail& mail) {
        MakerPane& m = session_.panels.maker;
        if (!m.open() && !m.saved.open()) {
            say("no pane is open -- nothing to discard", true);
            return;
        }
        if (!m.dirty()) {
            say("pane " + m.definition.name + " matches " + m.path + " -- nothing to discard",
                false);
            return;
        }
        const std::string was = m.definition.name;
        m.definition = m.saved;
        rebuild_subject_rows();
        apply_setup(mail);
        if (m.open()) {
            say("discarded unsaved edits -- pane " + m.definition.name + " is back to what " +
                    m.path + " holds",
                false);
        } else {
            say("discarded pane " + was +
                    " -- it was never saved, so no pane is open; its row on this layout is "
                    "kept and reads unresolved",
                false);
        }
    }

    /// KEEP THE NAME PROMPT'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW -- `refresh_setup_name`
    /// for the Pane Creator's line, against the Pane Manager's own heading columns.
    void refresh_pane_name() {
        if (!session_.pane_naming.open) {
            return;
        }
        const Screen sc = screen_of(session_);
        const PanelBounds where =
            bounds_of(session_.panels, session_.setup.active, panel::kPaneEditor, sc);
        if (!where.open) {
            return;
        }
        const PaneEditorBodyPlace body = pane_editor_body(session_, sc, where.rect);
        session_.pane_naming.line.keep_caret_visible(pane_name_columns(body.fit.columns));
    }

    /// A PRESS INSIDE THE PANE EDITOR'S BODY.
    // WL-PED-02 -- agents/workshop/pane-manager.md
    void pane_editor_press(const zengine::input::PointerButton& b, std::int64_t modifiers) {
        repair_pane_editor_subject();
        PaneEditor& ed = session_.pane_editor;
        const PaneEditorAt where = pane_editor_at(session_, b.space, b.x, b.y);
        if (!where.present) {
            return; // the heading, or the padding: consumed, still
        }
        for (std::size_t i = 0; i < ed.rows.size(); ++i) {
            Row& row = ed.rows[i];
            if (!row.editing()) {
                continue;
            }
            if (where.at.column < 0 || where.at.column > where.body.fit.columns ||
                field_at_prose_row(where.body, where.at.row) != i) {
                break; // on the pane, but not on the draft's row
            }
            const std::size_t target =
                row.editor().position_at_column(property_value_column(where.at.column));
            if (!press_selects_word(modifiers, row, target)) {
                row.place(target);
            }
            session_.text_drag.active = true;
            session_.text_drag.place = text_drag_place::kPaneEditorDraft;
            return;
        }
        const std::size_t pane = editor_pane_at_prose_row(where.body, where.at.row);
        if (pane != kNoObject) {
            const std::vector<CatalogRow> rows = picker_population();
            if (pane >= rows.size()) {
                return;
            }
            if (pane_editor_draft_live(session_)) {
                say(finish_draft_first(), true);
                return;
            }
            ed.on_rows = false;
            ed.cursor = pane;
            choose_subject(rows[pane].ref);
            return;
        }
        const std::size_t field = field_at_prose_row(where.body, where.at.row);
        if (field != kNoProperty && field < ed.rows.size() && !ed.rows[field].section()) {
            if (pane_editor_draft_live(session_)) {
                say(finish_draft_first(), true);
                return;
            }
            ed.on_rows = true;
            ed.row_cursor = field;
        }
    }

    /// OPEN THE SOURCE THE BUILDER'S CHOSEN RECIPE NAMES -- Builder's half, and only its
    /// half.
    // WL-EDIT-05 -- agents/workshop/editor.md
    void edit_source(loom::Mail& mail) {
        if (!session_.panels.has(panel::kBuilder)) {
            return; // an unbound key with no Builder panel open, exactly as `b` is
        }
        const BuilderPane& pane = session_.panels.builder;
        if (!pane.heard) {
            say("the Builder has not said what it builds yet -- nothing was opened", true);
            return;
        }
        if (pane.known.recipes.empty()) {
            say("this project has no build recipes -- nothing was opened", true);
            return;
        }
        const std::size_t at =
            pane.chosen < pane.known.recipes.size() ? pane.chosen : std::size_t{0};
        const std::string chosen = pane.known.recipes[at].recipe;
        if (!host_->recipe_source) {
            say("this host resolves no recipe sources -- nothing was opened", true);
            return;
        }
        const HostContext::RecipeSource named = host_->recipe_source(chosen);
        if (!named.known) {
            say("the Builder's catalog and this project's recipes disagree about `" +
                    chosen + "` -- nothing was opened",
                true);
            return;
        }
        if (named.source.empty()) {
            // THE RECIPE OWNER'S OWN VOCABULARY: the kind word is the recipe file's, so
            // the refusal reads in the terms the maker authored.
            say("`" + chosen + "` is a " + named.kind +
                    " recipe -- it names no single source file to edit",
                true);
            return;
        }
        open_source(named.source, mail);
    }

    /// THE ONE DOOR INTO THE EDITOR'S DOCUMENT -- a path in, this session's one open
    /// source out, and every referrer arrives through it.
    // WL-EDIT-03, WL-EDIT-05, WL-EDIT-06, WL-EDIT-11, WL-EDIT-13 -- agents/workshop/editor.md
    // WL-FRONT-04 -- agents/workshop/planes.md
    void open_source(const std::string& requested, loom::Mail& mail) {
        const std::string path = persist::resolved_against(host_->project_dir, requested);
        EditorState& e = session_.editor;
        if (e.open_document() && e.path == path) {
            // RE-REQUESTING THE OPEN SOURCE REVEALS IT AND DESTROYS NOTHING: the buffer,
            // its caret, its selection, its history and its viewport all stand; what
            // moves is presence (a removed pane comes back) and the keyboard.
            if (!ensure_editor_pane(mail)) {
                return;
            }
            session_.panels.selected = panel::kEditor;
            session_.panels.keyboard = panel::kEditor;
            e.follow_caret = true;
            say("editing " + e.path + (e.dirty() ? " -- UNSAVED edits stand" : ""), false);
            return;
        }
        if (e.dirty()) {
            // THE UNSAVED-LOSS FLOOR: a different source must not silently replace a
            // dirty buffer. The two ways out are the editor's own save and its one
            // deliberate discard door, both named with their effective gestures.
            say(e.path + " has unsaved changes -- " + hotkey(Act::kEditorSave) +
                    " in the editor saves them, " + hotkey(Act::kEditorDiscard) +
                    " discards them; nothing was opened",
                true);
            return;
        }
        // READ AND JUDGE BEFORE ANYTHING MOVES: a refused file costs the maker the
        // notice and nothing else -- the pane, the setup, the current document (if any)
        // and the file itself are all exactly as they were.
        const persist::FileText read =
            persist::read_file(path, kMaxSourceBytes, "a source file");
        if (!read.outcome.accepted) {
            say(read.outcome.refusal, true);
            return;
        }
        SourceIn admitted = source_in(read.text);
        if (!admitted.outcome.accepted) {
            say(path + ": " + admitted.outcome.refusal, true);
            return;
        }
        if (!ensure_editor_pane(mail)) {
            return;
        }
        e.path = path;
        e.saved_lines = admitted.lines;
        e.buffer.set_lines(std::move(admitted.lines));
        e.convention = admitted.convention;
        ++e.doc_epoch;
        e.first_row = 0;
        e.first_col = 0;
        e.wheel_accum = 0.0;
        e.follow_caret = true;
        // AND IT SELECTS THE PANE IT JUST FILLED. The keyboard candidate's own
        // argument, one question wider: an open that pointed the keys at a pane still
        // sitting behind another would put the first keystroke somewhere the maker
        // cannot see. The two facts are written together everywhere they are written.
        session_.panels.selected = panel::kEditor;
        session_.panels.keyboard = panel::kEditor;
        say("editing " + e.path, false);
    }

    /// MAKE THE EDITOR PANE PRESENT AND SEATABLE, or refuse with the room named -- the
    /// picker's own trial-seat shape, so the edit-source door cannot author a pane the
    /// screen has no room to show and then pour a document into the invisible result.
    bool ensure_editor_pane(loom::Mail& mail) {
        const PaneRef ref = pane_ref_of(panel::kEditor);
        Setup candidate = session_.setup.active;
        const bool added = add_pane(candidate, ref);
        const Seating trial = seat_panes(candidate, session_.panels,
                                         stack_capacity(screen_of(session_)));
        for (const std::int64_t k : trial.waiting) {
            if (k == panel::kEditor) {
                say("no room for the Editor on this screen -- make the window taller, "
                    "then try again",
                    true);
                return false;
            }
        }
        if (added) {
            session_.setup.active = std::move(candidate);
        }
        apply_setup(mail);
        return true;
    }

    /// WRITE THE SOURCE TO ITS FILE -- the editor's save authority.
    // WL-EDIT-01 -- agents/workshop/editor.md
    void save_source() {
        EditorState& e = session_.editor;
        if (!e.open_document()) {
            say("no source is open -- nothing was saved", true);
            return;
        }
        const Written written =
            persist::write_file(e.path, source_text(e.buffer.lines(), e.convention));
        if (!written.accepted) {
            say(written.refusal, true);
            return;
        }
        e.saved_lines = e.buffer.lines();
        say("saved " + e.path, false);
    }

    /// THE ONE DELIBERATE DISCARD DOOR: put the buffer back to the last saved state.
    // WL-EDIT-03 -- agents/workshop/editor.md
    void discard_source_edits() {
        EditorState& e = session_.editor;
        if (!e.open_document()) {
            say("no source is open -- nothing to discard", true);
            return;
        }
        if (!e.dirty()) {
            say("the source matches its last saved state -- nothing to discard", false);
            return;
        }
        e.buffer.revert_to(e.saved_lines);
        e.follow_caret = true;
        say("discarded unsaved edits -- " + e.path +
                " is back to its last saved state; undo takes them back",
            false);
    }

    // ---- Save and open -------------------------------------------------------

    /// Write the document to its file.
    // WL-DOC-16 -- agents/workshop/document.md
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
    // WL-CTX-01 -- agents/workshop/contextual.md; WL-DOC-16 -- agents/workshop/document.md
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
        // the selection is re-established rather than preserved. A room or pane
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
    /// went.
    // WL-DOC-10 -- agents/workshop/document.md
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

    /// DELETE AN EXPLICIT OBJECT -- `delete_selected`'s target-taking sibling.
    // WL-CTX-07 -- agents/workshop/contextual.md
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

    /// THE CONTEXTUAL DELETE: the captured object id, spent through the explicit-id door.
    // WL-CTX-07 -- agents/workshop/contextual.md
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
    /// Height goes through.
    // WL-DOC-07 -- agents/workshop/document.md
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
    // WL-DOC-08 -- agents/workshop/document.md
    static std::string edge_of(const Handled& done) {
        return done.clamped() ? " -- " + done.boundary : std::string();
    }
    /// A move notice names the AUTHORED position and, when there is one, the
    /// frame that position is authored IN.
    // WL-DOC-06 -- agents/workshop/document.md
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

    /// IS THE INSPECTOR ON THE SCREEN AT ALL? the rows are shown by a
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
    // WL-INFO-05 -- agents/workshop/info-body.md
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
        // The ceiling is THIS SCREEN'S room, not a constant: a surface can offer
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

    /// The rows are rebuilt, never patched.
    // WL-INFO-06 -- agents/workshop/info-body.md
    void rebuild_rows() { refocus(state_, session_); }

    /// KEEP THE NAME EDITOR'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW -- the
    /// same call `refresh_inspector` makes for a property draft.
    // WL-TEXT-03 -- agents/workshop/text-box.md
    void refresh_setup_name() {
        if (!session_.setup.naming.open) {
            return;
        }
        session_.setup.naming.line.keep_caret_visible(
            setup_name_columns(session_, screen_of(session_)));
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
    /// only when the answer has changed.
    // WL-PANE-06 -- agents/workshop/panes-and-windows.md
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

    /// TELL A PROVIDER A MAKER PRESSED IN ITS ROOM -- the whole of the input seam.
    // WL-PRESS-04 -- agents/workshop/press-chain.md
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

    /// WHICH EXTERNAL PANE THE KEYBOARD IS POINTED AT RIGHT NOW, or `kNoPaneKind`.
    // WL-FOCUS-01, WL-FOCUS-05 -- agents/workshop/focus.md
    std::int64_t keyboard_pane() const {
        return zengine::workshop::keyboard_pane(session_.panels);
    }

    /// TELL A PROVIDER A KEY WENT DOWN WHILE ITS PANE HELD THE KEYBOARD.
    ///
    /// WHAT WORKSHOP KNOWS WHEN IT SENDS THIS, EXACTLY AND ONLY: that a key transition
    /// arrived, and that the pane a maker last pressed into is still on screen with a room.
    /// It does not know what the pane is showing, whether the key means anything there,
    /// whether a field is being edited, or whether the provider will answer. Nothing in
    /// this function reads `ExternalPane::shown` and nothing may -- the moment Workshop
    /// looks at a provider's rows to decide what a key means, the seam has stopped being
    /// one (rule, one gesture further on).
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

    /// THE WHEEL TURNED OVER AN EXTERNAL PANE'S BODY: `external_press`'s shape for
    /// the one other pointer gesture that crosses the seam. Sent only while the pointer is
    /// over a prose row of the granted body -- `external_press_at`, the same one measurer,
    /// so the header and the remainder under the last row send nothing -- and only to a
    /// pane that holds a room, for the press's reason. The notches cross unchanged: how many
    /// rows one is worth is the provider's grammar, and Workshop keeps no accumulator for a
    /// list it cannot see. It follows the POINTER, not the keyboard: a pane a maker never
    /// pressed into is scrolled by pointing at it, exactly as it is pressed. Nothing is
    /// repainted here; if the provider answers, its own `PaneContent` repaints.
    void external_wheel(std::int64_t kind, const zengine::input::PointerWheel& w,
                        loom::Mail& mail) {
        const ExternalPressAt at =
            external_press_at(session_.panels, session_.setup.active, screen_of(session_), kind,
                              session_.pane_titles, w.space, w.x, w.y);
        if (!at.named) {
            return;
        }
        const RuntimePane* row = session_.panels.runtime.of_kind(kind);
        const ExternalPane* pane = session_.panels.external_pane(kind);
        if (row == nullptr || pane == nullptr || !pane->granted) {
            return;
        }
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(row->provider, PaneWheel{row->pane, w.dx, w.dy});
    }

    /// PUT THE SELECTED PANE DOWN -- the press-elsewhere gesture's two lines.
    // WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
    // WL-FOCUS-05 -- agents/workshop/focus.md
    // WL-FRONT-04 -- agents/workshop/planes.md
    void unselect_pane() {
        const std::string name = kind_name(session_.panels, session_.panels.selected);
        session_.panels.selected = kNoPaneKind;
        session_.panels.keyboard = kNoPaneKind;
        say("unselected " + name, false);
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

    /// THE PROJECT FRONTIER, READ ALIVE, NOW.
    // WL-ATTN-04 -- agents/workshop/attention.md
    ProjectFrontier frontier_now() const {
        return host_->frontier ? host_->frontier() : ProjectFrontier{};
    }

    /// WHAT MONOTONIC TIME IT IS -- `frontier_now`'s shape, for the one temporal gesture this
    /// application has. The host's reading if it wired one, the steady clock if not,
    /// and the answer is spent immediately by the caller that asked; nothing stores it.
    std::int64_t interaction_now() const {
        return host_->interaction_now ? host_->interaction_now() : interaction_now_ms();
    }

    void repaint(loom::Mail& mail) {
        refresh_terminal();  // the pane is a snapshot, and a snapshot is only true when taken
        refresh_inspector(); // and a draft's window is only true against the room it has now
        refresh_setup_name(); // ...and so is the name editor's, against the same room
        refresh_pane_name();  // ...and the Pane Creator's name prompt, against its heading
        refresh_editor();     // ...and the source viewport, against the body it has now
        refresh_external_rooms(mail); // ...and an external pane's room, against the same one
        // THE FRONTIER IS DERIVED HERE, PER PAINT, AND STORED NOWHERE. `paint` stays a
        // pure projection of what it is handed, and what it is handed is this repaint's
        // reading of the living realization owner — never a member, never a field of the
        // session, never yesterday's answer.
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
        mail.publish(paint(state_, session_, frontier));
    }

    /// LEAVE -- and write down what was on the desk on the way out.
    // WL-EDIT-03 -- agents/workshop/editor.md
    // WL-MAKER-08 -- agents/workshop/maker-pane.md
    // WL-SESSION-13 -- agents/workshop/session.md
    void quit() {
        // THE UNSAVED-LOSS FLOOR AT THE ONE EXIT: dirty source may leave this process
        // only by the maker's own deliberate act. All three arrival doors -- `q`, the
        // ctrl chord, a native close box -- meet the same refusal, which names the two
        // real ways out; there is no confirmation surface and no armed second press,
        // because a maker who has saved or discarded simply quits. The OBJECT document
        // keeps its recorded policy (its UNSAVED marker is its statement); the source
        // buffer is the one draft in this application whose loss is a file's worth of
        // work, which is why it alone holds the door.
        if (session_.editor.dirty()) {
            say("the source editor has unsaved changes -- " + hotkey(Act::kEditorSave) +
                    " in the editor saves them, " + hotkey(Act::kEditorDiscard) +
                    " discards them; Workshop stays open",
                true);
            return;
        }
        // AND A MAKER-MADE PANE HOLDS THE DOOR THE SAME WAY: a definition that
        // differs from its file is a maker's authored truth, and it may leave this process
        // only by their own save or their own discard.
        if (session_.panels.maker.dirty()) {
            say(maker_pane_dirty_sentence("Workshop stays open"), true);
            return;
        }
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

    /// ...and for the pane-definition file, a third sentence for the third reason.
    static constexpr const char* kNoPaneFile =
        "no pane file -- start Workshop with --pane <path>";

    /// What to say to a gesture that would have changed the document or the
    /// selection out from under a live property draft.
    // WL-CTX-07 -- agents/workshop/contextual.md; WL-CTRL-03 -- agents/workshop/info-controls.md
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

    /// THE ASKER'S OWN BOOK OF PASTES STILL IN FLIGHT, and the drafts each one belongs to.
    // WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
    loom::AskBook paste_asks_{4};
    std::vector<PendingPaste> pending_pastes_;

    /// One moment's worth of memory: the character the gesture's OWN keystroke
    /// produced, which is not text a maker typed. Set by a gesture that opens a
    // WL-KEY-12 -- agents/workshop/keyboard.md
    std::string swallow_text_;

    /// WHETHER THIS PROCESS HAS ALREADY TRIED TO TAKE BACK ITS LAST SESSION.
    // WL-SESSION-14 -- agents/workshop/session.md
    bool restored_ = false;

    /// WHETHER THE SESSION FILE THIS RUN FOUND COULD BE READ.
    // WL-SESSION-15 -- agents/workshop/session.md
    bool session_refused_ = false;

    /// What loading the keymap DID, held until the first surface can show it, and this
    /// run's own bookkeeping for the same reason `restored_` is. Empty means there is
    /// nothing that happened worth saying: no path, no file, or a file with no authored
    /// difference. A file that could not be admitted says nothing HERE -- that
    /// is a standing wall and it is a condition (`kKeymapWallKey`).
    bool keymap_loaded_ = false;
    bool marks_loaded_ = false;
    /// WHETHER THIS RUN HAS TRIED TO READ ITS PANE-DEFINITION FILE, and whether
    /// that file was REFUSED.
    // WL-MAKER-09 -- agents/workshop/maker-pane.md
    bool pane_loaded_ = false;
    bool pane_refused_ = false;
    /// A MARKS FILE THIS RUN COULD NOT UNDERSTAND, remembered so a later toggle cannot
    /// write over it.
    // WL-FILES-08 -- agents/workshop/files.md
    bool marks_refused_ = false;
    std::string keymap_word_;
    bool keymap_bad_ = false;
    bool startup_spoken_ = false; ///< the one combined startup sentence has been said

    /// What loading the PREFS file produced, the keymap's own bookkeeping one file over.
    // WL-FOCUS-11 -- agents/workshop/focus.md
    bool prefs_loaded_ = false;
    bool prefs_bad_ = false;

    /// WHETHER THIS RUN'S MEDIUM HAS REPORTED A DESKTOP PLACEMENT.
    // WL-SESSION-09 -- agents/workshop/session.md
    bool medium_placed_ = false;

    /// The document as it is ON DISK, or an empty one when nothing has been
    /// written yet. Session, emphatically: it is a copy kept so the status line
    /// can answer "is this saved" by comparing rather than by trusting a flag.
    WorkshopDoc saved_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_WEAVE_HPP
