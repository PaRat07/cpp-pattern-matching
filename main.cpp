#include "pm.hpp"
#include <indirect.h>

#include <print>
#include <vector>



struct Expr;

struct Literal {
    int value;
};

struct FunctionCall {
    std::string func_name;
    std::vector<xyz::indirect<Expr>> args;
};

struct Add {
    xyz::indirect<Expr> lhs;
    xyz::indirect<Expr> rhs;
};

struct Expr : std::variant<Literal, FunctionCall, Add> {
    static constexpr auto Literal = DecFunctor<::Literal>;
    static constexpr auto FunctionCall = DecFunctor<::FunctionCall>;
    static constexpr auto Add = DecFunctor<::Add>;
};

xyz::indirect<Expr> _new(auto&&... args) {
    return xyz::indirect<Expr>(std::forward<decltype(args)>(args)...);
}

template<typename... Ts>
auto Vec(Ts&&... elems) -> std::vector<std::decay_t<std::common_type_t<Ts...>>> {
    std::vector<std::decay_t<std::common_type_t<Ts...>>> res;
    res.reserve(sizeof...(elems));
    (res.push_back(std::forward<decltype(elems)>(elems)), ...);
    return res;
}

int main() {
    auto expr = Expr(Add{ _new(Literal(2)), _new(FunctionCall{ "f", Vec(_new(Literal(1)), _new(Literal(2))) })});
    FunctionCall fcall;
    Match(std::move(expr)) (
        _ <=> [&] { std::terminate();  },
        Expr::Add(_, _) <=> [&] { std::terminate();  },
        Expr::Add(unbox(Expr::Literal(_)), unbox(Let(fcall))) <=> [&] { std::println("succ: {}", fcall.func_name); }
    );
    return 0;
}
