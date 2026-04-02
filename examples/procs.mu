# proc declaration, calls, recursion

proc square (n) {
    return n * n
}

proc cube (n) {
    return n * square(n)
}

proc factorial (n) {
    var result = 1
    while (n > 1) {
        result = result * n
        n = n - 1
    }
    return result
}

# iterative fibonacci (no integer div needed)
proc fib (n) {
    var a = 0
    var b = 1
    while (n > 0) {
        var t = a + b
        a = b
        b = t
        n = n - 1
    }
    return a
}

# gcd via repeated subtraction (avoids float/int division ambiguity)
proc gcd (a, b) {
    while (a != b) {
        while (a > b) { a = a - b }
        while (b > a) { b = b - a }
    }
    return a
}

# power: base^exp (integer exp)
proc pow (base, exp) {
    var result = 1
    while (exp > 0) {
        result = result * base
        exp = exp - 1
    }
    return result
}

print "square(7)      = " square(7)
print "cube(4)        = " cube(4)
print "factorial(12)  = " factorial(12)
print "fib(30)        = " fib(30)
print "gcd(48, 18)    = " gcd(48, 18)
print "gcd(100, 75)   = " gcd(100, 75)
print "pow(2, 10)     = " pow(2, 10)
print "pow(3, 5)      = " pow(3, 5)
