# dbgen: making a feature file for a folder of sounds (what Orchidea's dbgen does), then loading and
# questioning it. The folder is MicroSOL's own; the file goes next to it, so that
# (db-load "data/microsol/example.spectrum.db") is a database of the same sounds analysed here.
# Usage: musil dbgen.mu [folder out.db type block hop ncoeff]
load "music.mu"

var folder (if (> (length args) 0) (getidx args 0) "data/microsol/microsol")
var out (if (> (length args) 1) (getidx args 1) "data/microsol/example.spectrum.db")
var kind (if (> (length args) 2) (getidx args 2) "spectrum")       # spectrum logspec specpeaks specenv mfcc moments
var block (if (> (length args) 3) (num (getidx args 3)) 2048)
var hop (if (> (length args) 4) (num (getidx args 4)) 256)
var ncoeff (if (> (length args) 5) (num (getidx args 5)) 1024)

var t0 (clock)
var n (db-gen folder out kind block hop ncoeff)
print n "sounds analysed into" out "in" (fixed (- (clock) t0) 2) "s (" kind "," ncoeff "coefficients, block" block "hop" hop ")"

# --- the file loads like any other: its sounds are found next to it (the folder is named after it, or found by content) ---
var db (db-load out)
print "loaded:" (db-size db) "entries;" (db-instruments db) "; sounds on disk:" (length (db-available db))
var e (head (db-query db 'Vn 'C4 'mf 'ord))
print "an entry:" (get e 'file) "-> features" (length (db-features e)) "values, the first four" (fixed (take (db-features e) 4) 4)

# --- against the feature file that came with MicroSOL (the same analysis): identical features ------------------------
var ref (db-load "data/microsol/microsol.spectrum.db")
var mine (db-features e)
var theirs (db-features (head (db-query ref 'Vn 'C4 'mf 'ord)))
print "correlation with MicroSOL's own features:" (fixed (/ (dot mine theirs) (* (norm mine) (norm theirs))) 4)

# --- other feature types, smaller files: mfcc for timbre, moments for a four-number summary -----------------------
db-gen folder "/tmp/musil_example.mfcc.db" "mfcc" 2048 256 13
var mf (db-load "/tmp/musil_example.mfcc.db")
print "mfcc:" (get mf 'ncoeff) "coefficients; the sound most like the violin C4 by mfcc:" (map (db-nearest mf (head (db-query mf 'Vn 'C4 'mf 'ord)) 3) (function (x) (filename (get x 'file))))
db-gen folder "/tmp/musil_example.moments.db" "moments" 2048 256 4
var mo (db-load "/tmp/musil_example.moments.db")
print "moments of the violin C4: centroid, spread, skewness, kurtosis" (fixed (db-features (head (db-query mo 'Vn 'C4 'mf 'ord))) 2)
# a note from the new database plays like any other
var s (score "from the example db" 44100)
event s 0 1 (note db 'Vn "E4" 'mf 'ord)
print "a note from it renders" (length (head (render-event (head (score-events s)) 44100))) "samples"
