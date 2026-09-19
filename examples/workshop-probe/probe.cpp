// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE WORKSHOP PROBE -- one reusable testing journey through two real Loom hosts, as a
// loadable weave a supplied host boots.
//
// WHAT IT IS. An ordinary participant of the EXTERNAL host: it knows a link's office
// (`loom.link.<name>`, the supplied host's own door to a running Workshop) and a handful of
// Workshop shapes, and when an operator tells it to run, it drives one journey across the link:
//
//     1. open an input session with Workshop's Input owner          (zengine.input)
//     2. inject a key chord -- Ctrl+P by default, the desktop's own Pane Manager launch
//     3. ask the guest door for the connection inventory -- the one this session is in,
//        as the host established it                                  (zengine.guests)
//     4. take a picture of what Workshop then presents, after the frame it last saw,
//        and fetch it chunk by chunk into a file beside this host     (zengine.skin)
//     5. close the session, and answer the operator with what each step came to
//
// Every step is an ORDINARY ask -- `loom.link.Ask` to the link, settled on this weave's own
// `loom::AskBook` by the link's stamp and the correlation it chose -- and every outcome is
// what the far owner SAID: an `InputInjected` from the Input weave, a `zen.AcceptedShapes`
// from the desktop, a `SurfaceCaptured` from the Skin, or a `loom.link.Outcome` from the link
// about a crossing that did not come back as an answer. Nothing here infers success from a
// picture or from admission: the semantic checks are the owners' answers.
//
// WHAT IT IS NOT. Not a host: it holds no socket and no bus; the link does. Not privileged:
// what it may say to the link is one rule the operator grants at the console
// (`authority allow probe loom.link.Ask v1 -> role loom.link.workshop`), and what the link's
// session may say to Workshop is Workshop's own guests file. Not a framework: one journey,
// parameterised by the message that starts it, with room beside it for the next.
//
// BUILT AGAINST INSTALLED PACKAGES ONLY: `find_package(loom)` for the weave surface and the
// link envelope, `find_package(zengine)` for the Input and Surface vocabularies. Nothing here
// reaches a Workshop header that is not published.

#include "input/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/guest_seam_vocabulary.hpp"

#include <zen/bridge/link.hpp>
#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/standard_shapes.hpp>

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace input = zengine::input;
namespace surface = zengine::surface;

/// RUN THE JOURNEY. Sent by the operator (the console's `send <probe> RunProbe 1 ...`), and
/// answered when the journey settles or stops. Every parameter has a default a first run can
/// take: the link name, the chord, the far offices, the file the picture lands in.
struct RunProbe {
    std::string link = "workshop";   ///< the supplied host's link name (`loom.link.<link>`)
    std::string text;                ///< text to type after the chord, if any
    std::int64_t scancode = input::scan::kP;      ///< the chord's key (Ctrl+P: Pane Manager)
    std::int64_t modifiers = input::mod::kCtrl;
    std::string inspect = "zengine.guests";       ///< the office asked for the inventory
    std::string picture = "workshop-probe.bmp";   ///< where the fetched picture is written
    ZEN_SHAPE(RunProbe, 1, ZEN_FIELD(link), ZEN_FIELD(text), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(inspect), ZEN_FIELD(picture));
};

/// What the last run came to, for a poke and for the answer.
struct ProbeState {
    std::int64_t runs = 0;
    std::int64_t session = 0;
    std::int64_t injected = 0;
    std::int64_t accepted_shapes = 0;
    std::int64_t capture = 0;
    std::int64_t picture_bytes = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ProbeState, 1, ZEN_FIELD(runs), ZEN_FIELD(session), ZEN_FIELD(injected),
              ZEN_FIELD(accepted_shapes), ZEN_FIELD(capture), ZEN_FIELD(picture_bytes));
};

