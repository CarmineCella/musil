// scientific.h — linear algebra, statistics and machine learning, C++ half.
// The Musil half is scientific.mu. Ported from Musil 1.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// A matrix is a list of row vectors: (list (vec 1 2) (vec 3 4)) is 2x2.
// (getidx M i) is row i, (getidx (getidx M i) j) the element at i,j.
// Only what needs an element loop in C++ is here: products, transpose,
// elimination (det, rank, inv, solve), the symmetric eigensolver, the running
// median, k-means and KNN. Everything that composes vector operations
// (statistics, covariance, PCA, regression, the
// helper layers) is written in Musil in scientific.mu at the same speed.

#pragma once
#include "core.h"
#include "scientific/algorithms.h"
#include "scientific/KNN.h"

namespace musil {

// --- dense row-major matrix used inside this file ---
struct mat {
    size_t r = 0, c = 0; std::vector<double> d;
    mat() {}
    mat(size_t rows, size_t cols, double v = 0) : r(rows), c(cols), d(rows * cols, v) {}
    double& operator()(size_t i, size_t j) { return d[i * c + j]; }
    double  operator()(size_t i, size_t j) const { return d[i * c + j]; }
};
// (list of equal-length vectors) -> mat, with the checks every function needs
inline mat to_mat(Interp& i, const vptr& v) {
    vlist& rows = i.list(v);
    if (rows.empty()) i.bad("empty matrix");
    size_t c = i.num(rows[0]).size();
    if (c == 0) i.bad("zero-length rows");
    mat m(rows.size(), c);
    for (size_t k = 0; k < rows.size(); k++) {
        const varr& row = i.num(rows[k]);
        if (row.size() != c) i.bad("ragged matrix: row " + std::to_string(k) + " has " + std::to_string(row.size()) + " columns, expected " + std::to_string(c));
        for (size_t j = 0; j < c; j++) m(k, j) = row[j];
    }
    return m;
}
inline vptr from_mat(const mat& m) {
    vlist rows; rows.reserve(m.r);
    for (size_t k = 0; k < m.r; k++) { varr row(m.c); for (size_t j = 0; j < m.c; j++) row[j] = m(k, j); rows.push_back(v_arr(std::move(row))); }
    return v_list(std::move(rows));
}
inline vptr from_vec(const std::vector<double>& v) { varr a(v.size()); for (size_t k = 0; k < v.size(); k++) a[k] = v[k]; return v_arr(std::move(a)); }
inline void square(Interp& i, const mat& m) { if (m.r != m.c) i.bad("matrix must be square (" + std::to_string(m.r) + "x" + std::to_string(m.c) + ")"); }

// --- products ---
// (mat-mul A B ...) matrix product, left to right
inline vptr sci_mat_mul(vlist& a, Interp& i) {
    mat acc = to_mat(i, a[0]);
    for (size_t k = 1; k < a.size(); k++) {
        mat b = to_mat(i, a[k]);
        if (acc.c != b.r) i.bad("nonconformant: " + std::to_string(acc.r) + "x" + std::to_string(acc.c) + " times " + std::to_string(b.r) + "x" + std::to_string(b.c));
        mat out(acc.r, b.c);
        for (size_t x = 0; x < acc.r; x++) for (size_t y = 0; y < acc.c; y++) { double v = acc(x, y); if (v == 0) continue; for (size_t z = 0; z < b.c; z++) out(x, z) += v * b(y, z); }
        acc = std::move(out);
    }
    return from_mat(acc);
}
// (transpose M)
inline vptr sci_transpose(vlist& a, Interp& i) {
    mat A = to_mat(i, a[0]); mat T(A.c, A.r);
    for (size_t r = 0; r < A.r; r++) for (size_t c = 0; c < A.c; c++) T(c, r) = A(r, c);
    return from_mat(T);
}

// --- decompositions by Gaussian elimination with partial pivoting ---
// Reduce M in place to row echelon form; returns (rank, determinant-if-square, singular?)
struct elim_result { size_t rank; double det; bool singular; };
inline elim_result eliminate(mat& m, double eps = 1e-12) {
    size_t r = 0; double det = 1; bool sq = m.r == m.c;
    for (size_t c = 0; c < m.c && r < m.r; c++) {
        size_t piv = r; double best = std::fabs(m(r, c));
        for (size_t k = r + 1; k < m.r; k++) if (std::fabs(m(k, c)) > best) { best = std::fabs(m(k, c)); piv = k; }
        if (best < eps) { det = 0; continue; }
        if (piv != r) { for (size_t j = 0; j < m.c; j++) std::swap(m(r, j), m(piv, j)); det = -det; }
        det *= m(r, c);
        for (size_t k = r + 1; k < m.r; k++) { double f = m(k, c) / m(r, c); if (f == 0) continue; for (size_t j = c; j < m.c; j++) m(k, j) -= f * m(r, j); }
        r++;
    }
    return { r, sq ? det : 0.0, !sq || r < m.r };
}
// (det M)
inline vptr sci_det(vlist& a, Interp& i) { mat m = to_mat(i, a[0]); square(i, m); return v_num(eliminate(m).det); }
// (rank M)
inline vptr sci_rank(vlist& a, Interp& i) { mat m = to_mat(i, a[0]); return v_num((double)eliminate(m, 1e-10).rank); }
// (inv M) by Gauss-Jordan on [M | I]
inline vptr sci_inv(vlist& a, Interp& i) {
    mat m = to_mat(i, a[0]); square(i, m); size_t n = m.r;
    mat aug(n, 2 * n);
    for (size_t r = 0; r < n; r++) { for (size_t c = 0; c < n; c++) aug(r, c) = m(r, c); aug(r, n + r) = 1; }
    for (size_t c = 0; c < n; c++) {
        size_t piv = c; double best = std::fabs(aug(c, c));
        for (size_t k = c + 1; k < n; k++) if (std::fabs(aug(k, c)) > best) { best = std::fabs(aug(k, c)); piv = k; }
        if (best < 1e-12) i.bad("matrix is singular");
        if (piv != c) for (size_t j = 0; j < 2 * n; j++) std::swap(aug(c, j), aug(piv, j));
        double p = aug(c, c); for (size_t j = 0; j < 2 * n; j++) aug(c, j) /= p;
        for (size_t k = 0; k < n; k++) { if (k == c) continue; double f = aug(k, c); if (f == 0) continue; for (size_t j = 0; j < 2 * n; j++) aug(k, j) -= f * aug(c, j); }
    }
    mat out(n, n); for (size_t r = 0; r < n; r++) for (size_t c = 0; c < n; c++) out(r, c) = aug(r, n + c);
    return from_mat(out);
}
// (solve A b) => x with A x = b
inline vptr sci_solve(vlist& a, Interp& i) {
    mat A = to_mat(i, a[0]); square(i, A); const varr& b = i.num(a[1]); size_t n = A.r;
    if (b.size() != n) i.bad("b must have " + std::to_string(n) + " elements, got " + std::to_string(b.size()));
    mat aug(n, n + 1);
    for (size_t r = 0; r < n; r++) { for (size_t c = 0; c < n; c++) aug(r, c) = A(r, c); aug(r, n) = b[r]; }
    elim_result e = eliminate(aug);
    if (e.rank < n) i.bad("matrix is singular");
    varr x(0.0, n);
    for (size_t k = n; k-- > 0;) { double s = aug(k, n); for (size_t j = k + 1; j < n; j++) s -= aug(k, j) * x[j]; x[k] = s / aug(k, k); }
    return v_arr(std::move(x));
}

// --- statistics ---
// (median-filter v order) the running median of width order
inline vptr sci_median_filter(vlist& a, Interp& i) {
    const varr& v = i.num(a[0]); long order = i.index(a[1]);
    if (order < 1) i.bad("order must be >= 1");
    long n = (long)v.size(), half = order / 2; varr out(n);
    for (long k = 0; k < n; k++) {
        long lo = std::max(0L, k - half), hi = std::min(n - 1, k + half);
        std::vector<double> w(std::begin(v) + lo, std::begin(v) + hi + 1);
        out[k] = median<double>(w.data(), (int)w.size());
    }
    return v_arr(std::move(out));
}
// (eig-sym M) => (list eigenvalues eigenvectors) of a symmetric matrix by Jacobi rotations;
// eigenvalues descending, eigenvectors as rows in the same order, each with its largest component positive.
inline vptr sci_eig_sym(vlist& a, Interp& i) {
    mat A = to_mat(i, a[0]); square(i, A); size_t n = A.r;
    for (size_t p = 0; p < n; p++) for (size_t q = p + 1; q < n; q++)
        if (std::fabs(A(p, q) - A(q, p)) > 1e-9 * (1 + std::fabs(A(p, q)))) i.bad("matrix is not symmetric");
    mat V(n, n); for (size_t k = 0; k < n; k++) V(k, k) = 1;
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0; for (size_t p = 0; p < n; p++) for (size_t q = p + 1; q < n; q++) off += A(p, q) * A(p, q);
        if (off < 1e-22) break;
        for (size_t p = 0; p < n; p++) for (size_t q = p + 1; q < n; q++) {
            if (std::fabs(A(p, q)) < 1e-300) continue;
            double theta = (A(q, q) - A(p, p)) / (2 * A(p, q));
            double t = (theta >= 0 ? 1 : -1) / (std::fabs(theta) + std::sqrt(1 + theta * theta));
            double c = 1 / std::sqrt(1 + t * t), sn = t * c;
            for (size_t k = 0; k < n; k++) { double akp = A(k, p), akq = A(k, q); A(k, p) = c * akp - sn * akq; A(k, q) = sn * akp + c * akq; }
            for (size_t k = 0; k < n; k++) { double apk = A(p, k), aqk = A(q, k); A(p, k) = c * apk - sn * aqk; A(q, k) = sn * apk + c * aqk; }
            for (size_t k = 0; k < n; k++) { double vkp = V(k, p), vkq = V(k, q); V(k, p) = c * vkp - sn * vkq; V(k, q) = sn * vkp + c * vkq; }
        }
    }
    std::vector<size_t> idx(n); for (size_t k = 0; k < n; k++) idx[k] = k;
    std::sort(idx.begin(), idx.end(), [&](size_t x, size_t y) { return A(x, x) > A(y, y); });
    varr vals(n); mat vecs(n, n);
    for (size_t r = 0; r < n; r++) {
        size_t k = idx[r]; vals[r] = A(k, k);
        size_t big = 0; for (size_t c = 0; c < n; c++) if (std::fabs(V(c, k)) > std::fabs(V(big, k))) big = c;
        double sgn = V(big, k) < 0 ? -1 : 1;
        for (size_t c = 0; c < n; c++) vecs(r, c) = sgn * V(c, k);
    }
    return v_list({ v_arr(std::move(vals)), from_mat(vecs) });
}

