#pragma once
#include <vector>
#include <memory>
#include "lexer.hpp"
#include "ast.hpp"

class parser {
    std::vector<token> tokens;
    size_t pos = 0;
    
    token current();
    void advance();
    void expect(tokentype type);
    
    std::unique_ptr<expr> parse_primary();
    std::unique_ptr<expr> parse_factor();
    std::unique_ptr<expr> parse_term();
    
public:
    parser(const std::vector<token>& t) : tokens(t) {}
    std::unique_ptr<expr> parse_expr();
};