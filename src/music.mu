# music.mu — the score: things placed in time, and what to do with them
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# A score is a record: name, sample rate, a list of events, a layout. An event is a record
# with an onset (at, seconds), a duration (dur), a kind, its source, a gain and a position
# (azimuth, elevation for the spatial layouts). What an event can hold:
#     (event s 0 3.5 "bells.wav")                    a sound file (read when rendered, cached)
#     (event s 2 3.6 buffer)                         a buffer: a vector, or a list of channel vectors
#     (event s 4 1 (note db 'Vn 'C5 'mf 'ord))       a sound from a database (nearest pitch, shifted)
#     (event s 5 2 (instrument pluck (list (list 'freq 440))))   a synth: a function and its parameters
#     (event s 6 1 (call my-function (list 1 2)))    any function: called when rendered, its result the sound
# and a function alone, (event s 7 1 tone), is a call with the duration as its one argument. An event
# longer than its duration is faded out at the end; shorter ones are left as they are; a synth's gate is
# the duration and its release plays on after it. Pitches with a
# sharp are written as strings ("D#5"): a # in a symbol would start a comment.
#
# (render s "out.wav" "stereo")   writes the mix;  layouts: "mono" "stereo" "binaural" "ambi N", a list of
#                                 speaker azimuths (a ring), or a number of channels (multichannel events as they are)
# (play-score s gain)             plays it through live, ahead of the clock (synths compiled, the rest rendered),
#                                 everything scaled by gain
# (display s)                     the roll: rows by kind (notes in orchestral order, then files, buffers, synths,
#                                 calls); WASD and + - 0 navigate in time, hovering shows an event and the time
load "std.mu"
load "system.mu"
load "signals.mu"
load "live.mu"
load "plot.mu"

