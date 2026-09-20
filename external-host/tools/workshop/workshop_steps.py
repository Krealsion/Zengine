# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Steps the Workshop tools share: the link's own session, this session's inventory row, a
picture fetched and checked chunk by chunk, and key chords spelled as Workshop's Input owner
spells them (zengine/input/vocabulary.hpp: scancodes are SDL's, modifiers shift=1 ctrl=2 alt=4)."""

import json

from loom_session.values import BOOL, BYTES, FLOAT, INT, TEXT

ZERO = {INT: 0, FLOAT: 0.0, TEXT: "", BOOL: False, BYTES: b""}

LETTERS = dict((chr(ord("a") + i), 4 + i) for i in range(26))
DIGITS = dict((str((i + 1) % 10), 30 + i) for i in range(10))
NAMED = {"enter": 40, "return": 40, "escape": 41, "esc": 41, "backspace": 42, "tab": 43,
         "space": 44, "right": 79, "left": 80, "down": 81, "up": 82}
NAMED.update(dict(("f%d" % (i + 1), 58 + i) for i in range(12)))
MODIFIERS = {"shift": 1, "ctrl": 2, "control": 2, "alt": 4}


def chord(spelling):
    """``(scancode, modifiers)`` for a chord such as ``ctrl+p`` or ``down``."""
    parts = [p.strip().lower() for p in spelling.split("+") if p.strip()]
    if not parts:
        raise ValueError("an empty chord")
    mods = 0
    for p in parts[:-1]:
        if p not in MODIFIERS:
            raise ValueError("'%s' is not a modifier (shift, ctrl, alt)" % p)
        mods |= MODIFIERS[p]
    key = parts[-1]
    code = LETTERS.get(key) or DIGITS.get(key) or NAMED.get(key)
    if code is None:
        raise ValueError("'%s' is not a key this tool can spell" % key)
    return code, mods


def moment(ctx, kind, **given):
    """One WHOLE ``InjectedEvent`` of ``kind``: Workshop's gate refuses a moment that leaves out a
    field its shape declares. The field list is the host's own descriptor, never restated here;
    a field this moment does not use is zero, the default zengine/input/vocabulary.hpp declares
    for every one of them."""
    shape = ctx.conn.schema("InjectedEvent", 1)
    out = {}
    for f in shape.fields:
        if f.name == "kind":
            out[f.name] = kind
        elif f.name in given:
            out[f.name] = given.pop(f.name)
        elif f.type.kind in ZERO:
            out[f.name] = ZERO[f.type.kind]
        else:
            raise ValueError("InjectedEvent v1 declares '%s', which has no zero this tool can "
                             "supply" % f.name)
    if given:
        raise ValueError("InjectedEvent v1 declares no field %s" % ", ".join(sorted(given)))
    return out


def chord_moments(ctx, spelling, text=""):
    """The moments of one chord -- pressed, released -- and optionally the text typed after it."""
    code, mods = chord(spelling)
    events = [moment(ctx, "KeyPressed", scancode=code, modifiers=mods),
              moment(ctx, "KeyReleased", scancode=code, modifiers=mods)]
    if text:
        events.append(moment(ctx, "TextEntered", text=text))
    return events


def link_session(ctx, link):
    """The link's own word about the far session it holds, and as whom; the run fails unless it
    is admitted."""
    s = ctx.ask("loom.link." + link, "loom.link.StatusRequested", {})
    ctx.check(s["state"] == "admitted", "the link '%s' is %s%s" % (
        link, s["state"], (": " + s["detail"]) if s["detail"] else ""))
    return s


def own_row(ctx, link, status):
    """The guest door's inventory, and the row that IS this link's far session -- matched by the
    far session the link holds, never by being the last admitted row."""
    inv = ctx.ask("zengine.guests", "GuestConnectionsRequested", {}, via=link)
    mine = [r for r in inv["rows"] if r["session"] == status["session"]]
    ctx.check(mine, "zengine.guests lists %d connection(s) and none is this link's far session %d"
              % (len(inv["rows"]), status["session"]))
    row = mine[0]
    ctx.check(row["state"] == "admitted" and row["established"] == status["established_name"],
              "zengine.guests lists this session as %s '%s', not admitted as '%s'"
              % (row["state"], row["established"], status["established_name"]))
    ctx.produce("connections.json", json.dumps(inv.fields, indent=1).encode("utf-8"))
    return inv, row


def fetch_picture(ctx, link, captured):
    """Every byte of one retained picture, each chunk checked to continue it."""
    total = captured["bytes"]
    data = bytearray()
    while len(data) < total:
        c = ctx.ask("zengine.skin", "SurfaceCaptureChunkRequested",
                    {"capture": captured["capture"], "offset": len(data)}, via=link)
        piece = c["data"]
        ctx.check(c["capture"] == captured["capture"] and c["offset"] == len(data) and
                  c["total"] == total and piece and len(data) + len(piece) <= total,
                  "zengine.skin sent a chunk of capture %d at %d (%d of %d bytes) where capture "
                  "%d at %d of %d was asked for" % (c["capture"], c["offset"], len(piece),
                                                    c["total"], captured["capture"], len(data),
                                                    total))
        data += piece
    return bytes(data)


def check_picture(ctx, captured, data):
    """A picture is what its format says it is, whole."""
    ctx.check(captured["ok"], "zengine.skin refused the picture: %s" % captured.get("refusal"))
    ctx.check(len(data) == captured["bytes"], "the picture is %d bytes, not the %d described"
              % (len(data), captured["bytes"]))
    if captured["format"] == "image/bmp":
        ctx.check(data[:2] == b"BM" and int.from_bytes(data[2:6], "little") == len(data),
                  "the picture does not read as the whole BMP it claims to be")
        return "bmp"
    ctx.check(captured["format"] == "text/cells", "an unknown picture format '%s'"
              % captured["format"])
    return "cells.txt"


def picture(ctx, link, name, after_frame=-1, pending=None):
    """Ask for a picture (or collect one already asked for), fetch it whole and keep it as
    ``<name>.<bmp|cells.txt>``. Returns ``(captured, bytes)``."""
    captured = pending.wait(60.0) if pending is not None else ctx.ask(
        "zengine.skin", "SurfaceCaptureRequested", {"after_frame": after_frame}, via=link)
    data = fetch_picture(ctx, link, captured) if captured["ok"] else b""
    ext = check_picture(ctx, captured, data)
    ctx.produce("%s.%s" % (name, ext), data)
    return captured, data
