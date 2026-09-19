// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_EXAMPLES_WORKSHOP_PROBE_HPP
#define ZENGINE_EXAMPLES_WORKSHOP_PROBE_HPP

// THE WORKSHOP PROBE -- one reusable testing journey through two real Loom hosts, as a loadable
// weave a supplied host boots (probe.cpp exports it; this header is the weave, so a suite can
// drive the same code the host loads).
//
// WHAT IT IS. An ordinary participant of the EXTERNAL host: it knows a link's office
// (`loom.link.<name>`, the supplied host's own door to a running Workshop) and a handful of
// Workshop shapes, and when an operator tells it to run, it drives one journey across the link:
//
//     0. ask the link which session it holds on the far host, and as whom   (loom.link.<name>)
//     1. open an input session with Workshop's Input owner                   (zengine.input)
//     2. inject a key chord -- Ctrl+P by default, the desktop's own Pane Manager launch --
//        asking the link to hold the answer until Workshop's bus has DISPATCHED everything the
//        injection set in motion (the link's `settle`)
//     3. ask the guest door for the connection inventory, and find the row that IS this link's
//        session -- by the far session the link established, never "the last admitted row"
//                                                                            (zengine.guests)
//     4. take a picture of what Workshop presents now -- after that settlement -- and fetch it
//        chunk by chunk, checking each chunk continues the picture it belongs to  (zengine.skin)
//     5. write it to a file beside this host, and check the file really holds it
//     6. close the session, and answer the operator
//
// WHAT MAY MOVE IT. Every step is an ask across the link, answered once by the link with Loom's
// own answer authority: the far owner's answer, or the link's `Outcome`. The probe moves on an
// arrival only when THIS bus attests it as the answer to the probe's own open conversation with
// the link (`mail.answers_ask()`, the correlation, the link's stamp), and only when it is the
// answer the step it is on asked for. Anything else -- another participant's answer-shaped word,
// a far participant's ordinary speech, a duplicate -- is data, and moves nothing.
//
// WHAT PASS MEANS. Every promised result, from its owner: the link admitted; a session opened;
// the whole batch injected and settled; this link's session listed as admitted under the name the
// link was established as; a picture whose every chunk continues it and whose bytes all arrived;
// the file written, closed and read back at that size; the session closed. Anything short of
// that is STOPPED, with the step and the owner's own words.
//
// WHAT IT CLEANS UP. Once a session is open, a run that stops for any reason it can still speak
// through closes the session before it answers -- and says what the close came to beside the
// reason it stopped, never instead of it. When the LINK is gone, nothing can be closed from here:
// Workshop's guest door closes a lost guest's session when it sees the connection end, and the
// probe says exactly that, never that it closed anything. A close nobody has answered leaves the
// run pending, visibly (`step` says `cleanup`), rather than inventing a deadline.
//
// WHAT IT IS NOT. Not a host: it holds no socket and no bus; the link does. Not privileged:
// what it may say to the link is one rule the operator grants at the console
// (`authority allow probe loom.link.Ask v1 -> role loom.link.workshop`, and the same for
// `loom.link.StatusRequested`), and what the link's session may say to Workshop is Workshop's own
// guests file. Not a framework: one journey, parameterised by the message that starts it.
//
// BUILT AGAINST INSTALLED PACKAGES ONLY: `find_package(loom)` for the weave surface and the link
// envelope, `find_package(zengine)` for the Input, Surface and guest vocabularies.

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"

#include <zen/bridge/link.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace zengine::workshop_probe {

/// RUN THE JOURNEY. Sent by the operator (the console's `send <probe> RunProbe 1 ...`), and
/// answered when the journey settles or stops. Every parameter has a default a first run can
/// take: the link name, the chord, the far offices, the file the picture lands in.
struct RunProbe {
    std::string link = "workshop";   ///< the supplied host's link name (`loom.link.<link>`)
    std::string text;                ///< text to type after the chord, if any
    std::int64_t scancode = zengine::input::scan::kP;      ///< the chord's key (Ctrl+P)
    std::int64_t modifiers = zengine::input::mod::kCtrl;
    std::string inspect = "zengine.guests";       ///< the office asked for the inventory
    std::string picture = "workshop-probe.bmp";   ///< where the fetched picture is written
    ZEN_SHAPE(RunProbe, 1, ZEN_FIELD(link), ZEN_FIELD(text), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(inspect), ZEN_FIELD(picture));
};

