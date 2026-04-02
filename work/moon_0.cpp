// moon — a tiny interpreter

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>

using Value = std::variant<double, std::string>;

std::string to_str(const Value& v) {
    if (auto* d = std::get_if<double>(&v)) {
        if (*d == (long long)*d) return std::to_string((long long)*d);
        std::ostringstream s; s << *d; return s.str();
    }
    return std::get<std::string>(v);
}

enum TK {
    NUM, STR, IDENT,
    VAR, PROC, WHILE, RETURN, PRINT,
    ASSIGN, PLUS, MINUS, STAR, SLASH,
    LT, GT, LE, GE, EQ, NEQ,
    LPAREN, RPAREN, LBRACE, RBRACE, COMMA, END
};

struct Token { TK type; std::string val; };

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> toks;
    size_t i = 0, n = src.size();
    while (i < n) {
        if (isspace((unsigned char)src[i]))  { i++; continue; }
        if (src[i] == '#') { while (i < n && src[i] != '\n') i++; continue; }

        if (src[i] == '"') {
            std::string s; i++;
            while (i < n && src[i] != '"') s += src[i++];
            toks.push_back({STR, s}); i++; continue;
        }
        if (isdigit((unsigned char)src[i])) {
            std::string s;
            while (i < n && (isdigit((unsigned char)src[i]) || src[i] == '.')) s += src[i++];
            toks.push_back({NUM, s}); continue;
        }
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            std::string s;
            while (i < n && (isalnum((unsigned char)src[i]) || src[i] == '_')) s += src[i++];
            TK t = IDENT;
            if      (s == "var")    t = VAR;
            else if (s == "proc")   t = PROC;
            else if (s == "while")  t = WHILE;
            else if (s == "return") t = RETURN;
            else if (s == "print")  t = PRINT;
            toks.push_back({t, s}); continue;
        }
        if (i+1 < n) {
            char a = src[i], b = src[i+1];
            if (a=='<'&&b=='=') { toks.push_back({LE,"<="}); i+=2; continue; }
            if (a=='>'&&b=='=') { toks.push_back({GE,">="}); i+=2; continue; }
            if (a=='='&&b=='=') { toks.push_back({EQ,"=="}); i+=2; continue; }
            if (a=='!'&&b=='=') { toks.push_back({NEQ,"!="}); i+=2; continue; }
        }
        static const std::string ops = "=+-*/<>(),{}";
        static const TK     opt[] = { ASSIGN, PLUS, MINUS, STAR, SLASH,
                                      LT, GT, LPAREN, RPAREN, COMMA, LBRACE, RBRACE };
        size_t p = ops.find(src[i]);
        if (p != std::string::npos) { toks.push_back({opt[p], std::string(1,src[i])}); i++; continue; }
        throw std::runtime_error(std::string("unknown character: ") + src[i]);
    }
    toks.push_back({END, ""}); return toks;
}

struct ReturnSignal { Value val; };

struct Interp {
    const std::vector<Token>& T;
    size_t pos = 0;
    std::map<std::string, Value> vars;
    std::map<std::string, std::pair<std::vector<std::string>, size_t>> procs;
    //                                       param names ^        ^ index of '{' in T

    Token consume()       { return T[pos++]; }
    bool  check(TK t)     { return T[pos].type == t; }
    bool  match(TK t)     { if (check(t)) { pos++; return true; } return false; }
    Token expect(TK t)    {
        if (!check(t)) throw std::runtime_error("unexpected token: '" + T[pos].val + "'");
        return consume();
    }

    void run() { while (!check(END)) stmt(); }

    void stmt() {
        if (check(VAR)) {
            consume();
            std::string name = expect(IDENT).val;
            expect(ASSIGN); vars[name] = expr();
        }
        else if (check(PROC))   { proc_decl(); }
        else if (check(WHILE))  { while_stmt(); }
        else if (check(PRINT))  { print_stmt(); }
        else if (check(RETURN)) { consume(); throw ReturnSignal{expr()}; }
        else if (check(IDENT) && T[pos+1].type == ASSIGN) {
            std::string name = consume().val; consume(); vars[name] = expr();
        }
        else if (!check(RBRACE) && !check(END))
            throw std::runtime_error("unexpected token: '" + T[pos].val + "'");
    }

    void proc_decl() {
        consume(); // 'proc'
        std::string name = expect(IDENT).val;
        expect(LPAREN);
        std::vector<std::string> params;
        while (!check(RPAREN)) { params.push_back(expect(IDENT).val); match(COMMA); }
        expect(RPAREN);
        procs[name] = { params, pos };   // save position of '{'
        skip_block();                    // skip body during declaration
    }

