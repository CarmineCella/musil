#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>

using Env = std::unordered_map<std::string, std::string>;

struct Proc {
    std::vector<std::string> args;
    std::string body;
};

static std::unordered_map<std::string, Proc> procs;

static std::string eval_line(const std::string &line, Env &env);
static std::string eval_script(const std::string &script, Env &env);

static std::vector<std::string> split(const std::string &line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string t;
    while (iss >> t) tokens.push_back(t);
    return tokens;
}

static std::string expand(const std::string &arg, const Env &env) {
    if (!arg.empty() && arg[0] == '$') {
        auto it = env.find(arg.substr(1));
        if (it != env.end()) return it->second;
        return "";
    }
    return arg;
}

static double to_number(const std::string &s) {
    try { return std::stod(s); }
    catch (...) { return 0.0; }
}

static std::string from_number(double x) {
    std::ostringstream oss; oss << x; return oss.str();
}

static std::string subst_brackets(const std::string &line, Env &env) {
    std::string s = line;
    while (true) {
        size_t end = s.find(']');
        if (end == std::string::npos) break;
        size_t start = s.rfind('[', end);
        if (start == std::string::npos) break;
        std::string inner = s.substr(start + 1, end - start - 1);
        std::string res = eval_line(inner, env);
        s.replace(start, end - start + 1, res);
    }
    return s;
}

static std::string eval_command(const std::vector<std::string> &tokens, Env &env) {
    if (tokens.empty()) return "";
    const std::string &cmd = tokens[0];

    if (cmd == "exit") std::exit(0);

    if (cmd == "set") {
        if (tokens.size() < 3) { std::cerr << "set: need name value\n"; return ""; }
        std::string name = tokens[1], value;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (i > 2) value += " ";
            value += expand(tokens[i], env);
        }
        env[name] = value;
        return value;
    }

    if (cmd == "puts") {
        std::string out;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (i > 1) out += " ";
            out += expand(tokens[i], env);
        }
        std::cout << out << "\n";
        return out;
    }

    if (cmd == "if") {
        if (tokens.size() < 3) { std::cerr << "if: need cond body\n"; return ""; }
        double c = to_number(expand(tokens[1], env));
        std::string body;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (i > 2) body += " ";
            body += tokens[i];
        }
        if (!body.empty()) return c != 0.0 ? eval_script(body, env) : "";
        return "";
    }

    if (cmd == "while") {
        if (tokens.size() < 3) { std::cerr << "while: need cond body\n"; return ""; }
        std::string cond = tokens[1];
        std::string body;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (i > 2) body += " ";
            body += tokens[i];
        }
        std::string last;
        while (to_number(expand(cond, env)) != 0.0) {
            last = eval_script(body, env);
        }
        return last;
    }

    if (cmd == "proc") {
        if (tokens.size() < 4) { std::cerr << "proc: name {args} {body}\n"; return ""; }
        std::string name = tokens[1];
        std::string arglist = tokens[2];
        if (arglist.size() >= 2 && arglist.front() == '{' && arglist.back() == '}')
            arglist = arglist.substr(1, arglist.size() - 2);
        Proc p;
        std::istringstream a(arglist);
        std::string an;
        while (a >> an) p.args.push_back(an);
        std::string body;
        for (size_t i = 3; i < tokens.size(); ++i) {
            if (i > 3) body += " ";
            body += tokens[i];
        }
        if (body.size() >= 2 && body.front() == '{' && body.back() == '}')
            body = body.substr(1, body.size() - 2);
        p.body = body;
        procs[name] = p;
        return "";
    }

    auto binary_num = [&](auto op) -> std::string {
        if (tokens.size() < 3) {
            std::cerr << cmd << ": need two numeric args\n";
            return "";
        }
        double a = to_number(expand(tokens[1], env));
        double b = to_number(expand(tokens[2], env));
        return from_number(op(a, b));
    };

    if (cmd == "add") return binary_num([](double a, double b){ return a + b; });
    if (cmd == "sub") return binary_num([](double a, double b){ return a - b; });
    if (cmd == "mul") return binary_num([](double a, double b){ return a * b; });
    if (cmd == "div") return binary_num([](double a, double b){ return b == 0.0 ? 0.0 : a / b; });

    auto pit = procs.find(cmd);
    if (pit != procs.end()) {
        Proc &p = pit->second;
        for (size_t i = 0; i < p.args.size(); ++i) {
            if (1 + i < tokens.size())
                env[p.args[i]] = expand(tokens[1 + i], env);
        }
        return eval_script(p.body, env);
    }

    std::cerr << "unknown command: " << cmd << "\n";
    return "";
}

static std::string eval_line(const std::string &raw, Env &env) {
    std::string line = subst_brackets(raw, env);
    auto tokens = split(line);
    return eval_command(tokens, env);
}

// static std::string eval_script(const std::string &script, Env &env) {
//     std::string last;
//     std::istringstream iss(script);
//     std::string line;
//     while (std::getline(iss, line)) {
//         std::istringstream ls(line);
//         std::string part;
//         while (std::getline(ls, part, ';')) {
//             if (part.find_first_not_of(" \t\r\n") == std::string::npos) continue;
//             last = eval_line(part, env);
//         }
//     }
//     return last;
// }
static std::string eval_script(const std::string &script, Env &env) {
    std::string last;
    std::istringstream iss(script);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        std::string part;
        while (std::getline(ls, part, ';')) {
            // trim
            auto start = part.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            auto end = part.find_last_not_of(" \t\r\n");
            std::string chunk = part.substr(start, end - start + 1);

            // skip comments and lone braces
            if (chunk.empty()) continue;
            if (chunk[0] == '#') continue;
            if (chunk == "{" || chunk == "}") continue;

            last = eval_line(chunk, env);
        }
    }
    return last;
}

int main(int argc, char** argv) {
    Env env;

    // --- No arguments → start REPL ---
    if (argc == 1) {
        std::string line;
        std::cout << "Mini TCL-like interpreter. Type 'exit' to quit.\n";
        while (std::cout << "> " && std::getline(std::cin, line)) {
            std::string trimmed = line;
            while (!trimmed.empty() && (trimmed.back()=='\r'||trimmed.back()=='\n'||trimmed.back()==' '))
                trimmed.pop_back();
            if (trimmed.empty() || trimmed[0] == '#') continue;
            eval_line(trimmed, env);
        }
        return 0;
    }

    // --- Arguments present → interpret each file sequentially ---
    for (int i = 1; i < argc; ++i) {
        std::ifstream f(argv[i]);
        if (!f) {
            std::cerr << "cannot open file: " << argv[i] << "\n";
            continue;
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        eval_script(buffer.str(), env);
    }

    return 0;
}

// int main() {
//     Env env;
//     std::string line;
//     std::cout << "Mini TCL-like interpreter. Type 'exit' to quit.\n";
//     while (std::cout << "> " && std::getline(std::cin, line)) {
//         std::string trimmed = line;
//         while (!trimmed.empty() && (trimmed.back()=='\r'||trimmed.back()=='\n'||trimmed.back()==' '))
//             trimmed.pop_back();
//         if (trimmed.empty() || trimmed[0] == '#') continue;
//         eval_line(trimmed, env);
//     }
//     return 0;
// }
