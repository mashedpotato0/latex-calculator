#include <iostream>
#include <string>
#include <vector>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "integrator.hpp"

static void load_builtins(context& ctx) {
    ctx.builtins["cos"]   = [](double x){ return std::cos(x); };
    ctx.builtins["sin"]   = [](double x){ return std::sin(x); };
    ctx.builtins["tan"]   = [](double x){ return std::tan(x); };
    ctx.builtins["exp"]   = [](double x){ return std::exp(x); };
    ctx.builtins["log"]   = [](double x){ return std::log(x); };
    ctx.builtins["ln"]    = [](double x){ return std::log(x); };
    ctx.builtins["sqrt"]  = [](double x){ return std::sqrt(x); };
    ctx.builtins["arctan"]= [](double x){ return std::atan(x); };
    ctx.builtins["arcsin"]= [](double x){ return std::asin(x); };
    ctx.builtins["arccos"]= [](double x){ return std::acos(x); };
    ctx.builtins["sinh"]  = [](double x){ return std::sinh(x); };
    ctx.builtins["cosh"]  = [](double x){ return std::cosh(x); };
    ctx.builtins["tanh"]  = [](double x){ return std::tanh(x); };
    ctx.builtins["cot"]   = [](double x){ return 1.0/std::tan(x); };
    ctx.builtins["sec"]   = [](double x){ return 1.0/std::cos(x); };
    ctx.builtins["csc"]   = [](double x){ return 1.0/std::sin(x); };
    ctx.builtins["sech"]  = [](double x){ return 1.0/std::cosh(x); };
    ctx.vars["pi"]  = M_PI;
    ctx.vars["e"]   = M_E;
    ctx.vars["phi"] = (1+std::sqrt(5.0))/2;
    ctx.vars["tau"] = 2*M_PI;
}

static void test_eval(const std::string& label, const std::string& expr_str, context& ctx) {
    auto tokens = tokenize(expr_str);
    parser p(tokens);
    auto tree = p.parse_expr();
    if (!tree) { std::cout << "  [" << label << "] parse error\n"; return; }
    auto simp = tree->simplify();
    try {
        double val = simp->eval(ctx);
        std::cout << "  [" << label << "] = " << val << "\n";
    } catch (std::exception& e) {
        std::cout << "  [" << label << "] error: " << e.what() << "\n";
    }
}

static void test_integrate(const std::string& label, const std::string& expr_str, const std::string& var) {
    auto tokens = tokenize(expr_str);
    parser p(tokens);
    auto tree = p.parse_expr();
    if (!tree) { std::cout << "  [" << label << "] parse error\n"; return; }
    auto result = symbolic::integrate(*tree, var);
    if (result) std::cout << "  [" << label << "] " << result->to_string() << " + C\n";
    else        std::cout << "  [" << label << "] no closed form\n";
}

int main() {
    context ctx;
    load_builtins(ctx);

    std::cout << "=== evaluation tests ===\n";
    test_eval("7",               "7",                    ctx);
    test_eval("3+4",             "3+4",                  ctx);
    test_eval("fact(5)",         "\\fact{5}",             ctx);
    test_eval("fact(10)",        "\\fact{10}",            ctx);
    test_eval("C(8,3)",          "\\C{8}{3}",             ctx);
    test_eval("P(5,2)",          "\\P{5}{2}",             ctx);
    test_eval("C(10,4)",         "\\C{10}{4}",            ctx);
    test_eval("pi",              "pi",                   ctx);
    test_eval("|−5|",            "\\abs{-5}",             ctx);
    // sum i=1..10: i^2 = 385
    {
        auto sn = std::make_unique<sum_node>("i",
            std::make_unique<number>(1), std::make_unique<number>(10),
            std::make_unique<pow_node>(std::make_unique<variable>("i"), std::make_unique<number>(2)));
        std::cout << "  [sum i=1..10 i^2] = " << sn->eval(ctx) << "\n";
    }
    // product i=1..5: i = 120
    {
        auto pn = std::make_unique<product_node>("i",
            std::make_unique<number>(1), std::make_unique<number>(5),
            std::make_unique<variable>("i"));
        std::cout << "  [prod i=1..5 i] = " << pn->eval(ctx) << "\n";
    }

    std::cout << "\n=== integration tests ===\n";
    test_integrate("x^4",           "x^{4}",             "x");
    test_integrate("sin(x)",        "\\sin{x}",          "x");
    test_integrate("cos(x)",        "\\cos{x}",          "x");
    test_integrate("exp(x)",        "\\exp{x}",          "x");
    test_integrate("ln(x)",         "\\ln{x}",           "x");
    test_integrate("1/x",           "\\frac{1}{x}",      "x");
    test_integrate("x*cos(x)",      "x*\\cos{x}",        "x");
    test_integrate("x*exp(x)",      "x*\\exp{x}",        "x");
    test_integrate("2x*sin(x^2)",   "2*x*\\sin{{x}^{2}}","x");

    std::cout << "\n=== derivative tests ===\n";
    {
        auto tokens=tokenize("\\sin{x}");
        parser p(tokens);
        auto tree=p.parse_expr();
        auto d=tree->derivative("x")->simplify();
        std::cout << "  d/dx sin(x) = " << d->to_string() << "\n";
    }
    {
        auto tokens=tokenize("x^{3}+2*x");
        parser p(tokens);
        auto tree=p.parse_expr();
        auto d=tree->derivative("x")->simplify();
        std::cout << "  d/dx (x^3+2x+cos(x*sin(x))) = " << d->to_string() << "\n";
    }

    return 0;
}
