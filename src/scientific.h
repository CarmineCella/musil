// scientific.h
//
// Scientific / linear-algebra / statistics library for Musil v0.5
//
// Matrix representation at the language level:
//   matrix = Array of Vectors  (Moon ArrayPtr whose elements are NumVal rows)
//
//   M[i]    -> row i as a NumVal (vector)
//   M[i][j] -> scalar element at row i, column j
//
// Include this file and call add_scientific(env) to register all builtins.
// The helper headers (Matrix.h, BPF.h, PCA.h, KNN.h, algorithms.h) are
// pure C++ and require no changes.

#ifndef SCIENTIFIC_H
#define SCIENTIFIC_H

#include "musil.h"
#include "scientific/algorithms.h"
#include "scientific/KNN.h"
#include "scientific/Matrix.h"
#include "scientific/PCA.h"
#include "scientific/BPF.h"

#include <valarray>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>

using Real = double;

// ── Conversion helpers ────────────────────────────────────────────────────────

// Array-of-Vectors → C++ Matrix<Real>
// Each element of the ArrayPtr must be a NumVal (row vector).
static Matrix<Real> arr2matrix(const Value& v, const std::string& ctx) {
    if (!std::holds_alternative<ArrayPtr>(v))
        throw Error{"scientific", -1, ctx + ": expected array-of-vectors (matrix)"};
    const auto& rows_arr = std::get<ArrayPtr>(v)->elems;
    if (rows_arr.empty())
        throw Error{"scientific", -1, ctx + ": empty matrix"};
    if (!std::holds_alternative<NumVal>(rows_arr[0]))
        throw Error{"scientific", -1, ctx + ": rows must be numeric vectors"};
    const std::size_t n_rows = rows_arr.size();
    const std::size_t n_cols = std::get<NumVal>(rows_arr[0]).size();
    if (n_cols == 0)
        throw Error{"scientific", -1, ctx + ": zero-length rows"};
    Matrix<Real> m(n_rows, n_cols);
    for (std::size_t i = 0; i < n_rows; ++i) {
        if (!std::holds_alternative<NumVal>(rows_arr[i]))
            throw Error{"scientific", -1, ctx + ": row " + std::to_string(i) + " is not a vector"};
        const NumVal& row = std::get<NumVal>(rows_arr[i]);
        if (row.size() != n_cols)
            throw Error{"scientific", -1, ctx + ": ragged matrix (inconsistent row lengths)"};
        for (std::size_t j = 0; j < n_cols; ++j)
            m(i, j) = row[j];
    }
    return m;
}

// C++ Matrix<Real> → Array-of-Vectors
static Value matrix2arr(const Matrix<Real>& m) {
    auto arr = std::make_shared<Array>();
    arr->elems.reserve(m.rows());
    for (std::size_t i = 0; i < m.rows(); ++i) {
        NumVal row(m.cols());
        for (std::size_t j = 0; j < m.cols(); ++j)
            row[j] = m(i, j);
        arr->elems.push_back(std::move(row));
    }
    return arr;
}

// Convenience: extract scalar from a NumVal argument
static double scalar(const Value& v, const std::string& ctx) {
    if (!std::holds_alternative<NumVal>(v))
        throw Error{"scientific", -1, ctx + ": expected number"};
    return std::get<NumVal>(v)[0];
}

// Convenience: extract NumVal from a Value argument
static const NumVal& nvec(const Value& v, const std::string& ctx) {
    if (!std::holds_alternative<NumVal>(v))
        throw Error{"scientific", -1, ctx + ": expected numeric vector"};
    return std::get<NumVal>(v);
}

// ── Display ───────────────────────────────────────────────────────────────────

static Value fn_matdisp(std::vector<Value>& args, Interpreter& interp) {
    for (auto& a : args) {
        Matrix<Real> m = arr2matrix(a, "matdisp");
        m.print(std::cout) << std::endl;
    }
    return NumVal{0.0};
}

// ── Basic linear algebra ──────────────────────────────────────────────────────

static Value fn_matadd(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2) throw Error{interp.filename, interp.cur_line(), "matadd: at least 2 matrices required"};
    Matrix<Real> a = arr2matrix(args[0], "matadd");
    for (std::size_t i = 1; i < args.size(); ++i) {
        Matrix<Real> b = arr2matrix(args[i], "matadd");
        if (a.rows() != b.rows() || a.cols() != b.cols())
            throw Error{interp.filename, interp.cur_line(), "matadd: shape mismatch"};
        a = a + b;
    }
    return matrix2arr(a);
}

