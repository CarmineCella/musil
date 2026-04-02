# performance test: sum of first 500,000 integers

var i = 0
var s = 0
while (i < 500000) {
    s = s + i
    i = i + 1
}
print "sum(0..499999) = " s
