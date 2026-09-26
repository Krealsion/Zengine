// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_WEAVE_HPP
#define ZENGINE_BUILDER_WEAVE_HPP

// The Builder tool: an ordinary weave that knows which recipes this project has and the
// artifact each produces, follows the build of one, and publishes where it stands
// (`BuildStatus`). It holds no process, command, build tree, source path, timer or realization
// authority: it orders the runner, folds the runner's observations into one current picture,
// and judges the artifact. One build at a time is this tool's policy, not the runner's limit.
// Builder law: agents/realization.md

#include "builder/recipe.hpp"
#include "builder/vocabulary.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::builder {

/// The most of one operation's output this tool remembers for its status: a working tail, not
/// the record (`KeptOutput` is). The oldest characters go; the end says how a build ended.
inline constexpr std::size_t kMaxRemembered = 8u * 1024u;

/// How many of those lines a published status carries.
inline constexpr std::size_t kDetailLines = 3;

/// ONE LINE AN OPERATION SAID, AS KEPT: its bytes up to `kMaxKeptLineBytes`, and how many
/// more it had.
struct KeptLine {
    std::string text;
    std::int64_t cut = 0;
};

/// What one operation said, kept bounded at both ends: the first lines until `kKeptHeadBytes`,
/// the last until `kKeptTailBytes`, and a line falling out of the tail counted in `omitted`, so
/// line numbers stay the operation's own. A line is taken when its break arrives, or at the
/// end; a trailing CR is the break's.
// WL-OUT-02 -- agents/workshop/build-output.md
struct KeptOutput {
    std::int64_t op = 0;
    std::string recipe;
    std::string artifact;
    std::int64_t outcome = outcome::kRunning;
    std::int64_t status = 0;
    bool ended = false;
    std::int64_t said = 0;
    std::vector<KeptLine> head;
    std::size_t head_bytes = 0;
    bool head_closed = false;
    std::deque<KeptLine> tail;
    std::size_t tail_bytes = 0;
    std::int64_t omitted = 0;
    std::string pending;         ///< the start of a line whose break has not arrived
    std::int64_t pending_cut = 0; ///< bytes of that line past `kMaxKeptLineBytes`

    /// The next bytes of the operation's output.
    void take(const std::string& text) {
        std::size_t at = 0;
        while (at < text.size()) {
            const std::size_t nl = text.find('\n', at);
            const std::size_t end = nl == std::string::npos ? text.size() : nl;
            const std::size_t room =
                pending.size() < kMaxKeptLineBytes ? kMaxKeptLineBytes - pending.size() : 0;
            const std::size_t piece = end - at;
            const std::size_t kept = piece < room ? piece : room;
            pending.append(text, at, kept);
            pending_cut += static_cast<std::int64_t>(piece - kept);
            if (nl == std::string::npos) {
                return;
            }
            line();
            at = nl + 1;
        }
    }

    /// The operation will say no more: a last line with no break is still a line.
    void end(std::int64_t how, std::int64_t exit_status) {
        if (!pending.empty() || pending_cut != 0) {
            line();
        }
        ended = true;
        outcome = how;
        status = exit_status;
    }

    /// The line number the tail's first kept line has, or `said + 1` when the tail is empty.
    std::int64_t tail_from() const {
        return static_cast<std::int64_t>(head.size()) + omitted + 1;
    }

private:
    void line() {
        KeptLine kept{std::move(pending), pending_cut};
        pending.clear();
        pending_cut = 0;
        if (!kept.text.empty() && kept.text.back() == '\r') {
            kept.text.pop_back();
        }
        ++said;
        if (!head_closed && head_bytes + kept.text.size() <= kKeptHeadBytes) {
            head_bytes += kept.text.size();
            head.push_back(std::move(kept));
            return;
        }
        head_closed = true;
        tail_bytes += kept.text.size();
        tail.push_back(std::move(kept));
        while (tail_bytes > kKeptTailBytes && tail.size() > 1) {
            tail_bytes -= tail.front().text.size();
            tail.pop_front();
            ++omitted;
        }
    }
};

