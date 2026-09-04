# Verification method — dependencies

Register `VM-DEP`: judging a dependency, a package option, a toolchain — the census before the
argument, the cost in configure, the install diff, and subtracting in the green. One method per
heading; cite by ID. Router: [`../verification.md`](../verification.md).

## VM-DEP-01 — Judge a dependency by its consumers in the build graph

METHOD — Judge a dependency by its CONSUMERS in the generated build graph, never by grep references: parse the link lines for every target naming the library, then census those targets' consumers out to the downstreams.
BECAUSE — a grep across two repositories counted over a hundred references; the build graph said
three targets, all non-installed, and every consumer of those already opted out.
SEEN — nowhere yet

## VM-DEP-02 — Measure configure cost apart from build wall

METHOD — Measure configure cost apart from build wall, and quote wall and CPU separately: a dependency's compilation is embarrassingly parallel and fills idle cores, so the wall clock hides what configure paid.
BECAUSE — a dependency cost thirty-eight seconds of configure and under two seconds of build
wall at twenty-four jobs, because its compilation filled cores that were idle anyway.
SEEN — nowhere yet

## VM-DEP-03 — Same name is not same dependency

METHOD — Same name is not same dependency: two repositories share a dependency only when major version, linkage model, acquisition path and consumers all match.
BECAUSE — one repository fetched a static major version and the other a shared one it refuses to
link statically: no shared cost to reclaim and no duplication to collapse.
SEEN — nowhere yet

## VM-DEP-04 — Prove a package contract by diffing two installs

METHOD — Prove a package contract by diffing two installs — the option on and off — comparing file sets and per-archive defined-symbol sets; identical sets say the option is invisible to every consumer.
BECAUSE — fifty-seven identical files and five archives with identical defined-symbol sets said
the option was invisible to every consumer, cheaper and stronger than reading export lists.
SEEN — nowhere yet

## VM-DEP-05 — Subtract in the green

METHOD — Subtract in the green: land the replacement coverage and run the full lane with the doomed thing still present, so floors observably rise before anything falls; then delete.
BECAUSE — that run makes the population contract a witness to the whole edit: floors rise before
anything falls, and a red in the deletion step cannot be confused with a red in the addition.
SEEN — nowhere yet

## VM-DEP-06 — Coverage that dies with its subject is not coverage lost

METHOD — Coverage that dies with its subject is not coverage lost: count what a deletion costs per case, and rehome the case that guards a symbol the deletion leaves alive.
BECAUSE — two of three cases in a doomed suite exercised the thing being deleted; the third
guarded a symbol that stayed and was rehomed to the suite that owns it.
SEEN — nowhere yet

## VM-DEP-07 — A stranger lane can reach a different package than the one it was handed

METHOD — A stranger lane can silently reach a different package than the one it was handed: pin the package directory and disable the package registry, or a CRT mismatch reads like a broken repository.
BECAUSE — a stranger lane resolved a stale install from the package registry instead of the
prefix it was handed, and eighteen CRT mismatches read exactly like a broken repository.
SEEN — nowhere yet

## VM-DEP-08 — Run the OFF configuration's full lane before recommending a removal

METHOD — Run the OFF configuration's full lane before recommending a removal: an option whose OFF path is already a shipped default has answered most of the question.
BECAUSE — an option whose OFF path is a shipped default has answered most of the question
already; the census before the argument is what makes the recommendation a measurement.
SEEN — nowhere yet
