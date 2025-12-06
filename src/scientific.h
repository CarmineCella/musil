// scientific.h
//
// Scientific / linear-algebra / statistics library for Musil
//
// Exposes matrices as LIST-of-ARRAYs at the language level:
//
//   matrix = ( (row0-vec) (row1-vec) ... )
//
// and provides a set of primitive operators for linear algebra and
// simple ML-style processing.

#ifndef SCIENTIFIC_H
#define SCIENTIFIC_H

#include "core.h"
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

// helpers
AtomPtr matrix2list(Matrix<Real>& m) {
    AtomPtr l = make_atom(); // LIST
    for (std::size_t row = 0; row < m.rows(); ++row) {
        std::valarray<Real> rarray(m.cols());
        for (std::size_t col = 0; col < m.cols(); ++col) {
            rarray[col] = m(row, col);
        }
        l->tail.push_back(make_atom(rarray));
    }
    return l;
}
Matrix<Real> list2matrix(AtomPtr l) {
    AtomPtr lst = type_check(l, LIST);

    const std::size_t n_rows = lst->tail.size();
    if (n_rows == 0) {
        error("[list2matrix] empty matrix (no rows)", l);
    }

    // First row determines column count
    AtomPtr first_row_atom = type_check(lst->tail.at(0), ARRAY);
    const std::size_t n_cols = first_row_atom->array.size();
    if (n_cols == 0) {
        error("[list2matrix] empty first row", l);
    }

    Matrix<Real> m(n_rows, n_cols);

    for (std::size_t i = 0; i < n_rows; ++i) {
        AtomPtr row_atom = type_check(lst->tail.at(i), ARRAY);
        std::valarray<Real>& row = row_atom->array;
        if (row.size() != n_cols) {
            error("[list2matrix] ragged matrix (inconsistent row lengths)", l);
        }
        for (std::size_t j = 0; j < n_cols; ++j) {
            m(i, j) = row[j];
        }
    }

    return m;
}

// basic linear algebra: matmul, matsub, hadamard, transpose, shape
AtomPtr fn_matdisp(AtomPtr node, AtomPtr env) {
    for (unsigned i = 0; i < node->tail.size(); ++i) {
        AtomPtr lmatrix = type_check(node->tail.at(i), LIST);
        Matrix<Real> m = list2matrix(lmatrix);
        m.print(std::cout) << std::endl;
    }
    // Return the empty string just as a sentinel "unit" value
    return make_atom("");
}
#define MAKE_MATMUL_BINOP(op, name, tag) \
AtomPtr name(AtomPtr node, AtomPtr env) { \
    if (node->tail.size() < 2) { \
        error("[" tag "] at least two matrices required", node); \
    } \
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST)); \
    for (unsigned i = 1; i < node->tail.size(); ++i) { \
        Matrix<Real> b = list2matrix(type_check(node->tail.at(i), LIST)); \
        if (a.cols() != b.rows()) { \
            error("[" tag "] nonconformant arguments", node); \
        } \
        a = a op b; \
    }  \
    return matrix2list(a); \
}

#define MAKE_MATSAME_BINOP(op, name, tag) \
AtomPtr name(AtomPtr node, AtomPtr env) { \
    if (node->tail.size() < 2) { \
        error("[" tag "] at least two matrices required", node); \
    } \
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST)); \
    for (unsigned i = 1; i < node->tail.size(); ++i) { \
        Matrix<Real> b = list2matrix(type_check(node->tail.at(i), LIST)); \
        if (a.rows() != b.rows() || a.cols() != b.cols()) { \
            error("[" tag "] nonconformant arguments (shape mismatch)", node); \
        } \
        a = a op b; \
    }  \
    return matrix2list(a); \
}

MAKE_MATSAME_BINOP(+, fn_matadd, "matadd");
MAKE_MATMUL_BINOP(*, fn_matmul, "matmul");
MAKE_MATSAME_BINOP(-, fn_matsub, "matsub");

AtomPtr fn_hadamard(AtomPtr node, AtomPtr env) {// Element-wise product: hadamard / .*
    if (node->tail.size() < 2) {
        error("[hadamard] at least two matrices required", node);
    }

    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));

    for (unsigned i = 1; i < node->tail.size(); ++i) {
        Matrix<Real> b = list2matrix(type_check(node->tail.at(i), LIST));
        if (a.rows() != b.rows() || a.cols() != b.cols()) {
            error("[hadamard] nonconformant arguments", node);
        }
        for (std::size_t r = 0; r < a.rows(); ++r) {
            for (std::size_t c = 0; c < a.cols(); ++c) {
                a(r, c) *= b(r, c);
            }
        }
    }
    return matrix2list(a);
}
AtomPtr fn_mattran(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    Matrix<Real> tr = a.transpose();
    return matrix2list(tr);
}
AtomPtr fn_nrows(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    return make_atom(static_cast<int>(a.rows()));
}
AtomPtr fn_ncols(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    return make_atom(static_cast<int>(a.cols()));
}
AtomPtr fn_matsum(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    int axis = (int)type_check(node->tail.at(1), ARRAY)->array[0];

    if (axis != 0 && axis != 1) {
        error("[matsum] axis must be 0 (columns) or 1 (rows)", node);
    }

    Matrix<Real> b = a.sum(axis);
    return matrix2list(b);
}
template <int MODE>
AtomPtr fn_matget(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    int start = (int)type_check(node->tail.at(1), ARRAY)->array[0];
    int end   = (int)type_check(node->tail.at(2), ARRAY)->array[0];

    Matrix<Real> b(1, 1);

    if (MODE == 0) { // rows
        if (start < 0 || start >= (int)a.rows() ||
                end   < 0 || end   >= (int)a.rows() || end < start) {
            error("[getrows] invalid row selection", node);
        }
        b = a.get_rows(start, end);
    } else { // cols
        if (start < 0 || start >= (int)a.cols() ||
                end   < 0 || end   >= (int)a.cols() || end < start) {
            error("[getcols] invalid col selection", node);
        }
        b = a.get_cols(start, end);
    }
    return matrix2list(b);
}

