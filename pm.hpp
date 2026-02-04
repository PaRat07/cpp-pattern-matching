#pragma once

#include <exception>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>

#include <boost/pfr.hpp>


#define fwd(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)


template<typename T, template<typename> typename TagT>
concept TaggedWithC = std::is_base_of_v<TagT<T>, T>;

template<typename T>
struct PatternTag {};

template<typename T, typename U>
constexpr std::strong_ordering kCmpViability = std::strong_ordering::equal;

template<typename... Ts>
std::variant<Ts...>& InferAdtImpl(std::variant<Ts...>& obj) {
    return obj;
}

template<typename... Ts>
const std::variant<Ts...>& InferAdtImpl(const std::variant<Ts...>& obj) {
    return obj;
}

template<typename... Ts>
std::variant<Ts...>&& InferAdtImpl(std::variant<Ts...>&& obj) {
    return obj;
}

template<typename... Ts>
std::tuple<Ts...>& InferAdtImpl(std::tuple<Ts...>& obj) {
    return obj;
}

template<typename... Ts>
const std::tuple<Ts...>& InferAdtImpl(const std::tuple<Ts...>& obj) {
    return obj;
}

template<typename... Ts>
std::tuple<Ts...>&& InferAdtImpl(std::tuple<Ts...>&& obj) {
    return obj;
}

template<typename T>
auto&& InferAdt(T&& obj) {
    if constexpr (requires { InferAdtImpl(fwd(obj)); }) {
        return InferAdtImpl(fwd(obj));
    } else {
        return obj;
    }
}

template<typename T, typename MatchedT>
concept PatternC = requires (T obj, MatchedT matched) {
    { std::as_const(obj).Satisfy(std::as_const(matched)) } -> std::convertible_to<bool>;
    obj.Substitute(matched);
};



template<auto Val>
struct cw : PatternTag<cw<Val>> {
    static constexpr auto value = Val;
    using type = decltype(Val);


    bool Satisfy(const type& val) const {
        return val == value;
    }

    void Substitute(auto&& val) {}
};
static_assert(PatternC<cw<0>, int>);


constexpr struct PlaceHolder : PatternTag<PlaceHolder> {
    bool Satisfy(const auto& val) const {
        return true;
    }

    void Substitute(auto&& val) {}
} _;

template<typename T>
constexpr std::strong_ordering kCmpViability<PlaceHolder, T> = std::strong_ordering::less;

template<typename T>
constexpr std::strong_ordering kCmpViability<T, PlaceHolder> = std::strong_ordering::greater;

template<>
constexpr std::strong_ordering kCmpViability<PlaceHolder, PlaceHolder> = std::strong_ordering::equal;

static_assert(PatternC<PlaceHolder, int>);
static_assert(PatternC<PlaceHolder, std::type_identity<double>>);



template<typename T>
struct Let : PatternTag<Let<T>> {
    Let(T& r) : res(r) {}

    T& res;

    template<std::convertible_to<T> ValT>
    bool Satisfy(const ValT& val) const {
        return true;
    }

    template<typename ValT> requires ([] <typename... Ts> (std::type_identity<std::variant<Ts...>>) {
            return (std::convertible_to<Ts, T> || ...);
        } (std::type_identity<std::remove_cvref_t<ValT>>{}))
    bool Satisfy(const ValT& val) const {
        return std::visit([] (const auto& val) {
            return std::convertible_to<std::remove_cvref_t<decltype(val)>, T>;
        }, val);
    }

    template<std::convertible_to<T> ValT>
    void Substitute(ValT&& val) {
        res = fwd(val);
    }

    template<typename ValT> requires ([] <typename... Ts> (std::type_identity<std::variant<Ts...>>) {
            return (std::convertible_to<Ts, T> || ...);
        } (std::type_identity<std::remove_cvref_t<ValT>>{}))
    void Substitute(ValT&& val) {
        std::visit([this] (auto&& val) {
            res = fwd(val);
        }, val);
    }
};

template<typename T>
constexpr std::strong_ordering kCmpViability<PlaceHolder, Let<T>> = std::strong_ordering::equal;

template<typename T>
constexpr std::strong_ordering kCmpViability<Let<T>, PlaceHolder> = std::strong_ordering::equal;

template<typename T, typename U>
constexpr std::strong_ordering kCmpViability<Let<T>, Let<U>> = std::strong_ordering::equal;

template<typename T, typename U>
constexpr std::strong_ordering kCmpViability<Let<T>, U> = std::strong_ordering::less;

template<typename T, typename U>
constexpr std::strong_ordering kCmpViability<T, Let<U>> = std::strong_ordering::greater;

static_assert(PatternC<Let<int>, int>);
static_assert(PatternC<Let<double>, std::variant<int, double>>);

