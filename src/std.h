// std.h — the general-purpose standard library: vector constructors, sequence
// utilities, strings, files, higher-order functions, assert.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Written entirely against the public core API (interp::def, the argument
// helpers, bcast, checked_index, call_fn). core.h is the language; this file
// is what makes it comfortable to use. A host that embeds only core.h gets a
// complete language without any of this.

#pragma once
#include "core.h"

namespace musil {

inline size_t checked_size(interp& i, const vptr& v) { long n = i.index(v); if (n < 0) i.bad("negative size"); return (size_t)n; }

// --- Vectors ---
inline vptr fn_range(vlist& a, interp& i) {   // (range n) | (range a b) | (range a b step)
    
    double lo = 0, hi, step = 1;
    if (a.size()==1) hi = i.scalar(a[0]);
    else { lo = i.scalar(a[0]); hi = i.scalar(a[1]); if (a.size()==3) step = i.scalar(a[2]); }
    if (step == 0) i.err("range: step must be nonzero");
    long n = (long)std::ceil((hi - lo) / step); if (n < 0) n = 0;
    varr r(n); for (long k=0; k<n; k++) r[k] = lo + k*step;
    return v_arr(std::move(r));
}
inline vptr fn_linspace(vlist& a, interp& i) {
    
    double a0=i.scalar(a[0]), a1=i.scalar(a[1]); long n=i.index(a[2]);
    if (n<2) i.err("linspace: n must be >= 2");
    varr r(n); for (long k=0; k<n; k++) r[k]=a0+(a1-a0)*k/(n-1);
    return v_arr(std::move(r));
}
inline vptr fn_zeros(vlist& a, interp& i) { return v_arr(varr(0.0, checked_size(i, a[0]))); }
inline vptr fn_ones(vlist& a, interp& i) {  return v_arr(varr(1.0, checked_size(i, a[0]))); }
inline vptr fn_seed(vlist& a, interp& i) { i.rng.seed((uint64_t)i.scalar(a[0])); return v_nil(); }
inline vptr fn_rand(vlist& a, interp& i) {
    
    std::uniform_real_distribution<double> d(0.0, 1.0);
    if (a.empty()) return v_num(d(i.rng));
    size_t n=checked_size(i, a[0]); varr r(n); for (size_t k=0; k<n; k++) r[k]=d(i.rng);
    return v_arr(std::move(r));
}
inline vptr fn_sort(vlist& a, interp& i) { varr r=i.num(a[0]); std::sort(std::begin(r), std::end(r)); return v_arr(std::move(r)); }
inline vptr fn_prod(vlist& a, interp& i) { const varr& v=i.num(a[0]); double p=1; for (size_t k=0; k<v.size(); k++) p*=v[k]; return v_num(p); }
inline vptr fn_mean(vlist& a, interp& i) { const varr& v=i.num(a[0]); if (v.size()==0) i.err("mean: empty vector"); double s=0; for (size_t k=0; k<v.size(); k++) s+=v[k]; return v_num(s/v.size()); }
inline vptr fn_dot(vlist& a, interp& i) { const varr& x=i.num(a[0]); const varr& y=i.num(a[1]);
    if (x.size()!=y.size()) i.err("dot: size mismatch (" + std::to_string(x.size()) + " vs " + std::to_string(y.size()) + ")");
    double s=0; for (size_t k=0; k<x.size(); k++) s+=x[k]*y[k]; return v_num(s); }

// --- Sequences: list, vector, string ---
inline vptr fn_last(vlist& a, interp& i) { auto& v=a[0];
    if (v->t==value::LIST) { if (v->l.empty()) i.err("last: empty list"); return v->l.back(); }
    if (v->t==value::NUM)  { if (v->num.size()==0) i.err("last: empty vector"); return v_num(v->num[v->num.size()-1]); }
    i.err(std::string("last: expected list or number, got ") + type_name(v)); }
inline vptr fn_reverse(vlist& a, interp& i) {
    if (a[0]->t == value::LIST) { vlist r(a[0]->l.rbegin(), a[0]->l.rend()); return v_list(std::move(r)); }
    if (a[0]->t == value::NUM)  { varr r(a[0]->num.size()); for (size_t k=0; k<r.size(); k++) r[k] = a[0]->num[r.size()-1-k]; return v_arr(std::move(r)); }
    if (a[0]->t == value::STR)  { return v_str(std::string(a[0]->s.rbegin(), a[0]->s.rend())); }
    i.err(std::string("reverse: expected list, number or string, got ") + type_name(a[0])); }
inline vptr fn_slice(vlist& a, interp& i) {   // (slice x start [count]) — start may be negative
    auto& v=a[0];
    size_t size = v->t==value::LIST ? v->l.size() : v->t==value::NUM ? v->num.size() : v->t==value::STR ? v->s.size() : 0;
    if (v->t!=value::LIST && v->t!=value::NUM && v->t!=value::STR) i.err(std::string("slice: expected list, number or string, got ") + type_name(v));
    long s = i.index(a[1]); if (s < 0) s += (long)size;
    if (s < 0) s = 0;
    if ((size_t)s > size) s = (long)size;
    long n = a.size()==3 ? i.index(a[2]) : (long)size - s;
    if (n < 0) n = 0;
    if ((size_t)(s + n) > size) n = (long)size - s;
    if (v->t==value::LIST) return v_list(vlist(v->l.begin()+s, v->l.begin()+s+n));
    if (v->t==value::STR)  return v_str(v->s.substr((size_t)s, (size_t)n));
    varr r(n); for (long k=0; k<n; k++) r[k] = v->num[s+k]; return v_arr(std::move(r));
}
inline vptr fn_find(vlist& a, interp& i) {    // index of first element equal to x, or -1
    auto& v=a[0];
    if (v->t==value::LIST) { for (size_t k=0; k<v->l.size(); k++) if (equal(v->l[k], a[1])) return v_num((double)k); return v_num(-1); }
    if (v->t==value::NUM)  { double x = i.scalar(a[1]); for (size_t k=0; k<v->num.size(); k++) if (v->num[k]==x) return v_num((double)k); return v_num(-1); }
    if (v->t==value::STR)  { size_t p = v->s.find(i.str(a[1])); return v_num(p==std::string::npos ? -1.0 : (double)p); }
    i.err(std::string("find: expected list, number or string, got ") + type_name(v)); }
inline vptr fn_concat_list(vlist& a, interp& i) { vlist r; for (auto& x : a) for (auto& y : i.list(x)) r.push_back(y); return v_list(std::move(r)); }

// --- Strings ---
// Strings
inline vptr fn_concat(vlist& a, interp&) { std::string s; for (auto& x : a) s+=str_of(x); return v_str(std::move(s)); }
inline vptr fn_split(vlist& a, interp& i) {
    
    const std::string& s=i.str(a[0]); const std::string& sep=i.str(a[1]);
    vlist out;
    if (sep.empty()) { for (char c : s) out.push_back(v_str(std::string(1, c))); return v_list(std::move(out)); }
    size_t st=0, p;
    while ((p=s.find(sep, st))!=std::string::npos) { out.push_back(v_str(s.substr(st, p-st))); st=p+sep.size(); }
    out.push_back(v_str(s.substr(st)));
    return v_list(std::move(out));
}
inline vptr fn_join(vlist& a, interp& i) {
    
    vlist& l=i.list(a[0]); const std::string& sep=i.str(a[1]);
    std::string out; for (size_t k=0; k<l.size(); k++) { if (k) out+=sep; out+=str_of(l[k]); }
    return v_str(std::move(out));
}
inline vptr fn_format(vlist& a, interp& i) {   // (format "x = {} y = {}" x y)
    
    const std::string& f=i.str(a[0]); std::string out; size_t k=1;
    for (size_t p=0; p<f.size(); p++) {
        if (f[p]=='{' && p+1<f.size() && f[p+1]=='}') { if (k>=a.size()) i.err("format: not enough arguments"); out+=str_of(a[k++]); p++; }
        else out+=f[p];
    }
    if (k<a.size()) i.err("format: too many arguments");
    return v_str(std::move(out));
}
inline vptr fn_upper(vlist& a, interp& i) { std::string s=i.str(a[0]); for (auto& c : s) c=(char)std::toupper((unsigned char)c); return v_str(std::move(s)); }
inline vptr fn_lower(vlist& a, interp& i) { std::string s=i.str(a[0]); for (auto& c : s) c=(char)std::tolower((unsigned char)c); return v_str(std::move(s)); }
inline vptr fn_trim(vlist& a, interp& i) { const std::string& s=i.str(a[0]); size_t st=0, en=s.size(); while (st<en && std::isspace((unsigned char)s[st])) st++; while (en>st && std::isspace((unsigned char)s[en-1])) en--; return v_str(s.substr(st, en-st)); }
inline vptr fn_chr(vlist& a, interp& i) { return v_str(std::string(1, (char)i.index(a[0]))); }
inline vptr fn_ord(vlist& a, interp& i) { const std::string& s=i.str(a[0]); if (s.empty()) i.err("ord: empty string"); return v_num((double)(unsigned char)s[0]); }

// --- Files and console ---
inline vptr fn_read(vlist& a, interp& i) {
    const std::string& fn=i.str(a[0]);
    std::ifstream f(fn); if (!f) i.err("read: cannot open " + fn);
    std::stringstream ss; ss<<f.rdbuf(); return v_str(ss.str());
}
inline vptr fn_write(vlist& a, interp& i) {
    const std::string& fn=i.str(a[0]);
    std::ofstream f(fn); if (!f) i.err("write: cannot open " + fn);
    f << str_of(a[1]); return v_nil();
}
inline vptr fn_appendf(vlist& a, interp& i) {
    const std::string& fn=i.str(a[0]);
    std::ofstream f(fn, std::ios::app); if (!f) i.err("append-file: cannot open " + fn);
    f << str_of(a[1]); return v_nil();
}
inline vptr fn_existsp(vlist& a, interp& i) { return v_bool(fs::exists(i.str(a[0]))); }
inline vptr fn_input(vlist& a, interp& i) { if (!a.empty()) { *i.out << i.str(a[0]) << std::flush; } std::string s; if (!std::getline(std::cin, s)) return v_nil(); return v_str(std::move(s)); }

// --- Higher-order, polymorphic on list and vector ---
// Higher-order — polymorphic on LIST and NUM
inline vptr fn_map(vlist& a, interp& i) {
    i.fn(a[1]);
    if (a[0]->t==value::LIST) {
        vlist out; out.reserve(a[0]->l.size());
        for (auto& x : a[0]->l) { vlist arg={x}; out.push_back(i.call_fn(a[1], arg)); }
        return v_list(std::move(out));
    }
    if (a[0]->t==value::NUM) {
        varr r(a[0]->num.size());
        for (size_t k=0; k<r.size(); k++) { vlist arg={v_num(a[0]->num[k])}; r[k]=i.scalar(i.call_fn(a[1], arg)); }
        return v_arr(std::move(r));
    }
    i.err(std::string("map: expected list or number, got ") + type_name(a[0]));
}
inline vptr fn_filter(vlist& a, interp& i) {
    i.fn(a[1]);
    if (a[0]->t==value::LIST) {
        vlist out; for (auto& x : a[0]->l) { vlist arg={x}; if (truthy(i.call_fn(a[1], arg))) out.push_back(x); }
        return v_list(std::move(out));
    }
    if (a[0]->t==value::NUM) {
        std::vector<double> out;
        for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={v_num(a[0]->num[k])}; if (truthy(i.call_fn(a[1], arg))) out.push_back(a[0]->num[k]); }
        varr r(out.size()); for (size_t k=0; k<out.size(); k++) r[k]=out[k];
        return v_arr(std::move(r));
    }
    i.err(std::string("filter: expected list or number, got ") + type_name(a[0]));
}
inline vptr fn_reduce(vlist& a, interp& i) {
    i.fn(a[1]);
    vptr acc=a[2];
    if (a[0]->t==value::LIST) { for (auto& x : a[0]->l) { vlist arg={acc, x}; acc=i.call_fn(a[1], arg); } return acc; }
    if (a[0]->t==value::NUM)  { for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={acc, v_num(a[0]->num[k])}; acc=i.call_fn(a[1], arg); } return acc; }
    i.err(std::string("reduce: expected list or number, got ") + type_name(a[0]));
}
inline vptr fn_each(vlist& a, interp& i) {    // (each x fn) — call for side effects, return nil
    i.fn(a[1]);
    if (a[0]->t==value::LIST) { for (auto& x : a[0]->l) { vlist arg={x}; i.call_fn(a[1], arg); } return v_nil(); }
    if (a[0]->t==value::NUM)  { for (size_t k=0; k<a[0]->num.size(); k++) { vlist arg={v_num(a[0]->num[k])}; i.call_fn(a[1], arg); } return v_nil(); }
    i.err(std::string("each: expected list or number, got ") + type_name(a[0]));
}

