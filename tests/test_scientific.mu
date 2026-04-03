load("stdlib.mu")
load("scientific.mu")

var EPS = 0.00000001

# ---- basic matrix arithmetic -----------------------------------------

var A = arr(2)
A[0] = vec(1, 2)
A[1] = vec(3, 4)

var B = arr(2)
B[0] = vec(5, 6)
B[1] = vec(7, 8)

var Cadd = matadd(A, B)
assert_eq(Cadd[0][0], 6, "matadd [0][0]")
assert_eq(Cadd[0][1], 8, "matadd [0][1]")
assert_eq(Cadd[1][0], 10, "matadd [1][0]")
assert_eq(Cadd[1][1], 12, "matadd [1][1]")

var Csub = matsub(B, A)
assert_eq(Csub[0][0], 4, "matsub [0][0]")
assert_eq(Csub[0][1], 4, "matsub [0][1]")
assert_eq(Csub[1][0], 4, "matsub [1][0]")
assert_eq(Csub[1][1], 4, "matsub [1][1]")

var Chad = hadamard(A, B)
assert_eq(Chad[0][0], 5,  "hadamard [0][0]")
assert_eq(Chad[0][1], 12, "hadamard [0][1]")
assert_eq(Chad[1][0], 21, "hadamard [1][0]")
assert_eq(Chad[1][1], 32, "hadamard [1][1]")

var Cmul = matmul(A, B)
assert_eq(Cmul[0][0], 19, "matmul [0][0]")
assert_eq(Cmul[0][1], 22, "matmul [0][1]")
assert_eq(Cmul[1][0], 43, "matmul [1][0]")
assert_eq(Cmul[1][1], 50, "matmul [1][1]")

# ---- transpose / shape / slices --------------------------------------

var At = transpose(A)
assert_eq(nrows(At), 2, "transpose nrows")
assert_eq(ncols(At), 2, "transpose ncols")
assert_eq(At[0][0], 1, "transpose [0][0]")
assert_eq(At[0][1], 3, "transpose [0][1]")
assert_eq(At[1][0], 2, "transpose [1][0]")
assert_eq(At[1][1], 4, "transpose [1][1]")

var rs = getrows(B, 0, 0)
assert_eq(nrows(rs), 1, "getrows nrows")
assert_eq(rs[0][0], 5, "getrows value")

var cs = getcols(B, 1, 1)
assert_eq(ncols(cs), 1, "getcols ncols")
assert_eq(cs[0][0], 6, "getcols [0][0]")
assert_eq(cs[1][0], 8, "getcols [1][0]")

# ---- constructors -----------------------------------------------------

var I2 = eye(2)
assert_eq(I2[0][0], 1, "eye [0][0]")
assert_eq(I2[0][1], 0, "eye [0][1]")
assert_eq(I2[1][0], 0, "eye [1][0]")
assert_eq(I2[1][1], 1, "eye [1][1]")

var Z = zeros(3, 2)
assert_eq(nrows(Z), 2, "zeros nrows")
assert_eq(ncols(Z), 3, "zeros ncols")
assert_eq(Z[1][2], 0, "zeros value")

var O = ones(2, 3)
assert_eq(nrows(O), 3, "ones nrows")
assert_eq(ncols(O), 2, "ones ncols")
assert_eq(O[2][1], 1, "ones value")

# ---- diag / det / inv ------------------------------------------------

var dv = vec(2, 3, 4)
var D = diag(dv)
assert_eq(D[0][0], 2, "diag vec->mat [0][0]")
assert_eq(D[1][1], 3, "diag vec->mat [1][1]")
assert_eq(D[2][2], 4, "diag vec->mat [2][2]")

var dd = diag(A)
assert_eq(dd[0], 1, "diag mat->vec [0]")
assert_eq(dd[1], 4, "diag mat->vec [1]")

assert_near(det(A), -2, EPS, "det")

var Ainv = inv(A)
assert_near(Ainv[0][0], -2,   EPS, "inv [0][0]")
assert_near(Ainv[0][1], 1,    EPS, "inv [0][1]")
assert_near(Ainv[1][0], 1.5,  EPS, "inv [1][0]")
assert_near(Ainv[1][1], -0.5, EPS, "inv [1][1]")

