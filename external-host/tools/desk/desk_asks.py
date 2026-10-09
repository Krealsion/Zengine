# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The asks desk/read makes of a running Workshop, through the link, under the guest's own row --
and what each came to, as values the readings are written from.

THE DESK IN ONE TURN. `DeskReadRequested` answers the desk (`DeskView` v3), every presented pane's
stamp front to back, and as many panes' readings (`PaneView` v4) as one decoded value and one
reply's bytes hold. A stamp past the last reading names a pane this reader asks alone, by
`PaneViewRequested` v4: from item 0 under an empty stamp, then page after page from the next item
under the stamp the first page answered, until the page that reaches `total`. Workshop holds
nothing between the asks.

WHAT A PANE'S READ CAN COME TO, each bounded and said, never raised:
  - a page refused as stale or as past a reading that shrank, or answered under another stamp or
    cover, starts that pane again from item 0 -- and one answered in flight, IN_FLIGHT_WAIT
    later -- at most STALE_RESTARTS times; then "not read: its picture moved while it was read";
  - a pane whose newest picture is not yet aimed (`in_flight`, or a refusal saying so) is asked
    alone again IN_FLIGHT_WAIT later, at most IN_FLIGHT_ASKS times in a row; then "not read: its
    newest picture was in flight at every ask";
  - any other refusal is written in its owner's words.

THE GEOMETRY IS ONE DESK READ'S. When any pane was asked after a desk read, the desk is read
again to see whether its number moved before the last page. If it moved, the newer desk read is
the one the panes are read against, at most DESK_REREADS times; then the reading is written
with the span of desk numbers it stands on said. A pane read after the desk read names its own
stamp, so a later picture than the desk read's is said.

A REFUSAL IS AN ANSWER. Every ask's refusal -- the owner's `zen.Refused`, the bus's, the link's,
a shape this host does not resolve, no answer in time -- comes back as words beside the ask, and
a reading carries them. A desk read refused for Workshop's rate (4 a second, each asker) is asked
once more after the wait Workshop names, at most a second. A cancellation is not a refusal: it
ends the run as the context ends it.
"""

import re
import time

from loom_session.client import UnknownShape
from loom_session.tool import DispatchRefused, LinkOutcome, NotAnswered, Refused, SendRefused
from loom_session.values import ShapeError

WORKSHOP = "zengine.workshop"
GUESTS = "zengine.guests"

#: A pane's restarts from item 0 after a page found its picture moved.
STALE_RESTARTS = 3
#: A pane's asks alone while its newest picture is in flight, and the wait before each next one,
#: in seconds: a room just granted is answered by its office's next picture.
IN_FLIGHT_ASKS = 3
IN_FLIGHT_WAIT = 0.1
#: The desk read again when its number moved before the last page ("Consistency is per pane").
DESK_REREADS = 2
#: Pages of one pane at most: a guard against an answer that never reaches its own total. A pane
#: at its full canvas budgets (4,096 words and 2,048 parts) needs a handful.
MAX_PAGES = 4096
#: How long one ask is waited for, in seconds; a wait that runs out is said as not answered.
ASK_SECONDS = 30.0

#: Workshop's words for a desk read past its rate (4 a second, each asker), and the wait they
#: name: the read is asked once more after it, at most a second later.
RATE = re.compile(r"read again in (\d+) ms")

MOVED = "not read: its picture moved while it was read"
IN_FLIGHT = "not read: its newest picture was in flight at every ask"

#: Every way an ask can come back without an answer, each said in words.
REFUSALS = (Refused, DispatchRefused, SendRefused, NotAnswered, LinkOutcome, UnknownShape,
            ShapeError)

EMPTY_STAMP = {"provider": "", "pane": "", "holder": 0, "incarnation": 0, "grant": 0,
               "picture": 0}


def refusal_words(err):
    """An ask that came back without an answer, in words: its owner's own where it gave some."""
    if isinstance(err, Refused):
        return "refused: %s" % (err.reason or "no reason given")
    if isinstance(err, DispatchRefused):
        return "refused by the bus: %s" % err
    if isinstance(err, SendRefused):
        return "refused before the bus: %s" % err
    if isinstance(err, NotAnswered):
        return "not answered in time: %s" % err
    if isinstance(err, LinkOutcome):
        if err.unknown:
            return "the link was lost after the ask was sent: its outcome is unknown (%s)" % (
                err.reason)
        return "the link said %s: %s" % (err.state, err.reason)
    if isinstance(err, UnknownShape):
        return "this host resolves no such shape (%s): the guest vocabulary booted here does not " \
               "declare it" % err
    return "this host's shape did not take the ask: %s" % err


