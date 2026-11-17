// embed.cpp — Musil inside another program: many interpreters, each on its own thread, created,
// used and destroyed; afterwards nothing they made is left alive, and a host can refuse the
// builtins that wait.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. SPDX-License-Identifier: BSD-2-Clause

#include "musil.h"

#include <thread>

using namespace musil;

static int failures = 0;
static void expect(bool ok, const std::string& what) {
    if (!ok) { std::cerr << "FAIL " << what << "\n"; ++failures; }
}

int main() {
    const long v0 = live_values().load(), e0 = live_envs().load();
    // eight threads, each making and destroying interpreters that load the libraries and run code
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([t, &errors] {
            for (int k = 0; k < 5; ++k) {
                Interp I;
                make_env(I);
                std::ostringstream out; I.out = &out;
                try {
                    I.run("load \"std.mu\"\n"
                          "function local-sum (xs) {\n"
                          "    var acc 0\n"
                          "    function add (x) (set acc (+ acc x))\n"
                          "    each xs add\n"
                          "    return acc\n"
                          "}\n"
                          "var deep (list)\n"
                          "for (var k 0) (< k 20000) (var k (+ k 1)) (set deep (list deep))\n"
                          "print (local-sum (list 1 2 3 " + std::to_string(t) + "))\n", "<embed>");
                    if (out.str() != std::to_string(6 + t) + "\n") ++errors;
                } catch (const std::exception& ex) { std::cerr << ex.what() << "\n"; ++errors; }
            }
        });
    }
    for (auto& th : threads) th.join();
    expect(errors == 0, "interpreters on threads computed the right results");
    expect(live_values().load() == v0, "every value is freed when the interpreters are gone (left: " +
                                       std::to_string(live_values().load() - v0) + ")");
    expect(live_envs().load() == e0, "every environment is freed when the interpreters are gone (left: " +
                                     std::to_string(live_envs().load() - e0) + ")");
    // a host refuses builtins that wait
    {
        Interp I;
        make_env(I);
        I.allow_blocking = false;
        bool refused = false;
        try { I.run("sleep 10", "<embed>"); } catch (const Error& e) { refused = std::string(e.what()).find("not allowed") != std::string::npos; }
        expect(refused, "sleep is refused when blocking is not allowed");
    }
    if (failures == 0) std::cout << "embed: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