static Value fn_matsub(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2) throw Error{interp.filename, interp.cur_line(), "matsub: at least 2 matrices required"};
    Matrix<Real> a = arr2matrix(args[0], "matsub");
    for (std::size_t i = 1; i < args.size(); ++i) {
        Matrix<Real> b = arr2matrix(args[i], "matsub");
        if (a.rows() != b.rows() || a.cols() != b.cols())
            throw Error{interp.filename, interp.cur_line(), "matsub: shape mismatch"};
        a = a - b;
    }
    return matrix2arr(a);
}

static Value fn_matmul(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2) throw Error{interp.filename, interp.cur_line(), "matmul: at least 2 matrices required"};
    Matrix<Real> a = arr2matrix(args[0], "matmul");
    for (std::size_t i = 1; i < args.size(); ++i) {
        Matrix<Real> b = arr2matrix(args[i], "matmul");
        if (a.cols() != b.rows())
            throw Error{interp.filename, interp.cur_line(), "matmul: nonconformant dimensions"};
        a = a * b;
    }
    return matrix2arr(a);
}

static Value fn_hadamard(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2) throw Error{interp.filename, interp.cur_line(), "hadamard: at least 2 matrices required"};
    Matrix<Real> a = arr2matrix(args[0], "hadamard");
    for (std::size_t i = 1; i < args.size(); ++i) {
        Matrix<Real> b = arr2matrix(args[i], "hadamard");
        if (a.rows() != b.rows() || a.cols() != b.cols())
            throw Error{interp.filename, interp.cur_line(), "hadamard: shape mismatch"};
        for (std::size_t r = 0; r < a.rows(); ++r)
            for (std::size_t c = 0; c < a.cols(); ++c)
                a(r, c) *= b(r, c);
    }
    return matrix2arr(a);
}

static Value fn_transpose(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "transpose: 1 argument required"};
    return matrix2arr(arr2matrix(args[0], "transpose").transpose());
}

static Value fn_nrows(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "nrows: 1 argument required"};
    return NumVal{(double)arr2matrix(args[0], "nrows").rows()};
}

static Value fn_ncols(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "ncols: 1 argument required"};
    return NumVal{(double)arr2matrix(args[0], "ncols").cols()};
}

static Value fn_matsum(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "matsum: 2 arguments required (matrix, axis)"};
    Matrix<Real> a = arr2matrix(args[0], "matsum");
    int axis = (int)scalar(args[1], "matsum");
    if (axis != 0 && axis != 1) throw Error{interp.filename, interp.cur_line(), "matsum: axis must be 0 or 1"};
    return matrix2arr(a.sum(axis));
}

static Value fn_getrows(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 3) throw Error{interp.filename, interp.cur_line(), "getrows: 3 arguments required"};
    Matrix<Real> a = arr2matrix(args[0], "getrows");
    int start = (int)scalar(args[1], "getrows");
    int end   = (int)scalar(args[2], "getrows");
    if (start < 0 || start >= (int)a.rows() || end < start || end >= (int)a.rows())
        throw Error{interp.filename, interp.cur_line(), "getrows: invalid row range"};
    return matrix2arr(a.get_rows(start, end));
}

static Value fn_getcols(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 3) throw Error{interp.filename, interp.cur_line(), "getcols: 3 arguments required"};
    Matrix<Real> a = arr2matrix(args[0], "getcols");
    int start = (int)scalar(args[1], "getcols");
    int end   = (int)scalar(args[2], "getcols");
    if (start < 0 || start >= (int)a.cols() || end < start || end >= (int)a.cols())
        throw Error{interp.filename, interp.cur_line(), "getcols: invalid column range"};
    return matrix2arr(a.get_cols(start, end));
}

// ── Matrix construction ───────────────────────────────────────────────────────

static Value fn_eye(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "eye: 1 argument required"};
    int n = (int)scalar(args[0], "eye");
    if (n <= 0) throw Error{interp.filename, interp.cur_line(), "eye: size must be positive"};
    Matrix<Real> e(n, n); e.id();
    return matrix2arr(e);
}

// randvec(n)  -> NumVal of n values in [-1, 1]
// randmat(cols, rows) -> Array-of-Vectors matrix
static Value fn_rand(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 1 || args.size() > 2)
        throw Error{interp.filename, interp.cur_line(), "rand: 1 or 2 arguments required"};
    int len  = (int)scalar(args[0], "rand");
    int rows = args.size() == 2 ? (int)scalar(args[1], "rand") : 1;
    if (len <= 0 || rows <= 0) throw Error{interp.filename, interp.cur_line(), "rand: dimensions must be positive"};
    if (rows == 1) {
        NumVal out(len);
        for (int i = 0; i < len; ++i)
            out[i] = ((Real)std::rand() / RAND_MAX) * 2.0 - 1.0;
        return out;
    }
    auto arr = std::make_shared<Array>();
    for (int r = 0; r < rows; ++r) {
        NumVal row(len);
        for (int i = 0; i < len; ++i)
            row[i] = ((Real)std::rand() / RAND_MAX) * 2.0 - 1.0;
        arr->elems.push_back(std::move(row));
    }
    return arr;
}

