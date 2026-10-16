// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_FACETS_HPP
#define ZENGINE_WORKSHOP_PANE_FACETS_HPP

// The conversations every pane office holds with Workshop, as facets composed with
// `zengine::With` (activation/activation.hpp): its offer, room and manual (`PaneFacet`), its rows
// and the presses on them (`PaneRowsFacet`), and the asks it still waits on (`AskFacet`). The
// shapes they carry leave the weave's own lists; what each calls on the weave, and when, is
// workshop/docs/workshop-panes.md, "The pane facets".

#include "activation/activation.hpp"
#include "component/row_map.hpp"
#include "pane_canvas_rows.hpp"
#include "pane_canvas_vocabulary.hpp"
#include "pane_document.hpp"
#include "pane_menu.hpp"
#include "pane_operation.hpp"
#include "pane_parts.hpp"
#include "pane_vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <zen/weave.hpp>
#include <zen/weave/ask_book.hpp>
#include <zen/weave/dispatch_refusal.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// THE PANE ITS OFFICE OFFERS, ITS ROOM AND ITS MANUAL: the activation the office lives under,
/// the offer (`Offer`, from the weave's `offering()`) on each one and on Workshop's catalog
/// request, the manual from `Page`, and the room granted -- the canvas room while one is held,
/// else the prose room -- with the rows and columns the pane composes for. The weave draws in
/// `on_room`.
template <const char* const& Pane, const char* const& Office, const manual::Manual& Page,
          class Offer = v2::PaneOffered>
class PaneFacet {
public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneCanvasRoom,
                                PaneCanvasRejected, PaneDocumentRequested>;
    using Emit = loom::Emit<Offer, PaneDocumentShown>;

    /// An accepted activation offers the pane, or is the weave's `on_activated`, which announces.
    template <class Self>
    void on(Self& self, const loom::Activated& activated, loom::Mail& mail) {
        if (!activation_.accept(mail, activated)) {
            return;
        }
        if constexpr (requires { self.on_activated(mail); }) {
            self.on_activated(mail);
        } else {
            announce(self, mail);
        }
    }

    template <class Self>
    void on(Self& self, const PaneCatalogRequested&, loom::Mail& mail) {
        if (mail.authored_from_role(pane_menu::kWorkshopRole)) {
            announce(self, mail);
        }
    }

    /// The prose room, kept beside a canvas room, which, granted first, was this beat already.
    template <class Self>
    void on(Self& self, const PaneRoom& room, loom::Mail& mail) {
        if (!from_workshop(mail, room.pane)) {
            return;
        }
        prose_rows_ = room.rows;
        prose_columns_ = room.columns;
        if (on_canvas()) {
            return;
        }
        fit();
        self.on_room(mail);
    }

    /// The pane's own canvas: while it holds one, its rows stand on the room's lattice. A weave
    /// that treats a canvas grant apart says so in `on_canvas_room`.
    template <class Self>
    void on(Self& self, const PaneCanvasRoom& room, loom::Mail& mail) {
        if (!from_workshop(mail, room.pane)) {
            return;
        }
        canvas_ = room;
        fit();
        if constexpr (requires { self.on_canvas_room(mail); }) {
            self.on_canvas_room(mail);
        } else {
            self.on_room(mail);
        }
    }

    /// A refused picture leaves the last good one showing; a weave that wants the words takes
    /// them in `on_rejected`.
    template <class Self>
    void on([[maybe_unused]] Self& self, [[maybe_unused]] const PaneCanvasRejected& rejected,
            [[maybe_unused]] loom::Mail& mail) {
        if constexpr (requires { self.on_rejected(rejected, mail); }) {
            if (from_workshop(mail, rejected.pane)) {
                self.on_rejected(rejected, mail);
            }
        }
    }

    template <class Self>
    void on(Self&, const PaneDocumentRequested& ask, loom::Mail& mail) {
        (void)mail.answer(pane_document(ask, Pane, Page));
    }

    /// THE OFFER, from the office to Workshop, then the weave's own `offered` (its rows, its
    /// settings), when it has one.
    template <class Self>
    void announce(Self& self, loom::Mail& mail) {
        (void)mail.as_role(Office).send_to_role(pane_menu::kWorkshopRole, self.offering());
        if constexpr (requires { self.offered(mail); }) {
            self.offered(mail);
        }
    }

