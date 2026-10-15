// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_CENSUS_HPP
#define ZENGINE_TESTS_CENSUS_HPP

// The action census and the manual witness, shared by the pane suites: each suite registers one
// driver per office (`CensusDriverOf`) that walks its panes through their modes, and the census
// case in tests/test_workshop_panes_builder.cpp holds each office's lines to its pinned ones,
// `tests/actions.txt` or `tests/actions/<office>.txt`. A pane's manual is asked as any weave asks
// it and held to the ids its rows declare. Reference: manual/docs/manual.md.

#include "workshop_support.hpp"

#include "manual/manual.hpp"
#include "workshop/pane_document.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/// ONE CAUSE AS THE CENSUS SPELLS IT: `<shape>/<version>`, then its subject -- `@<prefix>` for the
/// part a press lands on, `><office>/<pane>` for one pane.
inline std::string census_cause(const ActionCause& c) {
    std::string out = c.shape + "/" + std::to_string(c.version);
    if (c.subject.rfind(kCauseOnPart, 0) == 0) out += "@" + c.subject.substr(kCauseOnPart.size());
    if (c.subject.rfind(kCauseToPane, 0) == 0) out += ">" + c.subject.substr(kCauseToPane.size());
    return out;
}

/// ...AND A ROW'S CAUSES: each, `,`-joined, in the order declared; `-` for none.
inline std::string census_causes(const std::vector<ActionCause>& causes) {
    std::string out;
    for (const ActionCause& c : causes) out += (out.empty() ? "" : ",") + census_cause(c);
    return out.empty() ? std::string("-") : out;
}

/// ONE CENSUS LINE: `<office> <mode> <id> <default key, as a keymap file spells it> <causes>
/// <enters> <label>`, the mode `-` for a pane that declares none.
inline std::string census_line(const std::string& office, const std::string& mode,
                               const std::string& id, std::int64_t scancode,
                               std::int64_t modifiers, const std::vector<ActionCause>& causes,
                               const std::string& enters, const std::string& label) {
    const Gesture g{scancode, modifiers};
    return office + " " + (mode.empty() ? std::string("-") : mode) + " " + id + " " +
           (is_bound(g) ? gesture_word(g) : std::string("unbound")) + " " +
           census_causes(causes) + " " + (enters.empty() ? std::string("-") : enters) + " " +
           label;
}

/// ...A HELD ROW'S, read from what Workshop holds of the declaration.
inline std::string census_line(const std::string& office, const std::string& mode,
                               const v3::PaneActionRow& a) {
    return census_line(office, mode, a.id, a.scancode, a.modifiers, a.causes, a.enters, a.label);
}

/// THE OFFICE A CENSUS LINE IS ABOUT: its first word.
inline std::string census_office(const std::string& line) {
    return line.substr(0, line.find(' '));
}

/// EVERY PINNED CENSUS FILE: `tests/actions.txt`, then each `tests/actions/<office>.txt` by name.
inline std::vector<std::filesystem::path> census_files() {
    namespace fs = std::filesystem;
    const fs::path pinned = ZENGINE_ACTIONS;
    std::vector<fs::path> more;
    std::error_code error;
    for (const fs::directory_entry& e :
         fs::directory_iterator(pinned.parent_path() / "actions", error)) {
        if (e.path().extension() == ".txt") more.push_back(e.path());
    }
    std::sort(more.begin(), more.end());
    std::vector<fs::path> out{pinned};
    out.insert(out.end(), more.begin(), more.end());
    return out;
}

/// THE ROWS ONE PINNED FILE HOLDS, its comments and blank lines left out.
inline std::vector<std::string> census_lines_of(const std::filesystem::path& file) {
    std::vector<std::string> out;
    std::ifstream in(file);
    REQUIRE_MESSAGE(in.good(), file.generic_string() << " could not be read");
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line[0] != '#') out.push_back(line);
    }
    return out;
}

/// EVERY PINNED ROW, file by file in `census_files` order.
inline std::vector<std::string> census_pinned() {
    std::vector<std::string> out;
    for (const std::filesystem::path& file : census_files()) {
        for (std::string& line : census_lines_of(file)) out.push_back(std::move(line));
    }
    return out;
}

