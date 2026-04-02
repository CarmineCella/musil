// moon v3 — tiny interpreter
// new: if/else, and/or/not, break, math/string built-ins, file I/O
// compile: g++ -std=c++17 -O2 -o moon moon.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <functional>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <stdexcept>

// ── Value ─────────────────────────────────────────────────────────────────────

using Value = std::variant<double, std::string>;

std::string to_str(const Value& v) {
    if (auto* d = std::get_if<double>(&v)) {
        
        char buf[32]; snprintf(buf,sizeof(buf),"%.15g",*d); return buf;
    }
    return std::get<std::string>(v);
}

// non-zero number or non-empty string = truthy
double to_bool(const Value& v) {
    if (auto* d = std::get_if<double>(&v)) return *d != 0.0 ? 1.0 : 0.0;
    return std::get<std::string>(v).empty() ? 0.0 : 1.0;
}

// ── Tokens ────────────────────────────────────────────────────────────────────

enum TK {
    NUM, STR, IDENT,
    VAR, PROC, WHILE, IF, ELSE, RETURN, PRINT, BREAK,
    AND, OR, NOT,
    ASSIGN, PLUS, MINUS, STAR, SLASH,
    LT, GT, LE, GE, EQ, NEQ,
    LPAREN, RPAREN, LBRACE, RBRACE, COMMA, END
};
struct Token { TK type; std::string val; int line = 1; };

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> toks;
    size_t i = 0, n = src.size();
    int line = 1;
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
        // number: optional leading dot, reject second dot
        if (isdigit((unsigned char)src[i]) ||
            (src[i] == '.' && i+1 < n && isdigit((unsigned char)src[i+1]))) {
            std::string s; bool has_dot = false;
            while (i < n && (isdigit((unsigned char)src[i]) || src[i] == '.')) {
                if (src[i] == '.') {
                    if (has_dot) throw std::runtime_error("malformed number: '" + s + ".'");
                    has_dot = true;
                }
                s += src[i++];
            }
            toks.push_back({NUM, s, line}); continue;
        }
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            std::string s;
            while (i < n && (isalnum((unsigned char)src[i]) || src[i] == '_')) s += src[i++];
            TK t = IDENT;
            if      (s=="var")    t=VAR;   else if (s=="proc")   t=PROC;
            else if (s=="while")  t=WHILE; else if (s=="if")     t=IF;
            else if (s=="else")   t=ELSE;  else if (s=="return") t=RETURN;
            else if (s=="print")  t=PRINT; else if (s=="break")  t=BREAK;
            else if (s=="and")    t=AND;   else if (s=="or")     t=OR;
            else if (s=="not")    t=NOT;
            toks.push_back({t, s, line}); continue;
        }
        if (i+1 < n) {
            char a=src[i], b=src[i+1];
            if (a=='<'&&b=='='){toks.push_back({LE,"<=", line}); i+=2; continue;}
            if (a=='>'&&b=='='){toks.push_back({GE,">=", line}); i+=2; continue;}
            if (a=='='&&b=='='){toks.push_back({EQ,"==", line}); i+=2; continue;}
            if (a=='!'&&b=='='){toks.push_back({NEQ,"!=", line}); i+=2; continue;}
        }
        static const std::string ops = "=+-*/<>(),{}";
        static const TK opt[] = {ASSIGN,PLUS,MINUS,STAR,SLASH,LT,GT,LPAREN,RPAREN,COMMA,LBRACE,RBRACE};
        size_t p = ops.find(src[i]);
        if (p != std::string::npos) { toks.push_back({opt[p], std::string(1,src[i]), line}); i++; continue; }
        throw std::runtime_error(std::string("unknown character: ") + src[i]);
    }
    toks.push_back({END, "", line}); return toks;
}

// ── Signals ───────────────────────────────────────────────────────────────────

struct ReturnSignal { Value val; };
struct BreakSignal  {};

// ── Proc ─────────────────────────────────────────────────────────────────────

struct Proc {
    std::vector<std::string> params;
    std::vector<Token>       body;   // self-contained; includes { and }
};

// ── Interpreter ───────────────────────────────────────────────────────────────

struct Interp {
    std::vector<Token>            T;
    size_t                        pos = 0;
    std::map<std::string, Value>& globals;  // top-level scope, shared across all calls
    std::map<std::string, Value>  locals;   // proc-local scope (params + var inside proc)
    std::map<std::string, Proc>&  procs;
    std::function<void(const std::string&)> load_fn;
    bool in_proc = false;

    Token consume()    { return T[pos++]; }
    bool  check(TK t)  { return T[pos].type == t; }
    bool  match(TK t)  { if (check(t)) { pos++; return true; } return false; }
    Token expect(TK t) {
        if (!check(t)) throw std::runtime_error(
            "line " + std::to_string(T[pos].line) + ": unexpected '" + T[pos].val + "'");
        return consume();
    }

