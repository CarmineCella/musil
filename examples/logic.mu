# and / or / not

# string concatenation first: confirming "hal" + "lo" = "hallo"
var a = "hal"
var b = "lo"
var c = a + b
print "string +: " c

# logic operators
print "--- logic ---"
print "1 and 1 = " (1 and 1)
print "1 and 0 = " (1 and 0)
print "0 or  1 = " (0 or 1)
print "0 or  0 = " (0 or 0)
print "not 1   = " (not 1)
print "not 0   = " (not 0)

# compound condition in while
var i = 0
var s = 0
while (i >= 0 and i < 10) {
    s = s + i
    i = i + 1
}
print "sum with compound while = " s

# not in if
var flag = 0
if (not flag) {
    print "flag is false"
}

# string truthiness: non-empty = true
var name = "musil"
if (name) {
    print "non-empty string is truthy"
}
var empty = ""
if (not empty) {
    print "empty string is falsy"
}

# de morgan
var p = 1
var q = 0
print "not(p and q) = " (not (p and q))
print "not p or not q = " ((not p) or (not q))
