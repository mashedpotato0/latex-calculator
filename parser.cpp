#include "parser.hpp"
#include <iostream>

token parser::current() {
    if (pos < tokens.size()) return tokens[pos];
    return {tokentype::eof, ""};
}

void parser::advance() {
    if (pos < tokens.size()) pos++;
}

void parser::expect(tokentype type) {
    if (current().type == type) {
        advance();
    } else {
        std::cerr << "unexpected token: " << current().value << "\n";
    }
}

std::unique_ptr<expr> parser::parse_primary() {
    token t = current();
    
    if (t.type == tokentype::number) {
        advance();
        return std::make_unique<number>(std::stod(t.value));
    }
    
    if (t.type == tokentype::variable) {
        advance();
        return std::make_unique<variable>(t.value);
    }
    
    if (t.type == tokentype::command) {
        std::string cmd = t.value;
        advance();
        
        if (cmd == "\\int") {
            std::unique_ptr<expr> lower = nullptr, upper = nullptr;
            if (current().value == "_") {
                advance(); expect(tokentype::lbrace);
                lower = parse_expr(); expect(tokentype::rbrace);
                expect(tokentype::op); // expects ^
                expect(tokentype::lbrace);
                upper = parse_expr(); expect(tokentype::rbrace);
            }
            auto integrand = parse_expr();
            if (current().value == "d") advance();
            token var_tok = current(); advance();
            return std::make_unique<integral>(std::move(lower), std::move(upper), std::move(integrand), var_tok.value);
        }
        
        if (cmd == "\\frac") {
            size_t start = pos;
            expect(tokentype::lbrace);
            if (current().value == "d") {
                advance();
                if (current().type == tokentype::rbrace) {
                    advance(); expect(tokentype::lbrace);
                    if (current().value == "d") {
                        advance(); std::string v = current().value; advance();
                        expect(tokentype::rbrace);
                        return std::make_unique<deriv_node>(v, parse_primary());
                    }
                } else {
                    auto arg = parse_expr();
                    expect(tokentype::rbrace); expect(tokentype::lbrace);
                    if (current().value == "d") {
                        advance(); std::string v = current().value; advance();
                        expect(tokentype::rbrace);
                        return std::make_unique<deriv_node>(v, std::move(arg));
                    }
                }
            }
            pos = start; // fallback to standard division
            expect(tokentype::lbrace); auto n = parse_expr(); expect(tokentype::rbrace);
            expect(tokentype::lbrace); auto d = parse_expr(); expect(tokentype::rbrace);
            return std::make_unique<divide>(std::move(n), std::move(d));
        }
        
        expect(tokentype::lbrace);
        auto arg = parse_expr();
        expect(tokentype::rbrace);
        return std::make_unique<func_call>(cmd.substr(1), std::move(arg));
    }
    
    if (t.type == tokentype::lbrace) {
        advance();
        auto e = parse_expr();
        expect(tokentype::rbrace);
        return e;
    }
    return nullptr;
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
    while (current().value == "*") {
        advance();
        left = std::make_unique<multiply>(std::move(left), parse_factor());
    }
    return left;
}

std::unique_ptr<expr> parser::parse_expr() {
    auto left = parse_term();
    while (current().value == "+") {
        advance();
        left = std::make_unique<add>(std::move(left), parse_term());
    }
    return left;
}