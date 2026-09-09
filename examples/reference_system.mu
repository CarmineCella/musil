# musil — library reference: system (system.h + system.mu)
#
# Operating-system access: processes, environment, directories and files,
# CSV and WAV files, UDP/OSC. system.mu loads std.mu itself.
# Run with: musil reference_system.mu
#
# Most of this output depends on the machine (paths, times, directory
# contents), so this file runs as a test but has no golden output.

load "system.mu"
print ""
print "================================================================"
print "  musil: library reference (system)"
print "================================================================"

# --- 1. Processes and environment ----------------------------------
print ""
print "--- processes and environment ---"

print "exec            :" (trim (exec "echo hello from the shell"))
print "getenv HOME     :" (type (getenv "HOME")) "(a string; nil when unset)"
print "now             :" (> (now) 1600000000) "(seconds since 1970; clock is monotonic)"
print "sleep 0.05 took :" (fixed (elapsed (function () (sleep 0.05))) 2) "s"

# --- 2. Directories and files ---------------------------------------
print ""
print "--- directories and files ---"

var dir "/tmp/musil_system_demo"
mkdir dir
var f (concat dir "/notes.txt")
write-lines f (list "one" "two" "three")
print "cwd is a string :" (type (cwd))
print "ls dir          :" (ls dir)
print "file-lines      :" (file-lines f)
print "file?, dir?     :" (file? f) (dir? dir)
print "file-size       :" (file-size f)
print "stat            :" (stat f) "(size, is-dir, modified-seconds)"
print "stat missing    :" (stat "/no/such/file")
print "basename        :" (basename "/a/b/c.wav")
print "dirname         :" (dirname "/a/b/c.wav")
print "extension       :" (extension "/a/b/c.wav")

# --- 3. CSV: a table is a list of rows ------------------------------
print ""
print "--- csv ---"

var csv (concat dir "/notes.csv")
var table (list (list "note" "midi" "hz") (list "A4" 69 440) (list "C4" 60 261.63) (list "comma, quoted" 0 0))
write-csv csv table
print "written:"
print (read csv)
print "read-csv        :" (read-csv csv) "(numeric cells come back as numbers)"

# --- 4. WAV: (read-wav path) => (sample-rate (channel ...)) -------------
print ""
print "--- wav ---"

var wav (concat dir "/tone.wav")
var sr 8000
var t (/ (range 4000) sr)
var left  (* 0.5 (sin (* tau 440 t)))
var right (* 0.5 (sin (* tau 660 t)))
write-wav wav sr (list left right)          # 16-bit PCM; add 32 for float
var w (read-wav wav)
print "sample rate     :" (head w)
print "channels, frames:" (length (getidx w 1)) (length (head (getidx w 1)))
print "duration        :" (wav-duration w) "s"
print "max error 16-bit:" (fixed (max (abs (- (head (getidx w 1)) left))) 5)
write-wav wav sr left                       # a bare vector is mono
print "mono channels   :" (length (getidx (read-wav wav) 1))

# --- 5. UDP and OSC ---------------------------------------------------
print ""
print "--- udp ---"

print "udp-send        :" (udp-send "127.0.0.1" 9 "hello") "(1 = sent)"
print "udp-send osc    :" (udp-send "127.0.0.1" 9 "/synth/freq" 1) "(address padded, empty type tag)"
print "udp-receive     :" (udp-receive "127.0.0.1" 47123 0.05) "(nil: nothing arrived within the timeout)"

# --- cleanup --------------------------------------------------------
remove f
remove csv
remove wav
print "remove dir      :" (remove dir)

print ""
print "================================================================"
print "  end of system reference"
print "================================================================"