    /// Whether this delivery is Workshop's own word about this office's pane.
    static bool from_workshop(const loom::Mail& mail, const std::string& pane) {
        return pane == Pane && mail.authored_from_role(pane_menu::kWorkshopRole);
    }

    const ActivationCursor& activation() const noexcept { return activation_; }
    /// A room was granted in this life, prose or canvas.
    bool granted() const noexcept { return granted_; }
    /// The rows and columns the pane composes for: the canvas lattice's, else the prose room's.
    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }
    std::int64_t prose_rows() const noexcept { return prose_rows_; }
    std::int64_t prose_columns() const noexcept { return prose_columns_; }
    bool on_canvas() const noexcept {
        return canvas_.grant > 0 && canvas_.width > 0 && canvas_.height > 0;
    }
    const PaneCanvasRoom& canvas_room() const noexcept { return canvas_; }

    /// The room composed for, measured again: after a canvas room, or a setting that moves rows.
    void fit() {
        const CanvasRows lattice = canvas_rows(canvas_);
        rows_ = on_canvas() ? lattice.rows : prose_rows_;
        columns_ = on_canvas() ? lattice.columns : prose_columns_;
        granted_ = true;
    }

private:
    ActivationCursor activation_;
    bool granted_ = false;
    std::int64_t rows_ = 0, columns_ = 0;
    std::int64_t prose_rows_ = 0, prose_columns_ = 0;
    PaneCanvasRoom canvas_;
};

/// ONE FIRST-BUTTON PRESS ON A SHOWN ROW OF THE PANE'S CURRENT PICTURE, as a prose press names
/// it, and whether the keys were already reaching the pane when it was made.
struct RowPress {
    std::int64_t row = 0, column = 0;
    bool keys_went_here = false;
};

/// A PANE THAT SAYS ROWS: `PaneFacet`, the rows being composed, what each row and run of
/// columns means (`component::RowMap<Meaning>`), the caret, and the presses on the picture. Its
/// rows go as the pane's canvas picture, numbered by what they mean, while it holds a canvas
/// room, and as prose rows with the caret beside them otherwise; a press is read against the
/// picture it was aimed at, and a third-button press that means nothing to the pane is passed
/// back to Workshop.
template <const char* const& Pane, const char* const& Office, const manual::Manual& Page,
          class Meaning, class Offer = v2::PaneOffered>
class PaneRowsFacet : public PaneFacet<Pane, Office, Page, Offer> {
    using Room = PaneFacet<Pane, Office, Page, Offer>;

public:
    using Accept = loom::Accept<loom::Activated, PaneCatalogRequested, PaneRoom, PaneCanvasRoom,
                                PaneCanvasRejected, PaneDocumentRequested, PaneCanvasPointer>;
    using Emit = loom::Emit<Offer, PaneDocumentShown, v5::PaneContent, v6::PaneCanvasContent,
                            PaneCaret, PanePassRequested>;
    using Room::on;

    /// A press, the wheel and the third button on the canvas: `press`, `wheel` and `right_press`
    /// on the weave; a press about a picture whose rows have moved since, or another room's, is
    /// its `fenced`, never resolved against whatever row now stands there. Motion, release and
    /// loss mean nothing here.
    template <class Self>
    void on([[maybe_unused]] Self& self, const PaneCanvasPointer& event, loom::Mail& mail) {
        if (!Room::from_workshop(mail, event.pane) || !this->on_canvas()) {
            return;
        }
        const PaneCanvasRoom& room = this->canvas_room();
        if (event.phase == canvas_pointer::kWheel) {
            if constexpr (requires { self.wheel(event.dy, mail); }) {
                if (event.grant == room.grant) {
                    self.wheel(event.dy, mail);
                }
            }
            return;
        }
        const RowCell at = row_cell_at(canvas_rows(room), event.x, event.y);
        if (event.phase != canvas_pointer::kPress ||
            ((event.button != 1 || !at.shown) && event.button != 3)) {
            return;
        }
        if (!pictures_.current(event.grant, event.picture)) {
            if constexpr (requires { self.fenced(mail); }) {
                self.fenced(mail);
            }
            return;
        }
        const RowPress press{at.row, at.column, event.keys_went_here};
        if (event.button == 1) {
            if constexpr (requires { self.press(press, mail); }) {
                self.press(press, mail);
            }
            return;
        }
        const Meaning* meant = at.shown ? map_.at(at.row, at.column) : nullptr;
        if constexpr (requires { self.right_press(*meant, press, mail); }) {
            if (meant != nullptr && self.right_press(*meant, press, mail)) {
                return;
            }
        }
        pass_back(mail);
    }

