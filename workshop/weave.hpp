// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WEAVE_HPP
#define ZENGINE_WORKSHOP_WEAVE_HPP
#include "setup_control.hpp"

// Workshop's own weave: the session, and the bindings from input moments to maker gestures.
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
#include "pane_operation.hpp"
#include "pane_shortcuts.hpp"
#include "pane_carry.hpp"
#include "pane_view.hpp"
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

/// What the weave needs from the host and cannot get by message; kept whole in this header so a
/// suite can build one without linking the host.
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

    /// Is this office still to come? Read from the realization owner's rows at the ask: pending,
    /// never unavailable. Empty answers no.
    // WL-DESK-04 -- agents/workshop/desktop.md
    std::function<bool(std::string_view office)> office_pending;

    /// WHAT MONOTONIC TIME IT IS, ANSWERED BY THE HOST -- `frontier`'s seam exactly.
    // WL-PTR-01 -- agents/workshop/pointer.md
    std::function<std::int64_t()> interaction_now;

    /// Does whoever holds `role` now accept `shape`? Read off the bus at the call
    /// (`holder_accepts_on`). It picks which published version to send and promises nothing about
    /// delivery; empty answers no, and the first version crosses.
    // WL-FOCUS-04 -- agents/workshop/focus.md
    std::function<bool(std::string_view role, const loom::Schema& shape)> holder_accepts;
    // Current incarnation, read afresh: canvas grants and held input never cross replacement.
    std::function<loom::WeaveId(std::string_view role)> role_holder;
    // A host-issued capability to read this actor's current authority. The ceiling is empty;
    // it never grants the actor anything. Unknown/dead actors yield an inert capability.
    std::function<loom::GrantAuthority(loom::WeaveId)> input_authority;

    /// Where a terminal line can be addressed now, read off the bus at the call and kept nowhere.
    /// Empty lists nothing, and the completer offers the address forms.
    // WL-TERM-16 -- agents/workshop/terminal.md
    std::function<std::vector<Destination>()> destinations;

    /// What the recipe catalog says about one recipe's source, answered by the host through the
    /// read-only project office.
    // WL-PROJ-02 -- agents/workshop/project.md
    struct RecipeSource {
        bool known = false; ///< the id names an authored recipe of this project
        std::string kind;   ///< `single_source` or `cmake_target`, the file's own words
        std::string source; ///< the one authored source file; empty when the kind has none
    };

    /// The host wires it over its recipes; the door spends it at the ask and stores nothing.
    // WL-PROJ-02 -- agents/workshop/project.md
    std::function<RecipeSource(const std::string&)> recipe_source;

    /// What stands behind one office's running code, read at the ask from the three owners of its
    /// edges: the bus (office -> weave), the realization owner (weave -> artifact) and the catalog
    /// in force (artifact -> recipes). Empty fields are absences a sentence names; choosing a
    /// recipe and opening anything are not this answer's.
    // WL-CODE-01 -- agents/workshop/code.md
    struct CodeSource {
        /// One authored recipe producing the artifact. `source` is a single-source recipe's file as
        /// the build reads it -- an editing entry, not its includes -- and empty for
        /// `cmake_target`.
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

    /// A maker's recipe row as typed. The host composes it, checks it by the recipe law, appends it
    /// as authored and installs the file; the answer is the catalog in force (`RecipeSwap`).
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
        std::string config;              ///< cmake-target: a multi-config generator's
                                          ///< configuration, or empty
        bool tree = false;               ///< which of the two kinds this draft is
    };
    std::function<RecipeSwap(const RecipeDraft&)> author_recipe;

    /// What authoring the minimum plan row came to: appended and written, or refused in the plan's
    /// or the executor's words -- and the recipe's product when the new row is the frontier and the
    /// product exists, so `o` can finish with the button's own act.
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

    /// Does the plan in force already name this artifact? The host answers; the weave stores
    /// nothing.
    // WL-AUTH-02 -- agents/workshop/authoring.md
    std::function<bool(const std::string& stem)> plan_names;

    /// An object document this run was pointed at or found and will not open, or empty: said once
    /// at startup and left exactly as it is.
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

    /// The one pane whose presentation this host claims and commits jointly with its document,
    /// spelled by the host; empty means none is managed.
    PaneRef managed_pane;

    /// THE BOOK A NATIVE SHOWING THAT DID NOT COMPLETE IS WRITTEN IN (`host_pump.hpp`): this
    /// weave's publication hook keeps its own words here, and the host's turn tells them.
    ShowingFailures showings;

    /// THE BOOK A QUIT DELIVERY LOOM REFUSED IS WRITTEN IN (`quit_delivery.hpp`): the host's
    /// watch writes what the tap said, and this weave reads it when the watch wakes it.
    // WL-SESSION-19 -- agents/workshop/session.md
    UndeliveredQuits undelivered_quits;

    /// What the host already knew was true, and still is. It can grow after the first picture (an
    /// optional row refusing inside a delivery), hence `conditions_generation`.
    // WL-ATTN-01 -- agents/workshop/attention.md
    std::vector<Condition> standing_conditions;
    /// How many times that list grew: compared, so a repaint costs one integer.
    std::uint64_t conditions_generation = 0;

    /// An artifact stem as this platform spells a shared library: the host's one rule, so one plan
    /// is legal on every platform. A view, so a literal and a stem read from a file resolve alike.
    std::string so(std::string_view stem) const { return so_in(dir, stem); }

    /// The same rule aimed at another directory (a CMake target lands where its own project puts
    /// it). Static: a fact about the platform. One copy of the suffix rule.
    static std::string so_in(std::string_view directory, std::string_view stem) {
        return std::string(directory) + "/" + std::string(stem) +
#if defined(_WIN32)
               ".dll";
#else
               ".so";
#endif
    }
};