// --- machine learning ---
// (kmeans M k) => (list labels centroids): labels is a vector of cluster indices, centroids a k x d matrix
inline vptr sci_kmeans(vlist& a, Interp& i) {
    mat X = to_mat(i, a[0]); long k = i.index(a[1]);
    if (k < 1 || (size_t)k > X.r) i.bad("k must be between 1 and the number of rows");
    std::vector<int> labels(X.r); std::vector<double> cent(k * X.c);
    try { kmeans<double>(X.d.data(), (int)X.r, (int)X.c, (int)k, 1e-5, labels.data(), cent.data()); } catch (std::exception& e) { i.bad(e.what()); }
    mat C(k, X.c); C.d = cent;
    varr lv(X.r); for (size_t r = 0; r < X.r; r++) lv[r] = labels[r];
    return v_list({ v_arr(std::move(lv)), from_mat(C) });
}
// (knn training k queries) => list of predicted labels.
// training: list of (list features label); queries: list of feature vectors.
inline vptr sci_knn(vlist& a, Interp& i) {
    vlist& train = i.list(a[0]); long k = i.index(a[1]); vlist& queries = i.list(a[2]);
    if (train.empty()) i.bad("empty training set");
    if (k < 1) i.bad("k must be >= 1");
    vlist& first = i.list(train[0]);
    if (first.size() != 2) i.bad("each training sample is (list features label)");
    int features = (int)i.num(first[0]).size();
    KNN<double> model((int)k, features);
    try {
        for (auto& s : train) {
            vlist& pair = i.list(s);
            if (pair.size() != 2) i.bad("each training sample is (list features label)");
            auto* o = new Observation<double>();
            o->attributes = i.num(pair[0]); o->classlabel = str_of(pair[1]);
            model.addObservation(o);
        }
    } catch (std::exception& e) { i.bad(e.what()); }
    vlist out;
    for (auto& q : queries) {
        Observation<double> qo; qo.attributes = i.num(q);
        if ((int)qo.attributes.size() != features) i.bad("query has " + std::to_string(qo.attributes.size()) + " features, expected " + std::to_string(features));
        try { out.push_back(v_str(model.classify(qo))); } catch (std::exception& e) { i.bad(e.what()); }
    }
    return v_list(std::move(out));
}

inline void add_scientific(Interp& i) {
    const int N = -1;
    i.def("mat-mul", sci_mat_mul, 2, N); i.def("transpose", sci_transpose, 1, 1);
    i.def("det", sci_det, 1, 1); i.def("rank", sci_rank, 1, 1); i.def("inv", sci_inv, 1, 1); i.def("solve", sci_solve, 2, 2);
    i.def("eig-sym", sci_eig_sym, 1, 1); i.def("median-filter", sci_median_filter, 2, 2);
    i.def("kmeans", sci_kmeans, 2, 2); i.def("knn", sci_knn, 3, 3);
}

} // namespace musil
