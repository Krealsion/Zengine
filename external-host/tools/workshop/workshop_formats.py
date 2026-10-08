# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Workshop's own setup and session files, as a tool writes them: at the versions the current
Workshop writes, whatever desk it was handed.

SETUP_VERSION and SESSION_VERSION are the setup file's and the session file's `kFormatVersion`
(workshop/setup_persist.hpp, workshop/session_persist.hpp). tests/code_values.txt reads both owners
and docs/workshop/demo-setups.md marks one value for each pair, so a Workshop that moves either
file's version is a red until this module moves with it. A desk a launcher is handed is lifted to
SETUP_VERSION before anything is written around it: a version-4 desk keeps every field and gains
each row's settings, none; another version is refused, naming what to do."""
import copy

SETUP_VERSION = 5
SESSION_VERSION = 8

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
    and each row given no settings at the version before, and refused at any other."""
    version = desk.get("version")
    claimed = desk.get("fields", {}).get("format_version")
    if version == SETUP_VERSION and claimed == str(SETUP_VERSION):
        return copy.deepcopy(desk)
    if version == LIFTED_SETUP_VERSION and claimed == str(LIFTED_SETUP_VERSION):
        fields = copy.deepcopy(desk["fields"])
        return setup(fields["name"], fields["panes"])
    raise ValueError("the desk is setup version %s; a launcher lifts version %d and writes %d -- "
                     "open it in Workshop and save it (`s`) to write version %d"
                     % (claimed, LIFTED_SETUP_VERSION, SETUP_VERSION, SETUP_VERSION))


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