/// What the last run came to, for a poke and for the answer. `step` is where a run stands --
/// `idle` when none is in flight, `cleanup` while a close it asked for is unanswered.
struct ProbeState {
    std::int64_t runs = 0;
    std::int64_t session = 0;
    std::int64_t injected = 0;
    std::int64_t capture = 0;
    std::int64_t picture_bytes = 0;
    std::int64_t ignored = 0; ///< arrivals that moved nothing: not answers, or not this step's
    std::string step = "idle";
    ZEN_EXPOSE();
    ZEN_SHAPE(ProbeState, 1, ZEN_FIELD(runs), ZEN_FIELD(session), ZEN_FIELD(injected),
              ZEN_FIELD(capture), ZEN_FIELD(picture_bytes), ZEN_FIELD(ignored), ZEN_FIELD(step));
};

class ProbeWeave final
    : public loom::WeaveBase<
          ProbeWeave, ProbeState,
          loom::Accept<RunProbe, loom::link::Status, zengine::input::InputSessionOpened,
                       zengine::input::InputInjected, zengine::workshop::GuestConnections,
                       zengine::surface::SurfaceCaptured, zengine::surface::SurfaceCaptureChunk,
                       loom::Ack, loom::Refused, loom::link::Outcome>,
          loom::Emit<loom::link::Ask, loom::link::StatusRequested, loom::Result>> {
public:
    ProbeWeave() : book_(4) {}

    // ---- the operator's door -----------------------------------------------------------------

    void on(const RunProbe& run, loom::Mail& mail) {
        if (answer_.valid()) {
            (void)mail.answer(loom::Refused{"a run is already in flight (" + state_.step +
                                            "); wait for its answer"});
            return;
        }
        loom::DeferredAnswer due = mail.defer_answer();
        if (!due.valid()) {
            (void)mail.answer(loom::Refused{"this delivery cannot defer its answer"});
            return;
        }
        answer_ = std::move(due);
        run_ = run;
        ++state_.runs;
        state_.session = 0;
        state_.injected = 0;
        state_.capture = 0;
        state_.picture_bytes = 0;
        report_.clear();
        failure_.clear();
        picture_.clear();
        link_id_ = loom::WeaveId{};
        far_session_ = 0;
        established_.clear();
        batch_size_ = 0;
        capture_ = 0;
        picture_total_ = 0;
        fetched_ = 0;
        set_step(Step::Status);
        // THE LINK'S OWN WORD ABOUT ITSELF: which far session this journey runs on, and as whom.
        const loom::AskOpened opened = book_.open_to_role(loom::link::role_of(run_.link),
                                                          loom::link::StatusRequested::zen_name, 1);
        if (!opened.ok) {
            stop(mail, "this probe's own book is full");
            return;
        }
        (void)mail.send_to_role(loom::link::role_of(run_.link), loom::link::StatusRequested{},
                                opened.correlation);
    }

    // ---- the answers, each settling one conversation on the book -----------------------------

    void on(const loom::link::Status& s, loom::Mail& mail) {
        if (!answer_to(mail, Step::Status, loom::link::StatusRequested::zen_name)) {
            return;
        }
        link_id_ = mail.sender(); // Loom attested this as the link's answer; later asks name it
        if (s.state != "admitted") {
            stop(mail, "the link '" + run_.link + "' is " + s.state +
                           (s.detail.empty() ? std::string() : ": " + s.detail));
            return;
        }
        far_session_ = s.session;
        established_ = s.established_name;
        note("link '" + run_.link + "' admitted as '" + established_ + "' (far session " +
             std::to_string(far_session_) + ")");
        set_step(Step::Open);
        ask(mail, zengine::input::kInputRole, zengine::input::InputSessionRequested{"workshop probe"});
    }

    void on(const zengine::input::InputSessionOpened& opened, loom::Mail& mail) {
        if (!answer_to(mail, Step::Open, zengine::input::InputSessionRequested::zen_name)) {
            return;
        }
        if (opened.session <= 0) {
            stop(mail, "zengine.input opened no session it can name (" +
                           std::to_string(opened.session) + ")");
            return;
        }
        state_.session = opened.session;
        note("session " + std::to_string(opened.session) + " opened by zengine.input");
        zengine::input::InjectInput batch;
        batch.session = opened.session;
        zengine::input::InjectedEvent down;
        down.kind = "KeyPressed";
        down.scancode = run_.scancode;
        down.modifiers = run_.modifiers;
        zengine::input::InjectedEvent up = down;
        up.kind = "KeyReleased";
        batch.events.push_back(down);
        batch.events.push_back(up);
        if (!run_.text.empty()) {
            zengine::input::InjectedEvent typed;
            typed.kind = "TextEntered";
            typed.text = run_.text;
            batch.events.push_back(typed);
        }
        batch_size_ = static_cast<std::int64_t>(batch.events.size());
        set_step(Step::Inject);
        // SETTLED: the answer comes back only once Workshop's bus has dispatched everything
        // the injection set in motion there -- the desktop's handling of the chord and every
        // delivery it caused, the repaint among them. The picture below is asked for after it.
        ask(mail, zengine::input::kInputRole, batch, /*settle=*/true);
    }

    void on(const zengine::input::InputInjected& done, loom::Mail& mail) {
        if (!answer_to(mail, Step::Inject, zengine::input::InjectInput::zen_name)) {
            return;
        }
        if (done.session != state_.session || done.admitted != batch_size_ ||
            done.last_seq - done.first_seq + 1 != batch_size_) {
            fail(mail, "zengine.input answered for session " + std::to_string(done.session) +
                           " with " + std::to_string(done.admitted) + " moment(s) (seq " +
                           std::to_string(done.first_seq) + ".." + std::to_string(done.last_seq) +
                           "), not the " + std::to_string(batch_size_) + " of session " +
                           std::to_string(state_.session) + " that were sent");
            return;
        }
        state_.injected = done.admitted;
        note("injected " + std::to_string(done.admitted) + " moment(s), session seq " +
             std::to_string(done.first_seq) + ".." + std::to_string(done.last_seq) +
             ", settled on Workshop's bus");
        set_step(Step::Inspect);
        ask(mail, run_.inspect, zengine::workshop::GuestConnectionsRequested{});
    }

    /// THE INVENTORY, FROM ITS OWNER: this session is the row the far host established for THIS
    /// link -- matched by the far session the link holds, and by the name it was established as.
    void on(const zengine::workshop::GuestConnections& said, loom::Mail& mail) {
        if (!answer_to(mail, Step::Inspect, zengine::workshop::GuestConnectionsRequested::zen_name)) {
            return;
        }
        const zengine::workshop::GuestConnection* mine = nullptr;
        for (const zengine::workshop::GuestConnection& c : said.rows) {
            if (c.session == far_session_) {
                mine = &c;
            }
        }
        if (mine == nullptr) {
            fail(mail, run_.inspect + " lists " + std::to_string(said.rows.size()) +
                           " connection(s), and none is this link's session (far session " +
                           std::to_string(far_session_) + ")");
            return;
        }
        if (mine->state != "admitted" || mine->established != established_) {
            fail(mail, run_.inspect + " lists this link's session as " + mine->state + " '" +
                           mine->established + "', not admitted as '" + established_ + "'");
            return;
        }
        note(run_.inspect + " lists " + std::to_string(said.rows.size()) +
             " connection(s); this session is '" + mine->established + "' (claimed '" +
             mine->claimed + "', weave " + std::to_string(mine->session) + ")");
        set_step(Step::Capture);
        ask(mail, zengine::surface::kSkinRole, zengine::surface::SurfaceCaptureRequested{});
    }

    void on(const zengine::surface::SurfaceCaptured& c, loom::Mail& mail) {
        if (!answer_to(mail, Step::Capture, zengine::surface::SurfaceCaptureRequested::zen_name)) {
            return;
        }
        if (!c.ok) {
            fail(mail, "zengine.skin refused the capture: " + c.refusal);
            return;
        }
        if (c.capture <= 0 || c.width <= 0 || c.height <= 0 || c.format.empty() || c.bytes <= 0 ||
            c.bytes > zengine::surface::kMaxCaptureBytes) {
            fail(mail, "zengine.skin described capture " + std::to_string(c.capture) + " as " +
                           std::to_string(c.width) + "x" + std::to_string(c.height) + " '" +
                           c.format + "', " + std::to_string(c.bytes) +
                           " bytes -- not a picture this probe can fetch");
            return;
        }
        state_.capture = c.capture;
        capture_ = c.capture;
        picture_total_ = c.bytes;
        picture_format_ = c.format;
        note("capture " + std::to_string(c.capture) + " at frame " + std::to_string(c.frame) + ": " +
             std::to_string(c.width) + "x" + std::to_string(c.height) + " " + c.format + ", " +
             std::to_string(c.bytes) + " bytes");
        set_step(Step::Fetch);
        fetch(mail);
    }

    /// ONE CHUNK: it must continue THIS picture, at exactly the offset asked for, within its size.
    void on(const zengine::surface::SurfaceCaptureChunk& chunk, loom::Mail& mail) {
        if (!answer_to(mail, Step::Fetch,
                       zengine::surface::SurfaceCaptureChunkRequested::zen_name)) {
            return;
        }
        const std::int64_t size = static_cast<std::int64_t>(chunk.data.size());
        if (chunk.capture != capture_ || chunk.offset != fetched_ || chunk.total != picture_total_ ||
            size <= 0 || fetched_ + size > picture_total_) {
            fail(mail, "zengine.skin sent a chunk of capture " + std::to_string(chunk.capture) +
                           " at " + std::to_string(chunk.offset) + " (" + std::to_string(size) +
                           " of " + std::to_string(chunk.total) + " bytes) where capture " +
                           std::to_string(capture_) + " at " + std::to_string(fetched_) + " of " +
                           std::to_string(picture_total_) + " was asked for");
            return;
        }
        picture_.append(chunk.data.begin(), chunk.data.end());
        fetched_ += size;
        if (fetched_ < picture_total_) {
            fetch(mail);
            return;
        }
        state_.picture_bytes = static_cast<std::int64_t>(picture_.size());
        std::string why;
        if (!write_picture(&why)) {
            fail(mail, why);
            return;
        }
        note("picture written to " + run_.picture + " (" + std::to_string(picture_.size()) + " of " +
             std::to_string(picture_total_) + " bytes, " + picture_format_ + ", read back whole)");
        picture_.clear();
        set_step(Step::Close);
        ask(mail, zengine::input::kInputRole,
            zengine::input::InputSessionClosed{state_.session, 0});
    }

    void on(const loom::Ack&, loom::Mail& mail) {
        if (step_ == Step::Close) {
            if (!answer_to(mail, Step::Close, zengine::input::InputSessionClosed::zen_name)) {
                return;
            }
            note("session " + std::to_string(state_.session) + " closed");
            finish(mail, true);
            return;
        }
        if (!answer_to(mail, Step::Cleanup, zengine::input::InputSessionClosed::zen_name)) {
            return;
        }
        note("session " + std::to_string(state_.session) + " closed in cleanup");
        finish(mail, false);
    }

    void on(const loom::Refused& r, loom::Mail& mail) {
        const std::optional<loom::PendingAsk> asked = settled(mail);
        if (!asked.has_value()) {
            return;
        }
        if (step_ == Step::Cleanup) {
            note("cleanup refused: " + r.reason);
            finish(mail, false);
            return;
        }
        fail(mail, "refused at " + step_name(step_) + ": " + r.reason);
    }

    /// THE LINK'S OWN WORD about a crossing that did not come back as a far answer: refused
    /// before the far bus, refused by the far bus, unlinked, or lost. Never redefined as a
    /// timeout, and never resent.
    void on(const loom::link::Outcome& o, loom::Mail& mail) {
        const std::optional<loom::PendingAsk> asked = settled(mail);
        if (!asked.has_value()) {
            return;
        }
        const std::string said = "link: " + o.state + " at " + step_name(step_) + " (" + o.shape +
                                 "): " + o.reason;
        const bool link_gone =
            o.state == loom::link::kOutcomeLost || o.state == loom::link::kOutcomeUnlinked;
        if (step_ == Step::Cleanup) {
            note(link_gone ? "cleanup unknown: " + said : "cleanup " + said);
            if (link_gone) {
                note(gone_cleanup());
            }
            finish(mail, false);
            return;
        }
        if (link_gone) {
            note(said);
            if (state_.session > 0) {
                note(gone_cleanup());
            }
            finish(mail, false);
            return;
        }
        fail(mail, said);
    }

    /// Where the run stands, for a host or a suite.
    const std::string& step() const noexcept { return state_.step; }

private:
    enum class Step { Idle, Status, Open, Inject, Inspect, Capture, Fetch, Close, Cleanup };

    static std::string step_name(Step s) {
        switch (s) {
        case Step::Idle:
            return "idle";
        case Step::Status:
            return "status";
        case Step::Open:
            return "open";
        case Step::Inject:
            return "inject";
        case Step::Inspect:
            return "inspect";
        case Step::Capture:
            return "capture";
        case Step::Fetch:
            return "fetch";
        case Step::Close:
            return "close";
        case Step::Cleanup:
            return "cleanup";
        }
        return "?";
    }

    void set_step(Step s) {
        step_ = s;
        state_.step = step_name(s);
    }

    /// Ask the far office through the link, respondent-bound to the link Loom named at Status.
    template <class T>
    void ask(loom::Mail& mail, const std::string& far_role, const T& msg, bool settle = false) {
        const loom::AskOpened opened = book_.open(link_id_, T::zen_name, T::zen_version);
        if (!opened.ok) {
            fail(mail, "this probe's own book is full");
            return;
        }
        (void)mail.send_to_role(loom::link::role_of(run_.link),
                                loom::link::ask_role(far_role, msg, settle), opened.correlation);
    }

    void fetch(loom::Mail& mail) {
        ask(mail, zengine::surface::kSkinRole,
            zengine::surface::SurfaceCaptureChunkRequested{capture_, fetched_});
    }

    /// THE ONE WALL. Loom's answer to a conversation this probe opened, or nothing: an arrival
    /// that is not an attested answer is data, and settles nothing.
    std::optional<loom::PendingAsk> settled(loom::Mail& mail) {
        if (!answer_.valid() || !mail.answers_ask()) {
            ++state_.ignored;
            return std::nullopt;
        }
        std::optional<loom::PendingAsk> asked = book_.settle(mail.correlation(), mail.sender());
        if (!asked.has_value()) {
            ++state_.ignored;
        }
        return asked;
    }

    /// ...and the answer THIS STEP asked for. A genuine answer to the open conversation that is
    /// not what the step expects is the far owner saying something unexpected: the run stops.
    bool answer_to(loom::Mail& mail, Step expected, const char* asked_shape) {
        const std::optional<loom::PendingAsk> asked = settled(mail);
        if (!asked.has_value()) {
            return false;
        }
        if (step_ != expected || asked->shape != asked_shape) {
            fail(mail, "an answer for " + asked->shape + " arrived at step " + step_name(step_) +
                           ", where the probe was waiting on " + step_name(expected));
            return false;
        }
        return true;
    }

    bool write_picture(std::string* why) {
        std::ofstream out(run_.picture, std::ios::binary | std::ios::trunc);
        if (!out) {
            *why = "the picture could not be opened for writing at " + run_.picture;
            return false;
        }
        out.write(picture_.data(), static_cast<std::streamsize>(picture_.size()));
        out.flush();
        if (!out) {
            *why = "the picture could not be written to " + run_.picture;
            return false;
        }
        out.close();
        if (out.fail()) {
            *why = "the picture could not be finished at " + run_.picture;
            return false;
        }
        std::error_code ec;
        const std::uintmax_t on_disk = std::filesystem::file_size(run_.picture, ec);
        if (ec || on_disk != picture_.size()) {
            *why = "the picture at " + run_.picture + " reads back as " +
                   (ec ? ec.message() : std::to_string(on_disk) + " bytes") + ", not " +
                   std::to_string(picture_.size());
            return false;
        }
        return true;
    }

    std::string gone_cleanup() const {
        return "session " + std::to_string(state_.session) +
               " is Workshop's to close: its guest door closes a lost guest's input session when "
               "it sees the connection end; nothing was closed from here";
    }

    void note(std::string line) {
        if (!report_.empty()) {
            report_ += "; ";
        }
        report_ += line;
    }

    /// A step went wrong. With a session open and a link to speak through, close it first and
    /// say both; otherwise stop now.
    void fail(loom::Mail& mail, std::string why) {
        failure_ = why;
        note(why);
        if (state_.session > 0 && step_ != Step::Cleanup && step_ != Step::Close) {
            set_step(Step::Cleanup);
            ask(mail, zengine::input::kInputRole,
                zengine::input::InputSessionClosed{state_.session, 0});
            return;
        }
        finish(mail, false);
    }

    void stop(loom::Mail& mail, std::string why) {
        failure_ = why;
        note(why);
        finish(mail, false);
    }

    void finish(loom::Mail& mail, bool ok) {
        set_step(Step::Idle);
        picture_.clear();
        if (!answer_.valid()) {
            return;
        }
        loom::DeferredAnswer due = std::move(answer_);
        answer_ = loom::DeferredAnswer{};
        (void)loom::answer_deferred(due, mail,
                                    loom::Result{std::string(ok ? "PASS: " : "STOPPED: ") + report_});
    }

    RunProbe run_;
    Step step_ = Step::Idle;
    loom::AskBook book_;
    loom::DeferredAnswer answer_;
    std::string report_;
    std::string failure_;
    std::string picture_;
    std::string picture_format_;
    loom::WeaveId link_id_{};
    std::int64_t far_session_ = 0;
    std::string established_;
    std::int64_t batch_size_ = 0;
    std::int64_t capture_ = 0;
    std::int64_t picture_total_ = 0;
    std::int64_t fetched_ = 0;
};

} // namespace zengine::workshop_probe

#endif // ZENGINE_EXAMPLES_WORKSHOP_PROBE_HPP