# ---- solve ------------------------------------------------------------

var b = vec(5, 11)
var x = solve(A, b)
assert_near(x[0], 1, EPS, "solve [0]")
assert_near(x[1], 2, EPS, "solve [1]")

# ---- columns / stacking ----------------------------------------------

var c0 = matcol(B, 0)
var c1 = matcol(B, 1)
assert_eq(c0[0], 5, "matcol 0 [0]")
assert_eq(c0[1], 7, "matcol 0 [1]")
assert_eq(c1[0], 6, "matcol 1 [0]")
assert_eq(c1[1], 8, "matcol 1 [1]")

var S2 = stack2(vec(1, 2, 3), vec(4, 5, 6))
assert_eq(nrows(S2), 3, "stack2 nrows")
assert_eq(ncols(S2), 2, "stack2 ncols")
assert_eq(S2[2][0], 3, "stack2 [2][0]")
assert_eq(S2[2][1], 6, "stack2 [2][1]")

var HS = hstack(A, B)
assert_eq(nrows(HS), 2, "hstack nrows")
assert_eq(ncols(HS), 4, "hstack ncols")
assert_eq(HS[0][2], 5, "hstack value")

var VS = vstack(A, B)
assert_eq(nrows(VS), 4, "vstack nrows")
assert_eq(ncols(VS), 2, "vstack ncols")
assert_eq(VS[2][0], 5, "vstack value")

# ---- vector / matrix interaction -----------------------

var vx = vec(10, 20)
var y = matvec(A, vx)
assert_eq(len(y), 2, "matvec len")
assert_eq(y[0], 50, "matvec [0]")
assert_eq(y[1], 110, "matvec [1]")

var vr = vec(1, 2)
var z = vecmat(vr, A)
assert_eq(len(z), 2, "vecmat len")
assert_eq(z[0], 7, "vecmat [0]")
assert_eq(z[1], 10, "vecmat [1]")

var As = matscale(A, 2)
assert_eq(As[0][0], 2, "matscale [0][0]")
assert_eq(As[0][1], 4, "matscale [0][1]")
assert_eq(As[1][0], 6, "matscale [1][0]")
assert_eq(As[1][1], 8, "matscale [1][1]")

var Ash = matshift(A, 10)
assert_eq(Ash[0][0], 11, "matshift [0][0]")
assert_eq(Ash[0][1], 12, "matshift [0][1]")
assert_eq(Ash[1][0], 13, "matshift [1][0]")
assert_eq(Ash[1][1], 14, "matshift [1][1]")

# ---- statistics -------------------------------------------------------

var M = arr(3)
M[0] = vec(1, 2)
M[1] = vec(3, 4)
M[2] = vec(5, 6)

var mu = matmean(M, 0)
assert_near(mu[0], 3, EPS, "matmean [0]")
assert_near(mu[1], 4, EPS, "matmean [1]")

var sigma = matstd(M, 0)
assert(sigma[0] > 0, "matstd positive [0]")
assert(sigma[1] > 0, "matstd positive [1]")

var COV = cov(M)
assert_eq(nrows(COV), 2, "cov nrows")
assert_eq(ncols(COV), 2, "cov ncols")

var COR = corr(M)
assert_eq(nrows(COR), 2, "corr nrows")
assert_eq(ncols(COR), 2, "corr ncols")
assert_near(COR[0][0], 1, EPS, "corr [0][0]")
assert_near(COR[1][1], 1, EPS, "corr [1][1]")

var ZM = zscore(M)
assert_eq(nrows(ZM), 3, "zscore nrows")
assert_eq(ncols(ZM), 2, "zscore ncols")

# ---- scalar/vector arithmetic sanity ---------------------------------

var s = 2
var v = vec(1, 2, 3)
var vv = v * s
assert_eq(vv[0], 2, "vector * scalar [0]")
assert_eq(vv[1], 4, "vector * scalar [1]")
assert_eq(vv[2], 6, "vector * scalar [2]")

var ww = s + v
assert_eq(ww[0], 3, "scalar + vector [0]")
assert_eq(ww[1], 4, "scalar + vector [1]")
assert_eq(ww[2], 5, "scalar + vector [2]")

test_summary()
