# while loops and related examples

proc sum_range (lo, hi) {
    var s = 0
    var i = lo
    while (i <= hi) { s = s + i   i = i + 1 }
    return s
}

# count multiples of d in [1..n]
proc count_div (n, d) {
    var count = 0
    var i = d
    while (i <= n) { count = count + 1   i = i + d }
    return count
}

print "sum 1..100   = " sum_range(1, 100)
print "sum 1..1000  = " sum_range(1, 1000)
print "multiples of 3 in [1,30] = " count_div(30, 3)
print "multiples of 7 in [1,49] = " count_div(49, 7)

print "--- 5x5 multiplication table ---"
var row = 1
while (row <= 5) {
    var col = 1
    var line = ""
    while (col <= 5) { line = line + (row * col) + " "   col = col + 1 }
    print line
    row = row + 1
}

proc hello (name) {
    print "hello " name " from while.mu"
}
