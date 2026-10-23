// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Builder pane: a loadable weave that offers Workshop one pane -- the weaver's seat at the
// build, at the realization frontier, and at the two acts a reload leaves behind. What it needs
// of the host (what realization is waiting on, whether the plan already names an artifact, the
// file a recipe was authored from) it asks for through the doors
// `workshop/builder_seam_vocabulary.hpp` spells; what crosses is values. Its keys are its own:
// a pane's rows are active only while it holds the keyboard, so a weaver presses into it first.
// Builder law: agents/realization.md

#include "builder/builder-pane/vocabulary.hpp"

#include "workshop/actor_scope.hpp" // the action classes each act asks Workshop about
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/open_seam_vocabulary.hpp" // the opening office the open is asked of
#include "workshop/pane_causes.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_text.hpp"

#include "workshop/pane_menu.hpp"
#include "workshop/pane_escape.hpp"

#include "builder/vocabulary.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "component/text_box.hpp"
#include "graphics/vocabulary.hpp"
#include "input/vocabulary.hpp"
#include "workshop/pane_rows.hpp"

#include "manuals/zengine.builder-pane.hpp" // this pane's manual, compiled in from its page

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/lifecycle.hpp>
#include <zen/weave/standard_shapes.hpp>

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
namespace gfx = zengine::graphics;
namespace ws = zengine::workshop;
namespace pane = zengine::builder_pane;
namespace builder = zengine::builder;
namespace pane_menu = zengine::workshop::pane_menu;

using ws::v3::PaneActionRow;
using ws::PaneKey;
using ws::v2::PaneOffered;
using ws::PanePassRequested;
using ws::PaneTextInput;
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

/// The office Workshop holds, named as a string rather than through `workshop/panes.hpp`: a
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

/// A labelled row, in the pane's own nine-column gutter.
std::string labelled_row(const char* label, const std::string& value) {
    return pad(label, 9) + value;
}

/// A ROLE A WEAVER TYPES IS ONE LINE OF PLAIN TEXT. The plan's own law judges it (the host
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
/// at what a weaver could see, so rows that moved between the aim and the delivery are said,
/// never spent on whatever slid into that place.

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
    bool available = true; ///< a control drawn `[label]`; false drawn `(label)`

    bool operator==(const BuilderMeaning& o) const {
        return kind == o.kind && index == o.index && id == o.id && subject == o.subject &&
               op == o.op && available == o.available;
    }
};

// ---- What the pane asked and still waits on (`AskFacet`) -----------------------------------

/// WHAT A GESTURE ASKS WORKSHOP ABOUT, and goes on to do once Workshop says its actor may; and
/// what the pane asks an office: the frontier its row paints and the frontier action's own, the
/// plan's names, a plan row, a recipe's source, its opening, a build sent, and the reader's page.
enum class Act {
    kBuild, kLoadBuilt, kPromote, kRevert, kFrontier, kRow, kOpen,
    kPaint, kFrontierAsk, kNames, kPlanRow, kSource, kOpening, kBuildSent, kPage
};

/// ONE ASK, AND THE ACT OR ANSWER IT WAITS ON, captured when it was asked: the answer does what
/// the weaver aimed at, never whatever the pane holds when it lands.
struct BuilderAsk {
    Act act = Act::kBuild;
    std::string subject;   ///< the recipe the ask was about, for the sentence a refusal needs
    bool realize = false;  ///< whether a build is offered for loading when it works
    std::string artifact;  ///< what a load, a promote or a revert names
    std::int64_t op = 0;   ///< the build operation that artifact was true of
    std::string stem;      ///< the role line's artifact
    std::string role;      ///< the role typed for it
    std::string path;      ///< the source an open opens: the ask's subject
    std::string undone;    ///< what did not happen, the sentence a refusal opens with
    /// THE MENU CHOICE THAT BEGAN A NAMES ASK, or zero. A chosen row that opens a text line owes
    /// that line the keyboard, and the line opens only once the ask is answered -- so the
    /// choice's own number crosses the round trip with it (`chose`).
    std::uint64_t choice = 0;
    std::uint64_t gesture = 0; ///< the gesture that began a source ask: `e` asks Workshop there
};

/// An ask of `act`, nothing captured yet.
inline BuilderAsk asking(Act act) {
    BuilderAsk a;
    a.act = act;
    return a;
}

/// Which asks an act is: the matcher every lookup in the table reads.
inline auto of_kind(Act act) {
    return [act](const BuilderAsk& a) { return a.act == act; };
}

// =============================================================================
// The weave
// =============================================================================

using Rows = ws::PaneRowsFacet<pane::kBuilderPane, pane::kBuilderPaneRole,
                               zengine::manual::pages::zengine_builder_pane::kManual,
                               BuilderMeaning>;
using Asks = ws::AskFacet<pane::kBuilderPane, pane::kBuilderPaneRole, BuilderAsk>;
using Actions = ws::PaneActionFacet<pane::kBuilderPane, pane::kBuilderPaneRole>;
using Menu = ws::PaneMenuFacet<pane::kBuilderPane, pane::kBuilderPaneRole>;

