# Verification method — reading a live witness

Register `VM-WIT`, its second file: reading what the witness shows — the frame, the diff, the
title, the pixel count — and asking what each oracle would say if the act had no effect. Driving
the witness is in [`witnesses.md`](witnesses.md). One method per heading; cite by ID. Router:
[`../verification.md`](../verification.md).

## VM-WIT-08 — Crop every calibration diff to the region being calibrated

METHOD — Crop every calibration diff to the region being calibrated: a calibration that spans two bands has measured neither, and a witness whose ruler is wrong reports the product as broken in the shape of the change.
BECAUSE — a keystroke that changes the top band also writes the notice at the foot, so an
uncropped diff spanned the window and put the derived band centre at the middle of the client.
SEEN — nowhere yet

## VM-WIT-09 — A TUI frame needs a cursor model

METHOD — A TUI frame needs a cursor model (addressing, CR/LF, erase); then the painted text is the best oracle: re-read after a full repaint, scope a row test to its own columns, expect the chrome glyph at column 0.
BECAUSE — splitting a frame on newlines gave row 1312 of a 44-row terminal and splitting on
cursor addressing gave one row holding the whole screen; a short wait caught partial frames.
SEEN — nowhere yet

## VM-WIT-10 — Three oracles, none of them "did the process exit"

METHOD — Three oracles, none "did the process exit": the title for text; a before/after frame diff with a tolerance, the notice band cropped, an idle pair first; a pixel count along a row for a claim about ink.
BECAUSE — a colour filter drifts a unit between shots, the notice band stretches every box to
the window's width, and neither the title nor a diff can say how thick a line is.
SEEN — nowhere yet

## VM-WIT-11 — A row scan tells an edge from a letter by a perpendicular run

METHOD — A row scan tells an edge from a letter by reporting each run of one colour whose colour also holds a long PERPENDICULAR run; the same scan counts inked row bands to measure how many prose rows a pane shows.
BECAUSE — no glyph makes a sixty-pixel vertical run, so a colour that holds one is an edge; the
same scan read a twelve-pixel border off one phase's frames and a one-pixel one off the next.
SEEN — nowhere yet

## VM-WIT-12 — Ask what the oracle would say if the act had no effect

METHOD — Ask of every oracle what it would say if the act had NO effect, and whether the ink measured is produced by the act itself; read a claim from outside the mode that was editing it.
BECAUSE — a search for the pane a ring addressed used "the reserved rows changed" as its test,
and addressing draws handles on those rows; the search reported success and the pane never moved.
SEEN — nowhere yet

## VM-WIT-13 — A read through the product's own channel may also write it

METHOD — A check that reads through the product's own channel may also WRITE it: ask of every step what it repairs, order the hypothesis-destroying reads last, and when a falsifier survives suspect the sequence first.
BECAUSE — an unmaximize before a second close handed the application a normal extent through the
ordinary channel and repaired its remembered room; a sharp falsifier passed fourteen of fourteen
against a half-broken build until the close moved first.
SEEN — nowhere yet

## VM-WIT-20 — Diff the pane's own slot when a neighbour changes by design

METHOD — Diff the pane's own slot, not the frame, when a neighbour changes with the subject by design; Enter on a list row keeps the keys on the list; a windowed list hides a row until the cursor reaches it.
BECAUSE — a same-picture-after-restart check must diff the pane's slot and separately expect the
manager's slot to differ, because the manager's rows change with the subject by design.
SEEN — nowhere yet

## VM-WIT-22 — Read a medium fact out of the medium's own bytes

METHOD — Read a medium fact out of the medium's own bytes (the mouse mode a skin asks for) rather than asserting it.
BECAUSE — the skin asks for button events and never for every motion, so an idle pointer is
reported to nobody there; the bytes it sends say so and an assertion would only repeat them.
SEEN — `surface/skin_tui.hpp` `kTuiPointerOn`.
