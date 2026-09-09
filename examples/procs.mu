# function declaration, calls, recursion

load "std.mu"
function square (n) (* n n)

function cube (n) (* n (square n))

function factorial (n) {
    var result 1
    while (> n 1) {
        var result (* result n)
        var n (- n 1)
    }
    return result
}

# iterative fibonacci
function fib (n) {
    var a 0
    var b 1
    while (> n 0) {
        var t (+ a b)
        var a b
        var b t
        var n (- n 1)
    }
    return a
}

# the recursive version, tail-recursive so it runs in constant space
function fib-rec (n) {
    function go (k a b) (if (== k 0) a (go (- k 1) b (+ a b)))
    return (go n 0 1)
}

# gcd via repeated subtraction
function gcd (a b) {
    while (!= a b) {
        while (> a b) { var a (- a b) }
        while (> b a) { var b (- b a) }
    }
    return a
}

# power by repeated multiplication (pow is a builtin; this is for the loop)
function power (base e) {
    var result 1
    while (> e 0) {
        var result (* result base)
        var e (- e 1)
    }
    return result
}

print "square(7)      =" (square 7)
print "cube(4)        =" (cube 4)
print "factorial(12)  =" (factorial 12)
print "fib(30)        =" (fib 30)
print "fib-rec(30)    =" (fib-rec 30)
print "gcd(48, 18)    =" (gcd 48 18)
print "gcd(100, 75)   =" (gcd 100 75)
print "power(2, 10)   =" (power 2 10)
print "power(3, 5)    =" (power 3 5)

# functions are values: partial application and higher-order use
function times3 (x) (* x 3)
print "map times3:" (map (range 5) times3)
print "compose:  " ((compose square cube) 2)
