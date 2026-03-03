#pragma once
#include <string>
#include <map>
#include <memory>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>
#include <limits>

struct context;
struct expr;

namespace symbolic {
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
    virtual bool is_one()  const { return false; }
    virtual bool is_number() const { return false; }
};

// core types

struct number : expr {
    double val;
    number(double v) : val(v) {}
    double eval(context&) const override { return val; }
    std::string to_string() const override {
        if (std::abs(val + 1.0) < 1e-9) return "-1";
        std::string s = std::to_string(val);
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s;
    }
    std::unique_ptr<expr> clone() const override { return std::make_unique<number>(val); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override { return clone(); }
    std::unique_ptr<expr> substitute(const std::string&, const expr&) const override { return clone(); }
    std::unique_ptr<expr> expand(const context&) const override { return clone(); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const number*>(&o); return p&&std::abs(val-p->val)<1e-7; }
    bool is_zero() const override { return std::abs(val) < 1e-9; }
    bool is_one()  const override { return std::abs(val - 1.0) < 1e-9; }
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
    bool equals(const expr& o) const override { auto p=dynamic_cast<const variable*>(&o); return p&&name==p->name; }
};

struct add : expr {
    std::unique_ptr<expr> left, right;
    add(std::unique_ptr<expr> l, std::unique_ptr<expr> r) : left(std::move(l)), right(std::move(r)) {}
    double eval(context& ctx) const override { return left->eval(ctx) + right->eval(ctx); }
    std::string to_string() const override { return "("+left->to_string()+"+"+right->to_string()+")"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<add>(left->clone(), right->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<add>(left->substitute(v,r), right->substitute(v,r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<add>(left->expand(c), right->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const add*>(&o); return p&&left->equals(*p->left)&&right->equals(*p->right); }
};

struct multiply : expr {
    std::unique_ptr<expr> left, right;
    multiply(std::unique_ptr<expr> l, std::unique_ptr<expr> r) : left(std::move(l)), right(std::move(r)) {}
    double eval(context& ctx) const override { return left->eval(ctx) * right->eval(ctx); }
    std::string to_string() const override {
        if (left->is_number() && std::abs(static_cast<number*>(left.get())->val + 1.0) < 1e-9)
            return "-" + right->to_string();
        return left->to_string() + "*" + right->to_string();
    }
    std::unique_ptr<expr> clone() const override { return std::make_unique<multiply>(left->clone(), right->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<multiply>(left->substitute(v,r), right->substitute(v,r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<multiply>(left->expand(c), right->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const multiply*>(&o); return p&&left->equals(*p->left)&&right->equals(*p->right); }
};

struct divide : expr {
    std::unique_ptr<expr> left, right;
    divide(std::unique_ptr<expr> l, std::unique_ptr<expr> r) : left(std::move(l)), right(std::move(r)) {}
    double eval(context& ctx) const override { return left->eval(ctx) / right->eval(ctx); }
    std::string to_string() const override { return "\\frac{"+left->to_string()+"}{"+right->to_string()+"}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<divide>(left->clone(), right->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<divide>(left->substitute(v,r), right->substitute(v,r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<divide>(left->expand(c), right->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const divide*>(&o); return p&&left->equals(*p->left)&&right->equals(*p->right); }
};

struct pow_node : expr {
    std::unique_ptr<expr> base, exponent;
    pow_node(std::unique_ptr<expr> b, std::unique_ptr<expr> e) : base(std::move(b)), exponent(std::move(e)) {}
    double eval(context& ctx) const override { return std::pow(base->eval(ctx), exponent->eval(ctx)); }
    std::string to_string() const override { return "{"+base->to_string()+"}^{"+exponent->to_string()+"}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<pow_node>(base->clone(), exponent->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return std::make_unique<pow_node>(base->substitute(v,r), exponent->substitute(v,r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<pow_node>(base->expand(c), exponent->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const pow_node*>(&o); return p&&base->equals(*p->base)&&exponent->equals(*p->exponent); }
};

struct func_call : expr {
    std::string name;
    std::unique_ptr<expr> arg;
    func_call(std::string n, std::unique_ptr<expr> a) : name(n), arg(std::move(a)) {}
    double eval(context& ctx) const override {
        double v = arg->eval(ctx);
        if (ctx.builtins.count(name)) return ctx.builtins[name](v);
        return 0.0;
    }
    std::string to_string() const override { return "\\"+name+"{"+arg->to_string()+"}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<func_call>(name, arg->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override;
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<func_call>(name, arg->substitute(v,r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<func_call>(name, arg->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const func_call*>(&o); return p&&name==p->name&&arg->equals(*p->arg); }
};

struct deriv_node : expr {
    std::string var;
    std::unique_ptr<expr> arg;
    deriv_node(std::string v, std::unique_ptr<expr> a) : var(v), arg(std::move(a)) {}
    double eval(context&) const override { return 0.0; }
    std::string to_string() const override { return "\\frac{d}{d"+var+"}"+arg->to_string(); }
    std::unique_ptr<expr> clone() const override { return std::make_unique<deriv_node>(var, arg->clone()); }
    std::unique_ptr<expr> derivative(const std::string& v) const override { return std::make_unique<deriv_node>(v, clone()); }
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<deriv_node>(var, arg->substitute(v,r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<deriv_node>(var, arg->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const deriv_node*>(&o); return p&&var==p->var&&arg->equals(*p->arg); }
};

struct integral : expr {
    std::unique_ptr<expr> lower, upper, integrand;
    std::string var;
    integral(std::unique_ptr<expr> l, std::unique_ptr<expr> u, std::unique_ptr<expr> i, std::string v)
        : lower(std::move(l)), upper(std::move(u)), integrand(std::move(i)), var(v) {}
    double eval(context&) const override { return 0.0; }
    std::string to_string() const override { return "\\int "+integrand->to_string()+" d"+var; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<integral>(nullptr, nullptr, integrand->clone(), var); }
    std::unique_ptr<expr> derivative(const std::string& d) const override {
        if (d == var) return integrand->clone();
        return std::make_unique<number>(0);
    }
    std::unique_ptr<expr> simplify() const override;
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        return v==var ? clone() : std::make_unique<integral>(nullptr, nullptr, integrand->substitute(v,r), var);
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<integral>(nullptr, nullptr, integrand->expand(c), var); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const integral*>(&o); return p&&var==p->var&&integrand->equals(*p->integrand); }
};

// extended nodes

static inline double fact_val(int n) {
    if (n < 0) return std::numeric_limits<double>::quiet_NaN();
    if (n > 170) return std::numeric_limits<double>::infinity();
    double r = 1;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}

struct factorial_node : expr {
    std::unique_ptr<expr> arg;
    factorial_node(std::unique_ptr<expr> a) : arg(std::move(a)) {}
    double eval(context& ctx) const override { return fact_val(static_cast<int>(std::round(arg->eval(ctx)))); }
    std::string to_string() const override { return arg->to_string() + "!"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<factorial_node>(arg->clone()); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override {
        auto s = arg->simplify();
        if (s->is_number()) {
            int n = static_cast<int>(std::round(static_cast<number*>(s.get())->val));
            if (n >= 0 && n <= 170) return std::make_unique<number>(fact_val(n));
        }
        return std::make_unique<factorial_node>(std::move(s));
    }
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<factorial_node>(arg->substitute(v,r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<factorial_node>(arg->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const factorial_node*>(&o); return p&&arg->equals(*p->arg); }
};

struct combination_node : expr {
    std::unique_ptr<expr> n_expr, r_expr;
    combination_node(std::unique_ptr<expr> n, std::unique_ptr<expr> r) : n_expr(std::move(n)), r_expr(std::move(r)) {}
    double eval(context& ctx) const override {
        int n = static_cast<int>(std::round(n_expr->eval(ctx)));
        int r = static_cast<int>(std::round(r_expr->eval(ctx)));
        if (r < 0 || r > n) return 0;
        double res = 1;
        for (int i = 0; i < r; ++i) res = res * (n - i) / (i + 1);
        return std::round(res);
    }
    std::string to_string() const override { return "\\dbinom{"+n_expr->to_string()+"}{"+r_expr->to_string()+"}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<combination_node>(n_expr->clone(), r_expr->clone()); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override {
        auto sn = n_expr->simplify(), sr = r_expr->simplify();
        if (sn->is_number() && sr->is_number()) {
            context c;
            return std::make_unique<number>(combination_node(sn->clone(), sr->clone()).eval(c));
        }
        return std::make_unique<combination_node>(std::move(sn), std::move(sr));
    }
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<combination_node>(n_expr->substitute(v,r), r_expr->substitute(v,r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<combination_node>(n_expr->expand(c), r_expr->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const combination_node*>(&o); return p&&n_expr->equals(*p->n_expr)&&r_expr->equals(*p->r_expr); }
};

struct permutation_node : expr {
    std::unique_ptr<expr> n_expr, r_expr;
    permutation_node(std::unique_ptr<expr> n, std::unique_ptr<expr> r) : n_expr(std::move(n)), r_expr(std::move(r)) {}
    double eval(context& ctx) const override {
        int n = static_cast<int>(std::round(n_expr->eval(ctx)));
        int r = static_cast<int>(std::round(r_expr->eval(ctx)));
        if (r < 0 || r > n) return 0;
        double res = 1;
        for (int i = 0; i < r; ++i) res *= (n - i);
        return res;
    }
    std::string to_string() const override { return "{}^{"+n_expr->to_string()+"}P_{"+r_expr->to_string()+"}"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<permutation_node>(n_expr->clone(), r_expr->clone()); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override {
        auto sn = n_expr->simplify(), sr = r_expr->simplify();
        if (sn->is_number() && sr->is_number()) {
            context c;
            return std::make_unique<number>(permutation_node(sn->clone(), sr->clone()).eval(c));
        }
        return std::make_unique<permutation_node>(std::move(sn), std::move(sr));
    }
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<permutation_node>(n_expr->substitute(v,r), r_expr->substitute(v,r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<permutation_node>(n_expr->expand(c), r_expr->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const permutation_node*>(&o); return p&&n_expr->equals(*p->n_expr)&&r_expr->equals(*p->r_expr); }
};

struct sum_node : expr {
    std::string index_var;
    std::unique_ptr<expr> from, to_expr, body;
    sum_node(std::string iv, std::unique_ptr<expr> f, std::unique_ptr<expr> t, std::unique_ptr<expr> b)
        : index_var(iv), from(std::move(f)), to_expr(std::move(t)), body(std::move(b)) {}
    double eval(context& ctx) const override {
        int fr = static_cast<int>(std::round(from->eval(ctx)));
        int to = static_cast<int>(std::round(to_expr->eval(ctx)));
        double saved = ctx.vars.count(index_var) ? ctx.vars.at(index_var) : 0.0;
        double sum = 0;
        for (int i = fr; i <= to; ++i) { ctx.vars[index_var] = i; sum += body->eval(ctx); }
        ctx.vars[index_var] = saved;
        return sum;
    }
    std::string to_string() const override { return "\\sum_{"+index_var+"="+from->to_string()+"}^{"+to_expr->to_string()+"} "+body->to_string(); }
    std::unique_ptr<expr> clone() const override { return std::make_unique<sum_node>(index_var, from->clone(), to_expr->clone(), body->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override {
        if (var == index_var) return std::make_unique<number>(0);
        return std::make_unique<sum_node>(index_var, from->clone(), to_expr->clone(), body->derivative(var));
    }
    std::unique_ptr<expr> simplify() const override { return std::make_unique<sum_node>(index_var, from->simplify(), to_expr->simplify(), body->simplify()); }
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        if (v == index_var) return clone();
        return std::make_unique<sum_node>(index_var, from->substitute(v,r), to_expr->substitute(v,r), body->substitute(v,r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<sum_node>(index_var, from->expand(c), to_expr->expand(c), body->expand(c)); }
    bool equals(const expr& o) const override {
        auto p = dynamic_cast<const sum_node*>(&o);
        return p&&index_var==p->index_var&&from->equals(*p->from)&&to_expr->equals(*p->to_expr)&&body->equals(*p->body);
    }
};

struct product_node : expr {
    std::string index_var;
    std::unique_ptr<expr> from, to_expr, body;
    product_node(std::string iv, std::unique_ptr<expr> f, std::unique_ptr<expr> t, std::unique_ptr<expr> b)
        : index_var(iv), from(std::move(f)), to_expr(std::move(t)), body(std::move(b)) {}
    double eval(context& ctx) const override {
        int fr = static_cast<int>(std::round(from->eval(ctx)));
        int to = static_cast<int>(std::round(to_expr->eval(ctx)));
        double saved = ctx.vars.count(index_var) ? ctx.vars.at(index_var) : 1.0;
        double prod = 1;
        for (int i = fr; i <= to; ++i) { ctx.vars[index_var] = i; prod *= body->eval(ctx); }
        ctx.vars[index_var] = saved;
        return prod;
    }
    std::string to_string() const override { return "\\prod_{"+index_var+"="+from->to_string()+"}^{"+to_expr->to_string()+"} "+body->to_string(); }
    std::unique_ptr<expr> clone() const override { return std::make_unique<product_node>(index_var, from->clone(), to_expr->clone(), body->clone()); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override { return std::make_unique<product_node>(index_var, from->simplify(), to_expr->simplify(), body->simplify()); }
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override {
        if (v == index_var) return clone();
        return std::make_unique<product_node>(index_var, from->substitute(v,r), to_expr->substitute(v,r), body->substitute(v,r));
    }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<product_node>(index_var, from->expand(c), to_expr->expand(c), body->expand(c)); }
    bool equals(const expr& o) const override {
        auto p = dynamic_cast<const product_node*>(&o);
        return p&&index_var==p->index_var&&from->equals(*p->from)&&to_expr->equals(*p->to_expr)&&body->equals(*p->body);
    }
};

struct abs_node : expr {
    std::unique_ptr<expr> arg;
    abs_node(std::unique_ptr<expr> a) : arg(std::move(a)) {}
    double eval(context& ctx) const override { return std::abs(arg->eval(ctx)); }
    std::string to_string() const override { return "\\left|"+arg->to_string()+"\\right|"; }
    std::unique_ptr<expr> clone() const override { return std::make_unique<abs_node>(arg->clone()); }
    std::unique_ptr<expr> derivative(const std::string& var) const override {
        return std::make_unique<multiply>(
            std::make_unique<divide>(arg->clone(), std::make_unique<abs_node>(arg->clone())),
            arg->derivative(var));
    }
    std::unique_ptr<expr> simplify() const override {
        auto s = arg->simplify();
        if (s->is_number()) return std::make_unique<number>(std::abs(static_cast<number*>(s.get())->val));
        return std::make_unique<abs_node>(std::move(s));
    }
    std::unique_ptr<expr> substitute(const std::string& v, const expr& r) const override { return std::make_unique<abs_node>(arg->substitute(v,r)); }
    std::unique_ptr<expr> expand(const context& c) const override { return std::make_unique<abs_node>(arg->expand(c)); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const abs_node*>(&o); return p&&arg->equals(*p->arg); }
};

struct named_constant : expr {
    std::string name;
    double value;
    named_constant(const std::string& n, double v) : name(n), value(v) {}
    double eval(context&) const override { return value; }
    std::string to_string() const override {
        static const std::map<std::string,std::string> L = {{"pi","\\pi"},{"e","e"},{"phi","\\varphi"},{"tau","\\tau"},{"inf","\\infty"}};
        auto it = L.find(name); return it != L.end() ? it->second : name;
    }
    std::unique_ptr<expr> clone() const override { return std::make_unique<named_constant>(name, value); }
    std::unique_ptr<expr> derivative(const std::string&) const override { return std::make_unique<number>(0); }
    std::unique_ptr<expr> simplify() const override { return std::make_unique<number>(value); }
    std::unique_ptr<expr> substitute(const std::string&, const expr&) const override { return clone(); }
    std::unique_ptr<expr> expand(const context&) const override { return clone(); }
    bool equals(const expr& o) const override { auto p=dynamic_cast<const named_constant*>(&o); return p&&name==p->name; }
    bool is_number() const override { return true; }
};

// inline implementations

inline std::unique_ptr<expr> add::derivative(const std::string& var) const {
    return std::make_unique<add>(left->derivative(var), right->derivative(var));
}
inline std::unique_ptr<expr> add::simplify() const {
    auto l=left->simplify(), r=right->simplify();
    if (l->is_zero()) return r;
    if (r->is_zero()) return l;
    if (l->is_number() && r->is_number()) return std::make_unique<number>(static_cast<number*>(l.get())->val + static_cast<number*>(r.get())->val);
    return std::make_unique<add>(std::move(l), std::move(r));
}

inline std::unique_ptr<expr> multiply::derivative(const std::string& var) const {
    return std::make_unique<add>(
        std::make_unique<multiply>(left->derivative(var), right->clone()),
        std::make_unique<multiply>(left->clone(), right->derivative(var)));
}
inline std::unique_ptr<expr> multiply::simplify() const {
    auto l=left->simplify(), r=right->simplify();
    if (l->is_zero() || r->is_zero()) return std::make_unique<number>(0);
    if (l->is_one()) return r;
    if (r->is_one()) return l;
    if (r->is_number() && !l->is_number()) return std::make_unique<multiply>(std::move(r), std::move(l));
    if (l->is_number()) {
        double val = static_cast<number*>(l.get())->val;
        if (r->is_number()) return std::make_unique<number>(val * static_cast<number*>(r.get())->val);
        if (auto rm = dynamic_cast<multiply*>(r.get())) {
            if (rm->left->is_number()) {
                double nv = val * static_cast<number*>(rm->left.get())->val;
                return std::make_unique<multiply>(std::make_unique<number>(nv), rm->right->simplify());
            }
        }
    }
    return std::make_unique<multiply>(std::move(l), std::move(r));
}

inline std::unique_ptr<expr> divide::derivative(const std::string& var) const {
    auto num = std::make_unique<add>(
        std::make_unique<multiply>(left->derivative(var), right->clone()),
        std::make_unique<multiply>(std::make_unique<number>(-1.0), std::make_unique<multiply>(left->clone(), right->derivative(var))));
    auto den = std::make_unique<multiply>(right->clone(), right->clone());
    return std::make_unique<divide>(std::move(num), std::move(den));
}
inline std::unique_ptr<expr> divide::simplify() const {
    auto l=left->simplify(), r=right->simplify();
    if (l->is_zero()) return std::make_unique<number>(0);
    if (r->is_one()) return l;
    if (l->is_number() && r->is_number()) return std::make_unique<number>(static_cast<number*>(l.get())->val / static_cast<number*>(r.get())->val);
    return std::make_unique<divide>(std::move(l), std::move(r));
}

inline std::unique_ptr<expr> pow_node::derivative(const std::string& var) const {
    if (exponent->is_number()) {
        double n = static_cast<number*>(exponent.get())->val;
        if (n == 0.0) return std::make_unique<number>(0.0);
        
        auto power_down = std::make_unique<multiply>(
            std::make_unique<number>(n),
            std::make_unique<pow_node>(base->clone(), std::make_unique<number>(n - 1.0))
        );
        return std::make_unique<multiply>(std::move(power_down), base->derivative(var));
    }
    
    auto term1 = std::make_unique<multiply>(
        exponent->derivative(var),
        std::make_unique<func_call>("ln", base->clone())
    );
    auto term2 = std::make_unique<multiply>(
        exponent->clone(),
        std::make_unique<divide>(base->derivative(var), base->clone())
    );
    auto inner = std::make_unique<add>(std::move(term1), std::move(term2));
    return std::make_unique<multiply>(clone(), std::move(inner));
}
inline std::unique_ptr<expr> pow_node::simplify() const {
    auto b=base->simplify(), e=exponent->simplify();
    if (e->is_zero()) return std::make_unique<number>(1);
    if (e->is_one()) return b;
    if (b->is_number() && e->is_number()) return std::make_unique<number>(std::pow(static_cast<number*>(b.get())->val, static_cast<number*>(e.get())->val));
    return std::make_unique<pow_node>(std::move(b), std::move(e));
}

inline std::unique_ptr<expr> func_call::derivative(const std::string& var) const {
    auto chain = arg->derivative(var);
    std::unique_ptr<expr> outer;
    if      (name=="sin")  outer=std::make_unique<func_call>("cos",arg->clone());
    else if (name=="cos")  outer=std::make_unique<multiply>(std::make_unique<number>(-1.0),std::make_unique<func_call>("sin",arg->clone()));
    else if (name=="tan")  outer=std::make_unique<pow_node>(std::make_unique<func_call>("sec",arg->clone()),std::make_unique<number>(2.0));
    else if (name=="cot")  outer=std::make_unique<multiply>(std::make_unique<number>(-1.0),std::make_unique<pow_node>(std::make_unique<func_call>("csc",arg->clone()),std::make_unique<number>(2.0)));
    else if (name=="sec")  outer=std::make_unique<multiply>(std::make_unique<func_call>("sec",arg->clone()),std::make_unique<func_call>("tan",arg->clone()));
    else if (name=="csc")  outer=std::make_unique<multiply>(std::make_unique<number>(-1.0),std::make_unique<multiply>(std::make_unique<func_call>("csc",arg->clone()),std::make_unique<func_call>("cot",arg->clone())));
    else if (name=="arcsin"||name=="asin") outer=std::make_unique<divide>(std::make_unique<number>(1.0),std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<number>(1.0),std::make_unique<multiply>(std::make_unique<number>(-1.0),std::make_unique<pow_node>(arg->clone(),std::make_unique<number>(2.0)))),std::make_unique<number>(0.5)));
    else if (name=="arccos"||name=="acos") outer=std::make_unique<multiply>(std::make_unique<number>(-1.0),std::make_unique<divide>(std::make_unique<number>(1.0),std::make_unique<pow_node>(std::make_unique<add>(std::make_unique<number>(1.0),std::make_unique<multiply>(std::make_unique<number>(-1.0),std::make_unique<pow_node>(arg->clone(),std::make_unique<number>(2.0)))),std::make_unique<number>(0.5))));
    else if (name=="arctan"||name=="atan") outer=std::make_unique<divide>(std::make_unique<number>(1.0),std::make_unique<add>(std::make_unique<number>(1.0),std::make_unique<pow_node>(arg->clone(),std::make_unique<number>(2.0))));
    else if (name=="exp")  outer=std::make_unique<func_call>("exp",arg->clone());
    else if (name=="log"||name=="ln") outer=std::make_unique<divide>(std::make_unique<number>(1.0),arg->clone());
    else if (name=="sqrt") outer=std::make_unique<divide>(std::make_unique<number>(1.0),std::make_unique<multiply>(std::make_unique<number>(2.0),std::make_unique<func_call>("sqrt",arg->clone())));
    else if (name=="sinh") outer=std::make_unique<func_call>("cosh",arg->clone());
    else if (name=="cosh") outer=std::make_unique<func_call>("sinh",arg->clone());
    else if (name=="tanh") outer=std::make_unique<divide>(std::make_unique<number>(1.0),std::make_unique<pow_node>(std::make_unique<func_call>("cosh",arg->clone()),std::make_unique<number>(2.0)));
    else outer=std::make_unique<number>(0.0);
    return std::make_unique<multiply>(std::move(outer), std::move(chain));
}
inline std::unique_ptr<expr> func_call::simplify() const {
    auto s = arg->simplify();
    if (s->is_number()) {
        double v = static_cast<number*>(s.get())->val;
        if (name=="sin" && std::abs(v)<1e-9) return std::make_unique<number>(0);
        if (name=="cos" && std::abs(v)<1e-9) return std::make_unique<number>(1);
    }
    return std::make_unique<func_call>(name, std::move(s));
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