AtomPtr fn_eye(AtomPtr node, AtomPtr env) {
    int n = (int)type_check(node->tail.at(0), ARRAY)->array[0];
    if (n <= 0) {
        error("[eye] size must be positive", node);
    }
    Matrix<Real> e(n, n);
    e.id();
    return matrix2list(e);
}
AtomPtr fn_rand(AtomPtr node, AtomPtr env) {
    (void)env;
    const std::size_t nargs = node->tail.size();
    if (nargs < 1 || nargs > 2) {
        error("[rand] expects 1 or 2 numeric arguments", node);
    }
    AtomPtr len_atom = type_check(node->tail.at(0), ARRAY);
    int len = static_cast<int>(len_atom->array[0]);
    if (len <= 0) {
        error("[rand] length must be positive", node);
    }
    int rows = 1;
    if (nargs == 2) {
        AtomPtr rows_atom = type_check(node->tail.at(1), ARRAY);
        rows = static_cast<int>(rows_atom->array[0]);
        if (rows <= 0) {
            error("[rand] number of rows must be positive", node);
        }
    }
    if (rows == 1) {
        std::valarray<Real> out((Real)0, (std::size_t)len);
        for (int i = 0; i < len; ++i) {
            out[(std::size_t)i] = ((Real)std::rand() / (Real)RAND_MAX) * (Real)2.0 - (Real)1.0;
        }
        return make_atom(out);
    } else {
        AtomPtr l = make_atom(); // LIST
        for (int j = 0; j < rows; ++j) {
            std::valarray<Real> out((Real)0, (std::size_t)len);
            for (int i = 0; i < len; ++i) {
                out[(std::size_t)i] = ((Real)std::rand() / (Real)RAND_MAX) * (Real)2.0 - (Real)1.0;
            }
            l->tail.push_back(make_atom(out));
        }
        return l;
    }
}
AtomPtr fn_zeros(AtomPtr node, AtomPtr env) {
    (void)env;
    const std::size_t nargs = node->tail.size();
    if (nargs < 1 || nargs > 2) {
        error("[zeros] expects 1 or 2 numeric arguments", node);
    }

    AtomPtr len_atom = type_check(node->tail.at(0), ARRAY);
    int len = static_cast<int>(len_atom->array[0]);
    if (len <= 0) {
        error("[zeros] length must be positive", node);
    }

    int rows = 1;
    if (nargs == 2) {
        AtomPtr rows_atom = type_check(node->tail.at(1), ARRAY);
        rows = static_cast<int>(rows_atom->array[0]);
        if (rows <= 0) {
            error("[zeros] number of rows must be positive", node);
        }
    }

    if (rows == 1) {
        // 1D vector
        std::valarray<Real> out((Real)0, (std::size_t)len);
        return make_atom(out);
    } else {
        // rows x len matrix → LIST of ARRAY rows
        AtomPtr l = make_atom(); // LIST
        for (int j = 0; j < rows; ++j) {
            std::valarray<Real> out((Real)0, (std::size_t)len);
            l->tail.push_back(make_atom(out));
        }
        return l;
    }
}

