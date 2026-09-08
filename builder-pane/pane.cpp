// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Builder pane -- a loadable weave that offers Workshop one pane: the maker's seat at
// the build, at the realization frontier, and at the two acts a reload leaves behind.
//
// IT USED TO BE C++ INSIDE THE HOST (`workshop/panel.hpp`'s `BuilderPane`,
// `screen_pane_state.cpp`'s `paint_builder`, and nine `kActionCatalog` rows dispatched out
// of `weave_arrange.cpp`, `weave_recipes.cpp` and `weave_pane_editor.cpp`). Now it is a
// weave beside the Skin, the Timer and the Files browser, and everything it once read
// straight off the host's own context -- what realization is waiting on, whether the plan
// already names an artifact, the file a recipe was authored from -- it ASKS for, through the
// doors `workshop/builder_seam_vocabulary.hpp` spells. What crosses the seam is values.
//
// ⚠ AND ITS KEYS ARE ITS OWN NOW (VD-22). `b` was a command-mode row: it built from
// anywhere in Workshop, as long as a Builder panel happened to be open. A pane's rows are
// active only while the pane holds the keyboard, so a maker PRESSES INTO the Builder and
// then builds. Nothing does something by default from anywhere; buttons, hover-to-focus and
// a host-mapped route to a weave's action are later UX with many options, and none of them
// is a default now.
//
// THE COMPOSITION DID NOT MOVE ITS MEANING. The nine facts, their display order, their
// survival priorities and the three-row `said` block are `paint_builder`'s, carried here
// verbatim: what changed is that the rows are SAID as values into a room this pane is
// granted, instead of being written into a region this pane resolved for itself.

#include "builder-pane/vocabulary.hpp"

#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/pane_text.hpp"

#include "activation/activation.hpp"
#include "builder/vocabulary.hpp"
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

using ws::PaneActionRequested;
using ws::PaneActionRow;
using ws::PaneActions;
using ws::PaneCatalogRequested;
using ws::PaneContent;
using ws::PaneKey;
using ws::PaneOffered;
using ws::PaneRoom;
using ws::PaneTextInput;
using ws::PlanNames;
using ws::PlanNamesRequested;
using ws::PlanRowRequested;
using ws::PlanRowWritten;
using ws::ProjectFrontierRequested;
using ws::ProjectFrontierSaid;
using ws::RecipeSourceRequested;
using ws::SourceOpened;

/// The office Workshop holds, named as a STRING rather than reached through
/// `workshop/panel.hpp`: a provider is a stranger to Workshop's internals and says who it is
/// talking to the way a third party would.
constexpr const char* kWorkshopRole = "zengine.workshop";

// ---- The text helpers the composition spends ------------------------------------------
//
// `detail::pad`, `detail::fit`, `detail::wrap` and `detail::kElided` are Workshop's own
// (`screen_bindings.cpp`, `screen_gestures.cpp`) and they live behind `screen.hpp`, which is
// the host's presentation and not a header a loaded image may include. They are carried here
// byte-for-byte rather than approximated, because the composition below is a MOVE: a panel
// that cut its rows one character differently after the migration would be a panel a maker
// could see had changed, for no reason they were told about. `files.cpp` carries `fit` for
// exactly this reason, one pane over.

// ⚠ AND THEY ARE NOT COPIED HERE ANY MORE. `workshop/pane_text.hpp` holds the functions four
// packages each carried a copy of. Nothing about them changed for moving, with one measured
// exception written down in that header: `wrap` spends ONE space on a break rather than a run
// of them, which is what two of the four copies did and what this one did not.

using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::pad;
using zengine::workshop::pane_text::wrap;
constexpr std::int64_t kWrapIndent = zengine::workshop::pane_text::kWrapIndent;
constexpr const char* kElided = zengine::workshop::pane_text::kElided;


/// A LABELLED ROW, in the panel's own nine-column gutter -- `screen_pane_state.cpp`'s
/// `panel_field`, unchanged.
std::string panel_field(const char* label, const std::string& value) {
    return pad(label, 9) + value;
}

