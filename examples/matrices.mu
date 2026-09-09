# matrices: scalars, vectors and matrices in Musil (scientific library)
#
# Scalars and vectors use + - * /, which broadcast. A matrix is a list of
# row vectors, and gets its own functions: mat-add, mat-mul, transpose, ...

load "scientific.mu"

print "--- scalars and vectors ---"
var v1 (vec 1 2 3)
var v2 (vec 10 20 30)
print "v1 + v2  =" (+ v1 v2)
print "v2 - v1  =" (- v2 v1)
print "v1 * 2   =" (* v1 2)
print "v2 / 10  =" (/ v2 10)
print "dot      =" (dot v1 v2)
print "dist     =" (dist v1 v2) "  L1:" (dist-p v1 v2 1)

print ""
print "--- matrices ---"
var A (list (vec 1 2) (vec 3 4))
var B (list (vec 5 6) (vec 7 8))
print "A =" A
print "B =" B
print "shape A =" (mat-shape A)
print "A + B    =" (mat-add A B)
print "B - A    =" (mat-sub B A)
print "A .* B   =" (hadamard A B)
print "A * B    =" (mat-mul A B)
print "A'       =" (transpose A)
print "3 A      =" (mat-scale A 3)
print "A + 10   =" (mat-shift A 10)
print "A^2 elementwise:" (mat-map A (function (x) (* x x)))

print ""
print "--- matrix and vector ---"
var x (vec 10 20)
print "A x      =" (mat-vec A x)
print "x A      =" (vec-mat x A)
print "col 1 of B:" (mat-col B 1)
print "row 0 of B:" (mat-row B 0)

print ""
print "--- constructors ---"
print "eye 3:"
mat-print (eye 3)
print "diag (2 3 4):"
mat-print (diag (vec 2 3 4))
print "col-stack of (1 2 3) and (4 5 6):"
mat-print (col-stack (list (vec 1 2 3) (vec 4 5 6)))
print "hstack A B:" (hstack A B)
print "vstack A B:" (vstack A B)

print ""
print "--- decompositions ---"
print "det A    =" (det A)
print "rank A   =" (rank A) "  rank of (1 2; 2 4) =" (rank (list (vec 1 2) (vec 2 4)))
print "inv A    ="
mat-print-with (inv A) 2
print "A inv(A) ="
mat-print (mat-mul A (inv A))
var b (vec 5 11)
print "solve A x = (5 11):" (solve A b)
print "check A x =" (mat-vec A (solve A b))
print "singular:" (try (inv (list (vec 1 2) (vec 2 4))) catch e "caught, singular")

print ""
print "--- statistics along an axis (0 = down the columns, 1 = along the rows) ---"
var M (list (vec 1 2 3) (vec 4 5 6) (vec 7 8 10))
print "sum  0:" (mat-sum M 0) "  1:" (mat-sum M 1)
print "mean 0:" (mat-mean M 0) "  1:" (mat-mean M 1)
print "std  0:" (fixed (mat-std M 0) 3)
print "zscore:"
mat-print (zscore M)
print "cov:"
mat-print (cov M)
print "corr:"
mat-print (corr M)

print ""
print "--- a least-squares line through noisy points ---"
seed 7
var xs (range 0 10)
var ys (+ (* 2 xs) 1 (- (* 0.4 (rand 10)) 0.2))
var fit (linefit xs ys)
print "slope, intercept =" (fixed fit 2) "(true: 2, 1)"
print "with the normal equations (intercept, slope):" (fixed (linreg-fit (add-intercept (col-stack (list xs))) ys) 2)