// zeros/ones: zeros(n) -> vector; zeros(cols, rows) -> matrix
static Value fn_zeros(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 1 || args.size() > 2)
        throw Error{interp.filename, interp.cur_line(), "zeros: 1 or 2 arguments"};
    int len  = (int)scalar(args[0], "zeros");
    int rows = args.size() == 2 ? (int)scalar(args[1], "zeros") : 1;
    if (len <= 0 || rows <= 0) throw Error{interp.filename, interp.cur_line(), "zeros: dimensions must be positive"};
    if (rows == 1) return NumVal((size_t)len);
    auto arr = std::make_shared<Array>();
    for (int r = 0; r < rows; ++r) arr->elems.push_back(NumVal((size_t)len));
    return arr;
}

static Value fn_ones(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 1 || args.size() > 2)
        throw Error{interp.filename, interp.cur_line(), "ones: 1 or 2 arguments"};
    int len  = (int)scalar(args[0], "ones");
    int rows = args.size() == 2 ? (int)scalar(args[1], "ones") : 1;
    if (len <= 0 || rows <= 0) throw Error{interp.filename, interp.cur_line(), "ones: dimensions must be positive"};
    if (rows == 1) return NumVal(1.0, (size_t)len);
    auto arr = std::make_shared<Array>();
    for (int r = 0; r < rows; ++r) arr->elems.push_back(NumVal(1.0, (size_t)len));
    return arr;
}

// bpf(init, len0, end0 [, len1, end1, ...]) -> NumVal
// Break-point function: piecewise linear interpolation.
static Value fn_bpf(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 3) throw Error{interp.filename, interp.cur_line(), "bpf: at least 3 arguments: init, len, end"};
    Real init = scalar(args[0], "bpf");
    int  len0 = (int)scalar(args[1], "bpf");
    Real end0 = scalar(args[2], "bpf");
    if (len0 <= 0) throw Error{interp.filename, interp.cur_line(), "bpf: segment length must be positive"};
    std::size_t remaining = args.size() - 3;
    if (remaining % 2 != 0) throw Error{interp.filename, interp.cur_line(), "bpf: extra args must be (len, end) pairs"};
    BPF<Real> bpf(len0);
    bpf.add_segment(init, len0, end0);
    Real curr = end0;
    for (std::size_t i = 0; i < remaining / 2; ++i) {
        int  seg_len = (int)scalar(args[3 + 2*i],     "bpf");
        Real seg_end =      scalar(args[3 + 2*i + 1], "bpf");
        if (seg_len <= 0) throw Error{interp.filename, interp.cur_line(), "bpf: segment length must be positive"};
        bpf.add_segment(curr, seg_len, seg_end);
        curr = seg_end;
    }
    std::valarray<Real> out;
    bpf.process(out);
    return NumVal(out);
}

// ── Matrix decomposition ──────────────────────────────────────────────────────

static Value fn_inv(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "inv: 1 argument required"};
    Matrix<Real> a = arr2matrix(args[0], "inv");
    if (a.rows() != a.cols()) throw Error{interp.filename, interp.cur_line(), "inv: matrix must be square"};
    return matrix2arr(a.inverse());
}

static Value fn_det(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "det: 1 argument required"};
    Matrix<Real> a = arr2matrix(args[0], "det");
    if (a.rows() != a.cols()) throw Error{interp.filename, interp.cur_line(), "det: matrix must be square"};
    return NumVal{a.det()};
}

// diag(v)  -> diagonal matrix (NumVal -> matrix)
// diag(M)  -> extract diagonal (matrix -> NumVal)
static Value fn_diag(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "diag: 1 argument required"};
    if (std::holds_alternative<NumVal>(args[0])) {
        const NumVal& v = std::get<NumVal>(args[0]);
        std::size_t n = v.size();
        Matrix<Real> m(n, n);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                m(i, j) = (i == j) ? v[i] : 0.0;
        return matrix2arr(m);
    }
    if (std::holds_alternative<ArrayPtr>(args[0])) {
        Matrix<Real> a = arr2matrix(args[0], "diag");
        std::size_t n = std::min(a.rows(), a.cols());
        NumVal d(n);
        for (std::size_t i = 0; i < n; ++i) d[i] = a(i, i);
        return d;
    }
    throw Error{interp.filename, interp.cur_line(), "diag: argument must be vector or matrix"};
}

