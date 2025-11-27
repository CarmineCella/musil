;; env_and_clock_benchmark.scm
;;
;; Uses clock() to time a numeric operation,
;; and getvar to show environment info.

(load "stdlib.scm")

(print "=== env_and_clock_benchmark.scm ===\n\n")

(print "HOME = " (getvar "HOME") "\n")
(print "SHELL = " (getvar "SHELL") "\n\n")

(print "benchmark: sum of first 1e6 integers\n")

(def start (clock))

(def i   0)
(def sum 0)

(print "block 1\n")
(while (< i 1000000) {
  (= sum (+ sum i))
  (= i (+ i 1))
})
(print "block 2\n")
(while (< i 1000000) {
  (= sum (+ sum i))
  (= i (+ i 1))
})

(def stop (clock))

(print "result sum = " sum "\n")
(print "clock ticks elapsed = " (- stop start) "\n")
(print "Note: clock is CPU time, not wall clock.\n\n")
