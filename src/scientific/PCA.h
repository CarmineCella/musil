// PCA.h

#ifndef PCA_H
#define PCA_H

#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// Simple PCA via covariance + Jacobi eigen-decomposition.
//
// API matches your usage in scientific.h:
//
//   Matrix<Real> eigm(a.cols(), a.cols() + 1, 0); // rows = dims, cols+1 (eigs)
//   PCA<Real>(a.data(), eigm.data(), a.cols(), a.rows());
//
// where:
//   - data: pointer to row-major matrix with `rows` rows, `cols` columns
//   - eig_out: pointer to row-major (cols × (cols+1)) buffer:
//       row i: [eigenvector_i (size cols), eigenvalue_i]
//
// NOTE: This is not meant to be a production-grade PCA, but is fine for
//       moderate dimensionalities and your scientific library.

template <typename T>
void PCA(const T* data,
         T* eig_out,
         int cols,
         int rows,
         int max_iter = 50,
         T tol = static_cast<T>(1e-9)) {
    if (!data || !eig_out) {
        throw std::invalid_argument("[PCA] null pointer argument");
    }
    if (cols <= 0 || rows <= 0) {
        throw std::invalid_argument("[PCA] invalid rows/cols");
    }

    // 1) Compute column means
    std::vector<T> mean(cols, T(0));
    for (int i = 0; i < rows; ++i) {
        const T* row = data + i * cols;
        for (int j = 0; j < cols; ++j) {
            mean[j] += row[j];
        }
    }
    T invN = static_cast<T>(1) / static_cast<T>(rows);
    for (int j = 0; j < cols; ++j) {
        mean[j] *= invN;
    }

    // 2) Compute covariance matrix (cols × cols, row-major)
    std::vector<T> cov(cols * cols, T(0));

    for (int i = 0; i < rows; ++i) {
        const T* row = data + i * cols;
        // centered row
        std::vector<T> xc(cols);
        for (int j = 0; j < cols; ++j) {
            xc[j] = row[j] - mean[j];
        }
        for (int j = 0; j < cols; ++j) {
            for (int k = j; k < cols; ++k) {
                cov[j * cols + k] += xc[j] * xc[k];
            }
        }
    }

    T denom = static_cast<T>(rows - 1);
    if (denom <= static_cast<T>(0)) {
        denom = static_cast<T>(1);
    }

    for (int j = 0; j < cols; ++j) {
        for (int k = j; k < cols; ++k) {
            T v = cov[j * cols + k] / denom;
            cov[j * cols + k] = v;
            cov[k * cols + j] = v; // symmetry
        }
    }

    // 3) Jacobi eigen-decomposition for symmetric cov
    std::vector<T> eigvec(cols * cols, T(0));
    std::vector<T> eigval(cols, T(0));

    // Initialize eigvec as identity
    for (int i = 0; i < cols; ++i) {
        eigvec[i * cols + i] = static_cast<T>(1);
    }

    auto max_offdiag = [&](int& p, int& q) -> T {
        T max_val = static_cast<T>(0);
        p = 0;
        q = 1;
        for (int i = 0; i < cols; ++i) {
            for (int j = i + 1; j < cols; ++j) {
                T v = std::fabs(cov[i * cols + j]);
                if (v > max_val) {
                    max_val = v;
                    p = i;
                    q = j;
                }
            }
        }
        return max_val;
    };

    for (int iter = 0; iter < max_iter; ++iter) {
        int p = 0, q = 0;
        T off = max_offdiag(p, q);
        if (off < tol) {
            break;
        }

        T app = cov[p * cols + p];
        T aqq = cov[q * cols + q];
        T apq = cov[p * cols + q];

        // Jacobi rotation
        T tau = (aqq - app) / (2 * apq);
        T t   = ((tau >= 0) ? 1 : -1) /
                (std::fabs(tau) + std::sqrt(static_cast<T>(1) + tau * tau));
        T c   = static_cast<T>(1) / std::sqrt(static_cast<T>(1) + t * t);
        T s   = t * c;

        // Update covariance matrix
        for (int k = 0; k < cols; ++k) {
            if (k == p || k == q) continue;

            T aik = cov[p * cols + k];
            T ajk = cov[q * cols + k];
            cov[p * cols + k] = c * aik - s * ajk;
            cov[k * cols + p] = cov[p * cols + k];
            cov[q * cols + k] = s * aik + c * ajk;
            cov[k * cols + q] = cov[q * cols + k];
        }

        T app_new = c * c * app - 2 * s * c * apq + s * s * aqq;
        T aqq_new = s * s * app + 2 * s * c * apq + c * c * aqq;
        cov[p * cols + p] = app_new;
        cov[q * cols + q] = aqq_new;
        cov[p * cols + q] = cov[q * cols + p] = static_cast<T>(0);

        // Update eigenvectors
        for (int k = 0; k < cols; ++k) {
            T vip = eigvec[k * cols + p];
            T viq = eigvec[k * cols + q];
            eigvec[k * cols + p] = c * vip - s * viq;
            eigvec[k * cols + q] = s * vip + c * viq;
        }
    }

    // Extract eigenvalues from diagonal of cov
    for (int i = 0; i < cols; ++i) {
        eigval[i] = cov[i * cols + i];
    }

    // Sort components by descending eigenvalue
    std::vector<int> idx(cols);
    for (int i = 0; i < cols; ++i) idx[i] = i;

    std::sort(idx.begin(), idx.end(),
    [&](int a, int b) {
        return eigval[a] > eigval[b];
    });

    // Fill eig_out as rows:
    // row i: [ eigenvector_i (size cols), eigenvalue_i ]
    // where i is in sorted order.
    for (int r = 0; r < cols; ++r) {
        int comp = idx[r];
        // eigenvector: component comp
        for (int c = 0; c < cols; ++c) {
            // eigvec is stored as [row, col] -> row * cols + col;
            // columns of eigvec are eigenvectors of cov in this convention.
            // We want the "comp"-th eigenvector as row r.
            eig_out[r * (cols + 1) + c] = eigvec[c * cols + comp];
        }
        // eigenvalue
        eig_out[r * (cols + 1) + cols] = eigval[comp];
    }
}

#endif // PCA_H

// EOF

