proc loop(n) {
    while (n > 0) {
        n = n - 1
    }
}

print "starting test..."
var tic = clock()
loop(10000000)
var toc = clock()
print "elapsed time: " (toc - tic) "\n"
