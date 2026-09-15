# random_music: a score of notes drawn at random from a sample database, rendered to a file,
# shown as a roll, and played. The generator is a function returning events: change its
# distributions, or write your own the same way, and the rest follows.
# Usage: musil random_music.mu [seed]
load "music.mu"
seed (if (> (length args) 0) (num (getidx args 0)) 7)

#var db (db-load "data/microsol/microsol.spectrum.db")           # the bundled MicroSOL: Ob, Hn, Vn, Vc, C4-G4
var db (db-load "../datasets/StaticSOL.spectrum.db")           # the full TinySOL, after ./fetch_tinysol.sh (or any *SOL set)
print "database:" (db-size db) "entries;" (length (db-available db)) "sounds on disk (missing pitches are shifted from the nearest)"

# --- the generator: density in notes per second, a range of durations, instruments with their pitch ranges ----
# (random-notes db secs density instruments dyns techs durs)   events for a score: instruments a list of codes, dyns a
#   list of dynamics, techs a list of playing techniques (nil: every technique the database has for the instrument),
#   durs (list shortest longest); pitches uniform within each instrument's range in the database
function random-notes (db secs density instruments dyns techs durs) {
    var out (list)
    var n (floor (* secs density))
    each (range n) (function (k) {
        var instr (getidx instruments (floor (* (rand) (length instruments))))
        var range (db-range db instr)
        var midi (+ (head range) (floor (* (rand) (+ 1 (- (last range) (head range))))))
        var dyn (getidx dyns (floor (* (rand) (length dyns))))
        var choices (if (equal? (type techs) "nil") (unique (map (db-query db instr nil nil nil) (function (e) (get e 'tech)))) techs)
        var tech (getidx choices (floor (* (rand) (length choices))))
        var at (* (rand) secs)
        var dur (+ (head durs) (* (rand) (- (last durs) (head durs))))
        push out (list at dur (note db instr (midi->pitch midi) dyn tech))
    })
    return (sort-by out head)
}

# --- a score of thirty seconds for the orchestra: whichever of these instruments have sounds on disk ----------
#     (all of them with a full TinySOL; oboe, horn, violin and cello with the bundled MicroSOL)
var wanted (list 'Fl 'Ob 'ClBb 'Bn 'Hn 'TpC 'Tbn 'BTb 'Vn 'Va 'Vc 'Cb)
var here (db-instruments-available db)
var orchestra (filter wanted (function (i) (contains? here (str i))))
print "instruments with sounds on disk:" orchestra
var s (score "random" 44100)
print "techniques in the database:" (db-techniques db) "(nil below: every technique an instrument has)"
each (random-notes db 30 3.5 orchestra (list 'pp 'p 'mf 'f 'ff) nil (list 0.4 2.5)) (function (n) {
    var e (event s (head n) (getidx n 1) (last n))
    event-place! e (- (* 120 (rand)) 60) 0                  # each note somewhere between left and right
    event-gain! e (if (equal? (get e 'dyn) "pp") 0.5 (if (equal? (get e 'dyn) "mf") 0.8 1))
})
score-print s

# --- render, then a hall around it: the mix convolved with the Concertgebouw's impulse response ----------------
var dry (render s "/tmp/musil_random.wav" "stereo")
var ir (read-wav "data/Concertgebouw-s.wav")
var irs (map (getidx ir 1) (function (c) (resample-to c (head ir) 44100)))
function in-the-hall (chans) (map (zip chans irs) (function (p) {
    var wet (conv (head p) (last p))                                    # as long as the mix plus the hall's tail
    return (+ (* 0.6 (vec (head p) (zeros (- (length wet) (length (head p)))))) (* 0.4 wet))   # 60 % dry, 40 % hall
}))
var wet (normalize-peak-stereo (in-the-hall dry))
write-wav "/tmp/musil_random_hall.wav" 44100 wet
print "wrote /tmp/musil_random.wav (stereo, dry) and /tmp/musil_random_hall.wav (in the Concertgebouw)"
display s
if (interactive?) {
    print "playing the hall version..."
    audio-init
    play wet 44100
    sleep (+ 1 (/ (length (head wet)) 44100))
    audio-quit
}
