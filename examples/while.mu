# while loops, break, and the vector alternative

load "std.mu"
function sum-range (lo hi) {
    var s 0
    var i lo
    while (<= i hi) {
        var s (+ s i)
        var i (+ i 1)
    }
    return s
}

# count multiples of d in [1..n]
function count-div (n d) {
    var count 0
    var i d
    while (<= i n) {
        var count (+ count 1)
        var i (+ i d)
    }
    return count
}

print "sum 1..100   =" (sum-range 1 100)
print "sum 1..1000  =" (sum-range 1 1000)
print "multiples of 3 in [1,30] =" (count-div 30 3)
print "multiples of 7 in [1,49] =" (count-div 49 7)

print "--- 5x5 multiplication table ---"
var row 1
while (<= row 5) {
    var col 1
    var line ""
    while (<= col 5) {
        var line (concat line (* row col) " ")
        var col (+ col 1)
    }
    print line
    var row (+ row 1)
}

# the same table with vectors: one line per row, no inner loop
print "--- with vectors ---"
each (range 1 6) (function (r) (print (* r (range 1 6))))

# --- break: leaving a loop early ---

# find first multiple of 7 above 50
var i 50
while 1 {
    if (== (mod i 7) 0) {
        print "first multiple of 7 >= 50:" i
        break
    }
    var i (+ i 1)
}

# break from nested structure: only breaks the inner while
function find-pair (target) {
    var x 1
    while (<= x target) {
        var y x
        while (<= y target) {
            if (== (+ x y) target) {
                print "pair summing to" target ":" x "+" y
                break   # exits inner while only
            }
            var y (+ y 1)
        }
        if (== (+ x x) target) { break }   # exit outer when done
        var x (+ x 1)
    }
}
find-pair 10
find-pair 7

# break in while with accumulator: count primes below 1000 by trial division
var count 0
var n 2
while (< n 1000) {
    var is-prime 1
    var d 2
    while (<= (* d d) n) {
        if (== (mod n d) 0) {
            var is-prime 0
            break
        }
        var d (+ d 1)
    }
    if is-prime { var count (+ count 1) }
    var n (+ n 1)
}
print "primes below 1000:" count
