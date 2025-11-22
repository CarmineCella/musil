// musil.cpp
//
#include <iostream>
#include "musil.h"

using namespace std;

// BUGS:
// - se display non finisce, non è segnalato errore; va bene?

int main (int argc, char* argv[]) {
	AtomPtr env = make_env ();

	if (argc == 1) {
		cout << "[musil, v. 0.1]" << endl << endl;
		cout << "music processing language" << endl;
		cout << "(c) 2025 by Carmine-Emanuele Cella" << endl << endl;
	
		repl (cin, cout, env);
	} else {
		AtomPtr r;
		for (int i = 1; i < argc; ++i) {
			ifstream in (argv[1]);
			if (!in.good ()) cout << "warning: cannot open " << argv[i] << endl;
			load (argv[1], in, env);
		}
	}
	return 0;
}

// eof