class ProbeWeave final
    : public loom::WeaveBase<ProbeWeave, ProbeState,
                             loom::Accept<RunProbe, input::InputSessionOpened,
                                          input::InputInjected, zengine::workshop::GuestConnections,
                                          surface::SurfaceCaptured, surface::SurfaceCaptureChunk,
                                          loom::Ack, loom::Refused, loom::link::Outcome>,
                             loom::Emit<loom::link::Ask, loom::Result>> {
public:
    ProbeWeave() : book_(4) {}

    // ---- the operator's door ---------------------------------------------------------

    void on(const RunProbe& run, loom::Mail& mail) {
        if (answer_.valid()) {
            (void)mail.answer(loom::Refused{"a run is already in flight; wait for its answer"});
            return;
        }
        run_ = run;
        ++state_.runs;
        state_.session = 0;
        state_.injected = 0;
        state_.accepted_shapes = 0;
        state_.capture = 0;
        state_.picture_bytes = 0;
        picture_.clear();
        report_.clear();
        answer_ = mail.defer_answer();
        if (!answer_.valid()) {
            (void)mail.answer(loom::Refused{"this delivery cannot defer its answer"});
            return;
        }
        step_ = Step::Open;
        ask(mail, input::kInputRole, input::InputSessionRequested{"workshop probe"},
            input::InputSessionRequested::zen_name);
    }

    // ---- the far owners' answers, each settling one conversation on the book -------------

    void on(const input::InputSessionOpened& opened, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        state_.session = opened.session;
        note("session " + std::to_string(opened.session) + " opened by zengine.input");
        step_ = Step::Inject;
        input::InjectInput batch;
        batch.session = opened.session;
        input::InjectedEvent down;
        down.kind = "KeyPressed";
        down.scancode = run_.scancode;
        down.modifiers = run_.modifiers;
        input::InjectedEvent up = down;
        up.kind = "KeyReleased";
        batch.events.push_back(down);
        batch.events.push_back(up);
        if (!run_.text.empty()) {
            input::InjectedEvent typed;
            typed.kind = "TextEntered";
            typed.text = run_.text;
            batch.events.push_back(typed);
        }
        ask(mail, input::kInputRole, batch, input::InjectInput::zen_name);
    }

    void on(const input::InputInjected& done, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        state_.injected = done.admitted;
        note("injected " + std::to_string(done.admitted) + " moment(s), session seq " +
             std::to_string(done.first_seq) + ".." + std::to_string(done.last_seq));
        step_ = Step::Inspect;
        ask(mail, run_.inspect, zengine::workshop::GuestConnectionsRequested{},
            zengine::workshop::GuestConnectionsRequested::zen_name);
    }

    /// THE INVENTORY, FROM ITS OWNER: this session is in it under the name the far host
    /// established -- read off the owner's answer, never inferred from being admitted.
    void on(const zengine::workshop::GuestConnections& said, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        state_.accepted_shapes = static_cast<std::int64_t>(said.rows.size());
        std::string mine = "not listed";
        for (const zengine::workshop::GuestConnection& c : said.rows) {
            if (c.state == "admitted") {
                mine = "'" + c.established + "' (claimed '" + c.claimed + "', weave " +
                       std::to_string(c.session) + ")";
            }
        }
        note(run_.inspect + " lists " + std::to_string(said.rows.size()) +
             " connection(s); this session is " + mine);
        step_ = Step::Capture;
        // AFTER THE FRAME THE INJECTION'S CONSUMER PAINTED. The chord's consumer repaints in a
        // delivery queued before this request reaches the Skin (the bus is FIFO), so the first
        // capture at "now" already sees it; asking after the frame this probe last knew makes
        // the ordering explicit and lets a second run wait for a later paint.
        surface::SurfaceCaptureRequested want;
        want.after_frame = last_frame_;
        ask(mail, surface::kSkinRole, want, surface::SurfaceCaptureRequested::zen_name);
    }

    void on(const surface::SurfaceCaptured& c, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        if (!c.ok) {
            note("capture refused: " + c.refusal);
            finish(mail, false);
            return;
        }
        state_.capture = c.capture;
        last_frame_ = c.frame;
        picture_total_ = c.bytes;
        picture_format_ = c.format;
        note("capture " + std::to_string(c.capture) + " at frame " + std::to_string(c.frame) +
             ": " + std::to_string(c.width) + "x" + std::to_string(c.height) + " " + c.format +
             ", " + std::to_string(c.bytes) + " bytes");
        step_ = Step::Fetch;
        ask(mail, surface::kSkinRole, surface::SurfaceCaptureChunkRequested{c.capture, 0},
            surface::SurfaceCaptureChunkRequested::zen_name);
    }

    void on(const surface::SurfaceCaptureChunk& chunk, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        picture_.append(chunk.data.begin(), chunk.data.end());
        const std::int64_t next = chunk.offset + static_cast<std::int64_t>(chunk.data.size());
        if (next < chunk.total) {
            ask(mail, surface::kSkinRole,
                surface::SurfaceCaptureChunkRequested{chunk.capture, next},
                surface::SurfaceCaptureChunkRequested::zen_name);
            return;
        }
        state_.picture_bytes = static_cast<std::int64_t>(picture_.size());
        std::ofstream out(run_.picture, std::ios::binary | std::ios::trunc);
        const bool written = out && out.write(picture_.data(),
                                              static_cast<std::streamsize>(picture_.size()));
        note(std::string(written ? "picture written to " : "picture could not be written to ") +
             run_.picture + " (" + std::to_string(picture_.size()) + " of " +
             std::to_string(picture_total_) + " bytes, " + picture_format_ + ")");
        step_ = Step::Close;
        ask(mail, input::kInputRole, input::InputSessionClosed{state_.session, 0},
            input::InputSessionClosed::zen_name);
    }

    void on(const loom::Ack&, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        if (step_ == Step::Close) {
            note("session " + std::to_string(state_.session) + " closed");
            finish(mail, true);
        }
    }

    void on(const loom::Refused& r, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        note("refused at " + step_name() + ": " + r.reason);
        finish(mail, false);
    }

    /// THE LINK'S OWN WORD about a crossing that did not come back as an answer: refused
    /// before the far bus, refused by the far bus, unlinked, or lost. Never redefined as a
    /// timeout, and never resent.
    void on(const loom::link::Outcome& o, loom::Mail& mail) {
        if (!settle(mail)) {
            return;
        }
        note("link: " + o.state + " at " + step_name() + " (" + o.shape + "): " + o.reason);
        finish(mail, false);
    }

private:
    enum class Step { Idle, Open, Inject, Inspect, Capture, Fetch, Close };

    template <class T>
    void ask(loom::Mail& mail, const std::string& far_role, const T& msg, const char* shape) {
        const loom::AskOpened opened = book_.open_to_role(loom::link::role_of(run_.link), shape, 1);
        if (!opened.ok) {
            note("this probe's own book is full");
            finish(mail, false);
            return;
        }
        (void)mail.send_to_role(loom::link::role_of(run_.link), loom::link::ask_role(far_role, msg),
                                opened.correlation);
    }

    /// Settled on the LINK's stamp and the correlation this probe chose; anything else is
    /// data this probe did not ask for and does not act on.
    bool settle(loom::Mail& mail) {
        return book_.settle(mail.correlation(), mail.sender()).has_value();
    }

    void note(std::string line) {
        if (!report_.empty()) {
            report_ += "; ";
        }
        report_ += line;
    }

    std::string step_name() const {
        switch (step_) {
        case Step::Idle:
            return "idle";
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
        }
        return "?";
    }

    void finish(loom::Mail& mail, bool ok) {
        step_ = Step::Idle;
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
    std::string picture_;
    std::string picture_format_;
    std::int64_t picture_total_ = 0;
    std::int64_t last_frame_ = -1;
};

} // namespace

ZEN_EXPORT_WEAVE(ProbeWeave)
