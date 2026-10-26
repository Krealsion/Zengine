# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Workshop's own setup, session, window and guests files, as a tool writes them: at the versions
the current Workshop reads, whatever desk it was handed.

SETUP_VERSION and SESSION_VERSION are the setup file's and the session file's `kFormatVersion`
(workshop/setup_persist.hpp, workshop/layouts/session_persist.hpp). tests/code_values.txt reads
both owners and workshop/docs/demo-setups.md marks one value for each pair, so a Workshop that
moves either file's version is a red until this module moves with it. A desk a launcher is handed
is lifted to SETUP_VERSION before anything is written around it: a version-4 or version-5 desk
keeps every field, each coordinate and amount written only where its word reads one, and gains no
settings, locks, pins or baseline it did not have; another version is refused, naming what to do.
The room a session opens in is the window file's, written beside it (`window_path`).

GUESTS_VERSION is the guests file's `kGuestsFileVersion` (workshop/guests.hpp), tied to it the same
way: the version a launcher writes into the guests file of the Workshop it starts. From version 3 a
row reads only the panes its `capture` field names and points only within its `guide` field, so a
launcher's row that may capture or guide names every pane, `*`, written and never assumed."""
import copy
import json

SETUP_VERSION = 6
SESSION_VERSION = 12
GUESTS_VERSION = 3
WINDOW_VERSION = 1

# The shape of each layout line in a session file, `WorkshopLayout` v4: a shape's identity, frozen
# once published, which the session's lines carry as they are.
LAYOUT_VERSION = 4

# The guests file's `host`: a weaver's, where a guest writes no file, types into no pane and
# reaches none of the editor, the Terminal and the Hotkeys pane; or a development host, an
# agent's own, where a guest's hand reaches them as the weaver's does.
WEAVER_HOST = "weaver"
DEVELOPMENT_HOST = "development"

# WORKSHOP'S NOTICE THAT ITS DESK MOVED, as a row's `observe` names it: what `desk-machine/watch` follows
# and the walk tools wait on (hand.py), observed only by a row holding `capture`.
DESK_NOTICE = {"producer": "zengine.workshop", "shape": "DeskStamps", "version": "3"}

# The setup versions a desk is lifted from: version 4, whose rows hold every field of version 5's
# but the settings, and version 5, which writes every coordinate and amount, zero where its word
# reads none (`setup_persist::v4` and `v5` in workshop/setup_persist.hpp).
LIFTED_SETUP_VERSIONS = (4, 5)

SETUP_FORMAT = "zengine-workshop-setup"
SESSION_FORMAT = "zengine-workshop-session"
WINDOW_FORMAT = "zengine-workshop-window"


def _by_words(row):
    """`row` at version 6: a place's `x` and `y` present only where its word is `pixels`, a size's
    `amount` only where its word is `pixels` -- an older row's zero where the word reads none is
    the absence it meant."""
    row = copy.deepcopy(row)
    place = dict(row.get("place") or {"mode": "default"})
    if place.get("mode") != "pixels":
        place.pop("x", None)
        place.pop("y", None)
    row["place"] = place
    for axis in ("width", "height"):
        size = dict(row.get(axis) or {"mode": "default"})
        if size.get("mode") != "pixels":
            size.pop("amount", None)
        row[axis] = size
    return row


def setup(name, panes, theme=None, locks=(), pins=()):
    """A WorkshopSetup file at SETUP_VERSION named `name`, its rows `panes` as given by their words,
    each keeping the settings it names or none; the layout's baseline, locks and pins as given, or
    none."""
    rows = [_by_words(dict(p, settings=list(p.get("settings", [])))) for p in panes]
    return {"zen": 1, "schema": "WorkshopSetup", "version": SETUP_VERSION, "fields": {
        "format": SETUP_FORMAT, "format_version": str(SETUP_VERSION), "name": name, "panes": rows,
        "theme": dict(theme or {"name": "", "fingerprint": "0"}), "locks": list(locks),
        "pins": list(pins)}}


