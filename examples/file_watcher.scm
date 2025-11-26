;; file_watcher.scm
;;
;; A tiny file watcher using dirlist, filestat and schedule.
;; It checks for a given file every 2000 ms and prints basic info.
;; NB: after some time this will create a stack overflow and crash for the recursive call to schedule

(load "stdlib.scm")

(print "=== file_watcher.scm ===\n")
(print "Watching file \"target.txt\" in current directory every 2 seconds.\n")
(print "Stop by killing the process (Ctrl+C).\n\n")

(def watched-file "target.txt")

;; function: check file and schedule itself again
(function watch-once () {
    (def st (filestat watched-file))     ;; (exists size nlink perms)
    (def exists (lindex st (array 0)))
    (if exists {
          (def size (lindex st (array 1)))
          (def perms (lindex st (array 3)))
          (print "[watch] file exists: " watched-file
                 ", size = " size ", perms = " perms "\n")}
        (print "[watch] file does not exist: " watched-file "\n"))
    ;; schedule next check in 2000 ms
    (sleep 2000)
    (schedule (watch-once) (array 2000))
  })

;; start the loop by scheduling first call
(schedule (watch-once) (array 0))

(print "Initial watch scheduled.\n\n")
