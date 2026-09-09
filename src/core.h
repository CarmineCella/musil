// core.h — the Musil language: reader, evaluator, core builtins.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
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

#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
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
#ifndef _WIN32
#include <sys/resource.h>
#endif

#define MUSIL_VERSION "0.6"

namespace musil {
namespace fs = std::filesystem;

struct Value; struct Env; struct Interp;
using vptr = std::shared_ptr<Value>;
using eptr = std::shared_ptr<Env>;
using vlist = std::vector<vptr>;
using varr = std::valarray<double>;
using sptr = std::shared_ptr<std::string>;
using op_t = vptr (*)(vlist&, Interp&);

// AST
struct Value {
    enum tag { NIL, NUM, STR, SYM, LIST, FN, OPAQUE } t = NIL;
    varr num; std::string s; vlist l;
    int line = 0; sptr file;
    op_t op = nullptr; int amin = 0, amax = -1;   // builtin: arity (amax < 0 = unbounded)
    std::vector<std::string> params;
    vptr body; eptr closure;
    std::shared_ptr<void> opaque; std::string opaque_tag;
};
inline const char* type_name(const vptr& v) {
    static const char* names[] = { "nil", "number", "string", "symbol", "list", "function", "opaque" };
    return v ? names[v->t] : "nil";
}
inline vptr v_nil()              { return std::make_shared<Value>(); }
inline vptr v_num(double d)      { auto v = v_nil(); v->t = Value::NUM; v->num = varr(d, 1); return v; }
inline vptr v_arr(varr a)        { auto v = v_nil(); v->t = Value::NUM; v->num = std::move(a); return v; }
inline vptr v_bool(bool b)       { return v_num(b ? 1.0 : 0.0); }
inline vptr v_str(std::string s) { auto v = v_nil(); v->t = Value::STR; v->s = std::move(s); return v; }
inline vptr v_sym(std::string s) { auto v = v_nil(); v->t = Value::SYM; v->s = std::move(s); return v; }
inline vptr v_list(vlist x, int ln=0, sptr f={}) { 
    auto v=v_nil(); v->t=Value::LIST; v->l=std::move(x); v->line=ln; v->file=std::move(f); return v; 
}
inline vptr v_op(op_t f, std::string name, int amin, int amax) { 
    auto v = v_nil(); v->t = Value::FN; v->op = f; v->s = std::move(name); v->amin = amin; v->amax = amax; return v; 
}
inline vptr v_opaque(std::string tag, std::shared_ptr<void> p) { 
    auto v = v_nil(); v->t = Value::OPAQUE; v->opaque_tag = std::move(tag); v->opaque = std::move(p); return v; 
}
struct Env {
    std::unordered_map<std::string, vptr> vars;
    eptr parent;
    Env(eptr p = nullptr) : parent(std::move(p)) {}
    vptr find(const std::string& k) {
        for (Env* w = this; w; w = w->parent.get()) {
            auto it = w->vars.find(k);
            if (it != w->vars.end()) return it->second;
        }
        return nullptr;
    }
    bool assign(const std::string& k, vptr v) {
        for (Env* w = this; w; w = w->parent.get()) {
            auto it = w->vars.find(k);
            if (it != w->vars.end()) { it->second = std::move(v); return true; }
        }
        return false;
    }
};

// helpers
struct Error : std::exception {
    std::string file; int line = 0; std::string msg;
    std::vector<std::string> trace;
    mutable std::string cached;
    Error(std::string f, int ln, std::string m) : file(std::move(f)), line(ln), msg(std::move(m)) {}
    const char* what() const noexcept override {
        if (cached.empty()) {
            cached = file + ":" + std::to_string(line) + ": " + msg;
            for (size_t i=0; i<trace.size(); ++i) cached += "\n  " + std::to_string(i+1) + "> " + trace[i];
        }
        return cached.c_str();
    }
};
struct Return_signal { vptr val; }; struct break_signal {}; struct continue_signal {};
struct Exit_signal { int code; };
inline bool truthy(const vptr& v) {
    if (!v || v->t == Value::NIL) return false;
    if (v->t == Value::NUM) { for (size_t i=0; i<v->num.size(); i++) if (v->num[i]==0.0) return false; return v->num.size() > 0; }
    if (v->t == Value::STR) return !v->s.empty();
    if (v->t == Value::LIST) return !v->l.empty();
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
    case Value::NIL: return "nil";
    case Value::NUM:
        if (v->num.size() == 1) { put_double(os, v->num[0]); return os.str(); }
        os << "(";
        for (size_t i=0; i<v->num.size(); i++) { if (i) os << " "; put_double(os, v->num[i]); }
        os << ")";
        return os.str();
    case Value::STR: case Value::SYM: return v->s;
    case Value::FN: return v->op ? "<builtin>" : "<fn>";
    case Value::LIST:
        os << "("; for (size_t i=0; i<v->l.size(); i++) { if (i) os << " "; os << str_of(v->l[i]); } os << ")";
        return os.str();
    case Value::OPAQUE: return "<opaque:" + v->opaque_tag + ">";
    }
    return "";
}
inline bool equal(const vptr& a, const vptr& b) {
    if (!a || !b) return (!a || a->t == Value::NIL) && (!b || b->t == Value::NIL);
    if (a->t != b->t) return false;
    switch (a->t) {
    case Value::NIL: return true;
    case Value::NUM: if (a->num.size() != b->num.size()) return false;
        for (size_t i=0; i<a->num.size(); i++) { if (a->num[i] != b->num[i]) return false; }
        return true;
    case Value::STR: case Value::SYM: return a->s == b->s;
    case Value::LIST: if (a->l.size() != b->l.size()) return false;
        for (size_t i=0; i<a->l.size(); i++) { if (!equal(a->l[i], b->l[i])) return false; }
        return true;
    case Value::FN: return a.get() == b.get() || (a->op && a->op == b->op);
    case Value::OPAQUE: return a->opaque.get() == b->opaque.get();
    }
    return false;
}

