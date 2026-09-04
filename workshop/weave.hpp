// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_WEAVE_HPP
#define ZENGINE_WORKSHOP_WEAVE_HPP

// Workshop's own weave: the authored document, the session, and the bindings
// from input MOMENTS to maker GESTURES.
// Workshop law: agents/workshop/session.md (+12 registers; agents/workshop.md routes)




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
    explicit WorkshopWeave(HostContext& host);

    /// READ THE MAKER'S KEYMAP, OR STAND ON THE DEFAULTS.
    void load_keymap();

    /// READ THE MAKER'S PRESENTATION PREFERENCES, OR STAND ON THE DEFAULTS.
    void load_prefs();

    /// Say once what the startup file work DID, on the first surface that can show it --
    /// after the session restore, deliberately, so the sentence that survives on the one
    /// notice line is the one about this launch.
    void speak_startup_notes(loom::Mail& mail);

    /// TAKE THE CONDITIONS THE HOST ALREADY KNEW.
    void take_host_conditions();

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

    /// Open or close the full hotkey view. The whole of the mode change --
    /// `toggle_terminal`'s own shape, one screen element over.
    void toggle_hotkeys();

    /// THE VIEW'S OWN KEYS: Escape closes it, and everything else is swallowed.
    void hotkeys_key(const zengine::input::KeyPressed& k);

    /// Open or close the current-condition view -- `toggle_hotkeys`' own shape,
    /// one surface over.
    void toggle_attention();

    /// THE VIEW'S OWN KEYS: move the cursor, hide the condition it is on, close.
    void attention_key(const zengine::input::KeyPressed& k);

    // ---- What can I do with this? The contextual-action surface ---------------

    /// OPEN ON WHAT IS POINTED AT.
    void open_context_at(const PointedAt& at);

    /// OPEN ON A PAINTED LAYOUT TAB -- the same surface, on the one subject the band owns.
    void open_context_on_layout(const PointedAt& at, std::size_t layout);

    /// OPEN BY KEY, on the subject command mode can truthfully name: the selected object
    /// while one resolves, else the room.
    void open_context_ambient();

    /// Close it whole: subject, group and cursor go together, so a later open cannot
    /// inherit a stale identity. Silent -- the surface disappearing is the statement.
    void close_context();

    /// The surface's own keys: the picker's four, plus the opener closing it.
    void context_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    /// Back out one level, with the cursor landing on the group the maker just left --
    /// what makes backtracking read as returning rather than starting over.
    void leave_context_group();

    /// CHOOSE THE ROW THE CURSOR IS ON.
    void choose_context_row(loom::Mail& mail);

    /// SPEND ONE CHOSEN ACTION against the captured subject.
    void spend_context_choice(Act a, const ContextMenu& spent, loom::Mail& mail);

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
    /// `pane` is meaningful only when `pane_held`; `document_id` only when `document`.
    struct GesturesEnded {
        bool document = false;
        std::int64_t document_id = 0;
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
    void on(const zengine::builder::BuildStatus& said, loom::Mail& mail);

    /// THE BUILDER TOOL SAID WHAT THIS PROJECT CAN BUILD.
    void on(const zengine::builder::RecipeCatalog& said, loom::Mail& mail);

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

    /// The session, for a suite that wants to check where a gesture left things.
    /// Read-only: every change still goes through a message and a gesture.
    const Session& session() const;
    const WorkshopDoc& document() const;

private:
    /// IS THIS THE CHARACTER THAT KEY PRODUCED?
    static bool same_keystroke(const std::string& text, const std::string& owed);

    /// The effective spelling of one action, for this weave's own notices -- the same
    /// `hotkey_text` every screen surface spends, so a hint in the notice line and the
    /// band cannot spell one binding two ways.
    std::string hotkey(Act a) const;

    /// THE PANE EDITOR'S LIVE DRAFT, if any -- asked by its own name where the
    /// caller already knows which inspector it is standing in.
    Row* pane_editor_editing_row();

    /// THE DRAFT UNDER THE KEYS.
    Row* editing_row();


    /// Which of this weave's own editable places a consumed paste request came from
    /// `kNone` for every armless branch.
    // WL-TEXT-09 -- agents/workshop/text-box.md
    enum class PasteOwner : std::uint8_t { kNone, kTerminal, kNaming, kDraft, kEditor };

    /// THE ONE-LINE NAME EDITOR THAT IS OPEN, or nothing -- the layout's or the Pane
    /// Creator's.
    component::TextBox* naming_line();

    /// WHICH DRAFT WOULD THE CHAIN HAVE HANDED THE CLIPBOARD TO? A projection of the one
    /// resolved context rather than a second spelling of the routing, which closes the way
    /// two spellings could deliver a paste to a draft the keys never reached.
    PasteOwner paste_owner_now();

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
    void begin_clipboard_paste(loom::Mail& mail);

    /// The pending-paste record a settled conversation belongs to, removed from the list
    /// and handed back by value. An id the list does not hold answers the empty record
    /// (`owner == kNone`), which every consumer already treats as "nothing to do".
    PendingPaste take_pending_paste(std::uint64_t ask);

    /// Editing mode, KEY half: the three keys that are editor CONTROLS rather
    /// than text.
    void editing_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    /// THE ONE PLACE THE PROPERTY DRAFT'S HORIZONTAL WINDOW IS RECONCILED.
    void refresh_inspector();

    /// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK, AND IF SO SELECT THE WORD.
    bool press_selects_word(std::int64_t modifiers, std::int64_t place,
                            component::TextBox& box, std::size_t at);

    /// The same question for a property row, which keeps its draft behind its own invariant
    /// -- so the mutation goes through `Row`'s door and the reading through `editor()`.
    bool press_selects_word(std::int64_t modifiers, Row& row, std::size_t at);

    /// A PRESS INSIDE THE ACTIVE PROPERTY EDITOR, and nothing else: the raw pointer fact,
    /// the resolved Info body, a prose row and column, a semantic property row, a column of
    /// its value, a byte of the draft, the caret. It is a place, not a mode, and begins nothing.
    bool info_press(const InfoBodyAt& where, std::int64_t modifiers);

    /// A PRESS ON AN ACTION CONTROL PERFORMS THE ACT THE CONTROL NAMES.
    bool actions_press(const InfoBodyAt& where);

    /// A PRESS ON A VISIBLE OBJECT NAME SELECTS THAT OBJECT — in command mode, and only there.
    bool objects_press(const InfoBodyAt& where);

    // ---- The terminal overlay ------------------------------------------------

    /// Open or close the pane. The whole of the mode change.
    void toggle_terminal();

    /// Editing mode for the command line: the keys that are controls rather than
    /// text, exactly as the inspector's editor has.
    void terminal_key(const zengine::input::KeyPressed& k);

    /// A PRESS INSIDE THE TERMINAL MODE — the first place-within-a-mode.
    bool terminal_press(const zengine::input::PointerButton& b);

    /// IS THERE A LIST ON SCREEN WITH SOMETHING IN IT TO CHOOSE?
    bool completion_selectable() const;

    /// Move the selection, and stop at the ends.
    void move_completion(int by);

    /// TAKE THE SELECTED CANDIDATE INTO THE LINE.
    void accept_completion();

    /// AUTHOR ONE LINE THROUGH THE PARTICIPANT'S OWN DOOR.
    void submit_terminal_line();

    /// Take the pane's snapshot of the participant.
    void refresh_terminal();

    /// Command mode.
    void command(const zengine::input::KeyPressed& k, loom::Mail& mail);

    // ---- The dynamic panels --------------------------------------------------

    /// THE PICKER'S POPULATION — the shared recovery inventory, and there is exactly one of
    /// it.
    std::vector<CatalogRow> picker_population() const;

    /// Open the `+ panel` picker.
    void open_picker();

    /// STEP THE PICKER'S CURSOR, BOUNDED BY THE PAINTED POPULATION.
    void picker_move(std::int64_t by);

    /// THE WHEEL OVER THE PICKER MOVES ITS CURSOR.
    void picker_wheel(const zengine::input::PointerWheel& w, loom::Mail& mail);

    /// The picker's keys. Escape and `p` both close it: the key that opened it
    /// closes it, the terminal overlay's rule, and Escape closes it too because
    /// a maker who has changed their mind should not have to remember which of
    /// the two ways out this particular thing has.
    void picker_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    /// OPEN THE KIND THE CURSOR IS ON, OR REMOVE IT. The picker is the one owner
    /// of panel presence, and this is the whole of that ownership.
    void choose_panel(loom::Mail& mail);

    /// OPEN A CLOSED PANE, OR REMOVE AN OPEN ONE -- the picker's own two cases, as the ONE
    /// membership door two consumers spend.
    void toggle_participation(const CatalogRow& chosen, const std::string& again,
                              loom::Mail& mail);

    // ---- The setup: name it, save it, restore it ------------------------------

    /// MAKE THE OPEN PANELS BE WHAT THE ACTIVE SETUP SAYS -- the one owner, and
    /// the only thing in this file that opens or closes a panel.
    void apply_setup(loom::Mail& mail);

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
    void build_now(loom::Mail& mail, bool realize);

    /// MOVE THE MAKER'S CURSOR THROUGH THE RECIPES THE TOOL PUBLISHED.
    void choose_recipe(int by, loom::Mail& mail);

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
    void build_frontier(loom::Mail& mail);

    // ---- THE SOURCE EDITOR: choose source, edit, save, and never lose a byte ----------

    /// The editor's keys: the buffer's own vocabulary first, then the editor's policy.
    void editor_key(const zengine::input::KeyPressed& k);

    /// Text the maker typed into the source.
    void editor_text(const std::string& text);

    /// A press in the editor's body places the caret and begins the selection sweep.
    void editor_press(const zengine::input::PointerButton& b);

    /// THE ONE PLACE THE EDITOR'S VIEWPORT IS RECONCILED -- `refresh_terminal`'s
    /// argument, two dimensions instead of one, on the same once-per-repaint path.
    void refresh_editor();

    // ---- The filesystem browser ----------------------------------------------

    /// The directory the browser is showing. A FIELD READ: after the seed below, nothing
    /// here derives a location from the project anchor, which is what makes browsing
    /// structurally unable to move it.
    std::string files_dir() const;

    /// GENERATE THIS RUN'S ORIGIN AND READ ITS DURABLE MARKS -- once, at the moment
    /// navigation first needs either.
    void ensure_marks();

    /// READ THE MAKER'S OWN PLACES, OR STAND ON NONE.
    void load_marks();

    /// WRITE THE MAKER'S PLACES BACK. Empty path = no persistence, silently, exactly as it
    /// is for every other durable fact this weave holds.
    void save_marks();

    /// TAKE A FRESH LISTING OF WHERE THE MAKER IS STANDING.
    void files_refresh();

    /// A BUILD THIS SESSION WATCHED HAS FINISHED, so what is on disk may have changed --
    /// take a fresh listing if the browser is open, and put the maker back where they
    /// were.
    void files_build_settled();

    /// Put the cursor on a named row if this listing has one -- the ONE place a refresh
    /// does not send it home, because going UP has an answer to the question "which row
    /// did I come from" and landing at the top of a long directory would throw it away.
    void files_point_at(const std::string& name);

    /// MOVE THE CURSOR BY `by` ROWS, bounded at both ends. Bounded at USE, because the
    /// listing under it is replaced wholesale by every refresh.
    void files_move(std::int64_t by);

    /// GO UP ONE LEXICAL DIRECTORY.
    void files_parent();

    /// Where the maker is, for a notice: the absolute location, which is the
    /// only unambiguous answer -- a relative spelling would need a base, and the base a
    /// browser used to have (the project) is exactly the thing it may now be nowhere near.
    std::string files_where() const;

    /// SAY WHERE THE MAKER NOW IS, and why this place is one they might have meant.
    void files_say_where();

    /// ACT ON THE ROW THE CURSOR IS ON -- enter a directory, or hand a file to the one
    /// editor door.
    void files_open(loom::Mail& mail);

    /// MARK, OR UNMARK, THE LOCATION THE BROWSER IS SHOWING.
    void files_mark();

    /// GO TO THE NEXT (or previous) PLACE WORTH RETURNING TO.
    void files_jump_mark(std::int64_t by);

    /// USE THE FILE THE CURSOR IS ON AS THIS SESSION'S RECIPE CATALOG.
    void files_use_recipes(loom::Mail& mail);

    /// THE BROWSER'S KEYS -- nine verbs, every one of them a keymap row, so a maker who
    /// remapped them gets their own bindings here and on every help surface.
    void files_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    /// THE WHEEL OVER THE BROWSER'S BODY MOVES THE CURSOR, and the window follows it.
    void files_wheel(const zengine::input::PointerWheel& w, const Screen& sc,
                     loom::Mail& mail);

    /// A PRESS INSIDE THE LAYOUTS PANE -- the tab run's own inverse, and the whole
    /// of what the top band's two global pointer arms became.
    bool layouts_press(const zengine::input::PointerButton& b, loom::Mail& mail);

    /// A PRESS IN THE BROWSER'S BODY: the first press on a row SELECTS it, and a press on the
    /// row that is already selected ACTIVATES it -- only in a pane that already held the keys,
    /// so no single press can replace what is open. Double-click is not what this is.
    void files_press(const zengine::input::PointerButton& b, bool had_keyboard,
                     loom::Mail& mail);

    // ---- THE PANE EDITOR: a pane as a subject ------------------------------------------------

    /// A FRESH VIEW OF THE SUBJECT, TAKEN AT A GESTURE.
    void repair_pane_editor_subject();

    /// MAKE THIS PANE THE PANE EDITOR'S SUBJECT -- the one writer of `PaneEditor::subject`.
    void choose_subject(const PaneRef& ref);

    /// STEP THE CURSOR OF WHICHEVER LIST THE KEYS ARE IN, bounded, and over a section
    /// heading without stopping on it -- a heading is a boundary, not a row a maker edits.
    void pane_editor_move(std::int64_t by);

    /// STEP ONE OF THE TWO LISTS' CURSORS -- the subject's rows (`rows`) or the PANES list
    /// -- by one, bounded.
    void pane_editor_move_in(bool rows, std::int64_t by);

    /// THE WHEEL OVER THE PANE EDITOR MOVES THE LIST UNDER THE POINTER.
    void pane_editor_wheel(const zengine::input::PointerWheel& w, loom::Mail& mail);

    /// MOVE THE KEYS BETWEEN THE PANES LIST AND THE SUBJECT'S ROWS.
    void pane_editor_switch();

    /// THE ONE RETURN: on the PANES list it chooses the subject; on the rows it opens a
    /// draft, or says why the row under the cursor is not the maker's to author (the Info
    /// panel's `begin_edit` sentence, one inspector over).
    void pane_editor_choose();

    /// OPEN THE SUBJECT IF IT IS CLOSED, REMOVE IT IF IT IS OPEN -- through the picker's own
    /// door, on the row the picker itself would act on.
    void pane_editor_toggle(loom::Mail& mail);

    /// THE PANE EDITOR'S KEYS: a list with a cursor and one gesture on the row it is on.
    void pane_editor_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    // ---- THE PANE CREATOR: a pane made of authored data ---------------------------------------

    /// REBUILD THE PANE MANAGER'S SUBJECT ROWS WITHOUT CHANGING THE SUBJECT.
    void rebuild_subject_rows();

    /// The sentence a dirty definition refuses with, naming the two ways out where the maker
    /// is standing. One spelling, spent by the open door, the naming door and the quit guard.
    std::string maker_pane_dirty_sentence(const char* consequence) const;

    /// THE ONE OPEN DOOR: a pane-definition file becomes the run's open definition, or
    /// nothing moves.
    void open_maker_pane(const std::string& requested, loom::Mail& mail);

    /// MAKE A PANE FROM A NAME -- the Pane Creator's own act.
    bool new_maker_pane(const std::string& name, loom::Mail& mail);

    /// OPEN THE PANE CREATOR'S NAME PROMPT -- `n` inside the Pane Manager. A dirty
    /// definition is refused HERE, before a name is typed, so a maker is not asked for a
    /// word they cannot use. The trigger's own character is swallowed by the binding.
    void open_pane_naming();

    /// The name prompt's keys: the line's own vocabulary first, then make or cancel.
    void pane_naming_key(const zengine::input::KeyPressed& k, loom::Mail& mail);

    /// Close the prompt whole, so a later open cannot inherit a stale draft.
    void close_pane_naming();

    /// WRITE THE OPEN DEFINITION TO ITS FILE -- the one save door, through the family's
    /// safe write.
    void save_maker_pane();

    /// THE ONE DELIBERATE DISCARD DOOR: put the definition back to what its file holds.
    void discard_maker_pane_edits(loom::Mail& mail);

    /// KEEP THE NAME PROMPT'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW -- `refresh_setup_name`
    /// for the Pane Creator's line, against the Pane Manager's own heading columns.
    void refresh_pane_name();

    /// A PRESS INSIDE THE PANE EDITOR'S BODY.
    void pane_editor_press(const zengine::input::PointerButton& b, std::int64_t modifiers);

    /// OPEN THE SOURCE THE BUILDER'S CHOSEN RECIPE NAMES -- Builder's half, and only its
    /// half.
    void edit_source(loom::Mail& mail);

    /// THE ONE DOOR INTO THE EDITOR'S DOCUMENT -- a path in, this session's one open
    /// source out, and every referrer arrives through it.
    void open_source(const std::string& requested, loom::Mail& mail);

    /// MAKE THE EDITOR PANE PRESENT AND SEATABLE, or refuse with the room named -- the
    /// picker's own trial-seat shape, so the edit-source door cannot author a pane the
    /// screen has no room to show and then pour a document into the invisible result.
    bool ensure_editor_pane(loom::Mail& mail);

    /// WRITE THE SOURCE TO ITS FILE -- the editor's save authority.
    void save_source();

    /// THE ONE DELIBERATE DISCARD DOOR: put the buffer back to the last saved state.
    void discard_source_edits();

    // ---- Save and open -------------------------------------------------------

    /// Write the document to its file.
    void save_document();

    /// Replace the document with the one in its file.
    void load_document();

    /// Open onto the first object, or onto none. The rule a fresh Workshop uses
    /// and the rule a load uses, written once so a loaded document and a new one
    /// cannot come to open differently.
    void open_on_first();

    // ---- THE DOCUMENT'S GESTURES: the objects, the inspector, and what the tool says ----

    /// Make one. The notice names the IDENTITY and not the label, because the
    /// default label is the same word the other objects already carry -- which is
    /// the lesson, arriving at the moment a maker can see it is not a problem.
    void create_object();

    /// What deleting THE SELECTED object says, read after the repair: where the selection
    /// went.
    std::string deleted_notice(std::int64_t was) const;

    /// Delete the selected one, and say where the selection went.
    void delete_object();

    /// DELETE AN EXPLICIT OBJECT -- `delete_selected`'s target-taking sibling.
    Written delete_object_at(std::int64_t id);

    /// THE CONTEXTUAL DELETE: the captured object id, spent through the explicit-id door.
    void context_delete_object(std::int64_t id);

    /// One cell, through the same document operation a typed X or Y goes through.
    void move_by(std::int64_t ddx, std::int64_t ddy);

    /// One cell of SIZE, through the same document operation a typed Width or
    /// Height goes through.
    void size_by(std::int64_t dw, std::int64_t dh);

    /// The two notices a direct manipulation produces, in one place so the
    /// pointer and the keyboard cannot describe the same act differently.
    static std::string edge_of(const Handled& done);
    /// A move notice names the AUTHORED position and, when there is one, the
    /// frame that position is authored IN.
    static std::string move_notice(const ui::Element& e, const Handled& done);
    static std::string size_notice(const ui::Element& e, const Handled& done);

    /// IS THE INSPECTOR ON THE SCREEN AT ALL? the rows are shown by a
    /// panel a maker may remove, and `Session::rows` goes on existing when they
    /// do -- correctly, because the rows are a fact about the SELECTION and the
    /// selection is not the panel's. What must not go on happening is a gesture
    /// over them.
    bool inspector_shown() const;

    /// True if the gesture the caller is about to perform has nothing on screen
    /// to perform it on, HAVING SAID SO. A silent no-op would be the worse half
    /// of both available answers: these keys did something before Info could be
    /// removed, so a maker pressing one and seeing nothing has been given no way
    /// to tell a removed panel from a broken tool -- the same argument the empty
    /// OBJECTS list already won.
    bool inspector_absent();

    /// Step the inspector's cursor, when there is an inspector to step it in.
    void move_cursor(std::int64_t delta);

    /// BEGIN AN EDIT -- and refuse to begin one nobody can see.
    void begin_edit();

    /// Resize the workspace: NO authored value changes, and a share visibly
    /// resolves to something else. One keystroke, and the difference between an
    /// authored fact and a resolved one stops being an argument.
    void resize_workspace(std::int64_t delta);

    void select_next();

    void select(std::int64_t id);

    /// The rows are rebuilt, never patched.
    void rebuild_rows();

    /// KEEP THE NAME EDITOR'S WINDOW TRUE AGAINST THE ROOM IT HAS NOW -- the
    /// same call `refresh_inspector` makes for a property draft.
    void refresh_setup_name();

    void say(std::string text, bool bad);

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
    std::string status_line() const;

    // ---- THE EXTERNAL PANE'S ROOM AND GESTURES: the grant, a press, a key, the wheel, text ----

    /// GRANT EACH OPEN EXTERNAL PANE THE ROOM IT CURRENTLY HAS -- once per repaint, and
    /// only when the answer has changed.
    void refresh_external_rooms(loom::Mail& mail);

    /// TELL A PROVIDER A MAKER PRESSED IN ITS ROOM -- the whole of the input seam.
    void external_press(std::int64_t kind, const zengine::input::PointerButton& b,
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
    void external_key(std::int64_t kind, const zengine::input::KeyPressed& k,
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

    /// LEAVE -- and write down what was on the desk on the way out.
    void quit();

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
    std::string finish_draft_first() const;

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
