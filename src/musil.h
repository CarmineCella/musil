// musil.h 
//

#ifndef MUSIL_H
#define MUSIL_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <functional>
#include <memory>
#include <algorithm>
#include <valarray>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <stdexcept>

struct Error {
    std::string file;
    int line;
    std::string msg;
    std::vector<std::string> trace;   // proc call stack at point of error
};
struct Proc;
using NumVal   = std::valarray<double>;
struct Array;
struct Proc;                              // forward declaration
using ArrayPtr = std::shared_ptr<Array>;
using ProcVal  = std::shared_ptr<Proc>;
using Value    = std::variant<NumVal, std::string, ArrayPtr, ProcVal>;
struct Array { std::vector<Value> elems; };

inline double nv_scalar(const NumVal& v) { return v[0]; }
inline bool   nv_is_scalar(const NumVal& v) { return v.size() == 1; }
std::string to_str(const Value& v);   // forward declaration — defined after Proc
double to_bool(const Value& v);       // forward declaration — defined after Proc
bool values_equal(const Value& a, const Value& b);  // forward declaration

enum TK {
    NUM, STR, IDENT,
    VAR, PROC, WHILE, FOR, IN, IF, ELSE, RETURN, PRINT, BREAK,
    AND, OR, NOT,
    ASSIGN, PLUS, MINUS, STAR, SLASH,
    LT, GT, LE, GE, EQ, NEQ,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, COMMA, END
};
struct Token { TK type; std::string val; int line = 1; };
std::vector<Token> lex(const std::string& src, const std::string& filename = "<stdin>") {
    std::vector<Token> toks;
    size_t i = 0, n = src.size(); int line = 1;
    while (i < n) {
        if (src[i] == '\n') { line++; i++; continue; }
        if (isspace((unsigned char)src[i])) { i++; continue; }
        if (src[i] == '#') { while (i < n && src[i] != '\n') i++; continue; }
        if (src[i] == '"') {
            std::string s; i++;
            while (i < n && src[i] != '"') {
                if (src[i] == '\\' && i+1 < n) {
                    switch (src[++i]) {
                        case 'n': s+='\n'; break; case 't': s+='\t'; break;
                        case '\\':s+='\\'; break; case '"': s+='"';  break;
                        default:  s+='\\'; s+=src[i]; break;
                    }
                } else { s += src[i]; }
                i++;
            }
            toks.push_back({STR, s, line}); i++; continue;
        }
        if (isdigit((unsigned char)src[i]) ||
                (src[i] == '.' && i+1 < n && isdigit((unsigned char)src[i+1]))) {
            std::string s; bool has_dot = false;
            while (i < n && (isdigit((unsigned char)src[i]) || src[i] == '.')) {
                if (src[i] == '.') {
                    if (has_dot) throw Error{filename, line, "malformed number '" + s + ".'", {}};
                    has_dot = true;
                }
                s += src[i++];
            }
            // optional exponent: e/E followed by optional +/- and digits  (e.g. 1e-10, 2.5E3)
            if (i < n && (src[i] == 'e' || src[i] == 'E')) {
                s += src[i++];
                if (i < n && (src[i] == '+' || src[i] == '-')) s += src[i++];
                if (i >= n || !isdigit((unsigned char)src[i]))
                    throw Error{filename, line, "malformed number '" + s + "'"  , {}};
                while (i < n && isdigit((unsigned char)src[i])) s += src[i++];
            }
            toks.push_back({NUM, s, line}); continue;
        }
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            std::string s;
            while (i < n && (isalnum((unsigned char)src[i]) || src[i] == '_')) s += src[i++];
            TK t = IDENT;
            if      (s=="var")    t=VAR;   else if (s=="proc")   t=PROC;
            else if (s=="while")  t=WHILE; else if (s=="for")    t=FOR;
            else if (s=="in")     t=IN;    else if (s=="if")     t=IF;
            else if (s=="else")   t=ELSE;  else if (s=="return") t=RETURN;
            else if (s=="print")  t=PRINT; else if (s=="break")  t=BREAK;
            else if (s=="and")    t=AND;   else if (s=="or")     t=OR;
            else if (s=="not")    t=NOT;
            toks.push_back({t, s, line}); continue;
        }
        if (i+1 < n) {
            char a=src[i], b=src[i+1];
            if (a=='<'&&b=='='){toks.push_back({LE,"<=",line}); i+=2; continue;}
            if (a=='>'&&b=='='){toks.push_back({GE,">=",line}); i+=2; continue;}
            if (a=='='&&b=='='){toks.push_back({EQ,"==",line}); i+=2; continue;}
            if (a=='!'&&b=='='){toks.push_back({NEQ,"!=",line}); i+=2; continue;}
        }
        static const std::string ops = "=+-*/<>(),{}[]";
        static const TK opt[] = {ASSIGN,PLUS,MINUS,STAR,SLASH,LT,GT,
                                  LPAREN,RPAREN,COMMA,LBRACE,RBRACE,LBRACKET,RBRACKET};
        size_t p = ops.find(src[i]);
        if (p != std::string::npos) { toks.push_back({opt[p], std::string(1,src[i]), line}); i++; continue; }
        throw Error{filename, line, std::string("unknown character '") + src[i] + "'", {}};
    }
    toks.push_back({END, "", line}); return toks;
}

struct ReturnSignal { Value val; };
struct BreakSignal  {};
struct Proc {
    std::vector<std::string> params;
    std::vector<Token>       body;
};

