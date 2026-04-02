# procs and vars from while.mu should still be in scope

hello ("multi-file world")
print "sum_range(1,10) = " sum_range(1, 10)


proc test_after_return (x) {
    print "number is " x
}

print test_after_return (40)