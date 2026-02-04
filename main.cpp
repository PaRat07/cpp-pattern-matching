#include "pm.hpp"

#include <print>
#include <vector>

struct Expr;

struct Literal {
    int value;
};

struct FunctionCall {
    std::string func_name;
    std::vector<Expr> args;
};

struct Add {
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
};

struct Expr : std::variant<Literal, FunctionCall, Add> {
    static constexpr auto Literal = DecFunctor<::Literal>;
    static constexpr auto FunctionCall = DecFunctor<::FunctionCall>;
    static constexpr auto Add = DecFunctor<::Add>;
};

std::unique_ptr<Expr> _new(auto&&... args) {
    return std::make_unique<Expr>(std::forward<decltype(args)>(args)...);
}

int main() {
    auto vec = std::vector(std::from_range, std::array{Expr(Literal(1))} | std::views::as_rvalue);
    auto expr = Expr(Add({ _new(Literal(2)), _new(FunctionCall({ "f", std::move(vec) }))}));
    FunctionCall fcall;
    Match(std::move(expr)) (
        _ <=> [&] { std::terminate();  },
        Expr::Add(_, _) <=> [&] { std::terminate();  },
        Expr::Add(unbox(Expr::Literal(_)), unbox(Let(fcall))) <=> [&] { std::println("succ: {}", fcall.func_name); }
    );
    return 0;
}
