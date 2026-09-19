// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WEAVE_HPP
#define ZENGINE_WORKSHOP_WEAVE_HPP

// Workshop's own weave: the authored document, the session, and the bindings
// from input MOMENTS to maker GESTURES.
// Workshop law: agents/workshop/session.md (+13 registers; agents/workshop.md routes)




#include "complete.hpp" // the completer's values, and where a line can go
#include "persist.hpp"
#include "host_pump.hpp" // the boundary this weave's publication hook runs inside
#include "quit_delivery.hpp" // the host's book of quit deliveries Loom refused, and its wake-up
#include "attention_seam_vocabulary.hpp" // what is true right now, said across the seam
#include "desktop_seam_vocabulary.hpp"   // the application-defaults owner: rows, launches, the floor
#include "builder_seam_vocabulary.hpp" // the doors the Builder pane asks; this host answers one
#include "pane_seam_vocabulary.hpp"  // the doors a pane weave asks; this host answers one
#include "open_seam_vocabulary.hpp"  // the managed opening's conversation
#include "editor_switch_vocabulary.hpp" // what a switch of the Editor is waiting on
#include "interaction_time.hpp" // what monotonic time it is, and nothing else
#include "keymap_persist.hpp"
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

    /// IS THIS OFFICE STILL TO COME? Answered by the host from the realization owner's rows at the
    /// moment of the ask and kept nowhere -- a READING, as `frontier` is: true while the plan row
    /// that loads a weave into it has not settled, so a provider that has not arrived YET is
    /// pending rather than unavailable. Empty answers no.
    // WL-DESK-04 -- agents/workshop/desktop.md
    std::function<bool(std::string_view office)> office_pending;

    /// WHAT MONOTONIC TIME IT IS, ANSWERED BY THE HOST -- `frontier`'s seam exactly.
    // WL-PTR-01 -- agents/workshop/pointer.md
    std::function<std::int64_t()> interaction_now;

    /// DOES WHOEVER HOLDS `role` AT THIS INSTANT ACCEPT `shape`? Answered by the host from the
    /// bus's own facts -- the role's holder and that holder's accepted schemas, the set a
    /// delivery is matched against -- for a native or a loaded holder alike, and kept nowhere
    /// (`holder_accepts_on` is the answer both the host and a suite wire). It chooses which
    /// published version of a sentence to send and PROMISES NOTHING ABOUT DELIVERY: the role is
    /// resolved again when the message is dispatched, and a different holder may refuse it.
    /// Empty answers no, and every such sentence then crosses in its first version.
    // WL-FOCUS-04 -- agents/workshop/focus.md
    std::function<bool(std::string_view role, const loom::Schema& shape)> holder_accepts;
    // Current incarnation, read afresh: canvas grants and held input never cross replacement.
    std::function<loom::WeaveId(std::string_view role)> role_holder;

    /// WHERE A TERMINAL LINE CAN BE ADDRESSED RIGHT NOW, read by the host off the bus at the call
    /// (`bus_destinations` is the answer both the host and a suite wire) and kept nowhere. Empty is
    /// a host that lists nothing, and the completer then offers the address forms.
    // WL-TERM-16 -- agents/workshop/terminal.md
    std::function<std::vector<Destination>()> destinations;

    /// WHAT THE AUTHORED RECIPE CATALOG SAYS ABOUT ONE RECIPE'S SOURCE, answered by the
    /// HOST -- through the read-only project office (`pane_doors.hpp`), to the Builder pane,
    /// which then asks the Editor's own office to open the file it names.
    // WL-PROJ-02 -- agents/workshop/project.md
    struct RecipeSource {
        bool known = false; ///< the id names an authored recipe of this project
        std::string kind;   ///< `single_source` or `cmake_target`, the file's own words
        std::string source; ///< the one authored source file; empty when the kind has none
    };

    /// `frontier`'s exact seam, one catalog over: the host wires a function over its own
    /// authored recipes, the door spends it at the moment of the ask and stores nothing.
    // WL-PROJ-02 -- agents/workshop/project.md
    std::function<RecipeSource(const std::string&)> recipe_source;

    /// WHAT STANDS BEHIND ONE OFFICE'S RUNNING CODE, answered by the HOST from the three owners
    /// that each know one edge, at the moment of the ask, and kept by nobody:
    ///
    ///     office   -> the weave holding it now           the bus
    ///     weave    -> the artifact it was realized from   the realization owner's row
    ///     artifact -> every recipe that produces it       the recipe catalog in force
    ///
    /// Empty fields are absences a sentence names: nobody holds the office, the plan's owner
    /// loaded no such weave, no recipe produces the artifact. Choosing among several recipes
    /// is not the answer's; neither is opening anything.
    // WL-CODE-01 -- agents/workshop/code.md
    struct CodeSource {
        /// ONE AUTHORED RECIPE THAT PRODUCES THE ARTIFACT. `source` is the file a single-source
        /// recipe compiles, completed as the build reads it -- an editing entry point, not a
        /// list of what that build includes -- and empty for a `cmake_target` recipe, which
        /// names a configured tree and a target and no one file.
        struct Recipe {
            std::string id;
            std::string kind;   ///< `single_source` or `cmake_target`, the recipe file's words
            std::string source; ///< empty when the kind names no single source
        };
        std::string office;          ///< the office asked about
        std::int64_t weave = 0;      ///< its holder at the ask; 0 when nobody holds it
        std::string artifact;        ///< the stem that weave was realized from; empty if none
        std::string reload;          ///< why a rebuilt image cannot reload in place; empty if it can
        std::vector<Recipe> recipes; ///< every authored recipe producing `artifact`, catalog order
    };
    // WL-CODE-01 -- agents/workshop/code.md
    std::function<CodeSource(const std::string& office)> code_source;

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

    /// WHAT A MAKER'S CHOICE OF SOMETHING BUILDABLE COMPOSED (PICK-1): the row's fields as
    /// the maker typed them, handed to the HOST, which composes the recipe, checks it by
    /// the recipe law, appends it to the catalog file AS AUTHORED through the atomic save,
    /// and installs the file through the one seam. `RecipeSwap` is the answer, because
    /// after the write the catalog in force IS the answer.
    // WL-AUTH-01 -- agents/workshop/authoring.md
    struct RecipeDraft {
        std::string id;                  ///< what the maker calls it
        std::string artifact;            ///< the stem it produces
        std::string source;              ///< single-source: the one .cpp, as the browser spelled it
        std::vector<std::string> packages; ///< single-source: CMAKE_PREFIX_PATH entries
        std::vector<std::string> links;    ///< single-source: exported target names
        std::string build_dir;           ///< cmake-target: the configured tree
        std::string target;              ///< cmake-target: the target in it
        std::string artifact_dir;        ///< cmake-target: where it lands, or empty
        bool tree = false;               ///< which of the two kinds this draft is
    };
    std::function<RecipeSwap(const RecipeDraft&)> author_recipe;

    /// WHAT AUTHORING THE MINIMUM PLAN ROW CAME TO (LOAD-IT): the row appended to the
    /// running project and written to the project's plan file, or refused in the plan's
    /// own words or the executor's -- and, when the new row is the frontier and the chosen
    /// recipe's product is already on disk, where that product is, so `o` can finish with
    /// the button's own act instead of leaving the maker a second key.
    // WL-AUTH-02, WL-AUTH-03 -- agents/workshop/authoring.md
    struct PlanAppend {
        bool accepted = false;
        std::string refusal;   ///< empty exactly when accepted
        std::string path;      ///< the plan file written, when one was
        std::string detail;    ///< what the running project made of the row, in its words
        bool frontier = false; ///< the new row is what the project is now waiting on
        std::string product;   ///< the recipe's product on disk, when the row is the frontier
                               ///< and the product is there; empty otherwise
    };
    std::function<PlanAppend(const std::string& stem, const std::string& role,
                             const std::string& recipe)>
        append_plan_row;

    /// DOES THE PLAN IN FORCE ALREADY NAME THIS ARTIFACT? Answered by the host, which
    /// holds the plan; the weave asks at the gesture and stores nothing.
    // WL-AUTH-02 -- agents/workshop/authoring.md
    std::function<bool(const std::string& stem)> plan_names;

    /// AN OBJECT DOCUMENT THIS RUN WAS POINTED AT OR FOUND, AND WILL NOT OPEN -- the path a
    /// `--document` named, or the retired default name when a file stands under it; empty when
    /// neither. The canvas that read such files retired; the file is left exactly as it is, and
    /// the host says so once at startup rather than reading its bytes as anything else.
    // WL-DOC-22 -- agents/workshop/document-file.md
    std::string retired_document;

    /// The one file this Workshop's SETUP saves to and restores from.
    // WL-LAYOUT-10 -- agents/workshop/layouts.md; WL-SESSION-01 -- agents/workshop/session.md
    std::string setup_path;

    /// The one file this Workshop's LAST SESSION is written to and read from.
    // WL-SESSION-01, WL-SESSION-04, WL-SESSION-13 -- agents/workshop/session.md
    std::string session_path;

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

    /// THE ONE PANE WHOSE PRESENTATION THIS HOST
    /// CLAIMS AND CAN COMMIT JOINTLY WITH ITS DOCUMENT -- host wiring, spelled by the host
    /// from the durable reference it already converts, never by this weave. Empty means no
    /// pane is managed: Workshop claims nothing and the managed doors answer nobody.
    PaneRef managed_pane;

    /// THE BOOK A NATIVE SHOWING THAT DID NOT COMPLETE IS WRITTEN IN (`host_pump.hpp`): this
    /// weave's publication hook keeps its own words here, and the host's turn tells them.
    ShowingFailures showings;

    /// THE BOOK A QUIT DELIVERY LOOM REFUSED IS WRITTEN IN (`quit_delivery.hpp`): the host's
    /// watch writes what the tap said, and this weave reads it when the watch wakes it.
    // WL-SESSION-19 -- agents/workshop/session.md
    UndeliveredQuits undelivered_quits;

    /// WHAT THE HOST ALREADY KNEW WAS TRUE, AND STILL IS.
    ///
    /// ⚠ IT CAN GROW AFTER THE FIRST PICTURE, WHICH IS NEW AND IS WHY THE COUNTER BELOW
    /// EXISTS. Realization settles inside an ordinary delivery, with the host loop already
    /// running and the first surface possibly already up -- so an optional plan row that
    /// refuses lands here AFTER `on(SurfaceReady)` took the list. A weave that read it once
    /// would show every condition the boot knew about except the ones the boot discovered.
    // WL-ATTN-01 -- agents/workshop/attention.md
    std::vector<Condition> standing_conditions;
    /// HOW MANY TIMES THAT LIST HAS BEEN ADDED TO. Compared rather than the list itself, so
    /// the common repaint costs one integer; `establish` is keyed and idempotent, so taking
    /// the list again when it moves re-establishes nothing that was already there.
    std::uint64_t conditions_generation = 0;

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

