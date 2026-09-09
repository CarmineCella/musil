// core.h — the Musil language: reader, evaluator, core builtins.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Surface (f8-style hybrid):
//   - Top level: each line is a command (auto-listified). No enclosing parens.
//   - (form ...) is a Lisp list. Newlines inside parens are whitespace.
//   - {form ...} is a (do form ...) block of newline-separated sub-lists.
//   - "..." string. 'x is (quote x). # comment. \ at line end continues.
//
// Numbers are valarray<double>; size-1 vectors broadcast.
// Values are shared_ptr — sharing by default; copy is explicit.
// Special forms dispatch by function-pointer comparison.
// User functions curry when called with too few args; with too many, the
// result is applied to the remaining args.
//
// The core has no dependencies beyond the C++17 standard library.

#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <valarray>
#include <vector>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#define MUSIL_VERSION "3.0.0"

namespace musil {
namespace fs = std::filesystem;

struct value; struct env; struct interp;
using vptr = std::shared_ptr<value>;
using eptr = std::shared_ptr<env>;
using vlist = std::vector<vptr>;
using varr = std::valarray<double>;
using sptr = std::shared_ptr<std::string>;
using op_t = vptr (*)(vlist&, interp&);

// === Values =====
struct value {
    enum tag { NIL, NUM, STR, SYM, LIST, FN, OPAQUE } t = NIL;
    varr num; std::string s; vlist l;
    int line = 0; sptr file;
    op_t op = nullptr;
    std::vector<std::string> params;
    vptr body; eptr closure;
    std::shared_ptr<void> opaque; std::string opaque_tag;
};
inline const char* type_name(const vptr& v) {
    static const char* names[] = { "nil", "number", "string", "symbol", "list", "function", "opaque" };
    return v ? names[v->t] : "nil";
}
inline vptr v_nil()              { return std::make_shared<value>(); }
inline vptr v_num(double d)      { auto v = v_nil(); v->t = value::NUM; v->num = varr(d, 1); return v; }
inline vptr v_arr(varr a)        { auto v = v_nil(); v->t = value::NUM; v->num = std::move(a); return v; }
inline vptr v_bool(bool b)       { return v_num(b ? 1.0 : 0.0); }
inline vptr v_str(std::string s) { auto v = v_nil(); v->t = value::STR; v->s = std::move(s); return v; }
inline vptr v_sym(std::string s) { auto v = v_nil(); v->t = value::SYM; v->s = std::move(s); return v; }
inline vptr v_list(vlist x, int ln=0, sptr f={}) { auto v=v_nil(); v->t=value::LIST; v->l=std::move(x); v->line=ln; v->file=std::move(f); return v; }
inline vptr v_op(op_t f) { auto v = v_nil(); v->t = value::FN; v->op = f; return v; }
inline vptr v_opaque(std::string tag, std::shared_ptr<void> p) { auto v = v_nil(); v->t = value::OPAQUE; v->opaque_tag = std::move(tag); v->opaque = std::move(p); return v; }

// === Environments =====
struct env {
    std::unordered_map<std::string, vptr> vars;
    eptr parent;
    env(eptr p = nullptr) : parent(std::move(p)) {}
    vptr find(const std::string& k) {
        for (env* w = this; w; w = w->parent.get()) {
            auto it = w->vars.find(k);
            if (it != w->vars.end()) return it->second;
        }
        return nullptr;
    }
    bool assign(const std::string& k, vptr v) {
        for (env* w = this; w; w = w->parent.get()) {
            auto it = w->vars.find(k);
            if (it != w->vars.end()) { it->second = std::move(v); return true; }
        }
        return false;
    }
};

// === Errors and control signals =====
struct musil_error : std::exception {
    std::string file; int line = 0; std::string msg;
    std::vector<std::string> trace;
    mutable std::string cached;
    musil_error(std::string f, int ln, std::string m) : file(std::move(f)), line(ln), msg(std::move(m)) {}
    const char* what() const noexcept override {
        if (cached.empty()) {
            cached = file + ":" + std::to_string(line) + ": " + msg;
            for (size_t i=0; i<trace.size(); ++i) cached += "\n  " + std::to_string(i+1) + "> " + trace[i];
        }
        return cached.c_str();
    }
};
struct return_signal { vptr val; }; struct break_signal {}; struct continue_signal {};
struct exit_signal { int code; };

// === Predicates and printing =====
inline bool truthy(const vptr& v) {
    if (!v || v->t == value::NIL) return false;
    if (v->t == value::NUM) { for (size_t i=0; i<v->num.size(); i++) if (v->num[i]==0.0) return false; return v->num.size() > 0; }
    if (v->t == value::STR) return !v->s.empty();
    if (v->t == value::LIST) return !v->l.empty();
    return true;
}
inline void put_double(std::ostream& os, double d) {
    if (std::isfinite(d) && std::fabs(d) < 1e15 && d == std::floor(d)) os << (long long)d;
    else os << std::setprecision(15) << d;
}
inline std::string str_of(const vptr& v) {
    if (!v) return "nil";
    std::ostringstream os;
    switch (v->t) {
    case value::NIL: return "nil";
    case value::NUM:
        if (v->num.size() == 1) { put_double(os, v->num[0]); return os.str(); }
        os << "(";
        for (size_t i=0; i<v->num.size(); i++) { if (i) os << " "; put_double(os, v->num[i]); }
        os << ")";
        return os.str();
    case value::STR: case value::SYM: return v->s;
    case value::FN: return v->op ? "<builtin>" : "<fn>";
    case value::LIST:
        os << "("; for (size_t i=0; i<v->l.size(); i++) { if (i) os << " "; os << str_of(v->l[i]); } os << ")";
        return os.str();
    case value::OPAQUE: return "<opaque:" + v->opaque_tag + ">";
    }
    return "";
}
// Structural equality (numbers elementwise, strings/symbols by text, lists recursively).
inline bool equal(const vptr& a, const vptr& b) {
    if (!a || !b) return (!a || a->t == value::NIL) && (!b || b->t == value::NIL);
    if (a->t != b->t) return false;
    switch (a->t) {
    case value::NIL: return true;
    case value::NUM: if (a->num.size() != b->num.size()) return false;
        for (size_t i=0; i<a->num.size(); i++) { if (a->num[i] != b->num[i]) return false; }
        return true;
    case value::STR: case value::SYM: return a->s == b->s;
    case value::LIST: if (a->l.size() != b->l.size()) return false;
        for (size_t i=0; i<a->l.size(); i++) { if (!equal(a->l[i], b->l[i])) return false; }
        return true;
    case value::FN: return a.get() == b.get() || (a->op && a->op == b->op);
    case value::OPAQUE: return a->opaque.get() == b->opaque.get();
    }
    return false;
}

// === Reader =====
struct parser {
    std::string src; size_t pos = 0; int line = 1; sptr file;
    parser(std::string s, sptr f) : src(std::move(s)), file(std::move(f)) {}
    bool eof() const { return pos >= src.size(); }
    char peek() const { return src[pos]; }
    [[noreturn]] void err(int ln, const std::string& m) { throw musil_error(file?*file:"<input>", ln, m); }
    void skip_h() {
        while (!eof()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') { pos++; continue; }
            if (c == '\\' && pos+1 < src.size() && src[pos+1] == '\n') { pos += 2; line++; continue; }
            if (c == '#') { while (!eof() && peek() != '\n') pos++; continue; }
            break;
        }
    }
    void skip_all() { while (true) { skip_h(); if (eof() || peek() != '\n') break; pos++; line++; } }
    vptr read() {
        skip_h();
        if (eof()) err(line, "unexpected end of input");
        char c = peek(); int ln = line;
        if (c == '(') {
            pos++; vlist out; skip_all();
            while (!eof() && peek() != ')') { out.push_back(read()); skip_all(); }
            if (eof()) err(ln, "missing )");
            pos++;
            return v_list(std::move(out), ln, file);
        }
        if (c == '{') {
            pos++; vlist out = { v_sym("do") };
            while (true) {
                skip_h();
                if (eof()) err(ln, "missing }");
                if (peek() == '}') { pos++; break; }
                if (peek() == '\n') { pos++; line++; continue; }
                vptr lf = read_line('}');
                if (lf->t != value::LIST || !lf->l.empty()) out.push_back(lf);
            }
            return v_list(std::move(out), ln, file);
        }
        if (c == ')' || c == '}') err(ln, std::string("unexpected ") + c);
        if (c == '"') {
            pos++; std::string str;
            while (!eof() && peek() != '"') {
                if (peek() == '\\' && pos+1 < src.size()) {
                    char e = src[pos+1]; str += (e=='n'?'\n':e=='t'?'\t':e=='r'?'\r':e=='0'?'\0':e); pos += 2;
                } else { if (peek() == '\n') line++; str += src[pos++]; }
            }
            if (eof()) err(ln, "missing closing \"");
            pos++; return v_str(std::move(str));
        }
        if (c == '\'') { pos++; return v_list({ v_sym("quote"), read() }, ln, file); }
        std::string w;
        while (!eof()) {
            char k = peek();
            if (k==' '||k=='\t'||k=='\n'||k=='\r'||k=='('||k==')'||k=='{'||k=='}'||k=='"'||k=='#'||k=='\'') break;
            w += src[pos++];
        }
        if (w.empty()) err(line, "empty word");
        char* e = nullptr; double d = std::strtod(w.c_str(), &e);
        return (e && *e == '\0' && e != w.c_str()) ? v_num(d) : v_sym(std::move(w));
    }
    vptr read_line(char term = 0) {
        skip_h();
        int ln = line;
        vlist out;
        while (!eof()) {
            char c = peek();
            if (c == '\n') { pos++; line++; break; }
            if (c == term) break;
            if (c == ')' || (c == '}' && term != '}')) err(line, std::string("unexpected ") + c);
            out.push_back(read());
            skip_h();
        }
        // A line holding a single list is that list; a single literal is that value.
        // A single symbol is still a command: `ctr` calls ctr.
        if (out.size() == 1 && out[0]->t != value::SYM) return out[0];
        return v_list(std::move(out), ln, file);
    }
    vptr program() {
        vlist out = { v_sym("do") };
        while (true) {
            skip_h();
            if (eof()) break;
            if (peek() == '\n') { pos++; line++; continue; }
            vptr lf = read_line(0);
            if (lf->t != value::LIST || !lf->l.empty()) out.push_back(lf);
        }
        return v_list(std::move(out), 1, file);
    }
};

// === Interpreter =====
struct frame { vptr name; sptr file; int line; };   // call-stack entry; formatted lazily on error

struct interp {
    eptr global;
    std::vector<frame> call_stack;
    int stack_depth = 0, max_stack = 10000;
    int function_depth = 0, loop_depth = 0;
    unsigned long eval_count = 0;
    std::function<void()> yield_fn;         // called every 1024 evals, for hosts that need to breathe
    std::vector<std::weak_ptr<env>> tracked_envs;
    std::unordered_set<std::string> loaded_files;
    std::vector<std::string> load_path;     // extra directories searched by load
    sptr current_file; int current_line = 0;
    std::mt19937_64 rng;
    std::ostream* out = &std::cout;