    void skip_block() {
        expect(LBRACE); int depth = 1;
        while (depth > 0) { if (check(LBRACE)) depth++; else if (check(RBRACE)) depth--; pos++; }
    }

    void run_block() {
        expect(LBRACE);
        while (!check(RBRACE) && !check(END)) stmt();
        expect(RBRACE);
    }

    void while_stmt() {
        consume(); // 'while'
        size_t cond = pos;
        while (true) {
            pos = cond;
            expect(LPAREN); Value c = expr(); expect(RPAREN);
            if (std::get<double>(c) == 0.0) { skip_block(); break; }
            run_block();
        }
    }

    void print_stmt() {
        consume(); // 'print'
        std::string out;
        while (!check(END) && !check(RBRACE)
               && !check(VAR) && !check(PROC) && !check(WHILE)
               && !check(PRINT) && !check(RETURN)
               && !(check(IDENT) && T[pos+1].type == ASSIGN))
            out += to_str(expr());
        std::cout << out << "\n";
    }

    Value call(const std::string& name, std::vector<Value> args) {
        auto& [params, body] = procs.at(name);
        if (args.size() != params.size())
            throw std::runtime_error("arity mismatch in call to '" + name + "'");
        auto saved_vars = vars;
        auto saved_pos  = pos;
        for (size_t i = 0; i < args.size(); i++) vars[params[i]] = args[i];
        pos = body;
        Value result = 0.0;
        try { run_block(); } catch (ReturnSignal& r) { result = r.val; }
        vars = saved_vars;
        pos  = saved_pos;
        return result;
    }

    Value expr() {
        Value l = add();
        if (check(LT)||check(GT)||check(LE)||check(GE)||check(EQ)||check(NEQ)) {
            TK op = consume().type; Value r = add();
            double a = std::get<double>(l), b = std::get<double>(r);
            switch (op) {
                case LT:  return a <  b ? 1.0 : 0.0;
                case GT:  return a >  b ? 1.0 : 0.0;
                case LE:  return a <= b ? 1.0 : 0.0;
                case GE:  return a >= b ? 1.0 : 0.0;
                case EQ:  return a == b ? 1.0 : 0.0;
                case NEQ: return a != b ? 1.0 : 0.0;
                default:  return 0.0;
            }
        }
        return l;
    }

    Value add() {
        Value l = mul();
        while (check(PLUS) || check(MINUS)) {
            bool plus = consume().type == PLUS;
            Value r = mul();
            if (plus && (std::holds_alternative<std::string>(l) ||
                         std::holds_alternative<std::string>(r)))
                l = to_str(l) + to_str(r);                         // string concat
            else if (plus) l = std::get<double>(l) + std::get<double>(r);
            else           l = std::get<double>(l) - std::get<double>(r);
        }
        return l;
    }

    Value mul() {
        Value l = atom();
        while (check(STAR) || check(SLASH)) {
            bool star = consume().type == STAR;
            Value r = atom();
            l = star ? std::get<double>(l) * std::get<double>(r)
                     : std::get<double>(l) / std::get<double>(r);
        }
        return l;
    }

    Value atom() {
        if (check(NUM))    return std::stod(consume().val);
        if (check(STR))    return consume().val;
        if (check(LPAREN)) { consume(); Value v = expr(); expect(RPAREN); return v; }
        if (check(IDENT)) {
            std::string name = consume().val;
            if (check(LPAREN)) {                          // function call
                consume();
                std::vector<Value> args;
                while (!check(RPAREN)) { args.push_back(expr()); match(COMMA); }
                expect(RPAREN);
                return call(name, args);
            }
            auto it = vars.find(name);
            if (it == vars.end()) throw std::runtime_error("undefined variable: '" + name + "'");
            return it->second;
        }
        throw std::runtime_error("unexpected token in expression: '" + T[pos].val + "'");
    }
};

int main(int argc, char* argv[]) {
    std::string src;
    if (argc > 1) {
        std::ifstream f(argv[1]);
        if (!f) { std::cerr << "moon: cannot open '" << argv[1] << "'\n"; return 1; }
        src.assign(std::istreambuf_iterator<char>(f), {});
    } else {
        src.assign(std::istreambuf_iterator<char>(std::cin), {});
    }
    try {
        auto toks = lex(src);
        Interp interp{toks};
        interp.run();
    } catch (std::exception& e) {
        std::cerr << "moon error: " << e.what() << "\n"; return 1;
    }
}
