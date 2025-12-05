;; plot_adv.scm
;;
;; More "scientific" examples for the plotting functions.
;; Assumes:
;;   - stdlib.scm is available
;;   - scientific primitives (rand, matmean, matstd, linefit, kmeans, pca, ...)
;;   - plotting primitives (plot, scatter)

(load "stdlib.scm")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Local helpers (small wrappers)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Random vector of length n
(def randvec
  (lambda (n)
    (rand n)))

;; Random matrix with given rows and cols
;; NOTE: C++ fn_rand is (len rows) with len = number of columns
(def randmat
  (lambda (rows cols)
    (rand cols rows)))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Example A: linear regression with noisy data
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; x grid (0, 0.5, 1.0, ... 9.5)
(def X (array 0
              0.5 1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5
              5.0 5.5 6.0 6.5 7.0 7.5 8.0 8.5 9.0 9.5))

(def N (size X))

;; Clean line: y = 2x + 1
(def Yclean (+ (* X 2) 1))

;; Add small noise in [-0.5, 0.5]
(def noise      (randvec N))
(def smallnoise (* noise 0.5))
(def Ynoisy     (+ Yclean smallnoise))

;; Fit line on noisy data (just to exercise the primitive)
(def LF (linefit X Ynoisy))
;; LF is an ARRAY [slope intercept], but since lindex is LIST-only
;; we won't try to index it here. The example is still valid as a
;; usage demonstration.

;; Scatter plot of noisy data
(scatter "Linear regression (data)"
         X Ynoisy "noisy samples"
         ".")

;; Plot true line and noisy samples together
(plot "Linear regression (true vs noisy)"
      Yclean "true 2x+1"
      Ynoisy "noisy samples"
      "*")


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Example B: k-means on clearly separated 2D clusters
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Three clusters, well separated in the plane
;; Cluster 1 around (-4, -4)
;; Cluster 2 around (0,  4)
;; Cluster 3 around (4,  0)

(def SMALL_DATA
  (list
    ;; cluster 1
    (array -4.0 -4.0)
    (array -3.8 -4.2)
    (array -4.2 -3.7)
    (array -3.5 -4.5)
    ;; cluster 2
    (array  0.0  4.0)
    (array  0.2  4.3)
    (array -0.3  3.7)
    (array  0.4  4.2)
    ;; cluster 3
    (array  4.0  0.0)
    (array  4.3  0.2)
    (array  3.7 -0.3)
    (array  4.1 -0.4)))

;; Run k-means with K = 3 on this small dataset
(def KM_SMALL (kmeans SMALL_DATA (array 3)))

;; Extract centroid matrix (3 x 2) from result:
;; KM_SMALL = (list labels centroids)
(def CENTROIDS (lindex KM_SMALL (array 1)))

;; Extract data coordinates from SMALL_DATA
(def Xdata (matcol SMALL_DATA (array 0)))
(def Ydata (matcol SMALL_DATA (array 1)))

;; Extract centroid coordinates
(def Xcent (matcol CENTROIDS (array 0)))
(def Ycent (matcol CENTROIDS (array 1)))

;; Figure 1: only data points (dots)
(scatter "k-means: input clusters"
         Xdata Ydata "data"
         ".")

;; Figure 2: data + centroids
;; (centroids will be a different color)
(scatter "k-means: data + centroids"
         Xdata  Ydata  "data"
         Xcent  Ycent  "centroids"
         ".")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Example C: PCA – original vs PCA-projected coordinates
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Step 1: correlated 2D data
(def N [300])

;; latent t in [-1, 1]
(def t (randvec N))

;; noise in [-1,1], scaled
(def noise      (randvec N))
(def smallnoise (* noise 0.2))

(def Xcorr t)
(def Ycorr (+ (* t 0.5) smallnoise))

;; Step 2: build N x 2 data matrix
(def DATA2D (stack2 Xcorr Ycorr))

;; Step 3: PCA on 2D data
(def P (pca DATA2D))
;; P is 2 x 3: [v11 v12 λ1
;;             v21 v22 λ2]

;; Extract eigenvector matrix (2 x 2) = first two columns
(def EV (getcols P (array 0) (array 1)))

;; Step 4: standardize data and project into PCA coords
(def Z (zscore DATA2D))                 ;; N x 2
(def EVt (transpose EV))                ;; 2 x 2
(def Yproj (matmul Z EVt))              ;; N x 2

;; Extract columns of original data (just reuse Xcorr/Ycorr)
;; and projected data:
(def Xpca (matcol Yproj (array 0)))
(def Ypca (matcol Yproj (array 1)))

;; Step 5a: original correlated cloud
(scatter "PCA: original correlated data"
         Xcorr Ycorr "original"
         ".")

;; Step 5b: PCA-transformed cloud (should look roughly circular)
(scatter "PCA: projected coordinates"
         Xpca Ypca "PCA coords"
         ".")


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Example D: column stats on random matrix
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def ROWS [100])
(def COLS [5])

(def M (randmat ROWS COLS))

;; Column-wise mean and std using primitive matmean/matstd
(def mu  (matmean M (array 0)))   ;; ARRAY length COLS
(def sig (matstd  M (array 0)))   ;; ARRAY length COLS

;; Plot the column means and std deviations as two series
(plot "Column statistics (random matrix)"
      mu  "mean"
      sig "std"
      "*")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Example E: median filtering
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def SR [100])

;; time axis t in seconds: 0, 0.01, ..., 0.99  (100 samples)
(def t
  (array
    0.0
    0.01 0.02 0.03 0.04 0.05 0.06 0.07 0.08 0.09
    0.10 0.11 0.12 0.13 0.14 0.15 0.16 0.17 0.18 0.19
    0.20 0.21 0.22 0.23 0.24 0.25 0.26 0.27 0.28 0.29
    0.30 0.31 0.32 0.33 0.34 0.35 0.36 0.37 0.38 0.39
    0.40 0.41 0.42 0.43 0.44 0.45 0.46 0.47 0.48 0.49
    0.50 0.51 0.52 0.53 0.54 0.55 0.56 0.57 0.58 0.59
    0.60 0.61 0.62 0.63 0.64 0.65 0.66 0.67 0.68 0.69
    0.70 0.71 0.72 0.73 0.74 0.75 0.76 0.77 0.78 0.79
    0.80 0.81 0.82 0.83 0.84 0.85 0.86 0.87 0.88 0.89
    0.90 0.91 0.92 0.93 0.94 0.95 0.96 0.97 0.98 0.99))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Noisy signal and median filtering
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; x(t) = sin(2π·3·t) + 0.25·sin(2π·40·t)
(def x (+ (sin (* t TWOPI 3))
          (* (sin (* t TWOPI 40)) 0.25)))

;; Median filter with window order 10
(def y (median x (array 10)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Plot noisy vs filtered signal
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(plot "Median filtering"
      x "noisy signal"
      y "filtered signal"
      "*")

;; eof


