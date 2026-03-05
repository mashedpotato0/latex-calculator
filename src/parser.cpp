#include "parser.hpp"
#include "ast_ext.hpp"
#include <iostream>

token parser::current() {
  if (pos < tokens.size())
    return tokens[pos];
  return {tokentype::eof, ""};
}

void parser::advance() {
  if (pos < tokens.size())
    pos++;
}

void parser::expect(tokentype type) {
  if (current().type == type) {
    advance();
  } else {
    std::cerr << "unexpected token: '" << current().value << "'\n";
  }
}

bool parser::is_implicit_mul_start() {
  token t = current();
  if (t.type == tokentype::number || t.type == tokentype::variable ||
      t.type == tokentype::command || t.type == tokentype::lbrace) {
    if (t.type == tokentype::variable) {
      if (t.value == "from" || t.value == "to" || t.value == "wrt" ||
          t.value == "d" || t.value == "dx" || t.value == "dy" ||
          t.value == "dz" || t.value == "dt" || t.value == "du" ||
          t.value == "dv")
        return false;
    }
    return true;
  }
  if (t.type == tokentype::op && t.value == "(")
    return true;
  return false;
}

std::unique_ptr<expr> parser::parse_primary() {
  token t = current();

  // unary plus/minus
  if (t.type == tokentype::op && (t.value == "-" || t.value == "+")) {
    advance();
    auto operand = parse_factor();
    if (!operand)
      return nullptr;
    if (t.value == "-")
      return std::make_unique<multiply>(std::make_unique<number>(-1.0),
                                        std::move(operand));
    return operand;
  }

  // numbers
  if (t.type == tokentype::number) {
    advance();
    return std::make_unique<number>(std::stod(t.value));
  }

  // variables & commands
  if (t.type == tokentype::variable) {
    advance();

    if (t.value == "int") {
      auto integrand = parse_expr();
      if (!integrand)
        return nullptr;
      std::unique_ptr<expr> lower = nullptr, upper = nullptr;
      if (current().value == "from") {
        advance();
        lower = parse_expr();
        if (current().value == "to") {
          advance();
          upper = parse_expr();
        }
      }

      if (current().value == "wrt" || current().value == "d")
        advance();

      std::string var_name = "x";
      if (current().type == tokentype::variable) {
        if (current().value.size() > 1 && current().value[0] == 'd')
          var_name = current().value.substr(1);
        else
          var_name = current().value;
        advance();
      }
      return std::make_unique<integral>(std::move(lower), std::move(upper),
                                        std::move(integrand), var_name);
    }

    // function call
    if (current().type == tokentype::op && current().value == "(") {
      advance();
      std::vector<std::unique_ptr<expr>> args;
      if (current().type != tokentype::op || current().value != ")") {
        while (true) {
          args.push_back(parse_expr());
          if (current().type == tokentype::op && current().value == ",") {
            advance();
          } else {
            break;
          }
        }
      }
      if (current().type == tokentype::op && current().value == ")")
        advance();

      std::unique_ptr<expr> call =
          std::make_unique<func_call>(t.value, std::move(args));

      if (current().type == tokentype::op && current().value == "!") {
        advance();
        return std::make_unique<factorial_node>(std::move(call));
      }
      return call;
    }

    // factorial postfix
    if (current().type == tokentype::op && current().value == "!") {
      advance();
      return std::make_unique<factorial_node>(
          std::make_unique<variable>(t.value));
    }

    if (t.value == "pi" || t.value == "e" || t.value == "phi" ||
        t.value == "tau") {
      return std::make_unique<named_constant>(t.value);
    }
    return std::make_unique<variable>(t.value);
  }

  if (t.type == tokentype::command) {
    std::string cmd = t.value;
    advance();

    // integral
    if (cmd == "\\int") {
      std::unique_ptr<expr> lower = nullptr, upper = nullptr;
      if (current().value == "_") {
        advance();
        expect(tokentype::lbrace);
        lower = parse_expr();
        expect(tokentype::rbrace);
        expect(tokentype::op); // '^'
        expect(tokentype::lbrace);
        upper = parse_expr();
        expect(tokentype::rbrace);
      }
      auto integrand = parse_expr();
      if (current().value == "wrt" || current().value == "d")
        advance();
      std::string var_name = "x";
      if (current().type == tokentype::variable) {
        if (current().value.size() > 1 && current().value[0] == 'd')
          var_name = current().value.substr(1);
        else
          var_name = current().value;
        advance();
      }
      return std::make_unique<integral>(std::move(lower), std::move(upper),
                                        std::move(integrand), var_name);
    }

    // gcd / lcm
    if (cmd == "\\gcd" || cmd == "\\lcm" || cmd == "\\text{lcm}") {
      expect(tokentype::lbrace);
      auto a = parse_expr();
      expect(tokentype::rbrace);
      expect(tokentype::lbrace);
      auto b = parse_expr();
      expect(tokentype::rbrace);
      if (cmd == "\\gcd")
        return std::make_unique<gcd_node>(std::move(a), std::move(b));
      return std::make_unique<lcm_node>(std::move(a), std::move(b));
    }

    // fraction
    if (cmd == "\\frac") {
      size_t start = pos;
      expect(tokentype::lbrace);
      if (current().value == "d") {
        advance();
        if (current().type == tokentype::rbrace) {
          advance();
          expect(tokentype::lbrace);
          if (current().value == "d") {
            advance();
            std::string v = current().value;
            advance();
            expect(tokentype::rbrace);
            return std::make_unique<deriv_node>(v, parse_primary());
          }
        } else {
          auto arg = parse_expr();
          expect(tokentype::rbrace);
          expect(tokentype::lbrace);
          if (current().value == "d") {
            advance();
            std::string v = current().value;
            advance();
            expect(tokentype::rbrace);
            return std::make_unique<deriv_node>(v, std::move(arg));
          }
        }
      }
      pos = start;
      expect(tokentype::lbrace);
      auto n = parse_expr();
      expect(tokentype::rbrace);
      expect(tokentype::lbrace);
      auto d = parse_expr();
      expect(tokentype::rbrace);
      return std::make_unique<divide>(std::move(n), std::move(d));
    }

    // dbinom / binom
    if (cmd == "\\dbinom" || cmd == "\\binom" || cmd == "\\C") {
      expect(tokentype::lbrace);
      auto n = parse_expr();
      expect(tokentype::rbrace);
      expect(tokentype::lbrace);
      auto r = parse_expr();
      expect(tokentype::rbrace);
      return std::make_unique<combination_node>(std::move(n), std::move(r));
    }

    // P
    if (cmd == "\\P") {
      expect(tokentype::lbrace);
      auto n = parse_expr();
      expect(tokentype::rbrace);
      expect(tokentype::lbrace);
      auto r = parse_expr();
      expect(tokentype::rbrace);
      return std::make_unique<permutation_node>(std::move(n), std::move(r));
    }

    // absolute value
    if (cmd == "\\left") {
      if (current().value == "|") {
        advance();
        auto inner = parse_expr();
        if (current().type == tokentype::command &&
            current().value == "\\right")
          advance();
        if (current().value == "|")
          advance();
        return std::make_unique<abs_node>(std::move(inner));
      }
    }

    // generic command -> function call
    if (current().type == tokentype::lbrace) {
      advance();
      auto arg = parse_expr();
      expect(tokentype::rbrace);
      return std::make_unique<func_call>(cmd.substr(1), std::move(arg));
    }
    return std::make_unique<variable>(cmd);
  }

  // braces / parentheses
  if (t.type == tokentype::lbrace) {
    advance();
    auto e = parse_expr();
    expect(tokentype::rbrace);
    return e;
  }
  if (t.type == tokentype::op && t.value == "(") {
    advance();
    auto e = parse_expr();
    if (current().type == tokentype::op && current().value == ")")
      advance();
    return e;
  }

  return nullptr;
}

