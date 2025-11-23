// core.h -- core language components for Musil
//

#ifndef CORE_H
#define CORE_H

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
#include <thread>
#include <algorithm>
#include <iterator>
#include <functional>
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
	Atom (const std::valarray<Real>& a) {
		type = ARRAY;
		array = {a};
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
			print_valarray (e->array, out);
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
				if (accum.str().size()) {
					in.putback(c);
					return accum.str();
				} else {
					accum << c;  // opening quote
					bool closed = false;
					while (!in.eof()) {
						in.get(c);
						if (c == '\n') ++linenum;
						if (c == '\"') {
							closed = true;
							break;
						} else if (c == '\\') {
							c = in.get();
							if (c == 'n')      accum << '\n';
							else if (c == 'r') accum << '\r';
							else if (c == 't') accum << '\t';
							else if (c == '\"') accum << "\"";
							else if (c == '\\') accum << '\\';
						} else {
							accum << c;
						}
					}
					if (!closed) {
						// EOF reached before closing quote
						throw std::runtime_error("unterminated string literal");
					}
					return accum.str();
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
    std::string token = next(in, linenum);
    if (!token.size()) return nullptr; // EOF sentinel

    if (token == "(") {
        AtomPtr l = make_atom();
        while (true) {
            AtomPtr n = read(in, linenum);
            if (!n) {
                if (in.eof()) {
                    error("unexpected EOF while reading list", l);
                } else {
                    continue;
                }
            }
            if (n->type == SYMBOL && n->lexeme == ")") {
                break;
            }
            l->tail.push_back(n);
        }
        return l;
    } else if (token == "\'") {
        AtomPtr ll = make_atom();
        ll->tail.push_back(make_atom("quote"));
        AtomPtr quoted = read(in, linenum);
        if (!quoted && in.eof()) {
            error("unexpected EOF after quote", ll);
        }
        ll->tail.push_back(quoted);
        return ll;
    } else if (is_number(token)) {
        return make_atom(atof(token.c_str()));
    } else {
        return make_atom(token);
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
AtomPtr clone(AtomPtr n) {
    if (is_nil(n)) return make_atom();
    AtomPtr r = make_atom();
    r->type   = n->type;
    r->lexeme = n->lexeme;
    r->array  = n->array;
    r->op     = n->op;
    r->minargs = n->minargs;
    for (auto& t : n->tail) {
        r->tail.push_back(clone(t));
    }
    return r;
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
			return clone(node->tail.at(1));
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

			if (vars->tail.size () < args->tail.size ()) error ("[lambda/macro] too many arguments", node);
			unsigned minargs = (vars->tail.size () > args->tail.size () ? args->tail.size () : vars->tail.size ());
			for (unsigned i = 0; i < minargs; ++i) {
				extend (vars->tail.at (i), args->tail.at (i), nenv);
			}

			
			if (vars->tail.size () > args->tail.size ()) {		
				AtomPtr vars_rest = make_atom();
				for (unsigned i = minargs; i < vars->tail.size(); ++i) {
					vars_rest->tail.push_back(vars->tail.at(i));
				}

				AtomPtr new_lambda = make_atom();
				new_lambda->tail.push_back(vars_rest);
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
// helper: collect all variable names from env (current + parents)
void browse_env(AtomPtr env, AtomPtr vars) {
    for (unsigned i = 1; i < env->tail.size(); ++i) {
        AtomPtr binding = env->tail.at(i);
        if (!is_nil(binding) && binding->tail.size() >= 1) {
            vars->tail.push_back(binding->tail.at(0)); // symbol
        }
    }
    if (!is_nil(env->tail.at(0))) {
        browse_env(env->tail.at(0), vars);
    }
}
AtomPtr fn_info(AtomPtr b, AtomPtr env) {
    std::string cmd = type_check(b->tail.at(0), SYMBOL)->lexeme;
    AtomPtr l = make_atom();
    std::regex r;

    if (cmd == "vars") {
        // (info vars) or (info vars "regex")
        std::string pattern = ".*";
        if (b->tail.size() > 1) {
            // second arg should be a STRING with the regex pattern
            AtomPtr pat = type_check(b->tail.at(1), STRING);
            pattern = pat->lexeme;
        }
        r.assign(pattern);

        AtomPtr vars = make_atom();
        browse_env(env, vars);

        for (unsigned i = 0; i < vars->tail.size(); ++i) {
            std::string k = vars->tail.at(i)->lexeme;
            if (std::regex_match(k, r)) {
                // keep the same symbol node
                l->tail.push_back(vars->tail.at(i));
            }
        }
    } else if (cmd == "exists") {
        // (info exists 'a 'b ...)
        for (unsigned i = 1; i < b->tail.size(); ++i) {
            AtomPtr key = type_check(b->tail.at(i), SYMBOL);
            Real ans = 1;
            try {
                AtomPtr r = assoc(key, env);
                (void)r;
            } catch (...) {
                ans = 0;
            }
            l->tail.push_back(make_atom(ans));
        }
    } else if (cmd == "typeof") {
        for (unsigned i = 1; i < b->tail.size(); ++i) {
            AtomPtr v = b->tail.at(i);
            l->tail.push_back(make_atom(std::string(ATOM_NAMES[v->type])));
        }
    } else {
        error("[info] invalid request", b->tail.at(0));
    }
    return l;
}
AtomPtr fn_list (AtomPtr node, AtomPtr env) {
	return node;
}
AtomPtr fn_lindex (AtomPtr node, AtomPtr env) {
	AtomPtr o = type_check (node->tail.at (0), LIST);
	int p  = (int) type_check (node->tail.at (1), ARRAY)->array[0];
	if (!o->tail.size ()) return make_atom  ();
	if (p < 0 || p >= o->tail.size ()) error ("[lindex] invalid index", node);
	return o->tail.at (p);
}
AtomPtr fn_lset (AtomPtr node, AtomPtr env) {
	AtomPtr o = type_check (node->tail.at (0), LIST);
	AtomPtr e = node->tail.at (1);
	int p  = (int) type_check (node->tail.at (2), ARRAY)->array[0];
	if (!o->tail.size ()) return make_atom  ();
	if (p < 0 || p >= o->tail.size ()) error ("[lset] invalid index", node);
	o->tail.at (p) = e;
	return o;
}    
AtomPtr fn_llength (AtomPtr node, AtomPtr env) {
	return make_atom (type_check (node->tail.at (0), LIST)->tail.size ());
}
AtomPtr fn_lappend (AtomPtr n, AtomPtr env) {
	AtomPtr dst = type_check (n->tail.at(0), LIST);
	for (unsigned i = 1; i < n->tail.size (); ++i){
		dst->tail.push_back (n->tail.at (i));
	}	
	return dst;
}
AtomPtr fn_lrange (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->tail.at (0), LIST);
	int i = (int) (type_check(params->tail.at (1), ARRAY)->array[0]);
	int len = (int) (type_check(params->tail.at (2), ARRAY)->array[0]);
	int end = i + len;
	int stride = 1;
	if (params->tail.size () == 4) {
		stride  = (int) (type_check(params->tail.at (3), ARRAY)->array[0]);
	}
	if (i < 0) i = 0;
	if (len < i) len = i;
	if (end > l->tail.size ()) end = l->tail.size ();
	AtomPtr nl = make_atom();
	for (int j = i; j < end; j += stride) nl->tail.push_back(l->tail.at (j));
	return nl;
}
AtomPtr fn_lreplace (AtomPtr params, AtomPtr env) {
	AtomPtr l = type_check (params->tail.at (0), LIST);
	AtomPtr r = type_check (params->tail.at (1), LIST);
	int i = (int) (type_check(params->tail.at (2), ARRAY)->array[0]);
	int len = (int) (type_check(params->tail.at (3), ARRAY)->array[0]);
	int stride = 1;
	if (params->tail.size () == 5) {
		stride  = (int) (type_check(params->tail.at (4), ARRAY)->array[0]);
	}
	if (i < 0 || len < 0 || stride < 1 || i + len  > l->tail.size () || (int) (len / stride) > r->tail.size ()) {
		return make_atom();
	}
	AtomPtr nl = make_atom();
	int p = 0;
	for (int j = i; j < i + len; j += stride) {
		l->tail.at(j) = r->tail.at (p);
		++p;
	}
	return r;
}    
AtomPtr fn_lshuffle (AtomPtr node, AtomPtr env) {
	std::random_device rd;
	std::mt19937 g(rd());
	AtomPtr ll = make_atom ();
	ll->tail = type_check (node->tail.at (0), LIST)->tail;
	std::shuffle (ll->tail.begin (), ll->tail.end (), g);
	return ll;
}
// void append_valarray(std::vector<Real>& dst, const std::valarray<Real>& src) {
//     dst.insert(dst.end(), std::begin(src), std::end(src));
// }
// AtomPtr fn_array (AtomPtr node, AtomPtr env) {
//     std::vector<Real> v;
//     for (unsigned i = 0; i < node->tail.size (); ++i) {
//         append_valarray (v, type_check (node->tail.at (i), ARRAY)->array);
//     }
// 	return make_atom (v);
// }
void list2array (AtomPtr list, std::vector<Real>& out) {
	for (unsigned i = 0; i < list->tail.size (); ++i) {
		if (list->tail.at (i)->type == LIST) {
			list2array (list->tail.at (i), out);
		}  else if (list->tail.at (i)->type == ARRAY) {
				for (unsigned k = 0; k < list->tail.at (i)->array.size (); ++k) {
				out.push_back (list->tail.at (i)->array[k]);
			}
		} else {
			error ("numeric or list expected", list);
		}
	}
}

AtomPtr fn_array (AtomPtr node, AtomPtr env) {
	std::vector<Real> res;
	list2array (node, res);
	std::valarray<Real> f (res.data (), res.size ());
	return make_atom (f);
}
AtomPtr array2list (const std::valarray<Real>& out) {
	AtomPtr list = make_atom ();
	for (unsigned i = 0; i < out.size (); ++i) {
		list->tail.push_back (make_atom (out[i]));
	}
	return list->tail.size () == 1 ? list->tail.at (0) : list;
}    
AtomPtr fn_array2list (AtomPtr node, AtomPtr env) {
	return array2list (type_check (node->tail.at (0), ARRAY)->array);
}       
AtomPtr fn_eq (AtomPtr node, AtomPtr env) {
	return make_atom ((Real) atom_eq (node->tail.at (0), node->tail.at (1)));
}
#define MAKE_ARRAYBINOP(op,name) 	\
	AtomPtr name (AtomPtr n, AtomPtr env) { 	\
		std::valarray<Real> res (type_check (n->tail.at (0), ARRAY)->array); \
		for (unsigned i = 1; i < n->tail.size (); ++i) {  \
			std::valarray<Real>& a = type_check (n->tail.at (i), ARRAY)->array; \
			if (a.size () == 1) res = res op a[0]; \
			else res = res op a; \
		} \
		return make_atom (res); \
	} \

MAKE_ARRAYBINOP (+, fn_add);
MAKE_ARRAYBINOP (-, fn_sub);
MAKE_ARRAYBINOP (*, fn_mul);
MAKE_ARRAYBINOP (/, fn_div);
#define MAKE_ARRAYCMPOP(op,name) 	\
	AtomPtr name (AtomPtr n, AtomPtr env) { 	\
		std::valarray<bool> res; \
		for (unsigned i = 0; i < n->tail.size () - 1; ++i) {  \
			std::valarray<Real>& a = type_check (n->tail.at (i), ARRAY)->array; \
			std::valarray<Real>& b = type_check (n->tail.at (i + 1), ARRAY)->array; \
			std::valarray<bool> a_bool (a > 0); \
			std::valarray<bool> b_bool (b > 0); \
			if (b.size () == 1) res = a op  b[0]; \
			else res = a op b; \
			if (std::all_of(std::begin(res), std::end(res), [](bool i) { return !i; })) { \
				break; \
			} \
		} \
		std::valarray<Real> res_r (res.size ()); \
		for (unsigned i = 0; i < res.size (); ++i) res_r[i] = (Real) res[i]; \
		return make_atom (res_r); \
	} \

MAKE_ARRAYCMPOP (>, fn_greater);
MAKE_ARRAYCMPOP (>=, fn_greatereq);
MAKE_ARRAYCMPOP (<, fn_less);
MAKE_ARRAYCMPOP (<=, fn_lesseq);
#define MAKE_ARRAYMETHODS(op,name)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		std::valarray<Real> res (n->tail.size ()); \
		for (unsigned i = 0; i < n->tail.size (); ++i) { \
			res[i] = (type_check (n->tail.at (i), ARRAY)->array.op ()); \
		}\
		return make_atom (res);\
	}\

MAKE_ARRAYMETHODS (min, fn_min);
MAKE_ARRAYMETHODS (max, fn_max);
MAKE_ARRAYMETHODS (sum, fn_sum);
MAKE_ARRAYMETHODS (size, fn_size);
#define MAKE_ARRAYSINGOP(op,name)									\
	AtomPtr name (AtomPtr n, AtomPtr env) {						\
		AtomPtr res = make_atom (); \
		for (unsigned i = 0; i < n->tail.size (); ++i) { \
			std::valarray<Real> v = op (type_check (n->tail.at (i), ARRAY)->array); \
			res->tail.push_back (make_atom (v)); \
		}\
		return res->tail.size () == 1 ? res->tail.at (0) : res; \
	}\

MAKE_ARRAYSINGOP (std::abs, fn_abs);
MAKE_ARRAYSINGOP (exp, fn_exp);
MAKE_ARRAYSINGOP (log, fn_log);
MAKE_ARRAYSINGOP (log10, fn_log10);
MAKE_ARRAYSINGOP (sqrt, fn_sqrt);
MAKE_ARRAYSINGOP (sin, fn_sin);
MAKE_ARRAYSINGOP (cos, fn_cos);
MAKE_ARRAYSINGOP (tan, fn_tan);
MAKE_ARRAYSINGOP (asin, fn_asin);
MAKE_ARRAYSINGOP (acos, fn_acos);
MAKE_ARRAYSINGOP (atan, fn_atan);
MAKE_ARRAYSINGOP (sinh, fn_sinh);
MAKE_ARRAYSINGOP (cosh, fn_cosh);
MAKE_ARRAYSINGOP (tanh, fn_tanh);
AtomPtr fn_neg (AtomPtr n, AtomPtr env) {						
	AtomPtr res = make_atom (); 
	for (unsigned i = 0; i < n->tail.size (); ++i) { 
		std::valarray<Real> v = -(type_check (n->tail.at (i), ARRAY)->array); 
		res->tail.push_back (make_atom (v)); 
	}
	return res->tail.size () == 1 ? res->tail.at (0) : res; 
}
AtomPtr fn_floor (AtomPtr n, AtomPtr env) {						
	AtomPtr res = make_atom (); 
	for (unsigned i = 0; i < n->tail.size (); ++i) { 
		std::valarray<Real> v (type_check (n->tail.at (i), ARRAY)->array.size ());
		for (unsigned j = 0; j < n->tail.at (i)->array.size (); ++j) {
			v[j] = floor (n->tail.at (i)->array[j]); 
		} 
		res->tail.push_back (make_atom (v)); 
	}
	return res->tail.size () == 1 ? res->tail.at (0) : res; 
}
AtomPtr fn_slice (AtomPtr node, AtomPtr env) {
	std::valarray<Real>& input = type_check (node->tail.at (0), ARRAY)->array;
	int i = (int) type_check  (node->tail.at (1), ARRAY)->array[0];
	int len = (int) type_check  (node->tail.at (2), ARRAY)->array[0];
	int stride = 1;

	if (node->tail.size () == 4) stride = (int) type_check  (node->tail.at (3), ARRAY)->array[0];
	if (i < 0 || len < 1 || stride < 1) {
		error ("[slice] invalid indexing", node);
	}
	int j = i; 
	int ct = 0;
	while (j < input.size ()) {
		if (ct >= len) break;
		j += stride;
		++ct;
	}
	std::valarray<Real> s = input[std::slice (i, ct, stride)];
	return make_atom (s);
}    
AtomPtr fn_assign (AtomPtr node, AtomPtr env) {
	std::valarray<Real>& v1 = type_check (node->tail.at (0), ARRAY)->array;
	std::valarray<Real>& v2 = type_check (node->tail.at (1), ARRAY)->array;
	int i = (int) type_check  (node->tail.at (2), ARRAY)->array[0];
	int len = (int) type_check  (node->tail.at (3), ARRAY)->array[0];
	int stride = 1;
	if (node->tail.size () == 5) stride = (int) type_check  (node->tail.at (4), ARRAY)->array[0];
	if (i < 0 || len < 1 || stride < 1) {
		error ("[assign] invalid indexing", node);
	}
	int j = i; 
	int ct = 0;
	while (j < v1.size ()) {
		if (ct >= len) break;
		j += stride;
		++ct;
	}        
	v1[std::slice(i, ct, stride)] = v2;
	return make_atom (v1);
}  
template <bool WRITE>
AtomPtr fn_print (AtomPtr node, AtomPtr env) {
	std::ostream* out = &std::cout;
	if (WRITE) {
		out = new std::ofstream (type_check (node->tail.at (0), STRING)->lexeme);
		if (!out->good ()) error ("[save] cannot create output file", node);
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
        if (!in.good ()) error ("[read] cannot open input file", node);
        AtomPtr r = make_atom ();
        while (true) {
            AtomPtr l = read(in, linenum);
            if (!l && in.eof()) break;
            if (!l) continue;
            r->tail.push_back(l);
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
AtomPtr load (const std::string&fname, AtomPtr env) {
    std::ifstream in (fname);
    if (!in.good ()) error ("cannot open input file", make_atom(fname));
    AtomPtr r;
    unsigned linenum = 0;
    while (true) {
        try {
            AtomPtr l = read(in, linenum);
            if (!l && in.eof()) break;
            if (!l) continue;
            r = eval(l, env);
        } catch (std::exception& e) {
            std::cerr << "[" << fname << ":" << linenum << "] " << e.what () << std::endl;
        } catch (...) {
            std::cerr << "unknown error detected" << std::endl;
        }
    }
    return r;
}
AtomPtr fn_load (AtomPtr node, AtomPtr env) {
	return load (type_check (node->tail.at (0), STRING)->lexeme, env);
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
AtomPtr add_core (AtomPtr env) {
	add_op ("quote", &fn_quote, -1, env); // -1 are checked in the handling function
	add_op ("def", &fn_def, -1, env);
	add_op ("=", &fn_set, -1, env);
	add_op ("lambda", &fn_lambda, -1, env);
	add_op ("macro", &fn_macro, -1, env);
	add_op ("if", &fn_if, -1, env);
	add_op ("while", &fn_while, -1, env);
	add_op ("begin", &fn_begin, -1, env);
	add_op ("eval", &fn_eval, 1, env);
	add_op ("apply", &fn_apply, 2, env);
	add_op ("info", &fn_info, 1, env);  
	add_op ("list", &fn_list, 0, env);
	add_op ("lappend", &fn_lappend, 1, env);
	add_op ("lreplace", &fn_lreplace, 4, env);
	add_op ("lrange", &fn_lrange, 3, env);
	add_op ("lindex", &fn_lindex, 2, env);
	add_op ("lset", &fn_lset, 3, env);
	add_op ("llength", &fn_llength, 1, env);
	add_op ("lshuffle", &fn_lshuffle, 1, env); 	
    add_op ("array", &fn_array, 0, env);    
	add_op ("array2list", &fn_array2list, 1, env);
	add_op ("==", &fn_eq, 2, env);
    add_op ("+", &fn_add, 2, env);
    add_op ("-", &fn_sub, 2, env);
    add_op ("*", &fn_mul, 2, env);
    add_op ("/", &fn_div, 2, env);
    add_op ("<", &fn_less, 2, env);
    add_op ("<=", &fn_lesseq, 2, env);
    add_op (">", &fn_greater, 2, env);
    add_op (">=", &fn_greatereq, 2, env);    
    add_op ("min", &fn_min, 1, env);    
    add_op ("max", &fn_max, 1, env);    
    add_op ("sum", &fn_sum, 1, env);   
	add_op ("size", &fn_size, 1, env);   
	add_op ("sin", &fn_sin, 1, env);    
	add_op ("cos", &fn_cos, 1, env); 
	add_op ("tan", &fn_tan, 1, env); 
	add_op ("asin", &fn_asin, 1, env);    
	add_op ("acos", &fn_acos, 1, env); 
	add_op ("atan", &fn_atan, 1, env); 
	add_op ("sinh", &fn_sinh, 1, env);    
	add_op ("cosh", &fn_cosh, 1, env); 
	add_op ("tanh", &fn_tanh, 1, env); 
	add_op ("log", &fn_log, 1, env); 
	add_op ("log10", &fn_log10, 1, env); 
	add_op ("sqrt", &fn_sqrt, 1, env); 
	add_op ("exp", &fn_exp, 1, env); 
	add_op ("abs", &fn_abs, 1, env); 
	add_op ("neg", &fn_neg, 1, env);
	add_op ("floor", &fn_floor, 1, env);
	add_op ("slice", fn_slice, 3, env);   
	add_op ("assign", fn_assign, 4, env);     	
	add_op ("print", &fn_print<false>, 1, env);
	add_op ("save", &fn_print<true>, 2, env);
	add_op ("read", &fn_read, 0, env);
    add_op ("str", &fn_string, 2, env);
	add_op ("load", &fn_load, 0, env);
	add_op ("exec", &fn_exec, 1, env);
	add_op ("exit", &fn_exit, 0, env);
	return env;
}
void repl (std::istream& in, std::ostream& out, AtomPtr env) {
	unsigned linenum = 0;
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

#endif // CORE_H

// eof