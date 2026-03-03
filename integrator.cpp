#include "integrator.hpp"
#include "ast.hpp"
#include <optional>
#include <cmath>

namespace symbolic {

std::unique_ptr<expr> integrate(const expr& e, const std::string& var, int depth);

static std::unique_ptr<expr> try_constant_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_power_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_sum_rule(const expr& e, const std::string& var, int depth);
static std::unique_ptr<expr> try_constant_multiple_rule(const expr& e, const std::string& var, int depth);
static std::unique_ptr<expr> try_rational_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_exp_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_log_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_trig_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_inverse_trig_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_hyperbolic_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_inverse_hyperbolic_rule(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_chain_patterns(const expr& e, const std::string& var);
static std::unique_ptr<expr> try_substitution_rule(const expr& e, const std::string& var, int depth);
static std::unique_ptr<expr> try_parts_rule(const expr& e, const std::string& var, int depth);

// ── helpers ───────────────────────────────────────────────────────────────────

static std::unique_ptr<expr> num(double v)                           { return std::make_unique<number>(v); }
static std::unique_ptr<expr> var_node(const std::string& n)          { return std::make_unique<variable>(n); }
static std::unique_ptr<expr> func(const std::string& n, std::unique_ptr<expr> a) { return std::make_unique<func_call>(n, std::move(a)); }
static std::unique_ptr<expr> mul(std::unique_ptr<expr> l, std::unique_ptr<expr> r) { return std::make_unique<multiply>(std::move(l), std::move(r)); }
static std::unique_ptr<expr> add_e(std::unique_ptr<expr> l, std::unique_ptr<expr> r) { return std::make_unique<add>(std::move(l), std::move(r)); }
// subtraction: a + (-1)*b
static std::unique_ptr<expr> sub_e(std::unique_ptr<expr> l, std::unique_ptr<expr> r) { return add_e(std::move(l), mul(num(-1.0), std::move(r))); }
static std::unique_ptr<expr> div_e(std::unique_ptr<expr> l, std::unique_ptr<expr> r) { return std::make_unique<divide>(std::move(l), std::move(r)); }
static std::unique_ptr<expr> pow_e(std::unique_ptr<expr> b, std::unique_ptr<expr> e) { return std::make_unique<pow_node>(std::move(b), std::move(e)); }
static std::unique_ptr<expr> neg(std::unique_ptr<expr> e)            { return mul(num(-1.0), std::move(e)); }

static bool contains_var(const expr& e, const std::string& var) {
    if (auto v = dynamic_cast<const variable*>(&e))   return v->name == var;
    if (dynamic_cast<const number*>(&e))               return false;
    if (auto a = dynamic_cast<const add*>(&e))         return contains_var(*a->left, var) || contains_var(*a->right, var);
    if (auto m = dynamic_cast<const multiply*>(&e))    return contains_var(*m->left, var) || contains_var(*m->right, var);
    if (auto d = dynamic_cast<const divide*>(&e))      return contains_var(*d->left, var) || contains_var(*d->right, var);
    if (auto p = dynamic_cast<const pow_node*>(&e))    return contains_var(*p->base, var) || contains_var(*p->exponent, var);
    if (auto f = dynamic_cast<const func_call*>(&e))   return contains_var(*f->arg, var);
    return false;
}

// returns the numeric value of a node iff it is a number literal
static std::optional<double> num_val(const expr& e) {
    if (auto n = dynamic_cast<const number*>(&e)) return n->val;
    return std::nullopt;
}

// extracts (coeff, x-power) from a pure monomial like k*x^n; returns false if not monomial
static bool extract_monomial(const expr& e, const std::string& var, double& coeff, double& power) {
    if (auto n = dynamic_cast<const number*>(&e))       { coeff = n->val; power = 0;   return true; }
    if (auto v = dynamic_cast<const variable*>(&e))     { if (v->name == var) { coeff = 1; power = 1; return true; } }
    if (auto p = dynamic_cast<const pow_node*>(&e)) {
        auto v = dynamic_cast<const variable*>(p->base.get());
        auto n = num_val(*p->exponent);
        if (v && v->name == var && n) { coeff = 1; power = *n; return true; }
    }
    if (auto m = dynamic_cast<const multiply*>(&e)) {
        double lc, lp, rc, rp;
        if (extract_monomial(*m->left,  var, lc, lp) &&
            extract_monomial(*m->right, var, rc, rp)) { coeff = lc*rc; power = lp+rp; return true; }
    }
    return false;
}

// evaluates a confirmed-constant expression numerically and returns num(k)
static std::unique_ptr<expr> eval_const(const expr& e, const std::string& var) {
    context ctx;
    ctx.builtins["sin"]=ctx.builtins["cos"]=ctx.builtins["tan"]=
    ctx.builtins["exp"]=ctx.builtins["log"]=ctx.builtins["ln"]=
    ctx.builtins["sqrt"] = [](double x){ return x; }; // dummy — won't be called for const expr
    ctx.builtins["sin"]  = [](double x){ return std::sin(x);  };
    ctx.builtins["cos"]  = [](double x){ return std::cos(x);  };
    ctx.builtins["tan"]  = [](double x){ return std::tan(x);  };
    ctx.builtins["exp"]  = [](double x){ return std::exp(x);  };
    ctx.builtins["log"]  = [](double x){ return std::log(x);  };
    ctx.builtins["ln"]   = [](double x){ return std::log(x);  };
    ctx.builtins["sqrt"] = [](double x){ return std::sqrt(x); };
    ctx.vars[var] = 1.5;
    double k = e.eval(ctx);
    return num(k);
}
static bool is_const_in(const expr& e, const std::string& var) {
    if (!contains_var(e, var)) return true;
    context ctx;
    ctx.builtins["sin"]  = [](double x){ return std::sin(x);  };
    ctx.builtins["cos"]  = [](double x){ return std::cos(x);  };
    ctx.builtins["tan"]  = [](double x){ return std::tan(x);  };
    ctx.builtins["exp"]  = [](double x){ return std::exp(x);  };
    ctx.builtins["log"]  = [](double x){ return std::log(x);  };
    ctx.builtins["ln"]   = [](double x){ return std::log(x);  };
    ctx.builtins["sqrt"] = [](double x){ return std::sqrt(x); };
    const double pts[] = {1.3, 2.7, 4.1};
    double base = 0;
    for (int i = 0; i < 3; i++) {
        ctx.vars[var] = pts[i];
        double v = e.eval(ctx);
        if (std::isnan(v) || std::isinf(v)) return false;
        if (i == 0) base = v;
        else if (std::abs(v - base) > 1e-6) return false;
    }
    return true;
}

// simplifies (a/b)*(c/d) → (a*c)/(b*d)  so parts' v*du reduces correctly
static std::unique_ptr<expr> flatten_frac_product(const expr& e) {
    auto m = dynamic_cast<const multiply*>(&e);
    if (!m) return nullptr;
    auto ld = dynamic_cast<const divide*>(m->left.get());
    auto rd = dynamic_cast<const divide*>(m->right.get());
    if (ld && rd)
        return div_e(mul(ld->left->clone(), rd->left->clone()),
                     mul(ld->right->clone(), rd->right->clone()))->simplify();
    // k*(a/b)  or  (a/b)*k  → (k*a)/b
    if (ld && !contains_var(*m->right, ""))  // always try
        return div_e(mul(m->right->clone(), ld->left->clone()), ld->right->clone())->simplify();
    if (rd)
        return div_e(mul(m->left->clone(), rd->left->clone()), rd->right->clone())->simplify();
    return nullptr;
}

// tries to match e == a*var + b (a ≠ 0)
static bool extract_linear(const expr& e, const std::string& var, double& a, double& b) {
    if (auto v = dynamic_cast<const variable*>(&e)) {
        if (v->name == var) { a = 1.0; b = 0.0; return true; }
    }
    // k * x  or  x * k
    if (auto m = dynamic_cast<const multiply*>(&e)) {
        auto try_kx = [&](const expr* k, const expr* x) -> bool {
            auto kv = num_val(*k);
            if (kv && dynamic_cast<const variable*>(x) && dynamic_cast<const variable*>(x)->name == var)
                { a = *kv; b = 0.0; return true; }
            return false;
        };
        if (try_kx(m->left.get(), m->right.get())) return true;
        if (try_kx(m->right.get(), m->left.get())) return true;
    }
    // linear + constant  (note: no subtract node — subtraction is add(x, mul(-1, y)))
    if (auto ap = dynamic_cast<const add*>(&e)) {
        auto try_side = [&](const expr* lin, const expr* con) -> bool {
            if (contains_var(*con, var)) return false;
            auto bv = num_val(*con);
            if (!bv) return false;
            double ta, tb;
            if (extract_linear(*lin, var, ta, tb)) { a = ta; b = tb + *bv; return true; }
            return false;
        };
        if (try_side(ap->left.get(),  ap->right.get())) return true;
        if (try_side(ap->right.get(), ap->left.get()))  return true;
    }
    return false;
}

// LIATE ordering for integration by parts: Log < InvTrig < Algebraic < Trig < Exp
static int liate(const expr& e, const std::string& /*var*/) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        const auto& n = f->name;
        if (n=="log"||n=="ln"||n=="log2"||n=="log10") return 0;
        if (n=="arcsin"||n=="arccos"||n=="arctan"||n=="arccot"||n=="arcsec"||n=="arccsc"||
            n=="asin"||n=="acos"||n=="atan"||n=="acot"||n=="asec"||n=="acsc"||
            n=="arcsinh"||n=="arccosh"||n=="arctanh"||n=="arccoth"||n=="arcsech"||n=="arccsch") return 1;
        if (n=="sin"||n=="cos"||n=="tan"||n=="cot"||n=="sec"||n=="csc") return 3;
        if (n=="exp"||n=="sinh"||n=="cosh"||n=="tanh"||n=="coth"||n=="sech"||n=="csch") return 4;
    }
    if (dynamic_cast<const pow_node*>(&e) || dynamic_cast<const variable*>(&e)) return 2;
    return 5;
}