static Value fn_rank(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "rank: 1 argument required"};
    Matrix<Real> a = arr2matrix(args[0], "rank");
    Matrix<Real> m = a;
    const std::size_t rows = m.rows(), cols = m.cols();
    const Real eps = 1e-10;
    std::size_t r = 0, rank = 0;
    for (std::size_t c = 0; c < cols && r < rows; ++c) {
        std::size_t piv = r;
        Real maxv = std::fabs(m(r, c));
        for (std::size_t i = r + 1; i < rows; ++i) {
            Real v = std::fabs(m(i, c));
            if (v > maxv) { maxv = v; piv = i; }
        }
        if (maxv < eps) continue;
        if (piv != r)
            for (std::size_t j = 0; j < cols; ++j) std::swap(m(r, j), m(piv, j));
        for (std::size_t i = r + 1; i < rows; ++i) {
            Real f = m(i, c) / m(r, c);
            for (std::size_t j = c; j < cols; ++j) m(i, j) -= f * m(r, j);
        }
        ++rank; ++r;
    }
    return NumVal{(double)rank};
}

// solve(A, b) -> x such that A*x = b  (A square, b NumVal)
static Value fn_solve(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "solve: 2 arguments required (matrix, vector)"};
    Matrix<Real> A = arr2matrix(args[0], "solve");
    const NumVal& b = nvec(args[1], "solve");
    const std::size_t n = A.rows();
    if (A.cols() != n) throw Error{interp.filename, interp.cur_line(), "solve: A must be square"};
    if (b.size() != n) throw Error{interp.filename, interp.cur_line(), "solve: b length must equal number of rows"};
    Matrix<Real> M = A;
    std::valarray<Real> rhs(b), x((Real)0, n);
    const Real eps = 1e-12;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k; Real maxv = std::fabs(M(k, k));
        for (std::size_t i = k+1; i < n; ++i) { Real v = std::fabs(M(i,k)); if (v > maxv) { maxv = v; piv = i; } }
        if (maxv < eps) throw Error{interp.filename, interp.cur_line(), "solve: singular matrix"};
        if (piv != k) { for (std::size_t j = 0; j < n; ++j) std::swap(M(k,j), M(piv,j)); std::swap(rhs[k], rhs[piv]); }
        for (std::size_t i = k+1; i < n; ++i) {
            Real f = M(i,k) / M(k,k);
            for (std::size_t j = k; j < n; ++j) M(i,j) -= f * M(k,j);
            rhs[i] -= f * rhs[k];
        }
    }
    for (int i = (int)n-1; i >= 0; --i) {
        Real s = 0;
        for (std::size_t j = i+1; j < n; ++j) s += M(i,j) * x[j];
        x[i] = (rhs[i] - s) / M(i,i);
    }
    return NumVal(x);
}

// matcol(M, j) -> NumVal (column j as vector)
static Value fn_matcol(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "matcol: 2 arguments required"};
    Matrix<Real> a = arr2matrix(args[0], "matcol");
    int col = (int)scalar(args[1], "matcol");
    if (col < 0 || col >= (int)a.cols()) throw Error{interp.filename, interp.cur_line(), "matcol: column index out of range"};
    NumVal out(a.rows());
    for (std::size_t i = 0; i < a.rows(); ++i) out[i] = a(i, col);
    return out;
}

// stack2(x, y) -> n×2 matrix from two NumVals of same length
static Value fn_stack2(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "stack2: 2 arguments required"};
    const NumVal& x = nvec(args[0], "stack2");
    const NumVal& y = nvec(args[1], "stack2");
    if (x.size() != y.size()) throw Error{interp.filename, interp.cur_line(), "stack2: x and y must have same length"};
    std::size_t n = x.size();
    Matrix<Real> m(n, 2);
    for (std::size_t i = 0; i < n; ++i) { m(i,0) = x[i]; m(i,1) = y[i]; }
    return matrix2arr(m);
}

// hstack(A, B, ...) -> horizontally concatenated matrix
static Value fn_hstack(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2) throw Error{interp.filename, interp.cur_line(), "hstack: at least 2 matrices required"};
    Matrix<Real> a = arr2matrix(args[0], "hstack");
    std::size_t rows = a.rows(), total_cols = a.cols();
    std::vector<Matrix<Real>> mats{a};
    for (std::size_t i = 1; i < args.size(); ++i) {
        Matrix<Real> m = arr2matrix(args[i], "hstack");
        if (m.rows() != rows) throw Error{interp.filename, interp.cur_line(), "hstack: row count mismatch"};
        total_cols += m.cols(); mats.push_back(m);
    }
    Matrix<Real> out(rows, total_cols);
    std::size_t off = 0;
    for (auto& m : mats) {
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < m.cols(); ++c)
                out(r, off+c) = m(r,c);
        off += m.cols();
    }
    return matrix2arr(out);
}

