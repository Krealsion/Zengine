// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_ACTIVATION_ACTIVATION_HPP
#define ZENGINE_ACTIVATION_ACTIVATION_HPP

// The activation cursor: whether a weave acts on an arriving `zen.Activated`. What an activation
// means, and who may attest one, are the Loom's (its lifecycle laws); the cursor reads the Loom's
// attestation and keeps one lineage. Not a lifecycle and not a scheduler: what a weave does with
// an accepted activation is its own. Reference: activation/docs/activation.md.

#include <zen/switchboard/message.hpp>
#include <zen/weave.hpp>
#include <zen/weave/lifecycle.hpp>

#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace zengine {

/// The activation a weave is living under. A new cursor is unactivated, so nothing sent to an
/// earlier incarnation can make this one act, whichever arrives first.
class ActivationCursor {
public:
    /// Offers an arriving activation. True iff it becomes the current one, which is when the weave
    /// does its once-per-activation work. False, changing nothing, for:
    /// - an activation the Loom did not attest (`Mail::lifecycle_attested`), whoever sent it;
    /// - one whose attested sequence (`Mail::attested_sequence`) is not the payload's;
    /// - an invalid sender, or a sequence below 1;
    /// - from the current sender, a sequence no newer than the current one: a duplicate or a
    ///   replay.
    /// An attested activation from a different sender begins a new lineage at any positive
    /// sequence. It takes the whole `Mail` because both deciding facts are delivery facts, which
    /// no payload field can carry.
    bool accept(const loom::Mail& mail, const loom::Activated& activated) {
        if (!mail.lifecycle_attested()) {
            return false;
        }
        if (mail.attested_sequence() != activated.sequence) {
            return false;
        }
        const loom::WeaveId sender = mail.sender();
        const std::int64_t sequence = activated.sequence;
        if (!sender.valid() || sequence <= 0) {
            return false;
        }
        if (activated_ && sender == sender_ && sequence <= sequence_) {
            return false;
        }
        sender_ = sender;
        sequence_ = sequence;
        activated_ = true;
        return true;
    }

    /// The current activation. Before the first accepted one: false, an invalid id and 0.
    bool activated() const { return activated_; }
    loom::WeaveId sender() const { return sender_; }
    std::int64_t sequence() const { return sequence_; }

    /// The current sender as it travels on a wire: canonical decimal Text, because a `WeaveId` is
    /// unsigned 64-bit and a wire `Int` is signed. "0" before the first accepted activation.
    std::string sender_text() const { return std::to_string(sender_.value); }

    /// Whether a carried key, a `sender_text()` and a sequence, names the current activation.
    /// Both halves must match; false before the first accepted activation.
    bool matches(const std::string& sender_text_, std::int64_t sequence) const {
        return activated_ && sequence == sequence_ && sender_text_ == sender_text();
    }

private:
    loom::WeaveId sender_{};
    std::int64_t sequence_ = 0;
    bool activated_ = false;
};

