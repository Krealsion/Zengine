# The app host's laws (APP)

**Laws.** The numbered rules the app host keeps, each a rule that more than one file relies on; a
comment or a page that relies on one names it by its number. A rule about one declaration is in
that declaration's comment, not here. What an app is, its console and its end are [the
reference](app.md). The files that prove each law are listed in
[`tests/laws/app.txt`](../../tests/laws/app.txt), which the `law_register` check reads.

## APP-01 — The app host never holds `zengine.workshop`

`zengine-app` holds the bus, one operator catalog, the Kernel and its control door, the plan's
booter and executor, the Flow host and the view host, the role that says where the app is
(`zengine.app.host`), the close and the console, and nothing else. It never holds
`zengine.workshop`.

What this means: a weave that asks Workshop for something finds no one in an app, and everything an
app does past what a weave cannot hold is a weave, its own weave among them.