    interp();
    ~interp();
    eptr make_env(eptr parent = nullptr) {
        auto e = std::make_shared<env>(std::move(parent));
        tracked_envs.push_back(e);
        if (tracked_envs.size() >= 1024 && (tracked_envs.size() & 1023) == 0)
            tracked_envs.erase(std::remove_if(tracked_envs.begin(), tracked_envs.end(),
                [](const std::weak_ptr<env>& w) { return w.expired(); }), tracked_envs.end());
        return e;
    }
    [[noreturn]] void err(const std::string& m) { throw musil_error(current_file?*current_file:"<input>", current_line, m); }
    void yield_check() { if (yield_fn && (++eval_count & 1023) == 0) yield_fn(); }

    // Registration API for libraries.
    void def(const std::string& name, op_t f) { global->vars[name] = v_op(f); }
    void def(const std::string& name, vptr v) { global->vars[name] = std::move(v); }

    vptr eval(vptr expr, eptr e);
    vptr call_fn(vptr fn, vlist args);
    vptr run(const std::string& src, const std::string& filename = "<input>");
    void load(const std::string& path);
    void repl();

    // --- Argument checking helpers for builtins ---
    void argc(const vlist& a, size_t n, const char* sig) {
        if (a.size() != n) err(std::string(sig) + ": expected " + std::to_string(n) + " argument" + (n==1?"":"s") + ", got " + std::to_string(a.size()));
    }
    void argc(const vlist& a, size_t lo, size_t hi, const char* sig) {
        if (a.size() < lo || a.size() > hi) err(std::string(sig) + ": expected " + std::to_string(lo) + "-" + std::to_string(hi) + " arguments, got " + std::to_string(a.size()));
    }
    void argc_min(const vlist& a, size_t n, const char* sig) {
        if (a.size() < n) err(std::string(sig) + ": expected at least " + std::to_string(n) + " argument" + (n==1?"":"s") + ", got " + std::to_string(a.size()));
    }
    const varr& num(const vptr& v, const char* who) {
        if (!v || v->t != value::NUM) err(std::string(who) + ": expected number, got " + type_name(v));
        return v->num;
    }
    double scalar(const vptr& v, const char* who) {
        const varr& n = num(v, who);
        if (n.size() != 1) err(std::string(who) + ": expected scalar, got vector of size " + std::to_string(n.size()));
        return n[0];
    }
    long index(const vptr& v, const char* who) {
        double d = scalar(v, who);
        if (d != std::floor(d)) err(std::string(who) + ": expected integer index, got " + str_of(v));
        return (long)d;
    }
    const std::string& str(const vptr& v, const char* who) {
        if (!v || v->t != value::STR) err(std::string(who) + ": expected string, got " + type_name(v));
        return v->s;
    }
    vlist& list(const vptr& v, const char* who) {
        if (!v || v->t != value::LIST) err(std::string(who) + ": expected list, got " + type_name(v));
        return v->l;
    }
    const vptr& fn(const vptr& v, const char* who) {
        if (!v || v->t != value::FN) err(std::string(who) + ": expected function, got " + type_name(v));
        return v;
    }
};

// Elementwise binary op with size-1 broadcasting.
template <class F>
inline varr bcast(const varr& a, const varr& b, F f, interp& i, const char* who) {
    if (a.size() == b.size()) { varr r(a.size()); for (size_t k=0; k<a.size(); k++) r[k] = f(a[k], b[k]); return r; }
    if (a.size() == 1) { varr r(b.size()); for (size_t k=0; k<b.size(); k++) r[k] = f(a[0], b[k]); return r; }
    if (b.size() == 1) { varr r(a.size()); for (size_t k=0; k<a.size(); k++) r[k] = f(a[k], b[0]); return r; }
    i.err(std::string(who) + ": size mismatch (" + std::to_string(a.size()) + " vs " + std::to_string(b.size()) + ")");
}

// === expr: Pratt parser for infix arithmetic =====
inline int op_prec(const std::string& o) {
    if (o == "||") return 1;
    if (o == "&&") return 2;
    if (o == "==" || o == "!=") return 3;
    if (o == "<" || o == ">" || o == "<=" || o == ">=") return 4;
    if (o == "+" || o == "-")  return 5;
    if (o == "*" || o == "/" || o == "%")  return 6;
    return 0;
}
inline vptr expr_parse(vlist& w, size_t& p, int min_p, interp& i, eptr e);
inline vptr expr_atom(vlist& w, size_t& p, interp& i, eptr e) {
    if (p >= w.size()) i.err("expr: unexpected end of expression");
    vptr a = w[p];
    if (a->t == value::NUM) { p++; return a; }
    if (a->t == value::SYM) {
        if (a->s == "-") { p++; vptr v = expr_atom(w, p, i, e); return v_arr(-i.num(v, "expr: unary -")); }
        if (op_prec(a->s)) i.err("expr: unexpected operator " + a->s);
        p++; auto v = e->find(a->s);
        if (!v) i.err("expr: undefined: " + a->s);
        return v;
    }
    if (a->t == value::LIST) {
        p++;
        // (a op b ...) nested in an expr is a sub-expression; anything else is a call.
        vlist& sub = a->l;
        bool infix = sub.size() == 1 || (sub.size() >= 2 && sub[1]->t == value::SYM && op_prec(sub[1]->s) > 0);
        if (!infix) return i.eval(a, e);
        size_t q = 0; vptr r = expr_parse(sub, q, 0, i, e);
        if (q < sub.size()) i.err("expr: unexpected " + str_of(sub[q]));
        return r;
    }
    if (a->t == value::STR) { p++; return a; }
    i.err("expr: bad token " + str_of(a));
}
inline vptr expr_parse(vlist& w, size_t& p, int min_p, interp& i, eptr e) {
    vptr lhs = expr_atom(w, p, i, e);
    while (p < w.size()) {
        if (w[p]->t != value::SYM) i.err("expr: expected operator, got " + str_of(w[p]));
        int prec = op_prec(w[p]->s);
        if (prec == 0) i.err("expr: unknown operator " + w[p]->s);
        if (prec < min_p) break;
        std::string op = w[p++]->s;
        vptr rhs = expr_parse(w, p, prec + 1, i, e);
        if (op == "&&") { lhs = v_bool(truthy(lhs) && truthy(rhs)); continue; }
        if (op == "||") { lhs = v_bool(truthy(lhs) || truthy(rhs)); continue; }
        if (op == "==") { if (lhs->t != value::NUM || rhs->t != value::NUM) { lhs = v_bool(equal(lhs, rhs)); continue; } }
        if (op == "!=") { if (lhs->t != value::NUM || rhs->t != value::NUM) { lhs = v_bool(!equal(lhs, rhs)); continue; } }
        const varr& L = i.num(lhs, "expr"); const varr& R = i.num(rhs, "expr");
        auto bo = [&](auto f) { return v_arr(bcast(L, R, f, i, "expr")); };
        if      (op == "+")  lhs = bo([](double a, double b){ return a + b; });
        else if (op == "-")  lhs = bo([](double a, double b){ return a - b; });
        else if (op == "*")  lhs = bo([](double a, double b){ return a * b; });
        else if (op == "/")  lhs = bo([](double a, double b){ return a / b; });
        else if (op == "%")  lhs = bo([](double a, double b){ return std::fmod(a, b); });
        else if (op == "<")  lhs = bo([](double a, double b){ return a <  b ? 1.0 : 0.0; });
        else if (op == ">")  lhs = bo([](double a, double b){ return a >  b ? 1.0 : 0.0; });
        else if (op == "<=") lhs = bo([](double a, double b){ return a <= b ? 1.0 : 0.0; });
        else if (op == ">=") lhs = bo([](double a, double b){ return a >= b ? 1.0 : 0.0; });
        else if (op == "==") lhs = bo([](double a, double b){ return a == b ? 1.0 : 0.0; });
        else if (op == "!=") lhs = bo([](double a, double b){ return a != b ? 1.0 : 0.0; });
    }
    return lhs;
}

// === Special form markers (compared by address) =====
inline vptr fn_quote   (vlist&, interp&) { return v_nil(); }
inline vptr fn_do      (vlist&, interp&) { return v_nil(); }
inline vptr fn_if      (vlist&, interp&) { return v_nil(); }
inline vptr fn_while   (vlist&, interp&) { return v_nil(); }
inline vptr fn_for     (vlist&, interp&) { return v_nil(); }
inline vptr fn_var     (vlist&, interp&) { return v_nil(); }
inline vptr fn_function(vlist&, interp&) { return v_nil(); }
inline vptr fn_return  (vlist&, interp&) { return v_nil(); }
inline vptr fn_break   (vlist&, interp&) { return v_nil(); }
inline vptr fn_continue(vlist&, interp&) { return v_nil(); }
inline vptr fn_try     (vlist&, interp&) { return v_nil(); }
inline vptr fn_expr    (vlist&, interp&) { return v_nil(); }
inline vptr fn_eval    (vlist&, interp&) { return v_nil(); }
inline vptr fn_apply   (vlist&, interp&) { return v_nil(); }

inline vptr make_partial(interp& i, const vptr& fn, const vlist& args) {
    auto bound = i.make_env(fn->closure);
    for (size_t k=0; k<args.size(); k++) bound->vars[fn->params[k]] = args[k];
    auto curr = v_nil(); curr->t = value::FN; curr->closure = bound; curr->body = fn->body;
    curr->params = std::vector<std::string>(fn->params.begin() + args.size(), fn->params.end());
    return curr;
}
// Early returns without exceptions. A (return X) reached in tail position is
// free; one reached inside a nested form has to unwind with a C++ exception,
// which costs microseconds. Bodies are rewritten at definition time so that
//     (do A (if c (return X)) B C)   becomes   (do A (if c (return X) (do B C)))
// which puts the return, and everything after the if, in tail position.
// The rewrite is applied only where at least one branch ends in a return, so
// no code is duplicated and evaluation order is unchanged.
inline bool is_form(const vptr& v, const char* name) { return v && v->t == value::LIST && !v->l.empty() && v->l[0]->t == value::SYM && v->l[0]->s == name; }
inline bool ends_in_return(const vptr& v) {
    if (is_form(v, "return")) return true;
    if (is_form(v, "do")) return v->l.size() > 1 && ends_in_return(v->l.back());
    if (is_form(v, "if")) return v->l.size() == 4 && ends_in_return(v->l[2]) && ends_in_return(v->l[3]);
    return false;
}
inline vptr hoist_returns(const vptr& v) {
    if (!v || v->t != value::LIST || v->l.empty()) return v;
    if (is_form(v, "if")) {
        vlist n = v->l; for (size_t k=2; k<n.size(); k++) n[k] = hoist_returns(n[k]);
        return v_list(std::move(n), v->line, v->file);
    }
    if (!is_form(v, "do")) return v;
    vlist out = { v->l[0] };
    for (size_t k=1; k<v->l.size(); k++) {
        vptr f = hoist_returns(v->l[k]);
        bool last = k + 1 == v->l.size();
        if (!last && is_form(f, "if") && (f->l.size() == 3 || f->l.size() == 4)) {
            vptr T = f->l[2], E = f->l.size() == 4 ? f->l[3] : nullptr;
            if (ends_in_return(T) || (E && ends_in_return(E))) {
                vlist rest = { v->l[0] }; for (size_t j=k+1; j<v->l.size(); j++) rest.push_back(v->l[j]);
                vptr rest_do = hoist_returns(v_list(std::move(rest), v->line, v->file));
                if (!ends_in_return(T)) T = v_list({ v->l[0], T, rest_do }, T->line, T->file);
                if (!E) E = rest_do; else if (!ends_in_return(E)) E = v_list({ v->l[0], E, rest_do }, E->line, E->file);
                out.push_back(v_list({ f->l[0], f->l[1], T, E }, f->line, f->file));
                return v_list(std::move(out), v->line, v->file);
            }
        }
        out.push_back(f);
    }
    return v_list(std::move(out), v->line, v->file);
}
inline std::string frame_label(const frame& f) {
    return (f.name && f.name->t == value::SYM ? f.name->s : std::string("<anon>")) + "() at " +
        (f.file ? *f.file : "?") + ":" + std::to_string(f.line);
}

// === eval =====
// One try/catch wraps the whole eval loop. User-function tail calls REPLACE
// expr and env in place (no C++ recursion), so the try/catch catches the
// tail-merged function's return correctly.
inline vptr interp::eval(vptr expr, eptr e) {
    if (++stack_depth > max_stack) { --stack_depth; err("stack overflow (depth " + std::to_string(max_stack) + ")"); }
    struct guard { int& d; ~guard() { --d; } } g{stack_depth};
    yield_check();
    bool entered_fn = false;        // did we enter a user fn in this frame?
    int saved_loop = loop_depth;
    auto cleanup = [&]() {
        if (entered_fn) { call_stack.pop_back(); --function_depth; loop_depth = saved_loop; }
    };
try {
    while (true) {
        if (!expr) { cleanup(); return v_nil(); }
        if (expr->t != value::SYM && expr->t != value::LIST) { cleanup(); return expr; }
        if (expr->t == value::SYM) {
            auto v = e->find(expr->s);
            if (!v) err("undefined: " + expr->s);
            cleanup(); return v;
        }
        if (expr->line) current_line = expr->line;
        if (expr->file) current_file = expr->file;
        auto& l = expr->l;
        if (l.empty()) { cleanup(); return v_nil(); }
        if (l.size() == 1 && l[0]->t == value::LIST) { expr = l[0]; continue; }
        vptr head;
        if (l[0]->t == value::SYM) {
            head = e->find(l[0]->s);
            if (!head) err("undefined: " + l[0]->s);
        } else if (l[0]->t == value::FN) head = l[0];
        else head = eval(l[0], e);
        op_t op = head->op;

        if (op == fn_quote) { cleanup(); return l.size() > 1 ? l[1] : v_nil(); }
        if (op == fn_do) {
            if (l.size() == 1) { cleanup(); return v_nil(); }
            for (size_t i=1; i+1<l.size(); i++) eval(l[i], e);
            expr = l.back(); continue;
        }
        if (op == fn_if) {
            if (l.size() < 3 || l.size() > 4) err("if: expected (if cond then [else])");
            if (truthy(eval(l[1], e))) { expr = l[2]; continue; }
            if (l.size() == 4) { expr = l[3]; continue; }
            cleanup(); return v_nil();
        }
        if (op == fn_while) {
            if (l.size() != 3) err("while: expected (while cond body)");
            vptr last = v_nil(); ++loop_depth;
            try { while (truthy(eval(l[1], e)))
                try { last = eval(l[2], e); } catch (continue_signal&) {} }
            catch (break_signal&) {}
            --loop_depth; cleanup(); return last;
        }
        if (op == fn_for) {
            if (l.size() != 5) err("for: expected (for init cond step body)");
            eval(l[1], e);
            vptr last = v_nil(); ++loop_depth;
            try { while (truthy(eval(l[2], e))) {
                try { last = eval(l[4], e); } catch (continue_signal&) {}
                eval(l[3], e); } }
            catch (break_signal&) {}
            --loop_depth; cleanup(); return last;
        }
        if (op == fn_var) {
            if (l.size() != 3) err("var: expected (var name value)");
            if (l[1]->t != value::SYM) err("var: name must be a symbol, got " + str_of(l[1]));
            vptr v = eval(l[2], e);
            if (!e->assign(l[1]->s, v)) e->vars[l[1]->s] = v;
            cleanup(); return v;
        }
        if (op == fn_function) {
            bool named; size_t pi, bi;
            if (l.size() == 4 && l[1]->t == value::SYM && l[2]->t == value::LIST) { named = true;  pi = 2; bi = 3; }
            else if (l.size() == 3 && l[1]->t == value::LIST)                     { named = false; pi = 1; bi = 2; }
            else err("function: expected (function [name] (params) body)");
            auto fn = v_nil(); fn->t = value::FN; fn->closure = e; fn->body = hoist_returns(l[bi]);
            for (auto& p : l[pi]->l) {
                if (p->t != value::SYM) err("function: parameter must be a symbol, got " + str_of(p));
                fn->params.push_back(p->s);
            }
            if (named) e->vars[l[1]->s] = fn;
            cleanup(); return fn;
        }
        if (op == fn_return) {
            if (function_depth == 0) err("return outside function");
            if (l.size() > 2) err("return: expected (return [value])");
            if (entered_fn) { // tail position — convert (return X) to evaluating X in tail position
                if (l.size() <= 1) { cleanup(); return v_nil(); }
                expr = l[1]; continue;
            }
            throw return_signal{ l.size() > 1 ? eval(l[1], e) : v_nil() };
        }
        if (op == fn_break)    { if (loop_depth == 0) err("break outside loop"); throw break_signal{}; }
        if (op == fn_continue) { if (loop_depth == 0) err("continue outside loop"); throw continue_signal{}; }
        if (op == fn_try) {
            if (l.size() != 5 || l[2]->t != value::SYM || l[2]->s != "catch" || l[3]->t != value::SYM)
                err("try: expected (try body catch name handler)");
            size_t depth = call_stack.size(); int fd = function_depth, ld = loop_depth;
            try { vptr r = eval(l[1], e); cleanup(); return r; }
            catch (musil_error& ex) {
                call_stack.resize(depth); function_depth = fd; loop_depth = ld;
                auto ne = make_env(e); ne->vars[l[3]->s] = v_str(ex.what());
                vptr r = eval(l[4], ne); cleanup(); return r;
            }
        }
        if (op == fn_expr) {
            if (l.size() != 2 || l[1]->t != value::LIST) err("expr: expected (expr (a op b ...))");
            size_t p = 0; vptr r = expr_parse(l[1]->l, p, 0, *this, e); cleanup(); return r;
        }
        if (op == fn_eval) {
            if (l.size() != 2) err("eval: expected (eval form)");
            expr = eval(l[1], e); continue;
        }
        if (op == fn_apply) {
            if (l.size() != 3) err("apply: expected (apply fn list)");
            vptr fv = eval(l[1], e), lv = eval(l[2], e);
            fn(fv, "apply"); list(lv, "apply");
            vlist nl = { fv };
            for (auto& x : lv->l) nl.push_back(v_list({ v_sym("quote"), x }));
            expr = v_list(std::move(nl), expr->line, expr->file);
            continue;
        }

        // Function call
        if (head->t != value::FN) err("not callable: " + str_of(l[0]) + " (" + type_name(head) + ")");
        vlist args; args.reserve(l.size() - 1);
        for (size_t i=1; i<l.size(); i++) args.push_back(eval(l[i], e));
        if (head->op) { vptr r = head->op(args, *this); cleanup(); return r; }   // op may throw: cleanup after
        // Currying: too few args => return partial application
        if (args.size() < head->params.size()) { vptr r = make_partial(*this, head, args); cleanup(); return r; }
        // Over-application: call with the params it takes, apply the result to the rest
        if (args.size() > head->params.size()) {
            vlist first(args.begin(), args.begin() + head->params.size());
            vptr r = call_fn(head, std::move(first));
            if (r->t != value::FN)
                err((l[0]->t == value::SYM ? l[0]->s : std::string("<anon>")) + ": too many arguments (expected " +
                    std::to_string(head->params.size()) + ", got " + std::to_string(args.size()) + ")");
            vlist nl = { r };
            for (size_t i=head->params.size(); i<args.size(); i++) nl.push_back(v_list({ v_sym("quote"), args[i] }));
            expr = v_list(std::move(nl), expr->line, expr->file);
            continue;
        }
        // User function call — in-place TCO. Replace top of call_stack on each
        // tail-merged call so the trace reflects current location, not history.
        frame fr{ l[0], current_file, current_line };
        if (entered_fn) call_stack.back() = fr;
        else { call_stack.push_back(fr); ++function_depth; loop_depth = 0; entered_fn = true; }
        auto ne = make_env(head->closure);
        for (size_t i=0; i<head->params.size(); i++) ne->vars[head->params[i]] = args[i];
        expr = head->body; e = ne;
        // Continue eval loop with new expr/env — no new C++ frame.
    }
} catch (return_signal& rs) {
    if (entered_fn) { cleanup(); return rs.val; }
    throw;  // propagate up to the eval frame that entered the function
  }
  catch (musil_error& fe) {
    if (fe.trace.empty() && !call_stack.empty()) {
        fe.trace.reserve(call_stack.size());
        for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) fe.trace.push_back(frame_label(*it));
        if (fe.trace.empty()) fe.trace.push_back("<top>");
    }
    cleanup(); throw;
  }
  catch (...) { cleanup(); throw; }
}