namespace detail {

template <class List, class T>
struct Contains : std::false_type {};

template <template <class...> class List, class T, class... Ts>
struct Contains<List<Ts...>, T> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template <class Acc, class Next>
struct AppendUniqueOne;

template <template <class...> class List, class... Ts, class U>
struct AppendUniqueOne<List<Ts...>, U> {
    using type = std::conditional_t<(std::is_same_v<Ts, U> || ...), List<Ts...>, List<Ts..., U>>;
};

template <class Acc, class Right>
struct MergeLists;

template <template <class...> class List, class... Ts>
struct MergeLists<List<Ts...>, List<>> {
    using type = List<Ts...>;
};

template <template <class...> class List, class... Ts, class U, class... Us>
struct MergeLists<List<Ts...>, List<U, Us...>> {
    using type = typename MergeLists<typename AppendUniqueOne<List<Ts...>, U>::type,
                                     List<Us...>>::type;
};

template <class Init, class... Lists>
struct UnionLists {
    using type = Init;
};

template <class Init, class Next, class... Rest>
struct UnionLists<Init, Next, Rest...> {
    using type = typename UnionLists<typename MergeLists<Init, Next>::type, Rest...>::type;
};

template <class F, class = void>
struct FacetAccept {
    using type = loom::Accept<>;
};

template <class F>
struct FacetAccept<F, std::void_t<typename F::Accept>> {
    using type = typename F::Accept;
};

template <class F, class = void>
struct FacetEmit {
    using type = loom::Emit<>;
};

template <class F>
struct FacetEmit<F, std::void_t<typename F::Emit>> {
    using type = typename F::Emit;
};

template <class T>
struct IsEmitList : std::false_type {};

template <class... E>
struct IsEmitList<loom::Emit<E...>> : std::true_type {};

template <class B>
struct BaseHandlers : B {
    using B::on;
};

template <class S, class St, class Ac, class Em, class Cl>
struct BaseHandlers<loom::WeaveBase<S, St, Ac, Em, Cl>> : loom::WeaveBase<S, St, Ac, Em, Cl> {
    void on() = delete;
};

template <template <class...> class Base, class Self, class Dispatch, class State, class AcceptList,
          class EmitList, class = void>
struct SelectBase {
    using type = Base<Dispatch, State, AcceptList, EmitList>;
};

template <template <class...> class Base, class Self, class Dispatch, class State, class AcceptList,
          class EmitList>
struct SelectBase<
    Base, Self, Dispatch, State, AcceptList, EmitList,
    std::void_t<typename Base<Self, State, AcceptList, EmitList, Dispatch>::Bindings>> {
    using type = Base<Self, State, AcceptList, EmitList, Dispatch>;
};

template <class Tuple, class... Args>
struct FirstFacetConstructible : std::false_type {};

template <class F0, class... FRest, class... Args>
struct FirstFacetConstructible<std::tuple<F0, FRest...>, Args...>
    : std::bool_constant<std::is_constructible_v<F0, Args...> &&
                         (std::is_default_constructible_v<FRest> && ...)> {};

template <class F0, class... FRest, class... Args>
std::tuple<F0, FRest...> make_facets_from_first(Args&&... args) {
    return std::tuple<F0, FRest...>(F0(std::forward<Args>(args)...), FRest{}...);
}

template <class F>
struct HasCanvas : std::bool_constant<requires(F& f) { f.canvas(); }> {};

template <std::size_t I = 0, class Tuple>
decltype(auto) find_canvas(Tuple& t) {
    if constexpr (requires { std::get<I>(t).canvas(); }) {
        return std::get<I>(t).canvas();
    } else {
        return find_canvas<I + 1>(t);
    }
}

/// A weave composed with `Facets...` over `Base` (`loom::WeaveBase` or `timer::TimedWeave`).
/// Unites the weave's and each facet's `Accept`/`Emit` sets at mount, routes each facet's
/// messages alongside the weave's own `on(...)`, and runs `self->after_delivery(mail)` before
/// each facet's `after_delivery` in facet declaration order.
template <class Self, class State, class AcceptList, class EmitList,
          template <class...> class Base, class... Facets>
class FacetWeave
    : public BaseHandlers<typename SelectBase<
          Base, Self, FacetWeave<Self, State, AcceptList, EmitList, Base, Facets...>, State,
          typename UnionLists<loom::Accept<>, typename FacetAccept<Facets>::type...,
                              AcceptList>::type,
          typename UnionLists<loom::Emit<>, typename FacetEmit<Facets>::type...,
                              EmitList>::type>::type> {
    using MergedAccept = typename UnionLists<loom::Accept<>, typename FacetAccept<Facets>::type...,
                                             AcceptList>::type;
    using MergedEmit =
        typename UnionLists<loom::Emit<>, typename FacetEmit<Facets>::type..., EmitList>::type;
    using SelectedBase =
        typename SelectBase<Base, Self, FacetWeave, State, MergedAccept, MergedEmit>::type;
    using Handlers = BaseHandlers<SelectedBase>;

public:
    using Weave = FacetWeave;
    using With = FacetWeave;
    using Handlers::on;

    FacetWeave()
        requires (std::is_default_constructible_v<Facets> && ...)
    = default;

    template <class... Args>
        requires (sizeof...(Args) == sizeof...(Facets) && sizeof...(Facets) >= 1 &&
                  !(sizeof...(Args) == 1 &&
                    (std::is_same_v<std::remove_cvref_t<Args>, FacetWeave> && ...)) &&
                  (std::is_constructible_v<Facets, Args> && ...))
    explicit FacetWeave(Args&&... args) : facets_(Facets(std::forward<Args>(args))...) {}

    template <class Arg, class... Rest>
        requires (!std::is_same_v<std::remove_cvref_t<Arg>, FacetWeave> &&
                  !(sizeof...(Rest) + 1 == sizeof...(Facets) &&
                    (std::is_constructible_v<Facets, Arg, Rest...> && ...)) &&
                  FirstFacetConstructible<std::tuple<Facets...>, Arg, Rest...>::value)
    explicit FacetWeave(Arg&& arg, Rest&&... rest)
        : facets_(make_facets_from_first<Facets...>(std::forward<Arg>(arg),
                                                    std::forward<Rest>(rest)...)) {}

    loom::LifecyclePolicy policy_config() const {
        static_assert(
            !(loom::has_whole_state_tag<Self>() && !loom::has_whole_state_tag<State>()),
            "loom: a bare ZEN_EXPOSE();/ZEN_HIDE(); belongs inside the State struct "
            "(the ZEN_SHAPE type), not the weave class — it is read from the state type");
        if constexpr (!std::is_same_v<decltype(&Self::policy_config),
                                      loom::LifecyclePolicy (FacetWeave::*)() const>) {
            return static_cast<const Self*>(this)->policy_config();
        } else {
            return {};
        }
    }

    loom::CapabilityAsk ask_config() const {
        if constexpr (!std::is_same_v<decltype(&Self::ask_config),
                                      loom::CapabilityAsk (FacetWeave::*)() const>) {
            return static_cast<const Self*>(this)->ask_config();
        } else {
            return {};
        }
    }

    template <class S>
    decltype(auto) on_claim_published(const S& claim)
        requires (requires(Self& s, const S& c) { s.on_claim_published(c); })
    {
        return static_cast<Self*>(this)->on_claim_published(claim);
    }

    template <class S>
        requires (!std::is_reference_v<S> &&
                  (!requires(Handlers& h, const S& m, loom::Mail& ml) { h.on(m, ml); } ||
                   (Contains<typename FacetAccept<Facets>::type, S>::value || ...)))
    void on(S&& msg, loom::Mail& mail) {
        constexpr bool base_handles =
            requires(Handlers& h, const S& m, loom::Mail& ml) { h.on(m, ml); };
        constexpr bool self_handles =
            !base_handles &&
            requires(Self& s, const S& m, loom::Mail& ml) { s.on(m, ml); };
        if constexpr (base_handles) {
            if constexpr (std::is_same_v<S, loom::Activated> &&
                          requires { this->activation(); }) {
                const loom::WeaveId prev_sender = this->activation().sender();
                const std::int64_t prev_seq = this->activation().sequence();
                Handlers::on(std::as_const(msg), mail);
                if (this->activation().sender() == prev_sender &&
                    this->activation().sequence() == prev_seq) {
                    return;
                }
            } else {
                Handlers::on(std::as_const(msg), mail);
            }
        } else if constexpr (self_handles) {
            static_cast<Self*>(this)->on(std::as_const(msg), mail);
        }
        if constexpr ((Contains<typename FacetAccept<Facets>::type, S>::value || ...)) {
            if constexpr (base_handles || !self_handles || Contains<AcceptList, S>::value) {
                (dispatch_facet<Facets>(*static_cast<Self*>(this), std::as_const(msg), mail), ...);
            }
        } else if constexpr (!base_handles && !self_handles) {
            static_cast<Self*>(this)->on(std::as_const(msg), mail);
        }
    }

    void after_delivery(loom::Mail& mail) {
        if constexpr (!std::is_same_v<decltype(&Self::after_delivery),
                                      void (FacetWeave::*)(loom::Mail&)>) {
            static_cast<Self*>(this)->after_delivery(mail);
        }
        (run_facet_after_delivery<Facets>(*static_cast<Self*>(this), mail), ...);
    }

protected:
    template <class F>
    F& facet() {
        return std::get<F>(facets_);
    }
    template <class F>
    const F& facet() const {
        return std::get<F>(facets_);
    }

    decltype(auto) canvas()
        requires ((HasCanvas<Facets>::value || ...))
    {
        return find_canvas(facets_);
    }
    decltype(auto) canvas() const
        requires ((HasCanvas<const Facets>::value || ...))
    {
        return find_canvas(facets_);
    }

private:
    template <class F, class S>
    void dispatch_facet(Self& self, const S& msg, loom::Mail& mail) {
        if constexpr (Contains<typename FacetAccept<F>::type, S>::value) {
            auto& f = std::get<F>(facets_);
            if constexpr (requires { f.on(self, msg, mail); }) {
                f.on(self, msg, mail);
            } else if constexpr (requires { f.on(msg, mail); }) {
                f.on(msg, mail);
            }
        }
    }

    template <class F>
    void run_facet_after_delivery(Self& self, loom::Mail& mail) {
        auto& f = std::get<F>(facets_);
        if constexpr (requires { f.after_delivery(self, mail); }) {
            f.after_delivery(self, mail);
        } else if constexpr (requires { f.after_delivery(mail); }) {
            f.after_delivery(mail);
        }
    }

    std::tuple<Facets...> facets_;
};

} // namespace detail

