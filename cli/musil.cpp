// musil.cpp
//

#include "musil.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <ctime>

using namespace std;

int main(int argc, char* argv[]) {
    std::srand(time (NULL));
    Environment interpreter;

    add_scientific(interpreter);
    add_system(interpreter);
    add_signals(interpreter);
    add_plotting(interpreter);

    if (argc == 1) {
        cout << "[musil, v" << VERSION << "]" << endl << endl;
        cout << "(c) 2026 by Carmine-Emanuele Cella" << endl << endl;
        repl(interpreter);
        return 0;
    }
    for (int a=1; a<argc; a++) {
        std::ifstream f(argv[a]);
        if (!f) {
            std::cerr << "cannot open '" << argv[a] << "'\n";
            return 1;
        }
        std::string src(std::istreambuf_iterator<char>(f), {});
        try   {
            interpreter.exec(src, argv[a]);
        } catch (Error& e) {
            std::cerr << "error: " << e.file << ":" << e.line << ": " << e.msg << "\n";
            return 1;
        } catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }
}

// eof