/// THE HOST'S ANSWER TO `HostContext::holder_accepts`, READ OFF `bus` AT THE CALL: the weave
/// holding `role` now, and whether its published accept-set has a door of exactly `shape`'s
/// identity (name, version and structure -- what dispatch selects a door by and what the gate
/// then admits against). Nobody holding the role is no. A weave that accepts every registered
/// shape by its accept MODE declares no door here, and is answered no.
bool holder_accepts_on(const loom::Switchboard& bus, std::string_view role,
                       const loom::Schema& shape);

/// THE HOST'S ANSWER TO `HostContext::destinations`, READ OFF `bus` AT THE CALL: every registered
/// weave that is not a sealed candidate, with the office it holds now, the names of the shapes it
/// accepts and whether it is alive -- and `self` marked, the participant a line runs as. A
/// reading, not a registry or a tap: it keeps nothing and observes no traffic.
std::vector<Destination> bus_destinations(const loom::Switchboard& bus, loom::WeaveId self);

/// The Workshop weave: the authored document, the session, and the bindings.
class WorkshopWeave
    : public loom::WeaveBase<WorkshopWeave, WorkshopState,
                             loom::Accept<zengine::workshop::PaneCanvasContent, zengine::input::KeyPressed, zengine::input::TextEntered,
                                          zengine::input::PointerButton,
                                          zengine::input::PointerMoved,
                                          zengine::input::PointerWheel,
                                          zengine::surface::SurfaceReady,
                                          zengine::surface::SurfaceExtent,
                                          zengine::surface::SurfacePlacement,
                                          zengine::surface::SurfaceCloseRequested,
                                          zengine::surface::ClipboardText,
                                          zengine::surface::ClipboardCopy,
                                          zengine::workshop::PaneOffered,
                                          zengine::workshop::PaneActions,
                                          zengine::workshop::v2::PaneActions,
                                          zengine::workshop::PaneContent,
                                          zengine::workshop::PaneCaret,
                                          zengine::workshop::v2::PaneContent,
                                          zengine::workshop::v2::PaneCaret,
                                          zengine::workshop::v3::PaneContent,
                                          zengine::workshop::PaneRevealRequested,
                                          zengine::workshop::PaneEscapeUnspent,
                                          // the second button's continuations: a press
                                          // handed back, a menu asked for, a subject to manage
                                          zengine::workshop::PanePassRequested,
                                          zengine::workshop::PaneKeyboardRequested,
                                          zengine::workshop::PaneMenuRequested,
                                          zengine::workshop::PaneManageRequested,
                                          zengine::workshop::PaneQuitAnswered,
                                          zengine::workshop::QuitDeliveryRefusalNoted,
                                          // an inspector's pane subject: which pane, the
                                          // picture now, and one write through its owner
                                          zengine::workshop::InspectPaneRequested,
                                          zengine::workshop::PaneSubjectRequested,
                                          zengine::workshop::PaneCommitRequested,
                                          // the participating owner of the application's
                                          // default behaviour: what it declares, and what
                                          // it asks this host to open or focus
                                          zengine::workshop::AppActions,
                                          zengine::workshop::PaneLaunchRequested,
                                          zengine::workshop::PaneCloseRequested,
                                          zengine::workshop::PaneToggleRequested,
                                          zengine::workshop::KeymapEditRequested,
                                          zengine::workshop::MakerPaneRequested,
                                          zengine::workshop::DeselectRequested,
                                          zengine::workshop::DesktopFace,
                                          zengine::workshop::PaneInventoryRequested,
                                          zengine::workshop::KeymapRequested,
                                          zengine::workshop::TerminalActRequested,
                                          zengine::workshop::TerminalCompletionRequested,
                                          zengine::workshop::PresentationTrialRequested,
                                          zengine::workshop::PresentationAdmitRequested,
                                          zengine::workshop::ManagedOpenSettled,
                                          zengine::workshop::ManagedOpenProgress,
                                          zengine::workshop::EditorSwitchProgress,
                                          // a pane's Edit Code: the open it asked for, and
                                          // Loom's word that the ask was refused at dispatch
                                          zengine::workshop::SourceOpened,
                                          // the host's own fence behind a picture it handed
                                          // the medium (P-WORK-25)
                                          zengine::workshop::PictureFence,
                                          // the presenter participant's half of a pane's menu:
                                          // what it shows, when it ends it, and that it arrived
                                          zengine::workshop::MenuShown,
                                          zengine::workshop::MenuClosed,
                                          zengine::workshop::PresenterReady,
                                          // ...and the host's own fence behind a menu it
                                          // withdrew, whose requester may still be owed
                                          zengine::workshop::WithdrawalFence,
                                          loom::DispatchRefused>,
                             loom::Emit<zengine::workshop::PaneCanvasRoom,
                                        zengine::workshop::PaneCanvasPointer,
                                        zengine::workshop::PaneCanvasRejected,
                                        zengine::surface::SurfaceCanvas,
                                        zengine::surface::SurfaceText,
                                        zengine::surface::ClipboardCopy,
                                        zengine::surface::ClipboardTextRequested,
                                        zengine::surface::SurfacePlacementRemembered,
                                        zengine::workshop::PaneCatalogRequested,
                                        zengine::workshop::PaneRoom,
                                        zengine::workshop::PanePressed,
                                        zengine::workshop::v2::PanePressed,
                                        zengine::workshop::v3::PanePressed,
                                        zengine::workshop::PaneButton,
                                        zengine::workshop::PaneMenuAnswered,
                                        zengine::workshop::PaneKey,
                                        zengine::workshop::PaneTextInput,
                                        zengine::workshop::PaneWheel,
                                        zengine::workshop::PaneDragged,
                                        zengine::workshop::PaneActionRequested,
                                        zengine::workshop::AppActionRequested,
                                        zengine::workshop::ActionsJudged,
                                        zengine::workshop::ActionsWithdrawn,
                                        zengine::workshop::PaneLaunchAnswered,
                                        zengine::workshop::PaneCloseAnswered,
                                        zengine::workshop::PaneToggleAnswered,
                                        zengine::workshop::KeymapEditAnswered,
                                        zengine::workshop::MakerPaneAnswered,
                                        zengine::workshop::PaneInventory,
                                        zengine::workshop::KeymapShown,
                                        zengine::workshop::PaneQuitRequested,
                                        zengine::workshop::PaneRevealAnswered,
                                        zengine::workshop::StandingConditions,
                                        zengine::workshop::PaneSubjectShown,
                                        zengine::workshop::PaneSubjectActed,
                                        zengine::workshop::TranscriptShown,
                                        zengine::workshop::TerminalActed,
                                        zengine::workshop::TerminalCompletionOffered,
                                        zengine::workshop::PresentationTrial,
                                        zengine::workshop::PresentationAdmitted,
                                        zengine::workshop::OpenSourceRequested,
                                        zengine::workshop::PaneSourceOpened,
                                        zengine::workshop::PictureFence,
                                        zengine::workshop::MenuGranted,
                                        zengine::workshop::MenuInput,
                                        zengine::workshop::MenuWithdrawn,
                                        zengine::workshop::WithdrawalFence>,
                             // the one latest claim
                             // this host makes -- the managed pane's presentation.
                             loom::Claims<zengine::workshop::PanePresentation>> {
public:
    explicit WorkshopWeave(HostContext& host);

    /// READ THE MAKER'S KEYMAP, OR STAND ON THE DEFAULTS.
    void load_keymap(loom::Mail& mail);

    /// READ THE MAKER'S PRESENTATION PREFERENCES, OR STAND ON THE DEFAULTS.
    void load_prefs();

    /// Say once what the startup file work DID, on the first surface that can show it --
    /// after the session restore, deliberately, so the sentence that survives on the one
    /// notice line is the one about this launch.
    void speak_startup_notes(loom::Mail& mail);

    /// TAKE THE CONDITIONS THE HOST ALREADY KNEW.
    void take_host_conditions();

    /// SAY WHAT IS TRUE RIGHT NOW, TO ANYONE PRESENTING IT -- but only when it CHANGED.
    ///
    /// ⚠ THE GATE IS WHAT MAKES THIS SEAM TERMINATE, and it is measured rather than
    /// cautious: a pane that hears a publication says its rows, `on(PaneContent)` ends in a
    /// repaint, and a repaint that published unconditionally would say it again. So the
    /// comparison against the last utterance is not an optimisation -- without it there is
    /// no quiet state in this process at all.
    ///
    /// WHAT IT REMEMBERS IS ITS OWN SPEECH, NOT THE TRUTH. Every condition is still derived
    /// per repaint and held nowhere (WL-ATTN-03, WL-ATTN-04); `said_conditions_` is a record
    /// of what this weave last said, which is the same thing `builder::BuildStatus`'s
    /// publisher keeps for the same reason.
    void say_conditions(const ProjectFrontier& frontier, loom::Mail& mail);

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
    void on(const zengine::surface::SurfaceReady&, loom::Mail& mail);

    /// READ THE PROJECT'S PANE DEFINITION, OR STAND ON NONE.
    void load_pane_definition(loom::Mail& mail);

    /// THE HOST'S PANE PATH IN THE ONE SPELLING THE DOORS COMPARE.
    std::string host_pane_path() const;

    /// THE SURFACE SAID HOW MUCH ROOM IT HAS. Take it, and lay the screen out again.
    void on(const zengine::surface::SurfaceExtent& e, loom::Mail& mail);

    /// THE MEDIUM SAID WHERE ITS WINDOW SITS. Remember it, whole and opaque.
    void on(const zengine::surface::SurfacePlacement& p, loom::Mail&);

    /// THE SURFACE WAS ASKED TO CLOSE -- by the window manager, the close box,
    /// the platform. Workshop applies the quit policy it already has.
    void on(const zengine::surface::SurfaceCloseRequested&, loom::Mail&);

    /// A key TRANSITION: which key changed, and what was held when it did.
    void on(const zengine::input::KeyPressed& k, loom::Mail& mail);

    // ---- What can I do with this? The contextual-action surface ---------------

    /// OPEN ON WHAT IS POINTED AT.
    void open_context_at(const PointedAt& at);

    /// OPEN ON A PAINTED LAYOUT TAB -- the same surface, on the one subject the band owns.
    void open_context_on_layout(const PointedAt& at, std::size_t layout);

    /// OPEN BY KEY, on the subject command mode can truthfully name: the room.
    void open_context_ambient();

    /// Close it whole: subject, group and cursor go together, so a later open cannot
    /// inherit a stale identity. Silent -- the surface disappearing is the statement.
    void close_context();

    /// The surface's own four keys, plus the opener closing it.
    void context_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    /// Back out one level, with the cursor landing on the group the maker just left --
    /// what makes backtracking read as returning rather than starting over.
    void leave_context_group();

    /// CHOOSE THE ROW THE CURSOR IS ON.
    void choose_context_row(loom::Mail& mail);

    /// SPEND ONE CHOSEN ACTION against the captured subject.
    void spend_context_choice(Act a, const ContextMenu& spent, loom::Mail& mail);

    /// EDIT CODE: reach the authored source of the captured pane's running code, and open it
    /// through the managed opening -- or say, in words, what is missing and how to provide it.
    void edit_code(const PaneRef& ref, loom::Mail& mail);

    /// THE OPEN EDIT CODE ASKED FOR CAME TO SOMETHING: read against the one ask this desk holds,
    /// re-resolved before the Builder is told anything.
    void on(const SourceOpened& said, loom::Mail& mail);

    /// LOOM'S WORD THAT ONE OF THIS DESK'S SENDS WAS REFUSED BEFORE ANY HANDLER RAN. Only the
    /// Edit Code ask's exact attempt settles anything; every other refused send is silence.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail);

    /// A BUTTON-1 PRESS WHILE THE SURFACE IS OPEN.
    void context_press(const PointedAt& at, std::int64_t space, std::int64_t x,
                       std::int64_t y, loom::Mail& mail);

    /// ANOTHER PARTICIPANT'S COPY — a pane provider's field, mirrored under the no-echo rule.
    void on(const zengine::surface::ClipboardCopy& c, loom::Mail&);

    /// THE SKIN'S ANSWER TO A PASTE THIS WEAVE REQUESTED — the one road foreign clipboard
    /// text has into this application, and it is walked only under a maker's paste.
    void on(const zengine::surface::ClipboardText& a, loom::Mail& mail);


    /// TEXT the maker actually entered — the platform's answer, not a guess made
    /// from a key identity.
    void on(const zengine::input::TextEntered& t, loom::Mail& mail);

    /// WHAT A BUTTON-1 RELEASE ENDED — and it is asked, not assumed.
    ///
    /// `pane` is meaningful only when `pane_held`.
    struct GesturesEnded {
        bool pane_held = false;
        PaneRef pane;
    };

    /// END EVERY BUTTON-1 GESTURE THIS SESSION IS HOLDING, whatever mode saw the release.
    GesturesEnded end_held_gestures();

    /// A pointer button changed, AND the position it changed at.
    void on(const zengine::input::PointerButton& b, loom::Mail& mail);

    /// The pointer moved. Outside a drag this weave has nothing to do with it:
    /// the job of remembering where the pointer is went away with the
    /// reconstruction it existed to serve.
    void on(const zengine::input::PointerMoved& m, loom::Mail& mail);

    /// THE WHEEL TURNED.
    void on(const zengine::input::PointerWheel& w, loom::Mail& mail);

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
    void on(const PaneOffered& offer, loom::Mail& mail);

    /// AN OFFICE DECLARES WHAT ONE OF ITS PANES CAN DO -- the rows of the one action
    /// catalog, for a pane this office has offered. Judged whole under the same stamp the
    /// offer was: the shape's half in `admit_pane_actions` (setup.hpp), the rows' half and
    /// the collision law in `join_pane_rows` (keymap.hpp), and only when both have passed
    /// are the declaration retained on the catalog row and the joined rows put in force.
    /// A refusal is said on the notice line in the keymap's own words and changes nothing.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void on(const PaneActions& actions, loom::Mail& mail);

    /// THE SAME DECLARATION, in the version that can name an action the pane owns (VD-27).
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void on(const v2::PaneActions& actions, loom::Mail& mail);

    /// THE ONE BODY BOTH VERSIONS SPEND, over the host's own row type.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void declare_pane_actions(const std::string& pane,
                              const std::vector<v2::PaneActionRow>& rows, loom::Mail& mail);

    /// RE-JOIN EVERY PANE'S RETAINED DECLARATION INTO THE KEYMAP NOW IN FORCE -- what the
    /// keymap file's load does, because the file may arrive after a pane did and its
    /// overrides are owed to that pane's rows too. A pane whose rows the file's bindings
    /// now collide with keeps no rows, and the refusal is said once with the load's word.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void rejoin_pane_rows(std::string& refusals, loom::Mail& mail);

    // ---- The participating owner of the application's default behaviour (WL-DESK) --------

    /// THE DESKTOP DECLARES WHAT THE APPLICATION ANSWERS TO, above every mode and after
    /// every mode. Judged whole by `join_app_rows` under the same collision law a pane's
    /// rows meet; the verdict is said on the band and ANSWERED to the declaration
    /// (`ActionsJudged`), and a refusal changes nothing that was already in force.
    void on(const AppActions& actions, loom::Mail& mail);

    /// A PRESENTER THAT HAS JUST ARRIVED ASKS FOR THE INVENTORY AS IT IS NOW, and is answered
    /// with it -- the reading `publish_inventory` says when it changes, said to one asker.
    void on(const PaneInventoryRequested& asked, loom::Mail& mail);

    /// THE ONE INVENTORY AS A VALUE: `inventory_rows` with the three facts beside each row.
    PaneInventory inventory_reading() const;

    /// SAY THE EFFECTIVE KEYMAP OUT LOUD, if it changed since it was last said -- the one
    /// binding truth dispatch reads, for the presenters that show keys (WL-DESK-11).
    void publish_keymap(loom::Mail& mail);

    /// ...AND ANSWER IT TO A PRESENTER THAT HAS JUST ARRIVED.
    void on(const KeymapRequested& asked, loom::Mail& mail);

    /// OPEN THIS PANE, OR PUT THE MAKER IN IT. Resolved against the ONE inventory, seated
    /// through the same door the launcher and a restore share, and answered either way.
    void on(const PaneLaunchRequested& asked, loom::Mail& mail);

    /// TAKE THIS PANE OFF THE DESK, AND UNLOAD NOTHING. Its row leaves the live desk through the
    /// setup's own door; its provider and everything it holds are untouched. Answered either way.
    void on(const PaneCloseRequested& asked, loom::Mail& mail);

    /// SHOW IT IF HIDDEN, HIDE IT IF SHOWN -- judged against the desk as it is NOW, through the
    /// two doors above, so a presenter's stale reading cannot make a toggle mean the other thing.
    void on(const PaneToggleRequested& asked, loom::Mail& mail);

    /// CHANGE HOW ONE ACTION IS REQUESTED: the candidate map judged whole, applied live, then
    /// written -- or refused with nothing changed. Answered on the delivery that asked.
    void on(const KeymapEditRequested& asked, loom::Mail& mail);

    /// ONE OF THE PANE CREATOR'S THREE ACTS, asked by the office presenting them: make, save or
    /// discard, through the doors below, answered with the sentence the band says.
    void on(const MakerPaneRequested& asked, loom::Mail& mail);

    /// WHAT STANDS IN THE EMPTY ROOM. Retained whole and painted behind every pane; refused
    /// whole when it exceeds `kMaxBackdropRows`, for `PaneContent`'s reason.
    void on(const DesktopFace& face, loom::Mail& mail);

    /// THE DESKTOP'S ANSWER TO ONE OF ITS OWN REQUESTED ROWS: put the maker's selection down.
    /// Judged against the particular ask it echoes and the gesture that raised it, exactly as
    /// a pane's unspent Escape is (WL-ARR-15) -- an answer to a keystroke that is over acts on
    /// nothing.
    void on(const DeselectRequested& asked, loom::Mail& mail);

    /// ASK THE APPLICATION-DEFAULTS OWNER FOR ONE OF ITS DECLARED ROWS, under a number minted
    /// for this one keystroke. The one door both dispatch positions spend.
    void request_app_action(const std::string& id, loom::Mail& mail);

    /// PERFORM ONE LAUNCH, WITHOUT ASKING WHO WANTED IT. The body `on(PaneLaunchRequested)`
    /// and every host-side caller spend, so "launch" means one thing.
    PaneLaunchAnswered launch_pane(const PaneRef& ref, loom::Mail& mail);

    /// PERFORM ONE CLOSE, WITHOUT ASKING WHO WANTED IT -- `launch_pane`'s partner, and not its
    /// inverse: it removes participation and never unloads, and it never opens anything.
    PaneCloseAnswered close_pane(const PaneRef& ref, loom::Mail& mail);

    /// WHAT THE ONE INVENTORY CALLS A PANE -- its offered name, or its pane key when nothing
    /// names it -- for a sentence about it.
    std::string inventory_name(const PaneRef& ref) const;

    /// SAY THE ONE INVENTORY OUT LOUD, if what it says has changed since the last time.
    /// Compared before it is published, for `StandingConditions`' reason: a presenter that is
    /// told the same thing twice repaints for nothing.
    void publish_inventory(loom::Mail& mail);

    /// IS ANYBODY THERE TO FILL THIS PANE NOW? A built-in or the maker's pane always is; a
    /// runtime pane is when its office's current holder accepts a room. Presence, not health.
    bool provider_present(std::int64_t kind, const PaneRef& ref) const;

    /// ANSWER ONE DECLARATION WITH WORKSHOP'S VERDICT (BL-WORK-04), when the declaring office's
    /// holder accepts the shape; a refusal is said on the band as well. Workshop owns the fact;
    /// the provider owns its recovery.
    void answer_declaration(const std::string& office, ActionsJudged verdict, loom::Mail& mail);

    /// TELL A DECLARING OFFICE THAT A DECLARATION IT HAD IN FORCE LEFT THE KEYMAP, naming the
    /// number the verdict gave it, when that office's holder accepts the shape.
    void say_withdrawn(const std::string& office, ActionsWithdrawn withdrawn, loom::Mail& mail);

    /// JOIN THE APPLICATION ROWS IN FORCE AGAIN, under the keymap now in force -- what a keymap
    /// file's load owes them, before the panes are joined again. A declaration the file's rows now
    /// collide with is withdrawn and told so; it is not retried.
    void rejoin_app_rows(std::string& refusals, loom::Mail& mail);

    /// SEAT THE PANE THAT ASKS, IN THIS DELIVERY, OR REFUSE WITH NOTHING MOVED. Judged through
    /// the launch door's own trial seat; on a seat the pane is authored if it was not, selected,
    /// and the keys are pointed at it before the answer (`PaneRevealAnswered`) is given. This
    /// delivery is the acquisition's commitment point, and the host holds nothing across it:
    /// no record, no reservation, no settle to wait for (pane_vocabulary.hpp says why).
    void on(const PaneRevealRequested& asked, loom::Mail& mail);

    /// A PANE'S WORD THAT THE ESCAPE IT WAS SENT HAD NOTHING MORE SPECIFIC TO DO THERE: put that
    /// pane down, exactly as a bare Escape in command mode would -- but only while the pane still
    /// has the desk and the keys and that Escape is still the last gesture this host handled.
    void on(const PaneEscapeUnspent& said, loom::Mail& mail);

    /// A PANE HANDS A SECONDARY PRESS BACK: judged against the newest press of that button and
    /// what happened since; acting opens this host's own pane menu, once, at the press's cell.
    void on(const PanePassRequested& said, loom::Mail& mail);
    void on(const PaneKeyboardRequested& said, loom::Mail& mail);
    /// A PANE ASKS THIS HOST TO PRESENT ROWS OF ITS OWN, beside a place in its room, continuing
    /// a press or an action of this host's. Eligibility is judged HERE, where the surface opens;
    /// a request that is not the maker's latest act is answered unchosen at once.
    void on(const PaneMenuRequested& asked, loom::Mail& mail);
    /// A PANE ASKS FOR THIS HOST'S OWN PANE MENU ON A SUBJECT it names -- the Pane Manager's
    /// route to a pane that is covered, closed or consumes every right press; judged against
    /// the menu answer it continues.
    void on(const PaneManageRequested& asked, loom::Mail& mail);

    /// ONE PANE'S ANSWER TO THE QUIT ASK. Counted against the fan-out `quit` recorded; the
    /// last answer decides -- every permission ends the process, any refusal keeps it open,
    /// says why, and replays the gestures held while the room was being asked.
    void on(const PaneQuitAnswered& said, loom::Mail& mail);

    /// THE HOST'S WAKE-UP: a delivery of a quit question was refused, and the book says which.
    /// The wake-up carries nothing and decides nothing; an entry for the quit in flight refuses
    /// it, naming who could not be asked and why, and every other entry is discarded.
    void on(const QuitDeliveryRefusalNoted& noted, loom::Mail& mail);

    // ---- A pane as an inspector's subject (WL-INFO-14, WL-INFO-15) -------------------------

    /// MAKE A PANE THE INSPECTOR'S SUBJECT -- the one writer of `Session::inspected`. An office
    /// asks; a reference in neither this build's vocabulary nor the live desk is refused in words
    /// with nothing moved. Asking for the pane already inspected keeps its name.
    void on(const InspectPaneRequested& asked, loom::Mail& mail);

    /// AN INSPECTOR THAT HAS JUST ARRIVED ASKS FOR THE PICTURE AS IT IS NOW, and is answered it.
    void on(const PaneSubjectRequested& asked, loom::Mail& mail);

    /// WRITE ONE ROW OF THE SUBJECT THE ASK NAMES, through that row's own setter -- the setup's
    /// gesture or reset door for a placement, the definition's door for a region -- or refuse
    /// with nothing written. The name is judged first; an accepted write reseats the desk.
    void on(const PaneCommitRequested& asked, loom::Mail& mail);

    /// NAME WHAT THE SUBJECT'S ROWS ADDRESS AFRESH, AND REBUILD THEM, when the live desk or the
    /// rows' INTERIOR arm moved since they were named -- asked before every reading and every
    /// judgement, so a commit is never judged against a name the facts have left.
    void refresh_inspected();

    /// SAY THE SUBJECT OUT LOUD, if it changed since it was last said (the inventory's rule).
    void publish_pane_subject(loom::Mail& mail);

    /// A COMMIT TYPED FOR ROWS THAT ARE NO LONGER THE SUBJECT'S (WL-INFO-15).
    static constexpr const char* kPaneCommitSubjectGone =
        "the inspected pane, its desk or its rows changed before it arrived, so nothing was "
        "written";

    /// AUTHOR ONE LINE AS THE TERMINAL PARTICIPANT, asked for by whoever presents it.
    ///
    /// ANSWERED AT THIS OFFICE: the party that holds the participant is the party that answers
    /// for it, and a second office would be a second answer to "who may speak as this
    /// terminal".
    void on(const TerminalActRequested& asked, loom::Mail& mail);

    /// WHAT COULD BE SAID NEXT, given a line the asker is holding.
    ///
    /// READS AND NEVER AUTHORS, and that is a property of the call rather than a rule:
    /// every method on the path is const, and the participant's channel -- the only thing
    /// that can send -- is unreachable through a const reference.
    void on(const TerminalCompletionRequested& asked, loom::Mail& mail);

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
    void on(const PaneContent& content, loom::Mail& mail);
    void on(const PaneCanvasContent& content, loom::Mail& mail);

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
    static Written judge_content(const PaneContent& content, const ExternalPane& pane);

    /// WHERE A PANE SAYS ITS CARET IS -- accepted whole into the pane's record, or refused
    /// whole so the pane has none.
    ///
    /// ⚠ IT DOES NOT REPAINT BY ITSELF AND THAT IS THE POINT. A caret arrives beside the
    /// rows it belongs to, on the same drain, and `on(PaneContent)` already ends in a
    /// repaint; a second one here would paint the rows once with the old caret and once
    /// with the new. A caret that arrives with no content behind it -- a pane moving its
    /// insertion point without changing a character, which is what an arrow key does --
    /// still has to reach the screen, so this handler repaints exactly when the caret it
    /// admitted DIFFERS from the one the pane already had.
    void on(const PaneCaret& caret, loom::Mail& mail);

    /// IS THIS CARET INSIDE THE CONTENT THIS PANE LAST HAD ACCEPTED?
    ///
    /// PURE, AND ANSWERED AGAINST `shown` RATHER THAN AGAINST THE GRANTED ROOM. The room is
    /// what a pane MAY fill; `shown` is what it did fill, and a caret on row 9 of a
    /// four-row answer is at a place with no text under it whether or not nine rows were
    /// granted. The column is judged against the row's own text, for the same reason, and
    /// a caret one past the last byte is legal -- that is where the insertion point sits
    /// at the end of a line.
    static Written judge_caret(const PaneCaret& caret, const ExternalPane& pane);

    // ---- The presentation owner's half of a managed opening (WL-OPEN) --------------------
    //
    // Bodies in weave_managed.cpp. Content and caret that name their generation; the
    // trial, the admission and the settlement of a managed opening; the publication hook;
    // and the end-of-delivery mirror that keeps this host's latest claim true.

    /// CONTENT NAMING ITS GENERATION: admitted as `PaneContent` is, unless it names a
    /// generation older than the one this pane holds -- a projection of a document that
    /// has since been replaced, refused rather than painted over its replacement.
    void on(const v2::PaneContent& content, loom::Mail& mail);
    void on(const v2::PaneCaret& caret, loom::Mail& mail);
    /// Content numbering its picture: admitted under v2's rule, the number recorded on the
    /// pane's view; a press is stamped with it once the medium has been handed it (`PictureFence`).
    void on(const v3::PaneContent& content, loom::Mail& mail);
    /// THE HOST'S OWN FENCE, COMING ROUND: the first hop sends it round once more, the second
    /// makes every picture handed out before it the one a press is stamped with.
    void on(const PictureFence& fence, loom::Mail& mail);
    /// THE PRESENTER SHOWS THE OPEN MENU'S LINES -- drawn when they fit the room granted, the menu
    /// withdrawn in words when they cannot be drawn.
    void on(const MenuShown& shown, loom::Mail& mail);
    /// THE PRESENTER ENDED THE OPEN MENU; a choice is recorded as the continuation of the act the
    /// presenter names, if that act was one this host forwarded to it.
    void on(const MenuClosed& closed, loom::Mail& mail);
    /// A HOLDER OF THE PRESENTER'S OFFICE ARRIVED: it carries the open menu (a handoff across a
    /// reload), or it does not and the menu ends, answered by this host.
    void on(const PresenterReady& ready, loom::Mail& mail);
    /// THE HOST'S OWN FENCE BEHIND A WITHDRAWAL, COMING ROUND: the first hop sends it round once
    /// more; the second forgets the withdrawn menu's record, with no refusal of it left to hear.
    void on(const WithdrawalFence& fence, loom::Mail& mail);
    /// WOULD THE PANE SEAT, AND WITH WHAT ROOM? Judged on a copy; nothing moves.
    void on(const PresentationTrialRequested& asked, loom::Mail& mail);
    /// ADMIT THE TRIAL'S CONTENT AND OFFER THE PRESENTATION for the exact operation.
    void on(const PresentationAdmitRequested& asked, loom::Mail& mail);
    /// THE OPERATION ENDED, said afterwards; the commitment already happened or did not.
    void on(const ManagedOpenSettled& said, loom::Mail& mail);
    /// WHAT THE MANAGER IS WAITING ON, kept as a standing condition a maker can read.
    void on(const ManagedOpenProgress& said, loom::Mail& mail);
    /// WHAT A SWITCH OF THE EDITOR IS WAITING ON, kept the same way, and how to stop it.
    void on(const EditorSwitchProgress& said, loom::Mail& mail);
    /// THE PUBLICATION HOOK: the trial becomes the desk, all at once, before any observer --
    /// Applied; or DECLINED: a presentation this desk did not prepare, or one it could not
    /// seat, is not applied and never reported as applied; the desk keeps what it has and
    /// re-claims its own truth at the end of its next delivery. The application runs inside
    /// the host's showing boundary, so what it throws is Failed, in its own words.
    loom::Weave::PublishedClaim on_claim_published(const PanePresentation& published);
    /// THE END OF EVERY DELIVERY: a repaint the hook owed, then the claim if it moved.
    void after_delivery(loom::Mail& mail);

    /// The session, for a suite that wants to check where a gesture left things.
    /// Read-only: every change still goes through a message and a gesture.
    const Session& session() const;
    /// ...and the menus withdrawn from the screen whose requesters may still be owed an answer,
    /// for a suite that wants to see that each record is forgotten. Read-only, as above.
    const std::vector<WithdrawnMenu>& withdrawn_menus() const;

