#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <memory>
#include <vector>

class parser {
  std::vector<token> tokens;
  size_t pos = 0;

  token current();
  void advance();
  void expect(tokentype type);
  bool is_implicit_mul_start();

  std::unique_ptr<expr> parse_primary();
  std::unique_ptr<expr> parse_factor();
  std::unique_ptr<expr> parse_term();

public:
  parser(const std::vector<token> &t) : tokens(t) {}
  std::unique_ptr<expr> parse_expr();
};