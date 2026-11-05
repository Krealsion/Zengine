# Night, day and block ship; classic is night

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [chrome](../workshop/chrome.md).

**Context.** Colours named by use left four shipped themes where the skins had been: `classic`, the
pool's own; `block`, classic's light with a fill character and an ink of its own for each colour in a
terminal; `day`; and `night`. In a window classic, block and night painted the same bytes; in a
terminal classic, day and night did, every colour in the terminal's one ink.

**Decision.** Three looks ship and are offered: `night`, the pool's own, light on a cool dark ground;
`day`, dark on light; and `block`, the terminal look, the one theme that changes how a terminal draws.
`classic` is another name for `night`: a row, a setting or an `app.json` naming it wears night, and
nothing offers it. A terminal shows text and every ground in its default colours, a pane's frame in grey (`bright
black`) in every look so it reads on a dark terminal and a light one, and each other colour of
night and day as the nearest of its sixteen inks; block keeps an ink of its own for each.

**Alternatives considered.**
- *Argued: all four ship* -- two names for one look in a window and three in a terminal, a choice
  that changes nothing on screen.
- *Argued: block a tests' fixture alone* -- it is the only look a terminal shows otherwise, and the
  tests and the snake keep it.
- *Argued: the terminal's default ink for every colour of night and day* -- a terminal would show no
  colour at all; an accent would read as text.

**Consequences.** A saved file that names `classic` keeps working and wears night; the setting's row
offers night, day and block. Block worn over night or day, as a layer, waits for themes that merge.

**Laws supported.** [WL-CHROME-10](../workshop/chrome.md).