std::string to_str(const Value& v) {
    if (auto* nv = std::get_if<NumVal>(&v)) {
        if (nv->size() == 1) {
            char buf[32]; snprintf(buf, sizeof(buf), "%.15g", (*nv)[0]); return buf;
        }
        std::string r = "<";
        for (size_t i = 0; i < nv->size(); i++) {
            if (i) r += " ";
            char buf[32]; snprintf(buf, sizeof(buf), "%.15g", (*nv)[i]); r += buf;
        }
        return r + ">";
    }
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    if (auto* p = std::get_if<ProcVal>(&v)) {
        std::string r = "<proc(";
        for (size_t i = 0; i < (*p)->params.size(); i++) { if (i) r += ", "; r += (*p)->params[i]; }
        return r + ")>";
    }
    const auto& el = std::get<ArrayPtr>(v)->elems;
    std::string r = "[";
    for (size_t i = 0; i < el.size(); i++) {
        if (i) r += ", ";
        if (std::holds_alternative<std::string>(el[i]))
            r += '"' + std::get<std::string>(el[i]) + '"';
        else r += to_str(el[i]);
    }
    return r + "]";
}
double to_bool(const Value& v) {
    if (auto* nv = std::get_if<NumVal>(&v))    return nv->size() > 0 && (*nv)[0] != 0.0 ? 1.0 : 0.0;
    if (auto* s = std::get_if<std::string>(&v)) return s->empty() ? 0.0 : 1.0;
    if (std::holds_alternative<ProcVal>(v))     return 1.0;
    return std::get<ArrayPtr>(v)->elems.empty() ? 0.0 : 1.0;
}
bool values_equal(const Value& a, const Value& b) {
    if (a.index() != b.index()) return false;
    if (auto* na = std::get_if<NumVal>(&a)) {
        const NumVal& nb = std::get<NumVal>(b);
        if (na->size() != nb.size()) return false;
        for (size_t i = 0; i < na->size(); i++) if ((*na)[i] != nb[i]) return false;
        return true;
    }
    if (auto* sa = std::get_if<std::string>(&a)) return *sa == std::get<std::string>(b);
    if (std::holds_alternative<ProcVal>(a))
        return std::get<ProcVal>(a) == std::get<ProcVal>(b);
    return std::get<ArrayPtr>(a) == std::get<ArrayPtr>(b);
}

struct Interpreter;
using Builtin = std::function<Value(std::vector<Value>&, Interpreter&)>;
struct Interpreter {
    std::vector<Token>              T;
    size_t                          pos = 0;
    std::map<std::string, Value>&   globals;
    std::map<std::string, Value>    locals;
    std::map<std::string, Proc>&    procs;
    std::map<std::string, Builtin>& builtins;
    std::function<void(const std::string&, const std::string&)> load_fn;
    bool in_proc = false;
    std::string filename;
    std::vector<std::string>& call_stack;   // shared across all sub-interpreters

    Token consume()    { return T[pos++]; }
    bool  check(TK t)  { return T[pos].type == t; }
    bool  match(TK t)  { if (check(t)) { pos++; return true; } return false; }
    Token expect(TK t) {
        if (!check(t)) throw Error{filename, T[pos].line, "unexpected '" + T[pos].val + "'", call_stack};
        return consume();
    }
    int cur_line() { return T[pos].line; }
    Error make_err(const std::string& msg) { return Error{filename, cur_line(), msg, call_stack}; }

    Value get_var(const std::string& n) {
        auto it = locals.find(n);   if (it != locals.end())  return it->second;
        auto it2 = globals.find(n); if (it2 != globals.end()) return it2->second;
        throw make_err("undefined '" + n + "'");
    }
    Value* get_var_ptr(const std::string& n) {
        auto it = locals.find(n);   if (it != locals.end())  return &it->second;
        auto it2 = globals.find(n); if (it2 != globals.end()) return &it2->second;
        return nullptr;
    }
    void decl_var(const std::string& n, Value v) {
        if (in_proc) locals[n] = std::move(v); else globals[n] = std::move(v);
    }
    void assign_var(const std::string& n, Value v) {
        auto it = locals.find(n);   if (it != locals.end())  { it->second = std::move(v); return; }
        auto it2 = globals.find(n); if (it2 != globals.end()) { it2->second = std::move(v); return; }
        if (in_proc) locals[n] = std::move(v); else globals[n] = std::move(v);
    }