def fields_of(answer):
    """An answer's fields as a plain mapping: an attested Answer's, or a mapping as it is."""
    fields = getattr(answer, "fields", answer)
    return fields if fields is not None else {}


def key(provider, pane):
    return "%s/%s" % (provider, pane)


def key_of(item):
    return key(item.get("provider", ""), item.get("pane", ""))


def stamp_of(view):
    """The stamp a pane's reading stands on: its holder, incarnation, room grant and picture."""
    return {"provider": view.get("provider", ""), "pane": view.get("pane", ""),
            "holder": view.get("holder", 0), "incarnation": view.get("incarnation", 0),
            "grant": view.get("grant", 0), "picture": view.get("picture", 0)}


def same_stamp(a, b):
    return all(a.get(f, 0) == b.get(f, 0) for f in ("holder", "incarnation", "grant", "picture"))


def says_stale(words):
    """A page refused because the reading it continues is no longer the pane's: its stamp moved,
    or what covers the pane moved and the reading shrank below the page."""
    w = (words or "").lower()
    return "stale" in w or "picture moved" in w or "again from the start" in w


def says_in_flight(words):
    w = (words or "").lower()
    return "in flight" in w or "no settled picture" in w or "not yet aimed" in w


def items_of(view):
    return len(view.get("words") or []) + len(view.get("parts") or [])


class Asker(object):
    """The run's asks through one link, each answered or said in words, and counted."""

    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link
        self.asks = 0

    def ask(self, office, shape, version, fields):
        """``(fields, None)`` for an answer, ``(None, words)`` for anything else."""
        self.asks += 1
        try:
            answer = self.ctx.ask(office, shape, fields, version=version, via=self.link,
                                  timeout=ASK_SECONDS)
        except REFUSALS as err:
            return None, refusal_words(err)
        return fields_of(answer), None

    def page(self, provider, pane, start, stamp):
        return self.ask(WORKSHOP, "PaneViewRequested", 4,
                        {"provider": provider, "pane": pane, "from": start,
                         "stamp": dict(stamp or EMPTY_STAMP)})


def said(words, stamp=None, pages=0):
    """A pane's read that came to words, not a reading."""
    return {"view": None, "not_read": words, "stamp": stamp, "pages": pages}


def rest_of(asker, first):
    """The pages after a settled first page, under its stamp: ``(view, None)`` with every word and
    part, or ``(None, "stale")`` when the reading moved, ``(None, "in flight")`` when a newer
    picture is on its way, or ``(None, words)``."""
    provider, pane = first.get("provider", ""), first.get("pane", "")
    stamp = stamp_of(first)
    words = list(first.get("words") or [])
    parts = list(first.get("parts") or [])
    total = first.get("total", 0)
    upto = first.get("from", 0) + len(words) + len(parts)
    pages = 1
    while upto < total:
        if pages >= MAX_PAGES:
            return None, "not read: %d pages reached item %d of %d" % (pages, upto, total)
        page, words_said = asker.page(provider, pane, upto, stamp)
        pages += 1
        if page is None:
            if says_in_flight(words_said):
                return None, "in flight"
            if says_stale(words_said):
                return None, "stale"
            return None, "not read: page from item %d %s" % (upto, words_said)
        if page.get("in_flight"):
            return None, "in flight"
        # A COVER THAT MOVED shifts the items a page counts without moving the stamp: the pane
        # is read again from its first item, as for a picture that moved.
        if not same_stamp(stamp_of(page), stamp) or \
                page.get("total", 0) != total or page.get("covered") != first.get("covered"):
            return None, "stale"
        if page.get("from", 0) != upto:
            return None, "not read: Workshop answered a page from item %d where %d was asked" % (
                page.get("from", 0), upto)
        held = items_of(page)
        if held == 0:
            return None, "not read: the page from item %d of %d held nothing" % (upto, total)
        words += list(page.get("words") or [])
        parts += list(page.get("parts") or [])
        upto += held
    whole = dict(first)
    whole.update({"words": words, "parts": parts, "from": 0, "total": total})
    return {"view": whole, "not_read": None, "stamp": stamp, "pages": pages}, None


