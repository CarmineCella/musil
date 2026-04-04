# plot_basic.mu
#
# Illustrates basic plotting examples

load("plotting.mu")

# Simple grid of x values (0 .. 6, step ~0.5)
var X = vec(0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0)

var Ysin = sin(X)
var Ycos = cos(X)

plot("Sine and Cosine",
     Ysin, "sin(x)",
     Ycos, "cos(x)",
     "-")

var N = 200
var NOISE = rand(N)

plot("Random noise",
     NOISE, "rand in [-1,1]",
     "-")

# x from 0 to 2π (roughly)
X = vec(0,
        0.2, 0.4, 0.6, 0.8,
        1.0, 1.2, 1.4, 1.6, 1.8,
        2.0, 2.2, 2.4, 2.6, 2.8,
        3.0, 3.2, 3.4, 3.6, 3.8,
        4.0, 4.2, 4.4, 4.6, 4.8,
        5.0, 5.2, 5.4, 5.6, 5.8,
        6.0)

var Yclean = sin(X)

N = len(X)
var noise = rand(N)
var smallnoise = noise * 0.2
var Ynoisy = Yclean + smallnoise

plot("Sine with noise",
     Yclean, "clean sin(x)",
     Ynoisy, "noisy sin(x)",
     "-")

N = 200

# Cluster A around (+1, +1)
var X1 = rand(N) + 1.0
var Y1 = rand(N) + 1.0

# Cluster B around (-1, -1)
var X2 = rand(N) - 1.0
var Y2 = rand(N) - 1.0

scatter("Two random clusters",
        X1, Y1, "cluster A",
        X2, Y2, "cluster B",
        ".")

X = vec(0, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45,
        0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95,
        1.00, 1.05, 1.10, 1.15, 1.20, 1.25, 1.30, 1.35, 1.40, 1.45,
        1.50, 1.55, 1.60, 1.65, 1.70, 1.75, 1.80, 1.85, 1.90, 1.95,
        2.00, 2.05, 2.10, 2.15, 2.20, 2.25, 2.30, 2.35, 2.40, 2.45,
        2.50, 2.55, 2.60, 2.65, 2.70, 2.75, 2.80, 2.85, 2.90, 2.95)

var Ycos2 = cos(X * 2.0)

noise = rand(len(X))
smallnoise = noise * 0.3
Ynoisy = Ycos2 + smallnoise

plot("Cosine vs noisy cosine",
     Ycos2, "cos(2x)",
     Ynoisy, "cos(2x) + noise",
     "-")
