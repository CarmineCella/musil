# system.mu — operating-system access, Musil half.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: (load "system.mu"). Needs std.mu.
load "std.mu"

# (file-lines path)          list of lines, without the trailing newline
function file-lines (path) {
    var text (read path)
    if (== text "") { return (list) }
    var lines (split text "\n")
    if (== (last lines) "") { pop lines }
    return lines
}
# (write-lines path lines)   write a list of lines, one per line
function write-lines (path lines) (write path (concat (join lines "\n") "\n"))
# (file? path) (dir? path)   existence tests by kind
function file? (path) {
    var s (stat path)
    if (== (type s) "nil") { return false }
    return (not (getidx s 1))
}
function dir? (path) {
    var s (stat path)
    if (== (type s) "nil") { return false }
    return (getidx s 1)
}
# (file-size path)           size in bytes, or nil
function file-size (path) {
    var s (stat path)
    if (== (type s) "nil") { return nil }
    return (getidx s 0)
}
# (basename path) (dirname path) (extension path)   string surgery on paths
function basename (path) (last (split path "/"))
function dirname (path) {
    var parts (split path "/")
    if (< (length parts) 2) { return "." }
    pop parts
    return (join parts "/")
}
function extension (path) {
    var name (basename path)
    var p (find name ".")
    if (< p 0) { return "" }
    return (slice name (+ p 1))
}
# (csv-escape s)             quote a field if it holds a comma, a quote or a newline
function csv-escape (s) {
    var t (str s)
    if (not (or (contains? t ",") (contains? t "\"") (contains? t "\n"))) { return t }
    return (concat "\"" (replace t "\"" "\"\"") "\"")
}
# (write-csv path table)     table is a list of rows, a row a list of cells
function write-csv (path table) {
    var lines (map table (function (row) (join (map row csv-escape) ",")))
    write path (concat (join lines "\n") "\n")
}
# (elapsed thunk)            seconds taken by calling thunk
function elapsed (thunk) {
    var t0 (clock)
    thunk
    return (- (clock) t0)
}
# (wav-duration w)           seconds of a value returned by read-wav
function wav-duration (w) (/ (length (head (getidx w 1))) (head w))