// vstack(A, B, ...) -> vertically stacked matrix
static Value fn_vstack(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2) throw Error{interp.filename, interp.cur_line(), "vstack: at least 2 matrices required"};
    Matrix<Real> a = arr2matrix(args[0], "vstack");
    std::size_t cols = a.cols(), total_rows = a.rows();
    std::vector<Matrix<Real>> mats{a};
    for (std::size_t i = 1; i < args.size(); ++i) {
        Matrix<Real> m = arr2matrix(args[i], "vstack");
        if (m.cols() != cols) throw Error{interp.filename, interp.cur_line(), "vstack: column count mismatch"};
        total_rows += m.rows(); mats.push_back(m);
    }
    Matrix<Real> out(total_rows, cols);
    std::size_t off = 0;
    for (auto& m : mats) {
        for (std::size_t r = 0; r < m.rows(); ++r)
            for (std::size_t c = 0; c < cols; ++c)
                out(off+r, c) = m(r,c);
        off += m.rows();
    }
    return matrix2arr(out);
}

// ── Statistics and filters ────────────────────────────────────────────────────

// median(v, order) -> NumVal: running median filter of given window order
static Value fn_median(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "median: 2 arguments required (vector, order)"};
    const NumVal& v = nvec(args[0], "median");
    int order = (int)scalar(args[1], "median");
    if (order <= 0) throw Error{interp.filename, interp.cur_line(), "median: order must be positive"};
    if (v.size() == 0) return NumVal{};
    std::size_t n = v.size();
    NumVal out(n);
    int half = order / 2;
    for (std::size_t i = 0; i < n; ++i) {
        int start = std::max(0, (int)i - half);
        int end   = std::min((int)n - 1, (int)i + half);
        int wlen  = end - start + 1;
        std::vector<Real> win(wlen);
        for (int j = 0; j < wlen; ++j) win[j] = v[start + j];
        out[i] = median<Real>(win.data(), wlen);
    }
    return out;
}

// linefit(x, y) -> vec(slope, intercept)
static Value fn_linefit(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "linefit: 2 arguments required"};
    const NumVal& x = nvec(args[0], "linefit");
    const NumVal& y = nvec(args[1], "linefit");
    if (x.size() != y.size()) throw Error{interp.filename, interp.cur_line(), "linefit: x and y must have same size"};
    LineFit<Real> line;
    std::valarray<Real> xv(x), yv(y);
    if (!line.fit(xv, yv)) throw Error{interp.filename, interp.cur_line(), "linefit: cannot fit (vertical line)"};
    Real slope, intercept;
    line.get_params(slope, intercept);
    NumVal r(2); r[0] = slope; r[1] = intercept;
    return r;
}

// norm(v [, p]) -> scalar Lp norm (default L2)
static Value fn_norm(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 1 || args.size() > 2) throw Error{interp.filename, interp.cur_line(), "norm: 1 or 2 arguments"};
    const NumVal& v = nvec(args[0], "norm");
    int p = args.size() == 2 ? (int)scalar(args[1], "norm") : 2;
    if (p <= 0) throw Error{interp.filename, interp.cur_line(), "norm: p must be positive"};
    if (v.size() == 0) return NumVal{0.0};
    Real result = 0;
    if (p == 1) {
        for (std::size_t i = 0; i < v.size(); ++i) result += std::fabs(v[i]);
    } else if (p == 2) {
        for (std::size_t i = 0; i < v.size(); ++i) result += v[i] * v[i];
        result = std::sqrt(result);
    } else {
        for (std::size_t i = 0; i < v.size(); ++i) result += std::pow(std::fabs(v[i]), (Real)p);
        result = std::pow(result, 1.0 / p);
    }
    return NumVal{result};
}

