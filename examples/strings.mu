# string built-ins

load "std.mu"
var s "hello, musil!"

print "original   :" s
print "length     :" (length s)
print "slice 0 5  :" (slice s 0 5)
print "slice 7 4  :" (slice s 7 4)
print "find ','   :" (find s ",")
print "find 'x'   :" (find s "x")
print "upper      :" (upper s)
print "split      :" (split s ", ")
print "join       :" (join (list "a" "b" "c") "-")
print "format     :" (format "{} has {} letters" s (length s))

# str / num conversion
var n 3.14
print "str(3.14)  :" (str n)
print "num('42')  :" (num "42")
print "arithmetic on num:" (+ (num "10") (num "32"))

# building strings in a loop
var result ""
for (var i 0) (< i 5) (var i (+ i 1)) {
    var result (concat result i " ")
}
print "built:" result

# find + slice: extract a prefix up to a delimiter
function extract (text delim) {
    var pos (find text delim)
    if (< pos 0) { return text }
    return (slice text 0 pos)
}
print "extract('John Doe', ' ') =" (extract "John Doe" " ")
print "extract('no-delim', ':') =" (extract "no-delim" ":")

# repeat is in std.mu
print (repeat "ha" 4)
print (repeat "-" 20)
