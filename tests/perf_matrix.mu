# perf_matrix.mu — a 300x300 matrix workout: product in C++, elementwise ops in Musil.
load "scientific.mu"
seed 1
var n 300
var a (mat-rand n n)
var b (mat-rand n n)
var t0 (clock)
var p (mat-mul a b)
print "mat-mul   " n "x" n "in" (fixed (- (clock) t0) 3) "s"
var t0 (clock)
hadamard a b
mat-add a b
mat-shift a 3
mat-scale b 10
print "elementwise x4      in" (fixed (- (clock) t0) 3) "s"
var t0 (clock)
var d (det a)
var inv-a (inv a)
print "det + inv           in" (fixed (- (clock) t0) 3) "s"
assert (== (mat-shape p) (list n n)) "shape"
print "done"
