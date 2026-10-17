// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_PANE_OUTPUT_READER_HPP
#define ZENGINE_BUILDER_PANE_OUTPUT_READER_HPP

// The Builder pane's output reader (WL-OUT-04): the members of `BuilderPaneWeave` that bind
// the reader to one build's operation, move its view, ask the tool for the page it starts at,
// and say that page with the controls drawn under it.
// A part of `builder/builder-pane/pane.cpp`, which includes it after `BuilderPaneWeave` and its
// aliases; nothing else includes it.
// Builder law: agents/realization.md

#include "builder/builder-pane/vocabulary.hpp"
#include "builder/vocabulary.hpp"
#include "surface/vocabulary.hpp"
#include "workshop/pane_text.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

/// OPEN THE READER ON THE OPERATION THE TOOL'S PICTURE IS ABOUT, from its first line. It is
/// bound to that operation's number from here on: a later build, a new choice or a catalog
/// moves nothing it shows, and the tool's words about another operation are never shown
/// under this one's header.
// WL-OUT-04 -- agents/workshop/build-output.md
inline void BuilderPaneWeave::open_output(loom::Mail& mail) {
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

inline void BuilderPaneWeave::bind_output(std::int64_t op, loom::Mail& mail) {
    const bool was_open = output_.open;
    (void)asks().forget(of_kind(Act::kPage));
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

inline void BuilderPaneWeave::close_output(loom::Mail& mail) {
    (void)asks().forget(of_kind(Act::kPage));
    output_ = Output{};
    declare(mail);
    say(mail);
}

/// ONE OF THE READER'S ROWS. Every id moves the view or closes it, and asks the tool again
/// for the page the view now starts at; none reaches a build.
inline void BuilderPaneWeave::read_output(const std::string& id, loom::Mail& mail) {
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
        const std::int64_t step = columns() > 8 ? columns() / 2 : 4;
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
inline void BuilderPaneWeave::scroll(std::int64_t by, loom::Mail& mail) {
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
inline void BuilderPaneWeave::ask_page(loom::Mail& mail) {
    const std::int64_t lines = output_body_rows() > 0 ? output_body_rows() : 1;
    (void)asks().forget(of_kind(Act::kPage));
    if (asks().ask(mail, builder::kBuilderRole,
                   builder::BuildOutputRequested{output_.op, output_.top, lines},
                   asking(Act::kPage)) != Asks::Asked::kHeld) {
        notice_ = "output #" + std::to_string(output_.op) +
                  ": the page was not read -- nothing was queued to " + builder::kBuilderRole;
    }
}

/// The rows the reader's lines may spend: the room, less its header, less a notice, less
/// the control strip the reader draws under them.
inline std::int64_t BuilderPaneWeave::output_body_rows() const {
    return rows() - 1 - (notice_.empty() ? 0 : 1) - room().strip_rows(output_controls());
}

/// THE READER'S ROWS. One header naming the operation, how it ended and which lines show;
/// then the lines, each on ONE row, cut at the width with a mark and panned by the arrows,
/// so a caret line stays under the line it points into and a long command echo costs one
/// row. Every line is spelled in ASCII (`ascii_spelling`), and the header says how many
/// characters that spelled on the rows shown. A gap the tool no longer keeps is a row of its
/// own, never two ends shown as one.
// WL-OUT-04 -- agents/workshop/build-output.md
inline void BuilderPaneWeave::say_output() {
    const builder::BuildOutputSaid& p = output_.page;
    const std::string number = "#" + std::to_string(output_.op);
    std::string head = "output " + number;
    if (!output_.heard) {
        const bool asking_page = asks().holds(of_kind(Act::kPage));
        push_row(head + (asking_page ? " -- asking the Builder" : " -- no page read"),
                 asking_page ? surface::role::kAccent : surface::role::kAlert);
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

inline std::vector<BuilderPaneWeave::ControlRow> BuilderPaneWeave::output_controls() const {
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

} // namespace

#endif // ZENGINE_BUILDER_PANE_OUTPUT_READER_HPP
