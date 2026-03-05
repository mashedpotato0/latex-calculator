#pragma once
#include <memory>
#include <string>

struct expr;

namespace symbolic {
    // default depth is already specified in ast.hpp
    std::unique_ptr<expr> integrate(const expr& e, const std::string& var, int depth);
}