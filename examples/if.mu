# if / else, early returns, nested if

# Musil has no `else if`; a chain of ifs with early returns reads the same way.
function classify (n) {
    if (< n 0)  { return "negative" }
    if (== n 0) { return "zero" }
    if (< n 10) { return "small" }
    return "large"
}

print (classify -5)
print (classify 0)
print (classify 7)
print (classify 100)

# if without else
var x 42
if (> x 40) {
    print "x is big"
}

# if with else: the third argument
if (> x 100) { print "huge" } { print "not huge" }

# nested if
function fizzbuzz (n) {
    for (var i 1) (<= i n) (var i (+ i 1)) {
        var div3 (== (mod i 3) 0)
        var div5 (== (mod i 5) 0)
        if (and div3 div5) { print "FizzBuzz" } {
            if div3 { print "Fizz" } {
                if div5 { print "Buzz" } { print i }
            }
        }
    }
}
fizzbuzz 15