// ── dispatcher ────────────────────────────────────────────────────────────────

std::unique_ptr<expr> integrate(const expr& e, const std::string& var, int depth) {
    if (depth > 8) return nullptr;

    auto s = e.simplify();

#define TRY(fn) if (auto res = fn) return res->simplify()
    TRY(try_constant_rule(*s, var));
    TRY(try_sum_rule(*s, var, depth));
    TRY(try_constant_multiple_rule(*s, var, depth));
    TRY(try_power_rule(*s, var));
    TRY(try_rational_rule(*s, var));
    TRY(try_exp_rule(*s, var));
    TRY(try_log_rule(*s, var));
    TRY(try_trig_rule(*s, var));
    TRY(try_inverse_trig_rule(*s, var));
    TRY(try_hyperbolic_rule(*s, var));
    TRY(try_inverse_hyperbolic_rule(*s, var));
    TRY(try_chain_patterns(*s, var));
    // reduce monomials like x^n / (k*x^m) before trying substitution/parts
    if (auto dv = dynamic_cast<const divide*>(s.get())) {
        double nc, np, dc, dp;
        if (extract_monomial(*dv->left, var, nc, np) && extract_monomial(*dv->right, var, dc, dp)) {
            double newp = np - dp;
            auto simplified = (std::abs(newp) < 1e-9)
                ? num(nc / dc)
                : mul(num(nc / dc), pow_e(var_node(var), num(newp)));
            if (auto res = integrate(*simplified, var, depth)) return res->simplify();
        }
    }
    TRY(try_substitution_rule(*s, var, depth));
    TRY(try_parts_rule(*s, var, depth));
#undef TRY
    return nullptr;
}