    // variable access: locals shadow globals
    Value get_var(const std::string& n) {
        auto it = locals.find(n);
        if (it != locals.end()) return it->second;
        auto it2 = globals.find(n);
        if (it2 != globals.end()) return it2->second;
        throw std::runtime_error("line " + std::to_string(T[pos].line) + ": undefined: '" + n + "'");
    }
    // var statement: always declares in the current scope
    void decl_var(const std::string& n, Value v) {
        if (in_proc) locals[n] = v; else globals[n] = v;
    }
    // assignment: update existing binding (locals first, then globals); create if not found
    void assign_var(const std::string& n, Value v) {
        auto it = locals.find(n);  if (it != locals.end()) { it->second = v; return; }
        auto it2 = globals.find(n); if (it2 != globals.end()) { it2->second = v; return; }
        if (in_proc) locals[n] = v; else globals[n] = v;
    }

    // ── Statements ───────────────────────────────────────────────────────────

    void run() { while (!check(END)) stmt(); }

    void stmt() {
        if      (check(VAR))    { consume(); std::string n=expect(IDENT).val; expect(ASSIGN); decl_var(n, expr()); }
        else if (check(PROC))   { proc_decl(); }
        else if (check(IF))     { if_stmt(); }
        else if (check(WHILE))  { while_stmt(); }
        else if (check(PRINT))  { print_stmt(); }
        else if (check(RETURN)) { consume(); throw ReturnSignal{expr()}; }
        else if (check(BREAK))  { consume(); throw BreakSignal{}; }
        else if (check(IDENT) && T[pos+1].type == ASSIGN) {
            std::string n=consume().val; consume(); assign_var(n, expr());
        }
        else if (check(IDENT) && T[pos+1].type == LPAREN) { expr(); } // bare call
        else if (!check(RBRACE) && !check(END))
            throw std::runtime_error("unexpected: '" + T[pos].val + "'");
    }