/// ...AND ONE OFFICE'S, in the order its file holds them.
inline std::vector<std::string> census_pinned_of(const std::string& office) {
    std::vector<std::string> out;
    for (std::string& line : census_pinned()) {
        if (census_office(line) == office) out.push_back(std::move(line));
    }
    return out;
}

/// EACH MODE'S CAUSES AS THE CENSUS LINES SAY THEM: of `lines` for `office`, by mode, every cause
/// token once.
inline std::map<std::string, std::set<std::string>> census_causes_by_mode(
    const std::vector<std::string>& lines, const std::string& office) {
    std::map<std::string, std::set<std::string>> out;
    for (const std::string& line : lines) {
        std::istringstream in(line);
        std::string who, mode, id, key, causes;
        in >> who >> mode >> id >> key >> causes;
        if (who != office) continue;
        std::set<std::string>& into = out[mode];
        if (causes == "-") continue;
        std::size_t from = 0;
        while (from <= causes.size()) {
            const std::size_t comma = causes.find(',', from);
            into.insert(causes.substr(from, comma == std::string::npos ? std::string::npos
                                                                        : comma - from));
            if (comma == std::string::npos) break;
            from = comma + 1;
        }
    }
    return out;
}

/// WHAT EACH MODE OF ONE PANE SAID OF THE OTHERS, AGAINST THE CENSUS: in every mode the pane was
/// driven into, its `elsewhere` names exactly its other modes driven, each with the union of that
/// mode's census causes.
inline void check_elsewhere(const std::string& office,
                            const std::map<std::string, std::vector<ModeCauses>>& said,
                            const std::vector<std::string>& census) {
    std::map<std::string, std::set<std::string>> by_mode;
    for (auto& [mode, causes] : census_causes_by_mode(census, office)) {
        if (said.count(mode) == 1) by_mode[mode] = std::move(causes);
    }
    for (const auto& [mode, elsewhere] : said) {
        std::set<std::string> named;
        for (const ModeCauses& other : elsewhere) {
            named.insert(other.mode);
            const auto expected = by_mode.find(other.mode);
            REQUIRE_MESSAGE(expected != by_mode.end(),
                            office << " in `" << mode << "` names the mode `" << other.mode
                                   << "`, which the census never drove");
            std::set<std::string> causes;
            for (const ActionCause& c : other.causes) causes.insert(census_cause(c));
            CHECK_MESSAGE(causes == expected->second,
                          office << " in `" << mode << "` says `" << other.mode
                                 << "` causes other than its census rows do");
        }
        for (const auto& [other, causes] : by_mode) {
            (void)causes;
            if (other != mode) {
                CHECK_MESSAGE(named.count(other) == 1,
                              office << " in `" << mode << "` does not say what `" << other
                                     << "` may cause");
            }
        }
    }
}

/// ONE OFFICE'S CENSUS AS ITS SUITE DROVE IT: its lines, in the order its panes declare their rows
/// in each mode driven, and, by pane, what each mode it was driven into said of its others.
struct OfficeCensus {
    std::vector<std::string> lines;
    std::map<std::string, std::map<std::string, std::vector<ModeCauses>>> elsewhere;
};

/// ...AND A HELD PANE'S ROWS ADDED IN ITS MODE, with what that mode says of the pane's others.
inline void census_take(OfficeCensus& into, const std::string& office, const RuntimePane& held) {
    for (const v3::PaneActionRow& a : held.actions) {
        into.lines.push_back(census_line(office, held.mode, a));
    }
    into.elsewhere[held.pane][held.mode] = held.elsewhere;
}

/// THE DRIVERS, one per office, registered by the suites that drive its panes; and every office
/// registered twice, which the census case refuses.
using CensusDriver = std::function<OfficeCensus()>;
inline std::map<std::string, CensusDriver>& census_drivers() {
    static std::map<std::string, CensusDriver> drivers;
    return drivers;
}
inline std::vector<std::string>& census_registered_twice() {
    static std::vector<std::string> twice;
    return twice;
}

