print "=== env_and_clock_benchmark.mu ===\n"
print "HOME = " getvar("HOME")
print "SHELL = " getvar("SHELL") "\n"
print "benchmark: sum of first 1e6 integers"

var start = clock()
var i = 0
var sum = 0

print "block 1"
while (i < 1000000) {
    sum = sum + i
    i = i + 1
}

print "block 2"
while (i < 2000000) {
    sum = sum + i
    i = i + 1
}

var stop = clock()

print "result sum = " sum
print "clock ticks elapsed = " (stop - start)
print "Note: clock is CPU time, not wall clock.\n"
