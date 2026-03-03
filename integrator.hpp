#pragma once
#include "ast.hpp"

namespace symbolic {
    std::unique_ptr<expr> integrate(const expr& e, const std::string& var, int depth);
}