def current(desk):
    """`desk`, a WorkshopSetup file, at SETUP_VERSION: as it is at that version, every field kept
    and each row given no settings at the version before, and refused at any other -- or refused
    first, as Workshop's reader refuses it, when it is not a Workshop setup at all."""
    fields = desk.get("fields", {}) if isinstance(desk, dict) else {}
    if not isinstance(desk, dict) or desk.get("schema") != "WorkshopSetup" or \
            fields.get("format") != SETUP_FORMAT:
        raise ValueError("not a Workshop setup: it says it is `%s`"
                         % (fields.get("format") or (desk.get("schema") if isinstance(desk, dict)
                                                     else type(desk).__name__)))
    version = desk.get("version")
    claimed = fields.get("format_version")
    if version == SETUP_VERSION and claimed == str(SETUP_VERSION):
        return copy.deepcopy(desk)
    if version in LIFTED_SETUP_VERSIONS and claimed == str(version):
        fields = copy.deepcopy(desk["fields"])
        return setup(fields["name"], fields["panes"])
    raise ValueError("the desk is setup version %s; a launcher lifts versions %s and writes %d -- "
                     "open it in Workshop and save it (`s`) to write version %d"
                     % (claimed, " and ".join(str(v) for v in LIFTED_SETUP_VERSIONS), SETUP_VERSION,
                        SETUP_VERSION))


def following_the_desk(row):
    """`row` with Workshop's notice that the desk moved in its `observe`, when it reads the desk
    (`capture`) and does not name it already: so its reader and its walks wait on the desk's moves."""
    row = dict(row)
    observe = list(row.get("observe") or [])
    if "capture" in (row.get("may") or []) and DESK_NOTICE not in observe:
        observe.append(dict(DESK_NOTICE))
    if observe:
        row["observe"] = observe
    return row


def every_pane(row, power):
    """`row` reading or pointing at every pane, `*`, when it may `power` and names no doors of it."""
    if power in (row.get("may") or []) and power not in row:
        return dict(row, **{power: ["*"]})
    return row


def guests(rows, host, listen="127.0.0.1:0", port_file=None):
    """A guests file at GUESTS_VERSION for `host` (WEAVER_HOST or DEVELOPMENT_HOST), its rows as
    given, each that may capture or guide and names no doors of it reaching every pane. Its `version` is
    written as the file writes every Int, a base-10 string."""
    if host not in (WEAVER_HOST, DEVELOPMENT_HOST):
        raise ValueError("a guests file's host is %r or %r, not %r"
                         % (WEAVER_HOST, DEVELOPMENT_HOST, host))
    rows = [every_pane(every_pane(r, "capture"), "guide") for r in rows]
    out = {"version": str(GUESTS_VERSION), "host": host, "listen": listen, "guests": rows}
    if port_file is not None:
        out["port_file"] = str(port_file)
    return out


def session(desk, link=""):
    """A session file's text at SESSION_VERSION holding one layout, id 1: `desk`, lifted to
    SETUP_VERSION, and the Setup file `link` names, if any, with the desk it was last known to
    hold. The room it opens in is the window file's (`window`). The file is a WorkshopSessionHeader
    line, then a WorkshopLayout line for each layout, each line one JSON value; write the text as
    it is."""
    fields = current(desk)["fields"]
    known = fields if link else setup("", [])["fields"]
    header = {"zen": 1, "schema": "WorkshopSessionHeader", "version": SESSION_VERSION, "fields": {
        "format": SESSION_FORMAT, "format_version": str(SESSION_VERSION), "layout_count": "1",
        "active": "0", "next_id": "2", "choices": []}}
    layout = {"zen": 1, "schema": "WorkshopLayout", "version": LAYOUT_VERSION, "fields": {
        "id": "1", "desk": fields, "link": {"path": str(link), "known": known}}}
    return "".join(json.dumps(value) + "\n" for value in (header, layout))


def window_path(session_path):
    """Where the window file a session opens in lives: beside it, `.window` before its extension."""
    p = str(session_path)
    dot = p.rfind(".")
    slash = max(p.rfind("/"), p.rfind(chr(92)))
    if dot > slash:
        return p[:dot] + ".window" + p[dot:]
    return p + ".window.json"


def window(viewport):
    """A window file's text at WINDOW_VERSION: the room `viewport`, (width, height) in canvas
    pixels, and no placement and no OS window's room."""
    width, height = viewport
    return json.dumps({"zen": 1, "schema": "WorkshopWindow", "version": WINDOW_VERSION, "fields": {
        "format": WINDOW_FORMAT, "format_version": str(WINDOW_VERSION),
        "viewport": {"width": str(width), "height": str(height)},
        "placement": {"mode": "none", "x": "0", "y": "0", "window": "normal"},
        "window_room": {"width": "0", "height": "0"}}})
