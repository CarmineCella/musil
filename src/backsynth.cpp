// backsynth.cpp
//

#include <iostream>
#include <vector>
#include <fstream>
#include <regex>
#include <string>
#include <map>
#include <sstream>
#include <valarray>
#include <cmath>

using namespace std;

// ast, helpers
using farray = std::valarray<float>;
struct Environment;
typedef void (*Op) (istream& in, Environment& env);
struct Environment {
    Environment () {}
    Environment (const Environment& e) {
        dict = e.dict;
        functions = e.functions;
        stack = e.stack;
    }
    map <string, Op> dict;
    vector<farray> stack;
    map<string, string> functions;
};
template <typename T>
farray top (vector<T>& v) {
    T r = v.at (v.size () - 1);
    v.pop_back ();
    return r;
}
template<typename T>
bool is_number (const char* s) {
    std::istringstream iss (s);
	T dummy;
	iss >> std::noskipws >> dummy;
	return iss && iss.eof ();
}
void error (const string& err, const string& token) {
    stringstream msg;
    msg << err << " " << token;
    throw runtime_error (msg.str ());
}
void eval (istream& in, Environment& env) {
    string token; 
    in >> token;
    if (token.size () == 0) return;
    if (is_number<float> (token.c_str ())) {
        env.stack.push_back (farray{(float) atof(token.c_str ())});
        return;
    }
    if (token == "[") {
        vector<float> acc;
        while (!in.eof ()) {
            in >> token;
            if (token == "]") break;
            if (is_number<float> (token.c_str ())) acc.push_back (atof(token.c_str ()));
            else error ("invalid type (expected number)", token);
        }
        env.stack.push_back (farray (acc.data(), acc.size()));
        return;
    }
    if (token == ":") {
        string fname;
        in >> fname;
        stringstream code;
        while (!in.eof ()) {
            in >> token;
            if (token == ";") break;
            code << token << " ";
        }
        env.functions[fname] = code.str ();
        return;
    }
    for (map<string, string>::iterator it = env.functions.begin (); it != env.functions.end (); ++it) {
        if (it->first == token) {
            stringstream code (it->second);
            while (!code.eof ()) eval (code, env);
            return;
        }
    }
    for (map<string, Op>::iterator it = env.dict.begin (); it != env.dict.end (); ++it) {
        if (it->first == token) {
            it->second (in, env);
            return;
        }
    }
    error ("unbound identifier", token);
}
template <typename T>
ostream& print_valarray(const valarray<T>& v, ostream& out = cout) {
    out << "[";
    for (size_t i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i + 1 < v.size()) out << " ";
    }
    out << "]";
    return out;
}

// ops
void fn_print (istream& in, Environment& env){
    for (unsigned i = 0; i < env.stack.size (); ++i) {
        print_valarray (env.stack.at (i)) << " ";
    }
    cout << endl;
}
void fn_drop (istream& in, Environment& env){
    if (env.stack.size () == 0) error ("empty stack", "");
    env.stack.pop_back ();
}
void fn_dup (istream& in, Environment& env){
    if (env.stack.size () == 0) error ("empty stack", "");
    env.stack.push_back (env.stack.back ());
}
void fn_size (istream& in, Environment& env){
   cout << env.stack.size () << endl;
}
void fn_clear (istream& in, Environment& env){
    env.stack.clear ();
}
void fn_help (istream& in, Environment& env){
    for (map<string, Op>::iterator it = env.dict.begin (); it != env.dict.end (); ++it) {
        cout << it->first << " ";
    }
    cout << endl;
    for (map<string, string>::iterator it = env.functions.begin (); it != env.functions.end (); ++it) {
        cout << ": " << it->first << " " << it->second << " ;" << endl;
    }
}
void fn_dump (istream& in, Environment& env) {
    ofstream out ("stack.pcm");
    if (!out.good ()) error ("cannot create output stream", "[dump]");
    for (unsigned i = 0; i < env.stack.size (); ++i) {
        out.write ((char*) &env.stack[i][0], sizeof (float) * env.stack[i].size ());
    }
    out.close ();
}
#define MAKE_ARRAYMETHOD(name, op, label)									\
	void name (istream& in, Environment& env) {						\
        if (env.stack.size () < 2) error ("2 parameters required", label);  \
        farray a = top (env.stack); \
        farray b = top (env.stack); \
        farray r; \
        if (a.size () == 1) r = a[0] op b; \
        else if (b.size () == 1) r = a op b[0]; \
        else r = a op b; \
        env.stack.push_back (r); \
	}\

