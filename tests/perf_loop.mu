# performance test: sum of the first 500,000 integers, three ways

load "std.mu"
var t0 (clock)
var i 0
var s 0
while (< i 500000) {
    var s (+ s i)
    var i (+ i 1)
}
print "while loop     :" s "in" (fixed (- (clock) t0) 3) "s"

var t0 (clock)
function go (i s) (if (== i 500000) s (go (+ i 1) (+ s i)))
print "tail recursion :" (go 0 0) "in" (fixed (- (clock) t0) 3) "s"

var t0 (clock)
print "vector sum     :" (sum (range 500000)) "in" (fixed (- (clock) t0) 3) "s"