# --- the score ------------------------------------------------------------------------------
# (score name sr)          a new, empty score at a sample rate
function score (name sr) (record (list 'name name 'sr sr 'events (list) 'layout "stereo"))
# (score-events s) (score-sr s) (score-duration s)   its events (in the order they were added), rate, end time
function score-events (s) (get s 'events)
function score-sr (s) (get s 'sr)
function score-duration (s) (if (== (length (get s 'events)) 0) 0 (max-of (map (get s 'events) event-end)))
# (event-end e)            when the event ends
function event-end (e) (+ (get e 'at) (get e 'dur))
# (event s at dur what)    add an event; what is a path, a buffer, a note, an instrument, a call, or a function;
#                          => the event record (its at and dur can be changed later with put!)
function event (s at dur what) (event-at s at dur what 0 0)
# (event-at s at dur what az el)   the same, placed at an azimuth and an elevation (degrees, 0 in front, left positive)
function event-at (s at dur what az el) {
    if (< dur 0) { error "event: the duration must be >= 0" }
    var e (payload->event what)
    put! e 'at at
    put! e 'dur dur
    put! e 'az az
    put! e 'el el
    put! e 'gain 1
    put! e 'id (+ 1 (length (get s 'events)))
    push (get s 'events) e
    return e
}
# (payload->event what)    the event record of a payload (kind and source), without its time
function payload->event (what) {
    var t (type what)
    if (equal? t "string") { return (record (list 'kind 'file 'source what 'label (filename what))) }
    if (equal? t "vec") { return (record (list 'kind 'buffer 'source what 'label (concat "buffer " (str (length what))))) }
    if (equal? t "function") { return (record (list 'kind 'call 'source what 'args (list 'dur) 'label (concat "call " (fn-name what)))) }
    if (equal? t "list") {
        if (and (> (length what) 0) (equal? (type (head what)) "vec")) { return (record (list 'kind 'buffer 'source what 'label (concat "buffer x" (str (length what))))) }
        if (has? what 'kind) { return (map what (function (p) (list (head p) (last p)))) }   # a note, instrument or call record: copied
    }
    error "event: cannot place a " t " in a score"
}
# (filename path)          the last part of a path, for labels
function filename (path) (last (split path "/"))
# (event-gain! e g)        the gain of an event (1 by default); (event-move! e at) (event-length! e dur) (event-place! e az el)
function event-gain! (e g) (put! e 'gain g)
function event-move! (e at) (put! e 'at at)
function event-length! (e dur) (put! e 'dur dur)
function event-place! (e az el) { put! e 'az az
                                  put! e 'el el }

# --- the payloads ---------------------------------------------------------------------------
# (instrument f params)    a synth event: f a streamable instrument function (gate first), params its parameters
#                          as (list (list 'freq 440) ...); the gate is the event's duration
function instrument (f params) (record (list 'kind 'synth 'source f 'params params 'label (concat "synth " (fn-name f))))
# (call f args)            a call event: f applied to args when the score is rendered; its result (a vector or a
#                          list of channels) is the sound. A function taking the duration can ask for it: pass
#                          'dur among the args, it is replaced by the event's duration in seconds
function call (f args) (record (list 'kind 'call 'source f 'args args 'label (concat "call " (fn-name f))))
# (fn-name f) the name a function prints as, for labels
function fn-name (f) {
    var s (str f)
    if (starts-with? s "<fn ") { return (replace (replace s "<fn " "") ">" "") }
    return "fn"
}

# --- rendering an event to channels ------------------------------------------------------------
var sound-cache (list)          # path -> (list sr channels), so a file read once serves every event
# (load-sound path)        a WAV as (list sr channels), from the cache
function load-sound (path) {
    var hit (opt sound-cache path nil)
    if (not (equal? (type hit) "nil")) { return hit }
    var w (read-wav path)
    push sound-cache (list path w)
    return w
}
# (clear-sound-cache)      forget the loaded files
function clear-sound-cache () (set sound-cache (list))
# (to-rate channels from to)   channels resampled from one rate to another (nothing to do when equal)
function to-rate (channels from to) (if (== from to) channels (map channels (function (c) (resample-to c from to))))
# (render-event e sr)      the sound of an event at the score's rate: a list of channels, cut to its duration
#                          with a fade at the end when the sound is longer, gain applied
function render-event (e sr) {
    var kind (get e 'kind)
    var chans nil
    if (equal? kind 'file) {
        var w (load-sound (get e 'source))
        set chans (to-rate (getidx w 1) (head w) sr)
    }
    if (equal? kind 'buffer) {
        var src (get e 'source)
        set chans (if (equal? (type src) "vec") (list src) src)
    }
    if (equal? kind 'note) {
        var w (load-sound (note-path e))
        var shift (opt e 'shift 0)
        set chans (to-rate (getidx w 1) (head w) sr)
        if (!= shift 0) { set chans (map chans (function (c) (resample c (pow 2 (/ (- 0 shift) 12))))) }   # a pitch shift by resampling
    }
    if (equal? kind 'synth) {
        var n (max 1 (floor (* (get e 'dur) sr)))
        var gate (ones n)
        var params (concat-list (list (list 'gate gate)) (get e 'params))
        var r (synth-render (get e 'source) params (/ (+ n (floor (* 0.5 sr))) sr))    # half a second for the release
        set chans (if (equal? (type r) "vec") (list r) r)
    }
    if (equal? kind 'call) {
        var args (map (get e 'args) (function (a) (if (equal? a 'dur) (get e 'dur) a)))
        var r (apply (get e 'source) args)
        set chans (if (equal? (type r) "vec") (list r) r)
    }
    if (equal? (type chans) "nil") { error "render-event: unknown kind " kind }
    if (equal? kind 'synth) { return (map chans (function (c) (* (get e 'gain) c))) }      # a synth's gate is its duration; its release plays on
    return (map chans (function (c) (* (get e 'gain) (fit-duration c (get e 'dur) sr))))
}
# (fit-duration x dur sr)  x cut to dur seconds with a fade-out over the last 20 ms (or a quarter of dur), if longer
function fit-duration (x dur sr) {
    var n (floor (* dur sr))
    if (<= (length x) n) { return x }
    var fade (max 1 (min (floor (* 0.02 sr)) (floor (/ n 4))))
    return (fade-out (take x n) fade)
}

# --- the mix: every event placed and spatialised into a layout ---------------------------------
# (layout-channels layout)   how many channels a layout has
function layout-channels (layout) {
    if (equal? (type layout) "scalar") { return layout }
    if (equal? (type layout) "vec") { return (length layout) }
    if (equal? layout "mono") { return 1 }
    if (equal? layout "stereo") { return 2 }
    if (equal? layout "binaural") { return 2 }
    if (starts-with? layout "ambi") { var o (num (trim (slice layout 4 4)))
                                      return (* (+ o 1) (+ o 1)) }
    error "layout: unknown " layout
}
# (place chans layout az el sr)   an event's channels into the layout's channels: a mono sound is spatialised at its
#                          direction; a sound with as many channels as the layout goes in as it is; anything else is
#                          summed to mono first
function place (chans layout az el sr) {
    var nch (layout-channels layout)
    if (== (length chans) nch) { return chans }
    var mono (if (== (length chans) 1) (head chans) (/ (reduce chans + (zeros (length (head chans)))) (length chans)))
    if (equal? (type layout) "scalar") { return (concat-list (list mono) (map (vec->list (range (- nch 1))) (function (k) (* 0 mono)))) }
    if (equal? (type layout) "vec") { return (pan-n mono layout az) }
    if (equal? layout "mono") { return (list mono) }
    if (equal? layout "stereo") { return (pan-azimuth mono az) }
    if (equal? layout "binaural") { return (binaural mono az el sr) }
    var o (num (trim (slice layout 4 4)))
    return (ambi-encode mono o az el)
}
# (score-render s layout)  the whole score as channels at its rate
function score-render (s layout) {
    var sr (get s 'sr)
    var nch (layout-channels layout)
    var total (max 1 (+ (floor (* (score-duration s) sr)) (floor (* 0.6 sr))))    # room for the last event's release and fades
    var out (map (vec->list (range nch)) (function (k) (zeros total)))
    each (get s 'events) (function (e) {
        var placed (place (render-event e sr) layout (get e 'az) (get e 'el) sr)
        var start (floor (* (get e 'at) sr))
        each (zip out placed) (function (p) {
            var room (- total start)
            if (> room 0) { add-at! (head p) start (take (last p) (min (length (last p)) room)) }
        })
    })
    return out
}
# (render s path layout)   the score written as a WAV; => the channels
function render (s path layout) {
    var out (score-render s layout)
    var peak (max-of (map out (function (c) (max (abs c)))))
    var safe (if (> peak 1) (map out (function (c) (/ c peak))) out)
    write-wav path (get s 'sr) safe
    if (> peak 1) { print "render: the mix peaked at" (fixed peak 2) "and was scaled to 1" }
    return safe
}

# --- playing: the events through live, ahead of the clock ----------------------------------------
# (play-score s gain)      play the score now (the device is opened if needed) and wait until it ends; files,
#                          buffers, notes and calls are rendered first, then everything is scheduled ahead of the
#                          clock; synths are compiled and gated on time (an instrument that is not streamable is
#                          refused, naming the event); gain scales everything (1 as rendered)
function play-score (s gain) (play-score-from s gain 0)
# (play-score-from s gain from)   the same from a time in seconds: events already over are skipped, one under way
#                          starts in the middle
function play-score-from (s gain from) {
    var sched (score-schedule s gain from)
    sleep (+ 0.4 (- (score-duration s) from) 0.6)
    each (head sched) free
    playhead-off!
    return nil
}
# (score-schedule s gain from)   schedule the score's events on the clock and return at once:
#                          => (list synth-ids end-clock-time start-clock-time); free the synths when it is over
function score-schedule (s gain from) {
    if (not (opt (audio-status) "open" 0)) { audio-init }
    var sr (audio-sr)
    var rendered (list)                                          # everything rendered before the clock is read
    each (get s 'events) (function (e) {
        if (> (event-end e) from) {
            var kind (get e 'kind)
            var skip (max 0 (- from (get e 'at)))
            if (equal? kind 'synth) {
                push rendered (list e nil skip)
            } {
                var chans (render-event e sr)
                var placed (place chans "stereo" (get e 'az) (get e 'el) sr)
                push rendered (list e (map placed (function (c) (drop c (floor (* skip sr))))) skip)
            }
        }
    })
    var t0 (+ (audio-time) 0.3)
    var synths (list)
    each rendered (function (r) {
        var e (head r)
        var when (+ t0 (max 0 (- (get e 'at) from)))
        if (equal? (get e 'kind) 'synth) {
            var id (try (synth (get e 'source)) catch err (error "play-score: the instrument of event " (get e 'id) " does not stream: " err))
            push synths id
            if (contains? (synth-params id) "amp") { set-param id 'amp gain 0 when }
            each (get e 'params) (function (p) (set-param id (head p) (last p) 0 when))
            note-at when id (- (get e 'dur) (last r))
        } {
            play-buffer (getidx r 1) sr gain 0 1 0 when
        }
    })
    playhead! t0 from (+ t0 (- (score-duration s) from) 0.6)                      # the roll's cursor follows
    return (list synths (+ t0 (- (score-duration s) from) 0.6) t0)
}
# --- the roll --------------------------------------------------------------------------------
var orchestral-order (list "Picc" "Fl" "AFl" "BFl" "Ob" "EH" "ClEb" "ClBb" "Cl" "BCl" "CbCl" "Bn" "CbBn" "SSax" "ASax" "TSax" "BSax"
                           "Hn" "TpC" "Tp" "Tbn" "BTb" "Tba" "Timp" "Perc" "Hp" "Pno" "Acc" "Gtr" "Vn" "Va" "Vc" "Cb")
# (instrument-rank instr) where an instrument code sits in the orchestral order (unknown ones after)
function instrument-rank (instr) {
    var k (find orchestral-order (str instr))
    return (if (< k 0) (+ 1000 (length (str instr))) k)
}
# (score-rows s)           the rows of the roll: one per instrument (orchestral order) for the notes, then one
#                          per kind: files, buffers, synths, calls; => (list (list label events) ...)
function score-rows (s) {
    var evs (get s 'events)
    var notes (filter evs (function (e) (equal? (get e 'kind) 'note)))
    var by-instr (group-by notes (function (e) (get e 'instr)))
    var instr-rows (sort-by by-instr (function (g) (instrument-rank (head g))))
    var rows (map instr-rows (function (g) (list (head g) (last g))))
    each (list (list 'file "files") (list 'buffer "buffers") (list 'synth "synths") (list 'call "calls")) (function (k) {
        var of-kind (filter evs (function (e) (equal? (get e 'kind) (head k))))
        if (> (length of-kind) 0) { push rows (list (last k) of-kind) }
    })
    return rows
}
# (score-roll s)           the roll as a figure: rows (name and clef) and bars (row start dur label tip group lane lanes
#                          midi dyn); pitched events sit on their row's staff, the others on a line
function score-roll (s) {
    var rows (score-rows s)
    var groups (list 'note 'file 'buffer 'synth 'call)
    var bars (list)
    var r 0
    var row-specs (list)
    each rows (function (row) {
        var evs (last row)
        var pitched (filter evs (function (e) (has? e 'midi)))
        var clef (if (== (length pitched) 0) "none" (if (< (median (vec (map pitched (function (e) (get e 'midi))))) 60) "bass" "treble"))
        push row-specs (list (head row) clef)
        var lanes (if (equal? clef "none") (event-lanes evs) (map evs (function (e) 0)))
        var nlanes (+ 1 (max-of lanes))
        each (zip evs lanes) (function (pair) {
            var e (head pair)
            var kind (get e 'kind)
            var tip (if (equal? kind 'note) (concat (get e 'pitch) " " (get e 'dyn) " " (get e 'tech) (if (!= (opt e 'shift 0) 0) (concat " (shifted " (str (opt e 'shift 0)) ")") "")) (concat "az " (str (get e 'az)) " el " (str (get e 'el))))
            push bars (list r (get e 'at) (get e 'dur) (get e 'label) tip (find groups kind) (last pair) nlanes (opt e 'midi -1) (opt e 'dyn "") (get e 'id))
        })
        set r (+ r 1)
    })
    return (list (get s 'name) (list (list "roll" row-specs bars (register-score s))) (list (list "xlabel" "time (s)")))
}
# --- scores on display: a registry, so that a window can name a score and an event to play -------------------
var displayed-scores (list)
# (register-score s)       remember a score under a number (the roll carries it); => the number
function register-score (s) {
    var hit (find-first displayed-scores (function (p) (equal? (last p) s)))
    if (not (equal? (type hit) "nil")) { return (head hit) }
    var id (+ 1 (length displayed-scores))
    push displayed-scores (list id s)
    return id
}
# (displayed-score id)     the score registered under a number
function displayed-score (id) (get displayed-scores id)
# (play-event score-id event-id)   play one event of a displayed score, alone (a double-click in the roll does this)
function play-event (score-id event-id) {
    var s (displayed-score score-id)
    var e (find-first (get s 'events) (function (x) (== (get x 'id) event-id)))
    if (equal? (type e) "nil") { error "play-event: no event " event-id }
    if (not (opt (audio-status) "open" 0)) { audio-init }
    var sr (audio-sr)
    if (equal? (get e 'kind) 'synth) {
        var id (synth (get e 'source))
        each (get e 'params) (function (p) (set-param id (head p) (last p)))
        note id (get e 'dur)
        return nil
    }
    play-buffer (place (render-event e sr) "stereo" (get e 'az) (get e 'el) sr) sr 1 0 1 0 (+ (audio-time) 0.05)
    return nil
}
# (event-lanes events)     a lane number per event such that events sharing a lane do not overlap in time
function event-lanes (events) {
    var ends (list)                                          # the end time of the last event in each lane
    return (map events (function (e) {
        var k 0
        while (and (< k (length ends)) (> (getidx ends k) (+ (get e 'at) 1e-9))) { set k (+ k 1) }
        if (== k (length ends)) { push ends (event-end e) } { setidx ends k (event-end e) }
        return k
    }))
}
# (display s)              show the score as a roll: a staff per instrument with note heads and duration lines, a
#                          line per other kind; A D pan and + - zoom in time, W S scroll and Z X zoom the rows,
#                          R resets; hovering shows an event; the cursor follows play-score
function display (s) (show (score-roll s) 1100 (min 900 (max 320 (+ 100 (* 120 (length (score-rows s)))))))

# --- databases (the *SOL layout: a feature file next to a folder of the same name holding the sounds) -----------
# (db-load path)           load a database from its feature file: TinySOL.spectrum.db with the sounds in TinySOL/ next
#                          to it (the paths in the file are relative to that folder); when no such folder exists the
#                          sounds are looked for next to the file itself. => a record with 'path, 'root (the sounds'
#                          folder), 'type, 'block, 'hop, 'ncoeff and 'entries (records: file instr tech pitch dyn
#                          other midi features). The sounds are not read: notes open them when rendered or played.
function db-load (path) {
    if (equal? (type path) "list") { return (db-merge (map path db-load)) }
    var r (db-read path)
    var full (resolve-path path)
    if (not (exists? full)) { set full (find-file path) }
    var parts (split full "/")
    var dir (join (take parts (- (length parts) 1)) "/")
    if (equal? dir "") { set dir "." }
    var root (db-sounds-folder dir (getidx r 4))
    return (record (list 'path full 'root root 'type (head r) 'block (getidx r 1) 'hop (getidx r 2) 'ncoeff (getidx r 3) 'entries (getidx r 4)))
}
# (db-sounds-folder dir entries)   where the sounds of a feature file in dir are: the folder next to it (or dir itself)
#                          under which the first entry's file exists, whatever the folder is called (TinySOL/, tinysol/,
#                          a copy named otherwise); dir when nothing is found (a note then says the sound is missing)
function db-sounds-folder (dir entries) {
    if (== (length entries) 0) { return dir }
    var probes (map (take entries (min 20 (length entries))) (function (e) (get e 'file)))
    var holds? (function (folder) (any? probes (function (rel) (not (equal? (db-locate folder rel) "")))))
    var subs (filter (map (ls dir) (function (n) (concat dir "/" n))) directory?)
    var hit (find-first subs holds?)                         # a folder next to the file holding some of the sounds, whatever its name
    if (not (equal? (type hit) "nil")) { return hit }
    var any-sounds (find-first subs (function (f) (> (db-index-size f) 0)))
    if (not (equal? (type any-sounds) "nil")) { return any-sounds }
    return dir
}
# (db-merge dbs)           one database out of a list of several, when their features agree (same type and number of
#                          coefficients): the entries of all, each remembering its own sounds' folder; a note
#                          then draws on every set at once. (db-load (list "a.db" "b.db")) is the same.
function db-merge (dbs) {
    if (== (length dbs) 0) { error "db-merge: no databases" }
    var first (head dbs)
    each dbs (function (d) {
        if (not (equal? (get d 'type) (get first 'type))) { error "db-merge: feature types differ: " (get first 'type) " and " (get d 'type) }
        if (!= (get d 'ncoeff) (get first 'ncoeff)) { error "db-merge: numbers of coefficients differ: " (get first 'ncoeff) " and " (get d 'ncoeff) }
    })
    var entries (list)
    each dbs (function (d) (each (get d 'entries) (function (e) (push entries (if (has? e 'root) e (put e 'root (get d 'root)))))))
    var paths (reduce dbs (function (acc d) (concat-list acc (opt d 'paths (list (get d 'path))))) (list))
    return (record (list 'path (head paths) 'paths paths 'root (get first 'root) 'type (get first 'type) 'block (get first 'block) 'hop (get first 'hop) 'ncoeff (get first 'ncoeff) 'entries entries))
}
# (db-size db) (db-instruments db) (db-techniques db) (db-dynamics db) (db-pitches db)   what the database holds
function db-size (db) (length (get db 'entries))
function db-values (db key) (unique (map (get db 'entries) (function (e) (get e key))))
function db-instruments (db) (sort-by (db-values db 'instr) instrument-rank)
function db-techniques (db) (sort-list (db-values db 'tech))
function db-dynamics (db) (db-values db 'dyn)
function db-pitches (db) (sort-list (filter (db-values db 'pitch) (function (p) (not (equal? p "N")))))
# (db-query db instr pitch dyn tech)   the entries matching the given values; nil matches anything, a list matches
#                          any of its members: (db-query db 'Vn nil 'mf nil) every mf violin sound,
#                          (db-query db (list 'Vn 'Va) nil (list 'pp 'p) 'ord) quiet upper strings
function db-query (db instr pitch dyn tech) (filter (get db 'entries) (function (e) (and (match? (get e 'instr) instr) (match? (get e 'pitch) pitch) (match? (get e 'dyn) dyn) (match? (get e 'tech) tech))))
function match? (value want) {
    if (equal? (type want) "nil") { return 1 }
    if (equal? (type want) "list") { return (any? want (function (w) (equal? (str value) (str w)))) }
    return (equal? (str value) (str want))
}
# (db-grep db pattern)     the entries whose file name matches a regular expression: (db-grep db "Fl-ord-C.-pp")
function db-grep (db pattern) (filter (get db 'entries) (function (e) (regex-match? pattern (get e 'file))))
# (db-find db text)        the entries whose file name contains the text: (db-find db "Vn-ord-C5")
function db-find (db text) (filter (get db 'entries) (function (e) (contains? (get e 'file) text)))
# (db-between db instr lo hi)   the entries of an instrument between two pitches (names or MIDI numbers), inclusive
function db-between (db instr lo hi) {
    var a (if (equal? (type lo) "string") (pitch->midi lo) (if (equal? (type lo) "symbol") (pitch->midi (str lo)) lo))
    var b (if (equal? (type hi) "string") (pitch->midi hi) (if (equal? (type hi) "symbol") (pitch->midi (str hi)) hi))
    return (filter (db-query db instr nil nil nil) (function (e) (and (>= (get e 'midi) a) (<= (get e 'midi) b))))
}
# (db-nearest db entry n)  the n entries whose features are closest (euclidean) to an entry's: sounds alike
function db-nearest (db entry n) {
    var f (get entry 'features)
    var scored (map (get db 'entries) (function (e) (list (norm (- (get e 'features) f)) e)))
    return (map (take (sort-by scored head) n) last)
}
# (db-available db)        the entries whose sounds are on disk; (db-instruments-available db) their instruments
function db-available (db) (filter (get db 'entries) (function (e) (db-available? db e)))
function db-instruments-available (db) (sort-by (unique (map (db-available db) (function (e) (get e 'instr)))) instrument-rank)
# (db-range db instr)      the lowest and highest MIDI note of an instrument, as (list lo hi)
function db-range (db instr) {
    var ms (vec (map (filter (db-query db instr nil nil nil) (function (e) (>= (get e 'midi) 0))) (function (e) (get e 'midi))))
    if (== (length ms) 0) { error "db-range: no pitched sounds of " instr }
    return (list (min ms) (max ms))
}
# (db-path db entry)       the sound file of an entry, on disk: under the sounds' folder at the path the feature file
#                          gives when it exists, else found by its file name anywhere under that folder (the folders
#                          of a copy may be named otherwise, and a sharp written # in the file may be _ on disk);
#                          the path as the file gives it when the sound is not there at all
function db-path (db entry) {
    var rel (get entry 'file)
    var root (opt entry 'root (get db 'root))                          # a merged entry knows its own folder
    var p (db-locate root rel)
    return (if (equal? p "") (concat root (if (starts-with? rel "/") "" "/") rel) p)
}
# (db-available? db entry)   is the sound file of an entry on disk? (a feature file may describe more sounds than are present)
function db-available? (db entry) (exists? (db-path db entry))
# (db-features entry)      the feature vector of an entry
function db-features (entry) (get entry 'features)

# (db-make folder path type block hop ncoeff)   analyse every WAV under a folder into a feature file (db-gen) and load it
function db-make (folder path type block hop ncoeff) {
    db-gen folder path type block hop ncoeff
    return (db-load path)
}

# --- notes: sounds from a database as events --------------------------------------------------------
# (note db instr pitch dyn tech)   a note event: the entry matching instrument, pitch, dynamics and technique, or
#                          the nearest available pitch of that instrument (same dynamics and technique when they
#                          exist), shifted by resampling to the pitch asked for. Only entries whose sound is on
#                          disk are candidates.
function note (db instr pitch dyn tech) {
    var want (pitch->midi (str pitch))
    if (< want 0) { error "note: not a pitch: " pitch }
    var same (filter (db-query db instr nil dyn tech) (function (e) (and (>= (get e 'midi) 0) (db-available? db e))))
    if (== (length same) 0) { set same (filter (db-query db instr nil nil tech) (function (e) (and (>= (get e 'midi) 0) (db-available? db e)))) }
    if (== (length same) 0) { set same (filter (db-query db instr nil nil nil) (function (e) (and (>= (get e 'midi) 0) (db-available? db e)))) }
    if (== (length same) 0) { error "note: no sound of " instr " on disk in this database" }
    var exact (filter same (function (e) (== (get e 'midi) want)))
    var entry (if (> (length exact) 0) (head exact) (min-by same (function (e) (abs (- (get e 'midi) want)))))
    var shift (- want (get entry 'midi))
    return (record (list 'kind 'note 'source (get entry 'file) 'db db 'entry entry 'instr (str instr) 'pitch (str pitch) 'midi want
                         'dyn (str dyn) 'tech (str tech) 'shift shift 'label (concat (str instr) " " (str pitch) " " (str dyn))))
}
# (note-path e)            the sound file of a note event, on disk
function note-path (e) (db-path (get e 'db) (get e 'entry))
# (note-duration e)        how long the note's sound is, in seconds (after the shift)
function note-duration (e) {
    var w (load-sound (note-path e))
    return (/ (* (length (head (getidx w 1))) (pow 2 (/ (- 0 (get e 'shift)) 12))) (head w))
}

# --- transformations: new events, the score untouched, or in place ------------------------------------
# (score-shift! s dt) (score-scale! s f)   every event moved by dt seconds; every onset and duration multiplied by f
function score-shift! (s dt) (each (get s 'events) (function (e) (put! e 'at (max 0 (+ (get e 'at) dt)))))
function score-scale! (s f) (each (get s 'events) (function (e) { put! e 'at (* f (get e 'at))
                                                                   put! e 'dur (* f (get e 'dur)) }))
# (score-select s f)       the events for which f holds
function score-select (s f) (filter (get s 'events) f)
# (score-sort! s)          the events ordered by onset
function score-sort! (s) (put! s 'events (sort-by (get s 'events) (function (e) (get e 'at))))
# (score-remove! s e)      an event taken out
function score-remove! (s e) (put! s 'events (reject (get s 'events) (function (x) (== (get x 'id) (get e 'id)))))
# (score-merge s1 s2)      a new score with the events of both (s2's after s1's end when shifted with score-shift!)
function score-merge (s1 s2) {
    var out (score (concat (get s1 'name) "+" (get s2 'name)) (get s1 'sr))
    each (concat-list (get s1 'events) (get s2 'events)) (function (e) (push (get out 'events) (map e (function (p) (list (head p) (last p))))))
    return out
}
# (score-print s)          the events, one line each
function score-print (s) {
    print "score" (get s 'name) ":" (length (get s 'events)) "events," (fixed (score-duration s) 3) "s at" (get s 'sr) "Hz"
    each (sort-by (get s 'events) (function (e) (get e 'at))) (function (e) (print "  " (fixed (get e 'at) 3) (fixed (get e 'dur) 3) (get e 'kind) (get e 'label) (if (or (!= (get e 'az) 0) (!= (get e 'el) 0)) (concat "@" (str (get e 'az)) "," (str (get e 'el))) "")))
}