    NumVal nv_binop(const NumVal& a, const NumVal& b, char op) {
        size_t sa = a.size(), sb = b.size();
        if (sa == sb) {
            switch(op) {
                case '+': return a + b; case '-': return a - b;
                case '*': return a * b; case '/': return a / b;
            }
        }
        if (sb == 1) {   // b is scalar: broadcast b[0]
            switch(op) {
                case '+': return a + b[0]; case '-': return a - b[0];
                case '*': return a * b[0]; case '/': return a / b[0];
            }
        }
        if (sa == 1) {   // a is scalar: broadcast a[0]
            NumVal ra(a[0], sb);
            switch(op) {
                case '+': return ra + b; case '-': return ra - b;
                case '*': return ra * b; case '/': return ra / b;
            }
        }
        throw make_err("vector size mismatch: " + std::to_string(sa) + " vs " + std::to_string(sb));
        return {};
    }
    NumVal nv_cmp(const NumVal& a, const NumVal& b, TK op) {
        size_t sa = a.size(), sb = b.size();
        size_t sz = std::max(sa, sb);
        const NumVal ra = (sa == 1 && sz > 1) ? NumVal(a[0], sz) : a;
        const NumVal rb = (sb == 1 && sz > 1) ? NumVal(b[0], sz) : b;
        if (ra.size() != rb.size()) throw make_err("vector size mismatch in comparison");
        NumVal r(ra.size());
        for (size_t i = 0; i < ra.size(); i++) {
            switch(op) {
                case LT:  r[i] = ra[i] <  rb[i] ? 1.0 : 0.0; break;
                case GT:  r[i] = ra[i] >  rb[i] ? 1.0 : 0.0; break;
                case LE:  r[i] = ra[i] <= rb[i] ? 1.0 : 0.0; break;
                case GE:  r[i] = ra[i] >= rb[i] ? 1.0 : 0.0; break;
                case EQ:  r[i] = ra[i] == rb[i] ? 1.0 : 0.0; break;
                case NEQ: r[i] = ra[i] != rb[i] ? 1.0 : 0.0; break;
                default: break;
            }
        }
        return r;
    }
    NumVal nv_apply(const NumVal& v, double(*f)(double)) {
        NumVal r(v.size());
        for (size_t i = 0; i < v.size(); i++) r[i] = f(v[i]);
        return r;
    }
    NumVal nv_apply2(const NumVal& a, const NumVal& b, double(*f)(double, double)) {
        size_t sa = a.size(), sb = b.size();
        size_t sz = std::max(sa, sb);
        const NumVal ra = (sa == 1 && sz > 1) ? NumVal(a[0], sz) : a;
        const NumVal rb = (sb == 1 && sz > 1) ? NumVal(b[0], sz) : b;
        if (ra.size() != rb.size()) throw make_err("vector size mismatch");
        NumVal r(ra.size());
        for (size_t i = 0; i < ra.size(); i++) r[i] = f(ra[i], rb[i]);
        return r;
    }

    void run() { while (!check(END)) stmt(); }
    void stmt() {
        if (check(VAR)) {
            consume(); std::string n = expect(IDENT).val; expect(ASSIGN); decl_var(n, expr());
        }
        else if (check(PROC))   { proc_decl(); }
        else if (check(IF))     { if_stmt(); }
        else if (check(WHILE))  { while_stmt(); }
        else if (check(FOR))    { for_stmt(); }
        else if (check(PRINT))  { print_stmt(); }
        else if (check(RETURN)) { consume(); throw ReturnSignal{expr()}; }
        else if (check(BREAK))  { consume(); throw BreakSignal{}; }
        else if (check(IDENT) && T[pos+1].type == LBRACKET) { indexed_assign(); }
        else if (check(IDENT) && T[pos+1].type == ASSIGN) {
            std::string n = consume().val; consume(); assign_var(n, expr());
        }
        else if (check(IDENT) && T[pos+1].type == LPAREN) { expr(); }
        else if (!check(RBRACE) && !check(END))
            throw make_err("unexpected '" + T[pos].val + "'");
    }
    void indexed_assign() {
        std::string n = consume().val;
        expect(LBRACKET); Value idx = expr(); expect(RBRACKET);
        // nested a[i][j] = v  — only for ArrayPtr
        if (check(LBRACKET)) {
            Value outer = get_var(n);
            auto& ap = std::get<ArrayPtr>(outer);
            int i = checked_arr_index(ap, idx, n);
            expect(LBRACKET); Value idx2 = expr(); expect(RBRACKET);
            expect(ASSIGN); Value rhs = expr();
            auto& inner = std::get<ArrayPtr>(ap->elems[i]);
            int j = checked_arr_index(inner, idx2, n);
            inner->elems[j] = rhs;
            return;
        }
        expect(ASSIGN); Value rhs = expr();
        Value* stored = get_var_ptr(n);
        if (!stored) throw make_err("undefined '" + n + "'");
        if (std::holds_alternative<NumVal>(*stored)) {
            NumVal& nv = std::get<NumVal>(*stored);
            int i = (int)nv_scalar(std::get<NumVal>(idx));
            if (i < 0) i += (int)nv.size();
            if (i < 0 || i >= (int)nv.size())
                throw make_err("index " + std::to_string(i) + " out of bounds for '" + n + "'");
            nv[i] = nv_scalar(std::get<NumVal>(rhs));
        } else if (std::holds_alternative<ArrayPtr>(*stored)) {
            auto& ap = std::get<ArrayPtr>(*stored);
            ap->elems[checked_arr_index(ap, idx, n)] = rhs;
        } else {
            throw make_err("cannot index into '" + n + "'");
        }
    }
    int checked_arr_index(const ArrayPtr& ap, const Value& idx, const std::string& name) {
        int i = (int)nv_scalar(std::get<NumVal>(idx));
        if (i < 0) i += (int)ap->elems.size();
        if (i < 0 || i >= (int)ap->elems.size())
            throw make_err("index " + std::to_string(i) + " out of bounds for '" + name + "'");
        return i;
    }
    void proc_decl() {
        consume(); std::string name = expect(IDENT).val;
        expect(LPAREN);
        std::vector<std::string> params;
        while (!check(RPAREN)) {
            params.push_back(expect(IDENT).val);
            if (!check(RPAREN)) expect(COMMA);
        }
        expect(RPAREN);
        std::vector<Token> body;
        body.push_back(expect(LBRACE));
        int depth = 1;
        while (depth > 0) {
            if (check(LBRACE)) depth++; else if (check(RBRACE)) depth--;
            body.push_back(consume());
        }
        body.push_back({END, ""});
        procs[name] = {params, std::move(body)};
    }
    void run_block() {
        expect(LBRACE);
        try { while (!check(RBRACE) && !check(END)) stmt(); }
        catch (...) {
            int depth = 0;
            while (!check(END)) {
                if      (check(LBRACE)) { depth++; pos++; }
                else if (check(RBRACE)) { if (depth==0) break; depth--; pos++; }
                else pos++;
            }
            expect(RBRACE); throw;
        }
        expect(RBRACE);
    }

