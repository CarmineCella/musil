print "matrix stress test..."

var a = rand (1000, 1000)
var b = rand (1000, 1000)

matmul (a, b)
hadamard (a, b)

matadd (a, b)

matshift (a, 3)
matscale (b, 10)

print "done"