private:
    // ---- The managed pane's bookkeeping ---------------------------------------------------

    /// THE APPLICATION ITSELF, run by the hook inside the boundary: `true` applied, `false`
    /// declined.
    bool show_presentation(const PanePresentation& published);

    /// THE ONE TRIAL IN FLIGHT: which pane, the candidate setup with it added, the room its
    /// body would get, and -- once admitted -- the rows and caret it will show.
    struct Trial {
        bool live = false;
        std::uint64_t op = 0;
        PaneRef ref;
        std::int64_t kind = kNoPaneKind;
        std::string name;
        std::string path;
        Setup candidate;
        std::int64_t room_rows = 0;
        std::int64_t room_columns = 0;
        std::vector<surface::SurfaceTextRow> rows;
        std::int64_t generation = 0;
        bool caret_ok = false;
        std::int64_t caret_row = surface::kNoCaret;
        std::int64_t caret_col = 0;
        std::int64_t sel_begin_row = surface::kNoSelection;
        std::int64_t sel_begin_col = 0;
        std::int64_t sel_end_row = surface::kNoSelection;
        std::int64_t sel_end_col = 0;
    };
    struct TrialRoom {
        bool ok = false;
        std::string refusal;
        std::int64_t rows = 0;
        std::int64_t columns = 0;
    };
    /// The kind the managed reference resolves to right now, or `kNoPaneKind`.
    std::int64_t managed_kind() const;
    /// ONE MORE INPUT ROUTED TO THIS PANE -- the admission counter the claim carries.
    void note_routed(std::int64_t kind);
    /// The managed pane's presentation, derived from the live session.
    PanePresentation derive_presentation() const;
    /// Claim it if it moved.
    void mirror_presentation(loom::Mail& mail);
    /// The room the pane's body would have after seating on `candidate`, or why not.
    TrialRoom trial_room(const Setup& candidate, std::int64_t kind, const std::string& name) const;
    /// The shared admission of content, generation-aware; the v1 door passes none.
    void admit_content(std::string_view office, const std::string& pane_key,
                       const std::vector<surface::SurfaceTextRow>& rows,
                       std::optional<std::int64_t> generation,
                       std::optional<std::int64_t> picture, loom::Mail& mail);
    void admit_caret(std::string_view office, const PaneCaret& caret,
                     std::optional<std::int64_t> generation, loom::Mail& mail);

    Trial trial_;
    PanePresentation claimed_presentation_;
    bool presentation_claimed_ = false;
    std::int64_t routed_ = 0;
    std::uint64_t shown_by_ = 0;
    bool repaint_owed_ = false;
    /// ...and the room grant the publication owes the pane it seated: said at the end of the
    /// delivery, through the one door every room goes through, so the pane's own record of
    /// its room agrees with the desk's (the refresh would stay silent, the room being equal).
    bool room_owed_ = false;

    /// IS THIS THE CHARACTER THAT KEY PRODUCED?
    static bool same_keystroke(const std::string& text, const std::string& owed);

    /// The effective spelling of one action, for this weave's own notices -- the same
    /// `hotkey_text` every screen surface spends, so a hint in the notice line and the
    /// band cannot spell one binding two ways.
    std::string hotkey(Act a) const;

    // ⭐ `editing_row` AND `pane_editor_editing_row` WERE HERE -- the property draft under the
    // keys, which the host's Pane Manager was the last to open. Every draft a maker types into a
    // property is an inspector's own now, in its own image.

    /// Which of this weave's own editable places a consumed paste request came from
    /// `kNone` for every armless branch.
    // WL-TEXT-09 -- agents/workshop/text-box.md
    /// ⚠ `kTerminal` IS GONE (VD-24), AND `kEditor` WENT THE SAME WAY (VD-25). A pane asks
    /// the Skin for the clipboard itself -- `surface::ClipboardTextRequested`, the same
    /// conversation this host opens for its own drafts -- so a line stops being one of the
    /// boxes this host pastes into on the day it stops being this host's box. The one left is
    /// the one this host still holds: the layout name. (`kDraft` left with the host's Pane
    /// Manager, and the Pane Creator's name line with it, into the desktop's pane.)
    enum class PasteOwner : std::uint8_t { kNone, kNaming };

    /// THE ONE-LINE NAME EDITOR THAT IS OPEN, or nothing -- the layout's.
    component::TextBox* naming_line();

    /// WHICH DRAFT WOULD THE CHAIN HAVE HANDED THE CLIPBOARD TO? A projection of the one
    /// resolved context rather than a second spelling of the routing, which closes the way
    /// two spellings could deliver a paste to a draft the keys never reached.
    PasteOwner paste_owner_now();

    /// ONE PASTE STILL IN FLIGHT: the conversation (by the book's own id) and the draft it
    /// belongs to.
    // WL-TEXT-09 -- agents/workshop/text-box.md
    struct PendingPaste {
        std::uint64_t ask = 0;
        PasteOwner owner = PasteOwner::kNone;
        std::uint64_t epoch = 0;
    };

    /// OPEN THE CLIPBOARD CONVERSATION A CONSUMED PASTE REQUEST ASKED FOR.
    void begin_clipboard_paste(loom::Mail& mail);

    /// The pending-paste record a settled conversation belongs to, removed from the list
    /// and handed back by value. An id the list does not hold answers the empty record
    /// (`owner == kNone`), which every consumer already treats as "nothing to do".
    PendingPaste take_pending_paste(std::uint64_t ask);

    // (`press_selects_word` WAS HERE, and left with its last caller: `weave_seam.cpp`.)


    // ⭐ `info_press`, `actions_press` AND `objects_press` LEFT WITH THE INFO PANEL. They were
    // the panel's three inverses -- a press in the live draft, a press on a control, a press on
    // an object name -- resolved against a body this host composed. The pane composes it now,
    // so a press inside its rectangle crosses as `PanePressed` with the pane's own prose row
    // and column, exactly as it does for every other pane, and the pane answers it.

    // ---- The terminal participant, behind a door ------------------------------

    /// WHAT THE PARTICIPANT'S RECORD HOLDS, TO WHOEVER IS PRESENTING IT -- derived on the
    /// repaint, compared against the last utterance, and said only when it changed.
    void say_transcript(loom::Mail& mail);

    /// COMPOSE AND AUTHOR ONE LINE through the participant, in Loom's own grammar.
    ///
    /// THE ONE PATH IN THIS PROCESS THAT SPEAKS AS THE TERMINAL, and the reason the
    /// participant stayed behind when its presentation left: `send`/`ask` go out through
    /// the participant's own channel, stamped with ITS identity and authorized against
    /// ITS grant. A pane authoring this would be authoring as the pane.
    ///
    /// ⚠ THE SCOPE OF THAT, SAID EXACTLY. No message in the interface `loom::TerminalSession`
    /// has TODAY drives it, and this work adds no Loom sentence -- so under that constraint
    /// it could not have moved. It is not a claim that no participant of this kind ever
    /// could; a driven door would change the answer, and that is a Loom conversation.
    void submit_terminal_line(const std::string& line);

    /// Command mode.
    void command(const zengine::input::KeyPressed& k, loom::Mail& mail);

    // ⭐ THE `+ panel` PICKER'S SECTION WAS HERE -- its population, cursor, wheel, keys and the
    // participation toggle two consumers spent. Launching and closing are the desktop's rows over
    // `launch_pane` and `close_pane`, which never toggle.

    // ---- The setup: name it, save it, restore it ------------------------------

    /// MAKE THE OPEN PANELS BE WHAT THE ACTIVE SETUP SAYS -- the one owner, and
    /// the only thing in this file that opens or closes a panel.
    void apply_setup(loom::Mail& mail);
    /// The same, from a place that has no delivery to speak from (the publication hook).
    void apply_setup_now();

    /// OPEN THE ONE-LINE NAME EDITOR ON THE LAYOUT AT `at`.
    void open_layout_rename(std::size_t at);

    /// The name editor's keys. Return commits the rename; Escape cancels and
    /// changes nothing; the rest is the ordinary editing of one line, through
    /// the component that owns the text, the caret and the window together.
    void naming_key(const zengine::input::KeyPressed& k, loom::Mail&);

    /// Close the editor whole: open, subject and line together, so a later open
    /// cannot inherit a stale position or a stale draft (`close_context`'s rule).
    void close_naming();

    /// TAKE THE TYPED NAME AND RENAME THE LAYOUT.
    void commit_layout_rename();

    /// WHAT TO SAY ABOUT A LAYOUT'S SETUP ASSOCIATION AFTER AN OPERATION
    /// -- nothing when there is none, and the artifact plus the verdict when there
    /// is.
    std::string link_note(std::size_t at) const;

    /// WHICH ARTIFACT `s` AND `r` ACT ON FOR THE LIVE LAYOUT: its own
    /// association where it has one, and the host's configured setup path where it
    /// does not.
    const std::string& setup_artifact() const;

    /// WRITE THE LIVE LAYOUT'S DESK TO ITS SETUP ARTIFACT (`s`).
    void save_setup();

    /// RESTORE THE LIVE LAYOUT FROM ITS SETUP ARTIFACT (`r`).
    void restore_setup(loom::Mail& mail);

    // ---- THE LAYOUT SHELF: several desks, one of them live --------------------

    /// WHICH LAYOUT IS LIVE AND WHERE IT SITS IN THE RUN, as one sentence.
    std::string layout_note() const;

    /// MAKE THE LAYOUT AT `to` LIVE -- the one operation every switching gesture spends,
    /// keyboard and pointer alike, so a tab press and a stepping key cannot come to mean
    /// two different transactions.
    void switch_layout(std::size_t to, loom::Mail& mail);

    /// STEP ONE ALONG THE RUN, wrapping -- over the WHOLE population, including the
    /// layouts the band's tab window had no room to paint. `by` is +1 or -1.
    void step_layout(std::int64_t by, loom::Mail& mail);

    /// What to say when the run is already as long as one Workshop keeps. One sentence,
    /// because both doors that can hit the ceiling deserve the same words.
    std::string layout_ceiling_note() const;

    /// ONE MORE LAYOUT: A FRESH BLANK DESK, APPENDED, AND LIVE.
    void new_layout(loom::Mail& mail);

    /// COPY THE LAYOUT AT `at`, INSERT THE COPY AFTER IT, AND STAND ON THE COPY.
    void duplicate_layout(std::size_t at, loom::Mail& mail);

    /// DROP THE LAYOUT AT `at` AND, WHERE IT WAS THE LIVE ONE, STAND ON A NEIGHBOUR.
    void drop_layout(std::size_t at, loom::Mail& mail);

    /// MOVE THE LAYOUT AT `at` ONE STEP ALONG THE MAKER'S ORDER.
    void shift_layout(std::size_t at, std::int64_t by);

    /// WHAT TO SAY ABOUT THE PANES THIS BUILD COULD NOT PRESENT -- nothing when
    /// there are none, and the first one BY NAME when there are.
    std::string unresolved_note(const Setup& s) const;

    // ---- THE LAST SESSION: the desk that comes back on its own ----------------

    /// TAKE BACK THE DESK AND THE ROOM THIS WORKSHOP WAS LAST USED IN.
    void restore_last_session(loom::Mail& mail);

    /// WRITE DOWN THE DESK AND THE ROOM, ON THE WAY OUT.
    void save_last_session();

    // ---- PANE MANAGEMENT: arrange the windows, and never lose one -------------

    /// THE ROWS A MAKER MAY ARRANGE: the shared inventory, restricted to what the setup
    /// names.
    std::vector<PaneRef> arrangeable() const;

    /// ARRANGE THE DESK: the global arrangement scope.
    void open_arrange_desk();

    /// ARRANGE ONE PANE: the pane-local scope, on an explicit target -- the
    /// context menu's captured subject, or the desk's keyboard target. ADMISSION PRECEDES
    void enter_arrange_pane(const PaneRef& ref);

    /// THE ACTIVE SETUP NO LONGER NAMES THE ADDRESSED PANE, SO NOTHING DOES.
    void forget_removed_selection();

    /// Leave the arrangement whole: scope, target and the reset prompt go together, so a
    /// later open cannot inherit a stale address -- the desk deliberately opens on no
    /// pane, and the one-pane scope binds its own.
    void close_arrange();

    /// What a maker reads about the pane the vocabulary addresses. One sentence, spent by
    /// every gesture that succeeds, so the notice line always names the thing that just
    /// moved -- and since the roster panel retired it carries the pane's STATE
    std::string arrange_status() const;

    /// MOVE THE KEYBOARD'S TARGET BY ONE ROW, wrapping.
    void arrange_step(std::int64_t by);

    /// CAN THIS PANE'S GEOMETRY BE AUTHORED RIGHT NOW, and if not, why not.
    Written arrange_geometry_ready(const PaneRef& ref) const;

    /// THE SELECTED PANE'S BOUNDS, both rectangles. Through `bounds_of`, never a second
    /// arithmetic: what a gesture measures from is what the painter drew.
    PanelBounds managed_bounds() const;

    /// THE WINDOW A GESTURE MEASURES FROM: authored where authored, resolved where
    /// reactive — the RESOLVED window, never the visible one (see `managed_bounds`).
    FineRect managed_window_base();

    /// AUTHOR AN ABSOLUTE PLACE. `x`/`y` are the whole proposal, saturated by the caller.
    void arrange_place(std::int64_t x, std::int64_t y, loom::Mail& mail);

    /// The place a one-cell nudge proposes: the resolved corner if this pane has no authored
    /// place yet, then the delta.
    void arrange_nudge(std::int64_t dx, std::int64_t dy, loom::Mail& mail);

    /// AUTHOR WHAT ONE RESIZE GESTURE PROPOSES — the whole window, in sub-units, split into its two axes.
    void arrange_resize(std::int64_t edge, std::int64_t base_x, std::int64_t base_y,
                        std::int64_t base_w, std::int64_t base_h, std::int64_t dx,
                        std::int64_t dy, loom::Mail& mail);

    /// The size a one-cell key press proposes: the authored window if there is one, else the
    /// resolved one -- the same "author the current resolved value, then apply the delta"
    /// rule the pointer follows, so the two gestures cannot disagree about where they start.
    void arrange_grow(std::int64_t dx, std::int64_t dy, loom::Mail& mail);

    /// ONE PANE ACTION, PERFORMED ON AN EXPLICIT TARGET -- the one place a targeted pane
    /// operation is spent, whatever asked for it.
    void spend_pane_action(Act a, const PaneRef& ref, loom::Mail& mail);

    /// RESET THE WHOLE SETUP'S FRONT ORDER -- zero-target, one owner call and its
    /// sentence, spent by the keyboard's reset prompt and by the room's contextual row.
    void reset_front_order();

    /// THE ARRANGEMENT KEYS -- one switch for both scopes and the reset prompt.
    void arrange_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    // ---- The pointer, inside arrangement -------------------------------------

    /// TAKE HOLD OF ONE PANE AT A POINTED POSITION -- the edge ring sizes, the body
    /// moves, and a press outside its rectangle is not this pane's. One function for both
    /// scopes, so the desk and the one-pane state cannot come to grab differently.
    bool take_pane_hold(const PaneRef& ref, const PointedAt& at, const Screen& sc);

    /// A PRESS WHILE ARRANGING, and the two scopes answer it differently because
    /// they are ABOUT different things.
    void arrange_press(const PointedAt& at);

    /// A MOTION WHILE A PANE GESTURE IS HELD. It targets the pane that CLAIMED THE PRESS,
    /// looked up by its reference, so nothing under the pointer can take the gesture over.
    void arrange_motion(std::int64_t sub_x, std::int64_t sub_y, loom::Mail& mail);

    // ⭐ THE SOURCE EDITOR'S SECTION WAS HERE AND IS GONE (VD-25). `editor_key`,
    // `editor_text`, `editor_press` and `refresh_editor` were the host's hands on a document
    // the host held; the document is the Editor weave's, and its keys, text, presses, drags
    // and viewport are its own, reached through the pane protocol like every other pane's.

    /// A PRESS INSIDE THE LAYOUTS PANE -- the tab run's own inverse, and the whole
    /// of what the top band's two global pointer arms became.
    bool layouts_press(const zengine::input::PointerButton& b, loom::Mail& mail);

    // ⭐ THE HOST PANE MANAGER'S SECTION WAS HERE -- its subject, its two lists, their cursors,
    // wheel and keys, its participation toggle and its presses. Its subject and rows are the
    // inspection seam's (below, and `weave_inspection.cpp`); its list is the desktop's pane.

    // ---- THE PANE CREATOR: a pane made of authored data ---------------------------------------

    /// THE GESTURE A DECLARED PANE ROW ANSWERS TO NOW, by its id, from the effective keymap's
    /// joined pane rows -- empty when no pane has declared it, or it answers to no key -- and,
    /// when asked, the name of the pane that declared it.
    std::string pane_row_hotkey(const std::string& id, std::string* pane_name) const;

    /// The sentence a dirty definition refuses with, naming the two ways out in the pane that
    /// presents them. One spelling, spent by the open door, the make door and the quit guard.
    std::string maker_pane_dirty_sentence(const char* consequence) const;

    /// THE ONE OPEN DOOR: a pane-definition file becomes the run's open definition, or
    /// nothing moves.
    void open_maker_pane(const std::string& requested, loom::Mail& mail);

    /// MAKE A PANE FROM A NAME -- the Pane Creator's own act. True when it was made.
    bool new_maker_pane(const std::string& name, loom::Mail& mail);

    /// WRITE THE OPEN DEFINITION TO ITS FILE -- the one save door, through the family's
    /// safe write. True when it was written.
    bool save_maker_pane();

    /// THE ONE DELIBERATE DISCARD DOOR: put the definition back to what its file holds. True
    /// unless there was no definition to put back.
    bool discard_maker_pane_edits(loom::Mail& mail);

    // ⭐ `open_source`, `ensure_editor_pane`, `save_source` AND `discard_source_edits` LEFT
    // WITH THE EDITOR. The one door into the document is the Editor weave's own
    // (`OpenSourceRequested` at `kEditorRole`); what this host still does for an opened
    // source is seat, select and point the keys at the pane that asked to be shown
    // (`on(PaneRevealRequested)`), which is `ensure_editor_pane`'s membership half made
    // general.

    // ⭐ THE OBJECT DOCUMENT'S DOORS WERE HERE -- save and open (`^s`, `^o`), create, delete,
    // the nudges and the four resize keys, the workspace refit, select and the rows' rebuild --
    // and retired with the prototype canvas. What the host keeps of a maker's hands is the desk
    // (panes, layouts), and a file that was an object document is left exactly as it is.

    /// KEEP THE NAME EDITOR'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW.
    void refresh_setup_name();

    void say(std::string text, bool bad);

    /// THE STATUS LINE: which layout is live, and what the desk's file says about it. It said
    /// how many objects and whether the object document matched its file until that retired.
    std::string status_line() const;

    // ---- THE EXTERNAL PANE'S ROOM AND GESTURES: the grant, a press, a key, the wheel, text ----

    /// GRANT EACH OPEN EXTERNAL PANE THE ROOM IT CURRENTLY HAS -- once per repaint, and
    /// only when the answer has changed.
    void refresh_external_rooms(loom::Mail& mail);
    void refresh_canvas_rooms(loom::Mail& mail);
    void end_canvas_holds(loom::Mail& mail);
    bool canvas_press(std::int64_t kind, const input::PointerButton& b,
                      bool keys_went_here, loom::Mail& mail);
    bool canvas_release(const input::PointerButton& b, loom::Mail& mail);
    bool canvas_motion(const input::PointerMoved& m, loom::Mail& mail);
    bool canvas_wheel(std::int64_t kind, const input::PointerWheel& w, loom::Mail& mail);
    void lose_canvas_hold(std::size_t slot, loom::Mail& mail);
    bool canvas_owner_current(std::int64_t kind) const;
    struct CanvasHold {
        bool active = false;
        std::int64_t kind = kNoPaneKind;
        loom::WeaveId owner{};
        std::int64_t origin_x = 0, origin_y = 0;
        PaneCanvasPointer event;
        loom::Ticket attempt{};
    } canvas_holds_[3];
    std::int64_t canvas_grants_ = 0, canvas_gestures_ = 0;

    /// TELL A PROVIDER A MAKER PRESSED IN ITS ROOM. Answers whether the press NAMED A ROW
    /// of the granted body -- which is what a sweep may begin from -- and nothing about
    /// what the provider made of it, which this host never learns.
    ///
    /// `at` AND `keys_went_here` ARE THE PICTURE THE MAKER PRESSED, read by the caller before
    /// the press moved the keyboard: where the press landed in the body as it was painted, and
    /// whether that pane was where an ordinary key went. The press crosses ONCE, as
    /// `v2::PanePressed` when the office's current holder accepts it and as v1 otherwise.
    bool external_press(std::int64_t kind, const ExternalPressAt& at, bool keys_went_here,
                        loom::Mail& mail);

    /// TELL THE PANE A PRESS BEGAN IN THAT THE HAND MOVED WITH THE BUTTON DOWN -- resolved
    /// against that pane's body as it is at THIS motion, through the same one measurer the
    /// press spent, and deliberately not clamped into it (`PaneDragged`). The pane is the
    /// press's, by handle, for the length of the gesture; a pane that lost its seat or its
    /// room ends the sweep here with nothing sent.
    void external_drag(std::int64_t kind, const zengine::input::PointerMoved& m,
                       loom::Mail& mail);

    /// WHICH EXTERNAL PANE THE KEYBOARD IS POINTED AT RIGHT NOW, or `kNoPaneKind`.
    std::int64_t keyboard_pane() const;

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
    ///
    /// A GESTURE THIS PANE DECLARED A ROW FOR CROSSES AS THE RESOLVED ID (WL-KEY-15):
    /// `PaneActionRequested{pane, id}` instead of the key, so the maker's override reaches
    /// the pane and the pane never re-derives a binding it cannot see. Every other key
    /// crosses as `PaneKey` exactly as before -- except a bare Escape to a pane whose holder has
    /// no door for a key and declared no row for it, which nothing on the far side could spend:
    /// that crosses as nothing, and Escape's own last meaning answers. Answers whether a sentence
    /// crossed.
    bool external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
                      loom::Mail& mail);

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
                        loom::Mail& mail);

    // ---- The second button: custody, continuation, and a pane's own menu -------------------
    // (the laws WL-PRESS-06, WL-CTX-08 and WL-CTX-09)

    /// DELIVER A SECONDARY PRESS to a pane whose holder has the `PaneButton` door; true iff it
    /// was queued. Delivery is consumption: a hold (release custody) and a continuation
    /// (eligibility to be handed back or to open a menu) are recorded, per button.
    bool external_button(std::int64_t kind, std::int64_t button, const ExternalPressAt& at,
                         const PointedAt& cell, loom::Mail& mail);
    /// THE RELEASE OF A HELD SECONDARY BUTTON goes to the hold's pane wherever the pointer is;
    /// true iff a hold ended. It never restores a continuation's eligibility.
    bool external_release(std::int64_t button, const zengine::input::PointerButton& b,
                          loom::Mail& mail);
    /// A HOLD WHOSE PANE LEFT THE DESK ends with a `lost` release owed to it; a CONTINUATION
    /// whose pane left the desk is dropped whether or not its hold is still active -- closing
    /// invalidates the continuation independently of the physical button; and a menu presented
    /// for a pane that left is closed and answered unchosen.
    void end_lost_holds(loom::Mail& mail);
    /// SETTLE A SECONDARY PRESS LOOM ATTESTS IT NEVER DELIVERED. Matched by the attempt ticket
    /// Loom's `DispatchRefused` names against the hold's own; on a match the hold and its
    /// continuation are dropped, so a later physical release sends no release for a press that
    /// never reached its recipient. An old attempt's refusal names no live hold and cancels
    /// nothing newer. Returns whether a hold was settled. (WL-PRESS-06)
    bool end_refused_button(const loom::Ticket& refused_attempt, loom::Mail& mail);
    /// GRANT A PANE'S MENU TO THE PRESENTER at a cell of its body, withdrawing an older menu and
    /// closing the host's own first -- one surface at a time. The ask was judged eligible by the
    /// caller; what the rows say and mean is the presenter's to show and the requester's to act on.
    void grant_menu(const RuntimePane& row, const PaneMenuRequested& asked,
                    std::uint64_t correlation, const PointedAt& at, loom::Mail& mail);
    /// END THE PRESENTED MENU BECAUSE CUSTODY MOVED, telling the presenter why; the presenter
    /// answers its requester. Who asked is kept until Loom has had its say (`withdrawn_`); a
    /// withdrawal that queues nothing is answered here at once. Nothing when no menu is open.
    void withdraw_menu(const std::string& why, loom::Mail& mail);
    /// END THE PRESENTED MENU WHEN NO PRESENTER CAN ANSWER IT -- it left, or the holder that
    /// replaced it does not carry the menu -- and answer the requester unchosen, as this office.
    void end_menu_unanswered(const std::string& why, loom::Mail& mail);
    /// ANSWER A WITHDRAWN MENU'S REQUESTER, unchosen, as this office, under the ask's number: the
    /// presenter could not be told. The menu is already off the screen, so nothing is repainted.
    void answer_withdrawn(const WithdrawnMenu& menu, const std::string& why, loom::Mail& mail);
    /// SETTLE THE MENU WHOSE SENTENCE LOOM REFUSED, by the attempt Loom names -- the open menu if
    /// the attempt is its own, a withdrawn one whose attempts span it, else nothing: an older
    /// menu's refusal ends, alters and answers no newer one. Returns whether a menu was settled.
    bool end_refused_menu(const loom::Ticket& refused_attempt, const std::string& reason,
                          loom::Mail& mail);
    /// FORWARD ONE OF THE MAKER'S ACTS TO THE PRESENTED MENU, numbered as the act it is.
    void forward_menu_input(std::int64_t kind, std::int64_t verb, std::int64_t scancode,
                            std::int64_t modifiers, std::int64_t button, std::int64_t line,
                            loom::Mail& mail);
    /// A KEY WHILE A PANE'S MENU IS PRESENTED: named by the maker's contextual rows, forwarded.
    void menu_key(const zengine::input::KeyPressed& k, loom::Mail& mail);
    /// A BUTTON WHILE A PANE'S MENU IS PRESENTED; true when the button was spent on the menu, false
    /// when the menu was withdrawn and the press must be routed as it would have been without it.
    bool menu_button(const zengine::input::PointerButton& b, loom::Mail& mail);
    /// IS THIS THE PRESENTER'S WORD ABOUT THE MENU THAT IS OPEN? Authored from its office, about
    /// the number this host granted.
    bool about_open_menu(std::int64_t menu, const loom::Mail& mail) const;
    /// THE ANCHOR CELL for a place in a pane's granted body, or the body's origin when the place
    /// is outside it; `understood` false when the pane has no body on this screen.
    PointedAt cell_of_body_place(std::int64_t kind, std::int64_t row, std::int64_t column) const;

    /// PUT THE SELECTED PANE DOWN -- the press-elsewhere gesture's two lines.
    void unselect_pane();

    /// ...AND THE TEXT THE PLATFORM MADE OF IT. `external_key`'s twin in every respect,
    /// and a separate send because they are separate facts: a key may produce no text and
    /// text may arrive with no key this application can name. Workshop maps no key to any
    /// character here any more than it does anywhere else.
    void external_text(std::int64_t kind, const zengine::input::TextEntered& t,
                       loom::Mail& mail);

    // ---- THE RUN: what is true now, the picture published, and the way out --------------

    /// THE PROJECT FRONTIER, READ ALIVE, NOW.
    ProjectFrontier frontier_now() const;

    /// WHAT MONOTONIC TIME IT IS -- `frontier_now`'s shape, for the one temporal gesture this
    /// application has. The host's reading if it wired one, the steady clock if not,
    /// and the answer is spent immediately by the caller that asked; nothing stores it.
    std::int64_t interaction_now() const;

    void repaint(loom::Mail& mail);
    /// A NUMBERED PICTURE THE CANVAS JUST HANDED THE MEDIUM is recorded as in flight, and one
    /// fence is sent behind the canvas for all of them (none when no picture moved).
    void fence_pictures(loom::Mail& mail);

    /// LEAVE -- by asking the room first. A maker-made pane's dirty definition refuses here,
    /// synchronously, as it always did; every pane that accepts `PaneQuitRequested` is then
    /// asked, and the answer count Loom hands back is what this host waits for. No answer
    /// owed means the exit proceeds now; otherwise `on(PaneQuitAnswered)` finishes it.
    void quit(loom::Mail& mail);

    /// THE EXIT ITSELF: write down the desk, then stop the bus. The one place both happen.
    void finish_quit();

    /// THE QUIT IN FLIGHT ENDS WITHOUT AN EXIT, for whatever made it impossible -- a pane's
    /// refusal, or a question Loom could not deliver. One cleanup for both: its bookkeeping is
    /// retired, `why` is said with the count of gestures the hold dropped, the held gestures are
    /// replayed in order, and the desk is repainted. Nothing is saved and the bus keeps running.
    void refuse_quit(std::string why, loom::Mail& mail);

    /// ONE INPUT GESTURE HELD WHILE A QUIT IS PENDING -- key, text, press, motion or wheel,
    /// whichever arrived, kept whole so a refused quit can replay it exactly.
    struct HeldInput {
        enum class Kind : std::uint8_t { kKey, kText, kButton, kMoved, kWheel };
        Kind kind = Kind::kKey;
        zengine::input::KeyPressed key;
        zengine::input::TextEntered text;
        zengine::input::PointerButton button;
        zengine::input::PointerMoved moved;
        zengine::input::PointerWheel wheel;
    };

    /// HOW MANY GESTURES A PENDING QUIT WILL HOLD. The exchange is one drain of the bus, so
    /// what arrives during it is one poll's burst at most; a burst past this is dropped and
    /// counted, and the count is said with the refusal rather than swallowed.
    static constexpr std::size_t kMaxHeldInput = 256;

    /// HOLD ONE GESTURE WHILE THE ROOM IS BEING ASKED, or say it need not be held. Every
    /// input handler asks this first: while a quit is pending nothing reaches a pane, which is
    /// what makes a pane's "clean" answer a fact about the instant the process ends.
    bool hold_input(HeldInput held);

    /// PLAY THE HELD GESTURES BACK, IN ORDER, through the same handlers they arrived at --
    /// the refused quit's promise that a maker who typed through it lost nothing.
    void replay_held(loom::Mail& mail);

    /// What to say when there is no setup file to save to or restore from -- one sentence, in
    /// one place, because a maker who meets it twice should not wonder whether they met two
    /// different problems.
    static constexpr const char* kNoSetupFile =
        "no setup file -- start Workshop with --setup <path>";

    /// ...and for the pane-definition file, a third sentence for the third reason.
    static constexpr const char* kNoPaneFile =
        "no pane file -- start Workshop with --pane <path>";


    /// The versions a `Shape v<N>` can name. `parse_u64` answers in 64 bits and
    /// a schema version is 32, so a wider number is REFUSED rather than
    /// truncated -- `send @x Foo 4294967297` must not quietly become version 1.
    static constexpr std::uint64_t kMaxVersion = 0xFFFFFFFFull;

    HostContext* host_;
    Session session_;

    /// THE QUIT IN FLIGHT: whether the room has been asked and not yet fully answered, which
    /// ask (its correlation), how many answers are still owed, what the refusals said, and
    /// the gestures held meanwhile. None of it is durable and none of it survives the quit:
    /// the last answer, or a delivery Loom refused, ends it (`refuse_quit`, `finish_quit`).
    /// THE ONE EDIT CODE OPEN IN FLIGHT: the pane it was spent on, the code the host named for it
    /// at the spend, and the ticket Loom handed back. Not durable: a reload of this desk is not a
    /// thing, and a newer Edit Code replaces it (the manager then answers the older ask, and that
    /// answer matches nothing here).
    // WL-CODE-03 -- agents/workshop/code.md
    struct CodeOpen {
        bool live = false;
        std::uint64_t ask = 0;      ///< the correlation the answer must carry
        loom::Ticket attempt{};     ///< the queued send, matched by Loom's refusal notice
        PaneRef pane;
        std::string name;           ///< what the maker sees the pane called
        HostContext::CodeSource code; ///< the host's answer at the spend
        std::string recipe;         ///< the one recipe whose source was asked for
        std::string source;
    };
    CodeOpen code_open_;
    std::uint64_t code_asks_ = 0;

    bool quitting_ = false;
    /// THE MINT FOR `PictureFence` NUMBERS: from one, one per canvas that handed out a picture.
    std::int64_t fences_ = 0;
    /// THE MINT FOR A PRESENTED MENU'S NUMBER: from one, one per grant, never reused.
    std::int64_t menus_ = 0;
    /// MENUS OFF THE SCREEN WHOSE REQUESTERS MAY STILL BE OWED AN ANSWER, oldest first: each from
    /// its withdrawal until Loom refuses one of its sentences (answered then) or its fence comes
    /// round twice (forgotten) -- so only withdrawals that recent are ever here.
    std::vector<WithdrawnMenu> withdrawn_;
    /// EVERY GESTURE THIS HOST HANDLED -- a key, text, a button PRESS, the wheel -- counted, so a
    /// pane's word about one of them can be asked whether it is still about the latest. A button's
    /// release completes the gesture its press began and is not counted (`on(PointerButton)`).
    std::uint64_t gestures_ = 0;
    /// THE LAST BARE ESCAPE SENT TO A PANE: which pane, which gesture it was, and the
    /// correlation it went out under -- the identity an answer must echo to be about THAT
    /// Escape. Current desk state cannot identify an event: a second Escape leaves the pane,
    /// the selection and the gesture count all matching again, so the first Escape's answer
    /// would borrow the second's record without this.
    struct EscapeSent {
        std::int64_t kind = kNoPaneKind;
        std::uint64_t gesture = 0;
        std::uint64_t answering = 0;
    };
    EscapeSent escape_sent_;
    /// THE LAST DECLARED ACTION SENT TO A PANE (`PaneActionRequested`), on `escape_sent_`'s
    /// terms: which pane, which gesture, which number -- the identity a menu request opened by
    /// key must echo to be about THAT keystroke. Every action goes out under a number now, not
    /// only an Escape; a pane that never echoes one is unchanged.
    EscapeSent action_sent_;
    /// THE SECOND BUTTON: release custody (a hold) and continuation eligibility are TWO records,
    /// one pair per secondary button (2 and 3). A hold begins when the press is queued and ends
    /// on the release, on owner loss or on arbitration -- nothing else ends it. A continuation
    /// is the newest press of its button: eligible to be handed back or to open a menu while it
    /// is unspent, its pane is still on the desk, and the only gesture since it is its own
    /// release; a release never restores it, and a press of the OTHER secondary button
    /// interrupts it. (WL-PRESS-06)
    struct SecondaryHold {
        bool active = false;
        std::int64_t kind = kNoPaneKind;
        std::int64_t button = 0;
        std::uint64_t correlation = 0;
        /// THE QUEUED PRESS SEND, matched by Loom's later refusal notice. A press Loom attests it
        /// never delivered settles here: the hold and its continuation are dropped, so the
        /// physical release does not send a release for a press that never reached a recipient
        /// (WL-PRESS-06, the review's fourth finding). A valid ticket is not delivery -- only that
        /// something was queued; silence from a recipient that DID hear it is a different fact and
        /// leaves the hold standing.
        loom::Ticket attempt{};
    };
    struct SecondaryContinuation {
        bool live = false;
        std::int64_t kind = kNoPaneKind;
        std::int64_t button = 0;
        std::uint64_t correlation = 0;
        std::uint64_t gesture_at_press = 0;
        bool released = false;
        bool interrupted = false;
        bool spent = false;
        PointedAt cell;
    };
    SecondaryHold secondary_hold_[2];
    SecondaryContinuation secondary_cont_[2];
    /// THE LAST MENU CHOICE A PRESENTER REPORTED FOR A PANE: the ask's number (which the answer
    /// echoed), the act that made the choice, and the pane -- what a `PaneManageRequested` or a
    /// `PaneKeyboardRequested` must echo to continue that choice, honored while that act is still
    /// the maker's latest. One record: a newer choice retires the older one.
    struct ChoiceAnswered {
        std::int64_t kind = kNoPaneKind;
        std::uint64_t gesture = 0;
        std::uint64_t correlation = 0;
        PointedAt cell;
        bool spent = true;
    };
    ChoiceAnswered choice_answered_;
    /// ⭐ THE ONE APPLICATION ROW THIS HOST IS WAITING ON AN ANSWER TO, and the keystroke it
    /// was raised by. `escape_sent_` one owner over, and for its exact reason: the desktop's
    /// reply arrives in a later delivery, by which time the maker may have pressed again, put
    /// a different pane down or picked a different one up. An answer that does not echo THIS
    /// number, or arrives after another gesture, acts on nothing.
    struct AppAsked {
        std::uint64_t gesture = 0;
        std::uint64_t answering = 0;
    };
    AppAsked app_asked_;
    std::uint64_t app_asks_ = 0;
    /// THE NUMBERS THOSE ESCAPES GO OUT UNDER, minted here and nowhere else. Monotonic from
    /// one, so zero is never an Escape: an answer that echoes nothing answers nothing. Gaps
    /// are meaningless -- an Escape this host answers itself burns a number and sends none.
    /// ⭐ ONE COUNTER FOR EVERY NUMBER THIS HOST MINTS FOR A PANE'S GESTURE -- Escapes, declared
    /// actions and secondary presses -- so a word echoing one can never match a record of
    /// another kind by coincidence.
    std::uint64_t escape_asks_ = 0;
    std::uint64_t quit_ask_ = 0;
    std::size_t quit_outstanding_ = 0;
    std::vector<std::string> quit_refusals_;
    std::vector<HeldInput> held_input_;
    std::size_t held_dropped_ = 0;

    /// THE ASKER'S OWN BOOK OF PASTES STILL IN FLIGHT, and the drafts each one belongs to.
    // WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
    loom::AskBook paste_asks_{4};
    std::vector<PendingPaste> pending_pastes_;

    /// One moment's worth of memory: the character the gesture's OWN keystroke
    /// produced, which is not text a maker typed. Set by a gesture that opens a
    // WL-KEY-12 -- agents/workshop/keyboard.md
    std::string swallow_text_;

    /// WHETHER THIS PROCESS HAS ALREADY TRIED TO TAKE BACK ITS LAST SESSION.
    // WL-SESSION-14 -- agents/workshop/session-restore.md
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
    /// WHETHER THIS RUN HAS TRIED TO READ ITS PANE-DEFINITION FILE, and whether
    /// that file was REFUSED.
    // WL-MAKER-09 -- agents/workshop/maker-pane.md
    bool pane_loaded_ = false;
    bool pane_refused_ = false;
    std::string keymap_word_;
    /// THE APPLICATION ROWS THE DESKTOP LAST DECLARED, AS IT DECLARED THEM -- retained for the
    /// same reason a pane's declaration is retained on its catalog row: a keymap file read
    /// afterwards is owed the maker's overrides over these ids too, so the declaration has to
    /// survive the join it was admitted by (WL-DESK-07). `app_declaration_` is Workshop's number
    /// for it, 0 when none is in force.
    std::vector<AppActionRow> app_actions_;
    std::int64_t app_declaration_ = 0;
    /// THE MINT FOR DECLARATION NUMBERS, pane and application alike: from one, never reused.
    std::int64_t declarations_ = 0;
    /// THE LAST INVENTORY THIS HOST SAID OUT LOUD, so it does not say it again unchanged -- and
    /// whether it has said one since a presenter last arrived (`on(PaneOffered)` clears it).
    std::vector<InventoryPane> inventory_said_;
    bool inventory_published_ = false;
    /// ...AND THE SAME RECORD FOR THE EFFECTIVE KEYMAP, and what reading the keymap file came to,
    /// kept for as long as it is true (the startup note is said once; this is not).
    KeymapShown keymap_said_;
    bool keymap_published_ = false;
    std::string keymap_standing_;
    /// ...AND FOR THE INSPECTOR'S SUBJECT (WL-INFO-14): what was last said, and whether it has
    /// been said since a presenter last arrived.
    PaneSubjectShown subject_said_;
    bool subject_published_ = false;
    /// Which generation of the host's standing list this weave has taken.
    std::uint64_t conditions_taken_ = 0;
    bool keymap_bad_ = false;
    /// THE BYTES OF THE KEYMAP FILE AS THIS HOST LAST READ OR WROTE THEM, and whether a file was
    /// there at all -- the comparison baseline an edit's write is judged against: a file another
    /// hand changed since is never overwritten (WL-KEY-18). Refreshed after every write of
    /// this host's own.
    std::string keymap_bytes_;
    bool keymap_file_present_ = false;
    bool startup_spoken_ = false; ///< the one combined startup sentence has been said

    /// What loading the PREFS file produced, the keymap's own bookkeeping one file over.
    // WL-FOCUS-11 -- agents/workshop/focus.md
    bool prefs_loaded_ = false;
    bool prefs_bad_ = false;

    /// WHAT THIS WEAVE LAST SAID ABOUT WHAT IS TRUE, and whether it has said anything at
    /// all. Not a cache of the conditions: it is this publisher's record of its own last
    /// utterance, which is what makes the publication silent when nothing changed and is
    /// what stops the pane seam looping (`say_conditions`).
    // WL-ATTN-12 -- agents/workshop/attention.md
    /// THE WIRE SHAPE AND NOT `Condition`, deliberately: what is compared has to be what
    /// is SAID, and the sentence carries a resolved suggestion where the condition carries
    /// an action id. Comparing the internal form would be comparing something this weave
    /// never published.
    std::vector<StandingCondition> said_conditions_;
    bool conditions_said_ = false;

    /// ...and the same pair for the terminal participant's record.
    TranscriptShown said_transcript_;
    bool transcript_said_ = false;

    /// WHETHER THIS RUN'S MEDIUM HAS REPORTED A DESKTOP PLACEMENT.
    // WL-SESSION-09 -- agents/workshop/session-restore.md
    bool medium_placed_ = false;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_WEAVE_HPP