    void skip_block() {
        expect(LBRACE); int depth = 1;
        while (depth > 0) {
            if (check(LBRACE)) depth++; else if (check(RBRACE)) depth--;
            pos++;
        }
    }
    void if_stmt() {
        consume();
        expect(LPAREN); bool taken = to_bool(expr()) != 0.0; expect(RPAREN);
        if (taken) run_block(); else skip_block();
        while (check(ELSE)) {
            consume();
            if (check(IF)) {
                consume();
                expect(LPAREN); bool c = to_bool(expr()) != 0.0; expect(RPAREN);
                if (!taken && c) { run_block(); taken = true; } else skip_block();
            } else {
                if (!taken) run_block(); else skip_block(); break;
            }
        }
    }
    void while_stmt() {
        consume(); size_t cond = pos;
        while (true) {
            pos = cond;
            expect(LPAREN); bool c = to_bool(expr()) != 0.0; expect(RPAREN);
            if (!c) { skip_block(); break; }
            try { run_block(); } catch (BreakSignal&) { break; }
        }
    }
    void for_stmt() {
        consume(); expect(LPAREN); expect(VAR);
        std::string var_name = expect(IDENT).val;
        expect(IN); Value collection = expr(); expect(RPAREN);
        size_t body_start = pos;

        auto run_body_with = [&](Value v) {
            pos = body_start; decl_var(var_name, std::move(v));
            try { run_block(); } catch (BreakSignal&) { throw; }
        };

        if (std::holds_alternative<NumVal>(collection)) {
            const NumVal& nv = std::get<NumVal>(collection);
            bool broken = false;
            for (size_t i = 0; i < nv.size() && !broken; i++) {
                try { run_body_with(NumVal{nv[i]}); }
                catch (BreakSignal&) { broken = true; }
            }
            if (!broken) { pos = body_start; skip_block(); }
        } else if (std::holds_alternative<ArrayPtr>(collection)) {
            const auto& elems = std::get<ArrayPtr>(collection)->elems;
            bool broken = false;
            for (size_t i = 0; i < elems.size() && !broken; i++) {
                try { run_body_with(elems[i]); }
                catch (BreakSignal&) { broken = true; }
            }
            if (!broken) { pos = body_start; skip_block(); }
        } else if (std::holds_alternative<std::string>(collection)) {
            const std::string& s = std::get<std::string>(collection);
            bool broken = false;
            for (size_t i = 0; i < s.size() && !broken; i++) {
                try { run_body_with(std::string(1, s[i])); }
                catch (BreakSignal&) { broken = true; }
            }
            if (!broken) { pos = body_start; skip_block(); }
        } else {
            throw make_err("'for in' requires number, array, or string");
        }
    }
    void print_stmt() {
        int pl = T[pos].line; consume();
        std::string out;
        while (!check(END)   && !check(RBRACE) && !check(VAR)   && !check(PROC)
            && !check(WHILE) && !check(FOR)    && !check(IF)    && !check(ELSE)
            && !check(BREAK) && !check(PRINT)  && !check(RETURN)
            && !(check(IDENT) && T[pos+1].type == ASSIGN)
            && T[pos].line == pl)
            out += to_str(expr());
        std::cout << out << "\n";
    }

