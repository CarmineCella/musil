# plot_adv.mu
#
# More "scientific" examples for the plotting functions.

load("stdlib.mu")
load("scientific.mu")
load("plotting.mu")

# Random vector of length n
proc randvec(n) {
    return rand(n)
}

# Random matrix with given rows and cols
# scientific rand is rand(cols, rows)
proc randmat(rows, cols) {
    return rand(cols, rows)
}

# Example A: linear regression with noisy data

var X = vec(0,
            0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5,
            5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5)

var N = len(X)

var Yclean = X * 2 + 1

var noise = randvec(N)
var smallnoise = noise * 0.5
var Ynoisy = Yclean + smallnoise

var LF = linefit(X, Ynoisy)

scatter("Linear regression (data)",
        X, Ynoisy, "noisy samples",
        ".")

plot("Linear regression (true vs noisy)",
     Yclean, "true 2x+1",
     Ynoisy, "noisy samples",
     "*")

# Example B: k-means on clearly separated 2D clusters

var SMALL_DATA = arr(12)
SMALL_DATA[0]  = vec(-4.0, -4.0)
SMALL_DATA[1]  = vec(-3.8, -4.2)
SMALL_DATA[2]  = vec(-4.2, -3.7)
SMALL_DATA[3]  = vec(-3.5, -4.5)
SMALL_DATA[4]  = vec( 0.0,  4.0)
SMALL_DATA[5]  = vec( 0.2,  4.3)
SMALL_DATA[6]  = vec(-0.3,  3.7)
SMALL_DATA[7]  = vec( 0.4,  4.2)
SMALL_DATA[8]  = vec( 4.0,  0.0)
SMALL_DATA[9]  = vec( 4.3,  0.2)
SMALL_DATA[10] = vec( 3.7, -0.3)
SMALL_DATA[11] = vec( 4.1, -0.4)

var KM_SMALL = kmeans(SMALL_DATA, 3)

# KM_SMALL = [labels_vector, centroids_matrix]
var CENTROIDS = KM_SMALL[1]

var Xdata = matcol(SMALL_DATA, 0)
var Ydata = matcol(SMALL_DATA, 1)

var Xcent = matcol(CENTROIDS, 0)
var Ycent = matcol(CENTROIDS, 1)

scatter("k-means: input clusters",
        Xdata, Ydata, "data",
        ".")

scatter("k-means: data + centroids",
        Xdata, Ydata, "data",
        Xcent, Ycent, "centroids",
        ".")

# Example C: PCA – original vs PCA-projected coordinates

N = 300

var t = randvec(N)
noise = randvec(N)
smallnoise = noise * 0.2

var Xcorr = t
var Ycorr = t * 0.5 + smallnoise

var DATA2D = stack2(Xcorr, Ycorr)

var P = pca(DATA2D)

var EV = getcols(P, 0, 1)

var Z = zscore(DATA2D)
var EVt = transpose(EV)
var Yproj = matmul(Z, EVt)

var Xpca = matcol(Yproj, 0)
var Ypca = matcol(Yproj, 1)

scatter("PCA: original correlated data",
        Xcorr, Ycorr, "original",
        ".")

scatter("PCA: projected coordinates",
        Xpca, Ypca, "PCA coords",
        ".")

# Example D: column stats on random matrix

var ROWS = 100
var COLS = 5

var M = randmat(ROWS, COLS)

var mu = matmean(M, 0)
var sig = matstd(M, 0)

plot("Column statistics (random matrix)",
     mu, "mean",
     sig, "std",
     "*")

# Example E: median filtering

var SR = 100

var t2 = vec(
    0.0,
    0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09,
    0.10, 0.11, 0.12, 0.13, 0.14, 0.15, 0.16, 0.17, 0.18, 0.19,
    0.20, 0.21, 0.22, 0.23, 0.24, 0.25, 0.26, 0.27, 0.28, 0.29,
    0.30, 0.31, 0.32, 0.33, 0.34, 0.35, 0.36, 0.37, 0.38, 0.39,
    0.40, 0.41, 0.42, 0.43, 0.44, 0.45, 0.46, 0.47, 0.48, 0.49,
    0.50, 0.51, 0.52, 0.53, 0.54, 0.55, 0.56, 0.57, 0.58, 0.59,
    0.60, 0.61, 0.62, 0.63, 0.64, 0.65, 0.66, 0.67, 0.68, 0.69,
    0.70, 0.71, 0.72, 0.73, 0.74, 0.75, 0.76, 0.77, 0.78, 0.79,
    0.80, 0.81, 0.82, 0.83, 0.84, 0.85, 0.86, 0.87, 0.88, 0.89,
    0.90, 0.91, 0.92, 0.93, 0.94, 0.95, 0.96, 0.97, 0.98, 0.99
)

var x = sin(t2 * TAU * 3) + sin(t2 * TAU * 40) * 0.25
var y = median(x, 10)

plot("Median filtering",
     x, "noisy signal",
     y, "filtered signal",
     "*")