    void proc_decl() {
        consume();
        std::string name = expect(IDENT).val;
        expect(LPAREN);
        std::vector<std::string> params;
        while (!check(RPAREN)) { params.push_back(expect(IDENT).val); match(COMMA); }
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

    // run_block: on any exception, fast-forward pos to the matching }
    // so callers always find pos cleanly positioned after the block.
    void run_block() {
        expect(LBRACE);
        try {
            while (!check(RBRACE) && !check(END)) stmt();
        } catch (...) {
            int depth = 0;
            while (!check(END)) {
                if      (check(LBRACE)) { depth++; pos++; }
                else if (check(RBRACE)) { if (depth == 0) break; depth--; pos++; }
                else pos++;
            }
            expect(RBRACE); throw; // re-throw with pos cleanly past }
        }
        expect(RBRACE);
    }

    void skip_block() {
        expect(LBRACE); int depth = 1;
        while (depth > 0) { if (check(LBRACE)) depth++; else if (check(RBRACE)) depth--; pos++; }
    }

    void if_stmt() {
        consume(); // 'if'
        expect(LPAREN); bool taken = to_bool(expr()) != 0.0; expect(RPAREN);
        if (taken) run_block(); else skip_block();
        while (check(ELSE)) {
            consume();
            if (check(IF)) {
                consume();
                expect(LPAREN); bool c = to_bool(expr()) != 0.0; expect(RPAREN);
                if (!taken && c) { run_block(); taken = true; } else skip_block();
            } else {
                if (!taken) run_block(); else skip_block();
                break; // plain else is always last
            }
        }
    }

    void while_stmt() {
        consume();
        size_t cond = pos;
        while (true) {
            pos = cond;
            expect(LPAREN); bool c = to_bool(expr()) != 0.0; expect(RPAREN);
            if (!c) { skip_block(); break; }
            try { run_block(); }
            catch (BreakSignal&) { break; } // run_block consumed } before re-throwing
        }
    }

    void print_stmt() {
        int print_line = T[pos].line; // line of 'print' keyword (not yet consumed)
        consume();                    // now consume it
        std::string out;
        // collect expressions only while they start on the same source line
        while (!check(END)    && !check(RBRACE) && !check(VAR)    && !check(PROC)
            && !check(WHILE)  && !check(IF)     && !check(ELSE)   && !check(BREAK)
            && !check(PRINT)  && !check(RETURN)
            && !(check(IDENT) && T[pos+1].type == ASSIGN)
            && T[pos].line == print_line)
            out += to_str(expr());
        std::cout << out << "\n";
    }

    // ── Calls ─────────────────────────────────────────────────────────────────

    Value call_user(const std::string& name, std::vector<Value> args) {
        auto& p = procs.at(name);
        if (args.size() != p.params.size())
            throw std::runtime_error("arity mismatch: " + name);
        Interp sub{p.body, 0, globals, {}, procs};
        sub.in_proc  = true;
        sub.load_fn  = load_fn;
        for (size_t i = 0; i < args.size(); i++) sub.locals[p.params[i]] = args[i];
        Value result = 0.0;
        try { sub.run_block(); } catch (ReturnSignal& r) { result = r.val; }
        return result;  // globals modified in place; locals discarded
    }

    Value call_builtin(const std::string& nm, std::vector<Value>& a) {
        auto d  = [&](int i){ return std::get<double>(a[i]); };
        auto sv = [&](int i){ return std::get<std::string>(a[i]); };
        auto chk= [&](size_t n){
            if (a.size()!=n) throw std::runtime_error(nm+": expected "+std::to_string(n)+" arg(s)");
        };
        // ── math ──────────────────────────────────────────────────────────────
        if (nm=="floor"){ chk(1); return std::floor(d(0)); }
        if (nm=="ceil") { chk(1); return std::ceil(d(0));  }
        if (nm=="abs")  { chk(1); return std::abs(d(0));   }
        if (nm=="sqrt") { chk(1); return std::sqrt(d(0));  }
        if (nm=="sin")  { chk(1); return std::sin(d(0));   }
        if (nm=="cos")  { chk(1); return std::cos(d(0));   }
        if (nm=="pow")  { chk(2); return std::pow(d(0),d(1)); }
        if (nm=="log")  { chk(1); return std::log(d(0));   }
        if (nm=="rand") { chk(0); return (double)std::rand()/RAND_MAX; }
        // ── string ────────────────────────────────────────────────────────────
        if (nm=="len")  { chk(1); return (double)sv(0).size(); }
        if (nm=="sub")  {
            chk(3); auto str=sv(0); int lo=(int)d(1), hi=(int)d(2);
            if (lo<0) lo=0; if (hi>(int)str.size()) hi=(int)str.size();
            return lo<hi ? str.substr(lo,hi-lo) : std::string{};
        }
        if (nm=="find") { chk(2); auto p=sv(0).find(sv(1)); return p==std::string::npos?-1.0:(double)p; }
        if (nm=="str")  { chk(1); return to_str(a[0]); }
        if (nm=="num")  { chk(1); return std::stod(sv(0)); }
        // ── file I/O ──────────────────────────────────────────────────────────
        if (nm=="read") {
            chk(1); std::ifstream f(sv(0));
            if (!f) throw std::runtime_error("read: can't open '"+sv(0)+"'");
            return std::string(std::istreambuf_iterator<char>(f),{});
        }
        if (nm=="write") {
            chk(2); std::ofstream f(sv(0));
            if (!f) throw std::runtime_error("write: can't open '"+sv(0)+"'");
            f << sv(1); return 0.0;
        }
        if (nm=="append") {
            chk(2); std::ofstream f(sv(0),std::ios::app);
            if (!f) throw std::runtime_error("append: can't open '"+sv(0)+"'");
            f << sv(1); return 0.0;
        }
        if (nm=="load") {
            chk(1);
            if (!load_fn) throw std::runtime_error("load: not available");
            std::ifstream f(sv(0));
            if (!f) throw std::runtime_error("load: can't open '"+sv(0)+"'");
            load_fn(std::string(std::istreambuf_iterator<char>(f),{}));
            return 0.0;
        }
        // ── interactive & misc ────────────────────────────────────────────────
        if (nm=="input") {
            if (a.size() > 1) throw std::runtime_error("input: 0 or 1 arg");
            if (a.size() == 1) std::cout << to_str(a[0]) << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) return std::string{};
            return line;
        }
        if (nm=="upper") {
            chk(1); std::string s=sv(0);
            for (char& c : s) c = toupper((unsigned char)c); return s;
        }
        if (nm=="lower") {
            chk(1); std::string s=sv(0);
            for (char& c : s) c = tolower((unsigned char)c); return s;
        }
        if (nm=="type")  { chk(1); return std::holds_alternative<double>(a[0]) ? std::string{"number"} : std::string{"string"}; }
        if (nm=="char")  { chk(1); return std::string(1, (char)(int)d(0)); }
        if (nm=="asc")   { chk(1); if(sv(0).empty()) throw std::runtime_error("asc: empty string"); return (double)(unsigned char)sv(0)[0]; }
        if (nm=="clock") { chk(0); return (double)std::clock() / CLOCKS_PER_SEC; }
        throw std::runtime_error("undefined: '"+nm+"'");
    }

    // ── Expressions: or > and > not > cmp > add > mul > unary > atom ─────────

    Value expr() { return or_expr(); }

