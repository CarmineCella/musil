// std.h — the general-purpose standard library, C++ half.
// The other half is std.mu, written in Musil; what can be expressed in the
// language lives there. Only what needs C++ (speed on vectors, strings, files,
// higher-order over both list and vector) is here.
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//

#pragma once
#include "core.h"
#include <regex>

namespace musil {

inline size_t checked_size(Interp& i, const vptr& v) { long n = i.index(v); if (n < 0) i.bad("negative size"); return (size_t)n; }

// vectors
// (range n) (range a b) (range a b step) a vector of evenly spaced numbers, end excluded
inline vptr fn_range(vlist& a, Interp& i) {   // (range n) | (range a b) | (range a b step)
    
    double lo = 0, hi, step = 1;
    if (a.size()==1) hi = i.scalar(a[0]);
    else { lo = i.scalar(a[0]); hi = i.scalar(a[1]); if (a.size()==3) step = i.scalar(a[2]); }
    if (step == 0) i.err("range: step must be nonzero");
    long n = (long)std::ceil((hi - lo) / step); if (n < 0) n = 0;
    varr r(n); for (long k=0; k<n; k++) r[k] = lo + k*step;
    return v_arr(std::move(r));
}
// (seed n) seed the random generator so that rand repeats
// (rand) a number in [0, 1); (rand n) a vector of n of them
inline vptr fn_seed(vlist& a, Interp& i) { i.rng.seed((uint64_t)i.scalar(a[0])); return v_nil(); }
inline vptr fn_rand(vlist& a, Interp& i) {
    
    std::uniform_real_distribution<double> d(0.0, 1.0);
    if (a.empty()) return v_num(d(i.rng));
    size_t n=checked_size(i, a[0]); varr r(n); for (size_t k=0; k<n; k++) r[k]=d(i.rng);
    return v_arr(std::move(r));
}
// (sort v) the elements of a vector in ascending order
inline vptr fn_sort(vlist& a, Interp& i) { varr r=i.num(a[0]); std::sort(std::begin(r), std::end(r)); return v_arr(std::move(r)); }

// list, vector, string
// (reverse x) a list, vector or string reversed
inline vptr fn_reverse(vlist& a, Interp& i) {
    if (a[0]->t == Value::LIST) { vlist r(a[0]->l.rbegin(), a[0]->l.rend()); return v_list(std::move(r)); }
    if (a[0]->t == Value::NUM)  { varr r(a[0]->num.size()); for (size_t k=0; k<r.size(); k++) r[k] = a[0]->num[r.size()-1-k]; return v_arr(std::move(r)); }
    if (a[0]->t == Value::STR)  { return v_str(std::string(a[0]->s.rbegin(), a[0]->s.rend())); }
    i.err(std::string("reverse: expected list, number or string, got ") + type_name(a[0])); }
// (slice x start [count]) part of a list, vector or string; negative start counts from the end
inline vptr fn_slice(vlist& a, Interp& i) {   // (slice x start [count]) — start may be negative
    auto& v=a[0];
    size_t size = v->t==Value::LIST ? v->l.size() : v->t==Value::NUM ? v->num.size() : v->t==Value::STR ? v->s.size() : 0;
    if (v->t!=Value::LIST && v->t!=Value::NUM && v->t!=Value::STR) i.err(std::string("slice: expected list, number or string, got ") + type_name(v));
    long s = i.index(a[1]); if (s < 0) s += (long)size;
    if (s < 0) s = 0;
    if ((size_t)s > size) s = (long)size;
    long n = a.size()==3 ? i.index(a[2]) : (long)size - s;
    if (n < 0) n = 0;
    if ((size_t)(s + n) > size) n = (long)size - s;
    if (v->t==Value::LIST) return v_list(vlist(v->l.begin()+s, v->l.begin()+s+n));
    if (v->t==Value::STR)  return v_str(v->s.substr((size_t)s, (size_t)n));
    varr r(n); for (long k=0; k<n; k++) r[k] = v->num[s+k]; return v_arr(std::move(r));
}
// (find x v) index of the first element equal to v, or of the substring v in a string; -1 if absent
inline vptr fn_find(vlist& a, Interp& i) {    // index of first element equal to x, or -1
    auto& v=a[0];
    if (v->t==Value::LIST) { for (size_t k=0; k<v->l.size(); k++) if (equal(v->l[k], a[1])) return v_num((double)k); return v_num(-1); }
    if (v->t==Value::NUM)  { double x = i.scalar(a[1]); for (size_t k=0; k<v->num.size(); k++) if (v->num[k]==x) return v_num((double)k); return v_num(-1); }
    if (v->t==Value::STR)  { size_t p = v->s.find(i.str(a[1])); return v_num(p==std::string::npos ? -1.0 : (double)p); }
    i.err(std::string("find: expected list, number or string, got ") + type_name(v)); }

// strings
// (concat x ...) the arguments printed and joined into one string
inline vptr fn_concat(vlist& a, Interp&) { std::string s; for (auto& x : a) s+=str_of(x); return v_str(std::move(s)); }
// --- records: lists of (key value) pairs; keys compare with equal? --------------------------------------------------
// The accessors are builtins: a record is looked up at every event, note and database entry, and a scan that stops at
// the key (rather than a filter over every pair) is what keeps a score of thousands of notes fast.
inline long rec_find(Interp& i, const vptr& rec, const vptr& key) {
    if (!rec || rec->t != Value::LIST) i.bad("a record is a list of (key value) pairs");
    for (size_t k = 0; k < rec->l.size(); k++) { const vptr& p = rec->l[k]; if (p && p->t == Value::LIST && p->l.size() == 2 && equal(p->l[0], key)) return (long)k; }
    return -1;
}
// (get rec key) the value of key; an error when absent
inline vptr fn_get(vlist& a, Interp& i) { long k = rec_find(i, a[0], a[1]); if (k < 0) i.bad("get: no key " + str_of(a[1])); return a[0]->l[(size_t)k]->l[1]; }
// (opt rec key default) the value of key in a record, or default
inline vptr fn_opt(vlist& a, Interp& i) { long k = rec_find(i, a[0], a[1]); return k < 0 ? a[2] : a[0]->l[(size_t)k]->l[1]; }
// (has? rec key) is the key present?
inline vptr fn_has(vlist& a, Interp& i) { return v_bool(rec_find(i, a[0], a[1]) >= 0); }
// (put! rec key value) set a key in place (replacing an existing pair, or adding one); returns the record
inline vptr fn_put_bang(vlist& a, Interp& i) { long k = rec_find(i, a[0], a[1]); if (k >= 0) a[0]->l[(size_t)k]->l[1] = a[2]; else a[0]->l.push_back(v_list({ a[1], a[2] })); return a[0]; }
// (split s sep) the pieces of s between occurrences of sep; (split s "") the characters
inline vptr fn_split(vlist& a, Interp& i) {
    
    const std::string& s=i.str(a[0]); const std::string& sep=i.str(a[1]);
    vlist out;
    if (sep.empty()) { for (char c : s) out.push_back(v_str(std::string(1, c))); return v_list(std::move(out)); }
    size_t st=0, p;
    while ((p=s.find(sep, st))!=std::string::npos) { out.push_back(v_str(s.substr(st, p-st))); st=p+sep.size(); }
    out.push_back(v_str(s.substr(st)));
    return v_list(std::move(out));
}
// (join L sep) the elements of L printed and joined with sep
inline vptr fn_join(vlist& a, Interp& i) {
    
    vlist& l=i.list(a[0]); const std::string& sep=i.str(a[1]);
    std::string out; for (size_t k=0; k<l.size(); k++) { if (k) out+=sep; out+=str_of(l[k]); }
    return v_str(std::move(out));
}
// (format "text {} more {}" x y) each {} replaced by the next argument
inline vptr fn_format(vlist& a, Interp& i) {   // (format "x = {} y = {}" x y)
    
    const std::string& f=i.str(a[0]); std::string out; size_t k=1;
    for (size_t p=0; p<f.size(); p++) {
        if (f[p]=='{' && p+1<f.size() && f[p+1]=='}') { if (k>=a.size()) i.err("format: not enough arguments"); out+=str_of(a[k++]); p++; }
        else out+=f[p];
    }
    if (k<a.size()) i.err("format: too many arguments");
    return v_str(std::move(out));
}
// (upper s) (lower s) (trim s) case changes and whitespace trimming
inline vptr fn_upper(vlist& a, Interp& i) { std::string s=i.str(a[0]); for (auto& c : s) c=(char)std::toupper((unsigned char)c); return v_str(std::move(s)); }
inline vptr fn_lower(vlist& a, Interp& i) { std::string s=i.str(a[0]); for (auto& c : s) c=(char)std::tolower((unsigned char)c); return v_str(std::move(s)); }
inline vptr fn_trim(vlist& a, Interp& i) { const std::string& s=i.str(a[0]); size_t st=0, en=s.size(); while (st<en && std::isspace((unsigned char)s[st])) st++; while (en>st && std::isspace((unsigned char)s[en-1])) en--; return v_str(s.substr(st, en-st)); }
// (chr n) the character with code n
// (ord s) the code of the first character of s
inline vptr fn_chr(vlist& a, Interp& i) { return v_str(std::string(1, (char)i.index(a[0]))); }
inline vptr fn_ord(vlist& a, Interp& i) { const std::string& s=i.str(a[0]); if (s.empty()) i.err("ord: empty string"); return v_num((double)(unsigned char)s[0]); }

// I/O
// (read path) the whole file as a string; a relative path that does not exist in the current
//   directory is looked for next to the file being run (so a script finds its data from anywhere)
inline vptr fn_read(vlist& a, Interp& i) {
    std::string fn=i.read_path(i.str(a[0]));
    std::ifstream f(fn); if (!f) i.err("read: cannot open " + fn);
    std::stringstream ss; ss<<f.rdbuf(); return v_str(ss.str());
}
// (write path x) write x (printed) to a file, replacing it
// (append-file path x) write x (printed) at the end of a file
inline vptr fn_write(vlist& a, Interp& i) {
    const std::string& fn=i.str(a[0]);
    std::ofstream f(fn); if (!f) i.err("write: cannot open " + fn);
    f << str_of(a[1]); return v_nil();
}
inline vptr fn_appendf(vlist& a, Interp& i) {
    const std::string& fn=i.str(a[0]);
    std::ofstream f(fn, std::ios::app); if (!f) i.err("append-file: cannot open " + fn);
    f << str_of(a[1]); return v_nil();
}
// (directory? path) is the path an existing directory?
inline vptr fn_directoryp(vlist& a, Interp& i) { std::error_code ec; return v_bool(std::filesystem::is_directory(i.read_path(i.str(a[0])), ec)); }
// (exists? path) does the file or directory exist?
inline vptr fn_existsp(vlist& a, Interp& i) { return v_bool(fs::exists(i.read_path(i.str(a[0])))); }
// (input [prompt]) a line read from the console, or nil at end of input
inline vptr fn_input(vlist& a, Interp& i) { if (!a.empty()) { *i.out << i.str(a[0]) << std::flush; } std::string s; if (!std::getline(std::cin, s)) return v_nil(); return v_str(std::move(s)); }

// higher-order
// (map x f) f applied to every element: a list for a list; for a vector, a vector when every result is a number, a list otherwise
inline vptr fn_map(vlist& a, Interp& i) {
    i.fn(a[1]);
    if (a[0]->t==Value::LIST) {
        vlist out; out.reserve(a[0]->l.size());
        for (auto& x : a[0]->l) { vlist arg={x}; out.push_back(i.call_fn(a[1], arg)); }
        return v_list(std::move(out));
    }
    if (a[0]->t==Value::NUM) {                       // a vector in: a vector out when every result is a number, else a list
        vlist out; out.reserve(a[0]->num.size()); bool numeric = true;
        for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={v_num(a[0]->num[k])}; vptr r = i.call_fn(a[1], arg); if (r->t != Value::NUM || r->num.size() != 1) numeric = false; out.push_back(r); }
        if (!numeric) return v_list(std::move(out));
        varr r(out.size()); for (size_t k=0; k<out.size(); k++) r[k]=out[k]->num[0]; return v_arr(std::move(r));
    }
    i.err(std::string("map: expected list or number, got ") + type_name(a[0]));
}
// (filter x f) the elements for which f is true, as a list or vector like x
inline vptr fn_filter(vlist& a, Interp& i) {
    i.fn(a[1]);
    if (a[0]->t==Value::LIST) {
        vlist out; for (auto& x : a[0]->l) { vlist arg={x}; if (truthy(i.call_fn(a[1], arg))) out.push_back(x); }
        return v_list(std::move(out));
    }
    if (a[0]->t==Value::NUM) {
        std::vector<double> out;
        for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={v_num(a[0]->num[k])}; if (truthy(i.call_fn(a[1], arg))) out.push_back(a[0]->num[k]); }
        varr r(out.size()); for (size_t k=0; k<out.size(); k++) r[k]=out[k];
        return v_arr(std::move(r));
    }
    i.err(std::string("filter: expected list or number, got ") + type_name(a[0]));
}
// (reduce x f init) fold: (f (f (f init x0) x1) x2) ...
inline vptr fn_reduce(vlist& a, Interp& i) {
    i.fn(a[1]);
    vptr acc=a[2];
    if (a[0]->t==Value::LIST) { for (auto& x : a[0]->l) { vlist arg={acc, x}; acc=i.call_fn(a[1], arg); } return acc; }
    if (a[0]->t==Value::NUM)  { for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={acc, v_num(a[0]->num[k])}; acc=i.call_fn(a[1], arg); } return acc; }
    i.err(std::string("reduce: expected list or number, got ") + type_name(a[0]));
}