def read_pane(asker, provider, pane, given=None):
    """One pane read whole: from `given` (its reading inside the desk read) where that is a
    settled first page, else asked alone. Every outcome is a value; see the module note."""
    in_flight = 0   # asks of this pane alone that its newest picture was still in flight for
    restarts = 0    # starts again from item 0 after its picture moved
    first = given if given is not None and given.get("from", 0) == 0 else None
    from_desk = first is not None
    while True:
        if first is not None and first.get("in_flight"):
            if not from_desk:
                in_flight += 1
                if in_flight >= IN_FLIGHT_ASKS:
                    return said(IN_FLIGHT, stamp_of(first))
            time.sleep(IN_FLIGHT_WAIT)
            first, from_desk = None, False
        if first is None:
            first, words = asker.page(provider, pane, 0, None)
            from_desk = False
            if first is None:
                if says_in_flight(words):
                    in_flight += 1
                    if in_flight >= IN_FLIGHT_ASKS:
                        return said(IN_FLIGHT)
                    time.sleep(IN_FLIGHT_WAIT)
                    continue
                return said("not read: " + words)
            if first.get("in_flight"):
                continue
            in_flight = 0  # a settled first page: "at every ask" counts from here
        done, why = rest_of(asker, first)
        if done is not None:
            return done
        if why not in ("stale", "in flight"):
            return said(why, stamp_of(first))
        restarts += 1
        if restarts > STALE_RESTARTS:
            return said(MOVED, stamp_of(first))
        if why == "in flight":
            time.sleep(IN_FLIGHT_WAIT)
        first, from_desk = None, False


def read_panes(asker, desk_read):
    """Every presented pane of one desk read: ``({key: pane read}, asked)``, `asked` true when
    any pane was asked after the desk read (so its number is checked again)."""
    stamps = list(desk_read.get("stamps") or [])
    given = dict((key_of(v), v) for v in (desk_read.get("panes") or []))
    out, asked = {}, False
    for s in stamps:
        k = key_of(s)
        view = given.get(k)
        if view is not None and not view.get("in_flight") and view.get("from", 0) == 0 and \
                items_of(view) >= view.get("total", 0):
            out[k] = {"view": view, "not_read": None, "stamp": stamp_of(view), "pages": 0}
            continue
        before = asker.asks
        out[k] = read_pane(asker, s.get("provider", ""), s.get("pane", ""), view)
        asked = asked or asker.asks > before
    for k, view in given.items():
        if k not in out:  # a reading the desk read carried without a stamp: kept as it came
            out[k] = {"view": view, "not_read": None, "stamp": stamp_of(view), "pages": 0}
    for k, s in ((key_of(s), s) for s in stamps):
        out[k]["desk_stamp"] = s
    return out, asked


def desk_number(desk_read):
    return (desk_read.get("desk") or {}).get("desk", 0)


def ask_desk(asker):
    """One desk read. Refused for its rate, it is asked once more after the wait Workshop names
    (at most a second); any other refusal, or a second one, is the answer."""
    answer, words = asker.ask(WORKSHOP, "DeskReadRequested", 1, {})
    wait = RATE.search(words or "")
    if answer is None and wait:
        time.sleep(min(int(wait.group(1)), 1000) / 1000.0)
        answer, words = asker.ask(WORKSHOP, "DeskReadRequested", 1, {})
    return answer, words


def read_desk(asker):
    """The desk, every pane read whole where it can be, and the desk numbers it stands on."""
    base, words = ask_desk(asker)
    if base is None:
        return {"desk": None, "refused": words}
    numbers = [desk_number(base)]
    rereads = 0
    while True:
        panes, asked = read_panes(asker, base)
        reading = {"desk": base.get("desk") or {}, "refused": None, "panes": panes,
                   "stamps": list(base.get("stamps") or []), "numbers": numbers,
                   "moved": False, "unconfirmed": None, "rereads": rereads}
        if not asked:
            return reading  # one turn: nothing was asked after it, so nothing moved under it
        again, words = ask_desk(asker)
        if again is None:
            reading["unconfirmed"] = words
            return reading
        if desk_number(again) == numbers[-1]:
            return reading
        numbers.append(desk_number(again))
        if rereads >= DESK_REREADS:
            reading["moved"] = True
            return reading
        rereads += 1
        base = again


def read_all(ctx, link):
    """Everything one desk/read asks, in order: the asker's own row, the desk and its panes, the
    pane inventory and the keymap."""
    asker = Asker(ctx, link)
    ctx.step("own row")
    row, row_refused = asker.ask(GUESTS, "GuestRowDescribedRequested", 1, {})
    ctx.step("desk")
    reading = read_desk(asker)
    ctx.step("inventory")
    inventory, inventory_refused = asker.ask(WORKSHOP, "PaneInventoryRequested", 1, {})
    ctx.step("keymap")
    keymap, keymap_refused = asker.ask(WORKSHOP, "KeymapRequested", 1, {})
    reading.update({"row": row, "row_refused": row_refused, "inventory": inventory,
                    "inventory_refused": inventory_refused, "keymap": keymap,
                    "keymap_refused": keymap_refused, "asks": asker.asks})
    return reading