AtomPtr fn_ones(AtomPtr node, AtomPtr env) {
    (void)env;
    const std::size_t nargs = node->tail.size();
    if (nargs < 1 || nargs > 2) {
        error("[ones] expects 1 or 2 numeric arguments", node);
    }

    AtomPtr len_atom = type_check(node->tail.at(0), ARRAY);
    int len = static_cast<int>(len_atom->array[0]);
    if (len <= 0) {
        error("[ones] length must be positive", node);
    }

    int rows = 1;
    if (nargs == 2) {
        AtomPtr rows_atom = type_check(node->tail.at(1), ARRAY);
        rows = static_cast<int>(rows_atom->array[0]);
        if (rows <= 0) {
            error("[ones] number of rows must be positive", node);
        }
    }

    if (rows == 1) {
        std::valarray<Real> out((Real)1, (std::size_t)len);
        return make_atom(out);
    } else {
        AtomPtr l = make_atom(); // LIST
        for (int j = 0; j < rows; ++j) {
            std::valarray<Real> out((Real)1, (std::size_t)len);
            l->tail.push_back(make_atom(out));
        }
        return l;
    }
}
AtomPtr fn_bpf(AtomPtr node, AtomPtr env) {
    (void)env;

    const std::size_t nargs = node->tail.size();
    if (nargs < 3) {
        error("[bpf] requires at least 3 arguments: init, len0, end0", node);
    }

    // First segment: init, len0, end0
    Real init = type_check(node->tail.at(0), ARRAY)->array[0];
    int  len0 = static_cast<int>(type_check(node->tail.at(1), ARRAY)->array[0]);
    Real end0 = type_check(node->tail.at(2), ARRAY)->array[0];

    if (len0 <= 0) {
        error("[bpf] segment length must be positive", node);
    }

    // Remaining arguments must come in (len, end) pairs
    std::size_t remaining = nargs - 3;
    if (remaining % 2 != 0) {
        error("[bpf] invalid number of arguments (expected pairs of len/end after first segment)", node);
    }

    BPF<Real> bpf(len0);
    bpf.add_segment(init, len0, end0);

    Real curr = end0;
    std::size_t n_extra = remaining / 2;

    for (std::size_t i = 0; i < n_extra; ++i) {
        // Index into the tail after the first 3 args:
        // arg index = 3 + 2*i (len) and 3 + 2*i + 1 (end)
        AtomPtr len_atom = type_check(node->tail.at(3 + 2 * i), ARRAY);
        AtomPtr end_atom = type_check(node->tail.at(3 + 2 * i + 1), ARRAY);

        int  seg_len  = static_cast<int>(len_atom->array[0]);
        Real seg_end  = end_atom->array[0];

        if (seg_len <= 0) {
            error("[bpf] segment length must be positive", node);
        }

        bpf.add_segment(curr, seg_len, seg_end);
        curr = seg_end;
    }

    std::valarray<Real> out;
    bpf.process(out);

    return make_atom(out);
}
AtomPtr fn_inv(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    if (a.rows() != a.cols()) {
        error("[inv] matrix must be square", node);
    }
    Matrix<Real> inv = a.inverse();
    return matrix2list(inv);
}
AtomPtr fn_det(AtomPtr node, AtomPtr env) {
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    if (a.rows() != a.cols()) {
        error("[det] matrix must be square", node);
    }
    return make_atom(a.det());
}
AtomPtr fn_diag(AtomPtr node, AtomPtr env) { // diag: ARRAY -> diagonal matrix, LIST(matrix) -> ARRAY(diagonal)
    AtomPtr arg = node->tail.at(0);

    if (arg->type == ARRAY) {
        std::valarray<Real>& v = arg->array;
        const std::size_t n = v.size();
        Matrix<Real> m(n, n);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                m(i, j) = (i == j) ? v[i] : Real(0);
            }
        }
        return matrix2list(m);
    }

    if (arg->type == LIST) {
        Matrix<Real> a = list2matrix(arg);
        const std::size_t n = std::min(a.rows(), a.cols());
        std::valarray<Real> d((Real)0, n);
        for (std::size_t i = 0; i < n; ++i) {
            d[i] = a(i, i);
        }
        return make_atom(d);
    }

    error("[diag] argument must be ARRAY or matrix (LIST)", node);
    return make_atom(0); // unreachable
}
AtomPtr fn_rank(AtomPtr node, AtomPtr env) { // Simple Gaussian-elimination-based rank estimation
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));

    Matrix<Real> m = a; // copy
    const std::size_t rows = m.rows();
    const std::size_t cols = m.cols();
    const Real eps = (Real)1e-10;

    std::size_t r = 0;
    std::size_t rank = 0;

    for (std::size_t c = 0; c < cols && r < rows; ++c) {
        // pivot: find row with max abs in column c from r..rows-1
        std::size_t piv = r;
        Real maxv = std::fabs(m(r, c));
        for (std::size_t i = r + 1; i < rows; ++i) {
            Real v = std::fabs(m(i, c));
            if (v > maxv) {
                maxv = v;
                piv = i;
            }
        }
        if (maxv < eps) {
            continue; // no pivot in this column
        }

        if (piv != r) {
            for (std::size_t j = 0; j < cols; ++j) {
                std::swap(m(r, j), m(piv, j));
            }
        }

        // eliminate below
        for (std::size_t i = r + 1; i < rows; ++i) {
            Real factor = m(i, c) / m(r, c);
            for (std::size_t j = c; j < cols; ++j) {
                m(i, j) -= factor * m(r, j);
            }
        }

        ++rank;
        ++r;
    }

    return make_atom(static_cast<int>(rank));
}
AtomPtr fn_solve(AtomPtr node, AtomPtr env) { // solve Ax = b for square A and vector b (single RHS)
    Matrix<Real> A = list2matrix(type_check(node->tail.at(0), LIST));
    std::valarray<Real>& b = type_check(node->tail.at(1), ARRAY)->array;

    const std::size_t n = A.rows();
    if (A.cols() != n) {
        error("[solve] A must be square", node);
    }
    if (b.size() != n) {
        error("[solve] b must have length equal to number of rows of A", node);
    }

    Matrix<Real> M = A; // copy
    std::valarray<Real> rhs = b;
    std::valarray<Real> x((Real)0, n);

    const Real eps = (Real)1e-12;

    // Forward elimination with partial pivoting
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        Real maxv = std::fabs(M(k, k));
        for (std::size_t i = k + 1; i < n; ++i) {
            Real v = std::fabs(M(i, k));
            if (v > maxv) {
                maxv = v;
                piv = i;
            }
        }
        if (maxv < eps) {
            error("[solve] singular or nearly singular matrix", node);
        }
        if (piv != k) {
            for (std::size_t j = 0; j < n; ++j) {
                std::swap(M(k, j), M(piv, j));
            }
            std::swap(rhs[k], rhs[piv]);
        }

        for (std::size_t i = k + 1; i < n; ++i) {
            Real f = M(i, k) / M(k, k);
            for (std::size_t j = k; j < n; ++j) {
                M(i, j) -= f * M(k, j);
            }
            rhs[i] -= f * rhs[k];
        }
    }

    // Back substitution
    for (int i = (int)n - 1; i >= 0; --i) {
        Real sum = 0;
        for (std::size_t j = i + 1; j < n; ++j) {
            sum += M((std::size_t)i, j) * x[j];
        }
        Real diag = M((std::size_t)i, (std::size_t)i);
        if (std::fabs(diag) < eps) {
            error("[solve] zero pivot in back substitution", node);
        }
        x[(std::size_t)i] = (rhs[(std::size_t)i] - sum) / diag;
    }

    return make_atom(x);
}
AtomPtr fn_matcol(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 2) {
        error("[matcol] expects matrix and column index", node);
    }

    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    std::valarray<Real>& idx_arr = type_check(node->tail.at(1), ARRAY)->array;
    if (idx_arr.size() < 1) {
        error("[matcol] column index must be a scalar array", node);
    }

    int col = static_cast<int>(idx_arr[0]);
    if (col < 0 || col >= static_cast<int>(a.cols())) {
        error("[matcol] column index out of range", node);
    }

    const std::size_t rows = a.rows();
    std::valarray<Real> out((Real)0, rows);
    for (std::size_t i = 0; i < rows; ++i) {
        out[i] = a(i, (std::size_t)col);
    }

    return make_atom(out);
}
AtomPtr fn_stack2(AtomPtr node, AtomPtr env) {
    (void)env; // given two arrays x, y of same length, build an N x 2 matrix.

    if (node->tail.size() < 2) {
        error("[stack2] expects two arrays x and y", node);
    }

    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    std::valarray<Real>& y = type_check(node->tail.at(1), ARRAY)->array;

    if (x.size() != y.size()) {
        error("[stack2] x and y must have the same length", node);
    }

    const std::size_t n = x.size();
    Matrix<Real> m(n, 2);

    for (std::size_t i = 0; i < n; ++i) {
        m(i, 0) = x[i];
        m(i, 1) = y[i];
    }

    return matrix2list(m);
}
AtomPtr fn_hstack(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 2) {
        error("[hstack] expects at least two matrices", node);
    }

    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    const std::size_t rows = a.rows();
    std::size_t total_cols = a.cols();

    // accumulate columns from subsequent matrices
    std::vector<Matrix<Real>> mats;
    mats.push_back(a);

    for (unsigned i = 1; i < node->tail.size(); ++i) {
        Matrix<Real> m = list2matrix(type_check(node->tail.at(i), LIST));
        if (m.rows() != rows) {
            error("[hstack] all matrices must have the same number of rows", node);
        }
        total_cols += m.cols();
        mats.push_back(m);
    }

    Matrix<Real> out(rows, total_cols);
    std::size_t col_offset = 0;

    for (const auto& m : mats) {
        for (std::size_t r = 0; r < rows; ++r) {
            for (std::size_t c = 0; c < m.cols(); ++c) {
                out(r, col_offset + c) = m(r, c);
            }
        }
        col_offset += m.cols();
    }

    return matrix2list(out);
}