    Value or_expr() {
        Value l = and_expr();
        while (check(OR))  { consume(); Value r=and_expr(); l=(to_bool(l)||to_bool(r))?1.0:0.0; }
        return l;
    }
    Value and_expr() {
        Value l = not_expr();
        while (check(AND)) { consume(); Value r=not_expr(); l=(to_bool(l)&&to_bool(r))?1.0:0.0; }
        return l;
    }
    Value not_expr() {
        if (check(NOT)) { consume(); return to_bool(not_expr())==0.0?1.0:0.0; }
        return cmp();
    }
    Value cmp() {
        Value l = add();
        if (check(LT)||check(GT)||check(LE)||check(GE)||check(EQ)||check(NEQ)) {
            TK op=consume().type; Value r=add();
            // == and != use variant equality: works for both strings and numbers
            if (op==EQ)  return l==r ? 1.0 : 0.0;
            if (op==NEQ) return l!=r ? 1.0 : 0.0;
            // ordering operators require numbers
            double a=std::get<double>(l), b=std::get<double>(r);
            switch(op){
                case LT: return a<b?1.0:0.0;  case GT: return a>b?1.0:0.0;
                case LE: return a<=b?1.0:0.0; case GE: return a>=b?1.0:0.0;
                default: return 0.0;
            }
        }
        return l;
    }
    Value add() {
        Value l = mul();
        while (check(PLUS)||check(MINUS)) {
            bool plus=consume().type==PLUS; Value r=mul();
            if (plus&&(std::holds_alternative<std::string>(l)||std::holds_alternative<std::string>(r)))
                l=to_str(l)+to_str(r);
            else if(plus) l=std::get<double>(l)+std::get<double>(r);
            else          l=std::get<double>(l)-std::get<double>(r);
        }
        return l;
    }
    Value mul() {
        Value l = unary();
        while (check(STAR)||check(SLASH)) {
            bool star=consume().type==STAR; Value r=unary();
            l=star?std::get<double>(l)*std::get<double>(r):std::get<double>(l)/std::get<double>(r);
        }
        return l;
    }
    Value unary() {
        if (check(MINUS)) { consume(); return -std::get<double>(unary()); }
        return atom();
    }
    Value atom() {
        if (check(NUM))    return std::stod(consume().val);
        if (check(STR))    return consume().val;
        if (check(LPAREN)){ consume(); Value v=expr(); expect(RPAREN); return v; }
        if (check(IDENT)) {
            std::string n=consume().val;
            if (check(LPAREN)) {
                consume();
                std::vector<Value> args;
                while (!check(RPAREN)) { args.push_back(expr()); match(COMMA); }
                expect(RPAREN);
                auto it=procs.find(n);
                return it!=procs.end() ? call_user(n,args) : call_builtin(n,args);
            }
            return get_var(n);
        }
        throw std::runtime_error("line " + std::to_string(T[pos].line) + ": unexpected in expr: '" + T[pos].val + "'");
    }
};

// ── Shared state ──────────────────────────────────────────────────────────────

struct MoonState {
    std::map<std::string, Value> globals;
    std::map<std::string, Proc>  procs;

    void exec(const std::string& src) {
        auto toks = lex(src);
        Interp interp{std::move(toks), 0, globals, {}, procs};
        interp.load_fn = [this](const std::string& s){ this->exec(s); };
        interp.run();
    }
};

// ── REPL ─────────────────────────────────────────────────────────────────────

int brace_depth(const std::string& s) {
    int d=0; bool in_str=false;
    for (char c:s) {
        if (c=='"') in_str=!in_str;
        else if (!in_str){ if(c=='{') d++; else if(c=='}') d--; }
    }
    return d;
}

void repl(MoonState& state) {
    std::cout << "moon v3 REPL  (Ctrl-D to exit)\n";
    std::string buf; int depth=0;
    while (true) {
        std::cout << (depth==0 ? "moon> " : "   .. ") << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << "\n"; break; }
        depth += brace_depth(line);
        buf += line + "\n";
        if (depth <= 0) {
            depth=0;
            if (buf.find_first_not_of(" \t\n") != std::string::npos) {
                try   { state.exec(buf); }
                catch (std::exception& e) { std::cerr << "moon: " << e.what() << "\n"; }
            }
            buf.clear();
        }
    }
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::srand(42); // deterministic by default; use srand(time(0)) for real randomness
    MoonState state;
    if (argc == 1) { repl(state); return 0; }
    for (int a=1; a<argc; a++) {
        std::ifstream f(argv[a]);
        if (!f) { std::cerr << "moon: cannot open '" << argv[a] << "'\n"; return 1; }
        std::string src(std::istreambuf_iterator<char>(f),{});
        try   { state.exec(src); }
        catch (std::exception& e) { std::cerr << "moon: " << e.what() << "\n"; return 1; }
    }
}