namespace util {
template<typename... Ts>
std::type_identity<std::variant<Ts...>> InferAdtImpl(const std::variant<Ts...>&);

template<typename... Ts>
std::type_identity<std::tuple<Ts...>> InferAdtImpl(const std::tuple<Ts...>&);

template<typename T>
using InferAdt = decltype(InferAdtImpl(std::declval<T>()))::type;

template<typename... Ts>
std::type_identity<std::tuple<Ts...>> InferTupleImpl(const std::tuple<Ts...>&);

template<typename... Ts>
std::type_identity<std::variant<Ts...>> InferVariantImpl(const std::variant<Ts...>&);

template<typename T>
constexpr bool kIsTuple = requires(T obj) { InferTupleImpl(obj); };

template<typename T>
constexpr bool kIsVariant = requires(T obj) { InferVariantImpl(obj); };

template<typename T>
using InferTule = decltype(InferTupleImpl(std::declval<T>()))::type;
}

template<typename PatT, typename ValT>
concept VisitSatisfyable = requires (ValT val) { util::InferVariantImpl(val); } &&
            [] <typename... Ts> (std::type_identity<std::variant<Ts...>>) {
                return (requires (PatT pat) { pat.Satisfy(std::declval<Ts>()); } || ...);
            } (std::type_identity<std::remove_cvref_t<decltype(InferAdt(std::declval<ValT>()))>>{});


template<typename PatT, typename ValT>
concept VisitSubstitable = requires (ValT val) { util::InferVariantImpl(val); } &&
            [] <typename... Ts> (std::type_identity<std::variant<Ts...>>) {
                return (requires (PatT pat) { pat.Substitute(std::declval<Ts>()); } || ...);
            } (std::type_identity<std::remove_cvref_t<decltype(InferAdt(std::declval<ValT>()))>>{});

bool Satisfy(const auto& pat, const auto& val) requires (requires { pat.Satisfy(fwd(val)); } || VisitSatisfyable<decltype(pat), decltype(val)>) {
    if constexpr (requires { pat.Satisfy(val); }) {
        return pat.Satisfy(val);
    } else {
        return std::visit([&pat] (const auto& val) {
            if constexpr (requires { pat.Satisfy(val); }) {
                return pat.Satisfy(val);
            } else {
                return false;
            }
        }, val);
    }
}

void Substitute(auto&& pat, auto&& val) requires (requires { pat.Substitute(fwd(val)); } || VisitSubstitable<decltype(pat), decltype(val)>) {
    if constexpr (requires { pat.Substitute(fwd(val)); }) {
        pat.Substitute(fwd(val));
    } else {
        std::visit([&pat] (auto&& val) {
            if constexpr (requires { pat.Substitute(fwd(val)); }) {
                return pat.Substitute(fwd(val));
            } else {
                std::terminate();
            }
        }, fwd(val));
    }
}

template<typename T, typename... Ts>
struct DecT;

template<typename T, typename... SubPats1, typename... SubPats2>
constexpr std::strong_ordering kCmpViability<DecT<T, SubPats1...>, DecT<T, SubPats2...>> = [] {
    if (((kCmpViability<SubPats1, SubPats2> == std::strong_ordering::less) && ...)) {
        return std::strong_ordering::less;
    } else if (((kCmpViability<SubPats1, SubPats2> == std::strong_ordering::greater) && ...)) {
        return std::strong_ordering::greater;
    } else {
        return std::strong_ordering::equal;
    }
} ();

template<typename T, typename PatT> requires util::kIsVariant<T>
struct DecT<T, PatT> : PatternTag<DecT<T, PatT>> {
    DecT(PatT in_pat) : pat(in_pat) {}

    bool Satisfy(const T& val) const {
        return Satisfy(pat, InferAdt(val));
    }

    void Substitute(auto&& val) {
        return Substitute(pat, fwd(val));
    }


    PatT pat;
};

decltype(auto) pfr_apply(auto&& fn, auto&& aggr) {
    return [&fn, &aggr] <size_t... Idxs> (std::index_sequence<Idxs...>) -> decltype(auto) {
        return fwd(fn)(boost::pfr::get<Idxs>(fwd(aggr))...);
    } (std::make_index_sequence<boost::pfr::tuple_size_v<std::remove_cvref_t<decltype(aggr)>>>{});
}

template<typename T, typename... Ts> requires (!util::kIsVariant<T>)
struct DecT<T, Ts...> : PatternTag<DecT<T, Ts...>> {
    DecT(Ts... in_pats) : pats(in_pats...) {}

    bool Satisfy(const T& val) const {
        return std::apply([&val] (const auto&... deced_pats) {
            return pfr_apply([&deced_pats...] (const auto&... deced_vals) {
                return (::Satisfy(deced_pats, deced_vals) && ...);
            }, val);
        }, pats);
    }

