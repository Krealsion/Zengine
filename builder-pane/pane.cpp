// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Builder pane: a loadable weave that offers Workshop one pane -- the maker's seat at the
// build, at the realization frontier, and at the two acts a reload leaves behind. What it needs
// of the host (what realization is waiting on, whether the plan already names an artifact, the
// file a recipe was authored from) it asks for through the doors
// `workshop/builder_seam_vocabulary.hpp` spells; what crosses is values. Its keys are its own:
// a pane's rows are active only while it holds the keyboard, so a maker presses into it first.
// Builder law: agents/realization.md

#include "builder-pane/vocabulary.hpp"

#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp" // the opening office the open is asked of
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_text.hpp"

#include "workshop/pane_menu.hpp"

#include "activation/activation.hpp"
#include "builder/vocabulary.hpp"
#include "component/control_strip.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ws = zengine::workshop;
namespace pane = zengine::builder_pane;
namespace builder = zengine::builder;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneButton;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneKey;
using ws::PaneManageRequested;
using ws::PaneMenuAnswered;
using ws::PaneMenuRequested;
using ws::v2::PaneOffered;
using ws::PanePassRequested;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PaneWheel;
using ws::PlanNames;
using ws::PlanNamesRequested;
using ws::PlanRowRequested;
using ws::PlanRowWritten;
using ws::ProjectFrontierRequested;
using ws::ProjectFrontierSaid;
using ws::OpenSourceRequested;
using ws::PaneSourceOpened;
using ws::RecipeSourceRequested;
using ws::RecipeSourceSaid;
using ws::SourceOpened;

/// The office Workshop holds, named as a string rather than through `workshop/panel.hpp`: a
/// provider is a stranger to Workshop's internals.
constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- The text helpers the composition spends ------------------------------------------
//
// Workshop's own text helpers, shared through `workshop/pane_text.hpp`, because `screen.hpp`
// is the host's presentation and not a header a loaded image may include.

using zengine::workshop::pane_text::ascii_spelling;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::fitted_label;
using zengine::workshop::pane_text::pad;
using zengine::workshop::pane_text::wrap;
constexpr std::int64_t kWrapIndent = zengine::workshop::pane_text::kWrapIndent;
constexpr const char* kElided = zengine::workshop::pane_text::kElided;


/// A labelled row, in the panel's own nine-column gutter.
std::string panel_field(const char* label, const std::string& value) {
    return pad(label, 9) + value;
}

/// The three-row block the compiler's answer is wrapped into: wrapped to the width in force,
/// cut to the rows that survived the budget, and marked when cut, so the elision mark tells the
/// truth about this face.
std::vector<std::string> panel_block(const char* label, const std::string& value,
                                     std::size_t rows, std::int64_t width) {
    std::vector<std::string> lines = wrap(panel_field(label, value), width);
    if (lines.size() > rows) {
        lines.resize(rows);
        lines.back() = fit(lines.back() + " " + kElided, width);
    }
    while (lines.size() < rows) {
        lines.push_back(std::string());
    }
    return lines;
}

std::string trimmed(const std::string& text) {
    std::size_t b = 0;
    std::size_t e = text.size();
    while (b < e && text[b] == ' ') {
        ++b;
    }
    while (e > b && text[e - 1] == ' ') {
        --e;
    }
    return text.substr(b, e - b);
}

/// A ROLE A MAKER TYPES IS ONE LINE OF PLAIN TEXT. The plan's own law judges it (the host
/// refuses an empty or malformed one in the plan's words); this only keeps control bytes and
/// newlines out of a single-line field, `files.cpp`'s `admissible` for the same reason.
bool admissible(std::string_view text) {
    for (const char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) {
            return false;
        }
    }
    return true;
}

/// The sentence for a press that named a picture this pane has since replaced: a press is aimed
/// at what a maker could see, so rows that moved between the aim and the delivery are said,
/// never spent on whatever slid into that place.
constexpr const char* kMovedSentence = "the rows moved -- press again";

/// HOW MANY ROWS OF ITS OWN THE CONTROL STRIP MAY SPEND -- Files' number, for Files' reason:
/// past three the rest of the controls are the pane menu's, which is what `[menu]` is first
/// for.
constexpr std::int64_t kMaxControlRows = 3;

/// THE FLOOR THE ROLE LINE'S TYPED VALUE NEVER GIVES UP -- Files' `kMinFieldValueColumns`, for
/// Files' reason: the role line's own prompt grows with the stem being loaded (`role for
/// zengine-really-long-example> `), and the same construction that hid a narrow Files field's
/// typed text applies here unless the value keeps a useful minimum (`fitted_label`).
constexpr std::int64_t kMinRoleValueColumns = 12;

// ---- What a published row, or a run of columns inside one, MEANS (component::RowMap) -------

namespace builder_row {
inline constexpr std::int64_t kNone = 0;
inline constexpr std::int64_t kRecipe = 1;  ///< the row naming the chosen recipe, or a list row
inline constexpr std::int64_t kControl = 2; ///< a labelled control; `id` is its operation
inline constexpr std::int64_t kLine = 3;    ///< the role line (the caret's row)
inline constexpr std::int64_t kOutputLine = 4; ///< one line of a build's own words
} // namespace builder_row

struct BuilderMeaning {
    std::int64_t kind = builder_row::kNone;
    std::size_t index = 0;
    std::string id;      ///< a control's operation id; empty for the other kinds
    std::string subject; ///< the recipe a list row names; empty where a row names none
    std::int64_t op = 0; ///< the build operation `subject` was true of; 0 where none applies

    bool operator==(const BuilderMeaning& o) const {
        return kind == o.kind && index == o.index && id == o.id && subject == o.subject &&
               op == o.op;
    }
};

// =============================================================================
// The weave
// =============================================================================