// ── tier 1: structural / linearity ───────────────────────────────────────────

// ∫ c dx = c*x
static std::unique_ptr<expr> try_constant_rule(const expr& e, const std::string& var) {
    if (!contains_var(e, var)) return mul(e.clone(), var_node(var));
    return nullptr;
}

// ∫ (f+g) dx = ∫f + ∫g
static std::unique_ptr<expr> try_sum_rule(const expr& e, const std::string& var, int depth) {
    if (auto a = dynamic_cast<const add*>(&e)) {
        auto l = integrate(*a->left,  var, depth);
        auto r = integrate(*a->right, var, depth);
        if (l && r) return add_e(std::move(l), std::move(r));
    }
    return nullptr;
}

// ∫ k*f dx = k*∫f,  ∫ f/k dx = (1/k)*∫f
static std::unique_ptr<expr> try_constant_multiple_rule(const expr& e, const std::string& var, int depth) {
    if (auto m = dynamic_cast<const multiply*>(&e)) {
        if (!contains_var(*m->left, var)) {
            if (auto i = integrate(*m->right, var, depth)) return mul(m->left->clone(), std::move(i));
        }
        if (!contains_var(*m->right, var)) {
            if (auto i = integrate(*m->left, var, depth)) return mul(m->right->clone(), std::move(i));
        }
    }
    if (auto d = dynamic_cast<const divide*>(&e)) {
        if (!contains_var(*d->right, var)) {
            if (auto i = integrate(*d->left, var, depth)) return div_e(std::move(i), d->right->clone());
        }
    }
    return nullptr;
}