    Value call_user(const std::string& name, std::vector<Value> args) {
        auto& p = procs.at(name);
        if (args.size() != p.params.size())
            throw make_err("arity mismatch calling '" + name + "': expected "
                           + std::to_string(p.params.size()) + " args");
        call_stack.push_back(name);
        Interpreter sub{p.body, 0, globals, {}, procs, builtins, load_fn, true, filename, call_stack};
        for (size_t i = 0; i < args.size(); i++) sub.locals[p.params[i]] = args[i];
        Value result{NumVal{0.0}};
        try {
            sub.run_block();
        } catch (ReturnSignal& r) {
            result = r.val;
        } catch (...) {
            call_stack.pop_back();
            throw;
        }
        call_stack.pop_back();
        return result;
    }
    Value call_procval(const ProcVal& pv, std::vector<Value> args, const std::string& label = "<proc>") {
        if (args.size() != pv->params.size())
            throw make_err("arity mismatch: expected " + std::to_string(pv->params.size()) + " args");
        call_stack.push_back(label);
        Interpreter sub{pv->body, 0, globals, {}, procs, builtins, load_fn, true, filename, call_stack};
        for (size_t i = 0; i < args.size(); i++) sub.locals[pv->params[i]] = args[i];
        Value result{NumVal{0.0}};
        try {
            sub.run_block();
        } catch (ReturnSignal& r) {
            result = r.val;
        } catch (...) {
            call_stack.pop_back();
            throw;
        }
        call_stack.pop_back();
        return result;
    }
    Value call_builtin(const std::string& nm, std::vector<Value>& a) {
        // user-registered functions take priority
        auto it = builtins.find(nm);
        if (it != builtins.end()) return it->second(a, *this);

        auto d = [&](int i) -> double {
            return nv_scalar(std::get<NumVal>(a[i]));
        };
        auto nv = [&](int i) -> const NumVal& {
            return std::get<NumVal>(a[i]);
        };
        auto sv = [&](int i) -> const std::string& {
            return std::get<std::string>(a[i]);
        };
        auto ap = [&](int i) -> ArrayPtr& {
            return std::get<ArrayPtr>(a[i]);
        };
        auto chk = [&](size_t n) {
            if (a.size() != n)
                throw make_err(nm + ": expected " + std::to_string(n) + " arg(s)");
        };
        auto chk_arr = [&](int i, const std::string& fn) {
            if (!std::holds_alternative<ArrayPtr>(a[i]))
                throw make_err(fn + ": argument must be an array");
        };

        if (nm=="floor") { chk(1); return nv_apply(nv(0), std::floor); }
        if (nm=="ceil")  { chk(1); return nv_apply(nv(0), std::ceil);  }
        if (nm=="abs")   { chk(1); return nv_apply(nv(0), std::abs);   }
        if (nm=="sqrt")  { chk(1); return nv_apply(nv(0), std::sqrt);  }
        if (nm=="sin")   { chk(1); return nv_apply(nv(0), std::sin);   }
        if (nm=="cos")   { chk(1); return nv_apply(nv(0), std::cos);   }
        if (nm=="tan")   { chk(1); return nv_apply(nv(0), std::tan);   }
        if (nm=="exp")   { chk(1); return nv_apply(nv(0), std::exp);   }
        if (nm=="log")   { chk(1); return nv_apply(nv(0), std::log);   }
        if (nm=="log2")  { chk(1); return nv_apply(nv(0), [](double x){ return std::log2(x); }); }
        if (nm=="atan2") { chk(2); return nv_apply2(nv(0), nv(1), std::atan2); }
        if (nm=="pow")   { chk(2); return nv_apply2(nv(0), nv(1), std::pow); }
        if (nm=="rand")  { chk(0); return NumVal{(double)std::rand() / RAND_MAX}; }

        if (nm=="vec") {
            NumVal r(a.size());
            for (size_t i = 0; i < a.size(); i++) r[i] = nv_scalar(std::get<NumVal>(a[i]));
            return r;
        }
        if (nm=="linspace") {
            chk(3); double lo=d(0), hi=d(1); int n=(int)d(2);
            if (n <= 0) return NumVal{};
            if (n == 1) return NumVal{lo};
            NumVal r(n);
            for (int i = 0; i < n; i++) r[i] = lo + (hi - lo) * i / (n - 1);
            return r;
        }
        if (nm=="zeros")  { chk(1); return NumVal((size_t)d(0)); }         // zero-filled
        if (nm=="ones")   { chk(1); return NumVal(1.0, (size_t)d(0)); }    // one-filled
        if (nm=="sum") {
            chk(1);
            if (!std::holds_alternative<NumVal>(a[0]))
                throw make_err("sum: argument must be a vector (use arr_sum from stdlib for arrays)");
            return NumVal{std::get<NumVal>(a[0]).sum()};
        }
        if (nm=="all") {
            chk(1);
            if (!std::holds_alternative<NumVal>(a[0]))
                throw make_err("all: argument must be a vector");
            const NumVal& v = std::get<NumVal>(a[0]);
            for (size_t i = 0; i < v.size(); i++) if (v[i] == 0.0) return NumVal{0.0};
            return NumVal{1.0};
        }
        if (nm=="any") {
            chk(1);
            if (!std::holds_alternative<NumVal>(a[0]))
                throw make_err("any: argument must be a vector");
            const NumVal& v = std::get<NumVal>(a[0]);
            for (size_t i = 0; i < v.size(); i++) if (v[i] != 0.0) return NumVal{1.0};
            return NumVal{0.0};
        }

        if (nm=="to_vec") {
            chk(1); chk_arr(0, "to_vec");
            const auto& el = ap(0)->elems;
            NumVal r(el.size());
            for (size_t i = 0; i < el.size(); i++) {
                if (!std::holds_alternative<NumVal>(el[i]))
                    throw make_err("to_vec: all array elements must be numbers");
                r[i] = nv_scalar(std::get<NumVal>(el[i]));
            }
            return r;
        }
        if (nm=="to_arr") {
            chk(1);
            if (!std::holds_alternative<NumVal>(a[0]))
                throw make_err("to_arr: argument must be a vector");
            const NumVal& v = std::get<NumVal>(a[0]);
            auto r = std::make_shared<Array>();
            r->elems.reserve(v.size());
            for (size_t i = 0; i < v.size(); i++) r->elems.push_back(NumVal{v[i]});
            return r;
        }
        if (nm=="len") {
            chk(1);
            if (std::holds_alternative<ArrayPtr>(a[0])) return NumVal{(double)ap(0)->elems.size()};
            if (std::holds_alternative<NumVal>(a[0]))   return NumVal{(double)std::get<NumVal>(a[0]).size()};
            return NumVal{(double)sv(0).size()};
        }

        if (nm=="sub")  {
            chk(3); auto s=sv(0); int lo=(int)d(1), hi=(int)d(2);
            if (lo<0) lo=0; if (hi>(int)s.size()) hi=(int)s.size();
            return lo<hi ? s.substr(lo,hi-lo) : std::string{};
        }
        if (nm=="find") {
            chk(2); auto p=sv(0).find(sv(1));
            return NumVal{p==std::string::npos ? -1.0 : (double)p};
        }
        if (nm=="str")  { chk(1); return to_str(a[0]); }
        if (nm=="num")  { chk(1); return NumVal{std::stod(sv(0))}; }
        if (nm=="upper"){ chk(1); std::string s=sv(0); for(char&c:s)c=toupper((unsigned char)c); return s; }
        if (nm=="lower"){ chk(1); std::string s=sv(0); for(char&c:s)c=tolower((unsigned char)c); return s; }
        if (nm=="char") { chk(1); return std::string(1,(char)(int)d(0)); }
        if (nm=="asc")  {
            chk(1);
            if (sv(0).empty()) throw make_err("asc: empty string");
            return NumVal{(double)(unsigned char)sv(0)[0]};
        }
        if (nm=="type") {
            chk(1);
            if (std::holds_alternative<NumVal>(a[0])) {
                return std::string{std::get<NumVal>(a[0]).size() <= 1 ? "number" : "vector"};
            }
            if (std::holds_alternative<std::string>(a[0])) return std::string{"string"};
            if (std::holds_alternative<ProcVal>(a[0]))     return std::string{"proc"};
            return std::string{"array"};
        }

        if (nm=="arr") {
            auto a_ = std::make_shared<Array>();
            if (a.empty()) return a_;
            int n = (int)d(0);
            Value fill = a.size() >= 2 ? a[1] : Value{NumVal{0.0}};
            a_->elems.resize(n, fill);
            return a_;
        }
        if (nm=="push") {
            if (a.size() < 2) throw make_err("push: needs array and value");
            chk_arr(0, "push");
            for (size_t i = 1; i < a.size(); i++) ap(0)->elems.push_back(a[i]);
            return a[0];
        }
        if (nm=="pop") {
            chk(1); chk_arr(0, "pop");
            if (ap(0)->elems.empty()) throw make_err("pop: empty array");
            Value v = ap(0)->elems.back(); ap(0)->elems.pop_back(); return v;
        }
        if (nm=="insert") {
            if (a.size() != 3) throw make_err("insert: needs 3 args");
            chk_arr(0, "insert");
            int i = (int)d(1);
            if (i < 0 || i > (int)ap(0)->elems.size()) throw make_err("insert: index out of bounds");
            ap(0)->elems.insert(ap(0)->elems.begin() + i, a[2]);
            return a[0];
        }
        if (nm=="remove") {
            chk(2); chk_arr(0, "remove");
            int i = (int)d(1);
            if (i < 0) i += (int)ap(0)->elems.size();
            if (i < 0 || i >= (int)ap(0)->elems.size()) throw make_err("remove: index out of bounds");
            Value v = ap(0)->elems[i];
            ap(0)->elems.erase(ap(0)->elems.begin() + i);
            return v;
        }
        if (nm=="slice") {
            chk(3); chk_arr(0, "slice");
            int lo=(int)d(1), hi=(int)d(2), sz=(int)ap(0)->elems.size();
            if (lo<0) lo=0; if (hi>sz) hi=sz;
            auto r = std::make_shared<Array>();
            if (lo < hi) r->elems.assign(ap(0)->elems.begin()+lo, ap(0)->elems.begin()+hi);
            return r;
        }
        if (nm=="concat") {
            chk(2); chk_arr(0, "concat"); chk_arr(1, "concat");
            auto r = std::make_shared<Array>(Array{ap(0)->elems});
            r->elems.insert(r->elems.end(), ap(1)->elems.begin(), ap(1)->elems.end());
            return r;
        }
        if (nm=="join") {
            chk(2); chk_arr(0, "join");
            std::string sep = sv(1), out;
            const auto& el = ap(0)->elems;
            for (size_t i = 0; i < el.size(); i++) { if(i) out+=sep; out+=to_str(el[i]); }
            return out;
        }
        if (nm=="split") {
            chk(2); std::string s=sv(0), delim=sv(1);
            auto r = std::make_shared<Array>();
            size_t dlen = delim.size();
            while (true) {
                size_t p = s.find(delim);
                if (p == std::string::npos) { r->elems.push_back(s); break; }
                r->elems.push_back(s.substr(0, p)); s = s.substr(p + dlen);
            }
            return r;
        }
        if (nm=="copy") {
            chk(1); chk_arr(0, "copy");
            return std::make_shared<Array>(Array{ap(0)->elems});
        }
        if (nm=="range") {
            if (a.size() < 2 || a.size() > 3) throw make_err("range: needs 2 or 3 args");
            double lo=d(0), hi=d(1), step = a.size()==3 ? d(2) : 1.0;
            if (step == 0) throw make_err("range: step cannot be 0");
            auto r = std::make_shared<Array>();
            for (double x = lo; step>0 ? x<hi : x>hi; x += step) r->elems.push_back(NumVal{x});
            return r;
        }
        if (nm=="shuffle") {
            chk(1); chk_arr(0, "shuffle");
            auto r = std::make_shared<Array>(Array{ap(0)->elems});
            auto& el = r->elems;
            for (size_t i = el.size()-1; i > 0; i--) {
                size_t j = (size_t)std::rand() % (i + 1);
                std::swap(el[i], el[j]);
            }
            return r;
        }
        if (nm=="keys") {
            chk(0);
            auto r = std::make_shared<Array>();
            for (auto& [k, _] : globals) r->elems.push_back(k);
            return r;
        }

        if (nm=="read") {
            chk(1); std::ifstream f(sv(0));
            if (!f) throw make_err("read: can't open '" + sv(0) + "'");
            return std::string(std::istreambuf_iterator<char>(f), {});
        }
        if (nm=="write") {
            chk(2); std::ofstream f(sv(0));
            if (!f) throw make_err("write: can't open '" + sv(0) + "'");
            f << sv(1); return NumVal{0.0};
        }
        if (nm=="append") {
            chk(2); std::ofstream f(sv(0), std::ios::app);
            if (!f) throw make_err("append: can't open '" + sv(0) + "'");
            f << sv(1); return NumVal{0.0};
        }
        if (nm=="load") {
            chk(1);
            if (!load_fn) throw make_err("load: not available");
            std::string path = sv(0); std::ifstream f(path);
            if (!f) {
                const char* home = getenv("HOME");
                if (home) { std::string hp = std::string(home) + "/.musil/" + path; f.open(hp); if (f) path = hp; }
            }
            if (!f) throw make_err("load: can't open '" + sv(0) + "'");
            load_fn(std::string(std::istreambuf_iterator<char>(f), {}), path);
            return NumVal{0.0};
        }

        if (nm=="input") {
            if (a.size() > 1) throw make_err("input: 0 or 1 arg");
            if (a.size() == 1) std::cout << to_str(a[0]) << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) return std::string{};
            return line;
        }
        if (nm=="eval") {
            chk(1);
            if (!load_fn) throw make_err("eval: not available");
            load_fn(sv(0), "<eval>");
            return NumVal{0.0};
        }
        if (nm=="exec") {
            chk(1);
            std::cout.flush();   // flush output before child process writes
            return NumVal{(double)std::system(sv(0).c_str())};
        }
        if (nm=="apply") {
            // apply(f, args_array) — f can be a proc value or a proc/builtin name string
            if (a.size() != 2) throw make_err("apply: needs 2 args (proc or name, array)");
            chk_arr(1, "apply");
            std::vector<Value> args = ap(1)->elems;
            if (std::holds_alternative<ProcVal>(a[0]))
                return call_procval(std::get<ProcVal>(a[0]), args);
            std::string proc_name = sv(0);
            auto pit = procs.find(proc_name);
            if (pit != procs.end()) return call_user(proc_name, args);
            return call_builtin(proc_name, args);
        }
        if (nm=="clock") { chk(0); return NumVal{(double)std::clock() / CLOCKS_PER_SEC}; }
        if (nm=="exit")  { chk(1); std::exit((int)d(0)); }
        if (nm=="assert"){
            if (a.size() < 1 || a.size() > 2) throw make_err("assert: 1 or 2 args");
            if (!to_bool(a[0])) {
                std::string msg = a.size() == 2 ? sv(1) : "assertion failed";
                throw make_err(msg);
            }
            return NumVal{0.0};
        }

