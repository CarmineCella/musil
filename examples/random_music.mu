# random_music: a score of notes drawn at random from a sample database, rendered to a file,
# shown as a roll, and played. The generator is a function returning events: change its
# distributions, or write your own the same way, and the rest follows.
# Usage: musil random_music.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 17)

var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
var db (db-load "../datasets/FullSOL2020.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh (or any *SOL set)
print "database:" (db-size db) "entries;" (length (db-available db)) "sounds on disk (missing pitches are shifted from the nearest)"

# --- the generator: density in notes per second, a range of durations, instruments with their pitch ranges ----
# (random-notes db secs density instruments dyns techs durs)   events for a score: instruments a list of codes, dyns a
#   list of dynamics, techs a list of playing techniques; nil for any of the three means everything the database has
#   (for the instrument, in the case of dynamics and techniques: what it was recorded with); durs (list shortest
#   longest); pitches uniform within each instrument's range in the database
function random-notes (db secs density instruments dyns techs durs) {
    var out (list)
    var n (floor (* secs density))
    var instrs (if (equal? (type instruments) "nil") (db-instruments-available db) instruments)
    each (range n) (function (k) {
        var instr (getidx instrs (floor (* (rand) (length instrs))))
        var range (db-range db instr)
        var midi (+ (head range) (floor (* (rand) (+ 1 (- (last range) (head range))))))
        var sounds (db-query db instr nil nil nil)
        var dyn-choices (if (equal? (type dyns) "nil") (unique (map sounds (function (e) (get e 'dyn)))) dyns)
        var dyn (getidx dyn-choices (floor (* (rand) (length dyn-choices))))
        var choices (if (equal? (type techs) "nil") (unique (map sounds (function (e) (get e 'tech)))) techs)
        var tech (getidx choices (floor (* (rand) (length choices))))
        var at (* (rand) secs)
        var dur (+ (head durs) (* (rand) (- (last durs) (head durs))))
        push out (list at dur (note db instr (midi->pitch midi) dyn tech))
    })
    return (sort-by out head)
}

# --- a score of a minute, 1.5 notes a second, 0.3 to 5 s each: every instrument, dynamic and technique the database has (nil = all of them);
#     give lists instead to choose: (random-notes db 30 3.5 (list 'Vn 'Vc) (list 'pp 'mf) (list 'ord) (list 0.4 2.5))
print "instruments:" (db-instruments-available db) " dynamics:" (db-dynamics db) " techniques:" (db-techniques db)
var s (score "random" 44100)
each (random-notes db 60 1.5 nil nil nil (list 0.3 5)) (function (n) {
    var e (event s (head n) (getidx n 1) (last n))
    event-place! e (- (* 120 (rand)) 60) 0                  # each note somewhere between left and right
    event-gain! e (if (equal? (get e 'dyn) "pp") 0.5 (if (equal? (get e 'dyn) "mf") 0.8 1))
})
score-print s

# --- render (in the concert hall, as the roll plays it; and dry), show, play ------------------------------------------
var hall (render s "/tmp/musil_random_hall.wav" "stereo")                    # render-buffer, written: the score's reverb applied
write-wav "/tmp/musil_random.wav" 44100 (score-render s "stereo")            # the dry mix itself
print "wrote /tmp/musil_random_hall.wav (in the Concertgebouw) and /tmp/musil_random.wav (dry);" (length (head hall)) "samples"
display s
# play it: press Play in the roll, or (play-score s 0.8) here; score-reverb! sets how much of the hall is heard
