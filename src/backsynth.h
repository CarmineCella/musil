// musil.h
//

#ifndef MUSIL_H
#define MUSIL_H

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <iomanip>
#include <random>
#include <valarray>
#include <cmath>

// ast
struct Atom;
typedef std::shared_ptr<Atom> AtomPtr;
typedef double Real;
typedef AtomPtr (*Functor) (AtomPtr, AtomPtr);
inline thread_local std::vector<AtomPtr> eval_stack; // call stack
struct StackGuard {
    StackGuard(AtomPtr node) {
        eval_stack.push_back(node);
    }
    ~StackGuard() {
        eval_stack.pop_back();
    }
};
#define make_atom(a)(std::make_shared<Atom> (a))
enum AtomType {LIST, SYMBOL, STRING, ARRAY, LAMBDA, MACRO, OP};
const char* ATOM_NAMES[] = {"list", "symbol", "string", "array", "lambda", "macro", "op"};
bool is_string (const std::string& l);
void error (const std::string& msg, AtomPtr n);
struct Atom {
	Atom () { type = LIST; }
	Atom (std::string lex) {
		lexeme = lex;
		if (is_string (lexeme)) {
			type = STRING;
			lexeme = lex.substr (1, lex.size () - 1);
		} else {
			type = SYMBOL;
			lexeme = lex;
		}
	}
	Atom (Real val) {
		type = ARRAY;
		array = {val};
	}	
    Atom (const std::vector<Real>& v) {
		type = ARRAY;
		array = std::valarray<Real>(v.data(), v.size());
	}
	Atom (AtomPtr ll) {
		type = LAMBDA;
		tail.push_back (ll->tail.at (0)); // vars
		tail.push_back (ll->tail.at (1)); // body
		tail.push_back (ll->tail.at (2)); // env
	}
	Atom (Functor f) {
		type = OP;
		op = f;
	}
	AtomType type;
	std::string lexeme;
	std::valarray<Real> array;
	Functor op;
	unsigned minargs;
	std::vector <AtomPtr> tail;
};
bool is_nil (AtomPtr e) {
	return (e == nullptr || (e->type == LIST && e->tail.size () == 0));
}

// helpers
bool is_string (const std::string& l) {
	if (l.size () > 1 && l.at (0) == '\"') return true;
	return false;
}
bool is_number (const std::string& tt) {
	const char* t = tt.c_str ();
	std::stringstream dummy;
	dummy << t;
	Real v; dummy >> v;
	return dummy && dummy.eof ();
}
std::ostream& print_valarray(const std::valarray<Real>& v, std::ostream& out = std::cout) {
    out << "[";
    for (size_t i = 0; i < v.size(); ++i) {
        out << v[i];    
        if (i + 1 < v.size()) out << " ";
    }
    out << "]";
    return out;
}
std::ostream& print (AtomPtr e, std::ostream& out, bool write = false) {
	if (e != nullptr) { // to have () printed for nil
		switch (e->type) {
		case LIST:
			out << "(";
			for (unsigned i = 0; i < e->tail.size (); ++i) {
				print (e->tail.at (i), out, write);
				if (i != e->tail.size () - 1) out << " ";
			}
			out << ")";
		break;
		case SYMBOL:
			out << e->lexeme;
		break;
		case STRING:
			if (write) out << "\"" << e->lexeme << "\"";
			else out << e->lexeme;
		break;
		case ARRAY:
			print_valarray (e->array, out) << std::endl;
		break;
		case LAMBDA: case MACRO:
			if (e->type == LAMBDA) out << "(lambda ";
			else out << "(macro ";
			print (e->tail.at (0), out, write) << " "; // vars
			print (e->tail.at (1), out, write) << ")"; // body
 		break;
		case OP:
			if (write) out << e->lexeme;
			else out << "<op @ " << (std::hex) << &e->op << ">";
		break;
		}
	}
	return out;
}
void error (const std::string& msg, AtomPtr n) {
	std::stringstream err;
	err << msg;
	if (!is_nil (n)) {
		err << " -> ";
		print (n, err);
	}
    if (eval_stack.size () > 1) {
        err << "\n\n[--- stack trace ---]" << std::endl;
		int ctx = eval_stack.size (); 
        for (auto it = eval_stack.rbegin(); it != eval_stack.rend(); ++it) {
        	err << ctx << "> "; print(*it, err) << std::endl;
			if (ctx > 1) err << std::endl;
			--ctx;
        }
		err << "[--- end of stack trace ---]\n";
    }	
	throw std::runtime_error (err.str ());
}
AtomPtr args_check (AtomPtr node, unsigned args) {
	std::stringstream err;
	err << "insufficient number of arguments (required " << args << ", got " << node->tail.size () << ")";
	if (node->tail.size () < args) error (err.str (), node);
	return node;
}
AtomPtr type_check (AtomPtr node, AtomType t) {
	std::stringstream err;
	err << "invalid type (required " << ATOM_NAMES[t] << ", got " << ATOM_NAMES[node->type] << ")";
	if (node->type != t) error (err.str (), node);
	return node;
}

