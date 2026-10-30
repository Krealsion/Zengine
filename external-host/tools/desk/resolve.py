# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""desk/resolve -- the controls on a running Workshop's desk whose press may cause a message,
resolved by the asker from what its own row may read, and, when exactly one is shown and usable,
marked for the weaver. It presses nothing.

The asker names the message it wants the weaver to cause (`shape`, `version`) and, for a message
about a subject, the subject (`zengine.builder-pane/builder`). It reads its own row (which panes it
reads), the keymap (every row in force with its address, the weaver's key and what a press of it
may cause, and each pane's other modes), the desk, the pane inventory, and the parts of each pane a
candidate stands in (`PaneView` v6, each part with whether its operation applies now). It answers
every control that may cause the message: its address (`<office>/<pane>:<id>`, or `<office>:<id>`
for a row above every pane), the part that draws it, whether that part shows and is usable now, the
weaver's key, and where its pane is. When none is shown and usable it resolves the steps that would
bring one: a row in force that enters the pane's other mode where one is declared, and showing the
pane, which is itself a message (`PaneLaunchRequested`, `PaneToggleRequested`) resolved the same way,
at most DEPTH steps deep. A route never runs through a pane only the weaver's hand reaches.

When no control the row may read causes it, in any mode: a shape no row or mode on the desk
declares and the session's vocabulary does not resolve is said unknown; otherwise, if the row reads
every pane, that is the case for a tool, said as its four claims with their evidence and filed
nowhere; if it does not, the panes outside its read are named, since one of them may. A mark that
marks nothing says why. The whole resolution is written as `resolve.json`.
"""

import json

import desk_asks

WORKSHOP = desk_asks.WORKSHOP
GUESTS = desk_asks.GUESTS
SHOWING = ("PaneLaunchRequested", "PaneToggleRequested")
DEPTH = 3
#: The panes only the weaver's hand reads or points at, on every host.
WEAVER_ONLY = (("zengine.connections", "*"), ("zengine.desktop", "hotkeys"))


def weaver_only(office, pane):
    return any(office == o and (p == "*" or pane == p) for o, p in WEAVER_ONLY)


def door_contains(door, office, pane):
    """Whether a `capture` door holds the pane `office`/`pane`, segment by segment."""
    if door == "*":
        return True
    head, _, rest = door.partition("/")
    return head == office and (rest == "*" or rest == pane)


def in_read(row, office, pane):
    """Whether the row reads `office`/`pane`: every pane for a row that reads the desk whole."""
    if not row or not row.get("by_door"):
        return True
    return any(door_contains(d, office, pane) for d in row.get("capture") or [])


def cause_text(c):
    text = "%s v%d" % (c.get("shape", ""), c.get("version", 0))
    return text + (" " + c["subject"] if c.get("subject") else "")


class Desk(object):
    """What the asker reads, once per resolution; a pane's parts read when a candidate needs them."""

    def __init__(self, row, keymap, desk, inventory, read_view):
        self.row = row or {}
        self.keymap = keymap or {}
        self.on_desk = dict((desk_asks.key_of(p), p) for p in (desk or {}).get("panes") or [])
        self.inventory = (inventory or {}).get("panes") or []
        self.read_view = read_view
        self.views = {}

    def bindings(self):
        return self.keymap.get("rows") or []

    def modes(self):
        return self.keymap.get("modes") or []

    def name_of(self, office, pane):
        for p in self.inventory:
            if p.get("office") == office and p.get("pane") == pane:
                return p.get("name", "").split("\n")[0]
        return (self.on_desk.get((office, pane)) or {}).get("name", "") or pane

    def place(self, office, pane):
        """`open`, `covered`, `off-room`, `unresolved` from the desk; `hidden` for a pane offered
        and not on it; `not offered` for none; `outside this row's read` for a pane it may not read."""
        if not in_read(self.row, office, pane):
            return "outside this row's read"
        d = self.on_desk.get((office, pane))
        if d is not None:
            return d.get("state", "")
        for p in self.inventory:
            if p.get("office") == office and p.get("pane") == pane:
                return "hidden" if p.get("available") else "not available"
        return "not offered"

    def part(self, office, pane, name):
        """The part as the asker's read of the pane says it, or None."""
        k = (office, pane)
        if k not in self.views:
            self.views[k] = self.read_view(office, pane) if (k in self.on_desk and
                                                             in_read(self.row, office, pane)) else None
        view = self.views[k] or {}
        for p in view.get("parts") or []:
            if p.get("name") == name:
                return p
        return None

    def outside(self):
        """Every offered pane this row does not read, by name."""
        return ["%s (%s/%s)" % (self.name_of(p.get("office", ""), p.get("pane", "")), p.get("office", ""),
                                p.get("pane", ""))
                for p in self.inventory if not in_read(self.row, p.get("office", ""), p.get("pane", ""))]


def matches(c, shape, version, subject):
    """Whether a declared cause may cause the message: a cause that names no subject answers only
    an ask about none, since nothing says which pane or part it would reach."""
    if c.get("shape") != shape or (version and c.get("version") != version):
        return False
    s = c.get("subject") or ""
    if s.startswith("to "):
        return not subject or s[3:] == subject
    if s.startswith("on "):
        return bool(subject)
    return not subject


def declared(desk, shape, version):
    """Whether any row in force, or any other mode a pane declares, names the shape."""
    causes = [c for b in desk.bindings() for c in b.get("causes") or []]
    causes += [c for m in desk.modes() for e in m.get("elsewhere") or [] for c in e.get("causes") or []]
    return any(c.get("shape") == shape and (not version or c.get("version") == version) for c in causes)


def candidate(desk, b, c, subject):
    office, pane, action = b.get("office", ""), b.get("pane", ""), b.get("id", "")
    out = {"address": "%s/%s:%s" % (office, pane, action) if pane else "%s:%s" % (office, action),
           "office": office, "pane": pane, "id": action, "cause": cause_text(c) if c else "enters",
           "key": b.get("shown") or b.get("gesture") or "no key", "label": b.get("label", "")}
    if not pane:  # a row above every pane: a key, no part, wherever the weaver stands
        out.update(place="the whole desk", part=None, control=None, shown=False, at=None, usable=None)
        return out
    s = (c or {}).get("subject") or ""
    part_name = s[3:] + subject if s.startswith("on ") else "control:" + action
    part = desk.part(office, pane, part_name)
    out.update(place=desk.place(office, pane), part=part_name,
               control=("part:" + part_name) if s.startswith("on ") else action,
               shown=part is not None and part.get("space", 0) != 0,
               at=None if part is None or not part.get("space", 0) else (part.get("x", 0), part.get("y", 0)),
               usable=None if part is None else bool(part.get("available", True)))
    return out


def candidates(desk, shape, version, subject):
    out = []
    for b in desk.bindings():
        if weaver_only(b.get("office", ""), b.get("pane", "")):
            continue
        for c in b.get("causes") or []:
            if not matches(c, shape, version, subject):
                continue
            target = (c.get("subject") or "")[3:] if (c.get("subject") or "").startswith("to ") else ""
            if target and weaver_only(*target.partition("/")[::2]):
                continue
            out.append(candidate(desk, b, c, subject))
            break
    return out


def entering(desk, office, pane, mode):
    """The rows in force of `office`/`pane` whose press enters `mode`, as candidates."""
    return [candidate(desk, b, None, "") for b in desk.bindings()
            if b.get("office") == office and b.get("pane") == pane and b.get("enters") == mode]


def other_modes(desk, shape, version, subject):
    """`(office, pane, mode)` for each pane whose other mode may cause the message."""
    out = []
    for m in desk.modes():
        for e in m.get("elsewhere") or []:
            if any(matches(c, shape, version, subject) for c in e.get("causes") or []):
                out.append((m.get("office", ""), m.get("pane", ""), e.get("mode", "")))
    return out


def ready(found):
    return [c for c in found if c["shown"] and c["usable"]]


def route(desk, shape, version, subject, depth, seen):
    """The controls that cause the message, and the steps that would bring one before the weaver,
    nearest first."""
    found = candidates(desk, shape, version, subject)
    if ready(found) or depth >= DEPTH:
        return found, []
    steps = []
    if not found:
        for office, pane, mode in other_modes(desk, shape, version, subject):
            by = entering(desk, office, pane, mode)
            steps.append({"to show": "%s/%s in its mode %s" % (office, pane, mode), "for": shape,
                          "by": by, "then": []})
    for c in found:
        if not c["pane"] or c["place"] == "open" or (c["office"], c["pane"]) in seen:
            continue
        seen.add((c["office"], c["pane"]))
        target = "%s/%s" % (c["office"], c["pane"])
        more, deeper = [], []
        for showing in SHOWING:
            here, below = route(desk, showing, 0, target, depth + 1, seen)
            # A control inside the pane being shown cannot show it.
            more += [m for m in here if (m["office"], m["pane"]) != (c["office"], c["pane"])]
            deeper += below
        if more or deeper:
            steps.append({"to show": target, "for": c["address"], "by": more, "then": deeper})
    return found, steps


def said(c):
    at = "@%d,%d" % c["at"] if c.get("at") else ""
    where = "part %s%s %s" % (c["part"], at, "shown" if c["shown"] else "not shown") if c["part"] else "no part"
    usable = "" if c["usable"] is None else (", usable" if c["usable"] else ", unusable now")
    return "%s [%s; %s%s; key %s]" % (c["address"], c["place"], where, usable, c["key"])


def flatten(steps):
    out = []
    for st in steps:
        out += flatten(st["then"])
        out.append(st)
    return out


def case_for_a_tool(desk, shape, version, office, why, pane):
    """The four claims, each with its evidence, as a draft the agent files only under its own
    account, unlabelled, and on a weaver's desk only after the weaver says yes."""
    rows = len(desk.bindings())
    modes = sum(len(m.get("elsewhere") or []) for m in desk.modes())
    return [
        "1. the operation exists: not checked here -- a row with `inspect` asks %s what it accepts"
        % (office or "the office that would take it"),
        "2. no control causes it: none of the %d rows in force, nor the %d other modes they declare, "
        "names %s%s" % (rows, modes, shape, " v%d" % version if version else ""),
        "3. the weaver needs it: %s" % (why or "the lesson's sentence is owed (`why`)"),
        "4. it belongs in %s" % (pane or "the pane that already speaks for %s to the weaver" % (office or
                                                                                         "its office")),
        "filed nowhere: an issue is the agent's own, unlabelled, and on a weaver's desk opened only "
        "after the weaver says yes",
    ]


def resolve(desk, shape, version, subject, known=None):
    """The resolution. `known(shape, version)` says whether this session's vocabulary resolves the
    shape (None: it cannot tell); a shape neither it nor any declaration on the desk names is
    `unknown`, never the case for a tool."""
    found, steps = route(desk, shape, version, subject, 0, set())
    whole = not desk.row.get("by_door") or desk.row.get("capture") == ["*"]
    unknown = (not found and not steps and not declared(desk, shape, version) and known is not None
               and known(shape, version) is False)
    return {"found": found, "steps": steps, "ready": ready(found), "unknown": unknown,
            "outside": [] if (found or steps or whole or unknown) else desk.outside(),
            "nowhere": not found and not steps and whole and not unknown}


def unmarked(r):
    """Why nothing was marked, when no ready control or step's control was chosen."""
    if not r["found"] and not r["steps"]:
        return "not marked: no control or step on this desk may cause it"
    for st in flatten(r["steps"]):
        near = [c for c in st["by"] if c["shown"] and c["usable"] and c["part"]]
        if len(near) > 1:
            return ("not marked: the step to show %s has %d controls shown and usable; the asker "
                    "names one by address" % (st["to show"], len(near)))
    keys = [c for st in flatten(r["steps"]) for c in st["by"] if not c["part"]]
    if keys:
        return ("not marked: no step's control is shown and usable with a part; %s has no part to "
                "mark: a key the weaver presses" % ", ".join("%s (%s)" % (c["key"], c["address"])
                                                             for c in keys))
    return "not marked: no control or step is shown and usable"


def mark(asker, desk_resolution, words):
    """Mark the one control shown and usable, else the nearest step's one; ``(line, address)``."""
    r = desk_resolution
    choice, why = None, ""
    if len(r["ready"]) == 1:
        choice = r["ready"][0]
    elif r["ready"]:
        return ("not marked: %d controls shown and usable; the asker names one by address"
                % len(r["ready"])), None
    else:
        for st in flatten(r["steps"]):
            near = [c for c in st["by"] if c["shown"] and c["usable"] and c["part"]]
            if len(near) == 1:
                choice, why = near[0], " (the nearest step, to show %s)" % st["to show"]
                break
    if choice is None:
        return unmarked(r), None
    answer, refused = asker.ask(WORKSHOP, "GuideRequested", 1, {
        "pane": "%s/%s" % (choice["office"], choice["pane"]), "control": choice["control"],
        "words": words})
    if refused is not None:
        return "not marked %s%s: %s" % (choice["address"], why, refused), None
    return "%s %s%s: %s" % ("marked" if answer.get("shown") else "not marked", choice["address"], why,
                            answer.get("said", "")), choice["address"] if answer.get("shown") else None


def read(ctx, asker):
    ctx.step("own row")
    row, _ = asker.ask(GUESTS, "GuestRowDescribedRequested", 2, {})
    ctx.step("keymap")
    keymap, keymap_refused = asker.ask(WORKSHOP, "KeymapRequested", 1, {})
    ctx.step("desk")
    desk, desk_refused = desk_asks.ask_desk(asker)
    ctx.step("inventory")
    inventory, _ = asker.ask(WORKSHOP, "PaneInventoryRequested", 1, {})
    pager = desk_asks.Asker(ctx, asker.link)

    def read_view(office, pane):
        got = desk_asks.read_pane(pager, office, pane)
        asker.asks += pager.asks
        pager.asks = 0
        return got.get("view")

    refused = keymap_refused or desk_refused
    return Desk(row, keymap, (desk or {}).get("desk"), inventory, read_view), refused


def flag(value):
    """A `bool` input as the manager types it, or text in the spellings the client types."""
    if isinstance(value, bool):
        return value
    return str(value or "").strip().lower() in ("1", "yes", "true", "on")


#: The most versions of a shape asked of the session's vocabulary when no version is named.
VERSIONS = 16


def vocabulary_knows(ctx):
    """`known(shape, version)` from the session's own vocabulary, or None where it cannot tell."""
    conn = getattr(ctx, "conn", None)
    if conn is None or not hasattr(conn, "schema"):
        return None
    from loom_session.client import UnknownShape

    def known(shape, version):
        for v in ([version] if version else range(1, VERSIONS + 1)):
            try:
                conn.schema(shape, v)
                return True
            except UnknownShape:
                continue
            except Exception:  # the host could not say: no claim either way
                return None
        return False
    return known


def lines(desk, r, shape, version, subject, inputs):
    """The resolution in words, as the summary says it."""
    asked = "%s%s%s" % (shape, " v%d" % version if version else "", " about " + subject if subject else "")
    if r.get("unknown"):
        return ["desk/resolve: %s -- unknown: no row or mode on this desk declares it, and this "
                "session's vocabulary resolves no such shape; check its spelling and version" % asked]
    out = ["desk/resolve: %s -- %d control(s) may cause it, %d shown and usable" % (
        asked, len(r["found"]), len(r["ready"]))]
    out += ["  " + said(c) for c in r["found"]]
    for st in r["steps"]:
        out.append("  to show %s (for %s):" % (st["to show"], st["for"]))
        out += ["    " + said(c) for c in st["by"]]
        for t in flatten(st["then"]):
            out.append("    then to show %s:" % t["to show"])
            out += ["      " + said(c) for c in t["by"]]
    if r["outside"]:
        out.append("  no control this row reads may cause it; outside its read: " + ", ".join(r["outside"]))
    if r["nowhere"]:
        out.append("  no control on this desk may cause it, in any mode: a case for a tool, the weaver's "
                   "to take --")
        out += ["    " + c for c in case_for_a_tool(desk, shape, version,
                                                     (inputs.get("office") or "").strip(),
                                                     (inputs.get("why") or "").strip(),
                                                     (inputs.get("pane") or "").strip())]
    return out


def answer(ctx, desk, asker, known=None):
    """Resolve, mark when asked, write `resolve.json` whole and return the summary."""
    shape = (ctx.inputs.get("shape") or "").strip()
    version = int(ctx.inputs.get("version") or 0)
    subject = (ctx.inputs.get("subject") or "").strip()
    ctx.step("resolve")
    r = resolve(desk, shape, version, subject, known)
    said_lines = lines(desk, r, shape, version, subject, ctx.inputs)
    marked = None
    if flag(ctx.inputs.get("mark")) and not r["unknown"]:
        line, address = mark(asker, r, ctx.inputs.get("words") or "")
        marked = {"line": line, "address": address}
        said_lines.append("  " + line)
    whole = dict(r, shape=shape, version=version, subject=subject, mark=marked, lines=said_lines)
    ctx.produce("resolve.json", (json.dumps(whole, indent=2, sort_keys=True, default=str) + "\n").encode("utf-8"))
    said_lines.append("  the whole resolution: resolve.json")
    text = "\n".join(said_lines)
    return text if len(text) <= 1900 else text[:1860] + "\n  ... cut here; resolve.json holds it whole"


def run(ctx):
    link = ctx.inputs.get("link") or "workshop"
    asker = desk_asks.Asker(ctx, link)
    desk, refused = read(ctx, asker)
    if refused is not None:
        return "desk/resolve: not resolved -- %s" % refused
    return answer(ctx, desk, asker, vocabulary_knows(ctx))