// dist(x, y [, p]) -> scalar Lp distance (default L2)
static Value fn_dist(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() < 2 || args.size() > 3) throw Error{interp.filename, interp.cur_line(), "dist: 2 or 3 arguments"};
    const NumVal& x = nvec(args[0], "dist");
    const NumVal& y = nvec(args[1], "dist");
    if (x.size() != y.size()) throw Error{interp.filename, interp.cur_line(), "dist: x and y must have same length"};
    int p = args.size() == 3 ? (int)scalar(args[2], "dist") : 2;
    if (p <= 0) throw Error{interp.filename, interp.cur_line(), "dist: p must be positive"};
    if (x.size() == 0) return NumVal{0.0};
    Real result = 0;
    if (p == 1) {
        for (std::size_t i = 0; i < x.size(); ++i) result += std::fabs(x[i] - y[i]);
    } else if (p == 2) {
        for (std::size_t i = 0; i < x.size(); ++i) { Real d = x[i]-y[i]; result += d*d; }
        result = std::sqrt(result);
    } else {
        for (std::size_t i = 0; i < x.size(); ++i) result += std::pow(std::fabs(x[i]-y[i]), (Real)p);
        result = std::pow(result, 1.0/p);
    }
    return NumVal{result};
}

// matmean(M, axis) -> NumVal: mean along axis 0 (per column) or 1 (per row)
static Value fn_matmean(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "matmean: 2 arguments required"};
    Matrix<Real> a = arr2matrix(args[0], "matmean");
    int axis = (int)scalar(args[1], "matmean");
    std::size_t rows = a.rows(), cols = a.cols();
    if (axis == 0) {
        NumVal mu(cols);
        for (std::size_t j = 0; j < cols; ++j) {
            Real s = 0; for (std::size_t i = 0; i < rows; ++i) s += a(i,j);
            mu[j] = s / rows;
        }
        return mu;
    } else if (axis == 1) {
        NumVal mu(rows);
        for (std::size_t i = 0; i < rows; ++i) {
            Real s = 0; for (std::size_t j = 0; j < cols; ++j) s += a(i,j);
            mu[i] = s / cols;
        }
        return mu;
    }
    throw Error{interp.filename, interp.cur_line(), "matmean: axis must be 0 or 1"};
}

// matstd(M, axis) -> NumVal: std dev along axis 0 or 1
static Value fn_matstd(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "matstd: 2 arguments required"};
    Matrix<Real> a = arr2matrix(args[0], "matstd");
    int axis = (int)scalar(args[1], "matstd");
    std::size_t rows = a.rows(), cols = a.cols();
    auto compute = [](std::vector<Real>& vals) -> Real {
        if (vals.size() < 2) return 0.0;
        Real s = 0, s2 = 0;
        for (Real v : vals) { s += v; s2 += v*v; }
        Real mean = s / vals.size();
        Real var = s2 / vals.size() - mean*mean;
        return var > 0 ? std::sqrt(var) : 0.0;
    };
    if (axis == 0) {
        NumVal sigma(cols);
        for (std::size_t j = 0; j < cols; ++j) {
            std::vector<Real> col(rows);
            for (std::size_t i = 0; i < rows; ++i) col[i] = a(i,j);
            sigma[j] = compute(col);
        }
        return sigma;
    } else if (axis == 1) {
        NumVal sigma(rows);
        for (std::size_t i = 0; i < rows; ++i) {
            std::vector<Real> row(cols);
            for (std::size_t j = 0; j < cols; ++j) row[j] = a(i,j);
            sigma[i] = compute(row);
        }
        return sigma;
    }
    throw Error{interp.filename, interp.cur_line(), "matstd: axis must be 0 or 1"};
}

// cov(M) -> covariance matrix of columns (rows = observations)
static Value fn_cov(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "cov: 1 argument required"};
    Matrix<Real> a = arr2matrix(args[0], "cov");
    std::size_t n = a.rows(), d = a.cols();
    if (n < 2) throw Error{interp.filename, interp.cur_line(), "cov: need at least 2 rows"};
    NumVal mu(d);
    for (std::size_t j = 0; j < d; ++j) { Real s=0; for (std::size_t i=0;i<n;++i) s+=a(i,j); mu[j]=s/n; }
    Matrix<Real> C(d, d, 0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j1 = 0; j1 < d; ++j1) {
            Real x1 = a(i,j1) - mu[j1];
            for (std::size_t j2 = 0; j2 <= j1; ++j2) {
                C(j1,j2) += x1 * (a(i,j2) - mu[j2]);
            }
        }
    Real denom = (Real)(n-1);
    for (std::size_t j1=0;j1<d;++j1)
        for (std::size_t j2=0;j2<=j1;++j2) {
            C(j1,j2)/=denom;
            if (j1!=j2) C(j2,j1)=C(j1,j2);
        }
    return matrix2arr(C);
}

