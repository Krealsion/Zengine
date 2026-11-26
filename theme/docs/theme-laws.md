# The themes' laws (THEME)

**Laws.** The numbered rules the theme role and every host that shows a palette keep, each a rule
that more than one file relies on; a comment or a page that relies on one names it by its number. A
rule about one declaration is in that declaration's comment, not here. What a theme is, a stack and
the messages are [the theme role's manual](zengine.theme.md). The files and cases that prove each
law are listed in [`tests/laws/theme.txt`](../../tests/laws/theme.txt), which the `law_register`
check reads.

## THEME-01 — One owner offers each palette name

Where the theme role, `zengine.theme`, runs, it offers every stack's palette to graphics, the
shipped looks among them, each under the name of its stack (`stack_palette`), and the host offers
none. A host where the theme role does not run, such as the snake or an app, offers its own.

What this means: a host that runs the theme role offers no palette of its own, and the theme role
names no host, so no palette name is offered by two owners at once.
