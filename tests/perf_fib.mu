# performance test: naive Fibonacci

load "std.mu"
function fib (n) {
    if (<= n 1) { return n }
    return (+ (fib (- n 1)) (fib (- n 2)))
}
var t0 (clock)
print "fib(20) =" (fib 20)
print "fib(25) =" (fib 25) "in" (fixed (- (clock) t0) 3) "s"

print "fib 10..24:" (map (range 10 25) fib)
