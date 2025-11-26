// musil.cpp
//

#include "musil.h"
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace std;

// TODO: scientific + esempi + tests, signals + esempi + tests, plotting + esempi
// 	     sparkle, bconv, maple, orchieda, sound-types
//		 granulatore orchestrale
// 		 parte simbolica (note, midi, musicxml, generatori algoritmici)

int main (int argc, char* argv[]) {
	srand (time (NULL));
	AtomPtr env = make_env ();
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

			cout << "music scripting language" << endl;
			cout << "(c) " << COPYRIGHT << ", www.carminecella.com" << endl << endl;

			repl (cin, cout, env);
		} else {
			for (int i = optind; i < argc; ++i) {
				load (argv[i], env);
			}
			if (interactive) repl (cin, cout, env);
		}
	} catch (AtomPtr& e) {
		cerr << RED << "error: "; print (e, cout) << RESET << std::endl;
	} catch (exception& e) {
		cerr << RED << "exception: " << e.what () << RESET << std::endl;
	} catch (...) {
		cerr << RED << "fatal unknown error" << RESET << std::endl;
	}
	
	return 0;
}

// eof