std::unique_ptr<expr> parser::parse_factor() {
  auto base = parse_primary();
  if (!base)
    return nullptr;
  while (current().value == "^") {
    advance();
    auto exponent = parse_primary();
    if (!exponent)
      break;
    base = std::make_unique<pow_node>(std::move(base), std::move(exponent));
  }
  while (current().type == tokentype::op && current().value == "!") {
    advance();
    base = std::make_unique<factorial_node>(std::move(base));
  }
  return base;
}

std::unique_ptr<expr> parser::parse_term() {
  auto left = parse_factor();
  if (!left)
    return nullptr;
  while (true) {
    if (current().value == "*" || current().value == "/") {
      std::string op = current().value;
      advance();
      auto right = parse_factor();
      if (!right)
        break;
      if (op == "*")
        left = std::make_unique<multiply>(std::move(left), std::move(right));
      else
        left = std::make_unique<divide>(std::move(left), std::move(right));
    } else if (is_implicit_mul_start()) {
      auto right = parse_factor();
      if (!right)
        break;
      left = std::make_unique<multiply>(std::move(left), std::move(right));
    } else {
      break;
    }
  }
  return left;
}

std::unique_ptr<expr> parser::parse_expr() {
  auto left = parse_term();
  if (!left)
    return nullptr;
  while (current().value == "+" || current().value == "-") {
    std::string op = current().value;
    advance();
    auto right = parse_term();
    if (!right)
      break;
    if (op == "+")
      left = std::make_unique<add>(std::move(left), std::move(right));
    else
      left = std::make_unique<add>(
          std::move(left),
          std::make_unique<multiply>(std::make_unique<number>(-1.0),
                                     std::move(right)));
  }
  return left;
}