AtomPtr fn_vstack(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 2) {
        error("[vstack] expects at least two matrices", node);
    }

    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    const std::size_t cols = a.cols();
    std::size_t total_rows = a.rows();

    std::vector<Matrix<Real>> mats;
    mats.push_back(a);

    for (unsigned i = 1; i < node->tail.size(); ++i) {
        Matrix<Real> m = list2matrix(type_check(node->tail.at(i), LIST));
        if (m.cols() != cols) {
            error("[vstack] all matrices must have the same number of columns", node);
        }
        total_rows += m.rows();
        mats.push_back(m);
    }

    Matrix<Real> out(total_rows, cols);
    std::size_t row_offset = 0;

    for (const auto& m : mats) {
        for (std::size_t r = 0; r < m.rows(); ++r) {
            for (std::size_t c = 0; c < cols; ++c) {
                out(row_offset + r, c) = m(r, c);
            }
        }
        row_offset += m.rows();
    }

    return matrix2list(out);
}

// simple statistics
AtomPtr fn_median(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& v = type_check(node->tail.at(0), ARRAY)->array;
    int order = (int)type_check(node->tail.at(1), ARRAY)->array[0];

    if (order <= 0) {
        error("[median] order must be positive", node);
    }
    if (v.size() == 0) {
        return make_atom(v); // empty in, empty out
    }

    const std::size_t n = v.size();
    std::valarray<Real> out((Real)0, n);
    const int half = order / 2;

    for (std::size_t i = 0; i < n; ++i) {
        int start = (int)i - half;
        int end   = (int)i + half;

        if (start < 0) start = 0;
        if (end >= (int)n) end = (int)n - 1;

        const int wlen = end - start + 1;
        std::vector<Real> window;
        window.reserve((std::size_t)wlen);
        for (int j = 0; j < wlen; ++j) {
            window.push_back(v[(std::size_t)start + (std::size_t)j]);
        }

        out[i] = median<Real>(&window[0], wlen);
    }

    return make_atom(out);
}
AtomPtr fn_linefit(AtomPtr node, AtomPtr env) {
    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    std::valarray<Real>& y = type_check(node->tail.at(1), ARRAY)->array;
    if (x.size() != y.size()) {
        error("[linefit] x and y must have the same size", node);
    }

    LineFit<Real> line;
    if (!line.fit(x, y)) {
        error("[linefit] cannot fit a vertical line", node);
    }

    Real slope = 0, intercept = 0;
    line.get_params(slope, intercept);
    std::valarray<Real> l({slope, intercept});
    return make_atom(l);
}
AtomPtr fn_norm(AtomPtr node, AtomPtr env) {
    (void)env;

    const std::size_t nargs = node->tail.size();
    if (nargs < 1 || nargs > 2) {
        error("[norm] expects 1 or 2 arguments: vector, [p]", node);
    }

    std::valarray<Real>& v = type_check(node->tail.at(0), ARRAY)->array;

    int p = 2; // default L2
    if (nargs == 2) {
        std::valarray<Real>& parr = type_check(node->tail.at(1), ARRAY)->array;
        if (parr.size() < 1) {
            error("[norm] p must be a scalar array", node);
        }
        p = static_cast<int>(parr[0]);
        if (p <= 0) {
            error("[norm] p must be positive (typically 1 or 2)", node);
        }
    }

    if (v.size() == 0) {
        return make_atom((Real)0);
    }

    Real result = 0;
    if (p == 1) {
        // L1 norm
        Real s = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            s += std::fabs(v[i]);
        }
        result = s;
    } else if (p == 2) {
        // L2 norm
        Real s = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            s += v[i] * v[i];
        }
        result = std::sqrt(s);
    } else {
        // Generic Lp
        Real s = 0;
        for (std::size_t i = 0; i < v.size(); ++i) {
            s += std::pow(std::fabs(v[i]), (Real)p);
        }
        result = std::pow(s, (Real)1.0 / (Real)p);
    }

    return make_atom(result);
}