// corr(M) -> correlation matrix of columns
static Value fn_corr(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "corr: 1 argument required"};
    Matrix<Real> a = arr2matrix(args[0], "corr");
    std::size_t n = a.rows(), d = a.cols();
    if (n < 2) throw Error{interp.filename, interp.cur_line(), "corr: need at least 2 rows"};
    NumVal mu(d), sigma(d);
    for (std::size_t j=0;j<d;++j) {
        Real s=0,s2=0; for (std::size_t i=0;i<n;++i){Real v=a(i,j);s+=v;s2+=v*v;}
        Real mean=s/n; Real var=s2/n-mean*mean;
        mu[j]=mean; sigma[j]=var>0?std::sqrt(var):0.0;
    }
    const Real eps=1e-12;
    Matrix<Real> R(d,d,0);
    for (std::size_t j1=0;j1<d;++j1)
        for (std::size_t j2=0;j2<=j1;++j2) {
            if (j1==j2){R(j1,j2)=1;continue;}
            if (sigma[j1]<eps||sigma[j2]<eps){R(j1,j2)=R(j2,j1)=0;continue;}
            Real s=0;
            for (std::size_t i=0;i<n;++i) s+=(a(i,j1)-mu[j1])*(a(i,j2)-mu[j2]);
            Real r=s/((n-1)*sigma[j1]*sigma[j2]);
            R(j1,j2)=R(j2,j1)=r;
        }
    return matrix2arr(R);
}

// zscore(M) -> column-wise z-score normalized matrix
static Value fn_zscore(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "zscore: 1 argument required"};
    Matrix<Real> a = arr2matrix(args[0], "zscore");
    std::size_t n = a.rows(), d = a.cols();
    if (n == 0 || d == 0) return matrix2arr(a);
    NumVal mu(d), sigma(d);
    for (std::size_t j=0;j<d;++j) {
        Real s=0,s2=0; for(std::size_t i=0;i<n;++i){Real v=a(i,j);s+=v;s2+=v*v;}
        Real mean=s/n; Real var=s2/n-mean*mean;
        mu[j]=mean; sigma[j]=var>0?std::sqrt(var):0.0;
    }
    const Real eps=1e-12;
    Matrix<Real> z(n,d);
    for (std::size_t i=0;i<n;++i)
        for (std::size_t j=0;j<d;++j)
            z(i,j)=sigma[j]<eps ? 0.0 : (a(i,j)-mu[j])/sigma[j];
    return matrix2arr(z);
}

// ── Machine learning ──────────────────────────────────────────────────────────

// pca(M) -> (d x d+1) matrix: first d cols = eigenvectors, last col = eigenvalues
static Value fn_pca(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "pca: 1 argument required"};
    Matrix<Real> X = arr2matrix(args[0], "pca");
    int rows = (int)X.rows(), cols = (int)X.cols();
    if (rows == 0 || cols == 0) throw Error{interp.filename, interp.cur_line(), "pca: empty matrix"};
    std::vector<Real> data_flat(rows*cols);
    for (int i=0;i<rows;++i) for (int j=0;j<cols;++j) data_flat[i*cols+j]=X(i,j);
    int eig_cols = cols+1;
    std::vector<Real> eig_flat(cols*eig_cols, 0.0);
    PCA<Real>(data_flat.data(), eig_flat.data(), cols, rows);
    Matrix<Real> eigm(cols, eig_cols);
    for (int i=0;i<cols;++i) for (int j=0;j<eig_cols;++j) eigm(i,j)=eig_flat[i*eig_cols+j];
    return matrix2arr(eigm);
}

// kmeans(M, K) -> [labels_vector, centroids_matrix]
// labels_vector: NumVal of length n (cluster indices as reals)
// centroids_matrix: K x m Array-of-Vectors
static Value fn_kmeans(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "kmeans: 2 arguments required (matrix, K)"};
    Matrix<Real> X = arr2matrix(args[0], "kmeans");
    int K = (int)scalar(args[1], "kmeans");
    int n = (int)X.rows(), m = (int)X.cols();
    if (n == 0 || m == 0) throw Error{interp.filename, interp.cur_line(), "kmeans: empty matrix"};
    if (K <= 0 || K > n) throw Error{interp.filename, interp.cur_line(), "kmeans: invalid K"};
    std::vector<Real> data_flat(n*m);
    for (int i=0;i<n;++i) for (int j=0;j<m;++j) data_flat[i*m+j]=X(i,j);
    std::vector<int>  labels_vec(n);
    std::vector<Real> centroids_flat(K*m, 0.0);
    kmeans<Real>(data_flat.data(), n, m, K, (Real)1e-5, labels_vec.data(), centroids_flat.data());
    Matrix<Real> centroids(K, m);
    for (int i=0;i<K;++i) for (int j=0;j<m;++j) centroids(i,j)=centroids_flat[i*m+j];
    NumVal labels_va(n);
    for (int i=0;i<n;++i) labels_va[i]=(Real)labels_vec[i];
    auto res = std::make_shared<Array>();
    res->elems.push_back(labels_va);
    res->elems.push_back(matrix2arr(centroids));
    return res;
}

