#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <optional>
#include <array>
#include <utility>
#include <ranges>
#include <format>
#include <tuple>
#include <variant>
#include <vector>
#include <memory>
#include <exception>
#include <print>


#define fwd(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

std::strong_ordering CmpViability(const auto&, const auto&) {
    return std::strong_ordering::equal;
}

template<typename T, typename U>
bool Is(const U&) {
    return std::is_same_v<T, U>;
}

template<typename T>
T As(const auto&) {
    std::terminate();
}

template<typename T>
T As(T obj) {
    return obj;
}

template<typename T, typename... Ts>
bool Is(const std::variant<Ts...>& var) {
    static_assert((std::is_same_v<T, Ts> || ...));
    return std::holds_alternative<T>(var);
}

template<typename T, typename... Ts>
T&& As(std::variant<Ts...>&& var) {
    static_assert((std::is_same_v<T, Ts> || ...));
    return std::get<T>(fwd(var));
}

template<typename T, typename... Ts>
T& As(std::variant<Ts...>& var) {
    static_assert((std::is_same_v<T, Ts> || ...));
    return std::get<T>(fwd(var));
}

template<typename T, typename... Ts>
const T& As(const std::variant<Ts...>& var) {
    static_assert((std::is_same_v<T, Ts> || ...));
    return std::get<T>(fwd(var));
}

template<typename T>
bool Is(const std::optional<T>& opt) {
    return opt.has_value();
}

template<typename T>
T As(std::optional<T>&& opt) {
    return std::move(opt.value());
}

template<std::same_as<std::nullopt_t>, typename T>
bool Is(const std::optional<T>& opt) {
    return !opt.has_value();
}

template<std::same_as<std::nullopt_t>, typename T>
std::nullopt_t As(const std::optional<T>& opt) {
    if (opt) {
        throw std::bad_optional_access{};
    }
    return std::nullopt;
}

template<typename T, typename U> requires(std::is_lvalue_reference_v<T> && std::is_same_v<std::remove_cvref_t<T>, U>)
bool Is(U* ptr) {
    return ptr;
}

struct BadAsCast : std::bad_cast {
    const char* what() const noexcept override {
        return "bad cast with As<T>(obj)";
    }
};

template<typename T, typename U> requires(std::is_lvalue_reference_v<T> && std::is_same_v<std::remove_cvref_t<T>, U>)
T As(U* ptr) {
    if (!ptr) {
        throw BadAsCast{};
    }
    return *ptr;
}

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
struct cw {
    static constexpr auto value = Val;
    using type = decltype(Val);


    bool Satisfy(const type& val) const {
        return val == value;
    }

    void Substitute(auto&& val) {}
};
static_assert(PatternC<cw<0>, int>);


constexpr struct PlaceHolder {
    bool Satisfy(const auto& val) const {
        return true;
    }

    void Substitute(auto&& val) {}
} _;
static_assert(PatternC<PlaceHolder, int>);
static_assert(PatternC<PlaceHolder, std::type_identity<double>>);



template<typename T>
struct Let {
    Let(T& r) : res(r) {}

    T& res;

    bool Satisfy(const auto& val) const {
        return Is<T>(val);
    }

    void Substitute(auto&& val) {
        res = fwd(val);
    }
};
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

template<typename T>
constexpr bool kIsTuple = requires(T obj) { InferTupleImpl(obj); };

template<typename T>
using InferTule = decltype(InferTupleImpl(std::declval<T>()))::type;
}

template<typename T, typename... Ts>
struct DecT;

template<typename T, typename PatT> requires (!util::kIsTuple<T>)
struct DecT<T, PatT> {
    DecT(PatT in_pat) : pat(in_pat) {}

    bool Satisfy(const auto& val) const {
        return Is<T>(val) && std::visit([this] (const auto& val) {
            return pat.Satisfy(val);
        }, As<T>(val));
    }

    void Substitute(auto&& val) {
        std::visit([this] (auto&& val) {
            pat.Substitute(fwd(val));
        }, std::forward_like<decltype(val)>(As<T>(val)));
    }


    PatT pat;
};