    /// THAT PRESS WAS NOT THE PANE'S: Workshop's own pane menu opens where it was made.
    void pass_back(loom::Mail& mail) { (void)pane_menu::pass_back(mail, Office, Pane); }

    /// BEGIN A PICTURE: no rows, no meanings, no caret. What the last picture meant is kept aside
    /// until `publish` compares the two.
    void begin() {
        map_.begin();
        composing_.clear();
        caret_ = RowsCaret{};
    }
    /// ONE ROW, while the room has one for it, and what it means when it means something. False,
    /// recording nothing, once the room has run out: a row nobody can see names nothing.
    bool push(surface::SurfaceTextRow row, std::optional<Meaning> meaning = std::nullopt) {
        if (next_row() >= this->rows()) {
            return false;
        }
        if (meaning) {
            map_.row(next_row(), std::move(*meaning));
        }
        composing_.push_back(std::move(row));
        return true;
    }
    /// The row the next `push` lands on.
    std::int64_t next_row() const noexcept { return static_cast<std::int64_t>(composing_.size()); }
    std::vector<surface::SurfaceTextRow>& composing() noexcept { return composing_; }
    component::RowMap<Meaning>& map() noexcept { return map_; }
    const component::RowMap<Meaning>& map() const noexcept { return map_; }
    /// Where the caret and the selection stand in the rows being composed; none by default.
    RowsCaret& caret() noexcept { return caret_; }

    /// NOTHING IS SAID THIS BEAT, so no row of it names anything.
    void hold() { (void)map_.settle(); }

    /// THE PICTURE COMPOSED, numbered by what its rows mean, each part named `name_of(meaning)`
    /// and available as `available_of(meaning)` says; the caret goes beside prose rows when
    /// `with_caret`.
    template <class NameOf, class AvailableOf>
    void publish(loom::Mail& mail, NameOf&& name_of, AvailableOf&& available_of,
                 bool with_caret = true) {
        const std::int64_t meaning = map_.settle();
        std::vector<v2::PaneRowPart> parts =
            row_parts(map_, this->columns(), name_of, available_of);
        std::vector<surface::SurfaceTextRow> rows = std::move(composing_);
        composing_.clear();
        send(mail, meaning, std::move(rows), std::move(parts), with_caret);
    }

    /// ...or rows composed elsewhere, numbered `meaning`, with their parts.
    void send(loom::Mail& mail, std::int64_t meaning, std::vector<surface::SurfaceTextRow> rows,
              std::vector<v2::PaneRowPart> parts, bool with_caret = true) {
        ++published_;
        if (this->on_canvas()) {
            const PaneCanvasRoom& room = this->canvas_room();
            (void)mail.as_role(Office).send_to_role(
                pane_menu::kWorkshopRole,
                rows_picture(room, pictures_.next(room, meaning), rows, parts, caret_));
            return;
        }
        (void)mail.as_role(Office).send_to_role(
            pane_menu::kWorkshopRole,
            v5::PaneContent{Pane, std::move(rows), 0, meaning, std::move(parts)});
        if (with_caret) {
            (void)mail.as_role(Office).send_to_role(
                pane_menu::kWorkshopRole,
                PaneCaret{Pane, caret_.row, caret_.column, caret_.sel_begin_row,
                          caret_.sel_begin_col, caret_.sel_end_row, caret_.sel_end_col});
        }
    }

    /// How many pictures the pane has published, so a handler can tell whether the act it ran
    /// already said its rows.
    std::uint64_t published() const noexcept { return published_; }
    /// The pictures numbered in this room, and which a press may still name.
    CanvasPictures& pictures() noexcept { return pictures_; }

private:
    std::vector<surface::SurfaceTextRow> composing_;
    component::RowMap<Meaning> map_;
    CanvasPictures pictures_;
    RowsCaret caret_;
    std::uint64_t published_ = 0;
};