// ── tier 2: closed-form patterns ──────────────────────────────────────────────

// ∫ x^n = x^(n+1)/(n+1),  ∫ x^(-1) = ln|x|
static std::unique_ptr<expr> try_power_rule(const expr& e, const std::string& var) {
    if (auto v = dynamic_cast<const variable*>(&e))
        if (v->name == var) return div_e(pow_e(var_node(var), num(2.0)), num(2.0));

    if (auto p = dynamic_cast<const pow_node*>(&e)) {
        auto v = dynamic_cast<const variable*>(p->base.get());
        if (!v || v->name != var || contains_var(*p->exponent, var)) return nullptr;
        if (auto nv = num_val(*p->exponent)) {
            double n = *nv, np1 = n + 1.0;
            if (std::abs(np1) < 1e-9) return func("log", var_node(var));
            return div_e(pow_e(var_node(var), num(np1)), num(np1));
        }
        // symbolic exponent
        auto np1 = add_e(p->exponent->clone(), num(1.0));
        return div_e(pow_e(var_node(var), np1->clone()), np1->clone());
    }
    return nullptr;
}

// ∫ c/(ax+b) = (c/a)*ln|ax+b|
// ∫ c/(x²+a²) = (c/a)*arctan(x/a)
// ∫ c/(x²-a²) = (c/2a)*ln|(x-a)/(x+a)|
// ∫ c/sqrt(a²-x²) = c*arcsin(x/a)
static std::unique_ptr<expr> try_rational_rule(const expr& e, const std::string& var) {
    auto d = dynamic_cast<const divide*>(&e);
    if (!d || !contains_var(*d->right, var) || contains_var(*d->left, var)) return nullptr;

    double c = num_val(*d->left).value_or(1.0);
    const expr* denom = d->right.get();

    // c / x
    if (auto v = dynamic_cast<const variable*>(denom))
        if (v->name == var)
            return (std::abs(c - 1.0) < 1e-9) ? func("log", var_node(var)) : mul(num(c), func("log", var_node(var)));

    // c / (ax+b)
    {
        double a, b;
        if (extract_linear(*denom, var, a, b))
            return mul(num(c / a), func("log", denom->clone()));
    }

    // c / (x²±k)
    {
        auto match_x2k = [&](const expr* ep, double& k) -> bool {
            auto is_x2 = [&](const expr* q) -> bool {
                auto p = dynamic_cast<const pow_node*>(q);
                auto vn = p ? dynamic_cast<const variable*>(p->base.get()) : nullptr;
                auto en = p ? num_val(*p->exponent) : std::nullopt;
                return vn && vn->name == var && en && std::abs(*en - 2.0) < 1e-9;
            };
            if (auto ap = dynamic_cast<const add*>(ep)) {
                auto try_side = [&](const expr* x2s, const expr* ks) -> bool {
                    if (!is_x2(x2s) || contains_var(*ks, var)) return false;
                    auto kv = num_val(*ks); if (!kv) return false;
                    k = *kv; return true;
                };
                if (try_side(ap->left.get(),  ap->right.get())) return true;
                if (try_side(ap->right.get(), ap->left.get()))  return true;
            }
            return false;
        };
        double k;
        if (match_x2k(denom, k)) {
            if (k > 0) {
                double a = std::sqrt(k);
                return mul(num(c / a), func("arctan", div_e(var_node(var), num(a))));
            } else {
                double a = std::sqrt(-k);
                return mul(num(c / (2.0 * a)),
                    func("log", div_e(sub_e(var_node(var), num(a)), add_e(var_node(var), num(a)))));
            }
        }
    }

    // c / sqrt(a²-x²)
    {
        auto as_sqrt = [&](const expr* ep) -> const expr* {
            if (auto f = dynamic_cast<const func_call*>(ep)) if (f->name == "sqrt") return f->arg.get();
            if (auto p = dynamic_cast<const pow_node*>(ep)) {
                auto ex = num_val(*p->exponent);
                if (ex && std::abs(*ex - 0.5) < 1e-9) return p->base.get();
            }
            return nullptr;
        };
        if (const expr* inner = as_sqrt(denom)) {
            // match a² - x²  (as add(a², mul(-1, x²)))
            if (auto ap = dynamic_cast<const add*>(inner)) {
                auto av = num_val(*ap->left);
                auto try_neg_x2 = [&](const expr* neg_x2) -> bool {
                    auto m = dynamic_cast<const multiply*>(neg_x2);
                    if (!m) return false;
                    auto coef = num_val(*m->left);
                    if (!coef || std::abs(*coef + 1.0) > 1e-9) return false;
                    auto p = dynamic_cast<const pow_node*>(m->right.get());
                    auto vn = p ? dynamic_cast<const variable*>(p->base.get()) : nullptr;
                    auto en = p ? num_val(*p->exponent) : std::nullopt;
                    return vn && vn->name == var && en && std::abs(*en - 2.0) < 1e-9;
                };
                if (av && try_neg_x2(ap->right.get())) {
                    double a = std::sqrt(*av);
                    return mul(num(c), func("arcsin", div_e(var_node(var), num(a))));
                }
            }
        }
    }
    return nullptr;
}