/// Composes `Facets...` onto a weave (`With<Facets...>::Weave<Self, State, Accept, Emit, Base>`
/// or directly `With<Self, State, Accept, Emit, Facets...>`).
template <class... Facets>
struct With {
    template <class Self, class State, class AcceptList, class EmitList = loom::Emit<>,
              template <class...> class Base = loom::WeaveBase>
    using Weave = detail::FacetWeave<Self, State, AcceptList, EmitList, Base, Facets...>;
};

template <class Self, class State, class... A, class... E, class... Facets>
struct With<Self, State, loom::Accept<A...>, loom::Emit<E...>, Facets...>
    : detail::FacetWeave<Self, State, loom::Accept<A...>, loom::Emit<E...>, loom::WeaveBase,
                         Facets...> {
    using Base = detail::FacetWeave<Self, State, loom::Accept<A...>, loom::Emit<E...>,
                                    loom::WeaveBase, Facets...>;
    using Base::Base;
    using Base::on;
};

template <class Self, class State, class... A, class FirstFacet, class... RestFacets>
    requires (!detail::IsEmitList<FirstFacet>::value)
struct With<Self, State, loom::Accept<A...>, FirstFacet, RestFacets...>
    : detail::FacetWeave<Self, State, loom::Accept<A...>, loom::Emit<>, loom::WeaveBase,
                         FirstFacet, RestFacets...> {
    using Base = detail::FacetWeave<Self, State, loom::Accept<A...>, loom::Emit<>,
                                    loom::WeaveBase, FirstFacet, RestFacets...>;
    using Base::Base;
    using Base::on;
};

} // namespace zengine

#endif // ZENGINE_ACTIVATION_ACTIVATION_HPP