// lexing, parsing, evaluation
std::string next (std::istream &in, unsigned& linenum) {
	std::stringstream accum;
	while (!in.eof ()) {
		char c = in.get ();
		switch (c) {
			case ';':
			do { c = in.get (); } while (c != '\n' && !in.eof ());
			++linenum;
			break;
			case '(': case ')': case '\'':
				if (accum.str ().size ()) {
					in.putback (c);
					return accum.str ();
				} else {
					accum << c;
					return accum.str ();
				}
			break;
			case '\t': case '\n': case '\r': case ' ':
				if (c == '\n') ++linenum;
				if (accum.str ().size ()) return accum.str ();
				else continue;
			break;
			case '\"':
			if (accum.str ().size ()) {
				in.putback(c);
				return accum.str ();
			} else {
				accum << c;
				while (!in.eof ()) {
					in.get (c);
					if (c == '\n') ++linenum;
					if (c == '\"') break;
					else if (c == '\\') {
						c = in.get ();
						switch (c) {
							case 'n': accum <<'\n'; break;
							case 'r': accum <<'\r'; break;
							case 't': accum <<'\t'; break;
							case '\"': accum << "\""; c = 0; break;
						}
					} else accum << c;
				}
				return accum.str ();
			}
			break; 
			default:
				if (c > 0) accum << c;
			break;
		}
	}
	return accum.str ();
}
AtomPtr read (std::istream& in, unsigned& linenum) {
	std::string token = next (in, linenum);
	if (!token.size ()) return make_atom();
	if (token == "(") {
		AtomPtr l = make_atom ();
		while (!in.eof ()) {
			AtomPtr n = read (in, linenum);
			if (n->lexeme == ")") break;
			else l->tail.push_back (n);
		}
		return l;
	} 
    else if (token == "\'") {
		AtomPtr ll = make_atom();
		ll->tail.push_back (make_atom ("quote"));
		ll->tail.push_back (read (in, linenum));
		return ll;
	} else if (is_number (token)) {
		return make_atom(atof (token.c_str ()));
	}
	else {
		return make_atom (token);
	}
}
bool atom_eq (AtomPtr a, AtomPtr b) {
	if (is_nil (a) && !is_nil (b)) return false;
	if (!is_nil (a) && is_nil (b)) return false;
	if (is_nil (a) && is_nil (b)) return true;
	if (a->type != b->type) return false;
	switch (a->type) {
		case LIST:
			if (a->tail.size () != b->tail.size ()) return false;
			for (unsigned i = 0; i < a->tail.size (); ++i) {
				if (! atom_eq (a->tail.at (i), b->tail.at (i))) return false;
			}
			return true;
		break;
		case SYMBOL: case STRING:
			return a->lexeme == b->lexeme;
		break;
		case ARRAY: { 
            Real eps = 1e-6f;
            return a->array.size() == b->array.size() &&
           ((a->array - b->array).apply(fabs)).max() < eps;
        } break;
		case LAMBDA: case MACRO:
			if (a->tail.at (0) != b->tail.at (0)) return false;
			if (a->tail.at (1) != b->tail.at (1)) return false;
			return true; // env not checked in comparison
		break;
		case OP:
			return a->op == b->op;
		break;
	}
	return false; // dummy
}
AtomPtr assoc (AtomPtr node, AtomPtr env) {
	for (unsigned i = 1; i < env->tail.size (); ++i) {
		AtomPtr vv = env->tail.at (i);
		if (atom_eq (node, vv->tail.at (0))) return vv->tail.at(1);
	}
	if (!is_nil (env->tail.at (0))) return assoc (node, env->tail.at (0));
	error ("unbound identifier", node);
	return make_atom (); // dummy
}
AtomPtr extend (AtomPtr node, AtomPtr val, AtomPtr env, bool recurse = false) {
	for (unsigned i = 1; i < env->tail.size (); ++i) {
		AtomPtr vv = env->tail.at (i);
		if (atom_eq (node, vv->tail.at (0))) {
			vv->tail.at(1) = val;
			return val;
		}
	}
	if (recurse) { // set
		if (!is_nil (env->tail.at (0))) return extend (node, val, env->tail.at (0), recurse);
		error ("unbound identifier", node);
	} else {
		AtomPtr vv = make_atom();
		vv->tail.push_back (node);
		vv->tail.push_back (val);
		env->tail.push_back (vv);
		return val;
	}
	
	return make_atom(); // dummy
}
AtomPtr fn_quote (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_def (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_set (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_lambda (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_macro (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_if (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_while (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_begin (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_apply (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr fn_eval (AtomPtr, AtomPtr) { return nullptr; } // dummy
AtomPtr eval (AtomPtr node, AtomPtr env) {
	StackGuard guard(node); 
	while (true) {
		if (is_nil (node)) return make_atom ();
		if (node->type == SYMBOL && node->lexeme.size ()) return assoc (node, env);
		if (node->type != LIST) return node;

		AtomPtr func = eval (node->tail.at (0), env);
		if (func->op == &fn_quote) {
			args_check (node, 2);
			return node->tail.at (1);
		}
		if (func->op == &fn_def) {
			args_check (node, 3);
			return extend (type_check (node->tail.at (1), SYMBOL), eval (node->tail.at (2), env), env);
		}
		if (func->op == &fn_set) {
			args_check (node, 3);
			return extend (type_check (node->tail.at (1), SYMBOL), eval (node->tail.at (2), env), env, true);
		}
		if (func->op == &fn_lambda || func->op == &fn_macro) {
			args_check (node, 3);
			AtomPtr ll = make_atom();
			ll->tail.push_back (type_check (node->tail.at (1), LIST)); // vars
			AtomPtr body = make_atom ();
			for (unsigned i = 2; i < node->tail.size (); ++i) {
				body->tail.push_back (node->tail.at (i));
			}
			ll->tail.push_back (body); // body
			ll->tail.push_back (env); // env (lexical scope)
			AtomPtr f = make_atom(ll); // lambda
			if (func->op == &fn_macro) f->type = MACRO;
			return f;
		}
		if (func->op == &fn_if) {
			args_check (node, 3);
			if (type_check (eval (node->tail.at (1), env), ARRAY)->array[0]) {
				node = node->tail.at (2);
				continue; 
			} else {
				if (node->tail.size () == 4) {
					node = node->tail.at (3);
					continue; 
				} else return make_atom ();
			}
		}
		if (func->op == &fn_while) {
			args_check (node, 3);
			AtomPtr r = make_atom ();
			while (type_check (eval (node->tail.at (1), env), ARRAY)->array[0]) {
				r = eval (node->tail.at (2), env);
			}
			return r;
		}	
		if (func->op == &fn_begin) {
			args_check (node, 2);
			for (unsigned i = 0; i < node->tail.size () - 1; ++i) {
				eval (node->tail.at (i), env);
			} 
			node = node->tail.at (node->tail.size () - 1);
			continue; 
		}
		AtomPtr args = make_atom();
		for (unsigned i = 1; i < node->tail.size (); ++i) {
			args->tail.push_back ((func->type == MACRO ? node->tail.at (i) : eval (node->tail.at (i), env)));
		}
		if (func->type == LAMBDA || func->type == MACRO) {
			AtomPtr vars = func->tail.at (0);
			AtomPtr body = func->tail.at (1);
			AtomPtr nenv = make_atom ();
			nenv->tail.push_back (func->tail.at (2)); // new environment with static binding

			if (vars->tail.size () < args->tail.size ()) error ("too many arguments in lambda/macro", node);
			unsigned minargs = (vars->tail.size () > args->tail.size () ? args->tail.size () : vars->tail.size ());
			for (unsigned i = 0; i < minargs; ++i) {
				extend (vars->tail.at (i), args->tail.at (i), nenv);
			}

			if (vars->tail.size () > args->tail.size ()) {		
				AtomPtr vars_cut = make_atom ();
				for (unsigned i = 0; i < minargs; ++i) {
					vars_cut->tail.push_back (vars->tail.at (i));
				}	
				AtomPtr new_lambda = make_atom (); 
				new_lambda->tail.push_back (vars_cut);
				new_lambda->tail.push_back (body);
				new_lambda->tail.push_back (nenv);
				AtomPtr f = make_atom (new_lambda); // return lambda/macro with bounded vars
				if (func->type == MACRO) f->type = MACRO;
				return f;
			}
			env = nenv;
			for (unsigned i = 0; i < body->tail.size () - 1; ++i) {
				eval ((func->type == MACRO ? eval (body->tail.at (i), nenv) : body->tail.at (i)), nenv);
			}
			node = (func->type == MACRO ? eval (body->tail.at (body->tail.size () - 1), nenv) 
				: body->tail.at (body->tail.size () - 1));		
			continue; 
		}
		if (func->type == OP) {
			args_check (args, func->minargs);
			if (func->op == &fn_eval) {
				node = args->tail.at (0);
				continue; 
			}	
			if (func->op == &fn_apply) {
				AtomPtr l = type_check (args->tail.at (1), LIST);
				l->tail.insert (l->tail.begin(), args->tail.at(0));

				node = l;
				continue; 
			}			
			return func->op (args, env);
		}	
		error ("function expected", node);
	}
}

// functors
AtomPtr fn_env (AtomPtr node, AtomPtr env) {
	if (node->tail.size () && type_check(node->tail.at(0), SYMBOL)->lexeme == "full") return env;
	AtomPtr l = make_atom();
	for (unsigned i = 1; i < env->tail.size (); ++i) l->tail.push_back (env->tail.at (i)->tail.at (0));
	return l;
}
AtomPtr fn_type (AtomPtr node, AtomPtr env) {
	return make_atom (ATOM_NAMES[node->tail.at (0)->type]);
}
AtomPtr fn_list (AtomPtr node, AtomPtr env) {
	return node;
}
AtomPtr fn_cons(AtomPtr node, AtomPtr env) {
	AtomPtr result = make_atom ();
    result->tail.push_back(node->tail.at (0));
    if (node->tail.at (1)->type == LIST) {
        result->tail.insert(result->tail.end(), node->tail.at (1)->tail.begin(), node->tail.at (1)->tail.end());
    } else result->tail.push_back(node->tail.at (1));
    return result;
}
AtomPtr fn_car (AtomPtr node, AtomPtr env) {
	if (!node->tail.at (0)->tail.size ()) return make_atom();
	return node->tail.at (0)->tail.at (0);
}
AtomPtr fn_cdr (AtomPtr node, AtomPtr env) {
	if (!node->tail.at (0)->tail.size ()) return make_atom();
	AtomPtr cdr = make_atom ();
	for (unsigned i = 1; i < node->tail.at (0)->tail.size (); ++i) {
		cdr->tail.push_back (node->tail.at (0)->tail.at (i));
	}
	return cdr;
}
void append_valarray(std::vector<Real>& dst, const std::valarray<Real>& src) {
    dst.insert(dst.end(), std::begin(src), std::end(src));
}
AtomPtr fn_array (AtomPtr node, AtomPtr env) {
    std::vector<Real> v;
    for (unsigned i = 0; i < node->tail.size (); ++i) {
        append_valarray (v, type_check (node->tail.at (i), ARRAY)->array);
    }
	return make_atom (v);
}
AtomPtr fn_eq (AtomPtr node, AtomPtr env) {
	return make_atom ((Real) atom_eq (node->tail.at (0), node->tail.at (1)));
}
#define MAKE_ARRAYMETHOD(name, op)									\
	AtomPtr name (AtomPtr node, AtomPtr env) {						\
        std::valarray<Real> a = type_check (node->tail.at (0), ARRAY)->array; \
        std::valarray<Real> b = type_check (node->tail.at (1), ARRAY)->array; \
        AtomPtr r = make_atom (); \
        r->type = ARRAY; \
        if (a.size () == 1) r->array = a[0] op b; \
        else if (b.size () == 1) r->array = a op b[0]; \
        else r->array = a op b; \
        return r; \
	}\

MAKE_ARRAYMETHOD (fn_vadd, +);
MAKE_ARRAYMETHOD (fn_vmul, *);
MAKE_ARRAYMETHOD (fn_vsub, -);
MAKE_ARRAYMETHOD (fn_vdiv, /);
MAKE_ARRAYMETHOD (fn_less, <);
MAKE_ARRAYMETHOD (fn_leq, <=);
MAKE_ARRAYMETHOD (fn_greater, >);
MAKE_ARRAYMETHOD (fn_geq, >=);

#define MAKE_ARRAYOP(name, op)									\
	AtomPtr name (AtomPtr node, AtomPtr env) {						\
        std::valarray<Real> a = type_check (node->tail.at (0), ARRAY)->array; \
        AtomPtr r = make_atom (); \
        r->type = ARRAY; \
        r->array = a.op (); \
        return r; \
	}\

MAKE_ARRAYOP (fn_min, min);
MAKE_ARRAYOP (fn_max, max);
MAKE_ARRAYOP (fn_sum, sum);
template <bool WRITE>
AtomPtr fn_print (AtomPtr node, AtomPtr env) {
	std::ostream* out = &std::cout;
	if (WRITE) {
		out = new std::ofstream (type_check (node->tail.at (0), STRING)->lexeme);
		if (!out->good ()) error ("cannot create output file", node);
	}
	for (unsigned int i = WRITE; i < node->tail.size (); ++i) {
		print (node->tail.at (i), *out, WRITE);
	}
	if (WRITE) ((std::ofstream*) out)->close (); // downcast
	return make_atom ("");
}
AtomPtr fn_read (AtomPtr node, AtomPtr env) {
	unsigned linenum = 0;
	if (node->tail.size ()) {
		std::ifstream in (type_check (node->tail.at (0), STRING)->lexeme);
		if (!in.good ()) error ("cannot open input file", node);
		AtomPtr r = make_atom ();
		while (!in.eof ()) {
			AtomPtr l = read (in, linenum);
			if (!in.eof ()) r->tail.push_back (l);
		}
		return r;
	} else return read (std::cin, linenum);
}
void replace (std::string &s, std::string from, std::string to) {
	int idx = 0;
	size_t next;
	while ((next = s.find (from, idx)) != std::string::npos) {
		s.replace (next, from.size (), to);
		idx = next + to.size ();
	} 
}
std::vector<std::string> split (const std::string& in, char separator) {
	std::istringstream iss(in);
	std::string s;
	std::vector<std::string> tokens;
	while (std::getline(iss, s, separator)) {
		tokens.push_back (s);
	}
	return tokens;
}
AtomPtr fn_string (AtomPtr node, AtomPtr env) {
	std::string cmd = type_check (node->tail.at (0), SYMBOL)->lexeme;
	AtomPtr l = make_atom();
	std::regex r;
	if (cmd == "length") { // argnum checked by default
		return make_atom(type_check (node->tail.at(1), STRING)->lexeme.size ());
	} else if (cmd == "find") {
		args_check (node, 3);
		unsigned long pos = type_check (node->tail.at(1), STRING)->lexeme.find (
			type_check (node->tail.at(2), STRING)->lexeme);
		if (pos == std::string::npos) return make_atom (-1);
		else return make_atom (pos);		
	} else if (cmd == "range") {
		args_check (node, 4);
		std::string tmp = type_check (node->tail.at(1), STRING)->lexeme.substr(
			type_check (node->tail.at(2), ARRAY)->array[0], 
			type_check (node->tail.at(3), ARRAY)->array[0]);
		return make_atom ((std::string) "\"" + tmp);
	} else if (cmd == "replace") {
		args_check (node, 4);
		std::string tmp = type_check (node->tail.at(1), STRING)->lexeme;
		replace (tmp,
			type_check (node->tail.at(2), STRING)->lexeme, 
			type_check (node->tail.at(3), STRING)->lexeme);
		return make_atom((std::string) "\"" + tmp);
	} else if (cmd == "split") {
		args_check (node, 3);
		std::string tmp = type_check (node->tail.at(1), STRING)->lexeme;
		char sep =  type_check (node->tail.at(2), STRING)->lexeme[0];
		std::vector<std::string> tokens = split (tmp, sep);
		AtomPtr l = make_atom ();
		for (unsigned i = 0; i < tokens.size (); ++i) l->tail.push_back (make_atom ((std::string) "\"" + tokens[i]));
		return l;
	} else if (cmd == "regex") {
		args_check (node, 3);
		std::string str = type_check (node->tail.at(1), STRING)->lexeme;
		std::regex r (type_check (node->tail.at(2), STRING)->lexeme);
		std::smatch m; 
		std::regex_search(str, m, r);
		AtomPtr l = make_atom();
		for(auto v: m) {
			l->tail.push_back (make_atom ((std::string) "\"" + v.str()));
		}
		return l;		
	} 
	return l;
}
AtomPtr load (const std::string&fname, std::istream& in, AtomPtr env) {
	AtomPtr r;
	unsigned linenum = 0;
	while (!in.eof ()) {
		try {
			AtomPtr l = read (in, linenum);
			if (!in.eof ()) r = eval (l, env);
		} catch (std::exception& e) {
			std::cerr << "[" << fname << ":" << linenum << "] " << e.what () << std::endl;
		} catch (...) {
			std::cerr << "unknown error detected" << std::endl;
		}
	}
	return r;
}
AtomPtr fn_load (AtomPtr node, AtomPtr env) {
	std::ifstream in (type_check (node->tail.at (0), STRING)->lexeme);
	if (!in.good ()) error ("cannot open input file", node);
	return load (node->tail.at (0)->lexeme, in, env);
}
AtomPtr fn_exec (AtomPtr node, AtomPtr env) {
	return make_atom (system (type_check (node->tail.at (0), STRING)->lexeme.c_str ()));
}
AtomPtr fn_exit (AtomPtr node, AtomPtr env) {
	std::cout << std::endl;
	exit (0);
	return make_atom ();
}

// interface
void add_op (const std::string& lexeme, Functor f, int minargs, AtomPtr env) {
	AtomPtr op = make_atom(f);
	op->lexeme = lexeme;
	op->minargs = minargs;
	extend (make_atom(lexeme), op, env);
}
AtomPtr make_env () {
	AtomPtr env = make_atom ();
	env->tail.push_back (make_atom ()); // nil parent
	add_op ("quote", &fn_quote, -1, env); // -1 are checked in the handling function
	add_op ("def", &fn_def, -1, env);
	add_op ("set", &fn_set, -1, env);
	add_op ("lambda", &fn_lambda, -1, env);
	add_op ("macro", &fn_macro, -1, env);
	add_op ("if", &fn_if, -1, env);
	add_op ("while", &fn_while, -1, env);
	add_op ("begin", &fn_begin, -1, env);
	add_op ("eval", &fn_eval, 1, env);
	add_op ("apply", &fn_apply, 2, env);
	add_op ("env", &fn_env, 0, env);
	add_op ("type", &fn_type, 1, env);    
	add_op ("list", &fn_list, 0, env);
	add_op ("cons", &fn_cons, 2, env);
	add_op ("car", &fn_car, 1, env);
	add_op ("cdr", &fn_cdr, 1, env);
    add_op ("array", &fn_array, 0, env);    
	add_op ("eq", &fn_eq, 2, env);
    add_op ("add", &fn_vadd, 2, env);
    add_op ("sub", &fn_vsub, 2, env);
    add_op ("mul", &fn_vmul, 2, env);
    add_op ("div", &fn_vdiv, 2, env);
    add_op ("<", &fn_less, 2, env);
    add_op ("<=", &fn_leq, 2, env);
    add_op (">", &fn_greater, 2, env);
    add_op (">=", &fn_geq, 2, env);    
    add_op ("min", &fn_min, 1, env);    
    add_op ("max", &fn_max, 1, env);    
    add_op ("sum", &fn_sum, 1, env);    
	add_op ("display", &fn_print<false>, 1, env);
	add_op ("save", &fn_print<true>, 2, env);
	add_op ("read", &fn_read, 0, env);
    add_op ("str", &fn_string, 2, env);
	add_op ("load", &fn_load, 0, env);
	add_op ("exec", &fn_exec, 1, env);
	add_op ("exit", &fn_exit, 0, env);
	return env;
}
void repl (std::istream& in, std::ostream& out, AtomPtr env) {
	unsigned linenum = 0;;
	while (true) {
		std::cout << ">> " << std::flush;
		try {
			print (eval (read (in, linenum), env), std::cout) << std::endl;
		} catch (std::exception& err) {
			std::cerr << "error: " << err.what () << std::endl;
		} catch (...) {
			std::cerr << "unknown error detected" << std::endl;
		}
	}
}

#endif // MUSIL_H

// eof