// ∫ exp(ax+b) = (1/a)*exp(ax+b)
// ∫ a^(kx+b) = a^(kx+b)/(k*ln a)
static std::unique_ptr<expr> try_exp_rule(const expr& e, const std::string& var) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        if (f->name == "exp") {
            double a, b;
            if (extract_linear(*f->arg, var, a, b))
                return mul(num(1.0 / a), func("exp", f->arg->clone()));
        }
    }
    if (auto p = dynamic_cast<const pow_node*>(&e)) {
        if (!contains_var(*p->base, var)) {
            double a, b;
            if (extract_linear(*p->exponent, var, a, b)) {
                if (auto bv = num_val(*p->base))
                    return div_e(e.clone(), num(a * std::log(*bv)));
                return div_e(e.clone(), mul(num(a), func("log", p->base->clone())));
            }
        }
    }
    return nullptr;
}

// ∫ ln(x) = x*ln(x) - x
// ∫ log_a(x) = x*log_a(x) - x/ln(a)
static std::unique_ptr<expr> try_log_rule(const expr& e, const std::string& var) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        if (!f->arg->equals(variable(var))) return nullptr;
        const auto& n = f->name;
        if (n == "log" || n == "ln")
            return sub_e(mul(var_node(var), func(n, var_node(var))), var_node(var));
        if (n == "log2")
            return sub_e(mul(var_node(var), func("log2", var_node(var))),
                         div_e(var_node(var), func("log", num(2.0))));
        if (n == "log10")
            return sub_e(mul(var_node(var), func("log10", var_node(var))),
                         div_e(var_node(var), func("log", num(10.0))));
    }
    return nullptr;
}