// // 0.00002267573696 is one sample at 44100
MAKE_ARRAYMETHOD (fn_vadd, +, "[+]");
MAKE_ARRAYMETHOD (fn_vmul, *, "[*]");
MAKE_ARRAYMETHOD (fn_vsub, -, "[-]");
MAKE_ARRAYMETHOD (fn_vdiv, /, "[/]");
void fn_line (istream& in, Environment& env){
    if (env.stack.size () < 3) error ("3 parameters required", "[line]");
    long samples = (long) top (env.stack)[0];
    float end = top (env.stack)[0];
    float init = top (env.stack)[0];
    if (samples <= 0) error ("invalid length speficied", "[line]");
    farray val(samples);
    for (long i = 0; i < samples; ++i) {
        float t   = static_cast<float>(i) / static_cast<float>(samples - 1); // [0,1]
        val[i] = init + t * (end - init);        
    }
    env.stack.push_back (val);
}
#define MAKE_ARRAYOP(name, op,label)									\
	void name (istream& in, Environment& env) {						\
        if (env.stack.size () < 1) error ("1 parameter required", label); \
        farray a = env.stack.at (env.stack.size () - 1);     \
        farray r { a.op () }; \
        env.stack.push_back (r); \
	}\

MAKE_ARRAYOP (fn_min, min, "[min]");
MAKE_ARRAYOP (fn_max, max, "[max]");
MAKE_ARRAYOP (fn_sum, sum, "[sum]");
void fn_join (istream& in, Environment& env){
    if (env.stack.size () < 2) error ("2 parameters required", "[join]");  
    farray b = top (env.stack);
    farray a = top (env.stack);
    farray r(a.size() + b.size());
    r[slice(0,        a.size(), 1)] = a;
    r[slice(a.size(), b.size(), 1)] = b;
    env.stack.push_back (r);
}
void fn_noise (istream& in, Environment& env){
    if (env.stack.size () < 1) error ("1 parameter required", "[noise]");  
    long samples = (long) top (env.stack)[0];
    if (samples <= 0) error ("invalid length speficied", "[noise]");
    farray r (samples);
    for (long i = 0; i < samples; ++i) {
        r[i] = ((float) rand () / RAND_MAX * 2. - 1.);
    }
    env.stack.push_back (r);
}
void fn_osc (istream& in, Environment& env){
    if (env.stack.size () < 2) error ("2 parameters required", "[osc]");  
    stringstream get_sr ("sr");
    eval (get_sr, env);
    float sr = top (env.stack)[0];
    long samples = (long) top (env.stack)[0];
    if (samples <= 0) error ("invalid length speficied", "[osc]");
    farray freq = top (env.stack);
    if (freq.size () == 1) freq = farray (freq[0], samples);
    farray r (samples);
    for (long i = 0; i < samples; ++i) {
        r[i] = (sin (2. * M_PI * (float) i * freq[i] / sr));
    }
    env.stack.push_back (r);
}

// interface
void init_env (Environment& env) {
    env.dict["."] = &fn_print;
    env.dict["#"] = &fn_size;
    env.dict["drop"] = &fn_drop;
    env.dict["dup"] = &fn_dup;
    env.dict["clear"] = &fn_clear;
    env.dict["dump"] = &fn_dump;
    env.dict["help"] = &fn_help;
    env.dict["+"] = &fn_vadd;
    env.dict["*"] = &fn_vmul;
    env.dict["-"] = &fn_vsub;
    env.dict["/"] = &fn_vdiv;
    env.dict["min"] = &fn_min;
    env.dict["max"] = &fn_max;
    env.dict["sum"] = &fn_sum;
    env.dict["join"] = &fn_join;
    env.dict["line"] = &fn_line;    
    env.dict["noise"] = &fn_noise;
    env.dict["osc"] = &fn_osc;
}
void repl (istream& in, Environment& env) {
    while (!in.eof ()) {
        try {
            eval (in, env);
        } catch (exception& e) {
            cerr << "error: " << e.what () << endl;
        } catch (...) {
            cerr << "fatal error" << endl;
        }
    }
}
void load (const string& fname, Environment& env) {
    ifstream in (fname);
    if (!in.good ()) error ("cannot open input file", fname);
    repl (in, env);
}
int main (int argc, char* argv[]) {
    srand (time (NULL));
    cout << "[backsynth, ver. 0.1]" << endl << endl;
    cout << "forth-like sound synthesis environment" << endl;
    cout << "(c) 2025 by Carmine-Emanuele Cella" << endl << endl;

    try {
        Environment env;
        init_env (env);

        if (argc != 1) {
            for (unsigned i = 1; i < argc; ++i) {
                load (argv[i], env);
            }
        }
        else repl (cin, env);
    } catch (exception& e) {
        cerr << "error: " << e.what () << endl;
    } catch (...) {
        cerr << "fatal error" << endl;
    }

    return 0;
}

// eof