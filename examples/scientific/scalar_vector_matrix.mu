load("scientific.mu")

print "=== Scalars ===\n"
var s1 = 2
var s2 = 5
print "s1 + s2 = " (s1 + s2)
print "s1 * s2 = " (s1 * s2)
print "s2 / s1 = " (s2 / s1)

print "\n=== Vectors ===\n"
var v1 = vec(1, 2, 3)
var v2 = vec(10, 20, 30)
print "v1       = " v1
print "v2       = " v2
print "v1 + v2  = " (v1 + v2)
print "v2 - v1  = " (v2 - v1)
print "v1 * 2   = " (v1 * 2)
print "2 * v1   = " (2 * v1)
print "v2 / 10  = " (v2 / 10)

print "\n=== Matrices ===\n"
var A = [
    vec(1, 2),
    vec(3, 4)
]

var B = [
    vec(5, 6),
    vec(7, 8)
]

print "A = " A
print "B = " B

print "\nmatadd(A, B) = "
matdisp(matadd(A, B))

print "\nmatsub(B, A) = "
matdisp(matsub(B, A))

print "\nhadamard(A, B) = "
matdisp(hadamard(A, B))

print "\nmatmul(A, B) = "
matdisp(matmul(A, B))

print "\ntranspose(A) = "
matdisp(transpose(A))

print "\nmatscale(A, 3) = "
matdisp(matscale(A, 3))

print "\nmatshift(A, 10) = "
matdisp(matshift(A, 10))

print "\n=== Matrix / Vector interaction ===\n"
var x = vec(10, 20)

print "x = " x
print "matvec(A, x) = " matvec(A, x)
print "vecmat(x, A) = " vecmat(x, A)

print "\n=== Matrix constructors and helpers ===\n"
print "eye(3) = "
matdisp(eye(3))

print "\ndiag(vec(2, 3, 4)) = "
matdisp(diag(vec(2, 3, 4)))

print "\nmatcol(B, 1) = " matcol(B, 1)

print "\nstack2(vec(1,2,3), vec(4,5,6)) = "
matdisp(stack2(vec(1, 2, 3), vec(4, 5, 6)))

print "\n=== Solving A x = b ===\n"
var b = vec(5, 11)
print "b = " b
print "solve(A, b) = " solve(A, b)

print "\n=== Important note ===\n"
print "Use + - * / for scalars and vectors."
print "Use matadd, matsub, matmul, hadamard, matscale, matshift for matrices."