// standard trig rules for bare var argument; powers & products
static std::unique_ptr<expr> try_trig_rule(const expr& e, const std::string& var) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        if (!f->arg->equals(variable(var))) return nullptr;
        const auto& n = f->name;
        if (n == "sin") return neg(func("cos", var_node(var)));
        if (n == "cos") return func("sin", var_node(var));
        if (n == "tan") return neg(func("log", func("cos", var_node(var))));
        if (n == "cot") return func("log", func("sin", var_node(var)));
        if (n == "sec") return func("log", add_e(func("sec", var_node(var)), func("tan", var_node(var))));
        if (n == "csc") return func("log", sub_e(func("csc", var_node(var)), func("cot", var_node(var))));
    }
    if (auto p = dynamic_cast<const pow_node*>(&e)) {
        auto fc = dynamic_cast<const func_call*>(p->base.get());
        if (!fc || !fc->arg->equals(variable(var))) return nullptr;
        auto nv = num_val(*p->exponent); if (!nv) return nullptr;
        const auto& fn = fc->name;
        if (std::abs(*nv - 2.0) < 1e-9) {
            if (fn == "sec") return func("tan", var_node(var));
            if (fn == "csc") return neg(func("cot", var_node(var)));
            if (fn == "sin") return sub_e(div_e(var_node(var), num(2.0)), div_e(func("sin", mul(num(2.0), var_node(var))), num(4.0)));
            if (fn == "cos") return add_e(div_e(var_node(var), num(2.0)), div_e(func("sin", mul(num(2.0), var_node(var))), num(4.0)));
            if (fn == "tan") return sub_e(func("tan", var_node(var)), var_node(var));
            if (fn == "cot") return sub_e(neg(func("cot", var_node(var))), var_node(var));
        }
        if (std::abs(*nv - 3.0) < 1e-9) {
            if (fn == "sec") return mul(num(0.5), add_e(mul(func("sec", var_node(var)), func("tan", var_node(var))), func("log", add_e(func("sec", var_node(var)), func("tan", var_node(var))))));
            if (fn == "csc") return mul(num(0.5), add_e(neg(mul(func("csc", var_node(var)), func("cot", var_node(var)))), func("log", sub_e(func("csc", var_node(var)), func("cot", var_node(var))))));
        }
    }
    if (auto m = dynamic_cast<const multiply*>(&e)) {
        auto fa = dynamic_cast<const func_call*>(m->left.get());
        auto fb = dynamic_cast<const func_call*>(m->right.get());
        if (fa && fb && fa->arg->equals(variable(var)) && fb->arg->equals(variable(var))) {
            auto& na = fa->name; auto& nb = fb->name;
            if ((na=="sec"&&nb=="tan")||(na=="tan"&&nb=="sec")) return func("sec", var_node(var));
            if ((na=="csc"&&nb=="cot")||(na=="cot"&&nb=="csc")) return neg(func("csc", var_node(var)));
            if ((na=="sin"&&nb=="cos")||(na=="cos"&&nb=="sin")) return div_e(pow_e(func("sin", var_node(var)), num(2.0)), num(2.0));
        }
    }
    return nullptr;
}

// ∫ arcsin x, arccos x, arctan x, arccot x, arcsec x, arccsc x
static std::unique_ptr<expr> try_inverse_trig_rule(const expr& e, const std::string& var) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        if (!f->arg->equals(variable(var))) return nullptr;
        const auto& n = f->name;
        auto x2 = [&]{ return pow_e(var_node(var), num(2.0)); };
        if (n=="arcsin"||n=="asin")
            return add_e(mul(var_node(var), func(n, var_node(var))), pow_e(sub_e(num(1.0), x2()), num(0.5)));
        if (n=="arccos"||n=="acos")
            return sub_e(mul(var_node(var), func(n, var_node(var))), pow_e(sub_e(num(1.0), x2()), num(0.5)));
        if (n=="arctan"||n=="atan")
            return sub_e(mul(var_node(var), func(n, var_node(var))), mul(num(0.5), func("log", add_e(num(1.0), x2()))));
        if (n=="arccot"||n=="acot")
            return add_e(mul(var_node(var), func(n, var_node(var))), mul(num(0.5), func("log", add_e(num(1.0), x2()))));
        if (n=="arcsec"||n=="asec")
            return sub_e(mul(var_node(var), func(n, var_node(var))),
                func("log", mul(var_node(var), add_e(num(1.0), pow_e(sub_e(num(1.0), pow_e(var_node(var), num(-2.0))), num(0.5))))));
        if (n=="arccsc"||n=="acsc")
            return add_e(mul(var_node(var), func(n, var_node(var))),
                func("log", mul(var_node(var), add_e(num(1.0), pow_e(sub_e(num(1.0), pow_e(var_node(var), num(-2.0))), num(0.5))))));
    }
    return nullptr;
}

