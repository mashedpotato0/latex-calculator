#include "lexer.hpp"
#include <cctype>

std::vector<token> tokenize(const std::string &input) {
  std::vector<token> tokens;
  size_t i = 0;

  while (i < input.length()) {
    if (std::isspace(input[i])) {
      i++;
      continue;
    }

    if (input[i] == '\\') {
      std::string cmd;
      cmd += input[i];
      i++;
      while (i < input.length() && std::isalpha(input[i])) {
        cmd += input[i];
        i++;
      }
      tokens.push_back({tokentype::command, cmd});
    } else if (std::isdigit(input[i])) {
      std::string num;
      while (i < input.length() &&
             (std::isdigit(input[i]) || input[i] == '.')) {
        num += input[i];
        i++;
      }
      tokens.push_back({tokentype::number, num});
    } else if (std::isalpha(input[i])) {
      std::string var;
      while (i < input.length() && std::isalnum(input[i])) {
        var += input[i];
        i++;
      }
      tokens.push_back({tokentype::variable, var});
    } else if (input[i] == '{') {
      tokens.push_back({tokentype::lbrace, "{"});
      i++;
    } else if (input[i] == '}') {
      tokens.push_back({tokentype::rbrace, "}"});
      i++;
    } else {
      tokens.push_back({tokentype::op, std::string(1, input[i])});
      i++;
    }
  }

  tokens.push_back({tokentype::eof, ""});
  return tokens;
}