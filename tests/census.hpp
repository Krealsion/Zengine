// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_CENSUS_HPP
#define ZENGINE_TESTS_CENSUS_HPP

// The action census, the press witness and the manual witness, shared by the pane suites: each
// suite registers per office a census driver (`CensusDriverOf`), a press driver (`PressDriverOf`)
// pressing each row and its press witness cases (`ZEN_PRESS_WITNESS_CASE`). The Builder's suite
// holds each office's lines to `tests/actions.txt` or `tests/actions/<office>.txt`, and each press
// to its row's causes; a pane's manual is held to the ids its rows declare.
// Reference: manual/docs/manual.md.

#include "workshop_support.hpp"

#include "graphics/vocabulary.hpp"
#include "manual/manual.hpp"
#include "workshop/pane_document.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
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
/// <enters> <label>`, the mode `-` for a pane that declares none, and a row's key for a POSIX
/// terminal after its default, joined by `~` (no key's name holds one).
inline std::string census_line(const std::string& office, const std::string& mode,
                               const std::string& id, std::int64_t scancode,
                               std::int64_t modifiers, const std::vector<ActionCause>& causes,
                               const std::string& enters, const std::string& label,
                               std::int64_t posix_scancode = 0, std::int64_t posix_modifiers = 0) {
    const Gesture g{scancode, modifiers};
    const Gesture posix{posix_scancode, posix_modifiers};
    return office + " " + (mode.empty() ? std::string("-") : mode) + " " + id + " " +
           (is_bound(g) ? gesture_word(g) : std::string("unbound")) +
           (is_bound(posix) ? "~" + gesture_word(posix) : std::string()) + " " +
           census_causes(causes) + " " + (enters.empty() ? std::string("-") : enters) + " " +
           label;
}

/// ...A HELD ROW'S, read from what Workshop holds of the declaration.
inline std::string census_line(const std::string& office, const std::string& mode,
                               const v4::PaneActionRow& a) {
    return census_line(office, mode, a.id, a.scancode, a.modifiers, a.causes, a.enters, a.label,
                       a.posix_scancode, a.posix_modifiers);
}

/// THE KEYS A CENSUS LINE'S KEY WORD NAMES: its default, then a POSIX key after a `~`, if any.
inline std::vector<std::string> census_keys(const std::string& word) {
    const std::size_t tilde = word.find('~');
    if (tilde == std::string::npos) return {word};
    return {word.substr(0, tilde), word.substr(tilde + 1)};
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
    for (const v4::PaneActionRow& a : held.actions) {
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

// ---- The press witness: each declared action, pressed, sends nothing its row does not declare

/// THE PANE PROTOCOL, by name in every version: what a pane says about itself to Workshop and the
/// layouts' owner -- its offer, rows, menus, its answer to a manual's ask, its row asked for, a
/// setting it cannot use, and the approval it asks before an act -- and what it draws in a
/// graphics window. A press's causes are every other send.
inline bool pane_protocol(const std::string& shape) {
    static const std::set<std::string> kSaid{
        PaneOffered::zen_name,           PaneActions::zen_name,
        PaneMenuRequested::zen_name,
        PanePassRequested::zen_name,     PaneKeyboardRequested::zen_name,
        PaneEscapeUnspent::zen_name,     PaneRevealRequested::zen_name,
        PaneManageRequested::zen_name,   PaneOperationRequested::zen_name,
        PaneDocumentShown::zen_name,     PaneSettingsDeclared::zen_name,
        PaneSettingsAsked::zen_name,     PaneSettingUnused::zen_name,
        zengine::graphics::Drawn::zen_name};
    return kSaid.count(shape) == 1;
}

/// EVERY SEND ONE PARTICIPANT MAKES OUTSIDE THE PANE PROTOCOL while this is held, as
/// `<shape>/<version>`: delivered or refused, to an office, a weave or anyone.
class SendsOf {
public:
    SendsOf(loom::Switchboard& bus, loom::WeaveId who) : bus_(bus) {
        tap_ = bus_.add_observer([this, who](const loom::BusEvent& ev) {
            if ((ev.kind == loom::EventKind::Delivered || ev.kind == loom::EventKind::Refused) &&
                ev.sender == who && !pane_protocol(ev.schema_name)) {
                sent.insert(ev.schema_name + "/" + std::to_string(ev.schema_version));
            }
        });
    }
    ~SendsOf() { bus_.remove_observer(tap_); }
    SendsOf(const SendsOf&) = delete;
    SendsOf& operator=(const SendsOf&) = delete;
    std::set<std::string> sent;

private:
    loom::Switchboard& bus_;
    loom::ObserverId tap_ = 0;
};

/// WHAT A ROW DECLARES, AS A SEND IS SEEN: each cause's shape and version.
inline std::set<std::string> declared_sends(const std::vector<ActionCause>& causes) {
    std::set<std::string> out;
    for (const ActionCause& c : causes) out.insert(c.shape + "/" + std::to_string(c.version));
    return out;
}

/// A ROW'S KEY AS THE PRESS WITNESS KEEPS IT: `<mode> <id>`, `-` for a pane that declares no mode.
inline std::string press_key(const std::string& mode, const std::string& id) {
    return (mode.empty() ? std::string("-") : mode) + " " + id;
}

/// THE SHARE A ROW'S PRESS FALLS IN, of `shares`: its key's FNV-1a hash, so a row's share is the
/// same in every process and on every platform, whatever else the office declares.
inline std::size_t press_share_of(const std::string& key, std::size_t shares) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char c : key) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(hash % shares);
}

