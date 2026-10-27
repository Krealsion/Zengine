# zengine.view.host -- the view host

**Manual.** The view host: the weave that runs described views, answering to the office
`zengine.view.host` (an office is the name a weave answers to). Given a view's description, it
registers the view as a participant of its own, changes it, and stops it. A Zengine host mounts
it -- Workshop, and an app's own host, `zengine-app` -- so it is not a file you load. In Workshop
the [View Builder](zengine.view.builder.md) is the weave that asks it, and in an app the app's own
weave ([running an app on its own](../../app/docs/app.md)).

## About

The view host is `view::Host` and its office's weave, `view::Manager`, in
[`view/host.hpp`](../host.hpp). Each view it runs is described as data, a `view::Description`
([`view/description.hpp`](../description.hpp)), and becomes its own participant, a `view::View`
([`view/view.hpp`](../view.hpp)), holding the description's `name` as its office. The host keeps
no view of its own: what a view shows and says is its description's
([Described views](view.md)).

A view's authority comes from its description alone, never from whoever asked: its grant,
`view::view_grant`, lets it send each intent the description declares to any weave that accepts
it, and its pane's conversation to `zengine.workshop` alone.

Every view belongs to a session, named by the asker and scoped to it: to the office the ask was
sent as, while that office's holder is still the one asking, or else to the asker's own identity
on the bus. Another weave cannot reach a session by guessing its name. One host runs at most
`view::kMaxViews` views at once ([`view/host.hpp`](../host.hpp)).

## Use

The View Builder, `zengine.view.builder`, or any weave granted `view::allow_view_requests`, sends
[`ViewRun`](#viewrun) to start a view, [`ViewResume`](#viewresume) to bring one back after a
relaunch, [`ViewApply`](#viewapply) to change it and [`ViewStop`](#viewstop) to end it. Each ask is
answered once, to the asker alone, with `ViewAnswer` v1 `{session, action, ok, reason, office,
fresh}`: `office` is the view's name, `fresh` whether this ask registered a participant afresh, and
`reason` what happened, or why nothing did. Any weave granted `ShapeDocumentRequested` to this
office may ask what a shape it accepts does, and is answered from this page.

## Commands

### `ViewRun`

`ViewRun` v1 runs a view from its description, as a participant of its own.

- **Takes:** `session`, a name of 1 to 128 bytes; `description`, a view's description as its file's
  bytes, within the maker file limit `maker::kMaxFileBytes`.
- **Answers:** `ViewAnswer` with `action` `run`, `ok`, `fresh`, `office` the view's name, and
  `reason` `registered example.panel`, adding `; it waits until told` and the shapes it shows when
  its fields show what other weaves tell it. The view then offers its pane and asks Workshop to show
  it.
- **Refuses:** `ok` false, nothing registered, with `reason`: `a live requesting participant is
  required`; `a session name is 1 to 128 bytes`; `the request's authored office has moved to another
  participant`; `this session already runs a view; apply a change or stop it first`; `a description
  exceeds the file limit`; the reason a description is not read; `the view host already runs <n>
  views`, `<n>` being `view::kMaxViews`; `` `example.panel` is already held by a running
  participant ``; or Loom's refusal to register it.
- **Example:** from the View Builder,
  `mail.as_role("zengine.view.builder").send_to_role(zengine::view::kViewHostRole, zengine::view::ViewRun{"workshop", bytes});`

### `ViewResume`

`ViewResume` v1 runs a view as `ViewRun` does, asking nothing of the desk: what a relaunched View
Builder sends for a view that was running.

- **Takes:** `session` and `description`, as `ViewRun`.
- **Answers:** `ViewAnswer` as `ViewRun`'s, with `action` `resume`. The view offers its pane without
  asking Workshop to show it, so a desk that names the pane seats it where it stood, and one that
  does not leaves it off.
- **Refuses:** as `ViewRun`.
- **Example:**
  `mail.as_role("zengine.view.builder").send_to_role(zengine::view::kViewHostRole, zengine::view::ViewResume{"workshop", bytes});`

### `ViewApply`

`ViewApply` v1 changes the view a session runs to a new description.

- **Takes:** `session`, one this asker runs; `description`, the new description's bytes.
- **Answers:** `ViewAnswer` with `action` `apply`, `ok`, and `office` the view's name. At the same
  name, telling and saying the same shapes, the running view takes it in place, its field text,
  focus and told values kept: `fresh` false, `reason` `applied in place: example.panel tells and
  says the same shapes`. Any other change registers a successor first, then retires the running
  view: `fresh` true, `reason` `registered example.panel afresh: its shapes changed`; the successor
  asks nothing of the desk, so its pane stands where it stood and the keys stay where they are.
  Under a new name, the running view's last picture names its successor, which asks Workshop to
  show it as a run does, and the two count once against `view::kMaxViews`.
- **Refuses:** `ok` false with `reason`: `a live requesting participant is required`; `a session
  name is 1 to 128 bytes`; `the request's authored office has moved to another participant`; `no
  view runs in this session for this participant`; `the view is no longer running`; `a description
  exceeds the file limit`; the reason a description is not read; or a successor's refusal, from the
  host or from Loom. A refused change leaves the running view as it was.
- **Example:**
  `mail.as_role("zengine.view.builder").send_to_role(zengine::view::kViewHostRole, zengine::view::ViewApply{"workshop", bytes});`

### `ViewStop`

`ViewStop` v1 stops the view a session runs.

- **Takes:** `session`, one this asker runs.
- **Answers:** `ViewAnswer` with `action` `stop`, `ok`, `office`, and `reason` `stopped
  example.panel`. The view's last picture says `stopped; run it again from the View Builder`, and
  the participant is unregistered once that picture is delivered.
- **Refuses:** `ok` false with `reason`: `a live requesting participant is required`; `a session
  name is 1 to 128 bytes`; `the request's authored office has moved to another participant`; `no
  view runs in this session for this participant`; or `the view is no longer running`.
- **Example:**
  `mail.as_role("zengine.view.builder").send_to_role(zengine::view::kViewHostRole, zengine::view::ViewStop{"workshop"});`

## Hears

### `ViewRetire`

`ViewRetire` v1, from the view host itself: a stopped or renamed view's last picture is queued
ahead of this note, so the view is unregistered now. A note from any other sender is ignored.

## See also

- [`zengine.view.builder`](zengine.view.builder.md), the View Builder, which asks this host.
- [Make a small panel by hand in the View Builder](view-builder.md).
- [Described views](view.md), the reference: a view's description, its renderer and this host.
- [`view/vocabulary.hpp`](../vocabulary.hpp), the shapes.