// --- Testing ---
inline vptr fn_assert(vlist& a, interp& i) {   // (assert cond [message...])
    
    if (truthy(a[0])) return v_nil();
    std::string m = "assertion failed"; for (size_t k=1; k<a.size(); k++) m += (k==1 ? ": " : " ") + str_of(a[k]);
    i.err(m);
}

inline void add_std(interp& i) {
    const int N = -1;   // unbounded
    // Vectors
    i.def("range", fn_range, 1, 3); i.def("linspace", fn_linspace, 3, 3); i.def("zeros", fn_zeros, 1, 1);
    i.def("ones", fn_ones, 1, 1); i.def("seed", fn_seed, 1, 1); i.def("rand", fn_rand, 0, 1);
    i.def("sort", fn_sort, 1, 1); i.def("prod", fn_prod, 1, 1); i.def("mean", fn_mean, 1, 1);
    i.def("dot", fn_dot, 2, 2);
    // Sequences: list, vector, string
    i.def("last", fn_last, 1, 1); i.def("reverse", fn_reverse, 1, 1); i.def("slice", fn_slice, 2, 3);
    i.def("find", fn_find, 2, 2); i.def("concat-list", fn_concat_list, 0, N);
    // Strings
    i.def("concat", fn_concat, 0, N); i.def("split", fn_split, 2, 2); i.def("join", fn_join, 2, 2);
    i.def("format", fn_format, 1, N); i.def("upper", fn_upper, 1, 1); i.def("lower", fn_lower, 1, 1);
    i.def("trim", fn_trim, 1, 1); i.def("chr", fn_chr, 1, 1); i.def("ord", fn_ord, 1, 1);
    // Files and console
    i.def("read", fn_read, 1, 1); i.def("write", fn_write, 2, 2); i.def("append-file", fn_appendf, 2, 2);
    i.def("exists?", fn_existsp, 1, 1); i.def("input", fn_input, 0, 1);
    // Higher-order, polymorphic on list and vector
    i.def("map", fn_map, 2, 2); i.def("filter", fn_filter, 2, 2); i.def("reduce", fn_reduce, 3, 3);
    i.def("each", fn_each, 2, 2);
    // Testing
    i.def("assert", fn_assert, 1, N);
}

} // namespace musil