/// ONE OFFICE'S PRESSES, JUDGED: every press `judge` is handed in its share of the office's rows is
/// held to its row's causes, and the rows pressed, what each declares and what each sent are kept,
/// by `<mode> <id>`. A row outside the share is not pressed: another case presses it.
class PressWitness {
public:
    explicit PressWitness(std::string office, std::size_t share = 0, std::size_t shares = 1)
        : office_(std::move(office)), share_(share), shares_(shares) {}

    /// WHETHER THE ROW KEYED `key` IS THIS WITNESS'S TO PRESS.
    bool mine(const std::string& key) const { return press_share_of(key, shares_) == share_; }

    /// ONE PRESS OF THE ROW `id` IN `mode` (empty for a pane that declares none): `press` presses
    /// it, and every send `who` makes outside the pane protocol meanwhile is one `causes` declares.
    void judge(loom::Switchboard& bus, loom::WeaveId who, const std::string& mode,
               const std::string& id, const std::vector<ActionCause>& causes,
               const std::function<void()>& press) {
        const std::string key = press_key(mode, id);
        if (!mine(key)) return;
        CAPTURE(office_);
        CAPTURE(key);
        REQUIRE(who.valid());
        declared[key] = causes;
        pressed.insert(key);
        SendsOf sends(bus, who);
        press();
        const std::set<std::string> said = declared_sends(causes);
        for (const std::string& s : sends.sent) {
            reached[key].insert(s);
            CHECK_MESSAGE(said.count(s) == 1, "`" << id << "` in `" << key.substr(0, key.find(' '))
                                                  << "` sent " << s
                                                  << ", which its row does not declare");
        }
    }

    /// ...PRESSED AS WORKSHOP SAYS A RESOLVED KEY: the row's id, to `pane` of this office, as
    /// Workshop's own office -- the press a pane hears whatever gesture made it.
    void judge(PaneRig& r, const std::string& pane, const std::string& mode,
               const v4::PaneActionRow& row) {
        judge(r.bus, r.bus.role_holder(office_), mode, row.id, row.causes, [&] {
            CHECK(r.workshop_action(office_, pane, row.id).delivered);
        });
    }

    std::map<std::string, std::vector<ActionCause>> declared;
    std::map<std::string, std::set<std::string>> reached;
    std::set<std::string> pressed;

private:
    std::string office_;
    std::size_t share_ = 0;
    std::size_t shares_ = 1;
};

/// THE PRESS DRIVERS, one per office, registered beside its census driver by the suite that drives
/// its panes: each presses every row its panes declare in every mode, a pane made afresh for each
/// press, so no press is judged by what an earlier one left. An office whose presses are long gives
/// them to `shares` cases, each pressing its share of the rows.
using PressDriver = std::function<void(PressWitness&)>;
inline std::map<std::string, PressDriver>& press_drivers() {
    static std::map<std::string, PressDriver> drivers;
    return drivers;
}
inline std::map<std::string, std::size_t>& press_shares() {
    static std::map<std::string, std::size_t> shares;
    return shares;
}
inline std::vector<std::string>& press_registered_twice() {
    static std::vector<std::string> twice;
    return twice;
}
struct PressDriverOf {
    PressDriverOf(const std::string& office, PressDriver drive, std::size_t shares = 1) {
        if (!press_drivers().emplace(office, std::move(drive)).second) {
            press_registered_twice().push_back(office);
        }
        press_shares()[office] = shares;
    }
};