/// THE ASKS A PANE STILL WAITS ON, in one bounded table numbered from its one correlation
/// sequence (`loom::AskBook`), each holding the pane's record of the act, `Act`. An approval asks
/// Workshop whether a gesture's actor may do an act, at most `Approvals` at once; an ask is any
/// other request the pane expects one answer to, or a send whose refusal it must hear. Workshop's
/// answer goes to the weave's `on_approval`, Loom's refusal of exactly that send to its
/// `on_refused`. Image-local: a successor asked nothing, so no earlier answer settles anything.
template <const char* const& Pane, const char* const& Office, class Act,
          std::size_t Approvals = 8, std::size_t Capacity = 32>
class AskFacet {
    static_assert(Approvals > 0 && Approvals <= Capacity, "AskFacet: the approvals fit the table");

public:
    using Accept = loom::Accept<PaneOperationAnswered, loom::DispatchRefused>;
    using Emit = loom::Emit<v2::PaneOperationRequested>;

    /// What became of an ask: held until it is answered, refused because the table (or its share
    /// for approvals) is full, or refused because the send queued nothing. Only a held one waits.
    enum class Asked { kHeld, kFull, kUnqueued };

    static constexpr std::size_t kApprovals = Approvals;

    /// ASK WORKSHOP WHETHER THE ACTOR OF `gesture` MAY DO AN ACT OF `classes` -- `subject` is what
    /// a class `open` opens -- and hold `act` until it answers. One approval stands for the act's
    /// every later beat under that gesture.
    Asked approve(loom::Mail& mail, Act act, std::uint64_t gesture,
                  std::vector<std::string> classes, std::string subject = {}) {
        if (approvals() >= Approvals) {
            return Asked::kFull;
        }
        v2::PaneOperationRequested asked;
        asked.pane = Pane;
        asked.gesture = static_cast<std::int64_t>(gesture);
        asked.classes = std::move(classes);
        asked.subject = std::move(subject);
        return ask(mail, pane_menu::kWorkshopRole, asked, std::move(act), /*approval=*/true);
    }

    /// SEND `request` TO THE OFFICE `role`, as the pane's office, under a fresh correlation, and
    /// hold `act` until its answer or its refusal.
    template <class Request>
    Asked ask(loom::Mail& mail, const char* role, const Request& request, Act act,
              bool approval = false) {
        const loom::AskOpened opened =
            book_.open_to_role(role, Request::zen_name, Request::zen_version);
        if (!opened) {
            return Asked::kFull;
        }
        const loom::Ticket attempt =
            mail.as_role(Office).send_to_role(role, request, opened.correlation);
        return keep(opened, attempt, std::move(act), approval);
    }

    /// HOLD AN ASK THE PANE SENDS ITSELF, of `shape` at `version` to `role`: `send(correlation)`
    /// sends it and returns its ticket.
    template <class Send>
    Asked ask_with(const char* role, std::string shape, std::uint32_t version, Act act,
                   Send&& send) {
        const loom::AskOpened opened = book_.open_to_role(role, std::move(shape), version);
        if (!opened) {
            return Asked::kFull;
        }
        return keep(opened, send(opened.correlation), std::move(act), false);
    }

    /// A number from the pane's one sequence, for a send that is not an ask.
    std::uint64_t mint() { return book_.mint_correlation(); }

    /// THE ASK THIS ANSWER SETTLES, taken from the table: Loom's answer, under the correlation of
    /// an ask held here, whose act `which` accepts. None for a stale, duplicate or forged answer,
    /// or one about another ask, which then stays held.
    template <class Which>
    std::optional<Act> answered(const loom::Mail& mail, Which&& which) {
        if (!mail.answers_ask()) {
            return std::nullopt;
        }
        const loom::PendingAsk* held = book_.match(mail.correlation(), mail.sender());
        const auto at = held == nullptr ? acts_.end() : find_id(held->id);
        if (at == acts_.end() || !which(at->act)) {
            return std::nullopt;
        }
        return take(at);
    }