// parsing and lexing
struct Parser {
    std::string src; size_t pos = 0; int line = 1; sptr file;
    Parser(std::string s, sptr f) : src(std::move(s)), file(std::move(f)) {}
    bool eof() const { return pos >= src.size(); }
    char peek() const { return src[pos]; }
    [[noreturn]] void err(int ln, const std::string& m) { throw Error(file?*file:"<input>", ln, m); }
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
                if (lf->t != Value::LIST || !lf->l.empty()) out.push_back(lf);
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
        if (out.size() == 1 && out[0]->t != Value::SYM) return out[0];
        return v_list(std::move(out), ln, file);
    }
    vptr program() {
        vlist out = { v_sym("do") };
        while (true) {
            skip_h();
            if (eof()) break;
            if (peek() == '\n') { pos++; line++; continue; }
            vptr lf = read_line(0);
            if (lf->t != Value::LIST || !lf->l.empty()) out.push_back(lf);
        }
        return v_list(std::move(out), 1, file);
    }
};
struct Frame { vptr name; sptr file; int line; };   // call-stack entry; formatted lazily on error
struct Interp {
    eptr global;
    std::vector<Frame> call_stack;
    int stack_depth = 0, max_stack = 100000;   // depth cap (secondary; the byte budget below is the real guard)
    std::uintptr_t stack_base = 0;             // address of a local in the outermost run()
    size_t stack_budget = 0;                   // bytes of C++ stack eval may use; set in the constructor, hosts may lower it
    int function_depth = 0, loop_depth = 0;
    unsigned long eval_count = 0;
    std::function<void()> yield_fn;         // called every 1024 evals, for hosts that need to breathe
    std::vector<std::weak_ptr<Env>> tracked_envs;
    std::unordered_set<std::string> loaded_files;   // ran to completion; a second load is a no-op
    std::unordered_set<std::string> loading_files;  // currently running; a load from inside is a cycle and is skipped
    std::vector<std::string> load_path;     // extra directories searched by load (after the local ones)
    std::string home_path;                  // ~/.musil, searched last
    sptr current_file; int current_line = 0;
    std::mt19937_64 rng;
    std::ostream* out = &std::cout;
    const char* who = "";                   // name of the builtin being executed (for error messages)