// knn(train_data, K, queries) -> array of label strings
// train_data: array of [features_vector, label_string] pairs
// queries: array of NumVals
static Value fn_knn(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 3) throw Error{interp.filename, interp.cur_line(), "knn: 3 arguments required (train, K, queries)"};
    if (!std::holds_alternative<ArrayPtr>(args[0]))
        throw Error{interp.filename, interp.cur_line(), "knn: training data must be an array"};
    const auto& train_arr = std::get<ArrayPtr>(args[0])->elems;
    int obs = (int)train_arr.size();
    if (obs < 1) throw Error{interp.filename, interp.cur_line(), "knn: insufficient training data"};
    int K = (int)scalar(args[1], "knn");
    if (K < 1 || K > obs) throw Error{interp.filename, interp.cur_line(), "knn: invalid K"};
    // Determine feature dimension from first training sample
    if (!std::holds_alternative<ArrayPtr>(train_arr[0]))
        throw Error{interp.filename, interp.cur_line(), "knn: each training item must be [features, label]"};
    const auto& first = std::get<ArrayPtr>(train_arr[0])->elems;
    if (first.size() != 2 || !std::holds_alternative<NumVal>(first[0]))
        throw Error{interp.filename, interp.cur_line(), "knn: training item[0] must be [NumVec, label]"};
    int features = (int)std::get<NumVal>(first[0]).size();
    KNN<Real> knn_model(K, features);
    for (int i = 0; i < obs; ++i) {
        const auto& item = std::get<ArrayPtr>(train_arr[i])->elems;
        auto* o = new Observation<Real>();
        o->attributes = std::valarray<Real>(std::get<NumVal>(item[0]));
        o->classlabel  = to_str(item[1]);
        knn_model.addObservation(o);
    }
    if (!std::holds_alternative<ArrayPtr>(args[2]))
        throw Error{interp.filename, interp.cur_line(), "knn: queries must be an array"};
    const auto& queries = std::get<ArrayPtr>(args[2])->elems;
    auto out = std::make_shared<Array>();
    for (const auto& q : queries) {
        Observation<Real> qo;
        qo.attributes = std::valarray<Real>(nvec(q, "knn query"));
        out->elems.push_back(knn_model.classify(qo));
    }
    return out;
}

// ── Registration ──────────────────────────────────────────────────────────────

inline void add_scientific(Environment& env) {
    // Display
    env.register_builtin("matdisp",   fn_matdisp);

    // Basic linear algebra
    env.register_builtin("matadd",    fn_matadd);
    env.register_builtin("matsub",    fn_matsub);
    env.register_builtin("matmul",    fn_matmul);
    env.register_builtin("hadamard",  fn_hadamard);
    env.register_builtin("transpose", fn_transpose);
    env.register_builtin("nrows",     fn_nrows);
    env.register_builtin("ncols",     fn_ncols);
    env.register_builtin("matsum",    fn_matsum);
    env.register_builtin("getrows",   fn_getrows);
    env.register_builtin("getcols",   fn_getcols);

    // Construction / decomposition
    env.register_builtin("eye",       fn_eye);
    env.register_builtin("rand",      fn_rand);
    env.register_builtin("zeros",     fn_zeros);
    env.register_builtin("ones",      fn_ones);
    env.register_builtin("bpf",       fn_bpf);
    env.register_builtin("inv",       fn_inv);
    env.register_builtin("det",       fn_det);
    env.register_builtin("diag",      fn_diag);
    env.register_builtin("rank",      fn_rank);
    env.register_builtin("solve",     fn_solve);
    env.register_builtin("matcol",    fn_matcol);
    env.register_builtin("stack2",    fn_stack2);
    env.register_builtin("hstack",    fn_hstack);
    env.register_builtin("vstack",    fn_vstack);

    // Statistics / filters
    env.register_builtin("median",    fn_median);
    env.register_builtin("linefit",   fn_linefit);
    env.register_builtin("norm",      fn_norm);
    env.register_builtin("dist",      fn_dist);
    env.register_builtin("matmean",   fn_matmean);
    env.register_builtin("matstd",    fn_matstd);
    env.register_builtin("cov",       fn_cov);
    env.register_builtin("corr",      fn_corr);
    env.register_builtin("zscore",    fn_zscore);

    // Machine learning
    env.register_builtin("pca",       fn_pca);
    env.register_builtin("kmeans",    fn_kmeans);
    env.register_builtin("knn",       fn_knn);
}

#endif // SCIENTIFIC_H
// eof
