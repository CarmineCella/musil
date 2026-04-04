# plot harmonics

load ("stdlib.mu")
load ("plotting.mu")

for (var i in range (1, 10)) {
	var a = sin (TAU * (linspace (0, 1, 1024) * i))
	plot (a, "-")
}