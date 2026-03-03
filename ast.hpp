#pragma once
#include <string>
#include <map>
#include <memory>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

struct context;
struct expr;

namespace symbolic {
    // core symbolic integration entry point
    std::unique_ptr<expr> integrate(const expr& e, const std::string& var, int depth = 0);
}

struct user_func {
    std::string param_name;
    std::shared_ptr<expr> body;
};

struct context {
    std::map<std::string, double> vars;
    std::map<std::string, user_func> funcs;
    std::map<std::string, std::function<double(double)>> builtins;
};

struct expr {
    virtual ~expr() = default;
    virtual double eval(context& ctx) const = 0;
    virtual std::string to_string() const = 0;
    virtual std::unique_ptr<expr> clone() const = 0;
    virtual std::unique_ptr<expr> derivative(const std::string& var) const = 0;
    virtual std::unique_ptr<expr> simplify() const = 0;
    virtual std::unique_ptr<expr> substitute(const std::string& var, const expr& replacement) const = 0;
    virtual std::unique_ptr<expr> expand(const context& ctx) const = 0;
    virtual bool equals(const expr& other) const = 0;
    virtual bool is_zero() const { return false; }
    virtual bool is_one() const { return false; }
    virtual bool is_number() const { return false; }
};

struct number : expr {
    double val;
    number(double v) : val(v) {}
    double eval(context&) const override { return val; }
    std::string to_string() const override { 
        if (std::abs(val + 1.0) < 1e-9) return "-1";
        std::string str = std::to_string(val);
        str.erase(str.find_last_not_of('0') + 1, std::string::npos);
        if (str.back() == '.') str.pop_back();
        return str; 
    }
    std::unique_ptr<expr> clone() const override { return std::make_unique<number>(val); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override { return clone(); }
    std::unique_ptr<expr> substitute(const std::string&, const expr&) const override { return clone(); }
    std::unique_ptr<expr> expand(const context&) const override { return clone(); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const number*>(&other);
        return o && std::abs(val - o->val) < 1e-7;
    }
    bool is_zero() const override { return std::abs(val) < 1e-9; }
    bool is_one() const override { return std::abs(val - 1.0) < 1e-9; }
    bool is_number() const override { return true; }
};

struct variable : expr {
    std::string name;
    variable(std::string n) : name(n) {}
    double eval(context& ctx) const override { return ctx.vars[name]; }
    std::string to_string() const override { return name; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<variable>(name); }
    std::unique_ptr<expr> derivative(const std::string& var) const override {
        return std::make_unique<number>(name == var ? 1.0 : 0.0);
    }
    std::unique_ptr<expr> simplify() const override { return clone(); }
    std::unique_ptr<expr> expand(const context&) const override { return clone(); }
    std::unique_ptr<expr> substitute(const std::string& var, const expr& r) const override {
        return name == var ? r.clone() : clone();
    }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const variable*>(&other);
        return o && name == o->name;
    }
};

