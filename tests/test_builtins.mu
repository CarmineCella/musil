# test suite for built-in functions

load("stdlib.mu")

# upper / lower
assert_eq(upper("hello"),       "HELLO",   "upper")
assert_eq(lower("musil v3"),     "musil v3", "lower")
assert_eq(upper(lower("MiXeD")), "MIXED",  "round-trip")

# type
assert_eq(type(42),             "number",  "type number")
assert_eq(type("hi"),           "string",  "type string")
assert_eq(type(sqrt(2)),        "number",  "type sqrt")

# char / asc  (ASCII round-trip)
assert_eq(char(65),             "A",       "char(65)=A")
assert_eq(asc("A"),             65,        "asc(A)=65")
assert_eq(char(asc("z")),       "z",       "char(asc) round-trip")

# char arithmetic: build lowercase from uppercase
proc to_lower_char (c) {
    var code = asc(c)
    if (code >= 65 and code <= 90) { return char(code + 32) }
    return c
}
assert_eq(to_lower_char("G"),   "g",       "to_lower_char G")
assert_eq(to_lower_char("3"),   "3",       "to_lower_char non-alpha")

# clock (just check it returns a non-negative number)
var t0 = clock()
assert(type(t0) == "number",               "clock returns number")
assert(t0 >= 0,                            "clock non-negative")

# string == comparison
assert_eq("musil" == "musil",     1,         "string == true")
assert_eq("musil" == "sun",      0,         "string == false")
assert_eq("a" != "b",           1,         "string != true")

test_summary()