    /// Is an ask whose act `which` accepts still held?
    template <class Which>
    bool holds(Which&& which) const {
        return find(which) != nullptr;
    }
    /// The first held act `which` accepts, or nullptr.
    template <class Which>
    const Act* find(Which&& which) const {
        for (const Entry& e : acts_) {
            if (which(e.act)) {
                return &e.act;
            }
        }
        return nullptr;
    }
    /// How many held acts `which` accepts.
    template <class Which>
    std::size_t count(Which&& which) const {
        return static_cast<std::size_t>(std::count_if(
            acts_.begin(), acts_.end(), [&](const Entry& e) { return which(e.act); }));
    }
    /// STOP WAITING ON EVERY ASK WHOSE ACT `which` ACCEPTS, locally: the far end is told nothing,
    /// and an answer that still arrives settles nothing. Returns how many were forgotten.
    template <class Which>
    std::size_t forget(Which&& which) {
        std::size_t gone = 0;
        for (auto at = acts_.begin(); at != acts_.end();) {
            if (which(at->act)) {
                (void)book_.forget(at->id);
                at = acts_.erase(at);
                ++gone;
            } else {
                ++at;
            }
        }
        return gone;
    }
    /// How many approvals are held.
    std::size_t approvals() const {
        return static_cast<std::size_t>(
            std::count_if(acts_.begin(), acts_.end(), [](const Entry& e) { return e.approval; }));
    }

    /// WORKSHOP'S WORD ON AN ACT IT WAS ASKED ABOUT: Loom's answer, under the correlation of an
    /// ask of `PaneOperationRequested` held here, settles exactly that ask.
    template <class Self>
    void on(Self& self, const PaneOperationAnswered& answer, loom::Mail& mail) {
        if (!mail.answers_ask()) {
            return;
        }
        const loom::PendingAsk* held = book_.match(mail.correlation(), mail.sender());
        const auto at = held == nullptr ? acts_.end() : find_id(held->id);
        if (at == acts_.end() || held->shape != PaneOperationRequested::zen_name) {
            return;
        }
        self.on_approval(take(at), answer, mail);
    }

    /// LOOM'S WORD THAT ONE OF THE PANE'S SENDS WAS REFUSED BEFORE ANY HANDLER RAN. Provenance
    /// first, then the exact attempt, office and shape against the asks held, and only the matched
    /// one is taken. A forged, stale, duplicate or unmatched notice settles nothing; delivered
    /// silence is not a refusal, and the ask stays held.
    template <class Self>
    void on(Self& self, const loom::DispatchRefused& refused, loom::Mail& mail) {
        if (!mail.dispatch_refused()) {
            return;
        }
        const loom::Ticket attempt = refused.refused_attempt();
        const loom::PendingAsk* held =
            attempt.valid() ? book_.match_attempt(attempt.seq) : nullptr;
        if (held == nullptr || held->role != refused.role || !refused.target.empty() ||
            held->shape != refused.shape ||
            static_cast<std::int64_t>(held->version) != refused.version) {
            return;
        }
        const auto at = find_id(held->id);
        if (at != acts_.end()) {
            self.on_refused(take(at), refused, mail);
        }
    }

private:
    struct Entry {
        std::uint64_t id = 0;
        bool approval = false;
        Act act;
    };

    Asked keep(const loom::AskOpened& opened, loom::Ticket attempt, Act act, bool approval) {
        if (!attempt.valid() || !book_.bind_attempt(opened.id, attempt.seq)) {
            (void)book_.forget(opened.id);
            return Asked::kUnqueued;
        }
        acts_.push_back(Entry{opened.id, approval, std::move(act)});
        return Asked::kHeld;
    }

    typename std::vector<Entry>::iterator find_id(std::uint64_t id) {
        return std::find_if(acts_.begin(), acts_.end(),
                            [id](const Entry& e) { return e.id == id; });
    }

    Act take(typename std::vector<Entry>::iterator at) {
        (void)book_.forget(at->id);
        Act act = std::move(at->act);
        acts_.erase(at);
        return act;
    }

    loom::AskBook book_{Capacity};
    std::vector<Entry> acts_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_FACETS_HPP