        throw make_err("undefined '" + nm + "'");
    }

    Value expr()     { return or_expr(); }
    Value or_expr()  {
        Value l = and_expr();
        while (check(OR)) { consume(); Value r = and_expr(); l = NumVal{(to_bool(l)||to_bool(r)) ? 1.0 : 0.0}; }
        return l;
    }
    Value and_expr() {
        Value l = not_expr();
        while (check(AND)) { consume(); Value r = not_expr(); l = NumVal{(to_bool(l)&&to_bool(r)) ? 1.0 : 0.0}; }
        return l;
    }
    Value not_expr() {
        if (check(NOT)) { consume(); return NumVal{to_bool(not_expr()) == 0.0 ? 1.0 : 0.0}; }
        return cmp();
    }
    Value cmp() {
        Value l = add();
        if (check(LT)||check(GT)||check(LE)||check(GE)||check(EQ)||check(NEQ)) {
            TK op = consume().type; Value r = add();
            // string equality
            if (std::holds_alternative<std::string>(l) || std::holds_alternative<std::string>(r)) {
                bool eq = values_equal(l, r);
                if (op==EQ)  return NumVal{eq ? 1.0 : 0.0};
                if (op==NEQ) return NumVal{!eq ? 1.0 : 0.0};
                throw make_err("strings only support == and !=");
            }
            // array identity comparison
            if (std::holds_alternative<ArrayPtr>(l) || std::holds_alternative<ArrayPtr>(r)) {
                bool eq = values_equal(l, r);
                if (op==EQ)  return NumVal{eq ? 1.0 : 0.0};
                if (op==NEQ) return NumVal{!eq ? 1.0 : 0.0};
                throw make_err("arrays only support == and !=");
            }
            // numeric: element-wise, returns NumVal mask
            return nv_cmp(std::get<NumVal>(l), std::get<NumVal>(r), op);
        }
        return l;
    }
    Value add() {
        Value l = mul();
        while (check(PLUS)||check(MINUS)) {
            bool plus = consume().type==PLUS; Value r = mul();
            if (plus && (std::holds_alternative<std::string>(l) || std::holds_alternative<std::string>(r)))
                l = to_str(l) + to_str(r);
            else if (plus) l = nv_binop(std::get<NumVal>(l), std::get<NumVal>(r), '+');
            else           l = nv_binop(std::get<NumVal>(l), std::get<NumVal>(r), '-');
        }
        return l;
    }
    Value mul() {
        Value l = unary();
        while (check(STAR)||check(SLASH)) {
            bool star = consume().type==STAR; Value r = unary();
            if (star) l = nv_binop(std::get<NumVal>(l), std::get<NumVal>(r), '*');
            else      l = nv_binop(std::get<NumVal>(l), std::get<NumVal>(r), '/');
        }
        return l;
    }
    Value unary() {
        if (check(MINUS)) { consume(); return -std::get<NumVal>(unary()); }
        return index_expr();
    }
    Value index_expr() {
        Value v = atom();
        while (check(LBRACKET) || (check(LPAREN) && std::holds_alternative<ProcVal>(v))) {
            if (check(LBRACKET)) {
                consume(); Value idx = expr(); expect(RBRACKET);
                if (std::holds_alternative<NumVal>(v)) {
                    const NumVal& n = std::get<NumVal>(v);
                    int i = (int)nv_scalar(std::get<NumVal>(idx));
                    if (i < 0) i += (int)n.size();
                    if (i < 0 || i >= (int)n.size())
                        throw make_err("index " + std::to_string(i) + " out of bounds");
                    v = NumVal{n[i]};
                } else if (std::holds_alternative<ArrayPtr>(v)) {
                    auto& ap = std::get<ArrayPtr>(v);
                    int i = (int)nv_scalar(std::get<NumVal>(idx));
                    if (i < 0) i += (int)ap->elems.size();
                    if (i < 0 || i >= (int)ap->elems.size())
                        throw make_err("index " + std::to_string(i) + " out of bounds");
                    v = ap->elems[i];
                } else {
                    throw make_err("subscript on non-indexable value");
                }
            } else {
                // LPAREN after a ProcVal: call it
                consume();
                std::vector<Value> args;
                while (!check(RPAREN)) { args.push_back(expr()); if (!check(RPAREN)) expect(COMMA); }
                expect(RPAREN);
                v = call_procval(std::get<ProcVal>(v), args);
            }
        }
        return v;
    }
    Value atom() {
        if (check(NUM))     return NumVal{std::stod(consume().val)};
        if (check(STR))     return consume().val;
        if (check(LPAREN))  { consume(); Value v=expr(); expect(RPAREN); return v; }
        // anonymous proc literal:  proc (params) { body }
        if (check(PROC)) {
            consume();
            std::vector<std::string> params;
            expect(LPAREN);
            while (!check(RPAREN)) {
                params.push_back(expect(IDENT).val);
                if (!check(RPAREN)) expect(COMMA);
            }
            expect(RPAREN);
            std::vector<Token> body;
            body.push_back(expect(LBRACE));
            int depth = 1;
            while (depth > 0) {
                if (check(LBRACE)) depth++; else if (check(RBRACE)) depth--;
                body.push_back(consume());
            }
            body.push_back({END, ""});
            return std::make_shared<Proc>(Proc{std::move(params), std::move(body)});
        }
        if (check(LBRACKET)) {
            consume();
            auto arr = std::make_shared<Array>();
            while (!check(RBRACKET)) {
                arr->elems.push_back(expr());
                if (!check(RBRACKET)) expect(COMMA);
            }
            expect(RBRACKET);
            return arr;
        }
        if (check(IDENT)) {
            std::string n = consume().val;
            if (check(LPAREN)) {
                consume();
                std::vector<Value> args;
                while (!check(RPAREN)) {
                    args.push_back(expr());
                    if (!check(RPAREN)) expect(COMMA);
                }
                expect(RPAREN);
                // 1. Named proc
                auto pit = procs.find(n);
                if (pit != procs.end()) return call_user(n, args);
                // 2. Variable holding a ProcVal
                Value* vp = get_var_ptr(n);
                if (vp && std::holds_alternative<ProcVal>(*vp))
                    return call_procval(std::get<ProcVal>(*vp), args, n);
                // 3. Builtin
                return call_builtin(n, args);
            }
            return get_var(n);
        }
        throw make_err("unexpected in expr '" + T[pos].val + "'");
    }
};