AtomPtr fn_dist(AtomPtr node, AtomPtr env) {
    (void)env;

    const std::size_t nargs = node->tail.size();
    if (nargs < 2 || nargs > 3) {
        error("[dist] expects 2 or 3 arguments: x, y, [p]", node);
    }

    std::valarray<Real>& x = type_check(node->tail.at(0), ARRAY)->array;
    std::valarray<Real>& y = type_check(node->tail.at(1), ARRAY)->array;

    if (x.size() != y.size()) {
        error("[dist] x and y must have the same length", node);
    }

    int p = 2; // default L2
    if (nargs == 3) {
        std::valarray<Real>& parr = type_check(node->tail.at(2), ARRAY)->array;
        if (parr.size() < 1) {
            error("[dist] p must be a scalar array", node);
        }
        p = static_cast<int>(parr[0]);
        if (p <= 0) {
            error("[dist] p must be positive (typically 1 or 2)", node);
        }
    }

    if (x.size() == 0) {
        return make_atom((Real)0);
    }

    Real result = 0;
    if (p == 1) {
        Real s = 0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            s += std::fabs(x[i] - y[i]);
        }
        result = s;
    } else if (p == 2) {
        Real s = 0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            Real d = x[i] - y[i];
            s += d * d;
        }
        result = std::sqrt(s);
    } else {
        Real s = 0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            Real d = std::fabs(x[i] - y[i]);
            s += std::pow(d, (Real)p);
        }
        result = std::pow(s, (Real)1.0 / (Real)p);
    }

    return make_atom(result);
}
AtomPtr fn_matmean(AtomPtr node, AtomPtr env) { // Matrix mean along axis -> ARRAY
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    int axis = (int)type_check(node->tail.at(1), ARRAY)->array[0];

    const std::size_t rows = a.rows();
    const std::size_t cols = a.cols();

    if (axis == 0) {
        // mean over rows -> per-column mean (length cols)
        std::valarray<Real> mu((Real)0, cols);
        if (rows > 0) {
            for (std::size_t j = 0; j < cols; ++j) {
                Real s = 0;
                for (std::size_t i = 0; i < rows; ++i) {
                    s += a(i, j);
                }
                mu[j] = s / (Real)rows;
            }
        }
        return make_atom(mu);
    } else if (axis == 1) {
        // mean over cols -> per-row mean (length rows)
        std::valarray<Real> mu((Real)0, rows);
        if (cols > 0) {
            for (std::size_t i = 0; i < rows; ++i) {
                Real s = 0;
                for (std::size_t j = 0; j < cols; ++j) {
                    s += a(i, j);
                }
                mu[i] = s / (Real)cols;
            }
        }
        return make_atom(mu);
    } else {
        error("[matmean] axis must be 0 (columns) or 1 (rows)", node);
    }
    return make_atom(0);
}
AtomPtr fn_matstd(AtomPtr node, AtomPtr env) { // Matrix std along axis -> ARRAY
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));
    int axis = (int)type_check(node->tail.at(1), ARRAY)->array[0];

    const std::size_t rows = a.rows();
    const std::size_t cols = a.cols();

    if (axis == 0) {
        std::valarray<Real> sigma((Real)0, cols);
        if (rows > 1) {
            for (std::size_t j = 0; j < cols; ++j) {
                Real s = 0, s2 = 0;
                for (std::size_t i = 0; i < rows; ++i) {
                    Real v = a(i, j);
                    s  += v;
                    s2 += v * v;
                }
                Real mean = s / (Real)rows;
                Real var  = (s2 / (Real)rows) - mean * mean;
                if (var < 0) var = 0;
                sigma[j] = std::sqrt(var);
            }
        }
        return make_atom(sigma);
    } else if (axis == 1) {
        std::valarray<Real> sigma((Real)0, rows);
        if (cols > 1) {
            for (std::size_t i = 0; i < rows; ++i) {
                Real s = 0, s2 = 0;
                for (std::size_t j = 0; j < cols; ++j) {
                    Real v = a(i, j);
                    s  += v;
                    s2 += v * v;
                }
                Real mean = s / (Real)cols;
                Real var  = (s2 / (Real)cols) - mean * mean;
                if (var < 0) var = 0;
                sigma[i] = std::sqrt(var);
            }
        }
        return make_atom(sigma);
    } else {
        error("[matstd] axis must be 0 (columns) or 1 (rows)", node);
    }
    return make_atom(0);
}
AtomPtr fn_cov(AtomPtr node, AtomPtr env) { // Covariance of columns (rows = observations, cols = variables)
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));

    const std::size_t n = a.rows();
    const std::size_t d = a.cols();

    if (n < 2 || d == 0) {
        error("[cov] not enough data", node);
    }

    // column means
    std::valarray<Real> mu((Real)0, d);
    for (std::size_t j = 0; j < d; ++j) {
        Real s = 0;
        for (std::size_t i = 0; i < n; ++i) {
            s += a(i, j);
        }
        mu[j] = s / (Real)n;
    }

    Matrix<Real> C(d, d, 0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j1 = 0; j1 < d; ++j1) {
            Real x1 = a(i, j1) - mu[j1];
            for (std::size_t j2 = 0; j2 <= j1; ++j2) {
                Real x2 = a(i, j2) - mu[j2];
                C(j1, j2) += x1 * x2;
            }
        }
    }

    Real denom = (Real)(n - 1);
    for (std::size_t j1 = 0; j1 < d; ++j1) {
        for (std::size_t j2 = 0; j2 <= j1; ++j2) {
            C(j1, j2) /= denom;
            if (j1 != j2) {
                C(j2, j1) = C(j1, j2);
            }
        }
    }

    return matrix2list(C);
}
AtomPtr fn_corr(AtomPtr node, AtomPtr env) {
    (void)env;

    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));

    const std::size_t n = a.rows();
    const std::size_t d = a.cols();

    if (n < 2 || d == 0) {
        error("[corr] not enough data", node);
    }

    // column means and std-devs
    std::valarray<Real> mu((Real)0, d);
    std::valarray<Real> sigma((Real)0, d);

    for (std::size_t j = 0; j < d; ++j) {
        Real s = 0, s2 = 0;
        for (std::size_t i = 0; i < n; ++i) {
            Real v = a(i, j);
            s  += v;
            s2 += v * v;
        }
        Real mean = s / (Real)n;
        Real var  = (s2 / (Real)n) - mean * mean;
        if (var < 0) var = 0;
        mu[j]    = mean;
        sigma[j] = std::sqrt(var);
    }

    const Real eps = (Real)1e-12;
    Matrix<Real> R(d, d, 0);

    for (std::size_t j1 = 0; j1 < d; ++j1) {
        for (std::size_t j2 = 0; j2 <= j1; ++j2) {
            if (j1 == j2) {
                R(j1, j2) = (Real)1;   // corr(x,x) = 1
                continue;
            }

            Real sd1 = sigma[j1];
            Real sd2 = sigma[j2];
            if (sd1 < eps || sd2 < eps) {
                // constant column → undefined corr; set 0
                R(j1, j2) = (Real)0;
                R(j2, j1) = (Real)0;
                continue;
            }

            Real s = 0;
            for (std::size_t i = 0; i < n; ++i) {
                Real x1 = a(i, j1) - mu[j1];
                Real x2 = a(i, j2) - mu[j2];
                s += x1 * x2;
            }

            Real denom = (Real)(n - 1) * sd1 * sd2;
            if (denom < eps) {
                R(j1, j2) = (Real)0;
                R(j2, j1) = (Real)0;
            } else {
                Real r = s / denom;
                R(j1, j2) = r;
                R(j2, j1) = r;
            }
        }
    }

    return matrix2list(R);
}
AtomPtr fn_zscore(AtomPtr node, AtomPtr env) { // Z-score normalization of columns: (x - mean) / std
    Matrix<Real> a = list2matrix(type_check(node->tail.at(0), LIST));

    const std::size_t n = a.rows();
    const std::size_t d = a.cols();

    if (n == 0 || d == 0) {
        return matrix2list(a);
    }

    std::valarray<Real> mu((Real)0, d);
    std::valarray<Real> sigma((Real)0, d);

    for (std::size_t j = 0; j < d; ++j) {
        Real s = 0, s2 = 0;
        for (std::size_t i = 0; i < n; ++i) {
            Real v = a(i, j);
            s  += v;
            s2 += v * v;
        }
        Real mean = s / (Real)n;
        Real var  = (s2 / (Real)n) - mean * mean;
        if (var < 0) var = 0;
        mu[j] = mean;
        sigma[j] = std::sqrt(var);
    }

    const Real eps = (Real)1e-12;
    Matrix<Real> z(n, d);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            Real sd = sigma[j];
            if (sd < eps) {
                z(i, j) = 0; // constant column -> zero
            } else {
                z(i, j) = (a(i, j) - mu[j]) / sd;
            }
        }
    }

    return matrix2list(z);
}