// ∫ sinh, cosh, tanh, coth, sech, csch, sech², csch², sech·tanh, csch·coth
static std::unique_ptr<expr> try_hyperbolic_rule(const expr& e, const std::string& var) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        if (!f->arg->equals(variable(var))) return nullptr;
        const auto& n = f->name;
        if (n == "sinh") return func("cosh", var_node(var));
        if (n == "cosh") return func("sinh", var_node(var));
        if (n == "tanh") return func("log",  func("cosh", var_node(var)));
        if (n == "coth") return func("log",  func("sinh", var_node(var)));
        if (n == "sech") return func("arctan", func("sinh", var_node(var)));
        if (n == "csch") return func("log",  func("tanh", div_e(var_node(var), num(2.0))));
    }
    if (auto p = dynamic_cast<const pow_node*>(&e)) {
        auto fc = dynamic_cast<const func_call*>(p->base.get());
        auto ne = num_val(*p->exponent);
        if (fc && ne && std::abs(*ne - 2.0) < 1e-9 && fc->arg->equals(variable(var))) {
            if (fc->name == "sech") return func("tanh", var_node(var));
            if (fc->name == "csch") return neg(func("coth", var_node(var)));
        }
    }
    if (auto m = dynamic_cast<const multiply*>(&e)) {
        auto fa = dynamic_cast<const func_call*>(m->left.get());
        auto fb = dynamic_cast<const func_call*>(m->right.get());
        if (fa && fb && fa->arg->equals(variable(var)) && fb->arg->equals(variable(var))) {
            auto& na = fa->name; auto& nb = fb->name;
            if ((na=="sech"&&nb=="tanh")||(na=="tanh"&&nb=="sech")) return neg(func("sech", var_node(var)));
            if ((na=="csch"&&nb=="coth")||(na=="coth"&&nb=="csch")) return neg(func("csch", var_node(var)));
        }
    }
    return nullptr;
}

// ∫ arcsinh, arccosh, arctanh, arccoth, arcsech, arccsch
static std::unique_ptr<expr> try_inverse_hyperbolic_rule(const expr& e, const std::string& var) {
    if (auto f = dynamic_cast<const func_call*>(&e)) {
        if (!f->arg->equals(variable(var))) return nullptr;
        const auto& n = f->name;
        auto x2 = [&]{ return pow_e(var_node(var), num(2.0)); };
        if (n=="arcsinh"||n=="asinh")
            return sub_e(mul(var_node(var), func(n, var_node(var))), pow_e(add_e(x2(), num(1.0)), num(0.5)));
        if (n=="arccosh"||n=="acosh")
            return sub_e(mul(var_node(var), func(n, var_node(var))), pow_e(sub_e(x2(), num(1.0)), num(0.5)));
        if (n=="arctanh"||n=="atanh")
            return add_e(mul(var_node(var), func(n, var_node(var))), mul(num(0.5), func("log", sub_e(num(1.0), x2()))));
        if (n=="arccoth"||n=="acoth")
            return add_e(mul(var_node(var), func(n, var_node(var))), mul(num(0.5), func("log", sub_e(x2(), num(1.0)))));
        if (n=="arcsech"||n=="asech")
            return add_e(mul(var_node(var), func(n, var_node(var))), func("arcsin", var_node(var)));
        if (n=="arccsch"||n=="acsch")
            return add_e(mul(var_node(var), func(n, var_node(var))), func("arcsinh", var_node(var)));
    }
    return nullptr;
}

// ∫ f'(x)*exp(f(x)) = exp(f(x))
// ∫ f'(x)/f(x) = ln|f(x)|
static std::unique_ptr<expr> try_chain_patterns(const expr& e, const std::string& var) {
    if (auto m = dynamic_cast<const multiply*>(&e)) {
        auto try_fprime_expf = [&](const expr* a, const expr* b) -> std::unique_ptr<expr> {
            auto ef = dynamic_cast<const func_call*>(b);
            if (!ef || ef->name != "exp" || !contains_var(*ef->arg, var)) return nullptr;
            auto fp    = ef->arg->derivative(var)->simplify();
            auto ratio = div_e(a->clone(), fp->clone())->simplify();
            if (!is_const_in(*ratio, var)) return nullptr;
            return mul(eval_const(*ratio, var), func("exp", ef->arg->clone()));
        };
        if (auto r = try_fprime_expf(m->left.get(),  m->right.get())) return r;
        if (auto r = try_fprime_expf(m->right.get(), m->left.get()))  return r;
    }
    if (auto dv = dynamic_cast<const divide*>(&e)) {
        if (!contains_var(*dv->right, var)) return nullptr;
        auto fp    = dv->right->derivative(var)->simplify();
        auto ratio = div_e(dv->left->clone(), fp->clone())->simplify();
        if (!is_const_in(*ratio, var)) return nullptr;
        return mul(eval_const(*ratio, var), func("log", dv->right->clone()));
    }
    return nullptr;
}

