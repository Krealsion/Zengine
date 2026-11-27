# Security

Report a security flaw privately, never in an issue, a pull request or a discussion: those are
public, and a flaw told there is known before it is fixed. Use GitHub's private vulnerability
reporting, **Report a vulnerability** on this repository's
[Security tab](https://github.com/Krealsion/Zengine/security), which opens an advisory only the
maintainers and you can read. The fix is made there, and the advisory is published once it has
landed. A flaw in Loom itself, the substrate Zengine builds on, goes to
[Loom's](https://github.com/Krealsion/Loom/security) instead; one you cannot place, to either.

Say what the flaw lets someone do, how to reproduce it, and the commit you found it at. A test that
shows it is the most useful report there is.

What Zengine's weaves can be trusted with is written down. A weave in Workshop's process shares its
memory, and a grant bounds what a weave may say, never what it may touch
([recurring principles](docs/architecture/README.md#recurring-principles)); what does not work yet
is [the limitations](workshop/docs/limitations.md). A participant that can do more than those pages
say it can is a flaw.