// Call a function value from C++ (used by map/filter/reduce and by libraries).
inline vptr interp::call_fn(vptr f, vlist args) {
    fn(f, "call");
    if (f->op == fn_eval) { if (args.size() != 1) err("eval: expected 1 argument"); return eval(args[0], global); }
    if (f->op == fn_apply) {
        if (args.size() != 2) err("apply: expected 2 arguments");
        fn(args[0], "apply"); list(args[1], "apply");
        return call_fn(args[0], args[1]->l);
    }
    if (f->op) return f->op(args, *this);
    if (args.size() < f->params.size()) return make_partial(*this, f, args);
    if (args.size() > f->params.size()) {
        vlist first(args.begin(), args.begin() + f->params.size());
        vptr r = call_fn(f, std::move(first));
        if (r->t != value::FN) err("too many arguments (expected " + std::to_string(f->params.size()) + ", got " + std::to_string(args.size()) + ")");
        return call_fn(r, vlist(args.begin() + f->params.size(), args.end()));
    }
    auto ne = make_env(f->closure);
    for (size_t i=0; i<f->params.size(); i++) ne->vars[f->params[i]] = args[i];
    call_stack.push_back(frame{ nullptr, current_file, current_line });
    ++function_depth; int sl = loop_depth; loop_depth = 0;
    vptr r;
    try { r = eval(f->body, ne); } catch (return_signal& rs) { r = rs.val; }
    catch (...) { call_stack.pop_back(); --function_depth; loop_depth = sl; throw; }
    call_stack.pop_back(); --function_depth; loop_depth = sl;
    return r;
}

