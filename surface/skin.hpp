#ifndef ZENGINE_SURFACE_SKIN_HPP
#define ZENGINE_SURFACE_SKIN_HPP

// The Skin weave shell, over an injected Medium (the Input package's Reader
// move, pointed at output). A Medium is anything with:
//
//   void frame(const zengine::snake::SnakeVisual&, bool first);
//   void note(std::string_view slot, std::string_view text);
//
// The real ones own an actual surface RAII-style — the terminal medium enters
// the alternate screen in its constructor and restores it in its destructor,
// the SDL medium opens and closes a window — so loading a Skin claims the
// surface and unloading releases it, with no cleanup protocol to forget. The
// suite's fake medium just records calls, so the weave's whole message
// contract is pinned without a terminal or a window in sight.
//
// The shell is deliberately dumb: count, delegate, and say hello exactly once.
// Everything visual lives in the Medium; everything wire-shaped lives here.
// A Medium also provides `void pump()` — service your OS surface, nothing
// else — driven by the host's PumpSurface lap message (see vocabulary.hpp);
// media with nothing to service keep it empty.

#include "vocabulary.hpp"

#include "snake/vocabulary.hpp" // the V1 canvas payload — see vocabulary.hpp on the coupling

#include <zen/weave.hpp>

#include <cstdint>
#include <utility>

namespace zengine::surface {

/// Two honest counters, poke-inspectable like any state: frames painted and
/// text notes delivered (delivered, not necessarily rendered — a slot the
/// Medium doesn't know is dropped there, and the golden tests pin which).
struct SkinState {
    std::int64_t frames = 0;
    std::int64_t texts = 0;
    std::int64_t pumps = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(SkinState, 1, ZEN_FIELD(frames), ZEN_FIELD(texts), ZEN_FIELD(pumps));
};

template <class Medium>
class SkinT : public loom::WeaveBase<SkinT<Medium>, SkinState,
                                     loom::Accept<zengine::snake::SnakeVisual, SurfaceText,
                                                  PumpSurface>,
                                     loom::Emit<SurfaceReady>> {
public:
    SkinT() = default;
    explicit SkinT(Medium medium) : medium_(std::move(medium)) {}

    void on(const zengine::snake::SnakeVisual& v, loom::Mail& mail) {
        hello_once(mail);
        medium_.frame(v, this->state_.frames == 0);
        ++this->state_.frames;
    }

    void on(const SurfaceText& t, loom::Mail& mail) {
        hello_once(mail);
        medium_.note(t.slot, t.text);
        ++this->state_.texts;
    }

    /// Execution time, not intent: service the medium's OS surface. On a
    /// pumped host this is also the skin's earliest first message, so the
    /// hello (and the text rows it re-summons) no longer waits for a frame.
    void on(const PumpSurface&, loom::Mail& mail) {
        hello_once(mail);
        medium_.pump();
        ++this->state_.pumps;
    }

    Medium& medium() { return medium_; }

private:
    /// One hello per INCARNATION, not per identity: a deliberate plain member
    /// (the v2 world's `asked_` stance), never state — a successor or a
    /// reloaded instance re-claims its surface, so it must re-announce even
    /// where state rides across.
    void hello_once(loom::Mail& mail) {
        if (announced_) {
            return;
        }
        announced_ = true;
        mail.publish(SurfaceReady{});
    }

    bool announced_ = false;
    Medium medium_;
};

} // namespace zengine::surface

#endif // ZENGINE_SURFACE_SKIN_HPP
