# string built-ins

var s = "hello, musil!"

print "original  : " s
print "len       : " len(s)
print "sub(0,5)  : " sub(s, 0, 5)
print "sub(7,11) : " sub(s, 7, 11)
print "find(',') : " find(s, ",")
print "find('x') : " find(s, "x")

# str / num conversion
var n = 3.14
print "str(3.14) : " str(n)
print "num('42') : " num("42")
print "arithmetic on num: " num("10") + num("32")

# building strings in a loop
var result = ""
var i = 0
while (i < 5) {
    result = result + str(i) + " "
    i = i + 1
}
print "built: " result

# find + sub: extract a substring
proc extract (text, delim) {
    var pos = find(text, delim)
    if (pos < 0) { return text }
    return sub(text, 0, pos)
}
print "extract('John Doe', ' ') = " extract("John Doe", " ")
print "extract('no-delim', ':') = " extract("no-delim", ":")

# repeat a string n times
proc repeat (s, n) {
    var out = ""
    while (n > 0) { out = out + s   n = n - 1 }
    return out
}
print repeat("ha", 4)
print repeat("-", 20)