class BuilderPaneWeave
    : public zengine::With<Rows, Asks, Actions, Menu>::Weave<
          BuilderPaneWeave, pane::BuilderPaneState,
          loom::Accept<PaneKey, PaneTextInput, builder::BuildStatus, builder::RecipeCatalog,
                       ProjectFrontierSaid, PlanNames, PlanRowWritten, RecipeSourceSaid,
                       SourceOpened, gfx::ClipboardCopy, gfx::ClipboardText,
                       PaneSourceOpened, builder::BuildOutputSaid>,
          loom::Emit<builder::StatusRequested, builder::BuildRequested, builder::PromoteArtifact,
                     builder::RevertArtifact, ProjectFrontierRequested, PlanNamesRequested,
                     PlanRowRequested, RecipeSourceRequested, OpenSourceRequested,
                     ws::PaneKeyboardRequested, ws::PaneEscapeUnspent, gfx::ClipboardCopy,
                     gfx::ClipboardTextRequested, builder::BuildOutputRequested>> {
public:
    PaneOffered offering() const {
        return PaneOffered{pane::kBuilderPane, pane::kBuilderPaneName, pane::kBuilderPaneSummary,
                           14, 70};
    }

    void offered(loom::Mail& mail) { declare(mail); }

    /// Workshop grants the pane its room, prose or canvas: the one beat on which this tool draws,
    /// and when the pane asks the two questions whose answers it deliberately does not keep --
    /// what the tool is (`StatusRequested`, answered with both its catalog and its status) and
    /// what the project is waiting on. It asks every time: each grant is a moment at which a
    /// picture this pane did not derive may be stale, and asking is one message.
    void on_room(loom::Mail& mail) {
        ask_status(mail);
        ask_frontier(mail, Act::kPaint);
        say(mail);
    }

    /// ONLY THE ROLE LINE READS RAW KEYS. Everything else this pane does arrives as a
    /// resolved id (`on_action`); a component's editing gestures are the component's, not the
    /// pane's commands.
    void on(const PaneKey& key, loom::Mail& mail) {
        if (!mail.authored_from_role(kWorkshopRole) || key.pane != pane::kBuilderPane) {
            return;
        }
        // Escape's default: this pane holds nothing selected, so it hands Escape back.
        if (ws::pane_escape::answer(key, mail, pane::kBuilderPaneRole, [] { return false; })) {
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
            mail.publish(gfx::ClipboardCopy{clip_.text});
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

    /// ONE OF THE PANE'S DECLARED ACTIONS, ASKED FOR BY NAME (WL-KEY-15): the id Workshop resolved
    /// the keystroke to against the effective keymap, admitted by `answers` in the mode the pane is
    /// in. (!) A KEY AND A CONTROL REACH ONE OPERATION: the id a keystroke resolved to and the id a
    /// pressed control carries are the same id, spent through the same `perform`, so a weaver's
    /// remapped key and the button beside it cannot come to mean two different things.
    void on_action(const std::string& id, loom::Mail& mail) {
        // THE WEAVER HAS ACTED, SO THE LAST ACT'S ANSWER IS SPENT: an act whose answer is still on
        // its way (Workshop's word on a build, `e`'s lookup, `o`'s plan names), or that meant
        // nothing, says no picture, and the rows are said once without the notice.
        room().spend_notice(notice_, [&] { perform(id, mail); }, [&] { say(mail); });
    }

    /// The mode owns the pane's actions first, and an id it does not answer to is no act: the
    /// declaration and the keystroke race across two messages (Escape and Return in one poll
    /// resolve to a cancel and a commit, and the commit arrives after the line closed), so a stale
    /// id, or one nobody declared, spends nothing and the notice stands (`agents/panes.md`).
    bool answers(const std::string& id) const {
        for (const PaneActionRow& row : action_rows()) {
            if (row.id == id) {
                return true;
            }
        }
        return false;
    }

    /// What an id will act on right now: the one place that says so, read when a control is
    /// painted or a menu row offered, and again when either is spent; empty for an id with no
    /// subject. Not the label and not the cursor: a label is what the weaver was promised, this
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
    /// under `[load built a]` without the weaver acting. A numbered press is fenced by its picture
    /// first (WL-HAND-03), and the subject in a control's meaning moves that picture with the
    /// promise; a menu carries no picture, so this check is its only fence. The operation is
    /// compared beside the name, never shown: the notice quotes the artifact the weaver read.
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
        const bool ended = !builder::still_going(said.outcome);
        heard_ = true;
        shown_ = said;
        if (ended) {
            awaiting_ = false;
            (void)asks().forget(of_kind(Act::kBuildSent));
        }
        // THE FRONTIER MOVES WHEN REALIZATION DOES, and realization moves behind a settled
        // build. Asking here is what keeps the `project` row honest without a publication
        // nobody asked the owner for.
        if (ended) {
            ask_frontier(mail, Act::kPaint);
        }
        const bool build_news = watching && ended;
        if (build_news) {
            notice_ = build_words(said);
        }
        // THE LOAD IS ANNOUNCED AT ITS OWN END, after the build's: realized or REFUSED, about the
        // operation whose build this pane watched end. A build that ended without offering its
        // artifact ended the load with it, and the build's sentence says so.
        if (awaiting_realization_ && build_news) {
            if (said.outcome == builder::outcome::kSucceeded &&
                said.realization == builder::realization::kOffered) {
                loading_op_ = said.op;
            } else {
                awaiting_realization_ = false;
            }
        } else if (awaiting_realization_ && !watching) {
            if (said.op != loading_op_) {
                awaiting_realization_ = false; // another operation's: this load is no longer said
            } else if (said.realization == builder::realization::kRealized ||
                       said.realization == builder::realization::kRefused) {
                awaiting_realization_ = false;
                notice_ = realize_words(said);
            }
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
        if (!asks().answered(mail, of_kind(Act::kPage))) {
            return;
        }
        if (!output_.open || said.op != output_.op) {
            return;
        }
        output_.page = said;
        output_.heard = true;
        say(mail);
    }

    /// THE WHEEL SCROLLS THE READER, three lines a notch, the Editor's measure. Outside the
    /// reader it means nothing here.
    void wheel(double dy, loom::Mail& mail) {
        if (!output_.open && !choosing_.open) {
            return; // the fact view is not a list: there is nothing for a notch to walk
        }
        const std::int64_t notches = component::take_wheel_notches(wheel_, dy * 3.0);
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
    /// THE CHOICE FOLLOWS ITS RECIPE AND NEVER ITS ROW (WL-PROJ-07). The weaver's pick is
    /// held BY NAME (`BuilderPaneState::chosen`), so a reordered catalog moves it with no
    /// work at all and a catalog that no longer holds it releases it -- there is no index to
    /// carry, and therefore no index to carry wrongly.
    void on(const builder::RecipeCatalog& said, loom::Mail& mail) {
        known_ = said;
        if (!state_.chosen.empty() && named_row(state_.chosen) == known_.recipes.size()) {
            // A SELECTION THAT NO LONGER NAMES ANYTHING IS NOT THE WEAVER'S ANY MORE: the
            // recipe their pick named is gone. The frontier action must not read what is
            // left as an explicit choice.
            state_.chosen.clear();
        }
        if (!picked_.empty() && named_row(picked_) == known_.recipes.size()) {
            // ...AND A PICK GOES WITH ITS RECIPE, so the name coming back in a later catalog is
            // not a pick the weaver made of it.
            picked_.clear();
        }
        say(mail);
    }

    // ---- What the host answers ------------------------------------------------------

    /// The answer this pane asked for must not erase what it just said: every gesture writes a
    /// notice, says its rows and asks the frontier again, and the answer arrives on the same
    /// drain, so an unconditional re-say would publish a notice-less picture over the first. A
    /// paint answer is news only when the frontier moved; a build answer is the gesture's own
    /// decision, made whether or not the picture moved. Each is matched to its own record.
    void on(const ProjectFrontierSaid& said, loom::Mail& mail) {
        const std::optional<BuilderAsk> asked = asks().answered(mail, [](const BuilderAsk& a) {
            return a.act == Act::kFrontierAsk || a.act == Act::kPaint;
        });
        if (!asked) {
            return;
        }
        const bool build = asked->act == Act::kFrontierAsk;
        const bool moved = !frontier_known_ || waiting_ != said.waiting ||
                           frontier_artifact_ != said.artifact ||
                           frontier_blocked_ != said.blocked;
        frontier_known_ = true;
        waiting_ = said.waiting;
        frontier_artifact_ = said.artifact;
        frontier_blocked_ = said.blocked;
        if (build) {
            finish_frontier_build(mail);
            return;
        }
        if (moved) {
            say(mail);
        }
    }

    void on(const PlanNames& said, loom::Mail& mail) {
        const std::optional<BuilderAsk> names = asks().answered(mail, of_kind(Act::kNames));
        if (!names) {
            return;
        }
        if (said.named) {
            notice_ = "`" + said.stem + "` is already in this project's plan -- " +
                      "build-and-load it instead";
            say(mail);
            return;
        }
        open_role(said.stem, names->subject);
        declare(mail);
        say(mail);
        // THE LINE IS OPEN: if a menu row began this, the keys it left behind are asked for
        // now, continuing THAT choice and no other (`chose`, `take_keys`).
        if (names->choice != 0) {
            take_keys(mail, names->choice);
        }
    }

    void on(const PlanRowWritten& said, loom::Mail& mail) {
        const std::optional<BuilderAsk> asked = asks().answered(mail, of_kind(Act::kPlanRow));
        if (!asked) {
            return;
        }
        const BuilderAsk& row = *asked;
        const std::string written = said.path.empty() ? std::string() : "; written to " + said.path;
        if (!said.accepted) {
            notice_ = "not loaded: " + said.refusal;
        } else if (!said.product.empty()) {
            // THE INTENT FINISHES: the row is the frontier and its product is already built,
            // so this is the button's own act -- the finished build's recipe asked for again
            // with the second intention aboard, to the same office, under the same grant.
            // No new route: the tool confirms the build, offers, and the owner decides.
            // Workshop approved the commit as a build and a write before the row was asked
            // for, and the row's ask carries that approval here: this send asks nothing more.
            if (send_build(mail, row.subject, /*realize=*/true)) {
                notice_ = "loading `" + row.stem + "` now -- loaded as " + row.role + ", " +
                          said.detail + "; Workshop stays live while the incremental build "
                          "confirms it" + written;
            } else {
                notice_ += "; the row was written as " + row.role + written;
            }
        } else {
            notice_ = "loaded `" + row.stem + "` as " + row.role + " -- " + said.detail +
                      (said.frontier ? "; nothing is built yet -- the frontier action builds "
                                       "and loads it"
                                     : std::string()) +
                      written;
        }
        // THE PLAN MOVED, SO WHAT THE PROJECT IS WAITING ON MAY HAVE MOVED WITH IT.
        ask_frontier(mail, Act::kPaint);
        say(mail);
    }

    /// The host's answer to "which file does this recipe name": the first of the two doors `e`
    /// walks. A refusal is the owner's own words; an accepted answer carries one absolute path,
    /// spent at the opening office once Workshop approves it, and held no longer. It is read
    /// against the recipe it asked about, which the notice names, even if the choice has moved.
    void on(const RecipeSourceSaid& said, loom::Mail& mail) {
        const std::optional<BuilderAsk> source = asks().answered(mail, of_kind(Act::kSource));
        if (!source) {
            return;
        }
        const std::string recipe = source->subject;
        const std::uint64_t gesture = source->gesture;
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
            return;
        }
        // WHAT OPENS IS KNOWN ONLY NOW, so here `e` asks Workshop its one question -- class
        // `open`, about this path, under the gesture that began it -- and the open is sent on
        // Workshop's word (`open_approved`). The only act whose ask lands at its second beat.
        BuilderAsk a;
        a.act = Act::kOpen;
        a.subject = recipe;
        a.path = said.source;
        a.undone = "`" + recipe + "`: the source was not opened";
        ask_approval(mail, std::move(a), gesture);
    }

    /// WORKSHOP'S WORD ON WHETHER THIS GESTURE'S ACTOR MAY DO THE ACT ASKED ABOUT -- Loom's
    /// answer to one of this pane's own asks, matched by `AskFacet`. Refused, nothing of the act
    /// happens and Workshop's reason is said in the pane's row; allowed, the act captured at the
    /// gesture is done (`carry_out`).
    void on_approval(const BuilderAsk& a, const ws::PaneOperationAnswered& answer,
                     loom::Mail& mail) {
        if (!answer.allowed) {
            say_undone(a, answer.reason, mail);
            return;
        }
        carry_out(a, mail);
    }

    /// The bus's word that one of this pane's attempts was refused before any handler ran
    /// (WL-OPEN-07), matched by `AskFacet` to the exact ask it ended -- `e`'s lookup, `o`'s plan
    /// names and the frontier action's ask at the project office, `e`'s open, the role line's
    /// row, the build, the reader's page and each ask for Workshop's word on a gesture -- which
    /// is named. A paint ask's refusal is no act's and says nothing; delivered silence is not a
    /// refusal.
    void on_refused(const BuilderAsk& a, const loom::DispatchRefused& refused, loom::Mail& mail) {
        const std::string why = " (" + refused.reason + ")";
        switch (a.act) {
        case Act::kPaint: return;
        case Act::kSource:
            notice_ = "`" + a.subject +
                      "`: the source could not be looked up -- it could not reach " +
                      ws::kProjectRole + why;
            break;
        case Act::kOpening:
            notice_ = "`" + a.subject +
                      "`: the source was not opened -- the open could not reach " +
                      ws::kOpeningRole + why;
            break;
        case Act::kNames:
            notice_ = "`" + a.subject +
                      "`: nothing was added to the plan -- the ask could not reach " +
                      ws::kProjectRole + why;
            break;
        case Act::kFrontierAsk:
            notice_ = std::string("nothing was asked for -- the ask could not reach ") +
                      ws::kProjectRole + why;
            break;
        case Act::kPlanRow:
            notice_ = "`" + a.stem +
                      "`: nothing was loaded and nothing was written -- the row could not reach " +
                      ws::kProjectRole + why;
            break;
        case Act::kBuildSent:
            notice_ = not_built(a.subject, awaiting_realization_) + " -- the ask could not reach " +
                      builder::kBuilderRole + why;
            awaiting_ = false;
            awaiting_realization_ = false;
            break;
        case Act::kPage:
            notice_ = "output #" + std::to_string(output_.op) +
                      ": the page was not read -- the ask could not reach " + builder::kBuilderRole +
                      why;
            break;
        default:
            // AN ASK WORKSHOP NEVER HEARD IS AN ACT NOT DONE: nothing of it happens, said in words.
            say_undone(a, std::string("the ask could not reach ") + kWorkshopRole + why, mail);
            return;
        }
        say(mail);
    }

    /// The opening office's answer -- the second door. An accepted open says nothing here: the
    /// document and its pane are shown together, and that is what a weaver reads. A refusal (a
    /// missing file, bytes the editor cannot carry, a dirty buffer) belongs beside its row.
    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!asks().answered(mail, of_kind(Act::kOpening))) {
            return;
        }
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
        }
    }

    /// A weaver reached the source behind a pane and it is open: Workshop's reading, published once
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
        // THE ROLE LINE KEEPS ITS OWN ROW: a weaver mid-way through typing a role still sees the
        // line; the choice has moved underneath it and the recipe row says so when it closes.
        if (!role_.open) {
            notice_ = "build recipe: " + said.recipe + " -> " + said.artifact + " -- the source of " +
                      said.name + " is open in the Editor; save it, then build (load after build: " +
                      (state_.arm ? "on" : "off") + ")";
        }
        say(mail);
    }

    // ---- The mouse: a press names a picture, a right press offers the pane's own rows ----


    void press(const ws::RowPress& at, loom::Mail& mail) {
        const std::int64_t column = at.column;
        const bool keys_went_here = at.keys_went_here;
        const BuilderMeaning* m = room().map().at(at.row, column);
        if (m == nullptr || m->kind == builder_row::kNone) {
            return; // a fact row, a heading, a build's own words: pointing, and nothing more
        }
        room().spend_notice(
            notice_,
            [&] {
                if (m->kind == builder_row::kControl) {
                    perform_on(m->id, m->subject, m->op, mail);
                } else if (m->kind == builder_row::kRecipe && choosing_.open) {
                    // THE LIST'S OWN SECOND PRESS: the first names the row, the second makes it
                    // the weaver's pick and closes the list -- Files' rule, so no press means two
                    // things. And only where the keys already were (WL-FOCUS-04): the press that
                    // brings them here points at the pane, and pointing is not choosing.
                    if (keys_went_here && m->subject == choosing_.name) {
                        take_choice(mail);
                    } else {
                        choosing_.name = m->subject;
                        say(mail);
                    }
                } else if (m->kind == builder_row::kRecipe && keys_went_here) {
                    // THE ROW THAT NAMES THE CHOICE IS A WAY INTO THE LIST -- for a weaver whose
                    // keys are already here. A press that merely brings them points at the pane
                    // and opens no mode, which is what keeps clicking a pane to focus it from
                    // doing anything.
                    open_recipes(mail);
                } else if (m->kind == builder_row::kLine && role_.open) {
                    // A PRESS IS READ IN WHAT THE ROW DRAWS, as the caret is: the pressed column
                    // counts the spelled characters before it, never the typed bytes.
                    const std::string prompt = active_role_prompt();
                    const std::string shown = role_.line.visible(role_value_columns(prompt));
                    role_.line.place(role_.line.position_at_column(spelled_column_byte(
                        shown, column - static_cast<std::int64_t>(prompt.size()))));
                    say(mail);
                }
            },
            [&] { say(mail); });
    }

    /// THE SECOND BUTTON. A right press on a row this pane owns OFFERS that row's menu, beside
    /// the press, continuing it; a right press on anything else is handed back to the host,
    /// whose own pane menu answers (WL-CTX-08). A middle press means nothing here and is
    /// consumed.
    bool right_press(const BuilderMeaning& m, const ws::RowPress& at, loom::Mail& mail) {
        if (m.kind == builder_row::kNone) {
            return false;
        }
        notice_.clear();
        if (m.kind == builder_row::kRecipe && choosing_.open) {
            choosing_.name = m.subject;
        }
        offer_menu(at.row, at.column, mail.correlation(), mail);
        say(mail);
        return true;
    }

    /// WHAT A MENU CAME TO -- one of THIS image's own asks, settled by `PaneMenuFacet`: a choice
    /// counts only from the presenter's office, under the number of an ask this image sent and
    /// has not heard answered, about the pane and subject it asked about, once. What the row
    /// MEANS is judged here, against what this pane holds now -- the catalog may have been
    /// republished while the menu was open.
    void on_menu(const std::string& id, const std::string& subject, loom::Mail& mail) {
        chose(id, subject, mail);
    }

    // ---- The recipe list (the pane's own mode) -------------------------------------------

    // Defined in `modes.hpp`.
    void open_recipes(loom::Mail& mail);
    void close_recipes(loom::Mail& mail, std::string said);
    std::size_t list_row() const;
    void choose_in_list(const std::string& id, loom::Mail& mail);
    void take_choice(loom::Mail& mail);

    /// ASK THE HOST FOR THE KEYBOARD, CONTINUING A MENU CHOICE BY ITS NUMBER. Spent only where
    /// a chosen row actually opened a line to type into: a right press by itself stays
    /// focus-neutral (WL-CTX-08), and the host refuses a grab that is no longer the weaver's
    /// latest act.
    void take_keys(loom::Mail& mail, std::uint64_t correlation) {
        if (correlation == 0) {
            return;
        }
        (void)pane_menu::take_keyboard_continuing(mail, pane::kBuilderPaneRole,
                                                  pane::kBuilderPane, correlation);
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
    /// A weaver who built `rocket` and then picked `probe` out of the list is still owed
    /// `rocket` by this control, because `rocket` is what is standing there built.
    void load_built(loom::Mail& mail) {
        if (!ready_to_load()) {
            notice_ = state_.arm ? "load after build is already on -- the next build is offered"
                                 : "nothing built is waiting to be loaded";
            say(mail);
            return;
        }
        ask_load_built(mail);
    }

    /// FLIP THE STANDING INTENT AND SEND NOTHING. Refused while an artifact is standing built
    /// and unoffered, because there the weaver's own control says `load built ...` and arming
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

    /// WHAT ONE OFFERED ROW PROMISED: the row's id, the subject its label named out loud (empty
    /// where the label named none), and the operation that subject was true of when the row was
    /// written (0 where none applies) -- the second half of the same promise, checked but never
    /// shown (`perform_on`).
    struct Offered {
        std::string id;
        std::string advertised;
        std::int64_t op = 0;
    };

    // Defined in `view.hpp`.
    void offer_here(loom::Mail& mail, std::uint64_t correlation);
    void offer_menu(std::int64_t row, std::int64_t column, std::uint64_t correlation,
                    loom::Mail& mail);
    Offered offered_as(const std::string& id) const;
    std::string menu_subject() const;
    void chose(const std::string& id, const std::string& subject, loom::Mail& mail);

    // ---- The clipboard the role line spends -----------------------------------------

    void on(const gfx::ClipboardCopy& said, loom::Mail&) {
        if (admissible(said.text)) {
            clip_.text = said.text;
        }
    }

    void on(const gfx::ClipboardText& a, loom::Mail& mail) {
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

    /// What this pane answers to right now, re-declared whenever the mode changes. A pane is one
    /// keyboard context and a mode is not a second one (WL-FILES-16): while a weaver types a role,
    /// the pane declares the line's own rows and every other key reaches the line as a
    /// `PaneKey`, so Backspace deletes a character. `PaneActions` is a replacement (WL-KEY-15),
    /// and an id never moves: a weaver's override applies wherever the id is in force.
    void declare(loom::Mail& mail) {
        const std::string in = mode();
        this->template facet<Actions>().declare(
            mail, rows_of(in), in,
            ws::causes_elsewhere(
                {pane::kModeBrowse, pane::kModeRecipes, pane::kModeOutput, pane::kModeRole}, in,
                [](const std::string& m) { return rows_of(m); }));
    }

    /// THE MODE THIS PANE IS IN: the role line, the output reader, the recipe list, or browsing.
    const char* mode() const {
        return role_.open      ? pane::kModeRole
               : output_.open  ? pane::kModeOutput
               : choosing_.open ? pane::kModeRecipes
                                : pane::kModeBrowse;
    }

    /// THE ROWS OF THE MODE THIS PANE IS IN -- what `declare` tells Workshop, and what `answers`
    /// reads, so what the pane acts on and what it said it acts on are one list.
    std::vector<PaneActionRow> action_rows() const { return rows_of(mode()); }

    /// THE ROWS OF ONE MODE, each with what its press may send beside the pane protocol and the
    /// mode it leaves the pane in, read from `perform` and the asks it spends: a send that waits on
    /// an answer is still this press's (the role line's commit writes the row, then builds and asks
    /// the frontier again); one that waits on another gesture, a menu's choice, is not.
    static std::vector<PaneActionRow> rows_of(const std::string& in) {
        using builder::BuildOutputRequested;
        using builder::BuildRequested;
        std::vector<PaneActionRow> rows;
        const auto row = [&rows](const char* id, const char* label, std::int64_t sc,
                                 std::int64_t mods = input::mod::kNone,
                                 std::vector<ws::ActionCause> causes = {},
                                 const char* enters = "") {
            rows.push_back(PaneActionRow{id, label, sc, mods, {}, std::move(causes), enters});
        };
        const std::vector<ws::ActionCause> build{ws::cause_of<BuildRequested>()};
        const std::vector<ws::ActionCause> page{ws::cause_of<BuildOutputRequested>()};
        const std::vector<ws::ActionCause> source{ws::cause_of<RecipeSourceRequested>(),
                                                  ws::cause_of<OpenSourceRequested>()};
        if (in == pane::kModeRole) {
            row(pane::kActionCommit, "load it", input::scan::kReturn, input::mod::kNone,
                {ws::cause_of<PlanRowRequested>(), ws::cause_of<BuildRequested>(),
                 ws::cause_of<ProjectFrontierRequested>()},
                pane::kModeBrowse);
            // And the menu declares no default key while the role line is open: Workshop resolves
            // a key transition before the character it produced arrives, so `Shift+M` would open
            // the menu and lose the `M` of `Main`. The `[menu]` control, first in every strip, and
            // the second button stay the route; a weaver who wants a key names `builder.menu`.
            row(pane::kActionMenu, "this pane's menu", input::scan::kUnknown);
            row(pane::kActionCancel, "cancel", input::scan::kEscape, input::mod::kNone, {},
                pane::kModeBrowse);
            return rows;
        }
        if (in == pane::kModeOutput) {
            row(pane::kActionOutputUp, "line up", input::scan::kUp, input::mod::kNone, page);
            row(pane::kActionOutputDown, "line down", input::scan::kDown, input::mod::kNone, page);
            row(pane::kActionOutputFirst, "first line", input::scan::kHome, input::mod::kNone,
                page);
            row(pane::kActionOutputLast, "last lines", input::scan::kEnd, input::mod::kNone, page);
            row(pane::kActionOutputLeft, "pan left", input::scan::kLeft);
            row(pane::kActionOutputRight, "pan right", input::scan::kRight);
            row(pane::kActionOutputOlder, "older build", input::scan::kLeftBracket,
                input::mod::kNone, page);
            row(pane::kActionOutputNewer, "newer build", input::scan::kRightBracket,
                input::mod::kNone, page);
            row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
            row(pane::kActionOutputClose, "close output", input::scan::kEscape, input::mod::kNone,
                {}, pane::kModeBrowse);
            return rows;
        }
        // ---- The recipe list: the two arrows, the take, the menu and the way out ---------
        if (in == pane::kModeRecipes) {
            row(pane::kActionRecipesUp, "row up", input::scan::kUp);
            row(pane::kActionRecipesDown, "row down", input::scan::kDown);
            row(pane::kActionRecipeChoose, "choose this recipe", input::scan::kReturn,
                input::mod::kNone, {}, pane::kModeBrowse);
            // THE SAME ID THE BROWSING MODE BINDS TO THE SAME KEY, because it is the same
            // operation: open the source of the recipe under the cursor. Which recipe THAT is
            // is the mode's answer, not the id's (`edit_source`).
            row(pane::kActionEditSource, "edit source", input::scan::kE, input::mod::kNone, source);
            row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
            row(pane::kActionRecipesClose, "close the list", input::scan::kEscape,
                input::mod::kNone, {}, pane::kModeBrowse);
            return rows;
        }
        // ---- Browsing: the ids and default keys a weaver's keymap already names ----------
        row(pane::kActionBuild, "build", input::scan::kB, input::mod::kNone, build);
        row(pane::kActionBuildRealize, "load after build", input::scan::kB, input::mod::kShift,
            build);
        row(pane::kActionPromote, "promote image", input::scan::kP, input::mod::kShift,
            {ws::cause_of<builder::PromoteArtifact>()});
        row(pane::kActionRevert, "revert image", input::scan::kR, input::mod::kShift,
            {ws::cause_of<builder::RevertArtifact>()});
        row(pane::kActionLoadIt, "load it", input::scan::kO, input::mod::kNone,
            {ws::cause_of<PlanNamesRequested>()}, pane::kModeRole);
        row(pane::kActionRecipeNext, "recipe", input::scan::kC);
        row(pane::kActionRecipeBack, "recipe back", input::scan::kC, input::mod::kShift);
        row(pane::kActionFrontier, "frontier", input::scan::kF, input::mod::kNone,
            {ws::cause_of<ProjectFrontierRequested>(), ws::cause_of<BuildRequested>()});
        row(pane::kActionEditSource, "edit source", input::scan::kE, input::mod::kNone, source);
        row(pane::kActionOutput, "read output", input::scan::kL, input::mod::kNone, page,
            pane::kModeOutput);
        // ---- And the rows that came with the controls: Return, unclaimed while browsing,
        // opens the list, since a pane whose whole subject is one choice should open it there.
        row(pane::kActionRecipes, "choose a recipe...", input::scan::kReturn, input::mod::kNone,
            {}, pane::kModeRecipes);
        row(pane::kActionMenu, "this pane's menu", input::scan::kM, input::mod::kShift);
        // THE TWO HALVES OF `builder.build-realize`, each reachable on its own terms and
        // neither bound by default (WL-KEY-13): the shipped key keeps both meanings, and a
        // weaver who wants one of them alone names its id.
        row(pane::kActionArm, "turn load-after-build on or off", input::scan::kUnknown);
        row(pane::kActionLoadBuilt, "load what was built", input::scan::kUnknown,
            input::mod::kNone, build);
        return rows;
    }

    // ---- The asks -------------------------------------------------------------------

    void ask_status(loom::Mail& mail) {
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(builder::kBuilderRole, builder::StatusRequested{});
    }

    /// ASK WHAT THE PROJECT IS WAITING ON, as `kind`'s own ask. One shape answers two gestures
    /// -- the row this pane paints, and the one action whose whole decision is the answer -- so
    /// each keeps its own record: a paint ask (every grant, every settled build, every written
    /// row asks one) never replaces the action's, nor is either answer read as the other.
    Asks::Asked ask_frontier(loom::Mail& mail, Act kind) {
        (void)asks().forget(of_kind(kind));
        return asks().ask(mail, ws::kProjectRole, ProjectFrontierRequested{}, asking(kind));
    }

    // ---- Workshop's word on a gesture ------------------------------------------------

    /// THE ACTION CLASSES EACH ACT IS (`workshop/actor_scope.hpp`): every build, load, promote and
    /// revert is `build`; the role line's commit writes a plan row and builds, so it is both, in
    /// one ask; `e` opens a file. Asked at the act's first beat, never per action id -- `e` at its
    /// second, the beat that learns the path, under its first beat's gesture.
    static std::vector<std::string> classes_of(Act kind) {
        if (kind == Act::kRow) {
            return {ws::scope::kBuild, ws::scope::kWrite};
        }
        if (kind == Act::kOpen) {
            return {ws::scope::kOpen};
        }
        return {ws::scope::kBuild};
    }

    /// ASK WORKSHOP WHETHER THIS GESTURE'S ACTOR MAY DO THE ACT, and hold the act until it
    /// answers. The ask names the act's classes and no send: the pane sends the act as its own
    /// office, so the actor is judged by the classes alone. `gesture` is the delivery that
    /// brought the act -- a key's action, a press, a menu choice -- and this one ask spends it,
    /// so an act of several beats asks once and carries the approval to its later sends.
    void ask_approval(loom::Mail& mail, BuilderAsk a, std::uint64_t gesture) {
        // A FULL BOOK REFUSES THE NEWEST ACT IN WORDS and drops none it holds: an answer Workshop
        // owes an act already asked about still finds it.
        const BuilderAsk act = a;
        switch (asks().approve(mail, std::move(a), gesture, classes_of(act.act), act.path)) {
        case Asks::Asked::kHeld: return;
        case Asks::Asked::kFull:
            say_undone(act,
                       std::to_string(Asks::kApprovals) + " acts are already waiting on Workshop",
                       mail);
            return;
        case Asks::Asked::kUnqueued:
            say_undone(act, std::string("nothing was queued to ") + kWorkshopRole, mail);
            return;
        }
    }

    /// AN ACT WORKSHOP REFUSED, OR NEVER HEARD, DID NOT HAPPEN AT ALL: nothing was sent and
    /// nothing the pane holds moved, and the row says what did not happen, then why.
    void say_undone(const BuilderAsk& a, const std::string& why, loom::Mail& mail) {
        notice_ = a.undone + " -- " + why;
        say(mail);
    }

    /// WORKSHOP SAID YES: the act captured at the gesture, done now. Each is judged once more
    /// against what the pane holds, since an answer lands after any number of other deliveries,
    /// and one that no longer makes sense is refused in words and sends nothing.
    void carry_out(const BuilderAsk& a, loom::Mail& mail) {
        switch (a.act) {
        case Act::kBuild:
            if (named_row(a.subject) == known_.recipes.size()) {
                say_undone(a, "it is not in the recipes this pane last heard", mail);
                return;
            }
            build_sent(mail, a.subject, a.realize);
            return;
        case Act::kLoadBuilt:
            if (!ready_to_load() || shown_.recipe != a.subject || shown_.op != a.op) {
                say_undone(a, "it is not what is built and waiting now", mail);
                return;
            }
            if (send_build(mail, a.subject, /*realize=*/true)) {
                notice_ = "loading the built `" + a.artifact +
                          "` now -- Workshop stays live while the incremental build confirms it";
            }
            say(mail);
            return;
        case Act::kPromote:
        case Act::kRevert:
            if (!standing() || shown_.artifact != a.artifact || shown_.op != a.op) {
                say_undone(a, "it is not what is standing now", mail);
                return;
            }
            if (a.act == Act::kPromote) {
                promote_sent(mail, a.artifact);
            } else {
                revert_sent(mail, a.artifact);
            }
            return;
        case Act::kFrontier:
            // THE FIRST BEAT'S APPROVAL, CARRIED: the frontier is asked into the action's own
            // record, and its answer builds without asking again (`finish_frontier_build`).
            if (ask_frontier(mail, Act::kFrontierAsk) != Asks::Asked::kHeld) {
                say_undone(a, std::string("nothing was queued to ") + ws::kProjectRole, mail);
            }
            return;
        case Act::kRow:
            commit_approved(a, mail);
            return;
        case Act::kOpen:
            open_approved(a, mail);
            return;
        default:
            return;
        }
    }

    /// THE BUILD, ASKED OF THE TOOL AS THIS PANE'S OFFICE. It asks Workshop nothing: every caller
    /// already holds its act's approval -- a one-beat act's on Workshop's word, the frontier
    /// action's and the role line's carried from their first beat. Its ticket is kept for Loom's
    /// word that exactly this send was refused (WL-OPEN-07); a send that queued nothing arms
    /// nothing, says so in the notice, and answers false.
    bool send_build(loom::Mail& mail, const std::string& recipe, bool realize) {
        (void)asks().forget(of_kind(Act::kBuildSent));
        BuilderAsk sent = asking(Act::kBuildSent);
        sent.subject = recipe;
        const Asks::Asked held = asks().ask_with(
            builder::kBuilderRole, builder::BuildRequested::zen_name,
            builder::BuildRequested::zen_version, std::move(sent), [&](std::uint64_t) {
                return mail.as_role(pane::kBuilderPaneRole)
                    .send_to_role(builder::kBuilderRole, builder::BuildRequested{recipe, realize});
            });
        if (held != Asks::Asked::kHeld) {
            notice_ = not_built(recipe, realize) + " -- nothing was queued to " +
                      builder::kBuilderRole;
            return false;
        }
        awaiting_ = true;
        awaiting_realization_ = realize;
        loading_op_ = 0;
        return true;
    }

    /// What a build send that never reached the tool did not do.
    static std::string not_built(const std::string& recipe, bool realize) {
        return "`" + recipe + "` was not built" + (realize ? " or loaded" : "");
    }

    // ---- The catalog and the weaver's choice -----------------------------------------

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

    /// The row the weaver is on: their explicit pick where it still names something, and the
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

    /// THE CHOSEN ROW, BUILT: the recipe the weaver chose and the realize intention are captured
    /// now and asked of Workshop as class `build`; the build is sent on its word (`build_sent`).
    void build_now(loom::Mail& mail, bool realize) {
        if (!has_recipe("nothing was asked for")) {
            say(mail);
            return;
        }
        BuilderAsk a;
        a.act = Act::kBuild;
        a.subject = known_.recipes[cursor_row()].recipe;
        a.realize = realize;
        a.undone = "`" + a.subject + "` was not built";
        ask_approval(mail, std::move(a), mail.correlation());
    }

    /// THE BUILD SENT, AND SAID: where `b` ends on Workshop's word, and where the frontier action
    /// ends with the approval its first beat carried. It asks nothing itself.
    void build_sent(loom::Mail& mail, const std::string& recipe, bool realize) {
        if (!send_build(mail, recipe, realize)) {
            say(mail);
            return;
        }
        notice_ = "asked the Builder for `" + recipe + "`" +
                  (realize ? " and to realize it" : std::string()) +
                  " -- Workshop stays live while it builds";
        say(mail);
    }

    /// WHAT IS BUILT AND WAITING, ASKED FOR AS CLASS `build`: the finished build's recipe, its
    /// artifact and its operation are captured now, and the load is sent on Workshop's word.
    void ask_load_built(loom::Mail& mail) {
        BuilderAsk a;
        a.act = Act::kLoadBuilt;
        a.subject = shown_.recipe;
        a.artifact = shown_.artifact;
        a.op = shown_.op;
        a.undone = "`" + shown_.artifact + "` was not loaded";
        ask_approval(mail, std::move(a), mail.correlation());
    }

    /// One action in two states. The button: an artifact is built and ready to load, nothing is
    /// armed and no build is in flight, so it asks Workshop, then sends the finished build's own
    /// ask again with the second intention aboard (`shown_.recipe`, never the cursor's row). The
    /// toggle, everywhere else: it flips the weaver's standing intent, and sends and asks nothing.
    void build_realize(loom::Mail& mail) {
        const builder::BuildStatus& s = shown_;
        const bool ready = heard_ && !awaiting_ && !state_.arm && !s.recipe.empty() &&
                           s.outcome == builder::outcome::kSucceeded &&
                           s.realization == builder::realization::kNotAsked;
        if (ready) {
            ask_load_built(mail);
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
        ask_image(mail, Act::kPromote, "promoted");
    }

    void revert_image(loom::Mail& mail) {
        if (!standing()) {
            notice_ = "nothing this Builder realized is standing -- nothing to revert";
            say(mail);
            return;
        }
        ask_image(mail, Act::kRevert, "reverted");
    }

    /// THE STANDING IMAGE, ASKED FOR AS CLASS `build`: a promote writes the file a restart loads
    /// and a revert loads the previous image. The artifact and the operation that built it are
    /// captured now, and the offer is said on Workshop's word.
    void ask_image(loom::Mail& mail, Act kind, const char* done) {
        BuilderAsk a;
        a.act = kind;
        a.artifact = shown_.artifact;
        a.op = shown_.op;
        a.undone = "`" + shown_.artifact + "` was not " + done;
        ask_approval(mail, std::move(a), mail.correlation());
    }

    void promote_sent(loom::Mail& mail, const std::string& artifact) {
        // ONE OFFER, DECIDED ELSEWHERE. Whether the artifact is live, whether it runs from a
        // per-operation copy, and whether the write is possible are the realization owner's
        // and the host's; this pane says one sentence and shows the answer.
        (void)mail.publish(builder::PromoteArtifact{artifact});
        notice_ = "asked to promote `" + artifact +
                  "` -- the file a restart loads takes the running image";
        say(mail);
    }

    void revert_sent(loom::Mail& mail, const std::string& artifact) {
        (void)mail.publish(builder::RevertArtifact{artifact});
        // ...AND WHAT A REVERT DOES NOT TOUCH, said at the gesture and inside one row: the source
        // a weaver saved is still the edited one, and the next build builds it. A running image and
        // a saved file are two facts, and a weaver reading only the pane would take one for the other.
        notice_ = "asked to revert `" + artifact +
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
        // THE ONE WRITER OF `picked`: this gesture is what makes a selection the WEAVER's
        // rather than the catalog's order wearing a name. The frontier action reads it when
        // several recipes produce one artifact -- and it records WHICH recipe was picked, so a
        // choice another gesture later moves elsewhere carries no pick with it.
        picked_ = state_.chosen;
        notice_ = "build recipe: " + known_.recipes[to].recipe + " -> " +
                  known_.recipes[to].artifact;
        say(mail);
    }

    /// The frontier build, in two beats: the gesture asks Workshop whether its actor may build,
    /// then, on its word, asks for the owner's frontier and decides when that answer lands. The
    /// approval is the first beat's, carried in the action's own frontier record: the second
    /// beat's send asks nothing, and a key typed between the beats does not undo it.
    void begin_frontier_build(loom::Mail& mail) {
        if (!heard_) {
            notice_ = "the Builder has not said what it builds yet -- nothing was asked for";
            say(mail);
            return;
        }
        BuilderAsk a;
        a.act = Act::kFrontier;
        a.undone = "nothing was asked for";
        ask_approval(mail, std::move(a), mail.correlation());
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
            // A STANDING PICK IS THE WEAVER'S PICK OF THE RECIPE STILL CHOSEN. Edit Code following
            // a pane's source and this gesture taking a lone producer both move the choice without
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
        // about, and `build_sent`'s notice says it again, so the pane never shows one recipe
        // while the gesture sends another. The approval came at the first beat: no second ask.
        state_.chosen = known_.recipes[match].recipe;
        build_sent(mail, state_.chosen, /*realize=*/true);
    }

    // Load it and the role line it opens, defined in `modes.hpp`.
    void begin_load_it(loom::Mail& mail);
    void open_role(const std::string& stem, const std::string& recipe);
    void close_role();
    void commit_role(loom::Mail& mail);
    void commit_approved(const BuilderAsk& a, loom::Mail& mail);

    /// Edit the source the chosen recipe names: two doors, walked in order. The pane holds a
    /// recipe's name and never its procedure, so it asks the project office which one file that
    /// name means (`RecipeSourceRequested` -> `RecipeSourceSaid`), then asks the opening office
    /// to open it (`OpenSourceRequested` -> `SourceOpened`), once Workshop approves the open for
    /// the gesture's actor. Every refusal comes back as its owner's own sentence and lands in
    /// this pane's row.
    void edit_source(loom::Mail& mail) {
        if (!has_recipe("nothing was opened")) {
            say(mail);
            return;
        }
        // The first door, its ticket kept (WL-OPEN-07). Which recipe depends on the mode: while
        // the list is open the row the weaver named is what they asked to edit, and the committed
        // choice is untouched (WL-PROJ-14); elsewhere the choice is the only cursor.
        const std::size_t at = choosing_.open ? list_row() : cursor_row();
        if (at >= known_.recipes.size()) {
            notice_ = "no recipe is under the cursor -- nothing was opened";
            say(mail);
            return;
        }
        const std::string recipe = known_.recipes[at].recipe;
        (void)asks().forget(of_kind(Act::kSource));
        BuilderAsk source = asking(Act::kSource);
        source.subject = recipe;
        source.gesture = mail.correlation();
        if (asks().ask(mail, ws::kProjectRole, RecipeSourceRequested{recipe}, std::move(source)) !=
            Asks::Asked::kHeld) {
            notice_ = "`" + recipe + "`: the source could not be looked up -- nothing was queued "
                      "to " + ws::kProjectRole;
            say(mail);
        }
    }

    /// THE SECOND DOOR, APPROVED: the opening office, which arranges the document and the desk
    /// together (WL-OPEN-01). The ticket is kept for the bus's later word that exactly this
    /// attempt was refused (WL-OPEN-07); nothing queued is refused now, in words.
    void open_approved(const BuilderAsk& a, loom::Mail& mail) {
        (void)asks().forget(of_kind(Act::kOpening));
        BuilderAsk opening = asking(Act::kOpening);
        opening.subject = a.subject;
        if (asks().ask(mail, ws::kOpeningRole, OpenSourceRequested{a.path}, std::move(opening)) !=
            Asks::Asked::kHeld) {
            notice_ = "`" + a.subject + "`: the source was not opened -- nothing was queued to "
                      "the opening office";
            say(mail);
        }
    }

    /// A paste is asked of the window that shows the pane's area, else the Skin.
    void begin_paste(loom::Mail& mail) {
        const std::string office = room().clipboard_office();
        const loom::AskOpened opened = clip_asks_.open_to_role(
            office, gfx::ClipboardTextRequested::zen_name,
            gfx::ClipboardTextRequested::zen_version);
        if (!opened) {
            return;
        }
        paste_ask_ = opened.id;
        paste_epoch_ = role_.line.draft_epoch();
        pasting_ = true;
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(office, gfx::ClipboardTextRequested{}, opened.correlation);
    }

    // ---- The output reader (WL-OUT-04), defined in `output_reader.hpp` ----------------------

    void open_output(loom::Mail& mail);
    void bind_output(std::int64_t op, loom::Mail& mail);
    void close_output(loom::Mail& mail);
    void read_output(const std::string& id, loom::Mail& mail);
    void scroll(std::int64_t by, loom::Mail& mail);
    void ask_page(loom::Mail& mail);
    std::int64_t output_body_rows() const;
    void say_output();

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

    /// One row's text as this pane draws it: spelled in what a canvas draws (WL-OUT-03), since a
    /// row may carry another owner's words, and one byte Workshop's canvas cannot draw refuses
    /// the pane's whole picture (`canvas_content_problem`).
    std::string fitted(const std::string& text) const {
        return fit(ascii_spelling(text), columns());
    }
    /// ...as the facet's strip and window take it (`PaneRowsFacet::push_controls`).
    auto fit_row() const {
        return [this](const std::string& text) { return fitted(text); };
    }

    /// One row of the picture, with what it means recorded as it is written: a press is answered
    /// from the record the composition made, never from a second calculation.
    void push_row(const std::string& text, std::int64_t role,
                  BuilderMeaning meaning = BuilderMeaning{}) {
        std::optional<BuilderMeaning> means;
        if (meaning.kind != builder_row::kNone) {
            means = std::move(meaning);
        }
        (void)room().push(zengine::workshop::TextRow{fitted(text), role},
                          std::move(means)); // a row past the room names nothing
    }

    /// The whole picture. The notice leads and is composed first, since the row map records
    /// absolute rows (every mode already asks for one fewer row when a notice stands). It is
    /// cleared by the weaver's next act, not by being said: one gesture can publish several times
    /// in one drain, Workshop keeps the last picture, and a notice cleared by the first `say`
    /// would be one no weaver ever reads -- it is the answer to their last act.
    void say(loom::Mail& mail) {
        room().begin();
        if (!room().granted() || rows() <= 0 || columns() <= 0) {
            room().hold();
            return;
        }
        if (!notice_.empty() && rows() > 1) {
            push_row(notice_, gfx::role::kAccent);
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
        // The role line's caret beside the rows, said every time, so a caret that went goes.
        room().publish(
            mail, [](const BuilderMeaning& m) { return part_name(m); },
            [](const BuilderMeaning& m) { return m.available; });
    }

    /// WHAT THE BUILDER CALLS ITS PARTS: a recipe's row by the recipe, `recipe:<name>`; a control
    /// by its operation, `control:<id>`; and the role line, `line`.
    static std::string part_name(const BuilderMeaning& m) {
        switch (m.kind) {
        case builder_row::kRecipe: return "recipe:" + m.subject;
        case builder_row::kControl: return "control:" + m.id;
        case builder_row::kLine: return "line";
        default: return std::string();
        }
    }

    // ---- The controls a weaver can press ---------------------------------------------

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

    // The control strip and the fact view, defined in `view.hpp`.
    std::vector<ControlRow> builder_controls() const;
    void say_controls(const std::vector<ControlRow>& controls);
    template <class Facts>
    void publish(Facts facts, const std::string& said_detail,
                 const std::vector<ControlRow>& controls);
    void say_builder();

    // The recipe list's and the role line's, defined in `modes.hpp`.
    std::vector<ControlRow> list_controls() const;
    std::vector<ControlRow> role_controls() const;
    std::string role_prompt() const;
    std::string active_role_prompt() const;
    std::int64_t role_value_columns(const std::string& prompt) const;
    static std::int64_t spelled_column_byte(const std::string& shown, std::int64_t column);
    void say_role();
    void say_recipes();

    // The output reader's, defined in `output_reader.hpp`.
    std::vector<ControlRow> output_controls() const;

    // ---- What this pane is ------------------------------------------------------------

    Rows& room() { return this->template facet<Rows>(); }
    const Rows& room() const { return this->template facet<Rows>(); }
    /// This image's one outstanding menu (`PaneMenuFacet`).
    Menu& menu() { return this->template facet<Menu>(); }
    /// The rows and columns the pane composes for (`PaneFacet`).
    std::int64_t rows() const { return room().rows(); }
    std::int64_t columns() const { return room().columns(); }
    Asks& asks() { return this->template facet<Asks>(); }
    const Asks& asks() const { return this->template facet<Asks>(); }

    /// THE TOOL'S PICTURE, HELD ONLY WHILE THIS PANE IS SHOWING IT (WL-PROJ-11, WL-PROJ-12).
    /// `heard_` tells "the tool has not answered" from "the tool never built anything";
    /// `awaiting_` is the only fact here that is genuinely this pane's own, and it is what
    /// decides whether an arriving status is news.
    bool heard_ = false;
    bool awaiting_ = false;
    bool awaiting_realization_ = false;
    /// The operation whose load `awaiting_realization_` waits on once its build has ended; 0
    /// while the build runs.
    std::int64_t loading_op_ = 0;
    builder::BuildStatus shown_{};
    builder::RecipeCatalog known_{};
    /// WHICH RECIPE THE WEAVER LAST PICKED WITH `c`, by name -- empty when they never have, or when
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

    /// NOTCHES NOT YET WORTH A WHOLE ROW -- one accumulator for the one list in force.
    double wheel_ = 0.0;
    /// WHAT THE ROWS OF THE MENU OFFERED LAST PROMISED, row by row (`offer_menu`). Replaced whole
    /// every time a menu is offered, and read once when its answer lands.
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
        builder::BuildOutputSaid page{};
    } output_;

    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace

// The output reader, the recipe list and the role line, and the fact view, control strip and
// pane menu: members of `BuilderPaneWeave` defined in parts of this file, which need the whole
// class before them.
#include "builder/builder-pane/modes.hpp"
#include "builder/builder-pane/output_reader.hpp"
#include "builder/builder-pane/view.hpp"

ZEN_EXPORT_WEAVE(BuilderPaneWeave)
