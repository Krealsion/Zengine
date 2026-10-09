# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Workshop's own setup, session and guests files, as a tool writes them: at the versions the
current Workshop reads, whatever desk it was handed.

SETUP_VERSION and SESSION_VERSION are the setup file's and the session file's `kFormatVersion`
(workshop/setup_persist.hpp, workshop/session_persist.hpp). tests/code_values.txt reads both owners
and docs/workshop/demo-setups.md marks one value for each pair, so a Workshop that moves either
file's version is a red until this module moves with it. A desk a launcher is handed is lifted to
SETUP_VERSION before anything is written around it: a version-4 desk keeps every field and gains
each row's settings, none; another version is refused, naming what to do.

GUESTS_VERSION is the guests file's `kGuestsFileVersion` (workshop/guests.hpp), tied to it the same
way: the version a launcher writes into the guests file of the Workshop it starts."""
import copy

SETUP_VERSION = 5
SESSION_VERSION = 8
GUESTS_VERSION = 2

# The guests file's `host`: a weaver's, where a guest writes no file, types into no pane and
# reaches none of the editor, the Terminal and the Hotkeys pane; or a development host, an
# agent's own, where a guest's hand reaches them as the weaver's does.
WEAVER_HOST = "weaver"
DEVELOPMENT_HOST = "development"

# WORKSHOP'S NOTICE THAT ITS DESK MOVED, as a row's `observe` names it: what `desk/watch` follows
# and the walk tools wait on (hand.py), observed only by a row holding `capture`.
DESK_NOTICE = {"producer": "zengine.workshop", "shape": "DeskStamps", "version": "1"}

# The setup version a desk is lifted from: version 4, whose rows hold every field of the current
# ones but the settings (`setup_persist::v4` in workshop/setup_persist.hpp).
LIFTED_SETUP_VERSION = 4

SETUP_FORMAT = "zengine-workshop-setup"
SESSION_FORMAT = "zengine-workshop-session"


def setup(name, panes):
    """A WorkshopSetup file at SETUP_VERSION named `name`, its rows `panes` as given, each keeping
    the settings it names or none."""
    rows = [dict(p, settings=list(p.get("settings", []))) for p in panes]
    return {"zen": 1, "schema": "WorkshopSetup", "version": SETUP_VERSION, "fields": {
        "format": SETUP_FORMAT, "format_version": str(SETUP_VERSION), "name": name, "panes": rows}}


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
    if version == LIFTED_SETUP_VERSION and claimed == str(LIFTED_SETUP_VERSION):
        fields = copy.deepcopy(desk["fields"])
        return setup(fields["name"], fields["panes"])
    raise ValueError("the desk is setup version %s; a launcher lifts version %d and writes %d -- "
                     "open it in Workshop and save it (`s`) to write version %d"
                     % (claimed, LIFTED_SETUP_VERSION, SETUP_VERSION, SETUP_VERSION))


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


def guests(rows, host, listen="127.0.0.1:0", port_file=None):
    """A guests file at GUESTS_VERSION for `host` (WEAVER_HOST or DEVELOPMENT_HOST), its rows as
    given. Its `version` is written as the file writes every Int, a base-10 string."""
    if host not in (WEAVER_HOST, DEVELOPMENT_HOST):
        raise ValueError("a guests file's host is %r or %r, not %r"
                         % (WEAVER_HOST, DEVELOPMENT_HOST, host))
    out = {"version": str(GUESTS_VERSION), "host": host, "listen": listen, "guests": list(rows)}
    if port_file is not None:
        out["port_file"] = str(port_file)
    return out


def session(desk, viewport, link=""):
    """A WorkshopSession file at SESSION_VERSION holding one layout: `desk`, lifted to SETUP_VERSION;
    the room `viewport`, (width, height) in canvas pixels; and the Setup file `link` names, if
    any, with the desk it was last known to hold."""
    fields = current(desk)["fields"]
    known = fields if link else setup("", [])["fields"]
    width, height = viewport
    return {"zen": 1, "schema": "WorkshopSession", "version": SESSION_VERSION, "fields": {
        "format": SESSION_FORMAT, "format_version": str(SESSION_VERSION),
        "viewport": {"width": str(width), "height": str(height)}, "active": "0",
        "placement": {"mode": "none", "x": "0", "y": "0", "window": "normal"},
        "layouts": [{"desk": fields, "link": {"path": str(link), "known": known}}]}}
