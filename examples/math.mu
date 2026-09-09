# math built-ins

load "std.mu"
print "floor(3.7)  =" (floor 3.7)
print "ceil(3.2)   =" (ceil 3.2)
print "abs(-7.5)   =" (abs -7.5)
print "sqrt(144)   =" (sqrt 144)
print "pow(2, 10)  =" (pow 2 10)
print "log(1)      =" (log 1)
print "sin(0)      =" (sin 0)
print "cos(0)      =" (cos 0)
print "sin(pi/2)   =" (sin (/ pi 2))
print "17 mod 5    =" (mod 17 5)
print "100 mod 7   =" (mod 100 7)

# every math function is elementwise on vectors
var v (vec 1 4 9 16)
print "sqrt of" v "=" (sqrt v)
print "expr on vectors:" (expr (v * 2 + 1))

# integer square root via Newton
function isqrt (n) {
    var x n
    var y (floor (/ (+ x 1) 2))
    while (< y x) {
        var x y
        var y (floor (/ (+ x (/ n x)) 2))
    }
    return x
}
print "isqrt(144) =" (isqrt 144)
print "isqrt(100) =" (isqrt 100)
print "isqrt(50)  =" (isqrt 50)

# random numbers (seeded for reproducibility)
seed 42
print "--- 5 random numbers in [0,1) ---"
print (rand 5)
print "rounded to 3 places:" (fixed (rand 5) 3)