// === Builtins =====
// Arithmetic: variadic, elementwise, broadcasting. (- x) and (/ x) are unary.
#define BINOP(name, sym, init, unary) \
    inline vptr name(vlist& a, interp& i) { \
        if (a.empty()) return v_num(init); \
        varr r = i.num(a[0], #sym); \
        auto f = [](double x, double y){ return x sym y; }; \
        if (a.size() == 1 && unary) r = bcast(varr(init,1), r, f, i, #sym); \
        else for (size_t k=1; k<a.size(); k++) r = bcast(r, i.num(a[k], #sym), f, i, #sym); \
        return v_arr(std::move(r)); }
BINOP(fn_add, +, 0.0, false) BINOP(fn_sub, -, 0.0, true)
BINOP(fn_mul, *, 1.0, false) BINOP(fn_div, /, 1.0, true)
#define CMP(name, sym) inline vptr name(vlist& a, interp& i) { i.argc(a, 2, #sym); \
    return v_arr(bcast(i.num(a[0], #sym), i.num(a[1], #sym), [](double x, double y){return x sym y ? 1.0 : 0.0;}, i, #sym)); }
CMP(fn_lt, <) CMP(fn_gt, >) CMP(fn_le, <=) CMP(fn_ge, >=)
// == and != are numeric (elementwise) on numbers, structural otherwise.
inline vptr fn_eq(vlist& a, interp& i) { i.argc(a, 2, "==");
    if (a[0]->t == value::NUM && a[1]->t == value::NUM) return v_arr(bcast(a[0]->num, a[1]->num, [](double x, double y){return x == y ? 1.0 : 0.0;}, i, "=="));
    return v_bool(equal(a[0], a[1])); }
inline vptr fn_ne(vlist& a, interp& i) { i.argc(a, 2, "!=");
    if (a[0]->t == value::NUM && a[1]->t == value::NUM) return v_arr(bcast(a[0]->num, a[1]->num, [](double x, double y){return x != y ? 1.0 : 0.0;}, i, "!="));
    return v_bool(!equal(a[0], a[1])); }
inline vptr fn_equalp(vlist& a, interp& i) { i.argc(a, 2, "equal?"); return v_bool(equal(a[0], a[1])); }
#define UN(name, sym, e) inline vptr name(vlist& a, interp& i) { i.argc(a, 1, sym); varr r=i.num(a[0], sym); for(size_t k=0;k<r.size();k++) r[k]=e; return v_arr(std::move(r)); }
UN(fn_sin, "sin", std::sin(r[k])) UN(fn_cos, "cos", std::cos(r[k])) UN(fn_tan, "tan", std::tan(r[k]))
UN(fn_asin, "asin", std::asin(r[k])) UN(fn_acos, "acos", std::acos(r[k])) UN(fn_atan, "atan", std::atan(r[k]))
UN(fn_sqrt, "sqrt", std::sqrt(r[k])) UN(fn_exp, "exp", std::exp(r[k])) UN(fn_log, "log", std::log(r[k]))
UN(fn_log2, "log2", std::log2(r[k])) UN(fn_log10, "log10", std::log10(r[k]))
UN(fn_abs, "abs", std::fabs(r[k])) UN(fn_flr, "floor", std::floor(r[k])) UN(fn_cei, "ceil", std::ceil(r[k])) UN(fn_rnd, "round", std::round(r[k]))

inline vptr fn_mod(vlist& a, interp& i) { i.argc(a, 2, "mod"); return v_arr(bcast(i.num(a[0], "mod"), i.num(a[1], "mod"), [](double x, double y){return std::fmod(x,y);}, i, "mod")); }
inline vptr fn_pow(vlist& a, interp& i) { i.argc(a, 2, "pow"); return v_arr(bcast(i.num(a[0], "pow"), i.num(a[1], "pow"), [](double x, double y){return std::pow(x,y);}, i, "pow")); }
inline vptr fn_atan2(vlist& a, interp& i) { i.argc(a, 2, "atan2"); return v_arr(bcast(i.num(a[0], "atan2"), i.num(a[1], "atan2"), [](double x, double y){return std::atan2(x,y);}, i, "atan2")); }
inline vptr fn_not(vlist& a, interp& i) { i.argc(a, 1, "not"); return v_bool(!truthy(a[0])); }
inline vptr fn_and(vlist& a, interp&) { for (auto& x : a) if (!truthy(x)) return v_bool(false); return v_bool(true); }
inline vptr fn_or (vlist& a, interp&) { for (auto& x : a) if (truthy(x))  return v_bool(true); return v_bool(false); }
// min/max: one vector argument => reduction; several arguments => elementwise.
inline vptr fn_min(vlist& a, interp& i) {
    i.argc_min(a, 1, "min");
    if (a.size() == 1) { const varr& v = i.num(a[0], "min"); if (v.size() == 0) i.err("min: empty vector"); return v_num(v.min()); }
    varr r = i.num(a[0], "min");
    for (size_t k=1; k<a.size(); k++) r = bcast(r, i.num(a[k], "min"), [](double x, double y){return x < y ? x : y;}, i, "min");
    return v_arr(std::move(r));
}
inline vptr fn_max(vlist& a, interp& i) {
    i.argc_min(a, 1, "max");
    if (a.size() == 1) { const varr& v = i.num(a[0], "max"); if (v.size() == 0) i.err("max: empty vector"); return v_num(v.max()); }
    varr r = i.num(a[0], "max");
    for (size_t k=1; k<a.size(); k++) r = bcast(r, i.num(a[k], "max"), [](double x, double y){return x > y ? x : y;}, i, "max");
    return v_arr(std::move(r));
}

// Lists
inline vptr fn_list(vlist& a, interp&) { return v_list(a); }
inline vptr fn_cons(vlist& a, interp& i) { i.argc(a, 2, "cons"); vlist& t = i.list(a[1], "cons"); vlist r; r.reserve(t.size()+1); r.push_back(a[0]); for (auto& x : t) r.push_back(x); return v_list(std::move(r)); }
inline vptr fn_append(vlist& a, interp& i) { i.argc_min(a, 1, "append"); vlist r = i.list(a[0], "append"); for (size_t k=1; k<a.size(); k++) r.push_back(a[k]); return v_list(std::move(r)); }
inline vptr fn_concat_list(vlist& a, interp& i) { vlist r; for (auto& x : a) for (auto& y : i.list(x, "concat-list")) r.push_back(y); return v_list(std::move(r)); }
inline vptr fn_push(vlist& a, interp& i) { i.argc(a, 2, "push"); i.list(a[0], "push").push_back(a[1]); return a[0]; }
inline vptr fn_pop(vlist& a, interp& i) { i.argc(a, 1, "pop"); vlist& l = i.list(a[0], "pop"); if (l.empty()) i.err("pop: empty list"); vptr r = l.back(); l.pop_back(); return r; }
inline vptr fn_reverse(vlist& a, interp& i) { i.argc(a, 1, "reverse");
    if (a[0]->t == value::LIST) { vlist r(a[0]->l.rbegin(), a[0]->l.rend()); return v_list(std::move(r)); }
    if (a[0]->t == value::NUM)  { varr r(a[0]->num.size()); for (size_t k=0; k<r.size(); k++) r[k] = a[0]->num[r.size()-1-k]; return v_arr(std::move(r)); }
    if (a[0]->t == value::STR)  { return v_str(std::string(a[0]->s.rbegin(), a[0]->s.rend())); }
    i.err(std::string("reverse: expected list, number or string, got ") + type_name(a[0])); }

// Polymorphic on LIST and NUM (and STR for length, getidx, slice, empty?)
inline vptr fn_length(vlist& a, interp& i) { i.argc(a, 1, "length"); auto& v=a[0];
    if (v->t==value::LIST) return v_num((double)v->l.size());
    if (v->t==value::NUM)  return v_num((double)v->num.size());
    if (v->t==value::STR)  return v_num((double)v->s.size());
    i.err(std::string("length: expected list, number or string, got ") + type_name(v)); }
inline vptr fn_head(vlist& a, interp& i) { i.argc(a, 1, "head"); auto& v=a[0];
    if (v->t==value::LIST) { if (v->l.empty()) i.err("head: empty list"); return v->l[0]; }
    if (v->t==value::NUM)  { if (v->num.size()==0) i.err("head: empty vector"); return v_num(v->num[0]); }
    i.err(std::string("head: expected list or number, got ") + type_name(v)); }
inline vptr fn_tail(vlist& a, interp& i) { i.argc(a, 1, "tail"); auto& v=a[0];
    if (v->t==value::LIST) { if (v->l.empty()) i.err("tail: empty list"); return v_list(vlist(v->l.begin()+1, v->l.end())); }
    if (v->t==value::NUM)  { if (v->num.size()==0) i.err("tail: empty vector"); varr r=v->num[std::slice(1, v->num.size()-1, 1)]; return v_arr(std::move(r)); }
    i.err(std::string("tail: expected list or number, got ") + type_name(v)); }
inline vptr fn_last(vlist& a, interp& i) { i.argc(a, 1, "last"); auto& v=a[0];
    if (v->t==value::LIST) { if (v->l.empty()) i.err("last: empty list"); return v->l.back(); }
    if (v->t==value::NUM)  { if (v->num.size()==0) i.err("last: empty vector"); return v_num(v->num[v->num.size()-1]); }
    i.err(std::string("last: expected list or number, got ") + type_name(v)); }
inline vptr fn_emptyp(vlist& a, interp& i) { i.argc(a, 1, "empty?"); auto& v=a[0];
    return v_bool(v->t==value::LIST?v->l.empty():v->t==value::NUM?v->num.size()==0:v->t==value::STR?v->s.empty():v->t==value::NIL); }
inline size_t checked_index(interp& i, const vptr& idx, size_t size, const char* who) {
    long k = i.index(idx, who);
    if (k < 0) k += (long)size;                       // negative indices count from the end
    if (k < 0 || (size_t)k >= size) i.err(std::string(who) + ": index " + str_of(idx) + " out of range (size " + std::to_string(size) + ")");
    return (size_t)k;
}
inline vptr fn_getidx(vlist& a, interp& i) {
    i.argc(a, 2, "getidx"); auto& v=a[0];
    if (v->t==value::NUM)  return v_num(v->num[checked_index(i, a[1], v->num.size(), "getidx")]);
    if (v->t==value::LIST) return v->l[checked_index(i, a[1], v->l.size(), "getidx")];
    if (v->t==value::STR)  return v_str(std::string(1, v->s[checked_index(i, a[1], v->s.size(), "getidx")]));
    i.err(std::string("getidx: expected list, number or string, got ") + type_name(v)); }
inline vptr fn_setidx(vlist& a, interp& i) {
    i.argc(a, 3, "setidx"); auto& v=a[0];
    if (v->t==value::NUM)  { v->num[checked_index(i, a[1], v->num.size(), "setidx")] = i.scalar(a[2], "setidx"); return v; }
    if (v->t==value::LIST) { v->l[checked_index(i, a[1], v->l.size(), "setidx")] = a[2]; return v; }
    i.err(std::string("setidx: expected list or number, got ") + type_name(v)); }
inline vptr fn_slice(vlist& a, interp& i) {   // (slice x start [count]) — start may be negative
    i.argc(a, 2, 3, "slice"); auto& v=a[0];
    size_t size = v->t==value::LIST ? v->l.size() : v->t==value::NUM ? v->num.size() : v->t==value::STR ? v->s.size() : 0;
    if (v->t!=value::LIST && v->t!=value::NUM && v->t!=value::STR) i.err(std::string("slice: expected list, number or string, got ") + type_name(v));
    long s = i.index(a[1], "slice"); if (s < 0) s += (long)size;
    if (s < 0) s = 0;
    if ((size_t)s > size) s = (long)size;
    long n = a.size()==3 ? i.index(a[2], "slice") : (long)size - s;
    if (n < 0) n = 0;
    if ((size_t)(s + n) > size) n = (long)size - s;
    if (v->t==value::LIST) return v_list(vlist(v->l.begin()+s, v->l.begin()+s+n));
    if (v->t==value::STR)  return v_str(v->s.substr((size_t)s, (size_t)n));
    varr r(n); for (long k=0; k<n; k++) r[k] = v->num[s+k]; return v_arr(std::move(r));
}
inline vptr fn_find(vlist& a, interp& i) {    // index of first element equal to x, or -1
    i.argc(a, 2, "find"); auto& v=a[0];
    if (v->t==value::LIST) { for (size_t k=0; k<v->l.size(); k++) if (equal(v->l[k], a[1])) return v_num((double)k); return v_num(-1); }
    if (v->t==value::NUM)  { double x = i.scalar(a[1], "find"); for (size_t k=0; k<v->num.size(); k++) if (v->num[k]==x) return v_num((double)k); return v_num(-1); }
    if (v->t==value::STR)  { size_t p = v->s.find(i.str(a[1], "find")); return v_num(p==std::string::npos ? -1.0 : (double)p); }
    i.err(std::string("find: expected list, number or string, got ") + type_name(v)); }

// Vectors
inline vptr fn_vec(vlist& a, interp& i) {
    if (a.size()==1 && a[0]->t==value::LIST) {
        varr r(a[0]->l.size());
        for (size_t k=0; k<a[0]->l.size(); k++) r[k]=i.scalar(a[0]->l[k], "vec");
        return v_arr(std::move(r));
    }
    size_t n = 0; for (auto& x : a) n += i.num(x, "vec").size();
    varr r(n); size_t p = 0;
    for (auto& x : a) for (size_t k=0; k<x->num.size(); k++) r[p++] = x->num[k];
    return v_arr(std::move(r));
}
inline vptr fn_range(vlist& a, interp& i) {   // (range n) | (range a b) | (range a b step)
    i.argc(a, 1, 3, "range");
    double lo = 0, hi, step = 1;
    if (a.size()==1) hi = i.scalar(a[0], "range");
    else { lo = i.scalar(a[0], "range"); hi = i.scalar(a[1], "range"); if (a.size()==3) step = i.scalar(a[2], "range"); }
    if (step == 0) i.err("range: step must be nonzero");
    long n = (long)std::ceil((hi - lo) / step); if (n < 0) n = 0;
    varr r(n); for (long k=0; k<n; k++) r[k] = lo + k*step;
    return v_arr(std::move(r));
}
inline vptr fn_linspace(vlist& a, interp& i) {
    i.argc(a, 3, "linspace");
    double a0=i.scalar(a[0], "linspace"), a1=i.scalar(a[1], "linspace"); long n=i.index(a[2], "linspace");
    if (n<2) i.err("linspace: n must be >= 2");
    varr r(n); for (long k=0; k<n; k++) r[k]=a0+(a1-a0)*k/(n-1);
    return v_arr(std::move(r));
}
inline size_t checked_size(interp& i, const vptr& v, const char* who) { long n = i.index(v, who); if (n < 0) i.err(std::string(who) + ": negative size"); return (size_t)n; }
inline vptr fn_zeros(vlist& a, interp& i) { i.argc(a, 1, "zeros"); return v_arr(varr(0.0, checked_size(i, a[0], "zeros"))); }
inline vptr fn_ones (vlist& a, interp& i) { i.argc(a, 1, "ones");  return v_arr(varr(1.0, checked_size(i, a[0], "ones"))); }
inline vptr fn_seed (vlist& a, interp& i) { i.argc(a, 1, "seed"); i.rng.seed((uint64_t)i.scalar(a[0], "seed")); return v_nil(); }
inline vptr fn_rand (vlist& a, interp& i) {
    i.argc(a, 0, 1, "rand");
    std::uniform_real_distribution<double> d(0.0, 1.0);
    if (a.empty()) return v_num(d(i.rng));
    size_t n=checked_size(i, a[0], "rand"); varr r(n); for (size_t k=0; k<n; k++) r[k]=d(i.rng);
    return v_arr(std::move(r));
}
inline vptr fn_sort(vlist& a, interp& i) { i.argc(a, 1, "sort"); varr r=i.num(a[0], "sort"); std::sort(std::begin(r), std::end(r)); return v_arr(std::move(r)); }
inline vptr fn_sum(vlist& a, interp& i) {
    i.argc(a, 1, "sum");
    if (a[0]->t==value::NUM)  { double s=0; for (size_t k=0; k<a[0]->num.size(); k++) s+=a[0]->num[k]; return v_num(s); }
    if (a[0]->t==value::LIST) { double s=0; for (auto& x : a[0]->l) s+=i.scalar(x, "sum"); return v_num(s); }
    i.err(std::string("sum: expected number or list, got ") + type_name(a[0]));
}
inline vptr fn_prod(vlist& a, interp& i) { i.argc(a, 1, "prod"); const varr& v=i.num(a[0], "prod"); double p=1; for (size_t k=0; k<v.size(); k++) p*=v[k]; return v_num(p); }
inline vptr fn_mean(vlist& a, interp& i) { i.argc(a, 1, "mean"); const varr& v=i.num(a[0], "mean"); if (v.size()==0) i.err("mean: empty vector"); double s=0; for (size_t k=0; k<v.size(); k++) s+=v[k]; return v_num(s/v.size()); }
inline vptr fn_dot(vlist& a, interp& i) { i.argc(a, 2, "dot"); const varr& x=i.num(a[0], "dot"); const varr& y=i.num(a[1], "dot");
    if (x.size()!=y.size()) i.err("dot: size mismatch (" + std::to_string(x.size()) + " vs " + std::to_string(y.size()) + ")");
    double s=0; for (size_t k=0; k<x.size(); k++) s+=x[k]*y[k]; return v_num(s); }

// Strings
inline vptr fn_concat(vlist& a, interp&) { std::string s; for (auto& x : a) s+=str_of(x); return v_str(std::move(s)); }
inline vptr fn_split(vlist& a, interp& i) {
    i.argc(a, 2, "split");
    const std::string& s=i.str(a[0], "split"); const std::string& sep=i.str(a[1], "split");
    vlist out;
    if (sep.empty()) { for (char c : s) out.push_back(v_str(std::string(1, c))); return v_list(std::move(out)); }
    size_t st=0, p;
    while ((p=s.find(sep, st))!=std::string::npos) { out.push_back(v_str(s.substr(st, p-st))); st=p+sep.size(); }
    out.push_back(v_str(s.substr(st)));
    return v_list(std::move(out));
}
inline vptr fn_join(vlist& a, interp& i) {
    i.argc(a, 2, "join");
    vlist& l=i.list(a[0], "join"); const std::string& sep=i.str(a[1], "join");
    std::string out; for (size_t k=0; k<l.size(); k++) { if (k) out+=sep; out+=str_of(l[k]); }
    return v_str(std::move(out));
}
inline vptr fn_upper(vlist& a, interp& i) { i.argc(a, 1, "upper"); std::string s=i.str(a[0], "upper"); for (auto& c : s) c=(char)std::toupper((unsigned char)c); return v_str(std::move(s)); }
inline vptr fn_lower(vlist& a, interp& i) { i.argc(a, 1, "lower"); std::string s=i.str(a[0], "lower"); for (auto& c : s) c=(char)std::tolower((unsigned char)c); return v_str(std::move(s)); }
inline vptr fn_trim(vlist& a, interp& i) { i.argc(a, 1, "trim"); const std::string& s=i.str(a[0], "trim"); size_t st=0, en=s.size(); while (st<en && std::isspace((unsigned char)s[st])) st++; while (en>st && std::isspace((unsigned char)s[en-1])) en--; return v_str(s.substr(st, en-st)); }
inline vptr fn_format(vlist& a, interp& i) {   // (format "x = {} y = {}" x y)
    i.argc_min(a, 1, "format");
    const std::string& f=i.str(a[0], "format"); std::string out; size_t k=1;
    for (size_t p=0; p<f.size(); p++) {
        if (f[p]=='{' && p+1<f.size() && f[p+1]=='}') { if (k>=a.size()) i.err("format: not enough arguments"); out+=str_of(a[k++]); p++; }
        else out+=f[p];
    }
    if (k<a.size()) i.err("format: too many arguments");
    return v_str(std::move(out));
}
inline vptr fn_chr(vlist& a, interp& i) { i.argc(a, 1, "chr"); return v_str(std::string(1, (char)i.index(a[0], "chr"))); }
inline vptr fn_ord(vlist& a, interp& i) { i.argc(a, 1, "ord"); const std::string& s=i.str(a[0], "ord"); if (s.empty()) i.err("ord: empty string"); return v_num((double)(unsigned char)s[0]); }

// I/O
inline vptr fn_print(vlist& a, interp& i) { for (size_t k=0; k<a.size(); k++) { if (k) *i.out<<" "; *i.out<<str_of(a[k]); } *i.out<<"\n"; return v_nil(); }
inline vptr fn_read(vlist& a, interp& i) {
    i.argc(a, 1, "read"); const std::string& fn=i.str(a[0], "read");
    std::ifstream f(fn); if (!f) i.err("read: cannot open " + fn);
    std::stringstream ss; ss<<f.rdbuf(); return v_str(ss.str());
}
inline vptr fn_write(vlist& a, interp& i) {
    i.argc(a, 2, "write"); const std::string& fn=i.str(a[0], "write");
    std::ofstream f(fn); if (!f) i.err("write: cannot open " + fn);
    f << str_of(a[1]); return v_nil();
}
inline vptr fn_appendf(vlist& a, interp& i) {
    i.argc(a, 2, "append-file"); const std::string& fn=i.str(a[0], "append-file");
    std::ofstream f(fn, std::ios::app); if (!f) i.err("append-file: cannot open " + fn);
    f << str_of(a[1]); return v_nil();
}
inline vptr fn_existsp(vlist& a, interp& i) { i.argc(a, 1, "exists?"); return v_bool(fs::exists(i.str(a[0], "exists?"))); }
inline vptr fn_exec(vlist& a, interp& i) {
    i.argc(a, 1, "exec"); const std::string& cmd=i.str(a[0], "exec");
    FILE* p = popen(cmd.c_str(), "r"); if (!p) i.err("exec: cannot run " + cmd);
    std::string out; char buf[256]; while (fgets(buf, sizeof(buf), p)) out+=buf;
    pclose(p); return v_str(std::move(out));
}
inline vptr fn_input(vlist& a, interp& i) { i.argc(a, 0, 1, "input"); if (!a.empty()) { *i.out << i.str(a[0], "input") << std::flush; } std::string s; if (!std::getline(std::cin, s)) return v_nil(); return v_str(std::move(s)); }
inline vptr fn_exit(vlist& a, interp& i) { i.argc(a, 0, 1, "exit"); throw exit_signal{ a.empty() ? 0 : (int)i.scalar(a[0], "exit") }; }

// Meta
inline vptr fn_type(vlist& a, interp& i) {
    i.argc(a, 1, "type");
    if (a[0]->t==value::NUM) return v_str(a[0]->num.size()==1 ? "scalar" : "vec");
    if (a[0]->t==value::OPAQUE) return v_str("opaque:" + a[0]->opaque_tag);
    return v_str(type_name(a[0]));
}
inline vptr fn_str(vlist& a, interp& i) { i.argc(a, 1, "str"); return v_str(str_of(a[0])); }
inline vptr fn_sym(vlist& a, interp& i) { i.argc(a, 1, "sym"); return v_sym(i.str(a[0], "sym")); }
inline vptr fn_num(vlist& a, interp& i) {
    i.argc(a, 1, "num");
    if (a[0]->t==value::NUM) return a[0];
    const std::string& s=i.str(a[0], "num");
    char* e=nullptr; double d=std::strtod(s.c_str(), &e);
    if (!e || *e!='\0' || e==s.c_str()) i.err("num: not a number: " + s);
    return v_num(d);
}
inline vptr fn_copy(vlist& a, interp& i) {   // shallow copy: a new container with the same elements
    i.argc(a, 1, "copy");
    auto& s=a[0]; auto v=v_nil();
    v->t=s->t; v->num=s->num; v->s=s->s; v->l=s->l;
    v->op=s->op; v->params=s->params; v->body=s->body; v->closure=s->closure;
    v->opaque=s->opaque; v->opaque_tag=s->opaque_tag;
    return v;
}
inline vptr fn_error(vlist& a, interp& i) { std::string m; for (auto& x : a) m+=str_of(x); i.err(m.empty() ? "error" : m); }
inline vptr fn_assert(vlist& a, interp& i) {   // (assert cond [message...])
    i.argc_min(a, 1, "assert");
    if (truthy(a[0])) return v_nil();
    std::string m = "assertion failed"; for (size_t k=1; k<a.size(); k++) m += (k==1 ? ": " : " ") + str_of(a[k]);
    i.err(m);
}
inline vptr fn_clock(vlist& a, interp& i) { i.argc(a, 0, "clock"); return v_num(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
inline vptr fn_load(vlist& a, interp& i) { i.argc(a, 1, "load"); i.load(i.str(a[0], "load")); return v_nil(); }
inline vptr fn_definedp(vlist& a, interp& i) { i.argc(a, 1, "defined?"); const std::string& n = a[0]->t==value::SYM ? a[0]->s : i.str(a[0], "defined?"); return v_bool(i.global->find(n) != nullptr); }
inline vptr fn_vars(vlist& a, interp& i) { i.argc(a, 0, "vars"); std::vector<std::string> names; for (auto& kv : i.global->vars) names.push_back(kv.first); std::sort(names.begin(), names.end()); vlist out; for (auto& n : names) out.push_back(v_sym(n)); return v_list(std::move(out)); }

// Higher-order — polymorphic on LIST and NUM
inline vptr fn_map(vlist& a, interp& i) {
    i.argc(a, 2, "map"); i.fn(a[1], "map");
    if (a[0]->t==value::LIST) {
        vlist out; out.reserve(a[0]->l.size());
        for (auto& x : a[0]->l) { vlist arg={x}; out.push_back(i.call_fn(a[1], arg)); }
        return v_list(std::move(out));
    }
    if (a[0]->t==value::NUM) {
        varr r(a[0]->num.size());
        for (size_t k=0; k<r.size(); k++) { vlist arg={v_num(a[0]->num[k])}; r[k]=i.scalar(i.call_fn(a[1], arg), "map: function result"); }
        return v_arr(std::move(r));
    }
    i.err(std::string("map: expected list or number, got ") + type_name(a[0]));
}
inline vptr fn_filter(vlist& a, interp& i) {
    i.argc(a, 2, "filter"); i.fn(a[1], "filter");
    if (a[0]->t==value::LIST) {
        vlist out; for (auto& x : a[0]->l) { vlist arg={x}; if (truthy(i.call_fn(a[1], arg))) out.push_back(x); }
        return v_list(std::move(out));
    }
    if (a[0]->t==value::NUM) {
        std::vector<double> out;
        for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={v_num(a[0]->num[k])}; if (truthy(i.call_fn(a[1], arg))) out.push_back(a[0]->num[k]); }
        varr r(out.size()); for (size_t k=0; k<out.size(); k++) r[k]=out[k];
        return v_arr(std::move(r));
    }
    i.err(std::string("filter: expected list or number, got ") + type_name(a[0]));
}
inline vptr fn_reduce(vlist& a, interp& i) {
    i.argc(a, 3, "reduce"); i.fn(a[1], "reduce");
    vptr acc=a[2];
    if (a[0]->t==value::LIST) { for (auto& x : a[0]->l) { vlist arg={acc, x}; acc=i.call_fn(a[1], arg); } return acc; }
    if (a[0]->t==value::NUM)  { for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={acc, v_num(a[0]->num[k])}; acc=i.call_fn(a[1], arg); } return acc; }
    i.err(std::string("reduce: expected list or number, got ") + type_name(a[0]));
}
inline vptr fn_each(vlist& a, interp& i) {    // (each x fn) — call for side effects, return nil
    i.argc(a, 2, "each"); i.fn(a[1], "each");
    if (a[0]->t==value::LIST) { for (auto& x : a[0]->l) { vlist arg={x}; i.call_fn(a[1], arg); } return v_nil(); }
    if (a[0]->t==value::NUM)  { for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={v_num(a[0]->num[k])}; i.call_fn(a[1], arg); } return v_nil(); }
    i.err(std::string("each: expected list or number, got ") + type_name(a[0]));
}

// === Constructor / destructor =====
inline interp::interp() {
    rng.seed((uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    global = make_env();
#ifdef _WIN32
    const char sep = ';'; const char* home = std::getenv("USERPROFILE");
#else
    const char sep = ':'; const char* home = std::getenv("HOME");
#endif
    if (const char* mp = std::getenv("MUSIL_PATH")) {
        std::string s = mp; size_t st = 0, p;
        while ((p = s.find(sep, st)) != std::string::npos) { if (p > st) load_path.push_back(s.substr(st, p-st)); st = p+1; }
        if (st < s.size()) load_path.push_back(s.substr(st));
    }
    if (home) load_path.push_back((fs::path(home) / ".musil").string());
    auto a = [&](const char* n, op_t f) { def(n, f); };
    // Constants
    def("nil", v_nil()); def("true", v_num(1)); def("false", v_num(0));
    def("pi", v_num(3.14159265358979323846)); def("inf", v_num(INFINITY));
    def("version", v_str(MUSIL_VERSION));
    // Special forms
    a("quote", fn_quote); a("do", fn_do); a("if", fn_if);
    a("while", fn_while); a("for", fn_for); a("var", fn_var);
    a("function", fn_function); a("return", fn_return);
    a("break", fn_break); a("continue", fn_continue);
    a("try", fn_try); a("expr", fn_expr);
    a("eval", fn_eval); a("apply", fn_apply);
    // Arithmetic
    a("+", fn_add); a("-", fn_sub); a("*", fn_mul); a("/", fn_div);
    a("<", fn_lt); a(">", fn_gt); a("<=", fn_le); a(">=", fn_ge); a("==", fn_eq); a("!=", fn_ne); a("equal?", fn_equalp);
    a("sin", fn_sin); a("cos", fn_cos); a("tan", fn_tan); a("asin", fn_asin); a("acos", fn_acos); a("atan", fn_atan); a("atan2", fn_atan2);
    a("sqrt", fn_sqrt); a("exp", fn_exp); a("log", fn_log); a("log2", fn_log2); a("log10", fn_log10); a("pow", fn_pow);
    a("abs", fn_abs); a("floor", fn_flr); a("ceil", fn_cei); a("round", fn_rnd);
    a("mod", fn_mod); a("not", fn_not); a("and", fn_and); a("or", fn_or);
    a("min", fn_min); a("max", fn_max);
    // Lists
    a("list", fn_list); a("cons", fn_cons); a("append", fn_append); a("concat-list", fn_concat_list);
    a("push", fn_push); a("pop", fn_pop); a("reverse", fn_reverse);
    // Polymorphic
    a("length", fn_length); a("head", fn_head); a("tail", fn_tail); a("last", fn_last);
    a("empty?", fn_emptyp); a("getidx", fn_getidx); a("setidx", fn_setidx); a("slice", fn_slice); a("find", fn_find);
    // Vectors
    a("vec", fn_vec); a("range", fn_range); a("linspace", fn_linspace);
    a("zeros", fn_zeros); a("ones", fn_ones);
    a("rand", fn_rand); a("seed", fn_seed);
    a("sort", fn_sort); a("sum", fn_sum); a("prod", fn_prod); a("mean", fn_mean); a("dot", fn_dot);
    // Strings
    a("concat", fn_concat); a("split", fn_split); a("join", fn_join); a("format", fn_format);
    a("upper", fn_upper); a("lower", fn_lower); a("trim", fn_trim); a("chr", fn_chr); a("ord", fn_ord);
    // I/O
    a("print", fn_print); a("read", fn_read); a("write", fn_write);
    a("append-file", fn_appendf); a("exists?", fn_existsp); a("exec", fn_exec); a("input", fn_input); a("exit", fn_exit);
    // Meta
    a("type", fn_type); a("str", fn_str); a("sym", fn_sym); a("num", fn_num);
    a("copy", fn_copy); a("error", fn_error); a("assert", fn_assert); a("clock", fn_clock); a("load", fn_load);
    a("defined?", fn_definedp); a("vars", fn_vars);
    // Higher-order
    a("map", fn_map); a("filter", fn_filter); a("reduce", fn_reduce); a("each", fn_each);
}
// Break env <-> closure reference cycles so everything is released.
inline interp::~interp() {
    for (auto& w : tracked_envs) if (auto e = w.lock()) e->vars.clear();
    if (global) global->vars.clear();
}

// === load / run / repl =====
// Search order for a relative path: the directory of the file doing the
// loading, the current directory, each entry of MUSIL_PATH, ~/.musil, and
// <exe dir>/../lang (for installed layouts). A file is loaded once per
// interpreter, keyed by its canonical path, so mutual loads are safe.
inline void interp::load(const std::string& path) {
    fs::path p(path), resolved;
    auto try_path = [&](const fs::path& q) { std::error_code ec; if (resolved.empty() && !q.empty() && fs::is_regular_file(q, ec)) resolved = q; };
    if (p.is_absolute()) try_path(p);
    else {
        if (current_file) { fs::path base = fs::path(*current_file).parent_path(); if (!base.empty()) try_path(base / p); }
        try_path(p);
        for (auto& d : load_path) try_path(fs::path(d) / p);
    }
    if (resolved.empty()) err("load: not found: " + path);
    std::error_code ec;
    fs::path canon_p = fs::weakly_canonical(resolved, ec);
    std::string canon = (ec ? fs::absolute(resolved) : canon_p).generic_string();
    if (loaded_files.count(canon)) return;
    loaded_files.insert(canon);
    std::ifstream f(canon); if (!f) err("load: cannot open " + canon);
    std::stringstream ss; ss << f.rdbuf();
    auto sf = current_file; int sl = current_line;
    run(ss.str(), canon);
    current_file = sf; current_line = sl;
}

inline vptr interp::run(const std::string& src, const std::string& filename) {
    auto file = std::make_shared<std::string>(filename);
    parser p(src, file);
    auto prog = p.program();
    current_file = file;
    return eval(prog, global);
}

inline void interp::repl() {
    *out << "musil " << MUSIL_VERSION << " — :q to quit\n";
    std::string buffer; int depth = 0;
    while (true) {
        const char* prompt = buffer.empty() ? "> " : "  ";
        std::string line;
#ifdef HAVE_READLINE
        char* rp = readline(prompt); if (!rp) break;
        if (*rp) add_history(rp); line = rp; free(rp);
#else
        *out << prompt << std::flush;
        if (!std::getline(std::cin, line)) break;
#endif
        if (line == ":q" && buffer.empty()) break;
        bool in_str = false;
        for (size_t i=0; i<line.size(); i++) {
            char c = line[i];
            if (in_str) { if (c == '\\' && i+1 < line.size()) { i++; continue; } if (c == '"') in_str = false; continue; }
            if (c == '"') in_str = true;
            else if (c == '#') break;
            else if (c == '(' || c == '{') depth++;
            else if (c == ')' || c == '}') depth--;
        }
        buffer += line; buffer += '\n';
        if (depth <= 0) {
            try { auto r = run(buffer, "<repl>"); if (r && r->t != value::NIL) *out << str_of(r) << "\n"; }
            catch (exit_signal&) { break; }
            catch (const std::exception& ex) { *out << "error: " << ex.what() << "\n"; }
            call_stack.clear(); function_depth = 0; loop_depth = 0; stack_depth = 0;
            buffer.clear(); depth = 0;
        }
    }
}

} // namespace musil