/// THE PRESS WITNESS'S CASES, registered as each is defined, in every process of the program
/// whatever it runs: for each office, the shares its cases press. The census case holds every
/// office's shares to one case each.
inline std::map<std::string, std::vector<std::size_t>>& press_cases() {
    static std::map<std::string, std::vector<std::size_t>> cases;
    return cases;
}
struct PressCaseOf {
    PressCaseOf(const std::string& office, std::size_t share) {
        press_cases()[office].push_back(share);
    }
};

/// ONE SHARE OF ONE OFFICE'S PRESS WITNESS, RUN (the Builder's suite holds it).
void witness_presses(const std::string& office, std::size_t share);

/// AN OFFICE'S PRESS WITNESS CASE: share `share` of the office's rows, pressed; `who` names the
/// rows in the case's name.
#define ZEN_PRESS_WITNESS_CASE(office, share, who)                                                 \
    static const PressCaseOf DOCTEST_ANONYMOUS(zen_press_case_){office, share};                    \
    TEST_CASE("the press witness: " who ", each pressed in each mode its suite drives, sends "     \
              "nothing beside the pane protocol that its row does not declare") {                  \
        witness_presses(office, share);                                                            \
    }

/// THE HOST A SUITE'S RIG DRIVES: the rig itself, or the `PaneRig` it holds as `r`.
inline PaneRig& pane_rig(PaneRig& r) { return r; }
template <class Rig>
    requires requires(Rig& rig) { rig.r.bus; }
PaneRig& pane_rig(Rig& rig) {
    return rig.r;
}

/// A WALK THROUGH A PANE'S STATES: it drives `Rig` from its first state to its last, handing each
/// state's pane, as Workshop holds it there, to `at`.
template <class Rig>
using StateWalk = std::function<void(Rig&, const std::function<void(const RuntimePane&)>&)>;

/// EACH ROW OF EACH STATE A WALK REACHES, PRESSED ON A RIG OF ITS OWN: `make` makes a fresh rig,
/// `walk` drives it to the state, the row is pressed as Workshop says a resolved key, and the walk
/// goes no further, so no press is judged by what another left. A row outside the witness's share
/// is neither walked to nor pressed.
template <class Rig>
void press_each_state(PressWitness& w, const std::function<std::unique_ptr<Rig>()>& make,
                      const StateWalk<Rig>& walk) {
    struct Pressed {};
    std::vector<std::vector<std::string>> rows;
    {
        const std::unique_ptr<Rig> rig = make();
        walk(*rig, [&](const RuntimePane& held) {
            std::vector<std::string> keys;
            for (const v4::PaneActionRow& row : held.actions) {
                keys.push_back(press_key(held.mode, row.id));
            }
            rows.push_back(std::move(keys));
        });
    }
    REQUIRE_FALSE(rows.empty());
    for (std::size_t state = 0; state < rows.size(); ++state) {
        for (std::size_t i = 0; i < rows[state].size(); ++i) {
            if (!w.mine(rows[state][i])) continue;
            const std::unique_ptr<Rig> rig = make();
            std::size_t at = 0;
            bool pressed = false;
            try {
                walk(*rig, [&](const RuntimePane& held) {
                    if (at++ != state) return;
                    REQUIRE(held.actions.size() == rows[state].size());
                    const v4::PaneActionRow row = held.actions[i];
                    const std::string pane = held.pane;
                    const std::string mode = held.mode;
                    w.judge(pane_rig(*rig), pane, mode, row);
                    pressed = true;
                    throw Pressed{};
                });
            } catch (const Pressed&) {
            }
            CHECK_MESSAGE(pressed, "the walk did not reach its state " << state << " again");
        }
    }
}

#endif // ZENGINE_TESTS_CENSUS_HPP
