#include "parser.hpp"
#include <iostream>

token parser::current() {
    if (pos < tokens.size()) return tokens[pos];
    return {tokentype::eof, ""};
}

void parser::advance() { if (pos < tokens.size()) pos++; }

void parser::expect(tokentype type) {
    if (current().type == type) advance();
    else std::cerr << "unexpected token: '" << current().value << "'\n";
}

std::unique_ptr<expr> parser::parse_primary() {
    token t = current();

    // unary minus
    if (t.type == tokentype::op && t.value == "-") {
        advance();
        auto operand = parse_primary();
        return std::make_unique<multiply>(std::make_unique<number>(-1.0), std::move(operand));
    }

    // standard parenthesised group
    if (t.type == tokentype::op && t.value == "(") {
        advance();
        auto e = parse_expr();
        if (current().value == ")") advance();
        return e;
    }

    // latex parenthesised group
    if (t.type == tokentype::lbrace) {
        advance();
        auto e = parse_expr();
        if (current().type == tokentype::rbrace) advance();
        return e;
    }

    if (t.type == tokentype::number) {
        advance();
        return std::make_unique<number>(std::stod(t.value));
    }

    if (t.type == tokentype::variable) {
        std::string name = t.value;
        advance();
        
        // check for math standard functions like "sin(x)"
        if (current().type == tokentype::op && current().value == "(") {
            advance();
            auto arg = parse_expr();
            if (current().value == ")") advance();
            
            if (name == "abs") return std::make_unique<abs_node>(std::move(arg));
            return std::make_unique<func_call>(name, std::move(arg));
        }

        static const std::map<std::string,double> CONSTS = {
            {"pi", M_PI}, {"e", M_E},
            {"phi", (1+std::sqrt(5.0))/2}, {"tau", 2*M_PI}
        };
        if (CONSTS.count(name))
            return std::make_unique<named_constant>(name, CONSTS.at(name));
            
        return std::make_unique<variable>(name);
    }

    if (t.type == tokentype::command) {
        std::string cmd = t.value;
        advance();
        std::string fname = cmd.substr(1);

        // handle integral blocks
        if (fname == "int") {
            std::unique_ptr<expr> lower, upper;
            if (current().value == "_") {
                advance(); expect(tokentype::lbrace);
                lower = parse_expr(); expect(tokentype::rbrace);
                expect(tokentype::op);
                expect(tokentype::lbrace);
                upper = parse_expr(); expect(tokentype::rbrace);
            }
            auto integrand = parse_expr();
            if (current().value == "d") advance();
            std::string v = current().value; advance();
            return std::make_unique<integral>(std::move(lower), std::move(upper), std::move(integrand), v);
        }

        // fractions
        if (fname == "frac") {
            std::unique_ptr<expr> num, den;
            if (current().type == tokentype::lbrace) { advance(); num = parse_expr(); if (current().type==tokentype::rbrace) advance(); }
            else num = parse_factor();
            if (current().type == tokentype::lbrace) { advance(); den = parse_expr(); if (current().type==tokentype::rbrace) advance(); }
            else den = parse_factor();
            return std::make_unique<divide>(std::move(num), std::move(den));
        }

        if (fname == "fact") {
            expect(tokentype::lbrace); auto a = parse_expr(); expect(tokentype::rbrace);
            return std::make_unique<factorial_node>(std::move(a));
        }

        if (fname == "C" || fname == "binom") {
            expect(tokentype::lbrace); auto n = parse_expr(); expect(tokentype::rbrace);
            expect(tokentype::lbrace); auto r = parse_expr(); expect(tokentype::rbrace);
            return std::make_unique<combination_node>(std::move(n), std::move(r));
        }

        if (fname == "P") {
            expect(tokentype::lbrace); auto n = parse_expr(); expect(tokentype::rbrace);
            expect(tokentype::lbrace); auto r = parse_expr(); expect(tokentype::rbrace);
            return std::make_unique<permutation_node>(std::move(n), std::move(r));
        }

        if (fname == "sum") {
            std::string idx = "i";
            std::unique_ptr<expr> from, to;
            if (current().value == "_") {
                advance(); expect(tokentype::lbrace);
                idx = current().value; advance();
                expect(tokentype::op);
                from = parse_expr(); expect(tokentype::rbrace);
                expect(tokentype::op);
                expect(tokentype::lbrace); to = parse_expr(); expect(tokentype::rbrace);
            }
            return std::make_unique<sum_node>(idx, std::move(from), std::move(to), parse_primary());
        }

        if (fname == "prod") {
            std::string idx = "i";
            std::unique_ptr<expr> from, to;
            if (current().value == "_") {
                advance(); expect(tokentype::lbrace);
                idx = current().value; advance();
                expect(tokentype::op);
                from = parse_expr(); expect(tokentype::rbrace);
                expect(tokentype::op);
                expect(tokentype::lbrace); to = parse_expr(); expect(tokentype::rbrace);
            }
            return std::make_unique<product_node>(idx, std::move(from), std::move(to), parse_primary());
        }
        
        // generic function with brackets or latex
        std::unique_ptr<expr> arg;
        if (current().type == tokentype::lbrace) {
            advance();
            arg = parse_expr();
            if (current().type == tokentype::rbrace) advance();
        } else if (current().value == "(") {
            advance();
            arg = parse_expr();
            if (current().value == ")") advance();
        } else {
            arg = parse_factor();
        }
        
        if (fname == "abs") return std::make_unique<abs_node>(std::move(arg));
        return std::make_unique<func_call>(fname, std::move(arg));
    }

    return std::make_unique<number>(0.0);
}

std::unique_ptr<expr> parser::parse_factor() {
    auto base = parse_primary();
    while (current().value == "^") {
        advance();
        base = std::make_unique<pow_node>(std::move(base), parse_primary());
    }
    return base;
}

std::unique_ptr<expr> parser::parse_term() {
    auto left = parse_factor();
    while (current().value == "*" || current().value == "/") {
        bool div = current().value == "/";
        advance();
        if (div) left = std::make_unique<divide>(std::move(left), parse_factor());
        else left = std::make_unique<multiply>(std::move(left), parse_factor());
    }
    return left;
}

std::unique_ptr<expr> parser::parse_expr() {
    auto left = parse_term();
    while (current().value == "+" || current().value == "-") {
        bool sub = current().value == "-";
        advance();
        auto right = parse_term();
        if (sub) right = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::move(right));
        left = std::make_unique<add>(std::move(left), std::move(right));
    }
    return left;
}