/// THE THREE-ROW BLOCK THE COMPILER'S ANSWER IS WRAPPED INTO -- `panel_block`, unchanged:
/// wrapped to the width in force, cut to the rows that survived the budget, and marked when
/// it was cut, so the elision mark tells the truth about THIS face rather than about the
/// nine-row one.
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

// =============================================================================
// The weave
// =============================================================================

class BuilderPaneWeave
    : public loom::WeaveBase<
          BuilderPaneWeave, pane::BuilderPaneState,
          loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneKey, PaneTextInput,
                       PaneActionRequested, builder::BuildStatus, builder::RecipeCatalog,
                       ProjectFrontierSaid, PlanNames, PlanRowWritten, SourceOpened,
                       surface::ClipboardCopy, surface::ClipboardText>,
          loom::Emit<PaneOffered, PaneActions, PaneContent, builder::StatusRequested,
                     builder::BuildRequested, builder::PromoteArtifact, builder::RevertArtifact,
                     ProjectFrontierRequested, PlanNamesRequested, PlanRowRequested,
                     RecipeSourceRequested, surface::ClipboardCopy,
                     surface::ClipboardTextRequested>> {
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

    /// WORKSHOP GRANTS THE PANE ITS ROOM -- the one beat on which this tool draws. It is
    /// also when the pane asks the two questions whose answers it deliberately does not
    /// keep: what the tool is (`StatusRequested`, which the tool answers with BOTH its
    /// catalog and its status -- the republish door) and what the project is waiting on.
    ///
    /// ⚠ IT ASKS EVERY TIME, and that is the point. The built-in asked when its panel
    /// opened and kept the answer on the panel until the panel was closed (WL-PROJ-12); a
    /// pane is granted a room when it opens and whenever its prose capacity changes, and
    /// each of those is a moment at which a picture this pane did not derive may be stale.
    /// Asking is one message; being wrong on a screen is what a maker acts on.
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
        notice_.clear();
        const std::uint64_t copied_before = clip_.writes;
        const std::uint64_t pastes_before = clip_.paste_requests;
        if (!role_.line.consume(key.scancode, key.modifiers, clip_)) {
            return;
        }
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
        notice_.clear(); // the maker has acted; the last act's answer is spent

        // THE MODE OWNS THE PANE'S ACTIONS FIRST. While the role line has the keyboard the
        // pane declares two rows and no more, so nothing else can arrive here -- but the
        // declaration and the keystroke race across two messages, and a stale id must mean
        // nothing rather than build something.
        if (role_.open) {
            if (asked.id == pane::kActionCommit) {
                commit_role(mail);
            } else if (asked.id == pane::kActionCancel) {
                close_role();
                notice_ = "nothing was loaded and nothing was written";
                declare(mail);
                say(mail);
            }
            return;
        }
        if (asked.id == pane::kActionBuild) {
            build_now(mail, state_.arm);
        } else if (asked.id == pane::kActionBuildRealize) {
            build_realize(mail);
        } else if (asked.id == pane::kActionPromote) {
            promote_image(mail);
        } else if (asked.id == pane::kActionRevert) {
            revert_image(mail);
        } else if (asked.id == pane::kActionRecipeNext) {
            choose_recipe(1, mail);
        } else if (asked.id == pane::kActionRecipeBack) {
            choose_recipe(-1, mail);
        } else if (asked.id == pane::kActionFrontier) {
            begin_frontier_build(mail);
        } else if (asked.id == pane::kActionLoadIt) {
            begin_load_it(mail);
        } else if (asked.id == pane::kActionEditSource) {
            edit_source(mail);
        }
    }

    // ---- What the tool says ---------------------------------------------------------

    /// THE TOOL'S OWN PICTURE, published `to_any`. Held for as long as this pane is showing
    /// it and no longer -- the copy is a member and not state, so a reload starts with
    /// nothing and asks (WL-PROJ-12's rule, carried across the seam).
    void on(const builder::BuildStatus& said, loom::Mail& mail) {
        // WAS THIS PANE WATCHING? The first live run of the built-in got this wrong and the
        // screen said so: reopening the panel asks the tool, the tool answers with the
        // outcome of a build that finished a minute ago, and the notice announced it as
        // though it had just happened. LEARNING a fact and WITNESSING an event are
        // different, and only the second is news (WL-PROJ-11).
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
        say(mail);
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
            picked_ = false;
        }
        say(mail);
    }

    // ---- What the host answers ------------------------------------------------------

    /// ⚠ THE ANSWER THIS PANE ASKED FOR MUST NOT ERASE WHAT IT JUST SAID. Every gesture here
    /// writes a notice, says its rows, and asks the frontier again -- and the answer arrives
    /// on the same drain, so an unconditional re-say would publish a second, notice-less
    /// picture over the first and a maker would see no sentence at all. That is exactly the
    /// defect the project browser's whole-loop witness found one pane over (`u` on a catalog
    /// produced no visible row), and it is gated the same way: on the answer being NEWS.
    ///
    /// A PAINT ANSWER IS NEWS WHEN THE FRONTIER MOVED, and nothing else about this pane can
    /// have changed while it was in flight -- so an answer that says what the pane already
    /// shows is a description and is dropped. The BUILD answer is never a description: it is
    /// the gesture's own decision, and it is made whether or not the picture moved.
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

    void on(const SourceOpened& said, loom::Mail& mail) {
        if (!mail.answers_ask() || !source_.awaiting || mail.correlation() != source_.pending) {
            return;
        }
        source_.awaiting = false;
        if (!said.accepted) {
            notice_ = said.refusal;
            say(mail);
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
                                                     pane::kBuilderPaneSummary});
        declare(mail);
    }

    /// WHAT THIS PANE ANSWERS TO RIGHT NOW -- re-declared whenever the mode changes.
    ///
    /// ⭐ A PANE IS ONE KEYBOARD CONTEXT, AND A MODE IS NOT A SECOND ONE (WL-FILES-16). The
    /// built-in's role line lived in a Workshop context of its own (`KeyContext::kAuthoring`)
    /// and could bind Return and Escape there without touching command mode; a pane's rows
    /// are joined into ONE map under its runtime handle. So while a maker is typing a role,
    /// this pane declares two rows and no more, and every other key reaches it as an ordinary
    /// `PaneKey` for the line to consume -- which is what lets Backspace delete a character
    /// rather than meaning one of the nine build verbs. `PaneActions` is a REPLACEMENT
    /// (WL-KEY-15): the host re-joins the map, so what leaves the declaration also leaves the
    /// keymap.
    ///
    /// AND THE IDS NEVER MOVE. `builder.build` is `builder.build` in every mode that declares
    /// it, so a maker's authored override for it is applied wherever it is in force.
    void declare(loom::Mail& mail) {
        PaneActions actions;
        actions.pane = pane::kBuilderPane;
        const auto row = [&actions](const char* id, const char* label, std::int64_t sc,
                                    std::int64_t mods = input::mod::kNone) {
            actions.rows.push_back(PaneActionRow{id, label, sc, mods});
        };
        if (role_.open) {
            row(pane::kActionCommit, "load it", input::scan::kReturn);
            row(pane::kActionCancel, "cancel", input::scan::kEscape);
            (void)mail.as_role(pane::kBuilderPaneRole).send_to_role(kWorkshopRole, actions);
            return;
        }
        // ---- THE SAME IDS THE OVERRIDE FILE ALREADY KNOWS, AND THE SAME DEFAULTS the
        // built-in shipped (workshop/keymap.hpp's command-mode rows, before this migration),
        // so every maker's authored keymap keeps working across it.
        row(pane::kActionBuild, "build", input::scan::kB);
        row(pane::kActionBuildRealize, "load after build", input::scan::kB, input::mod::kShift);
        row(pane::kActionPromote, "promote image", input::scan::kP, input::mod::kShift);
        row(pane::kActionRevert, "revert image", input::scan::kR, input::mod::kShift);
        row(pane::kActionLoadIt, "load it", input::scan::kO);
        row(pane::kActionRecipeNext, "recipe", input::scan::kC);
        row(pane::kActionRecipeBack, "recipe back", input::scan::kC, input::mod::kShift);
        row(pane::kActionFrontier, "frontier", input::scan::kF);
        row(pane::kActionEditSource, "edit source", input::scan::kE);
        (void)mail.as_role(pane::kBuilderPaneRole).send_to_role(kWorkshopRole, actions);
    }

    // ---- The asks -------------------------------------------------------------------

    /// WHY A FRONTIER WAS ASKED FOR. One shape answers two gestures -- the row this pane
    /// paints, and the one action whose whole decision is the answer -- and the answer must
    /// not be read as the other one.
    enum class Why { kPaint, kBuild };

    struct Ask {
        std::uint64_t pending = 0;
        bool awaiting = false;
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

    /// THE ROW THE MAKER IS ON: their explicit pick where it still names something, and the
    /// catalog's first row otherwise. `chosen` is bounded at USE and never at write, the
    /// built-in's own rule.
    std::size_t cursor_row() const {
        if (known_.recipes.empty()) {
            return 0;
        }
        const std::size_t at = named_row(state_.chosen);
        return at < known_.recipes.size() ? at : std::size_t{0};
    }

    /// Is there a recipe to act on at all, and if not, why not -- in the built-in's words.
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

    // ---- The nine gestures ----------------------------------------------------------

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

    /// ONE ACTION IN TWO STATES (RELOAD-1). THE BUTTON: an artifact is built and ready to
    /// load, nothing is armed, and no build is in flight -- what it sends is the finished
    /// build's OWN ask again with the second intention aboard (`shown_.recipe`, never the
    /// cursor's row, because the thing that is ready is the thing that was built). THE
    /// TOGGLE, everywhere else: it flips the maker's standing intent and nothing is sent.
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
        notice_ = "asked to revert `" + shown_.artifact +
                  "` -- the image before the last reload runs again, state kept";
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
        // several recipes produce one artifact.
        picked_ = true;
        notice_ = "build recipe: " + known_.recipes[to].recipe + " -> " +
                  known_.recipes[to].artifact;
        say(mail);
    }

    /// THE FRONTIER BUILD, IN TWO BEATS. The built-in read the owner's frontier inline; a
    /// pane must ask, so the gesture ASKS and the decision is made when the answer lands.
    /// Nothing between the two is remembered except that this ask was the gesture's.
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
            const std::size_t at = named_row(state_.chosen);
            const bool standing_pick =
                picked_ && at < known_.recipes.size() &&
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
        // THE SELECTION MOVES WITH THE GESTURE, VISIBLY: row 1 of the pane now names the
        // recipe this ask is about, and `build_now`'s own notice says it again. A gesture
        // that sent one recipe while the pane showed another would be the cross-referencing
        // this arc exists to end, reintroduced one row up.
        state_.chosen = known_.recipes[match].recipe;
        build_now(mail, /*realize=*/true);
    }

    /// LOAD IT (LOAD-IT), IN TWO BEATS. The chosen recipe's artifact gains the minimum plan
    /// row, with a role the maker types -- so the gesture first asks the host whether the
    /// plan already names the artifact, and opens the line only if it does not.
    void begin_load_it(loom::Mail& mail) {
        if (!has_recipe("nothing to load")) {
            say(mail);
            return;
        }
        const builder::RecipeSummary& row = known_.recipes[cursor_row()];
        names_.pending = ++asked_;
        names_.awaiting = true;
        names_.recipe = row.recipe;
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

    /// EDIT THE SOURCE THE CHOSEN RECIPE NAMES. The pane holds a recipe's NAME and never its
    /// procedure, so what crosses is the name and the host resolves it -- every refusal
    /// (an unknown id, a kind with no single source, a missing file, a dirty buffer) comes
    /// back as this door's own sentence.
    void edit_source(loom::Mail& mail) {
        if (!has_recipe("nothing was opened")) {
            say(mail);
            return;
        }
        source_.pending = ++asked_;
        source_.awaiting = true;
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(kWorkshopRole,
                          RecipeSourceRequested{known_.recipes[cursor_row()].recipe},
                          source_.pending);
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

    void say(loom::Mail& mail) {
        if (!granted_ || rows_ <= 0 || columns_ <= 0) {
            return;
        }
        std::vector<surface::SurfaceTextRow> out;
        if (role_.open) {
            say_role(out);
        } else {
            say_builder(out);
        }
        // A notice, when there is one, leads -- the built-in wrote it on the band; a pane has
        // only its own room, so its first row carries it.
        //
        // ⚠ IT IS CLEARED BY THE MAKER'S NEXT ACT, NOT BY BEING SAID, and that is a
        // correction the seam forced. One gesture here produces SEVERAL publications in one
        // drain -- it writes a notice, says its rows, and asks a door whose answer arrives on
        // the same turn and says them again -- and Workshop keeps only the last picture. A
        // notice cleared by the first `say` would therefore be a notice no maker ever reads,
        // which is the defect the project browser's whole-loop witness found one pane over
        // (`u` on a catalog produced no visible row at all). So the sentence stands until the
        // maker does something else, which is also the honest reading of it: it is the answer
        // to their last act.
        if (!notice_.empty() && static_cast<std::int64_t>(out.size()) < rows_) {
            out.insert(out.begin(),
                       surface::SurfaceTextRow{fit(notice_, columns_), surface::role::kAccent});
        }
        if (static_cast<std::int64_t>(out.size()) > rows_) {
            out.resize(static_cast<std::size_t>(rows_));
        }
        (void)mail.as_role(pane::kBuilderPaneRole)
            .send_to_role(kWorkshopRole, PaneContent{pane::kBuilderPane, std::move(out)});
    }

    void say_role(std::vector<surface::SurfaceTextRow>& out) {
        out.push_back(surface::SurfaceTextRow{fit("role for " + role_.stem + "> " +
                                                      role_.line.text(),
                                                  columns_),
                                              surface::role::kAccent});
        if (static_cast<std::int64_t>(out.size()) < rows_) {
            out.push_back(surface::SurfaceTextRow{
                fit(panel_field("loads", role_.stem + " (built by `" + role_.recipe + "`)"),
                    columns_),
                surface::role::kMuted});
        }
    }

    /// ⭐ THE COMPOSITION IS `paint_builder`'S, MOVED. The panel is one region and its rows
    /// are composed against the budget: nine facts do not fit five rows, so each fact carries
    /// a SURVIVAL PRIORITY and the rule is
    ///
    ///     what survives longest is what a maker is ACTING on -- the office's identity, the
    ///     live build's activity/result, the frontier the project is waiting on, what a build
    ///     will do next, the realization outcome, the compiler's own words -- and what yields
    ///     first is static metadata (the exit row, the command echo) and the tail of the
    ///     output block.
    ///
    /// The DISPLAY order never changes with the budget: a shorter face shows the same rows in
    /// the same order minus the ones that did not fit, so growing the window reveals more
    /// truth rather than switching to a different panel.
    void say_builder(std::vector<surface::SurfaceTextRow>& out) {
        struct Fact {
            std::string text;
            std::int64_t role = surface::role::kFill;
            std::int64_t priority = 0; ///< smaller survives longer; all distinct
        };
        std::vector<Fact> facts; // display order, priorities deciding survival

        // THE HEADER NAMES THE OFFICE IT IS PRESENTING, AND NOTHING ELSE -- the office of the
        // TOOL, because that is whose facts these are. This pane's own office is the pane
        // header Workshop draws above the room, which is where "who is showing me this"
        // belongs.
        //
        // ...AND SINCE RecipeCatalog v2 IT NAMES THE CATALOG IN FORCE AGAIN (P-WORK-20). The
        // row that said which authored file the recipes came from died with the built-in
        // browser's session projection, and nothing in the host could keep it honest
        // (WL-PROJ-09). It rides the catalog now, from its one owner, so the answer is the
        // tool's rather than a copy somebody kept.
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
            publish(out, std::move(facts), std::string());
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
            facts.push_back(Fact{panel_field("recipe", known_.recipes[at].recipe + " -> " +
                                                           known_.recipes[at].artifact + "  (" +
                                                           std::to_string(at + 1) + "/" +
                                                           std::to_string(held) + ")"),
                                 surface::role::kFill, 3});
        }
        // WHAT THE PROJECT IS WAITING ON, WHILE IT IS. The row exists exactly while the
        // frontier does, and it costs the third `said` row -- the row this pane can best
        // afford exactly here: a maker whose project is WAITING has no build output yet.
        //
        // THREE FACTS, ONE ROW, TWO OWNERS. The artifact and the blocked count are the
        // realization owner's, answered through the host's read-only door; which recipes can
        // produce the artifact is the tool's own published catalog, joined here BY STEM.
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
        const std::string carried =
            named_op ? " -- op #" + std::to_string(s.op) + ", " + std::to_string(s.chunks) + " out"
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
            realize_face = std::string(builder::name_of_realization(s.realization));
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
        publish(out, std::move(facts), s.detail);
    }

    /// KEEP WHAT THE BUDGET SEATS, IN DISPLAY ORDER -- `paint_builder`'s `publish`, moved. The
    /// priorities are distinct, so "the smallest that fit" is one threshold; the said block is
    /// wrapped LAST, into exactly the rows that survived. A dropped fact is dropped WHOLE --
    /// nothing substitutes for it, and the rows that remain neither move nor reword.
    ///
    /// ⚠ THE BUDGET IS ONE ROW SMALLER WHEN THERE IS A NOTICE, because a pane has no band to
    /// write one on: `say` inserts it in front and the whole content is cut to the room, so
    /// the composition is asked for one fewer row rather than having its last row silently
    /// dropped after the fact.
    template <class Facts>
    void publish(std::vector<surface::SurfaceTextRow>& out, Facts facts,
                 const std::string& said_detail) {
        const std::int64_t budget = rows_ - (notice_.empty() ? 0 : 1);
        if (budget <= 0) {
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
            said = panel_block("said", said_detail.empty() ? std::string("--") : said_detail,
                               said_kept, columns_);
        }
        std::size_t said_at = 0;
        for (auto& f : facts) {
            if (f.priority >= cut) {
                continue;
            }
            std::string text = f.text.empty() ? said[said_at++] : std::move(f.text);
            out.push_back(surface::SurfaceTextRow{fit(std::move(text), columns_), f.role});
        }
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
    /// HAS THE MAKER EXPLICITLY PICKED A RECIPE, as opposed to the catalog's first row being
    /// where they happen to be? The frontier action is the only reader (WL-PROJ-14).
    bool picked_ = false;

    /// THE PROJECT'S FRONTIER, AS OF THE LAST ANSWER. Not published, not state, and not
    /// derived here: three values the host's read-only door said, and a flag saying whether
    /// anything has been said at all.
    bool frontier_known_ = false;
    bool waiting_ = false;
    std::string frontier_artifact_;
    std::int64_t frontier_blocked_ = 0;

    std::string notice_;
    std::uint64_t asked_ = 0;

    struct FrontierAsk : Ask {
        Why why = Why::kPaint;
    } frontier_;
    struct NamesAsk : Ask {
        std::string recipe; ///< what the answer will open the role line for
    } names_;
    struct RowAsk : Ask {
        std::string stem;
        std::string role;
        std::string recipe;
    } row_;
    Ask source_;

    struct Role {
        bool open = false;
        std::string stem;   ///< the artifact the role is for
        std::string recipe; ///< the chosen recipe, whose product this may load
        component::TextBox line;
    } role_;

    component::Clipboard clip_;
    loom::AskBook clip_asks_{1};
    std::uint64_t paste_ask_ = 0;
    std::uint64_t paste_epoch_ = 0;
    bool pasting_ = false;
};

} // namespace

ZEN_EXPORT_WEAVE(BuilderPaneWeave)
