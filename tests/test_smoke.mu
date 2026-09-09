# Top-level: command-per-line, no enclosing parens.
# Sub-expressions: (...)
# Blocks: {...}

print "=== smoke test ==="

# Simple commands
var x 10
var y 20
print "x =" x "y =" y

# Arithmetic via expressions (sub-expr parens)
print "x + y =" (+ x y)
print "x * y =" (* x y)

# expr Pratt
var z (expr (x + y * 2))
print "z =" z

# if with brace block
if (< x y) {
    print "x < y, good"
}

# if/else
if (> x 100) {
    print "big"
} {
    print "small"
}

# while
var i 0
var s 0
while (< i 5) {
    var s (+ s i)
    var i (+ i 1)
}
print "sum 0..4 =" s

# Function definition: named, with params
function square (x) (* x x)
print "square 7 =" (square 7)

# Function with brace body
function fact (n) {
    if (< n 2) {
        return 1
    }
    return (* n (fact (- n 1)))
}
print "fact 6 =" (fact 6)

# Lambda
var inc (function (x) (+ x 1))
print "inc 5 =" (inc 5)

# Vectors
var v (vec 1 2 3 4 5)
print "v =" v
print "v + 10 =" (+ v 10)
print "sum v =" (sum v)

# Lists
var L (list 1 "two" 3.0)
print "L =" L
print "head L =" (head L)
print "length L =" (length L)

# Higher-order
var dbl (map (vec 1 2 3) (function (x) (* x 2)))
print "dbl =" dbl

# Strings
print (concat "hello, " "world")
print (upper "musil")
print (split "a,b,c" ",")

print "=== done ==="
