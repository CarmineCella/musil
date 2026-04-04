// musil.cpp
//

#include "musil.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <ctime>

using namespace std;

int main (int argc, char* argv[]) {
    std::srand(time (NULL));
    Environment interpreter;

    add_scientific(interpreter);
    add_system(interpreter);
    add_signals(interpreter);
    add_plotting(interpreter);
	add_rtsound(interpreter);

	try {
		bool interactive = false;
		int opt = 0;
		while ((opt = getopt(argc, argv, "i")) != -1) {
		    switch (opt) {
		    case 'i': interactive = true; break;
		    default:
		        std::stringstream msg;
		        msg << "usage is " << argv[0] << " [-i] [file...]";
		        throw runtime_error (msg.str ());
		    }
		}
		if (argc - optind == 0) {
			cout << BOLDBLUE << "[musil, version "
				<< VERSION <<"]" << RESET << endl << endl;

			cout << "scripting language for sound and music computing" << endl;
			cout << "(c) " << COPYRIGHT << " by Carmine-Emanuele Cella" << endl << endl;

			repl(interpreter);
		} else {
			for (int i = optind; i < argc; ++i) {
                std::ifstream f(argv[i]);
                if (!f) {
                    std::cerr << "cannot open '" << argv[i] << "'\n";
                    return 1;
                }
                std::string src(std::istreambuf_iterator<char>(f), {});
                interpreter.exec(src, argv[i]);
			}
			if (interactive) repl(interpreter);
		}
    } catch (Error& e) {
        std::cerr <<  RED << format_error(e) << RESET  << endl;
        return 1;
    } catch (std::exception& e) {
        std::cerr <<  RED << "error: " << e.what() << RESET  << endl;
        return 1;
	} catch (...) {
		cerr << RED << "fatal unknown error" << RESET << endl;
	}
	
	return 0;
}
// eof