/// REGISTERS ONE OFFICE'S DRIVER, at namespace scope in the suite that drives its panes:
/// `const CensusDriverOf kFilesCensus{"zengine.files", [] { ...; return census; }};`
struct CensusDriverOf {
    CensusDriverOf(const std::string& office, CensusDriver drive) {
        if (!census_drivers().emplace(office, std::move(drive)).second) {
            census_registered_twice().push_back(office);
        }
    }
};

/// A READER OF A PANE'S MANUAL: it asks the pane's office and keeps each answer with Loom's word
/// on whether it answered this asker.
struct ManualAskerState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ManualAskerState, 1, ZEN_FIELD(heard));
};
class ManualAsker
    : public loom::WeaveBase<ManualAsker, ManualAskerState, loom::Accept<PaneDocumentShown>,
                             loom::Emit<PaneDocumentRequested>> {
public:
    void on(const PaneDocumentShown& shown, loom::Mail& mail) {
        heard.push_back(shown);
        answers.push_back(mail.answers_ask());
    }
    std::vector<PaneDocumentShown> heard;
    std::vector<bool> answers;
};

/// ASK ONE OFFICE FOR A PANE'S MANUAL, as a weave would, and return its one answer.
struct ManualReader {
    PaneRig& r;
    std::string office;
    ManualAsker* asker = nullptr;
    loom::WeaveId id{};
    ManualReader(PaneRig& rig, std::string office_of) : r(rig), office(std::move(office_of)) {
        auto held = std::make_unique<ManualAsker>();
        asker = held.get();
        loom::Grant grant;
        grant.allow_to_role(PaneDocumentRequested::zen_name, PaneDocumentRequested::zen_version,
                            office);
        id = r.bus.register_weave(std::move(held), std::move(grant));
        asker->zen_set_self(id);
    }
    PaneDocumentShown ask(const std::string& pane_key, const std::string& section) {
        asker->heard.clear();
        asker->answers.clear();
        (void)r.bus.send_as_to_role(id, office,
                                    loom::Message(loom::to_value(PaneDocumentRequested{pane_key,
                                                                                       section}),
                                                  id, loom::WeaveId{}, 9));
        r.bus.drain_until_idle();
        REQUIRE(asker->heard.size() == 1);
        CHECK(asker->answers[0]); // the answer to this asker's own ask, and to it alone
        return asker->heard[0];
    }
};

/// THE MANUAL A PANE ANSWERS, HELD TO ITS ROWS: asked whole, it answers with no refusal and its
/// commands are exactly `declared`, the ids the office's panes declare in every mode driven; asked
/// for one, each declared id is answered by its own section, under a command group.
inline void check_answered_manual(ManualReader& reader, const std::string& pane,
                                  const std::set<std::string>& declared) {
    REQUIRE_FALSE(declared.empty());
    const PaneDocumentShown whole = reader.ask(pane, "");
    CHECK(whole.refusal.empty());
    CHECK_FALSE(whole.content_id.empty());
    std::set<std::string> commands;
    for (const PaneDocumentSection& s : whole.sections) {
        if (zengine::manual::is_command_group(s.group) && s.key != "commands" &&
            s.key.rfind("commands-", 0) != 0) {
            commands.insert(s.key);
        }
    }
    CHECK(commands == declared);
    for (const std::string& id : declared) {
        INFO("pane ", pane, ", id ", id);
        const PaneDocumentShown one = reader.ask(pane, id);
        CHECK(one.refusal.empty());
        REQUIRE(one.sections.size() == 1);
        CHECK(one.sections[0].key == id);
        CHECK(zengine::manual::is_command_group(one.sections[0].group));
        CHECK(one.content_id == whole.content_id);
    }
    const PaneDocumentShown missing = reader.ask(pane, "no.such-command");
    CHECK_FALSE(missing.refusal.empty());
    CHECK(missing.sections.empty());
}

/// THE IDS A CENSUS'S LINES NAME, once each.
inline std::set<std::string> census_ids(const std::vector<std::string>& lines) {
    std::set<std::string> out;
    for (const std::string& line : lines) {
        std::istringstream in(line);
        std::string office, mode, id;
        in >> office >> mode >> id;
        out.insert(id);
    }
    return out;
}

#endif // ZENGINE_TESTS_CENSUS_HPP