// basic machine learning
// PCA: input is a list-of-rows (matrix), output is a matrix whose last
// column contains eigenvalues and the first columns contain eigenvectors.
AtomPtr fn_pca(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 1) {
        error("[pca] one argument required", node);
    }

    AtomPtr lmatrix = type_check(node->tail.at(0), LIST);
    Matrix<Real> X = list2matrix(lmatrix);  // rows = samples, cols = features

    if (X.rows() == 0 || X.cols() == 0) {
        error("[pca] empty matrix", node);
    }

    const int rows = static_cast<int>(X.rows());
    const int cols = static_cast<int>(X.cols());

    // Flatten X into row-major buffer: rows x cols
    std::vector<Real> data_flat(rows * cols);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            data_flat[i * cols + j] = X(i, j);
        }
    }

    // eig_out is cols x (cols+1): eigenvectors + eigenvalues in last column
    const int eig_cols = cols + 1;
    std::vector<Real> eig_flat(cols * eig_cols, (Real)0);

    // Backend: PCA(const T* data, T* eig_out, int cols, int rows, ...)
    PCA<Real>(data_flat.data(), eig_flat.data(), cols, rows);

    // Repack into Matrix: cols x (cols+1)
    Matrix<Real> eigm(cols, eig_cols);
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < eig_cols; ++j) {
            eigm(i, j) = eig_flat[i * eig_cols + j];
        }
    }

    return matrix2list(eigm);
}
// K-means: input is a list-of-rows matrix and K; output is
//   (list labels centroids)
// labels: valarray of length n (cluster indices as reals)
// centroids: K x m matrix as list-of-rows
AtomPtr fn_kmeans(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 2) {
        error("[kmeans] two arguments required", node);
    }

    AtomPtr lmatrix = type_check(node->tail.at(0), LIST);
    Matrix<Real> X = list2matrix(lmatrix);  // rows = samples, cols = features

    if (X.rows() == 0 || X.cols() == 0) {
        error("[kmeans] empty matrix", node);
    }

    int K = static_cast<int>(
                type_check(node->tail.at(1), ARRAY)->array[0]
            );

    const int n = static_cast<int>(X.rows());
    const int m = static_cast<int>(X.cols());

    if (K <= 0 || K > n) {
        error("[kmeans] invalid K", node);
    }

    // Flatten X into row-major buffer: n x m
    std::vector<Real> data_flat(n * m);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            data_flat[i * m + j] = X(i, j);
        }
    }

    std::vector<int>   labels_vec(n);
    std::vector<Real>  centroids_flat(K * m, (Real)0);

    // Backend: kmeans(const T* data, int n, int m, int k, T tol,
    //                 int* labels, T* centroids)
    kmeans<Real>(data_flat.data(),
                 n,
                 m,
                 K,
                 (Real)1e-5,
                 labels_vec.data(),
                 centroids_flat.data());

    // Repack centroids into Matrix K x m
    Matrix<Real> centroids(K, m);
    for (int i = 0; i < K; ++i) {
        for (int j = 0; j < m; ++j) {
            centroids(i, j) = centroids_flat[i * m + j];
        }
    }

    // Convert labels to valarray<Real> (to be consistent with your API)
    std::valarray<Real> labels_va((Real)0, n);
    for (int i = 0; i < n; ++i) {
        labels_va[i] = static_cast<Real>(labels_vec[i]);
    }

    AtomPtr res = make_atom(); // LIST
    res->tail.push_back(make_atom(labels_va));    // labels
    res->tail.push_back(matrix2list(centroids));  // centroids

    return res;
}
AtomPtr fn_knn(AtomPtr node, AtomPtr env) {
    // 3 args: train-data, k, query-data
    if (node->tail.size() != 3) {
        error("[knn] expects 3 arguments: train-data, k, query-data", node);
    }

    // 1) Training data
    AtomPtr train = type_check(node->tail.at(0), LIST);
    int obs = static_cast<int>(train->tail.size());
    if (obs < 1) {
        error("[knn] insufficient number of observations", node);
    }

    // 2) k
    std::valarray<Real>& karr =
        type_check(node->tail.at(1), ARRAY)->array;
    if (karr.size() < 1) {
        error("[knn] k must be a scalar array", node);
    }
    int K = static_cast<int>(karr[0]);
    if (K < 1 || K > obs) {
        error("[knn] invalid K parameter", node);
    }

    // 3) Query data
    AtomPtr queries = type_check(node->tail.at(2), LIST);

    // Determine feature dimension from first training sample
    AtomPtr first_item = type_check(train->tail.at(0), LIST);
    if (first_item->tail.size() != 2) {
        error("[knn] each training item must be (features label)", node);
    }
    std::valarray<Real>& first_feat =
        type_check(first_item->tail.at(0), ARRAY)->array;
    int features = static_cast<int>(first_feat.size());
    if (features < 1) {
        error("[knn] invalid number of features", node);
    }

    // Build KNN model
    KNN<Real> knn(K, features);

    for (unsigned i = 0; i < train->tail.size(); ++i) {
        AtomPtr item = type_check(train->tail.at(i), LIST);
        if (item->tail.size() != 2) {
            error("[knn] malformed training item; expected (features label)", node);
        }

        Observation<Real>* o = new Observation<Real>();
        o->attributes =
            type_check(item->tail.at(0), ARRAY)->array;

        std::stringstream s;
        print (item->tail.at(1), s);  
        o->classlabel = s.str();

        knn.addObservation(o);
    }

    // Classify queries
    AtomPtr out = make_atom(); // list of labels
    for (unsigned i = 0; i < queries->tail.size(); ++i) {
        AtomPtr q = type_check(queries->tail.at(i), ARRAY);
        Observation<Real> qo;
        qo.attributes = q->array;

        std::string label = knn.classify(qo);
        out->tail.push_back(make_atom(label));
    }

    return out;
}