template<typename T, typename... Ts> requires util::kIsTuple<T>
struct DecT<T, Ts...> {
    DecT(Ts... in_pats) : pats(in_pats...) {}

    bool Satisfy(const auto& val) const {
        return Is<T>(InferAdt(val)) && std::apply([&val] (const auto&... deced_pats) {
            return std::apply([&deced_pats...] (const auto&... deced_vals) {
                return (deced_pats.Satisfy(deced_vals) && ...);
            }, InferAdt(As<T>(InferAdt(val))));
        }, pats);
    }

    void Substitute(util::InferTule<T>& val) {
        std::apply([this] (auto&&... deced_args) {
            SubstImpl(fwd(deced_args)...);
        }, fwd(val));
    }

    void Substitute(auto&& val) {
        std::apply([this] (auto&&... deced_args) {
            std::apply([&deced_args...] (auto&&... deced_pats) {
                (deced_pats.Substitute(fwd(deced_args)), ...);
            }, pats);
        }, InferAdt(As<T>(InferAdt(fwd(val)))));
    }

    void SubstImpl(auto&&... deced_args) {
        std::apply([&deced_args...] (auto&&... deced_pats) {
            (deced_pats.Substitute(fwd(deced_args)), ...);
        }, pats);
    }


    std::tuple<Ts...> pats;
};

template<typename T>
auto Dec(auto... pats) {
    // static_assert(!util::kIsTuple<T>);
    return DecT<T, decltype(pats)...>(pats...);
}

template<typename Pat>
struct Derefed {
    Derefed(Pat p) : pat(p) {}


    bool Satisfy(const auto& val) const {
        return val && pat.Satisfy(*val);
    }

    void Substitute(auto&& val) {
        pat.Substitute(*fwd(val));
    }


    Pat pat;
};


template<typename PatternT>
struct Case {
    Case(PatternT pat) : pattern(pat) {}

    PatternT pattern;

    template<typename CallbackT>
    struct CompleteCase {
        Case case_;
        CallbackT callback;

        bool Satisfy(const auto& val) const {
            return case_.pattern.Satisfy(std::as_const(val));
        }

        auto Substitute(auto&& val) {
            case_.pattern.Substitute(fwd(val));
            return callback();
        }
    };

    template<std::invocable<> T>
    CompleteCase<T> operator<=>(T cb) {
        return {
            .case_ = *this,
            .callback = std::move(cb)
        };
    }
};

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
                    std::strong_ordering cmp_res = CmpViability(complete_cases...[Idx], complete_cases...[Idx1]);
                    if (cmp_res == std::strong_ordering::less) {
                            self(cw<Idx1>{});
                            final_chosen = false;
                            return true;
                    } else if (cmp_res == std::strong_ordering::greater) {
                    } else if (cmp_res == std::strong_ordering::equal) {
                        [] noexcept {
                            throw std::invalid_argument("ambiguous");
                        } ();
                    } else {
                        [] noexcept {
                            throw std::invalid_argument("invalid std::strong_ordering value");
                        } ();
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

struct Expr;

struct Literal : std::tuple<int> {
    int value();
};

struct FunctionCall : std::tuple<std::string, std::vector<Expr>> {
    std::string_view func_name() const;
    std::span<const Expr> args() const;
};

struct Add : std::tuple<std::unique_ptr<Expr>, std::unique_ptr<Expr>> {
    Expr* lhs() const;
    Expr* rhs() const;
};

struct Expr : std::variant<Literal, FunctionCall, Add> {};

std::unique_ptr<Expr> _new(auto&&... args) {
    return std::make_unique<Expr>(fwd(args)...);
}

int main() {
    auto vec = std::vector(std::from_range, std::array{Expr(Literal(1))} | std::views::as_rvalue);
    auto expr = Expr(Add({ _new(Literal(2)), _new(FunctionCall({ "f", std::move(vec) }))}));
    int val;
    Match(std::move(expr)) (
        Case(Dec<Literal>(_)) <=> [&] {  },
        Case(Dec<Add>(Derefed(Dec<Literal>(_)), _)) <=> [&] { std::println("succ"); }
    );
    return 0;
}