/// The host's answer to `holder_accepts`, read off `bus` at the call: the weave holding `role` now,
/// and whether its accept-set has a door of exactly `shape`'s identity. Nobody holding the role, or
/// a holder accepting by mode, is no.
bool holder_accepts_on(const loom::Switchboard& bus, std::string_view role,
                       const loom::Schema& shape);

/// The host's answer to `destinations`, read off `bus` at the call: every weave that is not a
/// sealed candidate, its office, accepted shapes and liveness, and `self` marked. A reading, not a
/// registry.
std::vector<Destination> bus_destinations(const loom::Switchboard& bus, loom::WeaveId self);

/// The Workshop weave.
class WorkshopWeave
    : public loom::WeaveBase<WorkshopWeave, WorkshopState,
                             loom::Accept<PaneShortcutInvoked, PaneViewRequested, PanePointRequested, PaneObservationRequested, PaneObservationContinued, PaneObservationEnded, input::AttributedInput, PaneOperationRequested, PaneCarryRequested, PaneValueCarryRequested, v2::PaneValueCarryRequested, zengine::workshop::PaneCanvasContent, zengine::input::KeyPressed, zengine::input::TextEntered,
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
                                          zengine::workshop::v2::PaneOffered,
                                          zengine::workshop::SetupApplyRequested,
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
                                          zengine::workshop::TerminalValueRequested,
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
                                          // the medium
                                          zengine::workshop::PictureFence,
                                          // the presenter participant's half of a pane's menu:
                                          // what it shows, when it ends it, and that it arrived
                                          zengine::workshop::MenuShown,
                                          zengine::workshop::MenuClosed,
                                          zengine::workshop::MenuReturned,
                                          zengine::workshop::PresenterReady,
                                          // ...and the host's own fence behind a menu it
                                          // withdrew, whose requester may still be owed
                                          zengine::workshop::WithdrawalFence,
                                          loom::DispatchRefused>,
                             loom::Emit<loom::Ack, loom::Refused, PaneView, PanePoint, PaneObservationAnswered, PaneOperationAnswered, PaneCarryAnswered, PaneDrop, PaneValueDrop, v2::PaneValueDrop, zengine::workshop::PaneCanvasRoom,
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
                                        zengine::workshop::TerminalValueAnswered,
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

    /// Say what is true right now to anyone presenting it -- only when it changed: without the
    /// comparison, the publication, the pane's answer and the repaint would never stop. What it
    /// remembers is its own speech, not the truth (WL-ATTN-03).
    void say_conditions(const ProjectFrontier& frontier, loom::Mail& mail);

    /// A Skin claimed the surface: give it the whole screen, and ask the room who has panes,
    /// office-published as `zengine.workshop` so a provider can verify it. `SurfaceReady` rather
    /// than an activation, which Loom sends no native mount. This makes discovery converge in
    /// either load order, and repeating it is harmless: a re-offer refreshes in place.
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

    void on(const input::AttributedInput& event, loom::Mail& mail);
    void on(const PaneViewRequested& asked, loom::Mail& mail);
    void on(const PanePointRequested& asked, loom::Mail& mail);
    /// A pane's visible text body and what it shows, or why neither is available.
    struct VisibleBody {
        std::int64_t kind = 0;
        const ExternalPane* content = nullptr;
        ExternalBodyPlace body;
    };
    std::string visible_text_body(const std::string& provider, const std::string& pane,
                                  VisibleBody& out) const;
    bool cell_center(const VisibleBody& visible, std::int64_t row, std::int64_t column,
                     std::int64_t& x, std::int64_t& y, std::int64_t& space, bool exact_column) const;
    void on(const PaneShortcutInvoked& asked, loom::Mail& mail);
    /// The current attributed gesture of `pane` approves one (shape, version, role), or why not.
    /// Spends the gesture; sets nothing else.
    std::string approve_gesture(const std::string& pane, std::int64_t gesture, const std::string& role,
                                const std::string& shape, std::int64_t version, loom::Mail& mail);
    std::string authorize_pane_operation(const PaneOperationRequested& asked, loom::Mail& mail);
    void on(const PaneOperationRequested& asked, loom::Mail& mail);
    void on(const PaneObservationRequested& asked, loom::Mail& mail);
    void on(const PaneObservationContinued& asked, loom::Mail& mail);
    void on(const PaneObservationEnded& asked, loom::Mail& mail);
    /// The observation leases this host holds now -- a reading for tests and diagnostics.
    std::size_t observation_leases() const noexcept { return leases_.size(); }
    void on(const TerminalValueRequested& asked, loom::Mail& mail);
    void on(const PaneCarryRequested& asked, loom::Mail& mail);
    void on(const PaneValueCarryRequested& asked, loom::Mail& mail);
    void on(const v2::PaneValueCarryRequested& asked, loom::Mail& mail);
    void accept_carry(const PaneCarryRequested& asked, bool value, bool drag, loom::Mail& mail,
                      std::string token = {});
    bool drop_carry(std::int64_t kind, const ExternalPressAt& at, loom::Mail& mail,
                    std::int64_t picture = -1);
    PointedAt drag_pointer_;
    void begin_value_drag(const input::PointerButton& button);
    bool move_value_drag(const input::PointerMoved& motion, loom::Mail& mail);
    bool release_value_drag(const input::PointerButton& button, loom::Mail& mail);
    void finish_value_drag(loom::Mail& mail);
    /// A key transition: which key changed, and what was held when it did.
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

    /// A button-1 PRESS WHILE THE SURFACE IS OPEN.
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

    /// WHAT A button-1 RELEASE ENDED — and it is asked, not assumed.
    ///
    /// `pane` is meaningful only when `pane_held`.
    struct GesturesEnded {
        bool pane_held = false;
        PaneRef pane;
    };

    /// END EVERY button-1 GESTURE THIS SESSION IS HOLDING, whatever mode saw the release.
    GesturesEnded end_held_gestures();

    /// A pointer button changed, AND the position it changed at.
    void on(const zengine::input::PointerButton& b, loom::Mail& mail);

    /// The pointer moved; outside a drag this weave has nothing to do with it.
    void on(const zengine::input::PointerMoved& m, loom::Mail& mail);

    /// THE WHEEL TURNED.
    void on(const zengine::input::PointerWheel& w, loom::Mail& mail);

    // ---- The external pane seam: an office offers, Workshop grants, an office says ----------
    // Discovery adds a row a maker may choose; content fills a pane already open -- a provider
    // can put a pane in the list, never on the screen. Both are authenticated by
    // `mail.authored_role()`, the office Loom verified: personal speech, even from the holder,
    // registers nothing (Loom MSG-07). A role is a live service route, not an author's identity.

    /// AN OFFICE OFFERS A PANE. Admitted, refreshed, or refused -- and every one of those
    /// is bounded before a byte is retained.
    void on(const PaneOffered& offer, loom::Mail& mail);
    void on(const v2::PaneOffered& offer, loom::Mail& mail);
    void on(const SetupApplyRequested& request, loom::Mail& mail);
    void accept_pane_offer(const PaneOffered& offer, loom::Mail& mail,
                           std::int64_t rows, std::int64_t columns);

    /// An office declares what one of its panes can do, judged whole under the offer's stamp
    /// (`admit_pane_actions`, then `join_pane_rows` and the collision law) and retained only when
    /// both pass; a refusal changes nothing.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void on(const PaneActions& actions, loom::Mail& mail);

    /// The same declaration, in the version that can name an action the pane owns.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void on(const v2::PaneActions& actions, loom::Mail& mail);

    /// THE ONE BODY BOTH VERSIONS SPEND, over the host's own row type.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void declare_pane_actions(const std::string& pane,
                              const std::vector<v2::PaneActionRow>& rows, loom::Mail& mail);

    /// Re-join every pane's retained declaration into the keymap now in force: the file may arrive
    /// after a pane did, and its overrides are owed to that pane's rows too.
    // WL-KEY-15 -- agents/workshop/keyboard.md
    void rejoin_pane_rows(std::string& refusals, loom::Mail& mail);

    // ---- The participating owner of the application's default behaviour (WL-DESK) --------

    /// The desktop declares what the application answers to, judged whole by `join_app_rows`; the
    /// verdict is said on the band and answered to the declaration (`ActionsJudged`).
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

    /// Answer one declaration with Workshop's verdict, when the declaring office's holder accepts
    /// the shape; a refusal is said on the band too. The fact is Workshop's, the recovery the
    /// pane's.
    void answer_declaration(const std::string& office, ActionsJudged verdict, loom::Mail& mail);

    /// TELL A DECLARING OFFICE THAT A DECLARATION IT HAD IN FORCE LEFT THE KEYMAP, naming the
    /// number the verdict gave it, when that office's holder accepts the shape.
    void say_withdrawn(const std::string& office, ActionsWithdrawn withdrawn, loom::Mail& mail);

    /// JOIN THE APPLICATION ROWS IN FORCE AGAIN, under the keymap now in force -- what a keymap
    /// file's load owes them, before the panes are joined again. A declaration the file's rows now
    /// collide with is withdrawn and told so; it is not retried.
    void rejoin_app_rows(std::string& refusals, loom::Mail& mail);

    /// Seat the pane that asks, in this delivery, or refuse with nothing moved: judged through the
    /// launch door's trial; on a seat the pane is authored if it was not, selected and given the
    /// keys before `PaneRevealAnswered` is said. The host holds nothing across it.
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

    /// Author one line as the terminal participant, answered at this office: the party holding
    /// the participant answers for it.
    void on(const TerminalActRequested& asked, loom::Mail& mail);

    /// What could be said next, given a line the asker holds. It reads and never authors: every
    /// method on the path is const.
    void on(const TerminalCompletionRequested& asked, loom::Mail& mail);

    /// An office says what its pane says: judged whole against the room last granted -- authorship,
    /// identity, room, every row -- and copied only then. An application retention bound, not a
    /// decode-memory one.
    void on(const PaneContent& content, loom::Mail& mail);
    void on(const PaneCanvasContent& content, loom::Mail& mail);

    /// Is this update inside the room granted, and can a canvas carry every row? Pure, and judged
    /// before anything is copied: row count, row width, and `SurfaceTextRow`'s plain-ASCII
    /// contract. Refused whole, never truncated. Role and ground are not judged: the Surface
    /// answers an unknown one.
    static Written judge_content(const PaneContent& content, const ExternalPane& pane);

    /// Where a pane says its caret is, accepted or refused whole. It repaints only when the
    /// admitted caret differs: a caret arriving beside its rows rides their repaint.
    void on(const PaneCaret& caret, loom::Mail& mail);

    /// Is this caret inside the content this pane last had accepted (`shown`, not the room)? One
    /// past a row's last byte is legal: the end of a line.
    static Written judge_caret(const PaneCaret& caret, const ExternalPane& pane);

    // ---- The presentation owner's half of a managed opening (WL-OPEN) -----------------------
    // Bodies in weave_managed.cpp.

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
    /// THE PRESENTER ENDED THE OPEN MENU AND ANSWERED ITS REQUESTER; a choice is recorded as the
    /// continuation of the act the presenter names, if that act was one this host forwarded to
    /// it. One about a menu already withdrawn says nothing more is owed, and its record goes.
    void on(const MenuClosed& closed, loom::Mail& mail);
    /// THE PRESENTER GAVE AN INTERACTION BACK -- it holds no such menu, so that requester has no
    /// answer coming: this host settles it, open or withdrawn, and nothing newer is touched.
    void on(const MenuReturned& returned, loom::Mail& mail);
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
    /// The publication hook: the trial becomes the desk before any observer (Applied), or a
    /// presentation this desk did not prepare or could not seat is Declined and re-claimed at its
    /// next delivery. It runs inside the host's showing boundary, so a throw is Failed.
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
    /// ...and the room grant the publication owes the pane it seated, said at the end of the
    /// delivery through the one door every room goes through.
    bool room_owed_ = false;

    /// IS THIS THE CHARACTER THAT KEY PRODUCED?
    static bool same_keystroke(const std::string& text, const std::string& owed);

    /// One action's effective spelling for this weave's notices: `hotkey_text`, as every surface.
    std::string hotkey(Act a) const;

    /// Which of this weave's own editable places a consumed paste request came from; `kNone` for
    /// every armless branch. A pane asks the Skin for the clipboard itself.
    // WL-TEXT-09 -- agents/workshop/text-box.md
    enum class PasteOwner : std::uint8_t { kNone, kNaming };

    /// THE ONE-LINE NAME EDITOR THAT IS OPEN, or nothing -- the layout's.
    component::TextBox* naming_line();

    /// Which draft would the chain have handed the clipboard to? A projection of the one resolved
    /// context, so a paste cannot reach a draft the keys never did.
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

    /// The pending-paste record a settled conversation belongs to, removed and handed back; an id
    /// the list does not hold answers the empty record.
    PendingPaste take_pending_paste(std::uint64_t ask);

    // ---- The terminal participant, behind a door ------------------------------

    /// WHAT THE PARTICIPANT'S RECORD HOLDS, TO WHOEVER IS PRESENTING IT -- derived on the
    /// repaint, compared against the last utterance, and said only when it changed.
    void say_transcript(loom::Mail& mail);

    /// Compose and author one line through the participant, in Loom's own grammar: the one path
    /// that speaks as the terminal, stamped with its identity and judged against its grant -- why
    /// the participant stayed this host's (WL-TERM-02).
    void submit_terminal_line(const std::string& line);

    /// Command mode.
    void command(const zengine::input::KeyPressed& k, loom::Mail& mail);

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

    /// What to say about a layout's setup association after an operation: nothing, or the artifact
    /// and the verdict.
    std::string link_note(std::size_t at) const;

    /// Which artifact `s` and `r` act on for the live layout: its own association, or the host's
    /// configured setup path.
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

    /// Arrange one pane: the pane-local scope, on an explicit target -- the context menu's
    /// captured subject, or the desk's keyboard target.
    void enter_arrange_pane(const PaneRef& ref);

    /// THE ACTIVE SETUP NO LONGER NAMES THE ADDRESSED PANE, SO NOTHING DOES.
    void forget_removed_selection();

    /// Leave the arrangement whole: scope, target and reset prompt together, so a later open cannot
    /// inherit a stale address.
    void close_arrange();

    /// What a maker reads about the pane the vocabulary addresses, spent by every gesture that
    /// succeeds, so the notice line names what just moved and its state.
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

    /// A PRESS INSIDE THE LAYOUTS PANE -- the tab run's own inverse, and the whole
    /// of what the top band's two global pointer arms became.
    bool layouts_press(const zengine::input::PointerButton& b, loom::Mail& mail);

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

    /// KEEP THE NAME EDITOR'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW.
    void refresh_setup_name();

    void say(std::string text, bool bad);

    /// The status line: which layout is live, and what the desk's file says about it.
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

    /// Tell a provider a maker pressed in its room; answers whether the press named a row of the
    /// granted body. `at` and `keys_went_here` are the picture pressed, read before the press moved
    /// the keyboard; it crosses once, as `v2::PanePressed` to a holder that accepts it, else v1.
    bool external_press(std::int64_t kind, const ExternalPressAt& at, bool keys_went_here,
                        loom::Mail& mail);

    /// Tell the pane a press began in that the hand moved, resolved against its body at this motion
    /// and unclamped (`PaneDragged`); a pane that lost its seat or room ends the sweep, sending
    /// none.
    void external_drag(std::int64_t kind, const zengine::input::PointerMoved& m,
                       loom::Mail& mail);

    /// WHICH EXTERNAL PANE THE KEYBOARD IS POINTED AT RIGHT NOW, or `kNoPaneKind`.
    std::int64_t keyboard_pane() const;

    /// Tell a provider a key went down while its pane held the keyboard, as the office, to the
    /// office. Workshop never reads the pane's rows to decide what a key means. A gesture the pane
    /// declared a row for crosses as its resolved id (WL-KEY-15); a bare Escape to a holder that
    /// can spend no key crosses as nothing. Answers whether a sentence crossed.
    bool external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
                      loom::Mail& mail);

    /// The wheel turned over an external pane's body: sent only over a prose row of a pane that
    /// holds a room, the notches unchanged. It follows the pointer, not the keyboard.
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
    /// Settle a secondary press Loom attests it never delivered, by the attempt its refusal names:
    /// the hold and its continuation drop; an old attempt cancels nothing newer (WL-PRESS-06).
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
    /// STOP KEEPING WHO ASKED FOR A WITHDRAWN MENU, if this host still is: nothing more is owed
    /// about it. The fence's second hop and the presenter's own `MenuClosed` both end here.
    void forget_withdrawn(std::int64_t menu);
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

    /// ...and the text the platform made of it: `external_key`'s twin, and a separate send because
    /// a key may make no text and text may come with no key.
    void external_text(std::int64_t kind, const zengine::input::TextEntered& t,
                       loom::Mail& mail);

    // ---- THE RUN: what is true now, the picture published, and the way out --------------

    /// THE PROJECT FRONTIER, READ ALIVE, NOW.
    ProjectFrontier frontier_now() const;

    /// What monotonic time it is: the host's reading if it wired one, the steady clock if not;
    /// spent at once by the caller, stored nowhere.
    std::int64_t interaction_now() const;

    void repaint(loom::Mail& mail);
    /// A NUMBERED PICTURE THE CANVAS JUST HANDED THE MEDIUM is recorded as in flight, and one
    /// fence is sent behind the canvas for all of them (none when no picture moved).
    void fence_pictures(loom::Mail& mail);

    /// Leave, by asking the room first. A dirty maker-made definition refuses synchronously; every
    /// pane accepting `PaneQuitRequested` is asked, and the count Loom hands back is what this
    /// host waits for. None owed means the exit proceeds now.
    void quit(loom::Mail& mail);

    /// THE EXIT ITSELF: write down the desk, then stop the bus. The one place both happen.
    void finish_quit();

    /// The quit in flight ends without an exit (a pane's refusal, or a question Loom could not
    /// deliver): `why` is said with the count of gestures dropped, the held gestures replay in
    /// order, and the desk repaints. Nothing is saved and the bus keeps running.
    void refuse_quit(std::string why, loom::Mail& mail);

    struct InputActor {
        bool known = false;
        bool local = false;
        loom::WeaveId participant{};
    };
    struct ApprovedOperation {
        loom::WeaveId pane_owner{};
        std::string pane;
        std::uint64_t correlation = 0;
        std::uint64_t gesture = 0;
    };
    ApprovedOperation approved_operation_;
    /// OBSERVATIONS A CURRENT GESTURE APPROVED (`pane_operation.hpp`): one per pane, bounded, and
    /// re-judged at every continuation. Never a grant, never reload-kept.
    struct ObservationLease {
        std::int64_t id = 0;
        loom::WeaveId holder{};
        std::string office, pane, role, shape, subject;
        std::int64_t version = 0;
        InputActor actor;
    };
    static constexpr std::size_t kMaxObservationLeases = 16;
    std::vector<ObservationLease> leases_;
    std::int64_t next_lease_ = 0;
    struct CarriedData {
        loom::Bytes data;
        std::string label;
        InputActor actor;
        bool value = false;
        bool drag = false;
        std::string source_office, source_pane, token;
    };
    CarriedData carried_;
    struct ValueDrag {
        std::uint64_t gesture = 0;
        InputActor actor;
        std::int64_t x = 0, y = 0, space = 0;
        bool moved = false, released = false;
        std::int64_t target = kNoPaneKind, picture = 0;
        ExternalPressAt at;
        loom::WeaveId receiver;
    };
    ValueDrag value_drag_;
    InputActor input_actor_;
    InputActor gesture_actor_;
    loom::WeaveId attributed_producer_{};
    bool applying_attributed_ = false;
    bool duplicate_input(const loom::Mail& mail) const;

    /// One input gesture held while a quit is pending, kept whole so a refused quit can replay it.
    struct HeldInput {
        InputActor actor;
        enum class Kind : std::uint8_t { kKey, kText, kButton, kMoved, kWheel };
        Kind kind = Kind::kKey;
        zengine::input::KeyPressed key;
        zengine::input::TextEntered text;
        zengine::input::PointerButton button;
        zengine::input::PointerMoved moved;
        zengine::input::PointerWheel wheel;
    };

    /// How many gestures a pending quit will hold: one poll's burst; past it they are dropped and
    /// counted, and the count is said with the refusal.
    static constexpr std::size_t kMaxHeldInput = 256;

    /// HOLD ONE GESTURE WHILE THE ROOM IS BEING ASKED, or say it need not be held. Every
    /// input handler asks this first: while a quit is pending nothing reaches a pane, which is
    /// what makes a pane's "clean" answer a fact about the instant the process ends.
    bool hold_input(HeldInput held);

    /// PLAY THE HELD GESTURES BACK, IN ORDER, through the same handlers they arrived at --
    /// the refused quit's promise that a maker who typed through it lost nothing.
    void replay_held(loom::Mail& mail);

    /// What to say when there is no setup file to save to or restore from, said in one place.
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

    /// The one Edit Code open in flight: the pane, the code the host named for it at the spend, and
    /// Loom's ticket. Not durable; a newer Edit Code replaces it.
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
    /// Menus off the screen whose requesters may still be owed an answer, oldest first, each kept
    /// until Loom refuses one of its sentences or its fence comes round twice.
    std::vector<WithdrawnMenu> withdrawn_;
    /// EVERY GESTURE THIS HOST HANDLED -- a key, text, a button PRESS, the wheel -- counted, so a
    /// pane's word about one of them can be asked whether it is still about the latest. A button's
    /// release completes the gesture its press began and is not counted (`on(PointerButton)`).
    std::uint64_t gestures_ = 0;
    /// A gesture sent to a pane: the pane, the gesture and the correlation it went under -- the
    /// identity an answer must echo, because current desk state cannot identify an event.
    struct GestureSent {
        std::int64_t kind = kNoPaneKind;
        std::uint64_t gesture = 0;
        std::uint64_t answering = 0;
    };
    GestureSent escape_sent_;
    /// The last declared action sent to a pane, on `escape_sent_`'s terms: what a menu request
    /// opened by key must echo.
    GestureSent action_sent_;
    GestureSent shortcut_sent_;
    /// The last primary press sent to a pane, on the same terms. A pane's own drawn `[menu]`
    /// control is clicked with the primary button, so that press may continue into a menu too --
    /// judged on the same three facts, and spent once.
    GestureSent press_sent_;
    /// The second button keeps two records per button: a hold (release custody), ended by release,
    /// owner loss or arbitration; and a continuation, the newest unspent press whose pane is on the
    /// desk with nothing since but its own release (WL-PRESS-06).
    struct SecondaryHold {
        bool active = false;
        std::int64_t kind = kNoPaneKind;
        std::int64_t button = 0;
        std::uint64_t correlation = 0;
        /// The queued press send, matched by Loom's refusal notice: a press never delivered settles
        /// here, so no release is sent for it. A valid ticket means queued, not delivered.
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
    /// The last menu choice a presenter reported for a pane: what `PaneManageRequested` or
    /// `PaneKeyboardRequested` must echo to continue it, honoured while its act is the latest.
    struct ChoiceAnswered {
        std::int64_t kind = kNoPaneKind;
        std::uint64_t gesture = 0;
        std::uint64_t correlation = 0;
        PointedAt cell;
        bool spent = true;
    };
    ChoiceAnswered choice_answered_;
    /// The one application row this host awaits an answer to, and the keystroke that raised it: an
    /// answer not echoing this number, or arriving after another gesture, acts on nothing.
    struct AppAsked {
        std::uint64_t gesture = 0;
        std::uint64_t answering = 0;
    };
    AppAsked app_asked_;
    std::uint64_t app_asks_ = 0;
    /// Monotonic from one: zero is never a gesture's number, and no record matches another kind's.
    std::uint64_t gesture_asks_ = 0;
    /// The quit in flight: its ask, the answers still owed, the refusals, the gestures held.
    std::uint64_t quit_ask_ = 0;
    std::size_t quit_outstanding_ = 0;
    std::vector<std::string> quit_refusals_;
    std::vector<HeldInput> held_input_;
    std::size_t held_dropped_ = 0;

    /// THE ASKER'S OWN BOOK OF PASTES STILL IN FLIGHT, and the drafts each one belongs to.
    // WL-TEXT-09, WL-TEXT-10 -- agents/workshop/text-box.md
    loom::AskBook paste_asks_{4};
    std::vector<PendingPaste> pending_pastes_;

    /// The character a gesture's own keystroke produced, which is not text a maker typed.
    // WL-KEY-12 -- agents/workshop/keyboard.md
    std::string swallow_text_;

    /// WHETHER THIS PROCESS HAS ALREADY TRIED TO TAKE BACK ITS LAST SESSION.
    // WL-SESSION-14 -- agents/workshop/session-restore.md
    bool restored_ = false;

    /// WHETHER THE SESSION FILE THIS RUN FOUND COULD BE READ.
    // WL-SESSION-15 -- agents/workshop/session.md
    bool session_refused_ = false;

    /// Whether this run read its keymap; what reading it did waits in `keymap_word_` for the first
    /// surface. A file that could not be admitted is a standing wall (`kKeymapWallKey`).
    bool keymap_loaded_ = false;
    /// WHETHER THIS RUN HAS TRIED TO READ ITS PANE-DEFINITION FILE, and whether
    /// that file was REFUSED.
    // WL-MAKER-09 -- agents/workshop/maker-pane.md
    bool pane_loaded_ = false;
    bool pane_refused_ = false;
    std::string keymap_word_;
    /// The application rows the desktop last declared, retained so a keymap file read later applies
    /// the maker's overrides to them too (WL-DESK-07); `app_declaration_` numbers them, 0 for none.
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
    /// The keymap file's bytes as this host last read or wrote them, and whether it was there: an
    /// edit's write never overwrites a file another hand changed since (WL-KEY-18).
    std::string keymap_bytes_;
    bool keymap_file_present_ = false;
    bool startup_spoken_ = false; ///< the one combined startup sentence has been said

    /// What loading the PREFS file produced, the keymap's own bookkeeping one file over.
    // WL-FOCUS-11 -- agents/workshop/focus.md
    bool prefs_loaded_ = false;
    bool prefs_bad_ = false;

    /// What this weave last said about what is true: its own last utterance, not a cache of the
    /// conditions, so the publication is silent when nothing changed.
    // WL-ATTN-12 -- agents/workshop/attention.md
    /// The wire shape, not `Condition`: what is compared must be what was said.
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