struct Environment {
    std::map<std::string, Value>   globals;
    std::map<std::string, Proc>    procs;
    std::map<std::string, Builtin> builtins;
    std::vector<std::string>       call_stack;

    void register_builtin(const std::string& name, Builtin fn) {
        builtins[name] = std::move(fn);
    }
    void exec(const std::string& src, const std::string& filename = "<stdin>") {
        auto toks = lex(src, filename);
        Interpreter interp{std::move(toks), 0, globals, {}, procs, builtins,
                           {}, false, filename, call_stack};
        interp.load_fn = [this](const std::string& s, const std::string& f){ this->exec(s, f); };
        interp.run();
    }
};
std::string format_error(const Error& e) {
    std::string msg = e.file + ":" + std::to_string(e.line) + ": " + e.msg;
    if (!e.trace.empty()) {
        msg += "\n  call stack:";
        for (int i = (int)e.trace.size()-1; i >= 0; i--)
            msg += "\n    " + std::to_string(i+1) + "> " + e.trace[i];
    }
    return msg;
}
int brace_depth(const std::string& s) {
    int d = 0; bool in_str = false;
    for (char c : s) {
        if (c == '"') in_str = !in_str;
        else if (!in_str) { if (c=='{') d++; else if (c=='}') d--; }
    }
    return d;
}
void repl(Environment& env) {
    std::string buf; int depth = 0;
    while (true) {
        std::cout << (depth==0 ? ">> " : ".. ") << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << "\n"; break; }
        depth += brace_depth(line);
        buf += line + "\n";
        if (depth <= 0) {
            depth = 0;
            if (buf.find_first_not_of(" \t\n") != std::string::npos) {
                try { env.exec(buf, "<stdin>"); }
                catch (Error& e)         { std::cerr << format_error(e) << "\n"; }
                catch (std::exception& e){ std::cerr << "error: " << e.what() << "\n"; }
            }
            buf.clear();
        }
    }
}

#endif // MUSIL_H