/// A PAGE OF `kept`, from line `from`, of at most `lines` lines and `kMaxOutputPageBytes`
/// bytes (at least one line when there is one). A page that would cross the lines no longer
/// kept stops before them, and a page asked for inside them begins after them. `from` 0 is
/// the page that ends at the last line said so far -- a reader following a running build.
// WL-OUT-02 -- agents/workshop/build-output.md
inline BuildOutputSaid page_of(const KeptOutput& kept, std::int64_t from, std::int64_t lines) {
    BuildOutputSaid out;
    out.op = kept.op;
    out.kept = true;
    out.recipe = kept.recipe;
    out.artifact = kept.artifact;
    out.outcome = kept.outcome;
    out.status = kept.status;
    out.ended = kept.ended;
    out.said = kept.said;
    out.omitted = kept.omitted;
    out.omitted_from = kept.omitted == 0 ? 0 : static_cast<std::int64_t>(kept.head.size()) + 1;
    const std::int64_t want =
        lines < 1 ? 1
                  : (lines > static_cast<std::int64_t>(kMaxOutputPageLines)
                         ? static_cast<std::int64_t>(kMaxOutputPageLines)
                         : lines);
    std::int64_t at = from;
    if (at == 0) {
        at = kept.said - want + 1;
    }
    if (at < 1) {
        at = 1;
    }
    const std::int64_t head = static_cast<std::int64_t>(kept.head.size());
    if (at > head && at < kept.tail_from()) {
        at = kept.tail_from(); // inside the gap: the page begins after it
    }
    out.first = at;
    std::size_t bytes = 0;
    while (at <= kept.said && static_cast<std::int64_t>(out.text.size()) < want) {
        const KeptLine* line = nullptr;
        if (at <= head) {
            line = &kept.head[static_cast<std::size_t>(at - 1)];
        } else if (at >= kept.tail_from()) {
            line = &kept.tail[static_cast<std::size_t>(at - kept.tail_from())];
        } else {
            break; // the gap: stop before it
        }
        if (!out.text.empty() && bytes + line->text.size() > kMaxOutputPageBytes) {
            break;
        }
        bytes += line->text.size();
        out.cut += line->cut;
        out.text.push_back(line->text);
        ++at;
    }
    return out;
}

/// What a file looked like at one moment -- presence, size, last change -- taken before and
/// after a build so "built" is told from "already up to date". Not the success test: that is
/// `status == 0 && present`, since only the build system may say a file is current.
struct ArtifactStamp {
    bool present = false;
    std::uintmax_t size = 0;
    std::int64_t changed = 0; ///< an opaque, comparable modification time

    friend bool operator==(const ArtifactStamp&, const ArtifactStamp&) = default;
};

/// LOOK AT ONE FILE, NOW. Total: a path that is not there, cannot be read, or is
/// not a regular file all answer `present = false`, and nothing throws.
inline ArtifactStamp stamp_of(const std::string& path) {
    ArtifactStamp out;
    if (path.empty()) {
        return out;
    }
    std::error_code ec;
    const std::filesystem::path file(path);
    if (!std::filesystem::is_regular_file(file, ec) || ec) {
        return out;
    }
    out.present = true;
    out.size = std::filesystem::file_size(file, ec);
    if (ec) {
        out.size = 0;
    }
    const std::filesystem::file_time_type when = std::filesystem::last_write_time(file, ec);
    out.changed = ec ? 0 : static_cast<std::int64_t>(when.time_since_epoch().count());
    return out;
}

/// The tool's memory: what a presentation is shown, plus two tallies only an operator asks for.
/// `default_image` is heard from the realization owner and never derived here.
struct BuilderState {
    std::string recipe;               ///< the recipe this picture is about
    std::string artifact;             ///< the artifact stem that recipe produces
    std::int64_t outcome = outcome::kNeverBuilt;
    std::int64_t status = 0;          ///< the last process's exit status
    std::string command;              ///< what the runner said it ran, once it has run
    std::string detail;               ///< the last thing said about this recipe
    std::int64_t builds = 0;          ///< asks this tool has taken, ever
    std::int64_t stray = 0;           ///< observations that were about somebody else's work
    std::int64_t op = 0;              ///< the operation this picture is about
    std::int64_t chunks = 0;          ///< output observations folded in for it
    bool realize = false;             ///< the current ask was BUILD & REALIZE
    std::int64_t realization = realization::kNotAsked;
    std::string realized_detail;      ///< realization's own words, when it has said any
    std::int64_t offered = 0;         ///< artifacts this tool has offered for realization
    bool default_image = false;       ///< the realized image is the file a restart loads