    void Substitute(auto&& val) requires (std::is_same_v<T, std::remove_cvref_t<decltype(val)>>) {
        std::apply([&val] (auto&&... deced_pats) {
            return pfr_apply([&deced_pats...] (auto&&... deced_vals) {
                (::Substitute(deced_pats, fwd(deced_vals)), ...);
            }, fwd(val));
        }, pats);
    }

    std::tuple<Ts...> pats;
};

template<typename T>
constexpr auto DecFunctor = [] (auto... pats) { return DecT<T, decltype(pats)...>(pats...); };

template<typename T>
auto Dec(auto... pats) {
    return DecT<T, decltype(pats)...>(pats...);
}

template<typename Pat>
struct unbox : PatternTag<unbox<Pat>> {
    unbox(Pat p) : pat(p) {}


    bool Satisfy(const auto& val) const requires(requires(Pat pat) { Satisfy(pat, *val); }) {
        if constexpr (requires{ static_cast<bool>(val); }) {
            return static_cast<bool>(val) && ::Satisfy(pat, *val);
        } else if constexpr (requires{ val.valueless_after_move(); }) {
            return !val.valueless_after_move() && ::Satisfy(pat, *val);
        } else {
            static_assert(false);
        }
    }

    void Substitute(auto&& val) {
        ::Substitute(pat, std::forward_like<decltype(val)>(*val));
    }


    Pat pat;
};

template<typename T, typename U>
constexpr std::strong_ordering kCmpViability<unbox<T>, unbox<U>> = kCmpViability<T, U>;

template<typename PatT, typename CallbackT>
struct CompleteCase {
    PatT pat;
    CallbackT callback;

    bool Satisfy(const auto& val) const {
        return ::Satisfy(pat, InferAdt(val));
    }

    auto Substitute(auto&& val) {
        ::Substitute(pat, InferAdt(fwd(val)));
        return callback();
    }
};

template<TaggedWithC<PatternTag> PatT, typename CallbackT>
CompleteCase<PatT, CallbackT> operator<=>(PatT pat, CallbackT cb) {
    return {
        .pat = std::move(pat),
        .callback = std::move(cb)
    };
}

template<size_t From, size_t To>
void ConstexprLoop(auto&& func) {
    [&func] <size_t... Idxs> (std::index_sequence<Idxs...>) {
        if constexpr (std::is_same_v<void, decltype(func(cw<0uz>{}))>) {
            (func(cw<Idxs + From>{}), ...);
        } else {
            (func(cw<Idxs + From>{}) || ...);
        }
    } (std::make_index_sequence<To - From>{});
}

template<typename T>
using OptionalWrapped = std::conditional_t<std::is_same_v<void, T>, std::type_identity<void>, std::optional<T>>;

std::strong_ordering CmpViability(auto&& lhs, auto&& rhs) {
    return kCmpViability<std::remove_cvref_t<decltype(lhs)>, std::remove_cvref_t<decltype(rhs)>>;
}




auto Match(auto&& val) {
    return [&val] (auto&&... complete_cases) mutable {
        const auto sat = std::array{ complete_cases.Satisfy(std::as_const(val))... };
        OptionalWrapped<std::common_type_t<decltype(complete_cases.callback())...>> res;
        ConstexprLoop<0u, sizeof...(complete_cases)>([&] <size_t Idx> (this auto self, cw<Idx>) {
            if (!sat[Idx]) {
                return false;
            }
            bool final_chosen = true;
            ConstexprLoop<Idx + 1, sizeof...(complete_cases)>([&] <size_t Idx1> (cw<Idx1>) mutable {
                if (sat[Idx]) {
                    std::strong_ordering cmp_res = CmpViability(complete_cases...[Idx].pat, complete_cases...[Idx1].pat);
                    if (cmp_res == std::strong_ordering::less) {
                            self(cw<Idx1>{});
                            final_chosen = false;
                            return true;
                    } else if (cmp_res == std::strong_ordering::greater) {
                    } else if (cmp_res == std::strong_ordering::equal) {
                        try {
                            throw std::invalid_argument("ambiguous");
                        } catch (...) {
                            std::terminate();
                        }
                    } else {
                        try {
                            throw std::invalid_argument("invalid std::strong_ordering value");
                        } catch (...) {
                            std::terminate();
                        }
                    }
                }
                return false;
            });
            if (final_chosen) {
                if constexpr (std::is_same_v<decltype(res), std::type_identity<void>>) {
                    complete_cases...[Idx].Substitute(fwd(val));
                } else {
                    res.emplace(complete_cases...[Idx].Substitue(fwd(val)));
                }
            }
            return true;
        });
        if constexpr (!std::is_same_v<decltype(res), std::type_identity<void>>) {
            return *res;
        }
    };
}

#undef fwd