class BuilderPaneWeave
    : public loom::WeaveBase<
          BuilderPaneWeave, pane::BuilderPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneKey, PaneTextInput,
                       PaneActionRequested, builder::BuildStatus, builder::RecipeCatalog,
                       ProjectFrontierSaid, PlanNames, PlanRowWritten, RecipeSourceSaid,
                       SourceOpened, loom::DispatchRefused, surface::ClipboardCopy,
                       surface::ClipboardText, PaneSourceOpened, builder::BuildOutputSaid,
                       PaneWheel, ws::v3::PanePressed, PaneButton, PaneMenuAnswered>,
          loom::Emit<PaneOffered, PaneActions, ws::v3::PaneContent, builder::StatusRequested,
                     builder::BuildRequested, builder::PromoteArtifact, builder::RevertArtifact,
                     ProjectFrontierRequested, PlanNamesRequested, PlanRowRequested,
                     RecipeSourceRequested, OpenSourceRequested, PaneMenuRequested,
                     PanePassRequested, PaneManageRequested, ws::PaneKeyboardRequested,
                     surface::ClipboardCopy, surface::ClipboardTextRequested,
                     builder::BuildOutputRequested>> {
public:
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail, a)) {
            return;
        }
        announce(mail);
    }

    void on(const PaneCatalogRequested&, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        announce(mail);
    }

    /// Workshop grants the pane its room: the one beat on which this tool draws, and when the pane
    /// asks the two questions whose answers it deliberately does not keep -- what the tool is
    /// (`StatusRequested`, answered with both its catalog and its status) and what the project
    /// is waiting on. It asks every time: each grant is a moment at which a picture this pane
    /// did not derive may be stale, and asking is one message.
    void on(const PaneRoom& room, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || room.pane != pane::kBuilderPane) {
            return;
        }
        rows_ = room.rows;
        columns_ = room.columns;
        granted_ = true;
        ask_status(mail);
        ask_frontier(mail, Why::kPaint);
        say(mail);
    }

    /// ONLY THE ROLE LINE READS RAW KEYS. Everything else this pane does arrives as a
    /// resolved id (`on(PaneActionRequested)`); a component's editing gestures are the
    /// component's, not the pane's commands.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kBuilderPane) {
            return;
        }
        if (!role_.open) {
            return;
        }
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!role_.line.consume(key.scancode, key.modifiers, clip_)) {
            return; // a key that means nothing to the line is no act: the notice stands, unsaid
        }
        notice_.clear();
        if (clip_.writes != copied_before) {
            mail.publish(surface::ClipboardCopy{clip_.text});
        }
        if (clip_.paste_requests != pastes_before) {
            begin_paste(mail);
        }
        say(mail);
    }

    void on(const PaneTextInput& typed, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || typed.pane != pane::kBuilderPane) {
            return;
        }
        if (!role_.open || typed.text.empty() || !admissible(typed.text)) {
            return;
        }
        notice_.clear();
        role_.line.type(typed.text);
        say(mail);
    }

    /// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY NAME (WL-KEY-15). Workshop resolved
    /// the keystroke against the effective keymap -- the maker's override where one is
    /// authored, this office's declared default otherwise -- so what arrives is the id.
    void on(const PaneActionRequested& asked, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || asked.pane != pane::kBuilderPane) {
            return;
        }
        // The mode owns the pane's actions first, and an id it does not answer to is no act: the
        // declaration and the keystroke race across two messages (Escape and Return in one poll
        // resolve to a cancel and a commit, and the commit arrives after the line closed), so a
        // stale id, or one nobody declared, spends nothing and the notice stands
        // (`agents/panes.md`).
        if (!answers(asked.id)) {
            return;
        }
        // THE MAKER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT -- and spent means gone from the
        // rows Workshop holds, which are the rows a maker reads. Most acts say their own picture;
        // one whose answer is still on its way (`e`'s lookup, `f`'s frontier, `o`'s plan names)
        // or that meant nothing says none, and the spent notice would stand painted beside the
        // act that spent it. So when a notice stood and the act published nothing, the rows are
        // said here, once, without it.
        const bool spent = !notice_.empty();
        const std::uint64_t published = published_;
        notice_.clear();
        act(asked, mail);
        if (spent && published_ == published) {
            say(mail);
        }
    }

    /// WHAT ONE ACTION DOES, BY ID -- with the notice already spent (`on(PaneActionRequested)`),
    /// and only for an id `answers` admitted in the mode the pane is in.
    ///
    /// (!) A KEY AND A CONTROL REACH ONE OPERATION. The id a keystroke resolved to and the id a
    /// pressed control carries are the same id, spent through the same `perform` -- so a
    /// maker's remapped key and the button beside it cannot come to mean two different things.
    void act(const PaneActionRequested& asked, loom::Mail& mail) { perform(asked.id, mail); }

    /// What an id will act on right now: the one place that says so, read when a control is
    /// painted or a menu row offered, and again when either is spent; empty for an id with no
    /// subject. Not the label and not the cursor: a label is what the maker was promised, this
    /// is what the operation would touch, and `perform_on` compares the two.
    std::string target_of(const std::string& id) const {
        if (id == pane::kActionLoadBuilt) {
            return ready_to_load() ? shown_.artifact : std::string();
        }
        if (id == pane::kActionPromote || id == pane::kActionRevert) {
            return standing() ? shown_.artifact : std::string();
        }
        if (id == pane::kActionFrontier) {
            return waiting_ ? frontier_artifact_ : std::string();
        }
        if (id == pane::kActionOutput) {
            return shown_.op == 0 ? std::string() : "#" + std::to_string(shown_.op);
        }
        if (id == pane::kActionRecipeChoose ||
            (choosing_.open && id == pane::kActionEditSource)) {
            const std::size_t at = list_row();
            return at < known_.recipes.size() ? known_.recipes[at].recipe : std::string();
        }
        if (id == pane::kActionBuild || id == pane::kActionBuildRealize ||
            id == pane::kActionLoadIt || id == pane::kActionEditSource) {
            const std::size_t at = cursor_row();
            return at < known_.recipes.size() ? known_.recipes[at].recipe : std::string();
        }
        return std::string();
    }

    /// The operation the artifact name alone cannot say: two recipes may produce one artifact stem
    /// (WL-PROJ-14), so a build of one can replace the other's under an offer that named only the
    /// stem. `op` is minted once per build and held for its whole lifetime (`builder/runner.hpp`);
    /// zero where the id names no operation, or none is standing.
    std::int64_t target_op_of(const std::string& id) const {
        if (id == pane::kActionLoadBuilt) {
            return ready_to_load() ? shown_.op : 0;
        }
        if (id == pane::kActionPromote || id == pane::kActionRevert) {
            return standing() ? shown_.op : 0;
        }
        return 0;
    }

    /// One operation, asked for by a control or a menu row that named its subject out loud:
    /// refused, never retargeted, when that is no longer what it would touch -- a build can settle
    /// under `[load built a]` without the maker acting. A numbered press is fenced by its picture
    /// first (WL-HAND-03), and the subject in a control's meaning moves that picture with the
    /// promise; a menu carries no picture, so this check is its only fence. The operation is
    /// compared beside the name, never shown: the notice quotes the artifact the maker read.
    void perform_on(const std::string& id, const std::string& advertised,
                    std::int64_t advertised_op, loom::Mail& mail) {
        if (!advertised.empty() && target_of(id) != advertised) {
            notice_ = "`" + advertised + "` is not what is here now -- aim again";
            say(mail);
            return;
        }
        if (advertised_op != 0 && target_op_of(id) != advertised_op) {
            notice_ = "`" + advertised + "` is not what is here now -- aim again";
            say(mail);
            return;
        }
        perform(id, mail);
    }

    void perform(const std::string& id, loom::Mail& mail) {
        if (role_.open) {
            if (id == pane::kActionCommit) {
                commit_role(mail);
            } else if (id == pane::kActionCancel) {
                close_role();
                notice_ = "nothing was loaded and nothing was written";
                declare(mail);
                say(mail);
            } else if (id == pane::kActionMenu) {
                offer_here(mail, mail.correlation());
                say(mail);
            }
            return;
        }
        if (output_.open) {
            if (id == pane::kActionMenu) {
                offer_here(mail, mail.correlation());
                say(mail);
                return;
            }
            read_output(id, mail);
            return;
        }
        if (choosing_.open) {
            choose_in_list(id, mail);
            return;
        }
        if (id == pane::kActionOutput) {
            open_output(mail);
            return;
        }
        if (id == pane::kActionBuild) {
            build_now(mail, state_.arm);
        } else if (id == pane::kActionBuildRealize) {
            build_realize(mail);
        } else if (id == pane::kActionArm) {
            arm_only(mail);
        } else if (id == pane::kActionLoadBuilt) {
            load_built(mail);
        } else if (id == pane::kActionPromote) {
            promote_image(mail);
        } else if (id == pane::kActionRevert) {
            revert_image(mail);
        } else if (id == pane::kActionRecipeNext) {
            choose_recipe(1, mail);
        } else if (id == pane::kActionRecipeBack) {
            choose_recipe(-1, mail);
        } else if (id == pane::kActionFrontier) {
            begin_frontier_build(mail);
        } else if (id == pane::kActionLoadIt) {
            begin_load_it(mail);
        } else if (id == pane::kActionEditSource) {
            edit_source(mail);
        } else if (id == pane::kActionRecipes) {
            open_recipes(mail);
        } else if (id == pane::kActionMenu) {
            offer_here(mail, mail.correlation());
            say(mail);
        }
    }

    // ---- What the tool says ---------------------------------------------------------

    /// The tool's own picture, published `to_any`, held while this pane shows it: a member, not
    /// state, so a reload starts with nothing and asks (WL-PROJ-12).
    void on(const builder::BuildStatus& said, loom::Mail& mail) {
        // Was this pane watching? Learning a fact and witnessing an event are different, and only
        // the second is news (WL-PROJ-11): an answer about a build that finished earlier is not
        // announced as though it just happened.
        const bool watching = awaiting_;
        heard_ = true;
        shown_ = said;
        if (!builder::still_going(said.outcome)) {
            awaiting_ = false;
        }
        // THE FRONTIER MOVES WHEN REALIZATION DOES, and realization moves behind a settled
        // build. Asking here is what keeps the `project` row honest without a publication
        // nobody asked the owner for.
        if (!builder::still_going(said.outcome)) {
            ask_frontier(mail, Why::kPaint);
        }
        if (watching && !builder::still_going(said.outcome)) {
            notice_ = build_words(said);
        }
        if (watching && awaiting_realization_ &&
            said.realization != builder::realization::kNotAsked) {
            awaiting_realization_ = false;
            notice_ = realize_words(said);
        }
        // A READER BOUND TO THIS OPERATION HEARS THAT IT SAID MORE, OR ENDED: the page it shows
        // is asked for again, where it is. A reader bound to another operation is not moved.
        if (output_.open && said.op == output_.op) {
            ask_page(mail);
        }
        say(mail);
    }

    /// THE TOOL'S PAGE OF THE OPERATION THIS READER IS BOUND TO -- the answer to the one page
    /// ask outstanding, and nothing else: a stale answer, or one for a reader since closed or
    /// rebound, settles nothing (WL-OUT-04).
    void on(const builder::BuildOutputSaid& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !output_.ask.awaiting ||
            mail.correlation() != output_.ask.pending) {
            return;
        }
        output_.ask = Ask{};
        if (!output_.open || said.op != output_.op) {
            return;
        }
        output_.page = said;
        output_.heard = true;
        say(mail);
    }

    /// THE WHEEL SCROLLS THE READER, three lines a notch, the Editor's measure. Outside the
    /// reader it means nothing here.
    void on(const PaneWheel& wheel, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || wheel.pane != pane::kBuilderPane) {
            return;
        }
        if (!output_.open && !choosing_.open) {
            return; // the fact panel is not a list: there is nothing for a notch to walk
        }
        wheel_ += wheel.dy * 3.0;
        const std::int64_t notches = static_cast<std::int64_t>(wheel_);
        wheel_ -= static_cast<double>(notches);
        if (notches == 0) {
            return;
        }
        if (output_.open) {
            scroll(-notches, mail);
            return;
        }
        for (std::int64_t i = 0; i < (notches < 0 ? -notches : notches); ++i) {
            choose_in_list(notches > 0 ? pane::kActionRecipesUp : pane::kActionRecipesDown, mail);
        }
    }

    /// WHAT THIS PROJECT CAN BUILD, and -- since v2 -- which authored file said so.
    ///
    /// THE CHOICE FOLLOWS ITS RECIPE AND NEVER ITS ROW (WL-PROJ-07). The maker's pick is
    /// held BY NAME (`BuilderPaneState::chosen`), so a reordered catalog moves it with no
    /// work at all and a catalog that no longer holds it releases it -- there is no index to
    /// carry, and therefore no index to carry wrongly.
    void on(const builder::RecipeCatalog& said, loom::Mail& mail) {
        known_ = said;
        if (!state_.chosen.empty() && named_row(state_.chosen) == known_.recipes.size()) {
            // A SELECTION THAT NO LONGER NAMES ANYTHING IS NOT THE MAKER'S ANY MORE: the
            // recipe their pick named is gone. The frontier action must not read what is
            // left as an explicit choice.
            state_.chosen.clear();
        }
        if (!picked_.empty() && named_row(picked_) == known_.recipes.size()) {
            // ...AND A PICK GOES WITH ITS RECIPE, so the name coming back in a later catalog is
            // not a pick the maker made of it.
            picked_.clear();
        }
        say(mail);
    }

    // ---- What the host answers ------------------------------------------------------

    /// The answer this pane asked for must not erase what it just said: every gesture writes a
    /// notice, says its rows and asks the frontier again, and the answer arrives on the same
    /// drain, so an unconditional re-say would publish a notice-less picture over the first. A
    /// paint answer is news only when the frontier moved; a build answer is the gesture's own
    /// decision, made whether or not the picture moved.
    void on(const ProjectFrontierSaid& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !frontier_.awaiting ||
            mail.correlation() != frontier_.pending) {
            return;
        }
        frontier_.awaiting = false;
        const bool moved = !frontier_known_ || waiting_ != said.waiting ||
                           frontier_artifact_ != said.artifact ||
                           frontier_blocked_ != said.blocked;
        frontier_known_ = true;
        waiting_ = said.waiting;
        frontier_artifact_ = said.artifact;
        frontier_blocked_ = said.blocked;
        if (frontier_.why == Why::kBuild) {
            finish_frontier_build(mail);
            return;
        }
        if (moved) {
            say(mail);
        }
    }

    void on(const PlanNames& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !names_.awaiting || mail.correlation() != names_.pending) {
            return;
        }
        names_.awaiting = false;
        if (said.named) {
            notice_ = "`" + said.stem + "` is already in this project's plan -- " +
                      "build-and-load it instead";
            say(mail);
            return;
        }
        open_role(said.stem, names_.recipe);
        declare(mail);
        say(mail);
        // THE LINE IS OPEN: if a menu row began this, the keys it left behind are asked for
        // now, continuing THAT choice and no other (`chose`, `take_keys`).
        if (names_.choice != 0) {
            take_keys(mail, names_.choice);
            names_.choice = 0;
        }
    }

    void on(const PlanRowWritten& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !row_.awaiting || mail.correlation() != row_.pending) {
            return;
        }
        row_.awaiting = false;
        const std::string written = said.path.empty() ? std::string() : "; written to " + said.path;
        if (!said.accepted) {
            notice_ = "not loaded: " + said.refusal;
        } else if (!said.product.empty()) {
            // THE INTENT FINISHES: the row is the frontier and its product is already built,
            // so this is the button's own act -- the finished build's recipe asked for again
            // with the second intention aboard, to the same office, under the same grant.
            // No new route: the tool confirms the build, offers, and the owner decides.
            send_build(mail, row_.recipe, /*realize=*/true);
            notice_ = "loading `" + row_.stem + "` now -- loaded as " + row_.role + ", " +
                      said.detail + "; Workshop stays live while the incremental build "
                      "confirms it" + written;
        } else {
            notice_ = "loaded `" + row_.stem + "` as " + row_.role + " -- " + said.detail +
                      (said.frontier ? "; nothing is built yet -- the frontier action builds "
                                       "and loads it"
                                     : std::string()) +
                      written;
        }
        // THE PLAN MOVED, SO WHAT THE PROJECT IS WAITING ON MAY HAVE MOVED WITH IT.
        ask_frontier(mail, Why::kPaint);
        say(mail);
    }

    /// The host's answer to "which file does this recipe name": the first of the two doors `e`
    /// walks. A refusal is the owner's own words; an accepted answer carries one absolute path,
    /// spent at once at the opening office and held no longer. It is read against the recipe it
    /// asked about, which the notice names, even if the maker's choice has moved since.
    void on(const RecipeSourceSaid& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !source_.awaiting || mail.correlation() != source_.pending) {
            return;
        }
        const std::string recipe = source_.subject;
        source_ = Ask{};
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
            return;
        }
        // THE SECOND DOOR: the opening office, which arranges the document and the desk
        // together (WL-OPEN-01). The ticket is kept for the bus's later word that exactly this
        // attempt was refused (WL-OPEN-07); nothing queued is refused now, in words.
        open_.pending = ++asked_;
        open_.awaiting = true;
        open_.subject = recipe;
        open_.attempt = mail.as_role(pane::kBuilderPaneRole)
                            .send_to_role(ws::kOpeningRole, OpenSourceRequested{said.source},
                                          open_.pending);
        if (!open_.attempt.valid()) {
            open_ = Ask{};
            notice_ = "`" + recipe + "`: the source was not opened -- nothing was queued to the "
                      "opening office";
            say(mail);
        }
    }

    /// The bus's word that one of this pane's attempts was refused before any handler ran
    /// (WL-OPEN-07). Provenance first, then the exact attempt against the two asks `e` walks --
    /// the recipe-source lookup at the project office and the open at the opening office -- and
    /// only the matched one is cleared and named. Delivered silence is not a refusal.
    void on(const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        if (!attempt.valid()) {
            return;
        }
        if (source_.awaiting && source_.attempt.valid() && attempt.seq == source_.attempt.seq) {
            const std::string recipe = source_.subject;
            source_ = Ask{};
            notice_ = "`" + recipe + "`: the source could not be looked up -- it could not reach " +
                      ws::kProjectRole + " (" + refused.reason + ")";
            say(mail);
            return;
        }
        if (open_.awaiting && open_.attempt.valid() && attempt.seq == open_.attempt.seq) {
            const std::string recipe = open_.subject;
            open_ = Ask{};
            notice_ = "`" + recipe + "`: the source was not opened -- the open could not reach " +
                      ws::kOpeningRole + " (" + refused.reason + ")";
            say(mail);
        }
    }

    /// The opening office's answer -- the second door. An accepted open says nothing here: the
    /// document and its pane are shown together, and that is what a maker reads. A refusal (a
    /// missing file, bytes the editor cannot carry, a dirty buffer) belongs beside its row.
    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !open_.awaiting || mail.correlation() != open_.pending) {
            return;
        }
        open_ = Ask{};
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// A maker reached the source behind a pane and it is open: Workshop's reading, published once
    /// the open its Edit Code asked for took (WL-CODE-03). The choice moves visibly to that
    /// source's recipe; nothing is built, armed or realized. The office is read before a word
    /// is. No pick follows the choice (`picked` is what `c` named, WL-PROJ-14), and no reload is
    /// promised: that is the realization owner's to say, so the sentence says only whether the
    /// next build will be offered for loading.
    // WL-CODE-04 -- agents/workshop/code.md
    void on(const PaneSourceOpened& said, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole)) {
            return;
        }
        // A CATALOG THIS PANE HAS HEARD AND THAT DOES NOT HOLD THE RECIPE IS A DISAGREEMENT TO SAY,
        // not a choice to make: the pane's copy is behind the host's, and its next arrival is the
        // truth. One it has not heard yet takes the name, which that arrival keeps or releases.
        if (heard_ && named_row(said.recipe) == known_.recipes.size()) {
            if (!role_.open) {
                notice_ = "the source of " + said.name + " is open, but recipe `" + said.recipe +
                          "` is not in the recipes this pane last heard -- nothing was chosen";
                say(mail);
            }
            return;
        }
        state_.chosen = said.recipe;
        // THE ROLE LINE KEEPS ITS OWN ROW: a maker mid-way through typing a role still sees the
        // line; the choice has moved underneath it and the recipe row says so when it closes.
        if (!role_.open) {
            notice_ = "build recipe: " + said.recipe + " -> " + said.artifact + " -- the source of " +
                      said.name + " is open in the Editor; save it, then build (load after build: " +
                      (state_.arm ? "on" : "off") + ")";
        }
        say(mail);
    }

    // ---- The mouse: a press names a picture, a right press offers the pane's own rows ----

    /// A primary press in this pane, naming the picture the medium held when it was read. A press
    /// about an older picture is refused in words, never resolved against whatever row has since
    /// moved into its place (WL-DESK-14, one pane over).
    void on(const ws::v3::PanePressed& press, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || press.pane != pane::kBuilderPane) {
            return;
        }
        if (!map_.current(press.picture)) {
            notice_ = kMovedSentence;
            say(mail);
            return;
        }
        const BuilderMeaning* m = map_.at(press.row, press.column);
        if (m == nullptr || m->kind == builder_row::kNone) {
            return; // a fact row, a heading, a build's own words: pointing, and nothing more
        }
        const bool spent = !notice_.empty();
        const std::uint64_t published = published_;
        notice_.clear();
        if (m->kind == builder_row::kControl) {
            perform_on(m->id, m->subject, m->op, mail);
        } else if (m->kind == builder_row::kRecipe && choosing_.open) {
            // THE LIST'S OWN SECOND PRESS: the first names the row, the second makes it the
            // maker's pick and closes the list -- Files' rule, so no press means two things.
            // And only where the keys already were (WL-FOCUS-04): the press that brings them
            // here points at the pane, and pointing is not choosing.
            if (press.keys_went_here && m->subject == choosing_.name) {
                take_choice(mail);
            } else {
                choosing_.name = m->subject;
                say(mail);
            }
        } else if (m->kind == builder_row::kRecipe && press.keys_went_here) {
            // THE ROW THAT NAMES THE CHOICE IS A WAY INTO THE LIST -- for a maker whose keys
            // are already here. A press that merely brings them points at the pane and opens
            // no mode, which is what keeps clicking a pane to focus it from doing anything.
            open_recipes(mail);
        } else if (m->kind == builder_row::kLine && role_.open) {
            const std::int64_t prompt = static_cast<std::int64_t>(active_role_prompt().size());
            role_.line.place(role_.line.position_at_column(press.column - prompt));
            say(mail);
        }
        if (spent && published_ == published) {
            say(mail);
        }
    }

    /// THE SECOND BUTTON. A right press on a row this pane owns OFFERS that row's menu, beside
    /// the press, continuing it; a right press on anything else is handed back to the host,
    /// whose own pane menu answers (WL-CTX-08). A middle press and every release mean nothing
    /// here and are consumed.
    void on(const PaneButton& b, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || b.pane != pane::kBuilderPane) {
            return;
        }
        if (!b.pressed || b.button != 3) {
            return;
        }
        if (!map_.current(b.picture)) {
            notice_ = kMovedSentence;
            say(mail);
            return;
        }
        const BuilderMeaning* m = map_.at(b.row, b.column);
        if (m == nullptr || m->kind == builder_row::kNone) {
            (void)pane_menu::pass_back(mail, pane::kBuilderPaneRole, pane::kBuilderPane);
            return;
        }
        notice_.clear();
        if (m->kind == builder_row::kRecipe && choosing_.open) {
            choosing_.name = m->subject;
        }
        offer_menu(b.row, b.column, mail.correlation(), mail);
        say(mail);
    }

    /// WHAT A MENU CAME TO -- if it answers one of THIS image's own asks. `Asked::take` is the
    /// whole of what makes an answer safe to act on: a choice counts only from the presenter's
    /// office, under the number of an ask this image sent and has not heard answered, about the
    /// pane and subject it asked about, once. A reloaded pane asked nothing, so its
    /// predecessor's menus act on nothing. What the row MEANS is judged here, against what this
    /// pane holds now -- the catalog may have been republished while the menu was open.
    void on(const PaneMenuAnswered& a, loom::Mail& mail) {
        const std::string id = asked_menu_.take(mail, a);
        if (id.empty()) {
            return;
        }
        chose(id, a.subject, mail);
    }

    // ---- The recipe list (the pane's own mode) -------------------------------------------

    /// OPEN THE CATALOG AS ROWS, standing on the recipe the pane is already showing. Looking is
    /// not choosing: nothing this list does moves `chosen` until `builder.recipe-choose`.
    void open_recipes(loom::Mail& mail) {
        if (!has_recipe("there is nothing to choose between")) {
            say(mail);
            return;
        }
        choosing_ = Choosing{};
        choosing_.open = true;
        choosing_.name = known_.recipes[cursor_row()].recipe;
        declare(mail);
        notice_ = "choose a recipe -- Return takes it, Escape leaves the choice as it was";
        say(mail);
    }

    void close_recipes(loom::Mail& mail, std::string said) {
        choosing_ = Choosing{};
        declare(mail);
        notice_ = std::move(said);
        say(mail);
    }

    /// WHERE THE LIST'S CURSOR STANDS, by name and re-derived every time: a catalog that moved
    /// while the list was open moves the cursor with the recipe it named, and one that dropped
    /// the recipe leaves the cursor on the row the pane is showing.
    std::size_t list_row() const {
        const std::size_t at = named_row(choosing_.name);
        return at < known_.recipes.size() ? at : cursor_row();
    }

    void choose_in_list(const std::string& id, loom::Mail& mail) {
        if (id == pane::kActionRecipesUp || id == pane::kActionRecipesDown) {
            if (known_.recipes.empty()) {
                say(mail);
                return;
            }
            const std::int64_t held = static_cast<std::int64_t>(known_.recipes.size());
            std::int64_t to = static_cast<std::int64_t>(list_row()) +
                              (id == pane::kActionRecipesUp ? -1 : 1);
            to = to < 0 ? 0 : (to >= held ? held - 1 : to);
            choosing_.name = known_.recipes[static_cast<std::size_t>(to)].recipe;
            say(mail);
        } else if (id == pane::kActionRecipeChoose) {
            take_choice(mail);
        } else if (id == pane::kActionRecipesClose) {
            close_recipes(mail, "the list is closed -- the choice is unchanged");
        } else if (id == pane::kActionEditSource) {
            // The list's own row, acting on the list's cursor: `edit_source` reads `list_row()`
            // while the list is open, so the recipe opened is the one the row named, and the
            // committed choice is not touched (WL-PROJ-14: looking is not choosing).
            edit_source(mail);
        } else if (id == pane::kActionMenu) {
            offer_here(mail, mail.correlation());
            say(mail);
        }
    }

    /// ASK THE HOST FOR THE KEYBOARD, CONTINUING A MENU CHOICE BY ITS NUMBER. Spent only where
    /// a chosen row actually opened a line to type into: a right press by itself stays
    /// focus-neutral (WL-CTX-08), and the host refuses a grab that is no longer the maker's
    /// latest act.
    void take_keys(loom::Mail& mail, std::uint64_t correlation) {
        if (correlation == 0) {
            return;
        }
        (void)pane_menu::take_keyboard_continuing(mail, pane::kBuilderPaneRole,
                                                  pane::kBuilderPane, correlation);
    }

    /// TAKE THE ROW THE LIST IS STANDING ON AS THE MAKER'S PICK. The same two writes `c` makes
    /// -- the choice and the record that it was PICKED (WL-PROJ-14) -- so the frontier action
    /// reads a choice made here exactly as it reads one made by the key.
    void take_choice(loom::Mail& mail) {
        const std::size_t at = list_row();
        if (at >= known_.recipes.size()) {
            close_recipes(mail, "that recipe is not in the catalog any more -- nothing was chosen");
            return;
        }
        state_.chosen = known_.recipes[at].recipe;
        picked_ = state_.chosen;
        close_recipes(mail, "build recipe: " + known_.recipes[at].recipe + " -> " +
                                known_.recipes[at].artifact);
    }

    // ---- The two halves of load-after-build, each refusing on its own terms ---------------

    /// IS AN ARTIFACT BUILT, UNOFFERED, AND WAITING? The exact condition `build_realize` reads
    /// for its button face, named once so the face, the control and the spend agree.
    bool ready_to_load() const {
        return heard_ && !awaiting_ && !state_.arm && !shown_.recipe.empty() &&
               shown_.outcome == builder::outcome::kSucceeded &&
               shown_.realization == builder::realization::kNotAsked;
    }

    /// LOAD WHAT THE LAST BUILD PRODUCED -- and that is the BUILT recipe, never the chosen one.
    /// A maker who built `rocket` and then picked `probe` out of the list is still owed
    /// `rocket` by this control, because `rocket` is what is standing there built.
    void load_built(loom::Mail& mail) {
        if (!ready_to_load()) {
            notice_ = state_.arm ? "load after build is already on -- the next build is offered"
                                 : "nothing built is waiting to be loaded";
            say(mail);
            return;
        }
        const builder::BuildStatus& s = shown_;
        send_build(mail, s.recipe, /*realize=*/true);
        notice_ = "loading the built `" + s.artifact +
                  "` now -- Workshop stays live while the incremental build confirms it";
        say(mail);
    }

    /// FLIP THE STANDING INTENT AND SEND NOTHING. Refused while an artifact is standing built
    /// and unoffered, because there the maker's own control says `load built ...` and arming
    /// the NEXT build is a different answer to the question they asked.
    void arm_only(loom::Mail& mail) {
        if (ready_to_load()) {
            notice_ = "`" + shown_.artifact +
                      "` is built and waiting -- load it, or build again to arm the next one";
            say(mail);
            return;
        }
        state_.arm = !state_.arm;
        notice_ = state_.arm ? "load after build: on -- the next build is offered to the "
                               "running project when it works"
                             : "load after build: off -- the next build is a plain build";
        say(mail);
    }

    // ---- The pane's own menu (WL-CTX-09) --------------------------------------------------

    /// THE MENU KEY'S ENTRANCE: the mode's own rows, beside the row they are about.
    void offer_here(loom::Mail& mail, std::uint64_t correlation) {
        std::int64_t row = 0;
        if (choosing_.open) {
            const std::int64_t at =
                map_.row_of(BuilderMeaning{builder_row::kRecipe, 0, {}, choosing_.name});
            row = at < 0 ? 0 : at;
        }
        offer_menu(row, 0, correlation, mail);
    }

    /// The rows this mode offers: operations this pane already has, spelled with the subject they
    /// will act on, and a row not about the maker's choice says so by name. Every control of the
    /// mode has a row here, including an unavailable one: a narrow strip's `+N in menu` is a
    /// promise only this function keeps, and a maker who cannot reach an operation is owed its
    /// refusal rather than silence.
    void offer_menu(std::int64_t row, std::int64_t column, std::uint64_t correlation,
                    loom::Mail& mail) {
        pane_menu::Offer offer(pane::kBuilderPane, menu_subject());
        offer.at(row, column);
        offered_.clear();
        // WHAT EACH ROW ADVERTISES IT WILL ACT ON, kept in the image beside the ask. An answer
        // arrives after any number of the maker's other acts and after anything that moved
        // this pane's facts, and the row's own promise is the thing to judge it against
        // (`chose`); `menu_subject` establishes only the MODE the menu was opened in.
        const auto say_row = [&](const char* id, std::string label, std::string advertised = {},
                                 std::int64_t advertised_op = 0) {
            offer.row(id, std::move(label));
            offered_.push_back(Offered{id, std::move(advertised), advertised_op});
        };
        if (role_.open) {
            say_row(pane::kMenuCommit, "load `" + role_.stem + "` with the role typed");
            say_row(pane::kMenuCancel, "write nothing and load nothing");
        } else if (output_.open) {
            say_row(pane::kMenuOutputUp, "a line up");
            say_row(pane::kMenuOutputDown, "a line down");
            say_row(pane::kMenuOutputFirst, "the first line");
            say_row(pane::kMenuOutputLast, "the last lines");
            say_row(pane::kMenuOutputLeft, "pan left");
            say_row(pane::kMenuOutputRight, "pan right");
            say_row(pane::kMenuOutputOlder, "the older build's output");
            say_row(pane::kMenuOutputNewer, "the newer build's output");
            say_row(pane::kMenuClose, "close this build's output");
        } else if (choosing_.open) {
            const std::size_t at = list_row();
            if (at < known_.recipes.size()) {
                const std::string& named = known_.recipes[at].recipe;
                say_row(pane::kMenuChoose, "choose `" + named + "`", named);
                say_row(pane::kMenuEditSource, "edit `" + named + "`'s source", named);
            }
            say_row(pane::kMenuClose, "leave the choice as it was");
        } else {
            const std::size_t at = cursor_row();
            const std::string chosen =
                at < known_.recipes.size() ? known_.recipes[at].recipe : std::string();
            say_row(pane::kMenuRecipes, "choose a recipe from the list...");
            say_row(pane::kMenuBuild,
                    chosen.empty() ? std::string("build the chosen recipe")
                                   : "build `" + chosen + "`",
                    chosen);
            say_row(pane::kMenuArm, state_.arm ? "turn load-after-build off"
                                               : "turn load-after-build on");
            say_row(pane::kMenuLoadBuilt,
                    ready_to_load() ? "load the built `" + shown_.artifact + "` now"
                                    : std::string("load what was built"),
                    ready_to_load() ? shown_.artifact : std::string(),
                    ready_to_load() ? shown_.op : 0);
            say_row(pane::kMenuAddToPlan,
                    chosen.empty() ? std::string("add the chosen artifact to the load plan...")
                                   : "add `" + chosen + "`'s artifact to the load plan...",
                    chosen);
            say_row(pane::kMenuFrontier,
                    waiting_ ? "build and load `" + frontier_artifact_ + "`"
                             : std::string("build what the project is waiting on"),
                    waiting_ ? frontier_artifact_ : std::string());
            say_row(pane::kMenuPromote,
                    standing() ? "promote `" + shown_.artifact + "` -- a restart loads it"
                               : std::string("promote the loaded image"),
                    standing() ? shown_.artifact : std::string(),
                    standing() ? shown_.op : 0);
            say_row(pane::kMenuRevert,
                    standing() ? "revert `" + shown_.artifact + "` -- the previous image runs"
                               : std::string("revert the loaded image"),
                    standing() ? shown_.artifact : std::string(),
                    standing() ? shown_.op : 0);
            say_row(pane::kMenuEditSource,
                    chosen.empty() ? std::string("edit the chosen recipe's source")
                                   : "edit `" + chosen + "`'s source",
                    chosen);
            say_row(pane::kMenuOutput,
                    shown_.op == 0 ? std::string("read what a build said")
                                   : "read what build #" + std::to_string(shown_.op) + " said",
                    shown_.op == 0 ? std::string() : "#" + std::to_string(shown_.op));
        }
        say_row(pane::kMenuManage, "manage this pane...");
        asked_menu_ = offer.continuing(mail, pane::kBuilderPaneRole, correlation);
    }

    /// WHAT ONE OFFERED ROW PROMISED: the row's id, the subject its label named out loud (empty
    /// where the label named none), and the operation that subject was true of when the row was
    /// written (0 where none applies) -- the second half of the same promise, checked but never
    /// shown (`perform_on`).
    struct Offered {
        std::string id;
        std::string advertised;
        std::int64_t op = 0;
    };

    /// WHAT THIS ROW ADVERTISED WHEN IT WAS OFFERED, or an empty `Offered` -- read once, when
    /// the answer lands. A row nobody offered advertises nothing and is judged by the mode alone.
    Offered offered_as(const std::string& id) const {
        for (const Offered& row : offered_) {
            if (row.id == id) {
                return row;
            }
        }
        return Offered{};
    }

    /// WHAT A MENU IS ABOUT, carried across the seam and established again when it answers: the
    /// mode it was opened in, and the row it was opened on where the mode has rows.
    std::string menu_subject() const {
        if (role_.open) {
            return "role:" + role_.stem;
        }
        if (output_.open) {
            return "output:" + std::to_string(output_.op);
        }
        if (choosing_.open) {
            const std::size_t at = list_row();
            return "list:" + (at < known_.recipes.size() ? known_.recipes[at].recipe
                                                         : std::string());
        }
        return "builder";
    }

    /// WHAT A CHOSEN ROW MEANS -- judged here, against what this pane holds NOW. A menu that
    /// was opened in another mode, or about a recipe the catalog has since dropped, acts on
    /// nothing: the mode may have changed while the menu stood open, and every row below is
    /// about the mode it was written for.
    void chose(const std::string& id, const std::string& subject, loom::Mail& mail) {
        if (subject != menu_subject()) {
            notice_ = "that menu was about something else -- nothing was done";
            say(mail);
            return;
        }
        if (id == pane::kMenuManage) {
            (void)pane_menu::manage(mail, pane::kBuilderPaneRole, pane::kBuilderPane,
                                    pane::kBuilderPaneRole, pane::kBuilderPane);
            return;
        }
        static const struct {
            const char* menu;
            const char* action;
        } kRows[] = {{pane::kMenuRecipes, pane::kActionRecipes},
                     {pane::kMenuBuild, pane::kActionBuild},
                     {pane::kMenuArm, pane::kActionArm},
                     {pane::kMenuLoadBuilt, pane::kActionLoadBuilt},
                     {pane::kMenuAddToPlan, pane::kActionLoadIt},
                     {pane::kMenuFrontier, pane::kActionFrontier},
                     {pane::kMenuPromote, pane::kActionPromote},
                     {pane::kMenuRevert, pane::kActionRevert},
                     {pane::kMenuEditSource, pane::kActionEditSource},
                     {pane::kMenuOutput, pane::kActionOutput},
                     {pane::kMenuChoose, pane::kActionRecipeChoose},
                     {pane::kMenuCommit, pane::kActionCommit},
                     {pane::kMenuCancel, pane::kActionCancel},
                     {pane::kMenuOutputUp, pane::kActionOutputUp},
                     {pane::kMenuOutputDown, pane::kActionOutputDown},
                     {pane::kMenuOutputFirst, pane::kActionOutputFirst},
                     {pane::kMenuOutputLast, pane::kActionOutputLast},
                     {pane::kMenuOutputLeft, pane::kActionOutputLeft},
                     {pane::kMenuOutputRight, pane::kActionOutputRight},
                     {pane::kMenuOutputOlder, pane::kActionOutputOlder},
                     {pane::kMenuOutputNewer, pane::kActionOutputNewer}};
        for (const auto& row : kRows) {
            if (id == row.menu) {
                // The row's own promise, established again: a menu stands open across the
                // maker's other acts and every build that settles under it, so `load the built
                // `a` now` loads `a` or refuses. A choice that begins an edit carries its number
                // to where the edit opens: the role line opens only after the plan office
                // answers, so the number rides in `names_.choice` and the grab is spent there
                // (`on(PlanNames)`), still judged by the host as a continuation of this choice.
                const Offered offered = offered_as(id);
                choice_ = mail.correlation();
                perform_on(row.action, offered.advertised, offered.op, mail);
                choice_ = 0;
                return;
            }
        }
        if (id == pane::kMenuClose) {
            // ONE ROW, TWO MODES, AND THE MODE DECIDES WHICH OPERATION IT IS: the subject
            // above already established that this menu belongs to the mode in force.
            perform(output_.open ? pane::kActionOutputClose : pane::kActionRecipesClose, mail);
        }
    }

    // ---- The clipboard the role line spends -----------------------------------------

    void on(const surface::ClipboardCopy& said, loom::Mail&) {
        if (admissible(said.text)) {
            clip_.text = said.text;
        }
    }

    void on(const surface::ClipboardText& a, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const std::optional<loom::PendingAsk> settled =
            clip_asks_.settle(mail.correlation(), mail.sender());
        if (!settled || !pasting_ || settled->id != paste_ask_) {
            return;
        }
        pasting_ = false;
        if (!role_.open || role_.line.draft_epoch() != paste_epoch_) {
            return;
        }
        if (a.readable) {
            if (!admissible(a.text)) {
                return;
            }
            clip_.text = a.text;
        }
        role_.line.paste(clip_);
        say(mail);
    }