    Interp();
    ~Interp();
    eptr make_env(eptr parent = nullptr) {
        auto e = std::make_shared<Env>(std::move(parent));
        tracked_envs.push_back(e);
        if (tracked_envs.size() >= 1024 && (tracked_envs.size() & 1023) == 0)
            tracked_envs.erase(std::remove_if(tracked_envs.begin(), tracked_envs.end(),
                [](const std::weak_ptr<Env>& w) { return w.expired(); }), tracked_envs.end());
        return e;
    }
    [[noreturn]] void err(const std::string& m) { throw Error(current_file?*current_file:"<input>", current_line, m); }
    void yield_check() { if (yield_fn && (++eval_count & 1023) == 0) yield_fn(); }
    void def(const std::string& name, op_t f, int amin, int amax) { global->vars[name] = v_op(f, name, amin, amax); }
    void def(const std::string& name, op_t f, int arity = 0) { def(name, f, arity, arity); }
    void def(const std::string& name, vptr v) { global->vars[name] = std::move(v); }
    vptr eval(vptr expr, eptr e);
    vptr call_fn(vptr fn, vlist args);
    vptr run(const std::string& src, const std::string& filename = "<input>");
    void load(const std::string& path);
    void repl();
    [[noreturn]] void bad(const std::string& m) { err(std::string(who) + ": " + m); }
    const varr& num(const vptr& v) {
        if (!v || v->t != Value::NUM) bad(std::string("expected number, got ") + type_name(v));
        return v->num;
    }
    double scalar(const vptr& v) {
        const varr& n = num(v);
        if (n.size() != 1) bad("expected scalar, got vector of size " + std::to_string(n.size()));
        return n[0];
    }
    long index(const vptr& v) {
        double d = scalar(v);
        if (d != std::floor(d)) bad("expected integer index, got " + str_of(v));
        return (long)d;
    }
    const std::string& str(const vptr& v) {
        if (!v || v->t != Value::STR) bad(std::string("expected string, got ") + type_name(v));
        return v->s;
    }
    vlist& list(const vptr& v) {
        if (!v || v->t != Value::LIST) bad(std::string("expected list, got ") + type_name(v));
        return v->l;
    }
    const vptr& fn(const vptr& v) {
        if (!v || v->t != Value::FN) bad(std::string("expected function, got ") + type_name(v));
        return v;
    }
    vptr call_op(const vptr& f, vlist& args) {
        if ((int)args.size() < f->amin || (f->amax >= 0 && (int)args.size() > f->amax)) {
            std::string want = f->amax < 0 ? "at least " + std::to_string(f->amin)
                             : f->amin == f->amax ? std::to_string(f->amin)
                             : std::to_string(f->amin) + "-" + std::to_string(f->amax);
            err(f->s + ": expected " + want + " argument" + (want == "1" ? "" : "s") + ", got " + std::to_string(args.size()));
        }
        struct guard { const char*& w; const char* saved; ~guard() { w = saved; } } g{who, who};
        who = f->s.c_str();
        return f->op(args, *this);
    }
};
template <class F>
inline varr bcast(const varr& a, const varr& b, F f, Interp& i) {
    if (a.size() == b.size()) { varr r(a.size()); for (size_t k=0; k<a.size(); k++) r[k] = f(a[k], b[k]); return r; }
    if (a.size() == 1) { varr r(b.size()); for (size_t k=0; k<b.size(); k++) r[k] = f(a[0], b[k]); return r; }
    if (b.size() == 1) { varr r(a.size()); for (size_t k=0; k<a.size(); k++) r[k] = f(a[k], b[0]); return r; }
    i.bad("size mismatch (" + std::to_string(a.size()) + " vs " + std::to_string(b.size()) + ")");
}
inline int op_prec(const std::string& o) {
    if (o == "||") return 1;
    if (o == "&&") return 2;
    if (o == "==" || o == "!=") return 3;
    if (o == "<" || o == ">" || o == "<=" || o == ">=") return 4;
    if (o == "+" || o == "-")  return 5;
    if (o == "*" || o == "/" || o == "%")  return 6;
    return 0;
}
inline vptr expr_parse(vlist& w, size_t& p, int min_p, Interp& i, eptr e);
inline vptr expr_atom(vlist& w, size_t& p, Interp& i, eptr e) {
    if (p >= w.size()) i.err("expr: unexpected end of expression");
    vptr a = w[p];
    if (a->t == Value::NUM) { p++; return a; }
    if (a->t == Value::SYM) {
        if (a->s == "-") { p++; vptr v = expr_atom(w, p, i, e); return v_arr(-i.num(v)); }
        if (op_prec(a->s)) i.err("expr: unexpected operator " + a->s);
        p++; auto v = e->find(a->s);
        if (!v) i.err("expr: undefined: " + a->s);
        return v;
    }
    if (a->t == Value::LIST) {
        p++;
        // (a op b ...) nested in an expr is a sub-expression; anything else is a call.
        vlist& sub = a->l;
        bool infix = sub.size() == 1 || (sub.size() >= 2 && sub[1]->t == Value::SYM && op_prec(sub[1]->s) > 0);
        if (!infix) return i.eval(a, e);
        size_t q = 0; vptr r = expr_parse(sub, q, 0, i, e);
        if (q < sub.size()) i.err("expr: unexpected " + str_of(sub[q]));
        return r;
    }
    if (a->t == Value::STR) { p++; return a; }
    i.err("expr: bad token " + str_of(a));
}
inline vptr expr_parse(vlist& w, size_t& p, int min_p, Interp& i, eptr e) {
    vptr lhs = expr_atom(w, p, i, e);
    while (p < w.size()) {
        if (w[p]->t != Value::SYM) i.err("expr: expected operator, got " + str_of(w[p]));
        int prec = op_prec(w[p]->s);
        if (prec == 0) i.err("expr: unknown operator " + w[p]->s);
        if (prec < min_p) break;
        std::string op = w[p++]->s;
        vptr rhs = expr_parse(w, p, prec + 1, i, e);
        if (op == "&&") { lhs = v_bool(truthy(lhs) && truthy(rhs)); continue; }
        if (op == "||") { lhs = v_bool(truthy(lhs) || truthy(rhs)); continue; }
        if (op == "==") { if (lhs->t != Value::NUM || rhs->t != Value::NUM) { lhs = v_bool(equal(lhs, rhs)); continue; } }
        if (op == "!=") { if (lhs->t != Value::NUM || rhs->t != Value::NUM) { lhs = v_bool(!equal(lhs, rhs)); continue; } }
        const varr& L = i.num(lhs); const varr& R = i.num(rhs);
        auto bo = [&](auto f) { return v_arr(bcast(L, R, f, i)); };
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

// evaluation
inline vptr fn_quote   (vlist&, Interp&) { return v_nil(); }
inline vptr fn_do      (vlist&, Interp&) { return v_nil(); }
inline vptr fn_if      (vlist&, Interp&) { return v_nil(); }
inline vptr fn_while   (vlist&, Interp&) { return v_nil(); }
inline vptr fn_for     (vlist&, Interp&) { return v_nil(); }
inline vptr fn_var     (vlist&, Interp&) { return v_nil(); }
inline vptr fn_function(vlist&, Interp&) { return v_nil(); }
inline vptr fn_return  (vlist&, Interp&) { return v_nil(); }
inline vptr fn_break   (vlist&, Interp&) { return v_nil(); }
inline vptr fn_continue(vlist&, Interp&) { return v_nil(); }
inline vptr fn_try     (vlist&, Interp&) { return v_nil(); }
inline vptr fn_expr    (vlist&, Interp&) { return v_nil(); }
inline vptr fn_eval    (vlist&, Interp&) { return v_nil(); }
inline vptr fn_apply   (vlist&, Interp&) { return v_nil(); }
inline vptr make_partial(Interp& i, const vptr& fn, const vlist& args) {
    auto bound = i.make_env(fn->closure);
    for (size_t k=0; k<args.size(); k++) bound->vars[fn->params[k]] = args[k];
    auto curr = v_nil(); curr->t = Value::FN; curr->closure = bound; curr->body = fn->body;
    curr->params = std::vector<std::string>(fn->params.begin() + args.size(), fn->params.end());
    return curr;
}
inline bool is_form(const vptr& v, const char* name) { return v && v->t == Value::LIST && !v->l.empty() && v->l[0]->t == Value::SYM && v->l[0]->s == name; }
inline bool ends_in_return(const vptr& v) {
    if (is_form(v, "return")) return true;
    if (is_form(v, "do")) return v->l.size() > 1 && ends_in_return(v->l.back());
    if (is_form(v, "if")) return v->l.size() == 4 && ends_in_return(v->l[2]) && ends_in_return(v->l[3]);
    return false;
}
inline vptr hoist_returns(const vptr& v) {
    if (!v || v->t != Value::LIST || v->l.empty()) return v;
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
inline std::string frame_label(const Frame& f) {
    return (f.name && f.name->t == Value::SYM ? f.name->s : std::string("<anon>")) + "() at " +
        (f.file ? *f.file : "?") + ":" + std::to_string(f.line);
}
inline vptr Interp::eval(vptr expr, eptr e) {
    // frame sizes differ between compilers and thread stacks differ between hosts.
    { char probe; auto here = reinterpret_cast<std::uintptr_t>(&probe);
      if (stack_base && (here > stack_base ? here - stack_base : stack_base - here) > stack_budget) err("stack overflow (recursion too deep)"); }
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
        if (expr->t != Value::SYM && expr->t != Value::LIST) { cleanup(); return expr; }
        if (expr->t == Value::SYM) {
            auto v = e->find(expr->s);
            if (!v) err("undefined: " + expr->s);
            cleanup(); return v;
        }
        if (expr->line) current_line = expr->line;
        if (expr->file) current_file = expr->file;
        auto& l = expr->l;
        if (l.empty()) { cleanup(); return v_nil(); }
        if (l.size() == 1 && l[0]->t == Value::LIST) { expr = l[0]; continue; }
        vptr head;
        if (l[0]->t == Value::SYM) {
            head = e->find(l[0]->s);
            if (!head) err("undefined: " + l[0]->s);
        } else if (l[0]->t == Value::FN) head = l[0];
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
            if (l[1]->t != Value::SYM) err("var: name must be a symbol, got " + str_of(l[1]));
            vptr v = eval(l[2], e);
            if (!e->assign(l[1]->s, v)) e->vars[l[1]->s] = v;
            cleanup(); return v;
        }
        if (op == fn_function) {
            bool named; size_t pi, bi;
            if (l.size() == 4 && l[1]->t == Value::SYM && l[2]->t == Value::LIST) { named = true;  pi = 2; bi = 3; }
            else if (l.size() == 3 && l[1]->t == Value::LIST)                     { named = false; pi = 1; bi = 2; }
            else err("function: expected (function [name] (params) body)");
            auto fn = v_nil(); fn->t = Value::FN; fn->closure = e; fn->body = hoist_returns(l[bi]);
            for (auto& p : l[pi]->l) {
                if (p->t != Value::SYM) err("function: parameter must be a symbol, got " + str_of(p));
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
            throw Return_signal{ l.size() > 1 ? eval(l[1], e) : v_nil() };
        }
        if (op == fn_break)    { if (loop_depth == 0) err("break outside loop"); throw break_signal{}; }
        if (op == fn_continue) { if (loop_depth == 0) err("continue outside loop"); throw continue_signal{}; }
        if (op == fn_try) {
            if (l.size() != 5 || l[2]->t != Value::SYM || l[2]->s != "catch" || l[3]->t != Value::SYM)
                err("try: expected (try body catch name handler)");
            size_t depth = call_stack.size(); int fd = function_depth, ld = loop_depth;
            try { vptr r = eval(l[1], e); cleanup(); return r; }
            catch (Error& ex) {
                call_stack.resize(depth); function_depth = fd; loop_depth = ld;
                auto ne = make_env(e); ne->vars[l[3]->s] = v_str(ex.what());
                vptr r = eval(l[4], ne); cleanup(); return r;
            }
        }
        if (op == fn_expr) {
            if (l.size() != 2 || l[1]->t != Value::LIST) err("expr: expected (expr (a op b ...))");
            const char* saved_who = who; who = "expr";
            size_t p = 0; vptr r = expr_parse(l[1]->l, p, 0, *this, e); who = saved_who; cleanup(); return r;
        }
        if (op == fn_eval) {
            if (l.size() != 2) err("eval: expected (eval form)");
            expr = eval(l[1], e); continue;
        }
        if (op == fn_apply) {
            if (l.size() != 3) err("apply: expected (apply fn list)");
            vptr fv = eval(l[1], e), lv = eval(l[2], e);
            if (fv->t != Value::FN) err("apply: expected function, got " + std::string(type_name(fv)));
            if (lv->t != Value::LIST) err("apply: expected list, got " + std::string(type_name(lv)));
            vlist nl = { fv };
            for (auto& x : lv->l) nl.push_back(v_list({ v_sym("quote"), x }));
            expr = v_list(std::move(nl), expr->line, expr->file);
            continue;
        }

        // function call
        if (head->t != Value::FN) err("not callable: " + str_of(l[0]) + " (" + type_name(head) + ")");
        vlist args; args.reserve(l.size() - 1);
        for (size_t i=1; i<l.size(); i++) args.push_back(eval(l[i], e));
        if (head->op) { vptr r = call_op(head, args); cleanup(); return r; }   // op may throw: cleanup after
        // currying: too few args => return partial application
        if (args.size() < head->params.size()) { vptr r = make_partial(*this, head, args); cleanup(); return r; }
        // over-application: call with the params it takes, apply the result to the rest
        if (args.size() > head->params.size()) {
            vlist first(args.begin(), args.begin() + head->params.size());
            vptr r = call_fn(head, std::move(first));
            if (r->t != Value::FN)
                err((l[0]->t == Value::SYM ? l[0]->s : std::string("<anon>")) + ": too many arguments (expected " +
                    std::to_string(head->params.size()) + ", got " + std::to_string(args.size()) + ")");
            vlist nl = { r };
            for (size_t i=head->params.size(); i<args.size(); i++) nl.push_back(v_list({ v_sym("quote"), args[i] }));
            expr = v_list(std::move(nl), expr->line, expr->file);
            continue;
        }
        // user function call — in-place TCO; replace top of call_stack on each
        // tail-merged call so the trace reflects current location, not history.
        Frame fr{ l[0], current_file, current_line };
        if (entered_fn) call_stack.back() = fr;
        else { call_stack.push_back(fr); ++function_depth; loop_depth = 0; entered_fn = true; }
        auto ne = make_env(head->closure);
        for (size_t i=0; i<head->params.size(); i++) ne->vars[head->params[i]] = args[i];
        expr = head->body; e = ne;
        // continue eval loop with new expr/env — no new C++ frame.
    }
} catch (Return_signal& rs) {
    if (entered_fn) { cleanup(); return rs.val; }
    throw;  // propagate up to the eval frame that entered the function
  }
  catch (Error& fe) {
    if (fe.trace.empty() && !call_stack.empty()) {
        // innermost first. Deep recursions are abbreviated: 12 innermost, a count, 4 outermost.
        size_t n = call_stack.size(), head = 12, tail = 4;
        for (size_t k = 0; k < n; k++) {
            if (n > head + tail + 1 && k == head) { fe.trace.push_back("... " + std::to_string(n - head - tail) + " more frames ..."); k = n - tail - 1; continue; }
            fe.trace.push_back(frame_label(call_stack[n - 1 - k]));
        }
    }
    cleanup(); throw;
  }
  catch (...) { cleanup(); throw; }
}
inline vptr Interp::call_fn(vptr f, vlist args) {
    if (f->t != Value::FN) err("call: expected function, got " + std::string(type_name(f)));
    if (f->op == fn_eval) { if (args.size() != 1) err("eval: expected 1 argument"); return eval(args[0], global); }
    if (f->op == fn_apply) {
        if (args.size() != 2 || args[0]->t != Value::FN || args[1]->t != Value::LIST) err("apply: expected (apply fn list)");
        return call_fn(args[0], args[1]->l);
    }
    if (f->op) return call_op(f, args);
    if (args.size() < f->params.size()) return make_partial(*this, f, args);
    if (args.size() > f->params.size()) {
        vlist first(args.begin(), args.begin() + f->params.size());
        vptr r = call_fn(f, std::move(first));
        if (r->t != Value::FN) err("too many arguments (expected " + std::to_string(f->params.size()) + ", got " + std::to_string(args.size()) + ")");
        return call_fn(r, vlist(args.begin() + f->params.size(), args.end()));
    }
    auto ne = make_env(f->closure);
    for (size_t i=0; i<f->params.size(); i++) ne->vars[f->params[i]] = args[i];
    call_stack.push_back(Frame{ nullptr, current_file, current_line });
    ++function_depth; int sl = loop_depth; loop_depth = 0;
    vptr r;
    try { r = eval(f->body, ne); } catch (Return_signal& rs) { r = rs.val; }
    catch (...) { call_stack.pop_back(); --function_depth; loop_depth = sl; throw; }
    call_stack.pop_back(); --function_depth; loop_depth = sl;
    return r;
}

// builtins
#define BINOP(name, sym, init, unary) \
    inline vptr name(vlist& a, Interp& i) { \
        if (a.empty()) return v_num(init); \
        varr r = i.num(a[0]); \
        auto f = [](double x, double y){ return x sym y; }; \
        if (a.size() == 1 && unary) r = bcast(varr(init,1), r, f, i); \
        else for (size_t k=1; k<a.size(); k++) r = bcast(r, i.num(a[k]), f, i); \
        return v_arr(std::move(r)); }
BINOP(fn_add, +, 0.0, false) BINOP(fn_sub, -, 0.0, true)
BINOP(fn_mul, *, 1.0, false) BINOP(fn_div, /, 1.0, true)
#define CMP(name, sym) inline vptr name(vlist& a, Interp& i) { return v_arr(bcast(i.num(a[0]), i.num(a[1]), [](double x, double y){return x sym y ? 1.0 : 0.0;}, i)); }
CMP(fn_lt, <) CMP(fn_gt, >) CMP(fn_le, <=) CMP(fn_ge, >=)
inline vptr fn_eq(vlist& a, Interp& i) {
    if (a[0]->t == Value::NUM && a[1]->t == Value::NUM) return v_arr(bcast(a[0]->num, a[1]->num, [](double x, double y){return x == y ? 1.0 : 0.0;}, i));
    return v_bool(equal(a[0], a[1])); }
inline vptr fn_ne(vlist& a, Interp& i) {
    if (a[0]->t == Value::NUM && a[1]->t == Value::NUM) return v_arr(bcast(a[0]->num, a[1]->num, [](double x, double y){return x != y ? 1.0 : 0.0;}, i));
    return v_bool(!equal(a[0], a[1])); }
inline vptr fn_equalp(vlist& a, Interp&) { return v_bool(equal(a[0], a[1])); }
#define UN(name, f)  inline vptr name(vlist& a, Interp& i) { varr r=i.num(a[0]); for (size_t k=0; k<r.size(); k++) r[k]=f(r[k]); return v_arr(std::move(r)); }
#define BIN(name, f) inline vptr name(vlist& a, Interp& i) { return v_arr(bcast(i.num(a[0]), i.num(a[1]), [](double x, double y){ return f(x, y); }, i)); }
UN(fn_sin, std::sin) UN(fn_cos, std::cos) UN(fn_tan, std::tan) UN(fn_asin, std::asin) UN(fn_acos, std::acos) UN(fn_atan, std::atan)
UN(fn_sqrt, std::sqrt) UN(fn_exp, std::exp) UN(fn_log, std::log) UN(fn_log2, std::log2) UN(fn_log10, std::log10)
UN(fn_abs, std::fabs) UN(fn_flr, std::floor) UN(fn_cei, std::ceil) UN(fn_rnd, std::round)
BIN(fn_mod, std::fmod) BIN(fn_pow, std::pow) BIN(fn_atan2, std::atan2)
inline vptr fn_not(vlist& a, Interp& i) { return v_bool(!truthy(a[0])); }
inline vptr fn_and(vlist& a, Interp&) { for (auto& x : a) if (!truthy(x)) return v_bool(false); return v_bool(true); }
inline vptr fn_or (vlist& a, Interp&) { for (auto& x : a) if (truthy(x))  return v_bool(true); return v_bool(false); }
// min/max: one vector argument => reduction; several arguments => elementwise.
inline vptr fn_min(vlist& a, Interp& i) {
    if (a.size() == 1) { const varr& v = i.num(a[0]); if (v.size() == 0) i.err("min: empty vector"); return v_num(v.min()); }
    varr r = i.num(a[0]);
    for (size_t k=1; k<a.size(); k++) r = bcast(r, i.num(a[k]), [](double x, double y){return x < y ? x : y;}, i);
    return v_arr(std::move(r));
}
inline vptr fn_max(vlist& a, Interp& i) {
    if (a.size() == 1) { const varr& v = i.num(a[0]); if (v.size() == 0) i.err("max: empty vector"); return v_num(v.max()); }
    varr r = i.num(a[0]);
    for (size_t k=1; k<a.size(); k++) r = bcast(r, i.num(a[k]), [](double x, double y){return x > y ? x : y;}, i);
    return v_arr(std::move(r));
}
inline vptr fn_list(vlist& a, Interp&) { return v_list(a); }
inline vptr fn_cons(vlist& a, Interp& i) { vlist& t = i.list(a[1]); vlist r; r.reserve(t.size()+1); r.push_back(a[0]); for (auto& x : t) r.push_back(x); return v_list(std::move(r)); }
inline vptr fn_append(vlist& a, Interp& i) { vlist r = i.list(a[0]); for (size_t k=1; k<a.size(); k++) r.push_back(a[k]); return v_list(std::move(r)); }
inline vptr fn_push(vlist& a, Interp& i) { i.list(a[0]).push_back(a[1]); return a[0]; }
inline vptr fn_pop(vlist& a, Interp& i) { vlist& l = i.list(a[0]); if (l.empty()) i.err("pop: empty list"); vptr r = l.back(); l.pop_back(); return r; }
inline vptr fn_length(vlist& a, Interp& i) { auto& v=a[0];
    if (v->t==Value::LIST) return v_num((double)v->l.size());
    if (v->t==Value::NUM)  return v_num((double)v->num.size());
    if (v->t==Value::STR)  return v_num((double)v->s.size());
    i.err(std::string("length: expected list, number or string, got ") + type_name(v)); }
inline vptr fn_head(vlist& a, Interp& i) { auto& v=a[0];
    if (v->t==Value::LIST) { if (v->l.empty()) i.err("head: empty list"); return v->l[0]; }
    if (v->t==Value::NUM)  { if (v->num.size()==0) i.err("head: empty vector"); return v_num(v->num[0]); }
    i.err(std::string("head: expected list or number, got ") + type_name(v)); }
inline vptr fn_tail(vlist& a, Interp& i) { auto& v=a[0];
    if (v->t==Value::LIST) { if (v->l.empty()) i.err("tail: empty list"); return v_list(vlist(v->l.begin()+1, v->l.end())); }
    if (v->t==Value::NUM)  { if (v->num.size()==0) i.err("tail: empty vector"); varr r=v->num[std::slice(1, v->num.size()-1, 1)]; return v_arr(std::move(r)); }
    i.err(std::string("tail: expected list or number, got ") + type_name(v)); }
inline vptr fn_emptyp(vlist& a, Interp& i) { auto& v=a[0];
    return v_bool(v->t==Value::LIST?v->l.empty():v->t==Value::NUM?v->num.size()==0:v->t==Value::STR?v->s.empty():v->t==Value::NIL); }
inline size_t checked_index(Interp& i, const vptr& idx, size_t size) {
    long k = i.index(idx);
    if (k < 0) k += (long)size;                       // negative indices count from the end
    if (k < 0 || (size_t)k >= size) i.bad("index " + str_of(idx) + " out of range (size " + std::to_string(size) + ")");
    return (size_t)k;
}
inline vptr fn_getidx(vlist& a, Interp& i) {
    auto& v=a[0];
    if (v->t==Value::NUM)  return v_num(v->num[checked_index(i, a[1], v->num.size())]);
    if (v->t==Value::LIST) return v->l[checked_index(i, a[1], v->l.size())];
    if (v->t==Value::STR)  return v_str(std::string(1, v->s[checked_index(i, a[1], v->s.size())]));
    i.err(std::string("getidx: expected list, number or string, got ") + type_name(v)); }
inline vptr fn_setidx(vlist& a, Interp& i) {
    auto& v=a[0];
    if (v->t==Value::NUM)  { v->num[checked_index(i, a[1], v->num.size())] = i.scalar(a[2]); return v; }
    if (v->t==Value::LIST) { v->l[checked_index(i, a[1], v->l.size())] = a[2]; return v; }
    i.err(std::string("setidx: expected list or number, got ") + type_name(v)); }
inline vptr fn_vec(vlist& a, Interp& i) {
    if (a.size()==1 && a[0]->t==Value::LIST) {
        varr r(a[0]->l.size());
        for (size_t k=0; k<a[0]->l.size(); k++) r[k]=i.scalar(a[0]->l[k]);
        return v_arr(std::move(r));
    }
    size_t n = 0; for (auto& x : a) n += i.num(x).size();
    varr r(n); size_t p = 0;
    for (auto& x : a) for (size_t k=0; k<x->num.size(); k++) r[p++] = x->num[k];
    return v_arr(std::move(r));
}
inline vptr fn_sum(vlist& a, Interp& i) {
    if (a[0]->t==Value::NUM)  { double s=0; for (size_t k=0; k<a[0]->num.size(); k++) s+=a[0]->num[k]; return v_num(s); }
    if (a[0]->t==Value::LIST) { double s=0; for (auto& x : a[0]->l) s+=i.scalar(x); return v_num(s); }
    i.err(std::string("sum: expected number or list, got ") + type_name(a[0]));
}
inline vptr fn_print(vlist& a, Interp& i) { for (size_t k=0; k<a.size(); k++) { if (k) *i.out<<" "; *i.out<<str_of(a[k]); } *i.out<<"\n"; return v_nil(); }
inline vptr fn_exit(vlist& a, Interp& i) { throw Exit_signal{ a.empty() ? 0 : (int)i.scalar(a[0]) }; }
inline vptr fn_type(vlist& a, Interp& i) {
    if (a[0]->t==Value::NUM) return v_str(a[0]->num.size()==1 ? "scalar" : "vec");
    if (a[0]->t==Value::OPAQUE) return v_str("opaque:" + a[0]->opaque_tag);
    return v_str(type_name(a[0]));
}
inline vptr fn_str(vlist& a, Interp& i) { return v_str(str_of(a[0])); }
inline vptr fn_sym(vlist& a, Interp& i) { return v_sym(i.str(a[0])); }
inline vptr fn_num(vlist& a, Interp& i) {
    if (a[0]->t==Value::NUM) return a[0];
    const std::string& s=i.str(a[0]);
    char* e=nullptr; double d=std::strtod(s.c_str(), &e);
    if (!e || *e!='\0' || e==s.c_str()) i.err("num: not a number: " + s);
    return v_num(d);
}
// copy is shallow
inline vptr fn_copy(vlist& a, Interp& i) {
    auto& s=a[0]; auto v=v_nil();
    v->t=s->t; v->num=s->num; v->s=s->s; v->l=s->l;
    v->op=s->op; v->params=s->params; v->body=s->body; v->closure=s->closure;
    v->opaque=s->opaque; v->opaque_tag=s->opaque_tag;
    return v;
}
inline vptr fn_error(vlist& a, Interp& i) { std::string m; for (auto& x : a) m+=str_of(x); i.err(m.empty() ? "error" : m); }
inline vptr fn_clock(vlist&, Interp&) { return v_num(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
inline vptr fn_load(vlist& a, Interp& i) { i.load(i.str(a[0])); return v_nil(); }
inline vptr fn_definedp(vlist& a, Interp& i) { const std::string& n = a[0]->t==Value::SYM ? a[0]->s : i.str(a[0]); return v_bool(i.global->find(n) != nullptr); }
inline vptr fn_vars(vlist&, Interp& i) { std::vector<std::string> names; for (auto& kv : i.global->vars) names.push_back(kv.first); std::sort(names.begin(), names.end()); vlist out; for (auto& n : names) out.push_back(v_sym(n)); return v_list(std::move(out)); }

// constructor/destructor, methods
inline Interp::Interp() {
    rng.seed((uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    size_t limit = 1u << 20;   // 1 MB: the Windows default
#ifndef _WIN32
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) limit = (size_t)rl.rlim_cur;
    else limit = 8u << 20;
    if (limit > (64u << 20)) limit = 64u << 20;
#endif
    stack_budget = limit / 4 * 3;
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
    if (home) home_path = (fs::path(home) / ".musil").string();
    auto a = [&](const char* n, op_t f, int lo = 0, int hi = 0) { def(n, f, lo, hi); };
    const int N = -1;   // unbounded
    // Constants
    def("nil", v_nil()); def("true", v_num(1)); def("false", v_num(0));
    def("pi", v_num(3.14159265358979323846)); def("inf", v_num(INFINITY));
    def("version", v_str(MUSIL_VERSION));
    // Special forms (arity checked in eval)
    a("quote", fn_quote); a("do", fn_do); a("if", fn_if); a("while", fn_while); a("for", fn_for); a("var", fn_var);
    a("function", fn_function); a("return", fn_return); a("break", fn_break); a("continue", fn_continue);
    a("try", fn_try); a("expr", fn_expr); a("eval", fn_eval); a("apply", fn_apply);
    // Arithmetic                                   name        fn        min max
    a("+", fn_add, 0, N); a("-", fn_sub, 0, N); a("*", fn_mul, 0, N); a("/", fn_div, 0, N);
    a("<", fn_lt, 2, 2); a(">", fn_gt, 2, 2); a("<=", fn_le, 2, 2); a(">=", fn_ge, 2, 2);
    a("==", fn_eq, 2, 2); a("!=", fn_ne, 2, 2); a("equal?", fn_equalp, 2, 2);
    for (auto& u : { std::pair<const char*, op_t>{"sin", fn_sin}, {"cos", fn_cos}, {"tan", fn_tan}, {"asin", fn_asin}, {"acos", fn_acos}, {"atan", fn_atan},
                     {"sqrt", fn_sqrt}, {"exp", fn_exp}, {"log", fn_log}, {"log2", fn_log2}, {"log10", fn_log10},
                     {"abs", fn_abs}, {"floor", fn_flr}, {"ceil", fn_cei}, {"round", fn_rnd}, {"not", fn_not} })
        a(u.first, u.second, 1, 1);
    a("atan2", fn_atan2, 2, 2); a("pow", fn_pow, 2, 2); a("mod", fn_mod, 2, 2);
    a("and", fn_and, 0, N); a("or", fn_or, 0, N); a("min", fn_min, 1, N); a("max", fn_max, 1, N);
    // Lists and vectors
    a("list", fn_list, 0, N); a("cons", fn_cons, 2, 2); a("append", fn_append, 1, N); a("push", fn_push, 2, 2); a("pop", fn_pop, 1, 1);
    a("length", fn_length, 1, 1); a("head", fn_head, 1, 1); a("tail", fn_tail, 1, 1); a("empty?", fn_emptyp, 1, 1);
    a("getidx", fn_getidx, 2, 2); a("setidx", fn_setidx, 3, 3); a("vec", fn_vec, 0, N); a("sum", fn_sum, 1, 1);
    // Meta and I/O
    a("print", fn_print, 0, N); a("type", fn_type, 1, 1); a("str", fn_str, 1, 1); a("sym", fn_sym, 1, 1); a("num", fn_num, 1, 1);
    a("copy", fn_copy, 1, 1); a("error", fn_error, 0, N); a("clock", fn_clock, 0, 0); a("load", fn_load, 1, 1);
    a("defined?", fn_definedp, 1, 1); a("vars", fn_vars, 0, 0); a("exit", fn_exit, 0, 1);
}
inline Interp::~Interp() {
    for (auto& w : tracked_envs) if (auto e = w.lock()) e->vars.clear();
    if (global) global->vars.clear();
}
// load: search order for a relative path:
//   1. the directory of the file doing the loading
//   2. the current directory
//   3. each entry of load_path (MUSIL_PATH, plus whatever the host adds)
//   4. ~/.musil
inline void Interp::load(const std::string& path) {
    fs::path p(path), resolved;
    auto try_path = [&](const fs::path& q) { std::error_code ec; if (resolved.empty() && !q.empty() && fs::is_regular_file(q, ec)) resolved = q; };
    if (p.is_absolute()) try_path(p);
    else {
        if (current_file) { fs::path base = fs::path(*current_file).parent_path(); if (!base.empty()) try_path(base / p); }
        try_path(p);
        for (auto& d : load_path) try_path(fs::path(d) / p);
        if (!home_path.empty()) try_path(fs::path(home_path) / p);
    }
    if (resolved.empty()) err("load: not found: " + path);
    std::error_code ec;
    fs::path canon_p = fs::weakly_canonical(resolved, ec);
    std::string canon = (ec ? fs::absolute(resolved) : canon_p).generic_string();
    if (loaded_files.count(canon) || loading_files.count(canon)) return;
    std::ifstream f(canon); if (!f) err("load: cannot open " + canon);
    std::stringstream ss; ss << f.rdbuf();
    auto sf = current_file; int sl = current_line;
    loading_files.insert(canon);
    struct guard { Interp& i; std::string c; sptr f; int l; bool ok = false;
        ~guard() { i.loading_files.erase(c); if (ok) i.loaded_files.insert(c); i.current_file = f; i.current_line = l; } } g{*this, canon, sf, sl};
    run(ss.str(), canon);
    g.ok = true;   // a module that fails is not cached: the next load runs it again and reports the error again
}
inline vptr Interp::run(const std::string& src, const std::string& filename) {
    auto file = std::make_shared<std::string>(filename);
    Parser p(src, file);
    auto prog = p.program();
    current_file = file;
    char here;
    if (stack_depth == 0) stack_base = reinterpret_cast<std::uintptr_t>(&here);   // outermost run on this thread
    return eval(prog, global);
}
inline void Interp::repl() {
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
            try { auto r = run(buffer, "<repl>"); if (r && r->t != Value::NIL) *out << str_of(r) << "\n"; }
            catch (Exit_signal&) { break; }
            catch (const std::exception& ex) { *out << "error: " << ex.what() << "\n"; }
            call_stack.clear(); function_depth = 0; loop_depth = 0; stack_depth = 0;
            buffer.clear(); depth = 0;
        }
    }
}

} // namespace musil

// eof

