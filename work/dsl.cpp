#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <algorithm>

struct Event {
    double start;
    double dur;
    double pitch;
    double amp;
};

struct Expr {
    enum Kind { NUM, SYM, LIST } kind;
    double num{};
    std::string sym;
    std::vector<Expr> list;
};

/// ---------- Tokenizer ----------

std::vector<std::string> tokenize(const std::string &src) {
    std::vector<std::string> toks;
    std::string cur;
    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
        } else if (c == '(' || c == ')') {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
            toks.emplace_back(1, c);
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) toks.push_back(cur);
    return toks;
}

/// ---------- Parser ----------

struct Parser {
    std::vector<std::string> toks;
    size_t pos{0};

    bool eof() const { return pos >= toks.size(); }
    const std::string& peek() const { return toks[pos]; }
    std::string get() { return toks[pos++]; }

    Expr parse_expr() {
        if (eof()) throw std::runtime_error("Unexpected EOF");
        const std::string &t = peek();
        if (t == "(") {
            get(); // consume '('
            Expr e; e.kind = Expr::LIST;
            while (!eof() && peek() != ")") {
                e.list.push_back(parse_expr());
            }
            if (eof() || get() != ")")
                throw std::runtime_error("Missing ')'");
            return e;
        }
        // atom
        get();
        // number?
        char c0 = t[0];
        if (std::isdigit(static_cast<unsigned char>(c0)) || c0 == '-' || c0 == '+') {
            char *end = nullptr;
            double val = std::strtod(t.c_str(), &end);
            if (end && *end == '\0') {
                Expr e; e.kind = Expr::NUM; e.num = val; return e;
            }
        }
        Expr e; e.kind = Expr::SYM; e.sym = t; return e;
    }
};

/// ---------- Helpers ----------

double get_num(const Expr &e) {
    if (e.kind != Expr::NUM) throw std::runtime_error("Expected number");
    return e.num;
}
std::string get_sym(const Expr &e) {
    if (e.kind != Expr::SYM) throw std::runtime_error("Expected symbol");
    return e.sym;
}

double events_duration(const std::vector<Event> &evs) {
    double m = 0.0;
    for (auto &e : evs) {
        m = std::max(m, e.start + e.dur);
    }
    return m;
}

std::vector<Event> shift_events(double dt, const std::vector<Event> &evs) {
    std::vector<Event> out = evs;
    for (auto &e : out) e.start += dt;
    return out;
}

std::vector<Event> scale_time(double factor, const std::vector<Event> &evs) {
    std::vector<Event> out = evs;
    for (auto &e : out) {
        e.start *= factor;
        e.dur   *= factor;
    }
    return out;
}

/// Forward decl
double expr_duration(const Expr &e);
std::vector<Event> eval_expr(const Expr &e);

/// ---------- Duration computation on AST ----------

double expr_duration(const Expr &e) {
    if (e.kind != Expr::LIST || e.list.empty())
        throw std::runtime_error("Bad expression in duration");
    std::string op = get_sym(e.list[0]);

    if (op == "note") {
        if (e.list.size() < 3) throw std::runtime_error("note: needs pitch dur");
        return get_num(e.list[2]);
    }
    if (op == "rest") {
        if (e.list.size() < 2) throw std::runtime_error("rest: needs dur");
        return get_num(e.list[1]);
    }
    if (op == "seq") {
        double t = 0.0;
        for (size_t i = 1; i < e.list.size(); ++i)
            t += expr_duration(e.list[i]);
        return t;
    }
    if (op == "par") {
        double m = 0.0;
        for (size_t i = 1; i < e.list.size(); ++i)
            m = std::max(m, expr_duration(e.list[i]));
        return m;
    }
    if (op == "repeat") {
        if (e.list.size() < 3) throw std::runtime_error("repeat: n expr");
        int n = static_cast<int>(get_num(e.list[1]));
        double d = expr_duration(e.list[2]);
        return n * d;
    }
    if (op == "tempo") {
        if (e.list.size() < 3) throw std::runtime_error("tempo: factor expr");
        double factor = get_num(e.list[1]); // >1 = faster
        double d = expr_duration(e.list[2]);
        return d / factor;
    }

    return 0.0; // unknown: treat as 0
}

/// ---------- Evaluator ----------

std::vector<Event> eval_expr(const Expr &e) {
    if (e.kind != Expr::LIST || e.list.empty())
        throw std::runtime_error("Bad expression");
    std::string op = get_sym(e.list[0]);

    if (op == "note") {
        if (e.list.size() < 3) throw std::runtime_error("note: pitch dur [amp]");
        double pitch = get_num(e.list[1]);
        double dur   = get_num(e.list[2]);
        double amp   = (e.list.size() >= 4) ? get_num(e.list[3]) : 0.8;
        return { Event{0.0, dur, pitch, amp} };
    }

    if (op == "rest") {
        // no events, duration handled by expr_duration in seq/repeat
        return {};
    }

    if (op == "seq") {
        std::vector<Event> out;
        double t = 0.0;
        for (size_t i = 1; i < e.list.size(); ++i) {
            auto evs = eval_expr(e.list[i]);
            auto shifted = shift_events(t, evs);
            out.insert(out.end(), shifted.begin(), shifted.end());
            t += expr_duration(e.list[i]);
        }
        return out;
    }

    if (op == "par") {
        std::vector<Event> out;
        for (size_t i = 1; i < e.list.size(); ++i) {
            auto evs = eval_expr(e.list[i]); // all start at 0; seq will shift if needed
            out.insert(out.end(), evs.begin(), evs.end());
        }
        return out;
    }

    if (op == "repeat") {
        if (e.list.size() < 3) throw std::runtime_error("repeat: n expr");
        int n = static_cast<int>(get_num(e.list[1]));
        const Expr &body = e.list[2];
        std::vector<Event> out;
        double t = 0.0;
        double d = expr_duration(body);
        for (int i = 0; i < n; ++i) {
            auto evs = eval_expr(body);
            auto shifted = shift_events(t, evs);
            out.insert(out.end(), shifted.begin(), shifted.end());
            t += d;
        }
        return out;
    }

    if (op == "tempo") {
        if (e.list.size() < 3) throw std::runtime_error("tempo: factor expr");
        double factor = get_num(e.list[1]); // >1 = faster
        auto evs = eval_expr(e.list[2]);
        return scale_time(1.0 / factor, evs);
    }

    throw std::runtime_error("Unknown operator: " + op);
}

/// ---------- Main ----------

int main() {
    try {
        // read whole stdin into a string
        std::ostringstream buf;
        buf << std::cin.rdbuf();
        std::string src = buf.str();

        auto toks = tokenize(src);
        if (toks.empty()) {
            std::cerr << "No input.\n";
            return 1;
        }

        Parser p{toks};
        // parse a single top-level expression
        Expr ast = p.parse_expr();

        auto events = eval_expr(ast);

        // print events: start dur pitch amp
        for (const auto &e : events) {
            std::cout << e.start << " " << e.dur << " "
                      << e.pitch << " " << e.amp << "\n";
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