private:
    // ---- Announcing -----------------------------------------------------------------

    void announce(loom::Mail& mail) {
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(kWorkshopRole, PaneOffered{pane::kBuilderPane, pane::kBuilderPaneName,
                                                     pane::kBuilderPaneSummary, 14, 70});
        declare(mail);
    }

    /// What this pane answers to right now, re-declared whenever the mode changes. A pane is one
    /// keyboard context and a mode is not a second one (WL-FILES-16): while a maker types a role,
    /// the pane declares the line's own rows and every other key reaches the line as a
    /// `PaneKey`, so Backspace deletes a character. `PaneActions` is a replacement (WL-KEY-15),
    /// and an id never moves: a maker's override applies wherever the id is in force.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kBuilderPane;
        actions.rows = action_rows();
        (void)mail.as_role(pane::kBuilderPaneRole).send_to_role(kWorkshopRole, actions);
    }

    /// THE ROWS OF THE MODE THIS PANE IS IN -- what `declare` tells Workshop, and what `answers`
    /// reads, so what the pane acts on and what it said it acts on are one list.
    std::vector<PaneActionRow> action_rows() const {
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::int64_t mods = input::mod::kNone) {
            rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        if (role_.open) {
            row(pane::kActionCommit, "load it", input::scan::kReturn);
            // And the menu declares no default key while the role line is open: Workshop resolves
            // a key transition before the character it produced arrives, so `Shift+M` would open
            // the menu and lose the `M` of `Main`. The `[menu]` control, first in every strip, and
            // the second button stay the route; a maker who wants a key names `builder.menu`.
            row(pane::kActionMenu, "this pane's menu", input::scan::kUnknown);
            row(pane::kActionCancel, "cancel", input::scan::kEscape);
            return rows;
        }
        if (output_.open) {
            row(pane::kActionOutputUp, "line up", input::scan::kUp);
            row(pane::kActionOutputDown, "line down", input::scan::kDown);
            row(pane::kActionOutputFirst, "first line", input::scan::kHome);
            row(pane::kActionOutputLast, "last lines", input::scan::kEnd);
            row(pane::kActionOutputLeft, "pan left", input::scan::kLeft);
            row(pane::kActionOutputRight, "pan right", input::scan::kRight);
            row(pane::kActionOutputOlder, "older build", input::scan::kLeftBracket);
            row(pane::kActionOutputNewer, "newer build", input::scan::kRightBracket);
            row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
            row(pane::kActionOutputClose, "close output", input::scan::kEscape);
            return rows;
        }
        // ---- The recipe list: the two arrows, the take, the menu and the way out ---------
        if (choosing_.open) {
            row(pane::kActionRecipesUp, "row up", input::scan::kUp);
            row(pane::kActionRecipesDown, "row down", input::scan::kDown);
            row(pane::kActionRecipeChoose, "choose this recipe", input::scan::kReturn);
            // THE SAME ID THE BROWSING MODE BINDS TO THE SAME KEY, because it is the same
            // operation: open the source of the recipe under the cursor. Which recipe THAT is
            // is the mode's answer, not the id's (`edit_source`).
            row(pane::kActionEditSource, "edit source", input::scan::kE);
            row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
            row(pane::kActionRecipesClose, "close the list", input::scan::kEscape);
            return rows;
        }
        // ---- Browsing: the ids and default keys a maker's keymap already names ----------
        row(pane::kActionBuild, "build", input::scan::kB);
        row(pane::kActionBuildRealize, "load after build", input::scan::kB, input::mod::kShift);
        row(pane::kActionPromote, "promote image", input::scan::kP, input::mod::kShift);
        row(pane::kActionRevert, "revert image", input::scan::kR, input::mod::kShift);
        row(pane::kActionLoadIt, "load it", input::scan::kO);
        row(pane::kActionRecipeNext, "recipe", input::scan::kC);
        row(pane::kActionRecipeBack, "recipe back", input::scan::kC, input::mod::kShift);
        row(pane::kActionFrontier, "frontier", input::scan::kF);
        row(pane::kActionEditSource, "edit source", input::scan::kE);
        row(pane::kActionOutput, "read output", input::scan::kL);
        // ---- And the rows that came with the controls: Return, unclaimed while browsing,
        // opens the list, since a pane whose whole subject is one choice should open it there.
        row(pane::kActionRecipes, "choose a recipe...", input::scan::kReturn);
        row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
        // THE TWO HALVES OF `builder.build-realize`, each reachable on its own terms and
        // neither bound by default (WL-KEY-13): the shipped key keeps both meanings, and a
        // maker who wants one of them alone names its id.
        row(pane::kActionArm, "turn load-after-build on or off", input::scan::kUnknown);
        row(pane::kActionLoadBuilt, "load what was built", input::scan::kUnknown);
        return rows;
    }

    bool answers(const std::string& id) const {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    // ---- The asks -------------------------------------------------------------------

    /// WHY A FRONTIER WAS ASKED FOR. One shape answers two gestures -- the row this pane
    /// paints, and the one action whose whole decision is the answer -- and the answer must
    /// not be read as the other one.
    enum class Why { kPaint, kBuild };

    struct Ask {
        std::uint64_t pending = 0;
        bool awaiting = false;
        /// THE QUEUED ATTEMPT (Loom's sequence), kept so the bus's later word that exactly
        /// this send was refused before any handler ran is matched to it, and only to it.
        loom::Ticket attempt{};
        std::string subject; ///< the recipe the ask was about, for the sentence a refusal needs
    };

    void ask_status(loom::Mail& mail) {
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(builder::kBuilderRole, builder::StatusRequested{});
    }

    void ask_frontier(loom::Mail& mail, Why why) {
        frontier_.pending = ++asked_;
        frontier_.awaiting = true;
        frontier_.why = why;
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(ws::kProjectRole, ProjectFrontierRequested{}, frontier_.pending);
    }

    void send_build(loom::Mail& mail, const std::string& recipe, bool realize) {
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(builder::kBuilderRole, builder::BuildRequested{recipe, realize});
        awaiting_ = true;
        awaiting_realization_ = realize;
    }

    // ---- The catalog and the maker's choice -----------------------------------------

    /// WHERE THE NAMED RECIPE SITS, or `recipes.size()` for "it is not here". The pane holds
    /// a NAME, so this is the one place a name becomes a row and it is re-derived at every
    /// use rather than cached.
    std::size_t named_row(const std::string& recipe) const {
        for (std::size_t i = 0; i < known_.recipes.size(); ++i) {
            if (known_.recipes[i].recipe == recipe) {
                return i;
            }
        }
        return known_.recipes.size();
    }

    /// The row the maker is on: their explicit pick where it still names something, and the
    /// catalog's first row otherwise. `chosen` is bounded at use and never at write.
    std::size_t cursor_row() const {
        if (known_.recipes.empty()) {
            return 0;
        }
        const std::size_t at = named_row(state_.chosen);
        return at < known_.recipes.size() ? at : std::size_t{0};
    }

    /// Is there a recipe to act on at all, and if not, why not.
    bool has_recipe(const char* what) {
        if (!heard_) {
            notice_ = std::string("the Builder has not said what it builds yet -- ") + what;
            return false;
        }
        if (known_.recipes.empty()) {
            notice_ = std::string("this project has no build recipes -- ") + what;
            return false;
        }
        return true;
    }

    // ---- The gestures ---------------------------------------------------------------

    void build_now(loom::Mail& mail, bool realize) {
        if (!has_recipe("nothing was asked for")) {
            say(mail);
            return;
        }
        const std::string chosen = known_.recipes[cursor_row()].recipe;
        send_build(mail, chosen, realize);
        notice_ = "asked the Builder for `" + chosen + "`" +
                  (realize ? " and to realize it" : std::string()) +
                  " -- Workshop stays live while it builds";
        say(mail);
    }

    /// One action in two states. The button: an artifact is built and ready to load, nothing is
    /// armed and no build is in flight, so it sends the finished build's own ask again with the
    /// second intention aboard (`shown_.recipe`, never the cursor's row). The toggle, everywhere
    /// else: it flips the maker's standing intent and sends nothing.
    void build_realize(loom::Mail& mail) {
        const builder::BuildStatus& s = shown_;
        const bool ready = heard_ && !awaiting_ && !state_.arm && !s.recipe.empty() &&
                           s.outcome == builder::outcome::kSucceeded &&
                           s.realization == builder::realization::kNotAsked;
        if (ready) {
            send_build(mail, s.recipe, /*realize=*/true);
            notice_ = "loading the built `" + s.artifact +
                      "` now -- Workshop stays live while the incremental build confirms it";
            say(mail);
            return;
        }
        state_.arm = !state_.arm;
        notice_ = state_.arm ? "load after build: on -- the next build is offered to the "
                               "running project when it works"
                             : "load after build: off -- the next build is a plain build";
        say(mail);
    }

    /// THE ONE LOCAL JUDGEMENT: the tool's picture names a realization this Builder was part
    /// of. Everything else -- live, reloaded, promotable -- is the owner's to say.
    bool standing() const {
        return heard_ && !shown_.artifact.empty() &&
               (shown_.realization == builder::realization::kRealized ||
                shown_.realization == builder::realization::kRefused);
    }

    void promote_image(loom::Mail& mail) {
        if (!standing()) {
            notice_ = "nothing this Builder realized is standing -- nothing to promote";
            say(mail);
            return;
        }
        // ONE OFFER, DECIDED ELSEWHERE. Whether the artifact is live, whether it runs from a
        // per-operation copy, and whether the write is possible are the realization owner's
        // and the host's; this pane says one sentence and shows the answer.
        (void)mail.publish(builder::PromoteArtifact{shown_.artifact});
        awaiting_realization_ = true;
        notice_ = "asked to promote `" + shown_.artifact +
                  "` -- the file a restart loads takes the running image";
        say(mail);
    }

    void revert_image(loom::Mail& mail) {
        if (!standing()) {
            notice_ = "nothing this Builder realized is standing -- nothing to revert";
            say(mail);
            return;
        }
        (void)mail.publish(builder::RevertArtifact{shown_.artifact});
        awaiting_realization_ = true;
        // ...AND WHAT A REVERT DOES NOT TOUCH, said at the gesture and inside one row: the source
        // a maker saved is still the edited one, and the next build builds it. A running image and
        // a saved file are two facts, and a maker reading only the pane would take one for the other.
        notice_ = "asked to revert `" + shown_.artifact +
                  "`: the previous image runs, state kept; saved source unchanged";
        say(mail);
    }

    void choose_recipe(int by, loom::Mail& mail) {
        if (known_.recipes.empty()) {
            notice_ = heard_ ? "this project has no build recipes to choose between"
                             : "the Builder has not said what it builds yet -- nothing to "
                               "choose between";
            say(mail);
            return;
        }
        const std::size_t held = known_.recipes.size();
        const std::size_t at = cursor_row();
        const std::size_t to = by < 0 ? (at == 0 ? held - 1 : at - 1) : (at + 1 >= held ? 0 : at + 1);
        state_.chosen = known_.recipes[to].recipe;
        // THE ONE WRITER OF `picked`: this gesture is what makes a selection the MAKER's
        // rather than the catalog's order wearing a name. The frontier action reads it when
        // several recipes produce one artifact -- and it records WHICH recipe was picked, so a
        // choice another gesture later moves elsewhere carries no pick with it.
        picked_ = state_.chosen;
        notice_ = "build recipe: " + known_.recipes[to].recipe + " -> " +
                  known_.recipes[to].artifact;
        say(mail);
    }

    /// The frontier build, in two beats: the gesture asks for the owner's frontier and decides
    /// when the answer lands; nothing between is remembered but that this ask was the gesture's.
    void begin_frontier_build(loom::Mail& mail) {
        if (!heard_) {
            notice_ = "the Builder has not said what it builds yet -- nothing was asked for";
            say(mail);
            return;
        }
        ask_frontier(mail, Why::kBuild);
    }

    void finish_frontier_build(loom::Mail& mail) {
        if (!waiting_) {
            // THE ABSENCE IS THE OWNER'S OWN ANSWER, read a moment ago -- not a status this
            // pane manufactured. A project that is complete, still loading, or was never
            // begun is equally "not waiting", and all three are states in which there is no
            // frontier for this gesture to spend.
            notice_ = "this project is not waiting on any artifact -- nothing was asked for";
            say(mail);
            return;
        }
        std::size_t makers = 0;
        std::size_t match = 0;
        std::string named;
        for (std::size_t i = 0; i < known_.recipes.size(); ++i) {
            if (known_.recipes[i].artifact != frontier_artifact_) {
                continue;
            }
            ++makers;
            match = i;
            if (!named.empty()) {
                named += ", ";
            }
            named += "`" + known_.recipes[i].recipe + "`";
        }
        if (makers == 0) {
            notice_ = "no authored recipe produces `" + frontier_artifact_ +
                      "` -- nothing was asked for";
            say(mail);
            return;
        }
        if (makers > 1) {
            // A STANDING PICK IS THE MAKER'S PICK OF THE RECIPE STILL CHOSEN. Edit Code following a
            // pane's source and this gesture taking a lone producer both move the choice without
            // picking; a pick of the recipe they left does not stand for the one they chose.
            const std::size_t at = named_row(state_.chosen);
            const bool standing_pick =
                !picked_.empty() && picked_ == state_.chosen && at < known_.recipes.size() &&
                known_.recipes[at].artifact == frontier_artifact_;
            if (!standing_pick) {
                notice_ = std::to_string(makers) + " recipes produce `" + frontier_artifact_ +
                          "` (" + named + ") -- pick one, then the frontier action builds and "
                                          "realizes it";
                say(mail);
                return;
            }
            match = at;
        }
        // The selection moves with the gesture, visibly: row 1 now names the recipe this ask is
        // about, and `build_now`'s notice says it again, so the pane never shows one recipe while
        // the gesture sends another.
        state_.chosen = known_.recipes[match].recipe;
        build_now(mail, /*realize=*/true);
    }

    /// Load it, in two beats: the chosen recipe's artifact gains the minimum plan row, with a
    /// role the maker types, so the gesture first asks the host whether the plan already names
    /// the artifact and opens the line only if it does not.
    void begin_load_it(loom::Mail& mail) {
        if (!has_recipe("nothing to load")) {
            say(mail);
            return;
        }
        const builder::RecipeSummary& row = known_.recipes[cursor_row()];
        names_.pending = ++asked_;
        names_.awaiting = true;
        names_.recipe = row.recipe;
        names_.choice = choice_; // nonzero only when a menu row began this
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(ws::kProjectRole, PlanNamesRequested{row.artifact}, names_.pending);
    }

    void open_role(const std::string& stem, const std::string& recipe) {
        role_ = Role{};
        role_.open = true;
        role_.stem = stem;
        role_.recipe = recipe;
        role_.line.set(std::string(), 0);
        notice_ = "load `" + stem + "` -- type the role it holds";
    }

    void close_role() { role_ = Role{}; }

    void commit_role(loom::Mail& mail) {
        const std::string typed = trimmed(role_.line.text());
        // THE ROLE, THEN THE HOST. An empty role is refused here in the plan's own words
        // (`check_weave_role` says the same), and everything else -- the stem, the duplicate,
        // the executor's state, the file -- is the host's to say.
        if (typed.empty()) {
            notice_ = "a weave declaration needs a role -- nothing was loaded";
            say(mail);
            return;
        }
        row_.pending = ++asked_;
        row_.awaiting = true;
        row_.stem = role_.stem;
        row_.role = typed;
        row_.recipe = role_.recipe;
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(ws::kPlanRole, PlanRowRequested{role_.stem, typed, role_.recipe},
                          row_.pending);
        close_role();
        declare(mail);
        say(mail);
    }

    /// Edit the source the chosen recipe names: two doors, walked in order. The pane holds a
    /// recipe's name and never its procedure, so it asks the project office which one file that
    /// name means (`RecipeSourceRequested` -> `RecipeSourceSaid`), then asks the opening office
    /// to open it (`OpenSourceRequested` -> `SourceOpened`). Every refusal comes back as its
    /// owner's own sentence and lands in this pane's row.
    void edit_source(loom::Mail& mail) {
        if (!has_recipe("nothing was opened")) {
            say(mail);
            return;
        }
        // The first door, its ticket kept (WL-OPEN-07). Which recipe depends on the mode: while
        // the list is open the row the maker named is what they asked to edit, and the committed
        // choice is untouched (WL-PROJ-14); elsewhere the choice is the only cursor.
        const std::size_t at = choosing_.open ? list_row() : cursor_row();
        if (at >= known_.recipes.size()) {
            notice_ = "no recipe is under the cursor -- nothing was opened";
            say(mail);
            return;
        }
        const std::string recipe = known_.recipes[at].recipe;
        source_.pending = ++asked_;
        source_.awaiting = true;
        source_.subject = recipe;
        source_.attempt = mail.as_role(pane::kBuilderPaneRole)
                              .send_to_role(ws::kProjectRole, RecipeSourceRequested{recipe},
                                            source_.pending);
        if (!source_.attempt.valid()) {
            source_ = Ask{};
            notice_ = "`" + recipe + "`: the source could not be looked up -- nothing was queued "
                      "to " + ws::kProjectRole;
            say(mail);
        }
    }

    void begin_paste(loom::Mail& mail) {
        const loom::AskOpened opened = clip_asks_.open_to_role(
            surface::kSkinRole, surface::ClipboardTextRequested::zen_name,
            surface::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        paste_ask_ = opened.id;
        paste_epoch_ = role_.line.draft_epoch();
        pasting_ = true;
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(surface::kSkinRole, surface::ClipboardTextRequested{},
                          opened.correlation);
    }

    // ---- The output reader (WL-OUT-04) ------------------------------------------------------

    /// OPEN THE READER ON THE OPERATION THE TOOL'S PICTURE IS ABOUT, from its first line. It is
    /// bound to that operation's number from here on: a later build, a new choice or a catalog
    /// moves nothing it shows, and the tool's words about another operation are never shown
    /// under this one's header.
    // WL-OUT-04 -- agents/workshop/build-output.md
    void open_output(loom::Mail& mail) {
        if (!heard_) {
            notice_ = "the Builder has not said what it builds yet -- there is no output to read";
            say(mail);
            return;
        }
        if (shown_.op == 0) {
            notice_ = "no build has started in this Workshop -- there is no output to read";
            say(mail);
            return;
        }
        bind_output(shown_.op, mail);
    }

    void bind_output(std::int64_t op, loom::Mail& mail) {
        const bool was_open = output_.open;
        output_ = Output{};
        output_.open = true;
        output_.op = op;
        output_.top = 1;
        if (!was_open) {
            declare(mail);
        }
        ask_page(mail);
        say(mail);
    }

    void close_output(loom::Mail& mail) {
        output_ = Output{};
        declare(mail);
        say(mail);
    }

    /// ONE OF THE READER'S ROWS. Every id moves the view or closes it, and asks the tool again
    /// for the page the view now starts at; none reaches a build.
    void read_output(const std::string& id, loom::Mail& mail) {
        if (id == pane::kActionOutputClose) {
            close_output(mail);
        } else if (id == pane::kActionOutputUp) {
            scroll(-1, mail);
        } else if (id == pane::kActionOutputDown) {
            scroll(1, mail);
        } else if (id == pane::kActionOutputFirst) {
            output_.top = 1;
            ask_page(mail);
            say(mail);
        } else if (id == pane::kActionOutputLast) {
            output_.top = 0; // the page that ends at the last line, following what comes
            ask_page(mail);
            say(mail);
        } else if (id == pane::kActionOutputLeft || id == pane::kActionOutputRight) {
            const std::int64_t step = columns_ > 8 ? columns_ / 2 : 4;
            if (id == pane::kActionOutputLeft) {
                output_.pan = output_.pan > step ? output_.pan - step : 0;
            } else {
                output_.pan += step;
            }
            say(mail);
        } else if (id == pane::kActionOutputOlder || id == pane::kActionOutputNewer) {
            const std::vector<std::int64_t>& ops = output_.page.ops;
            std::size_t at = ops.size();
            for (std::size_t i = 0; i < ops.size(); ++i) {
                if (ops[i] == output_.op) {
                    at = i;
                }
            }
            const bool older = id == pane::kActionOutputOlder;
            if (at < ops.size() && older && at > 0) {
                bind_output(ops[at - 1], mail);
            } else if (at < ops.size() && !older && at + 1 < ops.size()) {
                bind_output(ops[at + 1], mail);
            } else {
                notice_ = older ? "no older build's output is kept"
                                : "no newer build's output is kept";
                say(mail);
            }
        }
    }

    /// MOVE THE VIEW BY `by` LINES, over the operation's own line numbers: a move into the lines
    /// the tool no longer keeps lands on the far side of them, and the view never starts past the
    /// last line said. From the following end, a move up starts where the shown page began.
    void scroll(std::int64_t by, loom::Mail& mail) {
        const builder::BuildOutputSaid& p = output_.page;
        std::int64_t top = output_.top == 0 ? (p.first > 0 ? p.first : 1) : output_.top;
        top += by;
        if (p.omitted > 0 && top >= p.omitted_from && top < p.omitted_from + p.omitted) {
            top = by > 0 ? p.omitted_from + p.omitted : p.omitted_from - 1;
        }
        if (top > p.said) {
            top = p.said;
        }
        if (top < 1) {
            top = 1;
        }
        if (top == output_.top) {
            return;
        }
        output_.top = top;
        ask_page(mail);
        say(mail);
    }

    /// ASK THE TOOL FOR THE PAGE THE VIEW STARTS AT, as many lines as the room shows. One ask
    /// outstanding: a newer one replaces the older, whose answer then settles nothing.
    void ask_page(loom::Mail& mail) {
        const std::int64_t lines = output_body_rows() > 0 ? output_body_rows() : 1;
        output_.ask.pending = ++asked_;
        output_.ask.awaiting = true;
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(builder::kBuilderRole,
                          builder::BuildOutputRequested{output_.op, output_.top, lines},
                          output_.ask.pending);
    }

    /// The rows the reader's lines may spend: the room, less its header, less a notice, less
    /// the control strip the reader draws under them.
    std::int64_t output_body_rows() const {
        return rows_ - 1 - (notice_.empty() ? 0 : 1) - strip_rows_for(output_controls());
    }

    /// THE READER'S ROWS. One header naming the operation, how it ended and which lines show;
    /// then the lines, each on ONE row, cut at the width with a mark and panned by the arrows,
    /// so a caret line stays under the line it points into and a long command echo costs one
    /// row. Every line is spelled in ASCII (`ascii_spelling`), and the header says how many
    /// characters that spelled on the rows shown. A gap the tool no longer keeps is a row of its
    /// own, never two ends shown as one.
    // WL-OUT-04 -- agents/workshop/build-output.md
    void say_output() {
        const builder::BuildOutputSaid& p = output_.page;
        const std::string number = "#" + std::to_string(output_.op);
        std::string head = "output " + number;
        if (!output_.heard) {
            push_row(head + " -- asking the Builder", surface::role::kAccent);
            say_controls(output_controls());
            return;
        }
        if (!p.kept) {
            std::string keeps;
            for (const std::int64_t op : p.ops) {
                keeps += (keeps.empty() ? "#" : ", #") + std::to_string(op);
            }
            push_row(head + " -- not kept any more: this Builder keeps the output of " +
                         (keeps.empty() ? std::string("no build") : keeps),
                     surface::role::kAlert);
            say_controls(output_controls());
            return;
        }
        std::vector<std::string> body;
        std::size_t spelled = 0;
        const std::int64_t room = output_body_rows();
        const std::int64_t asked_from = output_.top == 0 ? p.first : output_.top;
        if (p.omitted > 0 && p.first > asked_from && p.first == p.omitted_from + p.omitted) {
            body.push_back("... " + std::to_string(p.omitted) + " lines not kept ...");
        }
        std::int64_t last = p.first - 1;
        for (const std::string& line : p.text) {
            if (static_cast<std::int64_t>(body.size()) >= room) {
                break;
            }
            std::string shown = ascii_spelling(line, &spelled);
            const std::size_t pan = static_cast<std::size_t>(output_.pan);
            shown = shown.size() > pan ? shown.substr(pan) : std::string();
            body.push_back(std::move(shown));
            ++last;
        }
        if (p.omitted > 0 && last + 1 == p.omitted_from &&
            static_cast<std::int64_t>(body.size()) < room) {
            body.push_back("... " + std::to_string(p.omitted) + " lines not kept ...");
        }
        head += " " + p.recipe + " -- " + builder::name_of_outcome(p.outcome);
        if (p.ended && (p.outcome == builder::outcome::kFailed ||
                        p.outcome == builder::outcome::kSucceeded)) {
            head += ", exit " + std::to_string(p.status);
        }
        head += p.said == 0 ? std::string(" -- no lines")
                            : " -- lines " + std::to_string(p.first) + "-" +
                                  std::to_string(last < p.first ? p.first : last) + " of " +
                                  std::to_string(p.said) + (p.ended ? "" : " so far");
        if (p.omitted > 0) {
            head += ", " + std::to_string(p.omitted) + " not kept";
        }
        if (spelled > 0) {
            head += ", " + std::to_string(spelled) + " characters spelled in ASCII";
        }
        if (p.cut > 0) {
            head += ", " + std::to_string(p.cut) + " bytes cut from long lines";
        }
        if (output_.pan > 0) {
            head += ", from column " + std::to_string(output_.pan + 1);
        }
        if (shown_.op != 0 && shown_.op != output_.op) {
            head += " -- build #" + std::to_string(shown_.op) + " is newer";
        }
        push_row(head, surface::role::kAccent);
        for (std::string& line : body) {
            push_row(line, surface::role::kFill);
        }
        say_controls(output_controls());
    }

    // ---- The two sentences a settled build produces -----------------------------------

    static std::string build_words(const builder::BuildStatus& s) {
        return std::string(builder::name_of_outcome(s.outcome)) + " `" + s.recipe + "`" +
               (s.artifact.empty() ? std::string() : " -> " + s.artifact) +
               (s.detail.empty() ? std::string() : " -- " + s.detail);
    }

    static std::string realize_words(const builder::BuildStatus& s) {
        std::string said = std::string("realize: ") + builder::name_of_realization(s.realization);
        if (s.realization == builder::realization::kRealized && !s.default_image) {
            said += ", NOT DEFAULT (promote makes it the file a restart loads)";
        }
        if (!s.realized_detail.empty()) {
            said += " -- " + s.realized_detail;
        }
        return said;
    }

    // ---- Saying what the pane shows ---------------------------------------------------

    /// One row of the picture, with what it means recorded as it is written: a press is answered
    /// from the record the composition made, never from a second calculation. Spelled in what a
    /// canvas draws (WL-OUT-03): a row may carry another owner's words, and one byte Workshop's
    /// canvas cannot draw refuses the pane's whole picture (`judge_content`).
    void push_row(const std::string& text, std::int64_t role,
                  BuilderMeaning meaning = BuilderMeaning{}) {
        if (static_cast<std::int64_t>(composing_.size()) >= rows_) {
            return; // the room ran out: a row nobody can see names nothing
        }
        if (meaning.kind != builder_row::kNone) {
            map_.row(static_cast<std::int64_t>(composing_.size()), std::move(meaning));
        }
        composing_.push_back(
            surface::SurfaceTextRow{fit(ascii_spelling(text), columns_), role});
    }

    /// The whole picture. The notice leads and is composed first, since the row map records
    /// absolute rows (every mode already asks for one fewer row when a notice stands). It is
    /// cleared by the maker's next act, not by being said: one gesture can publish several times
    /// in one drain, Workshop keeps the last picture, and a notice cleared by the first `say`
    /// would be one no maker ever reads -- it is the answer to their last act.
    void say(loom::Mail& mail) {
        map_.begin();
        composing_.clear();
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            map_.settle();
            return;
        }
        if (!notice_.empty() && rows_ > 1) {
            push_row(notice_, surface::role::kAccent);
        }
        if (role_.open) {
            say_role();
        } else if (output_.open) {
            say_output();
        } else if (choosing_.open) {
            say_recipes();
        } else {
            say_builder();
        }
        ++published_;
        ws::v3::PaneContent said;
        said.pane = pane::kBuilderPane;
        said.rows = std::move(composing_);
        composing_.clear();
        said.picture = map_.settle();
        (void)mail.as_role(pane::kBuilderPaneRole).send_to_role(kWorkshopRole, said);
    }

    // ---- The controls a maker can press ---------------------------------------------

    /// ONE CONTROL: the operation it asks for, what it reads as, and whether this pane
    /// believes the operation applies. Availability is a HINT drawn on the face; every
    /// operation asks its own question again when the press arrives, and an unavailable
    /// control pressed answers with that operation's own refusal rather than with silence.
    struct ControlRow {
        ControlRow(const char* an_id, std::string a_label, bool is_available = true,
                   std::string a_subject = std::string(), std::int64_t a_subject_op = 0)
            : id(an_id), label(std::move(a_label)), available(is_available),
              subject(std::move(a_subject)), subject_op(a_subject_op) {}
        const char* id;
        std::string label;
        bool available = true;
        /// WHAT THIS FACE NAMES OUT LOUD, where it names anything -- `a` for `[load built a]`.
        /// Empty for a face whose label names no subject, which is then never subject-checked.
        std::string subject;
        /// THE OPERATION `subject` WAS TRUE OF -- the name alone does not tell two recipes
        /// sharing a stem apart (`target_op_of`); 0 where the face names no subject.
        std::int64_t subject_op = 0;
    };

    /// The Builder's controls, in the order a maker reads them: choose, build, decide what
    /// happens to what was built, then the two that change what a restart loads, then the two
    /// that read. `build`, `add to the load plan` and `edit source` act on the maker's choice,
    /// which the `recipe` row says; `load built ...`, `promote ...` and `revert ...` act on what
    /// was built or is standing, which can differ, so those three name what they will touch.
    std::vector<ControlRow> builder_controls() const {
        const bool ready = ready_to_load();
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
        controls.push_back(ControlRow{pane::kActionRecipes, "choose a recipe...",
                                      heard_ && !known_.recipes.empty()});
        controls.push_back(
            ControlRow{pane::kActionBuild, "build", heard_ && !known_.recipes.empty()});
        controls.push_back(ControlRow{pane::kActionArm,
                                      state_.arm ? "turn load-after-build off"
                                                 : "turn load-after-build on",
                                      !ready});
        controls.push_back(ControlRow{pane::kActionLoadBuilt,
                                      ready ? "load built " + shown_.artifact
                                            : std::string("load what was built"),
                                      ready, ready ? shown_.artifact : std::string(),
                                      ready ? shown_.op : 0});
        controls.push_back(ControlRow{pane::kActionLoadIt, "add to the load plan...",
                                      heard_ && !known_.recipes.empty()});
        controls.push_back(ControlRow{pane::kActionFrontier, "build what is waited on",
                                      heard_ && waiting_});
        controls.push_back(ControlRow{pane::kActionPromote,
                                      standing() ? "promote " + shown_.artifact
                                                 : std::string("promote the loaded image"),
                                      standing(),
                                      standing() ? shown_.artifact : std::string(),
                                      standing() ? shown_.op : 0});
        controls.push_back(ControlRow{pane::kActionRevert,
                                      standing() ? "revert " + shown_.artifact
                                                 : std::string("revert the loaded image"),
                                      standing(),
                                      standing() ? shown_.artifact : std::string(),
                                      standing() ? shown_.op : 0});
        controls.push_back(
            ControlRow{pane::kActionEditSource, "edit source", heard_ && !known_.recipes.empty()});
        controls.push_back(ControlRow{pane::kActionOutput,
                                      shown_.op == 0
                                          ? std::string("read output")
                                          : "read output #" + std::to_string(shown_.op),
                                      heard_ && shown_.op != 0,
                                      shown_.op == 0 ? std::string()
                                                     : "#" + std::to_string(shown_.op)});
        return controls;
    }

    std::vector<ControlRow> list_controls() const {
        const std::size_t at = list_row();
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
        controls.push_back(ControlRow{pane::kActionRecipeChoose, "choose this recipe",
                                      at < known_.recipes.size()});
        // (!!) NEITHER FACE NAMES THE RECIPE, AND THAT IS DELIBERATE. What they act on is the
        // list's CURSOR, which the `> ` marker says and only the maker's own act moves -- and
        // a face that named it would move this strip's spans on every row the maker looked at,
        // so the picture fence would refuse the second press of an ordinary double-click. The
        // subject a face NAMES is checked (`perform_on`); the subject a face points at is the
        // one the maker can see.
        controls.push_back(ControlRow{pane::kActionEditSource, "edit this recipe's source",
                                      at < known_.recipes.size()});
        controls.push_back(ControlRow{pane::kActionRecipesClose, "close the list", true});
        return controls;
    }

    std::vector<ControlRow> role_controls() const {
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
        controls.push_back(ControlRow{pane::kActionCommit, "load it with this role",
                                      !trimmed(role_.line.text()).empty()});
        controls.push_back(ControlRow{pane::kActionCancel, "cancel -- load nothing", true});
        return controls;
    }

    std::vector<ControlRow> output_controls() const {
        std::vector<ControlRow> controls;
        controls.push_back(ControlRow{pane::kActionMenu, "menu", true});
        controls.push_back(ControlRow{pane::kActionOutputUp, "up", true});
        controls.push_back(ControlRow{pane::kActionOutputDown, "down", true});
        controls.push_back(ControlRow{pane::kActionOutputFirst, "first line", true});
        controls.push_back(ControlRow{pane::kActionOutputLast, "last lines", true});
        controls.push_back(ControlRow{pane::kActionOutputLeft, "pan left", output_.pan > 0});
        controls.push_back(ControlRow{pane::kActionOutputRight, "pan right", true});
        controls.push_back(ControlRow{pane::kActionOutputOlder, "older build", true});
        controls.push_back(ControlRow{pane::kActionOutputNewer, "newer build", true});
        controls.push_back(ControlRow{pane::kActionOutputClose, "close output", true});
        return controls;
    }

    /// How many rows the strip may spend in the room this pane has. The controls do not get to
    /// eat the pane: in a six-row room two rows of buttons would cost the `realize` row, the one
    /// that says a loaded image is not the file a restart loads. So the strip is capped near a
    /// third of the room; what does not fit is counted and reachable through `[menu]`, and a room
    /// too small for one strip row leaves the right press.
    std::int64_t strip_budget() const {
        if (rows_ < 2) {
            return 0;
        }
        const std::int64_t want = (rows_ - 2) / 3;
        return want < 1 ? 1 : (want > kMaxControlRows ? kMaxControlRows : want);
    }

    component::ControlStrip packed(const std::vector<ControlRow>& controls) const {
        std::vector<component::Control> faces;
        faces.reserve(controls.size());
        for (const ControlRow& control : controls) {
            faces.push_back(component::Control{control.label, control.available});
        }
        return component::pack_controls(faces, columns_, strip_budget());
    }

    /// HOW MANY ROWS A STRIP OF THESE CONTROLS WOULD TAKE -- asked before the body is laid out,
    /// so each mode is given what is genuinely left rather than losing its tail afterwards.
    std::int64_t strip_rows_for(const std::vector<ControlRow>& controls) const {
        return static_cast<std::int64_t>(packed(controls).rows.size());
    }

    /// DRAW THE STRIP AND RECORD EVERY FACE AS A TARGET. A face the width cut is not recorded
    /// (`RowMap::span` refuses it): a press on the `...` a cut left behind must not operate a
    /// control the maker cannot read. What did not fit is counted on the last strip row, and
    /// the route to it is `[menu]`, never only a key.
    void say_controls(const std::vector<ControlRow>& controls) {
        const component::ControlStrip strip = packed(controls);
        for (std::size_t i = 0; i < strip.rows.size(); ++i) {
            std::string text = strip.rows[i];
            if (i + 1 == strip.rows.size() && strip.dropped > 0) {
                text += "  +" + std::to_string(strip.dropped) + " in menu";
            }
            const std::int64_t row = static_cast<std::int64_t>(composing_.size());
            push_row(text, surface::role::kFill);
            if (static_cast<std::int64_t>(composing_.size()) == row) {
                return; // the room ran out before this strip row
            }
            const std::int64_t solid = component::solid_columns(
                composing_[static_cast<std::size_t>(row)].text, text.size());
            for (const component::PlacedControl& placed : strip.placed) {
                if (placed.row != static_cast<std::int64_t>(i)) {
                    continue;
                }
                // The advertised subject and its operation are part of the meaning, so two
                // equal-width faces about different artifacts, or two builds sharing a stem, are
                // different spans: the picture moves when the promise does, and a press queued
                // against the older one is refused by the fence (`perform_on`).
                map_.span(row, placed.first, placed.width, solid,
                          BuilderMeaning{builder_row::kControl, 0, controls[placed.index].id,
                                         controls[placed.index].subject,
                                         controls[placed.index].subject_op});
            }
        }
    }

    /// THE PROMPT THE ROLE LINE DRAWS IN FRONT OF WHAT A MAKER IS TYPING -- the label's own
    /// full text, unshortened: what `active_role_prompt` shortens FROM.
    std::string role_prompt() const { return "role for " + role_.stem + "> "; }

    /// THE ROLE PROMPT AS DRAWN -- `role_prompt`'s full label in every room wide enough to give
    /// the typed value `kMinRoleValueColumns` beside it, shortened otherwise (`fitted_label`).
    /// Read here and by the press handler alike, so painting and the caret math it feeds never
    /// disagree about where the value begins (`files/files.cpp`'s `active_prompt`, the same
    /// repair for the same construction).
    std::string active_role_prompt() const {
        return fitted_label(role_prompt(), columns_, kMinRoleValueColumns);
    }

    void say_role() {
        const std::string prompt = active_role_prompt();
        const std::int64_t cols =
            columns_ > static_cast<std::int64_t>(prompt.size()) + 1
                ? columns_ - static_cast<std::int64_t>(prompt.size()) - 1
                : 1;
        role_.line.keep_caret_visible(cols);
        push_row(prompt + role_.line.visible(cols), surface::role::kAccent,
                 BuilderMeaning{builder_row::kLine, 0, {}, role_.stem});
        push_row(panel_field("loads", role_.stem + " (built by `" + role_.recipe + "`)"),
                 surface::role::kMuted);
        say_controls(role_controls());
    }

    /// THE RECIPE LIST: the catalog on rows, with the list's own cursor on one of them.
    ///
    /// (!!) THE CURSOR IS NOT THE CHOICE, and the heading says which row IS. A list whose cursor
    /// was the choice would arm the next build against whatever a maker was merely looking at.
    void say_recipes() {
        const std::size_t held = known_.recipes.size();
        const std::size_t at = list_row();
        std::string head = "choose a recipe -- " + std::to_string(held) +
                           (held == 1 ? " recipe" : " recipes");
        if (!known_.source.empty()) {
            head += " in " + known_.source;
        }
        push_row(head, surface::role::kAccent);
        const std::vector<ControlRow> controls = list_controls();
        const std::int64_t body_rows = rows_ - 1 - (notice_.empty() ? 0 : 1) -
                                       strip_rows_for(controls);
        if (body_rows > 0 && held > 0) {
            const component::ListWindow win = component::cursor_window(
                held, at, choosing_.hint, static_cast<std::size_t>(body_rows));
            choosing_.hint = win.first;
            // A marker is a row of the same budget, said only where the window reserved one
            // (`ListWindow::markers`): saying it anyway pushed the control strip, the one row a
            // maker with a mouse cannot lose, out of the room.
            if (win.before > 0 && win.markers > 0) {
                push_row("  ... " + std::to_string(win.before) + " earlier",
                         surface::role::kMuted);
            }
            for (std::size_t i = win.first; i < win.end(); ++i) {
                const builder::RecipeSummary& row = known_.recipes[i];
                const bool here = i == at;
                const bool chosen = row.recipe == state_.chosen;
                push_row(std::string(here ? "> " : "  ") + row.recipe + " -> " + row.artifact +
                             (chosen ? "   (chosen)" : ""),
                         here ? surface::role::kAccent : surface::role::kFill,
                         BuilderMeaning{builder_row::kRecipe, i, {}, row.recipe});
            }
            if (win.after > 0 && win.markers > 0) {
                push_row("  ... " + std::to_string(win.after) + " more", surface::role::kMuted);
            }
        }
        say_controls(controls);
    }

    /// The facts, composed against the budget: nine facts do not fit five rows, so each carries a
    /// survival priority. What a maker is acting on survives longest -- the office, the live
    /// build's activity, the frontier, what a build will do next, the realization outcome, the
    /// compiler's words -- and static metadata and the output's tail yield first. The display
    /// order never changes with the budget: a shorter face shows the same rows, fewer of them.
    void say_builder() {
        struct Fact {
            std::string text;
            std::int64_t role = surface::role::kFill;
            std::int64_t priority = 0; ///< smaller survives longer; all distinct
            BuilderMeaning meaning{};  ///< what a press on this row names, where it names one
        };
        std::vector<Fact> facts; // display order, priorities deciding survival
        // THE CONTROLS ARE ASKED FOR FIRST, because their rows come out of the same budget the
        // facts are seated in: a strip appended after the panel had already filled the room
        // would be the row that silently vanished, and it is the only mouse route there is.
        const std::vector<ControlRow> controls = builder_controls();

        // The header names the office it presents -- the tool's, whose facts these are; this
        // pane's own office is the pane header Workshop draws above the room. It also names the
        // catalog in force, carried by `RecipeCatalog` from its one owner (WL-PROJ-09).
        std::string head = std::string("BUILDER @") + builder::kBuilderRole;
        if (!known_.source.empty()) {
            head += "  " + known_.source;
        }
        facts.push_back(Fact{head, surface::role::kAccent, 0});

        if (!heard_) {
            // NOT THE SAME AS "NEVER BUILT", and the pane must not show it as though it were.
            // This is a fact about this pane -- it has asked and is waiting -- and the
            // recipe's own history is not knowable from here until the tool says it.
            facts.push_back(Fact{panel_field("recipe", "(the Builder has not answered yet)"),
                                 surface::role::kMuted, 1});
            publish(std::move(facts), std::string(), controls);
            return;
        }

        const builder::BuildStatus& s = shown_;
        // WHAT THE MAKER HAS PICKED OUT, AND HOW MANY THERE ARE TO PICK FROM. It is the
        // CHOICE and not the last build, and when they differ the choice is the truer row:
        // it is what a build will do next, which is the question a maker looking at this pane
        // is actually asking.
        const std::size_t held = known_.recipes.size();
        if (held == 0) {
            facts.push_back(Fact{panel_field("recipe", "(this project has no build recipes)"),
                                 surface::role::kMuted, 3});
        } else {
            const std::size_t at = cursor_row();
            // THE ROW THAT NAMES THE CHOICE IS ALSO A WAY INTO THE LIST: a press on it opens
            // the catalog. The subject is the recipe it names, so the picture moves when the
            // choice does -- and the row itself reads exactly as it always did, because the
            // control that ADVERTISES the list is `[choose a recipe...]` beside it.
            facts.push_back(Fact{panel_field("recipe", known_.recipes[at].recipe + " -> " +
                                                           known_.recipes[at].artifact + "  (" +
                                                           std::to_string(at + 1) + "/" +
                                                           std::to_string(held) + ")"),
                                 surface::role::kFill, 3,
                                 BuilderMeaning{builder_row::kRecipe, at, {},
                                                known_.recipes[at].recipe}});
        }
        // What the project is waiting on, while it is: the row exists exactly while the frontier
        // does and costs the third `said` row, which a waiting project has no output for. The
        // artifact and the blocked count are the realization owner's, through the host's
        // read-only door; which recipes produce it is the tool's catalog, joined by stem.
        const std::size_t shift = waiting_ ? 1u : 0u;
        if (waiting_) {
            std::size_t makers = 0;
            const builder::RecipeSummary* maker = nullptr;
            for (const builder::RecipeSummary& known : known_.recipes) {
                if (known.artifact == frontier_artifact_) {
                    ++makers;
                    maker = &known;
                }
            }
            std::string said = "waiting " + frontier_artifact_ + " (";
            if (makers == 0) {
                said += "no recipe";
            } else if (makers == 1) {
                said += maker->recipe;
            } else {
                said += std::to_string(makers) + " recipes";
            }
            said += ", blocks " + std::to_string(frontier_blocked_) + ")";
            facts.push_back(Fact{panel_field("project", said), surface::role::kAccent, 2});
        }
        // WHAT THIS PANE IS WATCHING beats what it was last told: the tool's last OUTCOME is
        // still the previous build's while a new one runs, and showing that would answer
        // "what happened on the last build" with a sentence about the wrong build.
        const bool named_op = s.op != 0;
        // A BUILD THAT DID NOT PRODUCE ITS ARTIFACT POINTS AT ITS OWN WORDS, by the action's label:
        // the `said` rows keep its last lines, and the reader keeps every line the tool kept.
        const bool unproduced = s.outcome == builder::outcome::kFailed ||
                                s.outcome == builder::outcome::kNoArtifact ||
                                s.outcome == builder::outcome::kNotStarted;
        const std::string carried =
            named_op ? " -- op #" + std::to_string(s.op) + ", " + std::to_string(s.chunks) +
                           " out" + (unproduced ? std::string(" -- read output") : std::string())
                     : std::string();
        const bool unanswered = awaiting_ && s.outcome != builder::outcome::kRunning;
        facts.push_back(
            Fact{unanswered ? panel_field("last", "asked -- waiting for it to start")
                            : panel_field("last", std::string(builder::name_of_outcome(s.outcome)) +
                                                      carried),
                 unanswered || s.outcome == builder::outcome::kRunning
                     ? surface::role::kAccent
                     : (s.outcome == builder::outcome::kFailed ||
                                s.outcome == builder::outcome::kNotStarted ||
                                s.outcome == builder::outcome::kNoArtifact ||
                                s.outcome == builder::outcome::kUnknownRecipe
                            ? surface::role::kAlert
                            : surface::role::kFill),
                 1});
        // THE EXIT STATUS IS ONLY SHOWN WHEN THERE WAS ONE. A `0` printed after a build that
        // never started reads as success, which is the exact wrong answer at the exact moment
        // a maker most needs the right one. The tool's own counter shares the row, because it
        // is the number that proves the tool outlives its presentation.
        facts.push_back(
            Fact{panel_field("exit", pad(s.outcome == builder::outcome::kSucceeded ||
                                                 s.outcome == builder::outcome::kFailed
                                             ? std::to_string(s.status)
                                             : std::string("--"),
                                         11) +
                                         "asks " + std::to_string(s.builds) + " ever"),
                 surface::role::kMuted, 6});
        // WHAT WAS ACTUALLY RUN, as the runner reported it. The first fact a constrained
        // budget gives up: it is an echo of the maker's own act.
        facts.push_back(Fact{panel_field("ran", s.command.empty()
                                                    ? std::string("(nothing has run yet)")
                                                    : s.command),
                             surface::role::kMuted, 7});
        // A BUILD OUTCOME AND A REALIZATION OUTCOME ARE TWO ANSWERS AND THIS PANE SHOWS TWO.
        // The row has three faces: ARMED (`[x] load after build`), THE BUTTON (an artifact is
        // built, nothing was asked about realizing it, nothing is armed), and OTHERWISE the
        // realization outcome -- with the clause a maker must not miss, that a realized image
        // which is NOT the file a restart loads says so.
        const bool button = !awaiting_ && !state_.arm && !s.recipe.empty() &&
                            s.outcome == builder::outcome::kSucceeded &&
                            s.realization == builder::realization::kNotAsked;
        std::string realize_face;
        std::int64_t realize_role = surface::role::kMuted;
        if (state_.arm && s.realization == builder::realization::kNotAsked) {
            realize_face = "[x] load after build";
            realize_role = surface::role::kAccent;
        } else if (button) {
            realize_face = "-- (load-after-build loads " + s.artifact + " now)";
            realize_role = surface::role::kAccent;
        } else if (s.realization == builder::realization::kNotAsked) {
            realize_face = "-- (load-after-build arms it)";
        } else {
            // THE OPERATION IT IS ABOUT, named the way the `last` row names its own: two reloads in
            // a row end on the same words, and only the number tells the newer answer from the older.
            realize_face = std::string(builder::name_of_realization(s.realization)) + " -- op #" +
                           std::to_string(s.op);
            if (s.realization == builder::realization::kRealized && !s.default_image) {
                realize_face += ", NOT DEFAULT (promote / revert)";
            }
            if (!s.realized_detail.empty()) {
                realize_face += " -- " + s.realized_detail;
            }
            realize_role = s.realization == builder::realization::kRefused
                               ? surface::role::kAlert
                               : (s.realization == builder::realization::kRealized
                                      ? surface::role::kFill
                                      : surface::role::kMuted);
        }
        facts.push_back(Fact{panel_field("realize", realize_face), realize_role, 4});
        // THREE ROWS FOR WHAT THE BUILD SAID, because this is the row budget a maker spends
        // when something has gone wrong, and one row of a compiler's answer is a row of
        // nothing. While the project is WAITING, the `project` row holds the third of them.
        // They are PLACEHOLDERS here: `publish` wraps the detail into exactly the rows that
        // survive the budget, so the elision mark tells the truth about THIS face.
        const std::size_t said_max = 3 - shift;
        const std::int64_t said_priorities[3] = {5, 8, 9};
        for (std::size_t i = 0; i < said_max; ++i) {
            facts.push_back(Fact{std::string(), surface::role::kMuted, said_priorities[i]});
        }
        publish(std::move(facts), s.detail, controls);
    }

    /// Keep what the budget seats, in display order. The priorities are distinct, so "the
    /// smallest that fit" is one threshold, and the said block is wrapped last, into exactly the
    /// rows that survived; a dropped fact is dropped whole. The budget is one row smaller when a
    /// notice stands, so the composition is asked for fewer rows rather than losing its last.
    template <class Facts>
    void publish(Facts facts, const std::string& said_detail,
                 const std::vector<ControlRow>& controls) {
        const std::int64_t budget =
            rows_ - (notice_.empty() ? 0 : 1) - strip_rows_for(controls);
        if (budget <= 0) {
            say_controls(controls); // the controls outrank the facts: they are the only route
            return;
        }
        std::vector<std::int64_t> priorities;
        priorities.reserve(facts.size());
        for (const auto& f : facts) {
            priorities.push_back(f.priority);
        }
        std::sort(priorities.begin(), priorities.end());
        const std::size_t seats = static_cast<std::size_t>(budget);
        const std::int64_t cut =
            priorities.size() > seats ? priorities[seats] : priorities.back() + 1;
        std::size_t said_kept = 0;
        for (const auto& f : facts) {
            if (f.priority < cut && f.text.empty()) {
                ++said_kept; // a said placeholder: counted now, written below
            }
        }
        std::vector<std::string> said;
        if (said_kept > 0) {
            said = panel_block("said",
                               said_detail.empty() ? std::string("--")
                                                   : ascii_spelling(said_detail),
                               said_kept, columns_);
        }
        std::size_t said_at = 0;
        for (auto& f : facts) {
            if (f.priority >= cut) {
                continue;
            }
            std::string text = f.text.empty() ? said[said_at++] : f.text;
            push_row(text, f.role, f.meaning);
        }
        say_controls(controls);
    }

    // ---- What this pane is ------------------------------------------------------------

    zengine::ActivationCursor activation_;
    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    bool granted_ = false;

    /// THE TOOL'S PICTURE, HELD ONLY WHILE THIS PANE IS SHOWING IT (WL-PROJ-11, WL-PROJ-12).
    /// `heard_` tells "the tool has not answered" from "the tool never built anything";
    /// `awaiting_` is the only fact here that is genuinely this pane's own, and it is what
    /// decides whether an arriving status is news.
    bool heard_ = false;
    bool awaiting_ = false;
    bool awaiting_realization_ = false;
    builder::BuildStatus shown_{};
    builder::RecipeCatalog known_{};
    /// WHICH RECIPE THE MAKER LAST PICKED WITH `c`, by name -- empty when they never have, or when
    /// a catalog no longer holds it. The frontier action is the only reader (WL-PROJ-14), and it
    /// counts the pick only while that recipe is still the choice. A member and not state, like
    /// the picture: a reloaded pane has picked nothing.
    std::string picked_;

    /// THE PROJECT'S FRONTIER, AS OF THE LAST ANSWER. Not published, not state, and not
    /// derived here: three values the host's read-only door said, and a flag saying whether
    /// anything has been said at all.
    bool frontier_known_ = false;
    bool waiting_ = false;
    std::string frontier_artifact_;
    std::int64_t frontier_blocked_ = 0;

    std::string notice_;
    /// HOW MANY PICTURES THIS PANE HAS PUBLISHED -- counted by `say` at the send, so a handler
    /// can tell whether the act it ran already said its rows.
    std::uint64_t published_ = 0;
    std::uint64_t asked_ = 0;

    struct FrontierAsk : Ask {
        Why why = Why::kPaint;
    } frontier_;
    struct NamesAsk : Ask {
        std::string recipe; ///< what the answer will open the role line for
        /// THE MENU CHOICE THAT BEGAN THIS, or zero. A chosen row that opens a text line owes
        /// that line the keyboard, and the line here opens only once this ask is answered --
        /// so the choice's own number crosses the round trip with it (`chose`).
        std::uint64_t choice = 0;
    } names_;
    struct RowAsk : Ask {
        std::string stem;
        std::string role;
        std::string recipe;
    } row_;
    Ask source_; ///< the resolution, at the project office
    Ask open_; ///< the opening, at the opening office

    struct Role {
        bool open = false;
        std::string stem;   ///< the artifact the role is for
        std::string recipe; ///< the chosen recipe, whose product this may load
        component::TextBox line;
    } role_;

    /// THE RECIPE LIST: whether it is open, and which recipe its cursor stands on BY NAME.
    /// A member and not state, like the role line and the reader -- a reloaded pane is not
    /// choosing. The name and not an index, for `chosen`'s own reason (WL-PROJ-07): a catalog
    /// republished while the list is open moves the cursor with the recipe it named.
    struct Choosing {
        bool open = false;
        std::string name;
        std::size_t hint = 0; ///< where the window began last time, for least motion
    } choosing_;

    /// WHAT EACH ROW AND EACH RUN OF COLUMNS IN THE LAST PICTURE MEANS, and the number of that
    /// picture -- replaced whole by every `say`. A press is answered from this record and from
    /// nowhere else, and one that names an older number is refused.
    component::RowMap<BuilderMeaning> map_;
    /// THE ROWS BEING COMPOSED, held while `say` runs so each mode's composer and the control
    /// strip write into one list and the map records the row each of them landed on.
    std::vector<surface::SurfaceTextRow> composing_;
    /// NOTCHES NOT YET WORTH A WHOLE ROW -- one accumulator for the one list in force.
    double wheel_ = 0.0;
    /// THIS IMAGE'S ONE OUTSTANDING MENU. Deliberately not reload-kept state: a successor that
    /// inherited it would accept its predecessor's menu as its own (`pane_menu::Asked`).
    pane_menu::Asked asked_menu_;
    /// WHAT THE ROWS OF THAT MENU PROMISED, row by row (`offer_menu`). Replaced whole every
    /// time a menu is offered, and read once when its answer lands.
    std::vector<Offered> offered_;
    /// THE NUMBER OF THE MENU CHOICE BEING SPENT, for the length of one `chose` and no longer.
    std::uint64_t choice_ = 0;

    /// THE OUTPUT READER: which operation it is bound to, where its view starts, and the one
    /// page the tool last answered for it. A member and not state, like the role line -- a
    /// reloaded pane is not reading, and nothing here is a copy the tool could disagree with
    /// for longer than one page.
    struct Output {
        bool open = false;
        std::int64_t op = 0;    ///< the operation this reader is bound to
        std::int64_t top = 1;   ///< the first line number in view; 0 follows the last line
        std::int64_t pan = 0;   ///< the first column in view
        bool heard = false;     ///< the tool has answered a page for this binding
        Ask ask;                ///< the page asked for and not yet answered
        builder::BuildOutputSaid page{};
    } output_;

    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(BuilderPaneWeave)