AtomPtr add_scientific(AtomPtr env) {
    // Display
    add_op("matdisp",  fn_matdisp,  1, env);

    // Basic linear algebra
    add_op("matadd",   fn_matadd,   2, env);
    add_op("matmul",   fn_matmul,   2, env);
    add_op("matsub",   fn_matsub,   2, env);
    add_op("hadamard", fn_hadamard, 2, env);

    add_op("matsum",   fn_matsum,   2, env);
    add_op("nrows",    fn_nrows,    1, env);
    add_op("ncols",    fn_ncols,    1, env);
    add_op("getrows",  fn_matget<0>, 3, env);
    add_op("getcols",  fn_matget<1>, 3, env);
    add_op("transpose", fn_mattran, 1, env);

    // Matrix/array construction / decomposition
    add_op("eye",      fn_eye,      1, env);
    add_op("rand",     fn_rand,     1, env);
    add_op("zeros",    fn_zeros,    1, env);
    add_op("ones",     fn_ones,     1, env);    
    add_op("bpf",      fn_bpf,      3, env);
    add_op("inv",      fn_inv,      1, env);
    add_op("det",      fn_det,      1, env);
    add_op("diag",     fn_diag,     1, env);
    add_op("rank",     fn_rank,     1, env);
    add_op("solve",    fn_solve,    2, env);
    add_op("matcol",   fn_matcol,   2, env);
    add_op("stack2",   fn_stack2,   2, env);
    add_op("hstack",   fn_hstack,   2, env);
    add_op("vstack",   fn_vstack,   2, env);    

    // Statistics / filters
    add_op("median",   fn_median,   2, env);
    add_op("linefit",  fn_linefit,  2, env);
    add_op("norm",     fn_norm,     1, env);
    add_op("dist",     fn_dist,     2, env);    
    add_op("matmean",  fn_matmean,  2, env);
    add_op("matstd",   fn_matstd,   2, env);
    add_op("cov",      fn_cov,      1, env);
    add_op("corr",     fn_corr,     1, env);    
    add_op("zscore",   fn_zscore,   1, env);

    // ML-style tools
    add_op("pca",      fn_pca,      1, env);
    add_op("kmeans",   fn_kmeans,   2, env);
    add_op("knn", fn_knn, 3, env);


    return env;
}

#endif // SCIENTIFIC_H


// eof
