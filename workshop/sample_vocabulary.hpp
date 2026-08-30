// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SAMPLE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_SAMPLE_VOCABULARY_HPP

// THE TWO SENTENCES AN EXPLICIT SAMPLE COSTS (SOURCE-1), and there is not a third.
//
//     SampleRequested   asker  ->  zengine.sources   "sample this identity, now."
//     SourceSampled     the door -> the asker        "here is what it said."
//
// ---- WHY THEY ARE NOT THE ARRANGEMENT DOOR'S ---------------------------------
//
// `arrangement_vocabulary.hpp` carries two questions whose whole charter is that
// answering them cannot run anybody's code. These two are the opposite: the first
// one's ONLY purpose is to cause an evaluator to run. Filing them beside the
// observation shapes would have made "which office can cause evaluation" a
// question with no one-word answer, and that sentence is worth more than the file
// it costs (workshop/sample_door.hpp says the rest).
//
// ---- WHAT CROSSES, AND WHAT CANNOT --------------------------------------------
//
// THE ASK CARRIES AN IDENTITY AND NOTHING ELSE. There is no argument pack, no
// schema, no provider preference, no timeout and no "give me the fresh one"
// flag: a Source is an operator with zero maker inputs (SOURCE-0), so an identity
// IS the whole question, and every other field would be a policy the asker was
// never entitled to author.
//
// THE ANSWER CARRIES LINES OF TEXT AND NEVER A VALUE. A sampled `loom::Value`
// claims a schema this wire has no way to name in advance -- the asker is a woven
// weave whose accept-set is closed at compile time, and raw bytes without schema
// custody explain no field to anybody. So the HOST renders the value where the
// schema still is (workshop/sample_presentation.hpp) and the rendering crosses as
// ordinary prose. What a consumer gets is a picture; what it gets no way to do is
// evaluate anything.
//
// `ok` AND `reason` ARE TWO FIELDS AND NOT ONE, for `Evaluation`'s own reason: a
// refusal is a SENTENCE somebody owns, and the empty string is not a refusal. A
// refused sample carries `lines` empty and `reason` set; an accepted one carries
// the reverse. Nothing here re-words a refusal -- `op::sample` and
// `op::Catalog::evaluate` own every one of them, and the door quotes them.
//
// ---- WHAT IT DOES NOT PROMISE -------------------------------------------------
//
// NOTHING ABOUT FRESHNESS. There is no timestamp, no sequence, no generation and
// no "still current" bit, because a sample is what a Source said at the moment it
// was asked and nothing here observes it again. A consumer that retains one
// retains HISTORY; saying so is that consumer's obligation and this shape
// deliberately gives it nothing to claim otherwise with.

#include <zen/weave/shape.hpp>

#include <string>
#include <vector>

namespace zengine::workshop {

/// THE OFFICE THAT MAY CAUSE A SOURCE TO BE EVALUATED, and the only address
/// anything reaches it by. A ROLE for `kArrangementRole`'s reason exactly: it
/// survives its holder being replaced, and a loaded artifact names it without
/// ever learning a `WeaveId`.
///
/// IT IS A SECOND OFFICE AND NOT A SECOND MECHANISM. The seam is the one INTR-1
/// already built -- ask an office, hear an answer -- pointed at the one act the
/// observation door refuses to perform. A host that mounts no sample door holds
/// no such office and an ask sent to it reaches nobody, which is the correct
/// answer for a host with no catalog to sample.
inline constexpr const char* kSampleRole = "zengine.sources";

/// SAMPLE THIS SOURCE, NOW.
///
/// THE IDENTITY IS RESOLVED AT THE SPEND AND NOT HERE. Between a maker reading a
/// row and pressing it, a provider may have been unmounted; the door resolves
/// current catalog truth when it acts, so this shape carries a NAME and never a
/// definition, a contribution, a provider or an index into anybody's list.
struct SampleRequested {
    std::string identity;
    ZEN_SHAPE(SampleRequested, 1, ZEN_FIELD(identity));
};

/// WHAT THAT SOURCE SAID WHEN IT WAS ASKED.
///
/// `identity` IS ECHOED BECAUSE A CONSUMER MAY HAVE MOVED ON. The answer is
/// correlated by Loom, which says WHICH ask it answers; the identity says what
/// the ask was ABOUT, so a pane can label a retained answer without keeping a
/// second book of what it asked for.
struct SourceSampled {
    std::string identity;
    bool ok = false;
    /// The refusal, in the words of whichever layer owns it -- empty when `ok`.
    std::string reason;
    /// The rendered value, one logical line per row -- empty when it was refused.
    std::vector<std::string> lines;

    ZEN_SHAPE(SourceSampled, 1, ZEN_FIELD(identity), ZEN_FIELD(ok), ZEN_FIELD(reason),
              ZEN_FIELD(lines));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SAMPLE_VOCABULARY_HPP
