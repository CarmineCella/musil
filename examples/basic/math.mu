# math built-ins

var pi = 3.14159265358979

print "floor(3.7)  = " floor(3.7)
print "ceil(3.2)   = " ceil(3.2)
print "abs(-7.5)   = " abs(-7.5)
print "sqrt(144)   = " sqrt(144)
print "pow(2, 10)  = " pow(2, 10)
print "log(1)      = " log(1)
print "sin(0)      = " sin(0)
print "cos(0)      = " cos(0)
print "sin(pi/2)   = " sin(pi / 2)

# floor enables integer mod
proc mod (a, b) {
    return a - floor(a / b) * b
}
print "17 mod 5 = " mod(17, 5)
print "100 mod 7 = " mod(100, 7)

# integer square root via Newton
proc isqrt (n) {
    var x = n
    var y = floor((x + 1) / 2)
    while (y < x) {
        x = y
        y = floor((x + n / x) / 2)
    }
    return x
}
print "isqrt(144) = " isqrt(144)
print "isqrt(100) = " isqrt(100)
print "isqrt(50)  = " isqrt(50)

# random numbers (seeded with 42 for reproducibility)
print "--- 5 random numbers in [0,1) ---"
var i = 0
while (i < 5) {
    print rand(1)
    i = i + 1
}