// helpers
// (assert cond [message ...]) stop with an error unless cond is true
inline vptr fn_assert(vlist& a, Interp& i) {   // (assert cond [message...])
 
    if (truthy(a[0])) return v_nil();
    std::string m = "assertion failed"; for (size_t k=1; k<a.size(); k++) m += (k==1 ? ": " : " ") + str_of(a[k]);
    i.err(m);
}
// (regex-match? pattern s) does the regular expression (ECMAScript syntax) match anywhere in s?
inline vptr fn_regex_match(vlist& a, Interp& i) {
    try { std::regex re(i.str(a[0])); return v_bool(std::regex_search(i.str(a[1]), re)); }
    catch (std::regex_error& e) { i.bad("regex-match?: bad pattern " + i.str(a[0])); }
}
inline void add_std(Interp& i) {
    const int N = -1;   // unbounded
    // vectors
    i.def("range", fn_range, 1, 3); i.def("seed", fn_seed, 1, 1); i.def("rand", fn_rand, 0, 1); i.def("sort", fn_sort, 1, 1);
    // sequences: list, vector, string
    i.def("reverse", fn_reverse, 1, 1); i.def("slice", fn_slice, 2, 3); i.def("find", fn_find, 2, 2);
    // strings
    i.def("regex-match?", fn_regex_match, 2, 2); i.def("concat", fn_concat, 0, N); i.def("split", fn_split, 2, 2); i.def("get", fn_get, 2, 2); i.def("opt", fn_opt, 3, 3); i.def("has?", fn_has, 2, 2); i.def("put!", fn_put_bang, 3, 3); i.def("join", fn_join, 2, 2);
    i.def("format", fn_format, 1, N); i.def("upper", fn_upper, 1, 1); i.def("lower", fn_lower, 1, 1);
    i.def("trim", fn_trim, 1, 1); i.def("chr", fn_chr, 1, 1); i.def("ord", fn_ord, 1, 1);
    // files and console
    i.def("read", fn_read, 1, 1); i.def("write", fn_write, 2, 2); i.def("append-file", fn_appendf, 2, 2);
    i.def("exists?", fn_existsp, 1, 1); i.def("directory?", fn_directoryp, 1, 1); i.def("input", fn_input, 0, 1);
    // higher-order, polymorphic on list and vector
    i.def("map", fn_map, 2, 2); i.def("filter", fn_filter, 2, 2); i.def("reduce", fn_reduce, 3, 3);
    // testing
    i.def("assert", fn_assert, 1, N);
}

} // namespace musil

// eof