struct add : expr {
    std::unique_ptr<expr> left, right;
    add(std::unique_ptr<expr> l, std::unique_ptr<expr> r) : left(std::move(l)), right(std::move(r)) {}
    double eval(context& ctx) const override { return left->eval(ctx) + right->eval(ctx); }
    std::string to_string() const override { return "(" + left->to_string() + "+" + right->to_string() + ")"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<add>(left->clone(), right->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<add>(left->substitute(v, r), right->substitute(v, r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<add>(left->expand(c), right->expand(c)); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const add*>(&other);
        return o && left->equals(*o->left) && right->equals(*o->right);
    }
};

struct multiply : expr {
    std::unique_ptr<expr> left, right;
    multiply(std::unique_ptr<expr> l, std::unique_ptr<expr> r) : left(std::move(l)), right(std::move(r)) {}
    double eval(context& ctx) const override { return left->eval(ctx) * right->eval(ctx); }
    std::string to_string() const override { 
        if (left->is_number() && std::abs(static_cast<number*>(left.get())->val + 1.0) < 1e-9) return "-" + right->to_string();
        return left->to_string() + "*" + right->to_string(); 
    }
    std::unique_ptr<expr> clone() const override { return std::make_unique<multiply>(left->clone(), right->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<multiply>(left->substitute(v, r), right->substitute(v, r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<multiply>(left->expand(c), right->expand(c)); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const multiply*>(&other);
        return o && left->equals(*o->left) && right->equals(*o->right);
    }
};

struct divide : expr {
    std::unique_ptr<expr> left, right;
    divide(std::unique_ptr<expr> l, std::unique_ptr<expr> r) : left(std::move(l)), right(std::move(r)) {}
    double eval(context& ctx) const override { return left->eval(ctx) / right->eval(ctx); }
    std::string to_string() const override { return "\\frac{" + left->to_string() + "}{" + right->to_string() + "}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<divide>(left->clone(), right->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<divide>(left->substitute(v, r), right->substitute(v, r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<divide>(left->expand(c), right->expand(c)); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const divide*>(&other);
        return o && left->equals(*o->left) && right->equals(*o->right);
    }
};

struct pow_node : expr {
    std::unique_ptr<expr> base, exponent;
    pow_node(std::unique_ptr<expr> b, std::unique_ptr<expr> e) : base(std::move(b)), exponent(std::move(e)) {}
    double eval(context& ctx) const override { return std::pow(base->eval(ctx), exponent->eval(ctx)); }
    std::string to_string() const override { return "{" + base->to_string() + "}^{" + exponent->to_string() + "}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<pow_node>(base->clone(), exponent->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<pow_node>(base->substitute(v, r), exponent->substitute(v, r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<pow_node>(base->expand(c), exponent->expand(c)); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const pow_node*>(&other);
        return o && base->equals(*o->base) && exponent->equals(*o->exponent);
    }
};

struct func_call : expr {
    std::string name;
    std::unique_ptr<expr> arg;
    func_call(std::string n, std::unique_ptr<expr> a) : name(n), arg(std::move(a)) {}
    double eval(context& ctx) const override {
        double val = arg->eval(ctx);
        if (ctx.builtins.count(name)) return ctx.builtins[name](val);
        return 0.0;
    }
    std::string to_string() const override { return "\\" + name + "{" + arg->to_string() + "}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<func_call>(name, arg->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<func_call>(name, arg->substitute(v, r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<func_call>(name, arg->expand(c)); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const func_call*>(&other);
        return o && name == o->name && arg->equals(*o->arg);
    }
};

struct deriv_node : expr {
    std::string var;
    std::unique_ptr<expr> arg;
    deriv_node(std::string v, std::unique_ptr<expr> a) : var(v), arg(std::move(a)) {}
    double eval(context&) const override { return 0.0; }
    std::string to_string() const override { return "\\frac{d}{d" + var + "}" + arg->to_string(); }
    std::unique_ptr<expr> clone() const override { return std::make_unique<deriv_node>(var, arg->clone()); }
    std::unique_ptr<expr> derivative(const std::string& v) const override { return std::make_unique<deriv_node>(v, clone()); }
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<deriv_node>(var, arg->substitute(v, r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<deriv_node>(var, arg->expand(c)); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const deriv_node*>(&other);
        return o && var == o->var && arg->equals(*o->arg);
    }
};

struct integral : expr {
    std::unique_ptr<expr> lower, upper, integrand;
    std::string var;
    integral(std::unique_ptr<expr> l, std::unique_ptr<expr> u, std::unique_ptr<expr> i, std::string v)
        : lower(std::move(l)), upper(std::move(u)), integrand(std::move(i)), var(v) {}
    double eval(context& ctx) const override { return 0.0; }
    std::string to_string() const override { return "\\int " + integrand->to_string() + " d" + var; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<integral>(nullptr, nullptr, integrand->clone(), var); }
    std::unique_ptr<expr> derivative(const std::string& d_var) const override { 
        if (d_var == var) return integrand->clone();
        return std::make_unique<number>(0); 
    }
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return v == var ? clone() : std::make_unique<integral>(nullptr, nullptr, integrand->substitute(v, r), var);
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<integral>(nullptr, nullptr, integrand->expand(c), var); }
    bool equals(const expr& other) const override {
        auto o = dynamic_cast<const integral*>(&other);
        return o && var == o->var && integrand->equals(*o->integrand);
    }
};

// --- implementations ---

inline std::unique_ptr<expr> add::derivative(const std::string& var) const {
    return std::make_unique<add>(left->derivative(var), right->derivative(var));
}

inline std::unique_ptr<expr> add::simplify() const {
    auto l = left->simplify(); auto r = right->simplify();
    if (l->is_zero()) return r; if (r->is_zero()) return l;
    if (l->is_number() && r->is_number()) return std::make_unique<number>(static_cast<number*>(l.get())->val + static_cast<number*>(r.get())->val);
    return std::make_unique<add>(std::move(l), std::move(r));
}

inline std::unique_ptr<expr> multiply::derivative(const std::string& var) const {
    return std::make_unique<add>(std::make_unique<multiply>(left->derivative(var), right->clone()), std::make_unique<multiply>(left->clone(), right->derivative(var)));
}

inline std::unique_ptr<expr> multiply::simplify() const {
    auto l = left->simplify(); auto r = right->simplify();
    if (l->is_zero() || r->is_zero()) return std::make_unique<number>(0);
    if (l->is_one()) return r; if (r->is_one()) return l;
    
    // push numbers to left
    if (r->is_number() && !l->is_number()) return std::make_unique<multiply>(std::move(r), std::move(l));
    
    // fold constants
    if (l->is_number()) {
        double val = static_cast<number*>(l.get())->val;
        if (r->is_number()) return std::make_unique<number>(val * static_cast<number*>(r.get())->val);
        // flatten nested multiplications
        if (auto rm = dynamic_cast<multiply*>(r.get())) {
            if (rm->left->is_number()) {
                double new_val = val * static_cast<number*>(rm->left.get())->val;
                return std::make_unique<multiply>(std::make_unique<number>(new_val), rm->right->simplify());
            }
        }
    }
    
    return std::make_unique<multiply>(std::move(l), std::move(r));
}

inline std::unique_ptr<expr> divide::derivative(const std::string& var) const {
    auto num = std::make_unique<add>(
        std::make_unique<multiply>(left->derivative(var), right->clone()),
        std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<multiply>(left->clone(), right->derivative(var)))
    );
    auto den = std::make_unique<multiply>(right->clone(), right->clone());
    return std::make_unique<divide>(std::move(num), std::move(den));
}

inline std::unique_ptr<expr> divide::simplify() const {
    auto l = left->simplify(); auto r = right->simplify();
    if (l->is_zero()) return std::make_unique<number>(0);
    if (r->is_one()) return l;
    if (l->is_number() && r->is_number()) return std::make_unique<number>(static_cast<number*>(l.get())->val / static_cast<number*>(r.get())->val);
    return std::make_unique<divide>(std::move(l), std::move(r));
}

inline std::unique_ptr<expr> pow_node::derivative(const std::string& var) const {
    auto term1 = std::make_unique<multiply>(exponent->clone(), std::make_unique<pow_node>(base->clone(), std::make_unique<add>(exponent->clone(), std::make_unique<number>(-1))));
    return std::make_unique<multiply>(std::move(term1), base->derivative(var));
}

inline std::unique_ptr<expr> pow_node::simplify() const {
    auto b = base->simplify(); auto e = exponent->simplify();
    if (e->is_zero()) return std::make_unique<number>(1);
    if (e->is_one()) return b;
    if (b->is_number() && e->is_number()) return std::make_unique<number>(std::pow(static_cast<number*>(b.get())->val, static_cast<number*>(e.get())->val));
    return std::make_unique<pow_node>(std::move(b), std::move(e));
}

inline std::unique_ptr<expr> func_call::derivative(const std::string& var) const {
    auto chain = arg->derivative(var);
    std::unique_ptr<expr> outer;

    // trig
    if (name == "sin")
        outer = std::make_unique<func_call>("cos", arg->clone());
    else if (name == "cos")
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<func_call>("sin", arg->clone()));
    else if (name == "tan")
        // sec²(x)
        outer = std::make_unique<pow_node>(std::make_unique<func_call>("sec", arg->clone()), std::make_unique<number>(2.0));
    else if (name == "cot")
        // -csc²(x)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<pow_node>(std::make_unique<func_call>("csc", arg->clone()), std::make_unique<number>(2.0)));
    else if (name == "sec")
        // sec(x)*tan(x)
        outer = std::make_unique<multiply>(std::make_unique<func_call>("sec", arg->clone()), std::make_unique<func_call>("tan", arg->clone()));
    else if (name == "csc")
        // -csc(x)*cot(x)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<multiply>(std::make_unique<func_call>("csc", arg->clone()), std::make_unique<func_call>("cot", arg->clone())));

    // inverse trig
    else if (name == "arcsin" || name == "asin")
        // 1/sqrt(1-x^2)
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)))), std::make_unique<number>(0.5)));
    else if (name == "arccos" || name == "acos")
        // -1/sqrt(1-x^2)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)))), std::make_unique<number>(0.5))));
    else if (name == "arctan" || name == "atan")
        // 1/(1+x^2)
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0))));
    else if (name == "arccot" || name == "acot")
        // -1/(1+x^2)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)))));
    else if (name == "arcsec" || name == "asec")
        // 1/(|x|*sqrt(x^2-1))
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<func_call>("abs", arg->clone()), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)), std::make_unique<number>(-1.0)), std::make_unique<number>(0.5))));
    else if (name == "arccsc" || name == "acsc")
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<func_call>("abs", arg->clone()), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)), std::make_unique<number>(-1.0)), std::make_unique<number>(0.5)))));

    // exponential / log
    else if (name == "exp")
        outer = std::make_unique<func_call>("exp", arg->clone());
    else if (name == "log" || name == "ln")
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), arg->clone());
    else if (name == "log2")
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(arg->clone(), std::make_unique<func_call>("log", std::make_unique<number>(2.0))));
    else if (name == "log10")
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(arg->clone(), std::make_unique<func_call>("log", std::make_unique<number>(10.0))));

    // square root
    else if (name == "sqrt")
        // 1/(2*sqrt(x))
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<number>(2.0), std::make_unique<func_call>("sqrt", arg->clone())));

    // hyperbolic
    else if (name == "sinh")
        outer = std::make_unique<func_call>("cosh", arg->clone());
    else if (name == "cosh")
        outer = std::make_unique<func_call>("sinh", arg->clone());
    else if (name == "tanh")
        // sech²(x) = 1 - tanh²(x), represented as 1/(cosh²(x))
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<pow_node>(std::make_unique<func_call>("cosh", arg->clone()), std::make_unique<number>(2.0)));
    else if (name == "coth")
        // -csch²(x)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<pow_node>(std::make_unique<func_call>("sinh", arg->clone()), std::make_unique<number>(2.0))));
    else if (name == "sech")
        // -sech(x)*tanh(x)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<multiply>(std::make_unique<func_call>("sech", arg->clone()), std::make_unique<func_call>("tanh", arg->clone())));
    else if (name == "csch")
        // -csch(x)*coth(x)
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<multiply>(std::make_unique<func_call>("csch", arg->clone()), std::make_unique<func_call>("coth", arg->clone())));

    // inverse hyperbolic
    else if (name == "arcsinh" || name == "asinh")
        // 1/sqrt(x^2+1)
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)), std::make_unique<number>(1.0)), std::make_unique<number>(0.5)));
    else if (name == "arccosh" || name == "acosh")
        // 1/sqrt(x^2-1)
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)), std::make_unique<number>(-1.0)), std::make_unique<number>(0.5)));
    else if (name == "arctanh" || name == "atanh")
        // 1/(1-x^2)
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)))));
    else if (name == "arccoth" || name == "acoth")
        // 1/(1-x^2)  same form, different domain
        outer = std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)))));
    else if (name == "arcsech" || name == "asech")
        // -1/(x*sqrt(1-x^2))
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(arg->clone(), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)))), std::make_unique<number>(0.5)))));
    else if (name == "arccsch" || name == "acsch")
        // -1/(|x|*sqrt(x^2+1))
        outer = std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<divide>(std::make_unique<number>(1.0), std::make_unique<multiply>(std::make_unique<func_call>("abs", arg->clone()), std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<pow_node>(arg->clone(), std::make_unique<number>(2.0)), std::make_unique<number>(1.0)), std::make_unique<number>(0.5)))));

    else
        outer = std::make_unique<number>(0.0);

    return std::make_unique<multiply>(std::move(outer), std::move(chain));
}

inline std::unique_ptr<expr> func_call::simplify() const {
    auto s_arg = arg->simplify();
    if (s_arg->is_number()) {
        double v = static_cast<number*>(s_arg.get())->val;
        if (name == "sin" && std::abs(v) < 1e-9) return std::make_unique<number>(0);
        if (name == "cos" && std::abs(v) < 1e-9) return std::make_unique<number>(1);
    }
    return std::make_unique<func_call>(name, std::move(s_arg));
}

inline std::unique_ptr<expr> deriv_node::simplify() const {
    return arg->derivative(var)->simplify();
}

inline std::unique_ptr<expr> integral::simplify() const {
    auto si = integrand->simplify();
    auto result = symbolic::integrate(*si, var, 0);
    if (result) return result->simplify();
    return std::make_unique<integral>(nullptr, nullptr, std::move(si), var);
}