    ZEN_EXPOSE();
    ZEN_SHAPE(BuilderState, 4, ZEN_FIELD(recipe), ZEN_FIELD(artifact), ZEN_FIELD(outcome),
              ZEN_FIELD(status), ZEN_FIELD(command), ZEN_FIELD(detail), ZEN_FIELD(builds),
              ZEN_FIELD(stray), ZEN_FIELD(op), ZEN_FIELD(chunks), ZEN_FIELD(realize),
              ZEN_FIELD(realization), ZEN_FIELD(realized_detail), ZEN_FIELD(offered),
              ZEN_FIELD(default_image));
};

class BuilderWeave
    : public loom::WeaveBase<BuilderWeave, BuilderState,
                             loom::Accept<BuildRequested, StatusRequested, BuildStatusRequested,
                                          BuildStarted, BuildOutput, BuildFinished,
                                          BuildNotStarted, ArtifactRealized, ArtifactPromoted,
                                          BuildOutputRequested>,
                             loom::Emit<RunBuild, BuildStatus, BuildAsked, RecipeCatalog,
                                        OfferArtifact, BuildOutputSaid>> {
public:
    /// The recipe views and the file they came from are read from their owner, the host
    /// (`CurrentRecipes`), never copied and never state: a poke that could write an artifact
    /// path here could make this tool announce somebody else's file. Reading both at the ask
    /// means rows are never shown under another catalog's name, and what is read is still a
    /// `RecipeView`, never a procedure. The owner must outlive this weave.
    BuilderWeave(const std::vector<RecipeView>& recipes, const std::string& source)
        : recipes_(recipes), source_(&source) {}

    /// A temporary catalog of views would dangle, and only the compiler can say so in time.
    BuilderWeave(std::vector<RecipeView>&&, const std::string&) = delete;
    BuilderWeave(const std::vector<RecipeView>&, std::string&&) = delete;
    BuilderWeave(std::vector<RecipeView>&&, std::string&&) = delete;

    /// Say what you are: the catalog and the status, the two shapes a presentation needs when
    /// it opens -- two, because the catalog changes only when one is installed and the status
    /// on every line a compiler says. A host that has replaced the catalog republishes this way.
    void on(const StatusRequested&, loom::Mail& mail) {
        RecipeCatalog said;
        said.recipes.reserve(recipes_.size());
        for (const RecipeView& r : recipes_) {
            said.recipes.push_back(RecipeSummary{r.id, r.artifact});
        }
        // Where the rows came from, read at the ask from the same owner as the rows.
        said.source = *source_;
        (void)mail.publish(std::move(said));
        say(mail);
    }

    /// WHERE YOU STAND, TO ME ALONE: the picture `say` publishes, answered to one asker and
    /// published to nobody -- the baseline an observer that came late or came back joins
    /// (vocabulary.hpp, `BuildStatusRequested`). Read-only: it moves no build and no counter.
    void on(const BuildStatusRequested&, loom::Mail& mail) { (void)mail.answer(status()); }

    /// Build the recipe you know by this name. A name this tool does not hold is refused, which
    /// is why a `BuildRequested` from anywhere cannot widen what this program builds.
    void on(const BuildRequested& ask, loom::Mail& mail) {
        const RecipeView* chosen = view_named(recipes_, ask.recipe);
        if (chosen == nullptr) {
            state_.outcome = outcome::kUnknownRecipe;
            state_.detail = recipes_.empty()
                                ? std::string("this Builder holds no recipes at all")
                                : "this Builder holds no recipe called `" + ask.recipe +
                                      "` (it holds " + std::to_string(recipes_.size()) + ")";
            heard(ask, false, mail);
            say(mail);
            return;
        }
        if (still_going(state_.outcome)) {
            // One at a time, refused in the tool's own voice. `builds` does not move: it counts
            // the asks taken.
            state_.detail = state_.op == 0
                                ? std::string("a build was already asked for and has not "
                                              "started yet")
                                : "a build is already running: operation #" +
                                      std::to_string(state_.op);
            heard(ask, false, mail);
            say(mail);
            return;
        }
        ++state_.builds;
        state_.recipe = chosen->id;
        state_.artifact = chosen->artifact;
        state_.outcome = outcome::kAsked;
        state_.status = 0;
        state_.op = 0;
        state_.chunks = 0;
        state_.detail.clear();
        state_.realize = ask.realize;
        state_.realization = ask.realize ? realization::kAsked : realization::kNotAsked;
        state_.realized_detail.clear();
        state_.default_image = false;
        remembered_.clear();
        // The file this operation is about, taken now: the catalog can be replaced while the
        // build runs, and judging the ending against a later recipe would report on a build
        // that was never started.
        path_ = chosen->path;
        // What was there before: tells `built` from `unchanged`, and is never the success test.
        before_ = stamp_of(path_);
        // The previous command stays for the same recipe (it is about to be said again) and is
        // cleared for another, which it would misdescribe.
        if (built_ != chosen->id) {
            state_.command.clear();
        }
        // Said before the order, so the record reads in the order the facts became true.
        heard(ask, true, mail);
        say(mail);
        (void)mail.send_to_role(kBuildRunnerRole, RunBuild{state_.recipe});
    }

    /// A process began.
    void on(const BuildStarted& began, loom::Mail& mail) {
        if (!mine(began.recipe)) {
            return;
        }
        state_.op = began.op;
        state_.command = began.command;
        state_.outcome = outcome::kRunning;
        state_.status = 0;
        state_.chunks = 0;
        state_.detail.clear();
        remembered_.clear();
        // THE OPERATION'S OWN RECORD BEGINS HERE, under its own number, and the oldest one
        // kept is let go when there would be more than `kKeptOperations`.
        KeptOutput record;
        record.op = began.op;
        record.recipe = began.recipe;
        record.artifact = state_.artifact;
        kept_.push_back(std::move(record));
        while (kept_.size() > kKeptOperations) {
            kept_.erase(kept_.begin());
        }
        say(mail);
    }

    /// IT SAID SOMETHING. Folded in and republished, which is what makes a
    /// running build visibly alive rather than merely believed to be -- and kept, in
    /// the operation's own record, which is what makes it readable afterwards.
    void on(const BuildOutput& said, loom::Mail& mail) {
        if (!mine(said.recipe) || !about_current(said.op)) {
            return;
        }
        ++state_.chunks;
        remember(said.text);
        if (KeptOutput* record = kept(said.op)) {
            record->take(said.text);
        }
        state_.detail = tail_lines(remembered_, kDetailLines);
        say(mail);
    }

    /// A page of one operation's output, by its number; read-only. An operation not kept is
    /// answered `kept` false, never with another operation's lines.
    // WL-OUT-02 -- agents/workshop/build-output.md
    void on(const BuildOutputRequested& asked, loom::Mail& mail) {
        BuildOutputSaid out;
        if (const KeptOutput* record = kept(asked.op)) {
            out = page_of(*record, asked.from, asked.lines);
        } else {
            out.op = asked.op;
        }
        for (const KeptOutput& record : kept_) {
            out.ops.push_back(record.op);
        }
        (void)mail.answer(std::move(out));
    }

    /// It exited: only now is there an artifact question. The exit status is consulted first and
    /// the file second, so a file left by an earlier success is never this build's product.
    void on(const BuildFinished& done, loom::Mail& mail) {
        if (!mine(done.recipe) || !about_current(done.op)) {
            return;
        }
        state_.status = done.status;
        const std::string said = tail_lines(remembered_, kDetailLines);
        if (done.status != 0) {
            state_.outcome = outcome::kFailed;
            state_.detail = said;
            end_record(done.op);
            if (state_.realize) {
                // A FAILED BUILD OFFERS NOTHING. Said in the status rather than left
                // as an absence, because a maker who pressed BUILD & REALIZE is owed
                // the reason nothing was realized.
                state_.realization = realization::kRefused;
                state_.realized_detail = "the build failed, so nothing was offered to the "
                                         "project";
            }
            say(mail);
            return;
        }
        // The operation's own file, not the catalog's current answer: the catalog may have
        // changed under the running build.
        const ArtifactStamp after = stamp_of(path_);
        if (!after.present) {
            // A GREEN BUILD WITH NO PRODUCT. It is neither success nor failure and is
            // reported as neither: the process is fine and the project is not.
            state_.outcome = outcome::kNoArtifact;
            state_.detail = "the build succeeded and `" + state_.artifact + "` is not at " +
                            (path_.empty() ? std::string("(no recipe)") : path_) +
                            (said.empty() ? std::string() : " | " + said);
            end_record(done.op);
            if (state_.realize) {
                state_.realization = realization::kRefused;
                state_.realized_detail = "the expected artifact was not produced, so nothing "
                                         "was offered to the project";
            }
            say(mail);
            return;
        }
        state_.outcome = outcome::kSucceeded;
        built_ = done.recipe;
        end_record(done.op);
        const bool moved = !(after == before_);
        state_.detail = std::string(moved ? "built " : "already up to date: ") +
                        state_.artifact + (said.empty() ? std::string() : " | " + said);
        if (!state_.realize) {
            say(mail);
            return;
        }
        // One offer; the decision is the realization owner's, which refuses in its own words.
        ++state_.offered;
        state_.realization = realization::kOffered;
        state_.realized_detail = "offered to the project";
        say(mail);
        (void)mail.publish(OfferArtifact{state_.op, state_.recipe, state_.artifact, path_});
    }

    /// Nothing ran, and this is why. It may name the operation (a child that never became its
    /// program) or none (op 0, before any was announced); `about_current` accepts both.
    void on(const BuildNotStarted& never, loom::Mail& mail) {
        if (!mine(never.recipe) || !about_current(never.op)) {
            return;
        }
        state_.outcome = outcome::kNotStarted;
        state_.status = 0;
        if (!never.command.empty()) {
            state_.command = never.command;
        }
        state_.detail = never.trouble;
        end_record(never.op);
        if (state_.realize) {
            state_.realization = realization::kRefused;
            state_.realized_detail = "no build ran, so nothing was offered to the project";
        }
        say(mail);
    }

    /// The project answered: folded in and republished, changing nothing about the build.
    /// Matched by artifact, not operation (realization has none): an answer about the artifact
    /// this tool last offered, realized or was refused about is folded in -- a revert answers
    /// again -- and any other is somebody else's conversation.
    void on(const ArtifactRealized& answer, loom::Mail& mail) {
        const bool about_mine = answer.artifact == state_.artifact &&
                                (state_.realization == realization::kOffered ||
                                 state_.realization == realization::kRealized ||
                                 state_.realization == realization::kRefused);
        if (!about_mine) {
            ++state_.stray;
            return;
        }
        state_.realization = answer.realized ? realization::kRealized : realization::kRefused;
        state_.realized_detail = answer.detail;
        state_.default_image = answer.default_image;
        say(mail);
    }

    /// A promotion's answer, heard for the artifact this tool realized; any other is somebody
    /// else's. It lands on the realization row, where the maker reads.
    void on(const ArtifactPromoted& answer, loom::Mail& mail) {
        const bool about_mine = answer.artifact == state_.artifact &&
                                (state_.realization == realization::kRealized ||
                                 state_.realization == realization::kRefused);
        if (!about_mine) {
            ++state_.stray;
            return;
        }
        if (answer.promoted) {
            // A PROMOTED IMAGE IS A RUNNING ONE, whatever the last ask came to: the
            // owner refuses a promotion of anything that is not live.
            state_.realization = realization::kRealized;
            state_.default_image = true;
        }
        state_.realized_detail = answer.detail;
        say(mail);
    }

    /// What this tool knows, for a suite that wants to check where a message
    /// left it. Read-only, and it is deliberately MORE than the published
    /// status: `stray` is a fact an operator or a test may want and a
    /// presentation has no business showing.
    const BuilderState& known() const { return state_; }

    /// The recipes this tool answers for -- the host's own list coming back. Read-only,
    /// and no weave learns it this way.
    const std::vector<RecipeView>& recipes() const { return recipes_; }

    /// What the artifact looked like before the current build began.
    const ArtifactStamp& before() const { return before_; }

    /// The file the CURRENT operation is about -- resolved when it was ordered, and the
    /// one this tool will look at when it ends. Read-only, and a suite's way of proving
    /// that an operation kept its own subject across a catalog replacement.
    const std::string& artifact_path() const { return path_; }

private:
    /// Is this about the recipe this tool is following? Another recipe's fact is counted
    /// (`stray`), so "this never happens" is checkable, and is never folded in.
    bool mine(const std::string& recipe) {
        if (recipe == state_.recipe) {
            return true;
        }
        ++state_.stray;
        return false;
    }

    /// Is this about the operation this tool is following? It orders one build at a time, so
    /// another operation's observation is somebody else's: counted, not folded in.
    bool about_current(std::int64_t op) {
        if (op == state_.op) {
            return true;
        }
        ++state_.stray;
        return false;
    }

    /// THE STATUS'S WORKING TAIL, in the bytes the build wrote: a piece of output is the next
    /// bytes of one stream (`BuildOutput` v3), so nothing is inserted between two pieces.
    void remember(const std::string& text) {
        remembered_ += text;
        if (remembered_.size() > kMaxRemembered) {
            remembered_.erase(0, remembered_.size() - kMaxRemembered);
        }
    }

    /// THE RECORD THIS TOOL KEEPS FOR OPERATION `op`, or null.
    KeptOutput* kept(std::int64_t op) {
        if (op == 0) {
            return nullptr;
        }
        for (KeptOutput& record : kept_) {
            if (record.op == op) {
                return &record;
            }
        }
        return nullptr;
    }

    /// THE OPERATION ENDED, with the outcome this tool judged: its record says so, and a last
    /// line with no break is taken.
    void end_record(std::int64_t op) {
        if (KeptOutput* record = kept(op)) {
            record->end(state_.outcome, state_.status);
        }
    }

    /// WHAT BECAME OF ONE ASK, said once per `BuildRequested` (vocabulary.hpp, `BuildAsked`).
    void heard(const BuildRequested& ask, bool taken, loom::Mail& mail) {
        (void)mail.publish(BuildAsked{taken ? state_.builds : 0, ask.recipe, ask.realize, taken,
                                      taken ? std::string() : state_.detail});
    }

    BuildStatus status() const {
        return BuildStatus{state_.recipe,      state_.artifact,        state_.outcome,
                           state_.status,      state_.command,         state_.detail,
                           state_.builds,      state_.op,              state_.chunks,
                           state_.realize,     state_.realization,     state_.realized_detail,
                           state_.default_image};
    }

    void say(loom::Mail& mail) { (void)mail.publish(status()); }

    /// The owner's views, bound once to the vector the host holds, so replacing its contents
    /// replaces what this tool answers for. Never state.
    const std::vector<RecipeView>& recipes_;

    /// The file those views came from, read at the ask and never copied; a pointer only so the
    /// deleted rvalue constructors can exist beside the one that binds it.
    const std::string* source_;

    /// The current operation's output so far, bounded: a working buffer and not state. What the
    /// tool says of it is `detail`; a poke-readable copy would be a second answer.
    std::string remembered_;

    /// The one file the current operation is about, resolved when the ask was accepted, so a
    /// catalog changed underneath a running build cannot re-decide its subject.
    std::string path_;

    /// What the expected artifact looked like when the current build was ordered.
    ArtifactStamp before_;

    /// The last recipe this tool saw succeed -- which is how it knows whether the
    /// command it is still showing is about the recipe now being asked for.
    std::string built_;

    /// WHAT EACH OF THE LAST `kKeptOperations` OPERATIONS SAID, oldest first. A plain member
    /// and not state, for `remembered_`'s reason: it is answered by asking, one bounded page
    /// at a time, and a poke-writable copy would be a second answer to "what did it say".
    // WL-OUT-02 -- agents/workshop/build-output.md
    std::vector<KeptOutput> kept_;
};

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_WEAVE_HPP
