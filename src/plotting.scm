;; plotting.scm
;;
;; Convenience helpers on top of core (plot ...) and (scatter ...).
;;
;; Core primitives (from C++ side) have signatures:
;;
;;   (plot    "title" y1 "legend1" y2 "legend2" ... style)
;;   (scatter "title" x1 y1 "legend1" x2 y2 "legend2" ... style)
;;
;; where style is one of "*", ".", "-".
;;
;; Here we provide simpler variants where you often
;; don't want to specify title / legends / style explicitly.

(load "scientific.scm")  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Global defaults
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(def PLOT_DEFAULT_STYLE   ".")
(def SCATTER_DEFAULT_STYLE ".")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 1. Minimal quick-plot helpers (1D data)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; (qplot y)
;; Plot a single 1D array y, no title, no legend, default style.
(def qplot
  (lambda (y)
    (plot "" y PLOT_DEFAULT_STYLE)))

;; (qplot2 y1 y2)
;; Plot two series with generic legends.
(def qplot2
  (lambda (y1 y2)
    (plot "" y1 "series1"
              y2 "series2"
              PLOT_DEFAULT_STYLE)))

;; (qplot3 y1 y2 y3)
;; Plot three series with generic legends.
(def qplot3
  (lambda (y1 y2 y3)
    (plot "" y1 "series1"
              y2 "series2"
              y3 "series3"
              PLOT_DEFAULT_STYLE)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 2. Minimal quick-scatter helpers (2D data)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; (qscatter x y)
;; Scatter a single (x,y) dataset, no title, default style.
(def qscatter
  (lambda (x y)
    (scatter "" x y SCATTER_DEFAULT_STYLE)))

;; (qscatter2 x1 y1 x2 y2)
;; Scatter two datasets with generic legends.
(def qscatter2
  (lambda (x1 y1 x2 y2)
    (scatter "" x1 y1 "set1"
                 x2 y2 "set2"
                 SCATTER_DEFAULT_STYLE)))

;; (qscatter3 x1 y1 x2 y2 x3 y3)
;; Scatter three datasets with generic legends.
(def qscatter3
  (lambda (x1 y1 x2 y2 x3 y3)
    (scatter "" x1 y1 "set1"
                 x2 y2 "set2"
                 x3 y3 "set3"
                 SCATTER_DEFAULT_STYLE)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 3. Matrix-oriented helpers
;;
;; These assume matrices are in the same format as IRIS_X etc.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; (plot-col M col)
;; Extract column 'col' from matrix M and plot it quickly.
;; 'col' is an array index as used by matcol, e.g. (array 0).
(def plot-col
  (lambda (M col)
    {
      (def c (matcol M col))
      (qplot c)
    }))

;; (plot-cols2 M col1 col2)
;; Plot two columns from a matrix M as two series.
(def plot-cols2
  (lambda (M col1 col2)
    {
      (def c1 (matcol M col1))
      (def c2 (matcol M col2))
      (qplot2 c1 c2)
    }))

;; (scatter-cols M colx coly)
;; Scatter plot of two columns of matrix M as (x,y).
(def scatter-cols
  (lambda (M colx coly)
    {
      (def xs (matcol M colx))
      (def ys (matcol M coly))
      (qscatter xs ys)
    }))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; 4. Convenience wrappers with optional title
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; (qplot-t title y)
(def qplot-t
  (lambda (title y)
    (plot title y PLOT_DEFAULT_STYLE)))

;; (qscatter-t title x y)
(def qscatter-t
  (lambda (title x y)
    (scatter title x y SCATTER_DEFAULT_STYLE)))

;; eof
