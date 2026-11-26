# The launcher's laws (LAUNCHER)

**Laws.** The numbered rules the launcher and the roles that ask it keep, each a rule that more than
one file relies on; a comment or a page that relies on one names it by its number. A rule about one
declaration is in that declaration's comment, not here. What may be started, a start and the
listing are [the launcher's manual](zengine.launcher.md). The files and cases that prove each law
are listed in [`tests/laws/launcher.txt`](../../tests/laws/launcher.txt), which the `law_register`
check reads.

## LAUNCHER-01 — The launcher trusts its four askers' approval of the hand

Only the View Builder (`zengine.view.builder`), Files (`zengine.files`), the palette
(`zengine.palette`) and the quick setup (`zengine.first-run`) may ask `zengine.launcher` for a start
(`kAskers`, in `launcher/weave.cpp`). Each first asks Workshop whether the hand that pressed may act
in the classes `build` and `write`, and asks for the start only once Workshop allows it. The
launcher trusts that approval, and does not judge the hand again.

What this means:

- A role that joins the askers takes on the same promise: it asks Workshop for the hand before it
  asks for a start.
- A guest's hand starts nothing on a weaver's host, because Workshop refuses it before any start is
  asked.

What it does not mean: that the launcher trusts what is asked for. What may be started is still
judged against the catalog fixed at configure (`judge_launch`), whoever asks.
