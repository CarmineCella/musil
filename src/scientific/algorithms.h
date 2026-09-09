// algorithms.h


#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <valarray>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>

// ---------------------------------------------------------
// median<T>(ptr, n)
//   In-place nth_element-based median of an array segment.
// ---------------------------------------------------------
template <typename T>
T median(T* data, int n) {
    if (!data || n <= 0) {
        throw std::invalid_argument("[median] invalid data or size");
    }

    int mid = n / 2;
    // nth_element rearranges such that data[mid] is the element that would
    // be in that position in a sorted array.
    std::nth_element(data, data + mid, data + n);

    if ((n & 1) == 1) {
        // odd length: data[mid] is the median
        return data[mid];
    } else {
        // even length: need neighbor
        T v1 = data[mid];
        std::nth_element(data, data + mid - 1, data + n);
        T v0 = data[mid - 1];
        return (v0 + v1) / static_cast<T>(2);
    }
}

// ---------------------------------------------------------
// LineFit<T>: simple least-squares line y = a x + b
//   fit(x, y): returns false if degenerate (vertical line)
// ---------------------------------------------------------
template <typename T>
class LineFit {
public:
    LineFit() : m_slope(0), m_intercept(0) {}

    bool fit(const std::valarray<T>& x, const std::valarray<T>& y) {
        const std::size_t n = x.size();
        if (n == 0 || y.size() != n) {
            throw std::invalid_argument("[linefit] x and y must have same non-zero size");
        }

        T sumX  = x.sum();
        T sumY  = y.sum();
        T sumXY = (x * y).sum();
        T sumX2 = (x * x).sum();

        T xMean = sumX / static_cast<T>(n);
        T yMean = sumY / static_cast<T>(n);

        T denom = sumX2 - sumX * xMean;
        if (std::fabs(denom) < static_cast<T>(1e-7)) {
            // vertical or nearly-vertical line
            return false;
        }

        m_slope     = (sumXY - sumX * yMean) / denom;
        m_intercept = yMean - m_slope * xMean;
        return true;
    }

    void get_params(T& slope, T& intercept) const {
        slope     = m_slope;
        intercept = m_intercept;
    }

private:
    T m_slope;
    T m_intercept;
};

// ---------------------------------------------------------
// kmeans<T>
//   data:     pointer to n × m (row-major) samples
//   n:        number of samples
//   m:        number of features
//   k:        number of clusters
//   tol:      convergence tolerance on centroid movement
//   labels:   output array of size n (cluster index)
//   centroids:output array of size k × m (row-major)
// ---------------------------------------------------------
template <typename T>
void kmeans(const T* data,
            int n,
            int m,
            int k,
            T tol,
            int* labels,
            T* centroids) {
    if (!data || !labels || !centroids) {
        throw std::invalid_argument("[kmeans] null pointer argument");
    }
    if (n <= 0 || m <= 0 || k <= 0 || k > n) {
        throw std::invalid_argument("[kmeans] invalid n, m, or k");
    }
    if (tol <= static_cast<T>(0)) {
        tol = static_cast<T>(1e-5);
    }

    const int max_iter = 100;

    // Initialize centroids with first k samples
    for (int c = 0; c < k; ++c) {
        const T* src = data + c * m;
        T* dst       = centroids + c * m;
        for (int j = 0; j < m; ++j) {
            dst[j] = src[j];
        }
    }

    std::vector<T> new_centroids(k * m);
    std::vector<int> counts(k);

    auto sqdist = [m](const T* a, const T* b) -> T {
        T acc = 0;
        for (int j = 0; j < m; ++j) {
            T d = a[j] - b[j];
            acc += d * d;
        }
        return acc;
    };

    for (int iter = 0; iter < max_iter; ++iter) {
        // Assignment step
        for (int i = 0; i < n; ++i) {
            const T* x = data + i * m;
            T best_d   = std::numeric_limits<T>::max();
            int best_c = 0;
            for (int c = 0; c < k; ++c) {
                const T* mu = centroids + c * m;
                T d = sqdist(x, mu);
                if (d < best_d) {
                    best_d = d;
                    best_c = c;
                }
            }
            labels[i] = best_c;
        }

        // Update step
        std::fill(new_centroids.begin(), new_centroids.end(), T(0));
        std::fill(counts.begin(), counts.end(), 0);

        for (int i = 0; i < n; ++i) {
            int c = labels[i];
            const T* x = data + i * m;
            T* acc     = new_centroids.data() + c * m;
            for (int j = 0; j < m; ++j) {
                acc[j] += x[j];
            }
            counts[c] += 1;
        }

        // Handle empty clusters by copying a random sample (very simple)
        for (int c = 0; c < k; ++c) {
            if (counts[c] == 0) {
                int idx = c % n;
                const T* x = data + idx * m;
                T* acc     = new_centroids.data() + c * m;
                for (int j = 0; j < m; ++j) {
                    acc[j] = x[j];
                }
                counts[c] = 1;
            }
        }

        // Compute new centroids and track movement
        T max_shift = 0;
        for (int c = 0; c < k; ++c) {
            T* mu_old = centroids + c * m;
            T* mu_new = new_centroids.data() + c * m;
            for (int j = 0; j < m; ++j) {
                mu_new[j] /= static_cast<T>(counts[c]);
            }
            T d = sqdist(mu_old, mu_new);
            max_shift = std::max(max_shift, std::sqrt(d));
        }

        // Copy new centroids in place
        std::copy(new_centroids.begin(), new_centroids.end(), centroids);

        if (max_shift < tol) {
            break;
        }
    }
}

#endif // ALGORITHMS_H

// eof

