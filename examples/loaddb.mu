# loaddb: a sample database in the *SOL layout (a feature file next to a folder of the same
# name holding the sounds), loaded, questioned, and used for notes. The bundled
# data/microsol is a tiny one (four instruments, 32 sounds); TinySOL, OrchideaSOL or any
# database in the same layout loads the same way: (db-load "path/to/X.spectrum.db").
# Usage: musil loaddb.mu [path/to/features.db]
load "music.mu"

var db (db-load (if (> (length args) 0) (getidx args 0) "data/microsol/microsol.spectrum.db"))
# a full set, after ./fetch_tinysol.sh:  musil loaddb.mu ../datasets/TinySOL.spectrum.db
print "database:" (get db 'path)
print "  " (db-size db) "entries;" (get db 'type) "features of" (get db 'ncoeff) "coefficients, block" (get db 'block) "hop" (get db 'hop)
print "  instruments (orchestral order):" (db-instruments db)
print "  techniques:" (db-techniques db) " dynamics:" (db-dynamics db)
each (db-instruments db) (function (i) (print "   " i ":" (length (db-query db i nil nil nil)) "sounds, range" (midi->pitch (head (db-range db i))) "-" (midi->pitch (last (db-range db i)))))

# --- queries: nil matches anything -----------------------------------------------------------
print "mf violin sounds:" (length (db-query db 'Vn nil 'mf nil)) " all C4s:" (map (db-query db nil 'C4 nil nil) (function (e) (get e 'instr)))
print "lists match any member:" (length (db-query db (list 'Vn 'Vc) nil (list 'mf 'ff) nil)) "string sounds; regular expressions:" (length (db-grep db "Ob-ord-[CD]")) "oboe C's and D's; a pitch range:" (length (db-between db 'Hn "C4" "E4")) "horn C4-E4"
var e (head (db-query db 'Vn 'C4 'mf 'ord))
print "an entry:" (get e 'file) "-> instr" (get e 'instr) "tech" (get e 'tech) "pitch" (get e 'pitch) "(midi" (get e 'midi) ") dyn" (get e 'dyn) "other" (get e 'other)
print "its features:" (length (db-features e)) "values, the first five" (fixed (take (db-features e) 5) 4)
print "the sounds most alike (by features):" (map (db-nearest db e 4) (function (x) (filename (get x 'file))))

# --- what is on disk: the sounds are opened only when a note is rendered or played --------------------
var here (filter (get db 'entries) (function (x) (db-available? db x)))
print "sounds on disk:" (length here) "of" (db-size db) ":" (map here (function (x) (filename (get x 'file))))

# --- notes: the nearest available sound, shifted to the pitch asked for ----------------------------------
each (list (list 'Vn 'C4 'mf) (list 'Vn 'B4 'mf) (list 'Vc 'C4 'ff) (list 'Hn 'A3 'mf) (list 'Ob 'C5 'mf)) (function (q) {
    var n (note db (head q) (getidx q 1) (last q) 'ord)
    print "note" (head q) (getidx q 1) (last q) "-> uses" (filename (get (get n 'entry) 'file)) "shifted" (get n 'shift) "semitones;" (fixed (note-duration n) 2) "s"
})

# --- making a feature file from a folder of sounds (what db-gen does; here on MicroSOL's own sounds) --------
var n-made (db-gen (get db 'root) "/tmp/musil_microsol_mfcc.db" "mfcc" 2048 256 13)
print n-made "sounds analysed into /tmp/musil_microsol_mfcc.db (13 mfcc each); (db-make folder path type block hop ncoeff) generates and loads"

# --- the features as a picture: the average spectra of every sound, by instrument and pitch ------------------
load "plot.mu"
var all (sort-by (get db 'entries) (function (x) (+ (* 100 (instrument-rank (get x 'instr))) (get x 'midi))))
var fig (figure "MicroSOL: average spectra, one row per sound (Ob, Hn, Vn, Vc, low to high) x frequency bin")
add-image fig (map all (function (x) (take (db-features x) 200)))
set-labels fig "frequency bin" "sound (low to high)"
show fig