// ── tier 3: algorithmic ────────────────────────────────────────────────────────

// u-substitution: ∫ h(x)*f(g(x)) dx  where h == k*g'(x)
static std::unique_ptr<expr> try_substitution_rule(const expr& e, const std::string& var, int depth) {
    auto m = dynamic_cast<const multiply*>(&e);
    if (!m) return nullptr;

    auto attempt = [&](const expr& composite, const expr& multiplier) -> std::unique_ptr<expr> {
        // func_call(g(x))
        if (auto fc = dynamic_cast<const func_call*>(&composite)) {
            if (fc->arg->equals(variable(var)) || !contains_var(*fc->arg, var)) return nullptr;
            auto gp    = fc->arg->derivative(var)->simplify();
            auto ratio = div_e(multiplier.clone(), gp->clone())->simplify();
            if (!is_const_in(*ratio, var)) return nullptr;
            ratio = eval_const(*ratio, var);
            auto dummy = std::make_unique<func_call>(fc->name, var_node("__u__"));
            auto Fu    = integrate(*dummy, "__u__", depth + 1);
            if (!Fu) return nullptr;
            return mul(ratio->clone(), Fu->substitute("__u__", *fc->arg));
        }
        // g(x)^n
        if (auto p = dynamic_cast<const pow_node*>(&composite)) {
            if (p->base->equals(variable(var)) || !contains_var(*p->base, var) || contains_var(*p->exponent, var)) return nullptr;
            auto gp    = p->base->derivative(var)->simplify();
            auto ratio = div_e(multiplier.clone(), gp->clone())->simplify();
            if (!is_const_in(*ratio, var)) return nullptr;
            ratio = eval_const(*ratio, var);
            if (auto nv = num_val(*p->exponent)) {
                double n = *nv, np1 = n + 1.0;
                if (std::abs(np1) < 1e-9) return mul(ratio->clone(), func("log", p->base->clone()));
                return mul(ratio->clone(), div_e(pow_e(p->base->clone(), num(np1)), num(np1)));
            }
            auto np1 = add_e(p->exponent->clone(), num(1.0));
            return mul(ratio->clone(), div_e(pow_e(p->base->clone(), np1->clone()), np1->clone()));
        }
        return nullptr;
    };

    if (auto r = attempt(*m->left,  *m->right)) return r;
    if (auto r = attempt(*m->right, *m->left))  return r;
    return nullptr;
}

// integration by parts: ∫ u dv = u*v - ∫ v du  (LIATE ordering)
static std::unique_ptr<expr> try_parts_rule(const expr& e, const std::string& var, int depth) {
    auto m = dynamic_cast<const multiply*>(&e);
    if (!m) return nullptr;

    auto try_with = [&](const expr* u, const expr* dv_expr) -> std::unique_ptr<expr> {
        auto v = integrate(*dv_expr, var, depth + 1);
        if (!v) return nullptr;
        auto du       = u->derivative(var)->simplify();
        auto v_du_raw = mul(v->clone(), du->clone());
        auto v_du     = flatten_frac_product(*v_du_raw) ? flatten_frac_product(*v_du_raw) : v_du_raw->simplify();
        auto int_vdu  = integrate(*v_du, var, depth + 1);
        if (!int_vdu) return nullptr;
        return sub_e(mul(u->clone(), std::move(v)), std::move(int_vdu));
    };

    int ol = liate(*m->left, var), or_ = liate(*m->right, var);
    if (ol <= or_) {
        if (auto r = try_with(m->left.get(),  m->right.get())) return r;
        if (auto r = try_with(m->right.get(), m->left.get()))  return r;
    } else {
        if (auto r = try_with(m->right.get(), m->left.get()))  return r;
        if (auto r = try_with(m->left.get(),  m->right.get()))  return r;
    }
    return nullptr;
}

} // namespace symbolic