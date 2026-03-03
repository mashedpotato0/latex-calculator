#pragma once
#include <string>
#include <vector>

enum class tokentype { command, number, variable, lbrace, rbrace, op, eof };

struct token {
    tokentype type;
    std::string value;
};

std::vector<token> tokenize(const std::string& input);