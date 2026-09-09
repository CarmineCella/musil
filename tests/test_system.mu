# test_system.mu — self-checking test of the system library (system.h + system.mu).
#
# Every check is a (check ...) from test.mu. A passing run prints only the final line.
# Run with: musil tests/test_system.mu   (from the tests directory)

load "test.mu"
load "system.mu"

var dir "/tmp/musil_test_system"
mkdir dir
check (dir? dir) "mkdir / dir?"
check (not (file? dir)) "file?: a directory is not a file"
check (equal? (type (stat "/tmp/musil/definitely/not")) "nil") "stat: missing path is nil"

# --- processes and environment ---
check (equal? (trim (exec "echo ok")) "ok") "exec"
check (equal? (exec "true") "") "exec: no output"
check (equal? (type (getenv "HOME")) "string") "getenv: HOME"
check (equal? (type (getenv "MUSIL_SURELY_UNSET_VARIABLE")) "nil") "getenv: unset is nil"
var t0 (clock)
sleep 0.05
check (>= (- (clock) t0) 0.04) "sleep"
check (> (now) 1600000000) "now: epoch seconds"
check (>= (elapsed (function () (sleep 0.02))) 0.015) "elapsed"

# --- directories and files ---
check (equal? (type (cwd)) "string") "cwd"
var f (concat dir "/a.txt")
write f "12345"
check (equal? (ls dir) (list "a.txt")) "ls"
check (file? f) "file?"
check (== (file-size f) 5) "file-size"
check (equal? (type (file-size "/nope")) "nil") "file-size: missing is nil"
var st (stat f)
check (== (length st) 3) "stat: (size is-dir mtime)"
check (> (getidx st 2) 1600000000) "stat: mtime is epoch seconds"
check (contains? (error-of (function () (ls "/definitely/not/a/dir"))) "cannot list") "ls: missing dir"
check (== (remove f) 1) "remove: removed"
check (== (remove f) 0) "remove: already gone"
check (not (exists? f)) "remove: really gone"

# --- system.mu: lines and paths ---
write f "one\ntwo\nthree\n"
check (equal? (file-lines f) (list "one" "two" "three")) "file-lines"
write f ""
check (equal? (file-lines f) (list)) "file-lines: empty file"
write-lines f (list "a" "b")
check (equal? (read f) "a\nb\n") "write-lines"
check (equal? (basename "/x/y/z.wav") "z.wav") "basename"
check (equal? (dirname "/x/y/z.wav") "/x/y") "dirname"
check (equal? (dirname "z.wav") ".") "dirname: no directory"
check (equal? (extension "/x/y/z.wav") "wav") "extension"
check (equal? (extension "noext") "") "extension: none"

# --- CSV ---
var csv (concat dir "/t.csv")
var table (list (list "name" "value") (list "a" 1) (list "quoted, with \"marks\"" 2.5))
write-csv csv table
var back (read-csv csv)
check (== (length back) 3) "csv: rows"
check (equal? (getidx back 0) (list "name" "value")) "csv: header"
check (equal? (getidx back 1) (list "a" 1)) "csv: numeric cells become numbers"
check (equal? (getidx back 2) (list "quoted, with \"marks\"" 2.5)) "csv: quoting round-trips"
check (equal? (csv-escape "plain") "plain") "csv-escape: plain"
check (equal? (csv-escape "a,b") "\"a,b\"") "csv-escape: comma"
check (equal? (csv-escape "say \"hi\"") "\"say \"\"hi\"\"\"") "csv-escape: quotes doubled"
check (contains? (error-of (function () (read-csv "/nope.csv"))) "cannot open") "csv: missing file"

# --- WAV ---
var wav (concat dir "/t.wav")
var sr 8000
var t (/ (range 800) sr)
var left (* 0.5 (sin (* 2 pi 440 t)))
var right (* 0.25 (sin (* 2 pi 880 t)))
write-wav wav sr (list left right)
var w (read-wav wav)
check (== (head w) sr) "wav: sample rate"
check (== (length (getidx w 1)) 2) "wav: channels"
check (== (length (head (getidx w 1))) 800) "wav: frames"
check (< (max (abs (- (head (getidx w 1)) left))) 0.001) "wav: 16-bit round trip within quantization"
check (== (wav-duration w) 0.1) "wav-duration"
write-wav wav sr left
check (== (length (getidx (read-wav wav) 1)) 1) "wav: a bare vector is mono"
write-wav wav sr left 32
check (< (max (abs (- (head (getidx (read-wav wav) 1)) left))) 1e-6) "wav: 32-bit float round trip"
check (contains? (error-of (function () (write-wav wav sr (list left (vec 1 2))))) "same length") "wav: channel length mismatch"
check (contains? (error-of (function () (write-wav wav 0 left))) "> 0") "wav: bad sample rate"
check (contains? (error-of (function () (read-wav "/nope.wav"))) "cannot open") "wav: missing file"

# --- UDP ---
check (== (udp-send "127.0.0.1" 9 "hello") 1) "udp-send: plain"
check (== (udp-send "127.0.0.1" 9 "/osc/addr" 1) 1) "udp-send: osc"
check (equal? (type (udp-receive "127.0.0.1" 47123 0.05)) "nil") "udp-receive: timeout gives nil"

# --- cleanup ---
remove csv
remove wav
remove f
check (== (remove dir) 1) "remove: empty directory"

report "test_system"
