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
function score (name sr) (record (list 'name name 'sr sr 'events (list) 'layout "stereo" 'reverb (list 0.7 0.3) 'bpm 60))
# (score-tempo! s bpm) (score-bpm s)   the score's tempo, for times written in beats (60 by default: a beat is a second)
function score-tempo! (s bpm) (put! s 'bpm bpm)
function score-bpm (s) (opt s 'bpm 60)
# (beat->sec s b)          a time in beats of the score as seconds
function beat->sec (s b) (* b (/ 60 (score-bpm s)))
# (score-reverb! s dry wet)   how much of the score is heard dry and through the hall when it is played (0.7 and 0.3
#                          by default; (score-reverb! s 1 0) plays it dry)
function score-reverb! (s dry wet) (put! s 'reverb (list dry wet))
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
    if (equal? (get e 'kind) 'score) {
        if (equal? (get e 'source) s) { error "event: a score cannot contain itself" }
        if (== dur 0) { set dur (+ (score-duration (get e 'source)) 0.6) }        # 0: the whole sub-score, with its releases
    }
    put! e 'at at
    put! e 'dur dur
    put! e 'az az
    put! e 'el el
    if (not (has? e 'gain)) { put! e 'gain 1 }                               # a payload may carry its gain (fragment-gain)
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
        if (has? what 'events) { return (record (list 'kind 'score 'source what 'label (concat "score " (get what 'name)))) }   # a score inside a score
        if (has? what 'kind) { return (map what (function (p) (list (head p) (last p)))) }   # a note, chord, instrument or call record: copied
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
# (note-sound e sr)        a note's channels at a rate: the file's, shifted by resampling when the pitch was not
#                          the recording's; only what the event's duration needs is resampled (a little more for
#                          the fade), and the result is kept in the cache under file, shift, rate and length
var note-cache (list)
function note-sound (e sr) {
    var path (note-path e)
    var shift (+ (opt e 'shift 0) (/ (opt e 'cents 0) 100))          # semitones, the cents included
    var need (+ 0.2 (get e 'dur))                                    # seconds of output wanted
    var key (concat path "@" (str shift) "@" (str sr) "@" (str (ceil need)))
    var hit (opt note-cache key nil)
    if (not (equal? (type hit) "nil")) { return hit }
    var w (load-sound path)
    var ratio (pow 2 (/ (- 0 shift) 12))                             # output length / input length
    var src-len (min (length (head (getidx w 1))) (ceil (* (ceil need) (head w) (/ 1 ratio))))
    var chans (map (getidx w 1) (function (c) (take c src-len)))
    set chans (to-rate chans (head w) sr)
    if (!= shift 0) { set chans (map chans (function (c) (resample c ratio))) }
    push note-cache (list key chans)
    if (> (length note-cache) 400) { set note-cache (drop note-cache 100) }        # a bounded cache
    return chans
}
# (clear-sound-cache)      forget the loaded files and the notes made from them
function clear-sound-cache () { set sound-cache (list)
                                 set note-cache (list) }
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
    if (equal? kind 'note) { set chans (note-sound e sr) }
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
    if (equal? kind 'chord) {                                            # the notes summed, each cut to the duration
        var parts (map (get e 'notes) (function (n) (head (render-event (put (put n 'dur (get e 'dur)) 'gain 1) sr))))
        var n (max-of (map parts length))
        set chans (list (reduce parts (function (acc p) (+ acc (vec p (zeros (- n (length p)))))) (zeros n)))
    }
    if (equal? kind 'score) {                                            # a score inside: rendered in stereo (score-render places it in any layout itself)
        set chans (score-render-at-depth (get e 'source) "stereo" sr (+ 1 render-depth))
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
# (score-render s layout)  the whole score as channels at its rate; a score placed as an event is rendered into the
#                          same layout (its own positions kept), cut to the event's duration with a fade
var render-depth 0
function score-render (s layout) (score-render-at-depth s layout (get s 'sr) 0)
function score-render-at-depth (s layout sr depth) {
    if (> depth 16) { error "score-render: scores nested more than 16 deep (a score inside itself?)" }
    set render-depth depth
    var nch (layout-channels layout)
    var total (max 1 (+ (floor (* (score-duration s) sr)) (floor (* 0.6 sr))))    # room for the last event's release and fades
    var out (map (vec->list (range nch)) (function (k) (zeros total)))
    each (get s 'events) (function (e) {
        var placed (if (equal? (get e 'kind) 'score)
                        (map (score-render-at-depth (get e 'source) layout sr (+ depth 1)) (function (c) (* (get e 'gain) (fit-duration c (get e 'dur) sr))))
                        (place (render-event e sr) layout (get e 'az) (get e 'el) sr))
        set render-depth depth
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
# (play-score s gain)      play the score now (the device is opened if needed) and wait until it ends or is stopped
#                          (stop-score): the whole score is rendered in stereo, put in the concert hall (see
#                          score-reverb!) and played as one buffer; gain scales it; the roll's cursor follows
function play-score (s gain) (play-score-from s gain 0)
# (play-score-from s gain from)   the same from a time in seconds
function play-score-from (s gain from) {
    score-play-now s gain from
    while (head (playhead)) { sleep 0.05 }
    return nil
}
# (score-play-now s gain from)   start playing and return at once (what the roll's Play button does); => the end time on
#                          the clock. stop-score stops it.
function score-play-now (s gain from) {
    if (not (opt (audio-status) "open" 0)) { audio-init }
    var sr (audio-sr)
    var hall (score-hall-mix s sr)
    var peak (max-of (map hall (function (c) (max (abs c)))))
    var norm (if (> (* gain peak) 0.98) (/ 0.98 peak) gain)                  # never clip: scaled down when the hall's tail pushes it over
    var skip (floor (* from sr))
    var out (map hall (function (c) (* norm (drop c (min skip (length c))))))
    var t0 (+ (audio-time) 0.1)
    play-buffer out sr 1 0 1 0 t0
    var end (+ t0 (/ (length (head out)) sr))
    playhead! t0 from end (register-score s)                                  # only this score's roll follows
    return end
}
# (score-hall-mix s sr)    the score rendered in stereo and put in the hall, at a rate; kept in the score (its
#                          'cache) with a signature of the events, so playing again without a change is instant
function score-hall-mix (s sr) {
    var sig (concat (str sr) " " (str (opt s 'reverb (list 0.7 0.3))) " " (str (map (get s 'events) (function (e) (list (get e 'id) (get e 'at) (get e 'dur) (get e 'gain) (get e 'az) (get e 'el))))))
    var cached (opt s 'cache nil)
    if (and (not (equal? (type cached) "nil")) (equal? (head cached) sig)) { return (last cached) }
    var rv (opt s 'reverb (list 0.7 0.3))
    var hall (concerthall (score-render-at s "stereo" sr) sr (head rv) (last rv))
    put! s 'cache (list sig hall)
    return hall
}
# (score-clear-cache! s)   forget the rendered mix (an event's sound changed in a way the signature cannot see)
function score-clear-cache! (s) (put! s 'cache nil)
# (stop-score)             stop whatever score is playing
function stop-score () {
    if (opt (audio-status) "open" 0) { stop-all }
    playhead-off!
    return nil
}
# (score-render-at s layout sr)   the score rendered at a rate other than its own (the device's, when playing)
function score-render-at (s layout sr) (score-render-at-depth s layout sr 0)
# (score-schedule s gain from)   the live way: every event scheduled on the clock (synths compiled and gated, the rest
#                          rendered), no hall; returns at once => (list synth-ids end-clock-time start-clock-time);
#                          free the synths when it is over
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
    var evs (roll-events s)
    var notes (filter evs (function (e) (equal? (get e 'kind) 'note)))
    var by-instr (group-by notes (function (e) (get e 'instr)))
    var instr-rows (sort-by by-instr (function (g) (instrument-rank (head g))))
    var rows (map instr-rows (function (g) (list (head g) (last g))))
    each (list (list 'score "scores") (list 'file "files") (list 'buffer "buffers") (list 'synth "synths") (list 'call "calls")) (function (k) {
        var of-kind (filter evs (function (e) (equal? (get e 'kind) (head k))))
        if (> (length of-kind) 0) { push rows (list (last k) of-kind) }
    })
    return rows
}
# (roll-events s)          the events as the roll shows them: a chord becomes one note per pitch (same id and time)
function roll-events (s) {
    var out (list)
    each (get s 'events) (function (e) {
        if (equal? (get e 'kind) 'chord) { each (get e 'notes) (function (n) (push out (put (put (put n 'at (get e 'at)) 'dur (get e 'dur)) 'id (get e 'id)))) } { push out e }
    })
    return out
}
# (score-roll s)           the roll as a figure: rows (name and clef) and bars (row start dur label tip group lane lanes
#                          midi dyn id tech cents); pitched events sit on their row's staff, the others on a line;
#                          the technique is written above a note where it changes, the tuning in cents when not zero
function score-roll (s) {
    var rows (score-rows s)
    var groups (list 'note 'file 'buffer 'synth 'call 'score)
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
            push bars (list r (get e 'at) (get e 'dur) (get e 'label) tip (find groups kind) (last pair) nlanes (opt e 'midi -1) (opt e 'dyn "") (get e 'id) (opt e 'tech "") (opt e 'cents 0))
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
# (roll-play score-id from) start a displayed score from a time (the roll's Play button); (roll-stop) stops it;
#   (roll-render score-id path) the score in the hall as a WAV (the roll's Render button)
function roll-play (score-id from) (score-play-now (displayed-score score-id) 1 from)
function roll-stop () (stop-score)
function roll-render (score-id path) (render-hall (displayed-score score-id) path)
# (render-hall s path)     the score rendered in stereo through the concert hall (score-reverb!), written as a WAV,
#                          scaled so that it never clips; => the channels
function render-hall (s path) {
    var sr (get s 'sr)
    var hall (score-hall-mix s sr)
    var peak (max-of (map hall (function (c) (max (abs c)))))
    var out (if (> peak 0.98) (map hall (function (c) (* (/ 0.98 peak) c))) hall)
    write-wav path sr out
    print "rendered" path "(in the hall)"
    return out
}
# (play-event score-id event-id)   play one event of a displayed score, alone (a double-click in the roll does this)
function play-event (score-id event-id) {
    var s (displayed-score score-id)
    var e (find-first (get s 'events) (function (x) (== (get x 'id) event-id)))
    if (equal? (type e) "nil") { error "play-event: no event " event-id }
    if (equal? (get e 'kind) 'score) { return (display (get e 'source)) }              # a score inside: its own roll opens
    if (not (opt (audio-status) "open" 0)) { audio-init }
    var sr (audio-sr)
    if (equal? (get e 'kind) 'synth) {
        var id (synth (get e 'source))
        each (get e 'params) (function (p) (set-param id (head p) (last p)))
        note id (get e 'dur)
        return nil
    }
    var rv (opt s 'reverb (list 0.7 0.3))
    var hall (concerthall (place (render-event e sr) "stereo" (get e 'az) (get e 'el) sr) sr (head rv) (last rv))
    var peak (max-of (map hall (function (c) (max (abs c)))))
    play-buffer hall sr (if (> peak 0.98) (/ 0.98 peak) 1) 0 1 0 (+ (audio-time) 0.05)
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
#                          R resets; hovering shows an event; a double-click plays it; Play (or Space) plays the
#                          score from the cursor (a click in the background places it), Stop stops; the cursor
#                          follows any play-score
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
function db-instruments-available (db) (sort-by (keys (db-index! db)) instrument-rank)
# (db-techniques-of db instr)   the techniques an instrument was recorded with (on disk); (db-dynamics-of db instr) its dynamics
function db-techniques-of (db instr) {
    var slot (opt (db-index! db) (str instr) nil)
    return (if (equal? (type slot) "nil") (list) (sort-list (unique (map (get slot 'all) (function (e) (get e 'tech))))))
}
function db-dynamics-of (db instr) {
    var slot (opt (db-index! db) (str instr) nil)
    return (if (equal? (type slot) "nil") (list) (unique (map (get slot 'all) (function (e) (get e 'dyn)))))
}
# (db-range db instr)      the lowest and highest MIDI note of an instrument, as (list lo hi)
function db-range (db instr) {
    var ms (vec (map (filter (db-query db instr nil nil nil) (function (e) (>= (get e 'midi) 0))) (function (e) (get e 'midi))))
    if (== (length ms) 0) { error "db-range: no pitched sounds of " instr }
    return (list (min ms) (max ms))
}
# (db-range-available db instr)   the range of the sounds actually on disk, from the index (fast; what the
#                          orchestrators use)
function db-range-available (db instr) {
    var slot (opt (db-index! db) (str instr) nil)
    if (equal? (type slot) "nil") { error "db-range-available: no pitched sound of " instr " on disk" }
    var ms (vec (map (get slot 'all) (function (e) (get e 'midi))))
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

# --- an index of the database, built once: what a note can choose from -----------------------------------
# (db-index! db)           builds (if not yet) the index: for every instrument, its available pitched entries,
#                          grouped by dynamics and technique; note and db-range read it, so a large database
#                          (FullSOL has tens of thousands of entries) costs a scan once, not per note
function db-index! (db) {
    if (has? db 'index) { return (get db 'index) }
    var by-instr (list)
    each (get db 'entries) (function (e) {
        if (and (>= (get e 'midi) 0) (db-available? db e)) {
            var i (get e 'instr)
            if (not (has? by-instr i)) { put! by-instr i (record (list 'all (list) 'by (list))) }
            var slot (get by-instr i)
            push (get slot 'all) e
            var key (concat (get e 'dyn) "|" (get e 'tech))
            if (has? (get slot 'by) key) { push (get (get slot 'by) key) e } { put! (get slot 'by) key (list e) }
        }
    })
    put! db 'index by-instr
    return by-instr
}
# (db-candidates db instr dyn tech)   the available pitched entries of an instrument with these dynamics and
#                          technique; failing that, with the technique; failing that, any of the instrument
function db-candidates (db instr dyn tech) {
    var idx (db-index! db)
    var slot (opt idx (str instr) nil)
    if (equal? (type slot) "nil") { return (list) }
    var exact (opt (get slot 'by) (concat (str dyn) "|" (str tech)) nil)
    if (not (equal? (type exact) "nil")) { return exact }
    var same-tech (filter (get slot 'all) (function (e) (equal? (get e 'tech) (str tech))))
    if (> (length same-tech) 0) { return same-tech }
    return (get slot 'all)
}

# --- notes: sounds from a database as events --------------------------------------------------------
# (note db instr pitch dyn tech)   a note event (pitch a name, "D#5", or a MIDI number): the entry matching instrument, pitch, dynamics and technique, or
#                          the nearest available pitch of that instrument (same dynamics and technique when they
#                          exist), shifted by resampling to the pitch asked for. Only entries whose sound is on
#                          disk are candidates.
function note (db instr pitch dyn tech) {
    var want (if (equal? (type pitch) "scalar") (round pitch) (pitch->midi (str pitch)))
    if (< want 0) { error "note: not a pitch: " pitch }
    var pname (midi->pitch want)
    var same (db-candidates db instr dyn tech)
    if (== (length same) 0) { error "note: no sound of " instr " on disk in this database" }
    var exact (filter same (function (e) (== (get e 'midi) want)))
    var entry (if (> (length exact) 0) (head exact) (min-by same (function (e) (abs (- (get e 'midi) want)))))
    var shift (- want (get entry 'midi))
    return (record (list 'kind 'note 'source (get entry 'file) 'db db 'entry entry 'instr (str instr) 'pitch pname 'midi want
                         'dyn (str dyn) 'tech (str tech) 'shift shift 'cents 0 'label (concat (str instr) " " pname " " (str dyn))))
}
# (note-cents! n cents)    a note's tuning away from its pitch, in cents (shown in the roll; used by the orchestrators
#                          that know the target's exact frequencies); the shift by resampling follows it
function note-cents! (n cents) { put! n 'cents cents
                                 return n }
# (note-path e)            the sound file of a note event, on disk
function note-path (e) (db-path (get e 'db) (get e 'entry))
# (note-duration e)        how long the note's sound is, in seconds (after the shift)
function note-duration (e) {
    var w (load-sound (note-path e))
    return (/ (* (length (head (getidx w 1))) (pow 2 (/ (- 0 (get e 'shift)) 12))) (head w))
}

# --- musical elements: pitches, chords, rhythms, lines, and fragments to place ---------------------------
# Pitches are names ("C4", "F#3": a sharp needs a string) or MIDI numbers; nil is a rest. A rhythm is a list of
# durations in seconds (negative: a pause). An element makes a *fragment*: a list of (list at dur payload), times
# from 0, which (add! s at fragment) places into a score, (fragment->score name sr fragment) turns into a score of
# its own, and (fragment-shift / fragment-scale / fragment-map ...) transform. Elements take a database and an
# instrument (or a list of instruments, one per voice, cycled), dynamics and technique, as note does.
# (pitch->name p)          a pitch as a name, whatever was given
function pitch->name (p) (if (equal? (type p) "scalar") (midi->pitch p) (str p))
# (pitch->number p)        a pitch as a MIDI number
function pitch->number (p) (if (equal? (type p) "scalar") p (pitch->midi (str p)))
# (rhythm d ...)           durations in seconds; negative ones are pauses: (rhythm 0.5 0.5 -0.25 1)
function rhythm (ds) ds
# (beats bpm r)            a rhythm given in beats, at a tempo
function beats (bpm r) (map r (function (d) (* d (/ 60 bpm))))
# (rhythm-duration r)      how long a rhythm lasts (pauses count)
function rhythm-duration (r) (sum-by r abs)
# (chord db instr dyn tech pitches)   a chord payload: the pitches sounding together on one instrument (as many
#                          notes, each resolved by note); place it with event, or in a fragment
function chord (db instr dyn tech pitches) {
    var ns (map (reject pitches (function (p) (equal? (type p) "nil"))) (function (p) (note db instr (pitch->name p) dyn tech)))
    return (record (list 'kind 'chord 'notes ns 'instr (str instr) 'dyn (str dyn) 'label (concat (str instr) " chord " (join (map ns (function (n) (get n 'pitch))) " "))))
}
# (notes db instr dyn tech pitches rhythm)   a line: the pitches one after another with the rhythm's durations; the
#                          shorter of the two repeats until the longer ends; => a fragment
function notes (db instr dyn tech pitches r) (line-of pitches r (function (p) (note db instr (pitch->name p) dyn tech)))
# (chords db instr dyn tech chord-list rhythm)   chords (lists of pitches) one after another, with the rhythm
function chords (db instr dyn tech chord-list r) (line-of chord-list r (function (ps) (chord db instr dyn tech ps)))
# (line-of items rhythm maker)   the shared engine of notes and chords: items and durations cycled to the longer, maker
#                          turns an item into a payload (nil items and negative durations are rests)
function line-of (items r maker) {
    if (or (== (length items) 0) (== (length r) 0)) { return (list) }
    var n (max (length items) (length r))
    var out (list)
    var t 0
    each (range n) (function (k) {
        var d (getidx r (mod k (length r)))
        var item (getidx items (mod k (length items)))
        if (and (> d 0) (not (equal? (type item) "nil"))) { push out (list t d (maker item)) }
        set t (+ t (abs d))
    })
    return out
}
# (fragment-duration f) (fragment-shift f dt) (fragment-scale f k) (fragment-map f g)   fragments as data: their length,
#                          moved in time, stretched (times and durations by k), or every (at dur payload) triple through g
function fragment-duration (f) (if (== (length f) 0) 0 (max-of (map f (function (x) (+ (head x) (getidx x 1))))))
function fragment-shift (f dt) (map f (function (x) (list (+ (head x) dt) (getidx x 1) (last x))))
function fragment-scale (f k) (map f (function (x) (list (* k (head x)) (* k (getidx x 1)) (last x))))
function fragment-map (f g) (map f g)
# (fragment-repeat f times)   the fragment played times in a row
function fragment-repeat (f times) {
    var d (fragment-duration f)
    var out (list)
    each (range times) (function (k) (each (fragment-shift f (* k d)) (function (x) (push out x))))
    return out
}
# (fragment-until f secs)  the fragment repeated until secs (the last events cut off), for textures
function fragment-until (f secs) {
    var d (fragment-duration f)
    if (<= d 0) { return f }
    return (filter (fragment-repeat f (ceil (/ secs d))) (function (x) (< (head x) secs)))
}
# (add! s at fragment)     place a fragment's events into a score at a time; => the events
function add! (s at f) (map f (function (x) {
    var pl (last x)
    var rec? (and (equal? (type pl) "list") (> (length pl) 0) (equal? (type (head pl)) "list"))
    return (event-at s (+ at (head x)) (getidx x 1) pl (if rec? (opt pl 'az 0) 0) (if rec? (opt pl 'el 0) 0))
}))
# (add-at! s at fragment az el)   the same, placed at a direction
function add-placed! (s at f az el) (map f (function (x) (event-at s (+ at (head x)) (getidx x 1) (last x) az el)))
# (fragment->score name sr fragment)   a score of its own, to place inside another with event
function fragment->score (name sr f) {
    var s (score name sr)
    add! s 0 f
    return s
}
# (texture db instrs dyn tech pitches rhythm scales)   micropolyphony: one voice per scale factor, each the same line
#                          with the rhythm multiplied by its factor, every voice repeating its line until the slowest
#                          has played it once; instruments cycled over the voices; => a fragment
function texture (db instrs dyn tech pitches r scales) {
    var ilist (if (equal? (type instrs) "list") instrs (list instrs))
    var voices (map (zip scales (vec->list (range (length scales)))) (function (p) (notes db (getidx ilist (mod (last p) (length ilist))) dyn tech pitches (map r (function (d) (* d (head p)))))))
    var longest (max-of (map voices fragment-duration))
    return (reduce (map voices (function (v) (fragment-until v longest))) concat-list (list))
}
# (pivots db instrs dyn tech chord interval rhythm)   as many lines as the chord has pitches, each wandering at random
#                          within +- interval semitones of its pitch, one note per duration of the rhythm; => a fragment
function pivots (db instrs dyn tech chord-pitches interval r) {
    var ilist (if (equal? (type instrs) "list") instrs (list instrs))
    var out (list)
    each (zip chord-pitches (vec->list (range (length chord-pitches)))) (function (p) {
        var centre (pitch->number (head p))
        var instr (getidx ilist (mod (last p) (length ilist)))
        var ps (map (vec->list (range (length r))) (function (k) (+ centre (round (* interval (- (* 2 (rand)) 1))))))
        each (notes db instr dyn tech ps r) (function (x) (push out x))
    })
    return out
}
# (chordinterp db instr dyn tech chord1 chord2 rhythm)   from one chord to another in as many steps as the rhythm has
#                          durations, each voice moving by rounded semitone steps (the shorter chord's voices held); => a fragment
function chordinterp (db instr dyn tech c1 c2 r) {
    var n (max (length c1) (length c2))
    var a (map (vec->list (range n)) (function (k) (pitch->number (getidx c1 (min k (- (length c1) 1))))))
    var b (map (vec->list (range n)) (function (k) (pitch->number (getidx c2 (min k (- (length c2) 1))))))
    var steps (length r)
    var chord-list (map (vec->list (range steps)) (function (k) {
        var u (if (<= steps 1) 1 (/ k (- steps 1)))
        return (map (zip a b) (function (p) (round (+ (head p) (* u (- (last p) (head p)))))))
    }))
    return (chords db instr dyn tech chord-list r)
}
# (transpose-pitches pitches n)   pitches n semitones higher (rests kept); (invert pitches axis) mirrored around a pitch
function transpose-pitches (pitches n) (map pitches (function (p) (if (equal? (type p) "nil") nil (+ (pitch->number p) n))))
function invert (pitches axis) (map pitches (function (p) (if (equal? (type p) "nil") nil (- (* 2 (pitch->number axis)) (pitch->number p)))))
# (scale-pitches root mode degrees)   pitches of a scale: root a pitch, mode 'major 'minor 'dorian 'phrygian 'lydian
#                          'mixolydian 'aeolian 'locrian 'chromatic 'whole 'pentatonic, degrees a list of scale steps (0 = root)
function scale-pitches (root mode degrees) {
    var steps (get (list (list 'major (list 0 2 4 5 7 9 11)) (list 'minor (list 0 2 3 5 7 8 10)) (list 'dorian (list 0 2 3 5 7 9 10)) (list 'phrygian (list 0 1 3 5 7 8 10))
                         (list 'lydian (list 0 2 4 6 7 9 11)) (list 'mixolydian (list 0 2 4 5 7 9 10)) (list 'aeolian (list 0 2 3 5 7 8 10)) (list 'locrian (list 0 1 3 5 6 8 10))
                         (list 'chromatic (list 0 1 2 3 4 5 6 7 8 9 10 11)) (list 'whole (list 0 2 4 6 8 10)) (list 'pentatonic (list 0 2 4 7 9))) mode)
    var r (pitch->number root)
    return (map degrees (function (d) (+ r (* 12 (floor (/ d (length steps)))) (getidx steps (mod d (length steps))))))
}

# (add-beats! s beat fragment)   a fragment whose times are in beats, placed at a beat of the score (its tempo)
function add-beats! (s beat f) (add! s (beat->sec s beat) (fragment-scale f (/ 60 (score-bpm s))))
# (fragment-gain f from to)   the events' gains ramped from one value to another along the fragment (a crescendo)
function fragment-gain (f from to) {
    var d (max 1e-9 (fragment-duration f))
    return (map f (function (x) (list (head x) (getidx x 1) (put (last x) 'gain (+ from (* (- to from) (/ (head x) d)))))))
}
# (fragment-dynamics db f dyns)   the notes of a fragment re-resolved with dynamics stepping through a list along it
function fragment-dynamics (db f dyns) {
    var d (max 1e-9 (fragment-duration f))
    return (map f (function (x) {
        var pl (last x)
        if (not (equal? (get pl 'kind) 'note)) { return x }
        var dyn (getidx dyns (min (- (length dyns) 1) (floor (* (length dyns) (/ (head x) d)))))
        return (list (head x) (getidx x 1) (note db (get pl 'instr) (get pl 'midi) dyn (get pl 'tech)))
    }))
}
# (fragment-articulate f fraction)   every event sounding for a fraction of its duration (staccato at 0.3)
function fragment-articulate (f fraction) (map f (function (x) (list (head x) (* fraction (getidx x 1)) (last x))))
# (fragment-thin f p)      each event kept with probability p
function fragment-thin (f p) (filter f (function (x) (< (rand) p)))
# (fragment-density f curve)   events kept with a probability read along the fragment from a curve (a list, 0..1)
function fragment-density (f curve) {
    var d (max 1e-9 (fragment-duration f))
    return (filter f (function (x) (< (rand) (getidx curve (min (- (length curve) 1) (floor (* (length curve) (/ (head x) d))))))))
}
# (fragment-place f az el) the events placed at a direction (kept when added with add!)
function fragment-place (f az el) (map f (function (x) (list (head x) (getidx x 1) (put (put (last x) 'az az) 'el el))))
# (fragment-snap f field)  the notes snapped to the nearest pitch of a field (a list of pitches), re-resolved
function fragment-snap (db f field) (map f (function (x) {
    var pl (last x)
    if (not (equal? (get pl 'kind) 'note)) { return x }
    return (list (head x) (getidx x 1) (note db (get pl 'instr) (snap-to-field (get pl 'midi) field) (get pl 'dyn) (get pl 'tech)))
}))
# --- pitches: fields, voicings, series, spectra ---
# (snap-to-field p field)  the pitch of the field nearest to p
function snap-to-field (p field) (min-by (map field pitch->number) (function (q) (abs (- q (pitch->number p)))))
# (pitch-field root mode octaves)   every pitch of a scale over some octaves from the root, as a field to snap to
function pitch-field (root mode octaves) (scale-pitches root mode (vec->list (range (* 7 octaves))))
# (chord-voicing pitches mode)   a chord revoiced: 'close (as given, sorted), 'open (every other voice up an octave),
#                          'drop2 (the second from the top down an octave), 'spread (each voice an octave above the
#                          previous), (list 'invert n) (n inversions: the lowest voices up an octave)
function chord-voicing (pitches mode) {
    var ps (sort-list (map pitches pitch->number))
    if (equal? mode 'close) { return ps }
    if (equal? mode 'open) { return (sort-list (map (zip ps (vec->list (range (length ps)))) (function (p) (if (odd? (last p)) (+ 12 (head p)) (head p))))) }
    if (equal? mode 'drop2) { if (< (length ps) 2) { return ps }
                              var k (- (length ps) 2)
                              return (sort-list (concat-list (list (- (getidx ps k) 12)) (remove-at ps k))) }
    if (equal? mode 'spread) { return (map (zip ps (vec->list (range (length ps)))) (function (p) (+ (head p) (* 12 (last p))))) }
    if (and (equal? (type mode) "list") (equal? (head mode) 'invert)) {
        var out ps
        each (range (last mode)) (function (k) (set out (sort-list (concat-list (tail out) (list (+ 12 (head out)))))))
        return out
    }
    error "chord-voicing: unknown mode " mode
}
# (rotate L n) (retrograde L) (cycle L n) (interleave a b)   series as lists: turned by n, backwards, n elements round and round, alternated
function rotate (L n) (concat-list (drop L (mod n (length L))) (take L (mod n (length L))))
function retrograde (L) (reverse L)
function cycle (L n) (map (vec->list (range n)) (function (k) (getidx L (mod k (length L)))))
function interleave (a b) {
    var out (list)
    each (range (max (length a) (length b))) (function (k) { if (< k (length a)) { push out (getidx a k) }
                                                             if (< k (length b)) { push out (getidx b k) } })
    return out
}
# (pitches-from-spectrum x sr n)   the n strongest partials of a sound as MIDI pitches, low to high: a chord from a
#                          spectrum, the way spectral music builds one (a harmonic sound gives its harmonic series)
function pitches-from-spectrum (x sr n) {
    var size (min 8192 (pow 2 (floor (log2 (length x)))))
    var mags (take (magnitude-spectrum (* (take x size) (hann size))) (/ size 2))
    var freqs (* (range (/ size 2)) (/ sr size))
    var peaks (filter (vec->list (local-maxima mags)) (function (k) (> (getidx freqs k) 25)))
    var strongest (take (sort-by peaks (function (k) (- 0 (getidx mags k)))) n)
    return (sort-list (map strongest (function (k) (round (hz->midi (getidx freqs k))))))
}
# (harmonic-series fundamental n)   the first n partials of a fundamental (a pitch) as MIDI pitches, rounded
function harmonic-series (fundamental n) (map (vec->list (range 1 (+ n 1))) (function (k) (round (hz->midi (* k (midi->hz (pitch->number fundamental)))))))
# --- rhythms: patterns, tuplets, polyrhythms ---
# (rhythm-from-pattern "x..x.x" step)   a rhythm from a string: x an attack, . a continuation of the step, each step lasting step seconds
function rhythm-from-pattern (pattern step) {
    var out (list)
    each (split pattern "") (function (c) (if (equal? c "x") (push out step) (if (> (length out) 0) (setidx out (- (length out) 1) (+ (last out) step)))))
    return out
}
# (rhythm-augment r k) (rhythm-diminish r k)   every duration multiplied, divided by k
function rhythm-augment (r k) (map r (function (d) (* d k)))
function rhythm-diminish (r k) (map r (function (d) (/ d k)))
# (tuplet n total)         n equal durations filling total seconds (a triplet: (tuplet 3 1))
function tuplet (n total) (map (vec->list (range n)) (function (k) (/ total n)))
# (polyrhythm ra rb)       the two rhythms extended to a common length (each repeated), => (list ra' rb')
function polyrhythm (ra rb) {
    var da (rhythm-duration ra)
    var db (rhythm-duration rb)
    var na (round (/ (lcm-of (* 1000 da) (* 1000 db)) (* 1000 da)))
    var nb (round (/ (lcm-of (* 1000 da) (* 1000 db)) (* 1000 db)))
    return (list (cycle ra (* na (length ra))) (cycle rb (* nb (length rb))))
}
function lcm-of (a b) (/ (* a b) (gcd-of a b))
function gcd-of (a b) (if (< (abs b) 0.5) (abs a) (gcd-of b (mod a b)))
# --- more lines: walks, staggered textures, arpeggios, interpolation variants ---
# (walk db instrs dyn tech starts step-max rhythm)   random walks: one line per start pitch, each note within step-max
#                          semitones of the previous one (pivots stay near a centre; a walk drifts)
function walk (db instrs dyn tech starts step-max r) {
    var ilist (if (equal? (type instrs) "list") instrs (list instrs))
    var out (list)
    each (zip starts (vec->list (range (length starts)))) (function (p) {
        var here (pitch->number (head p))
        var ps (list)
        each (range (length r)) (function (k) { push ps here
                                                set here (+ here (round (* step-max (- (* 2 (rand)) 1)))) })
        each (notes db (getidx ilist (mod (last p) (length ilist))) dyn tech ps r) (function (x) (push out x))
    })
    return out
}
# (texture-staggered db instrs dyn tech pitches rhythm scales entries)   a texture whose voices enter one after another
#                          (entries: a delay in seconds per voice), each repeating until the last has played once
function texture-staggered (db instrs dyn tech pitches r scales entries) {
    var ilist (if (equal? (type instrs) "list") instrs (list instrs))
    var voices (map (vec->list (range (length scales))) (function (k) (fragment-shift (notes db (getidx ilist (mod k (length ilist))) dyn tech pitches (map r (function (d) (* d (getidx scales k))))) (getidx entries (mod k (length entries))))))
    var longest (max-of (map voices fragment-duration))
    return (reduce (map voices (function (v) (fragment-until v longest))) concat-list (list))
}
# (arpeggio db instr dyn tech pitches spread mode dur)   the chord's notes one after another spread seconds apart,
#                          'up 'down 'updown or 'random, each held for dur; => a fragment
function arpeggio (db instr dyn tech pitches spread mode dur) {
    var ps (map pitches pitch->number)
    var order (if (equal? mode 'down) (reverse (sort-list ps)) (if (equal? mode 'updown) (concat-list (sort-list ps) (reverse (take (sort-list ps) (- (length ps) 1)))) (if (equal? mode 'random) (shuffle ps) (sort-list ps))))
    return (map (zip order (vec->list (range (length order)))) (function (p) (list (* spread (last p)) dur (note db instr (head p) dyn tech))))
}
# (chordinterp-ease db instr dyn tech c1 c2 rhythm exponent)   chord interpolation with a curve: exponent 1 linear, > 1
#                          slow then fast, < 1 fast then slow
function chordinterp-ease (db instr dyn tech c1 c2 r exponent) {
    var n (max (length c1) (length c2))
    var a (map (vec->list (range n)) (function (k) (pitch->number (getidx c1 (min k (- (length c1) 1))))))
    var b (map (vec->list (range n)) (function (k) (pitch->number (getidx c2 (min k (- (length c2) 1))))))
    var steps (length r)
    var chord-list (map (vec->list (range steps)) (function (k) {
        var u (pow (if (<= steps 1) 1 (/ k (- steps 1))) exponent)
        return (map (zip a b) (function (p) (round (+ (head p) (* u (- (last p) (head p)))))))
    }))
    return (chords db instr dyn tech chord-list r)
}
# (chordinterp-sets db instr dyn tech c1 c2 rhythm)   interpolation between pitch sets: every voice of the first
#                          chord goes to the nearest free pitch of the second (no crossing by index), then as chordinterp
function chordinterp-sets (db instr dyn tech c1 c2 r) {
    var a (sort-list (map c1 pitch->number))
    var free (sort-list (map c2 pitch->number))
    var target (list)
    each a (function (p) {
        if (== (length free) 0) { push target p } {
            var q (min-by free (function (t) (abs (- t p))))
            push target q
            set free (reject free (function (t) (== t q)))
        }
    })
    return (chordinterp db instr dyn tech a target r)
}
# (texture-on-chords db instrs dyn tech chord-list rhythm scales)   a texture whose line runs through the chords in
#                          turn (each chord's pitches, then the next), so the micropolyphony moves from one harmony to
#                          another: the chord list can come from chordinterp's chords
function texture-on-chords (db instrs dyn tech chord-list r scales) (texture db instrs dyn tech (reduce chord-list concat-list (list)) r scales)
# (texture-on-pivots db instrs dyn tech chord interval rhythm scales)   the pivots' lines taken as a texture: every
#                          voice wanders around its chord pitch at its own speed
function texture-on-pivots (db instrs dyn tech chord-pitches interval r scales) {
    var ilist (if (equal? (type instrs) "list") instrs (list instrs))
    var voices (map (zip chord-pitches (vec->list (range (length chord-pitches)))) (function (p) {
        var centre (pitch->number (head p))
        var k (last p)
        var sc (getidx scales (mod k (length scales)))
        var ps (map (vec->list (range (length r))) (function (j) (+ centre (round (* interval (- (* 2 (rand)) 1))))))
        return (notes db (getidx ilist (mod k (length ilist))) dyn tech ps (map r (function (d) (* d sc))))
    }))
    var longest (max-of (map voices fragment-duration))
    return (reduce (map voices (function (v) (fragment-until v longest))) concat-list (list))
}
# (orchestrate-line db line rhythm registers dyn tech)   a line distributed over instruments by register: registers a
#                          list of (list instr lo hi); each pitch goes to the first instrument whose range holds it
function orchestrate-line (db line r registers dyn tech) (line-of line r (function (p) {
    var m (pitch->number p)
    var reg (find-first registers (function (g) (and (>= m (pitch->number (getidx g 1))) (<= m (pitch->number (last g))))))
    if (equal? (type reg) "nil") { set reg (min-by registers (function (g) (min (abs (- m (pitch->number (getidx g 1)))) (abs (- m (pitch->number (last g))))))) }
    return (note db (head reg) m dyn tech)
}))
# (score-map! s f)         every event through f (a function of the event record, returning a record), in place
function score-map! (s f) (put! s 'events (map (get s 'events) f))
# (score-instrument s instr)   the events of an instrument
function score-instrument (s instr) (score-select s (function (e) (equal? (opt e 'instr "") (str instr))))


# --- MIDI files as material ------------------------------------------------------------------------------
# (midi->fragment path db instr tech)   the notes of a MIDI file as a fragment on one instrument, velocities
#                          becoming dynamics (pp p mp mf f ff over 0..127); times as the file's tempo map says
function midi->fragment (path db instr tech) (midi-notes->fragment (get (midi-read path) 'notes) db instr tech)
# (midi->fragment-by-channel path db instrs tech)   the same with an instrument per MIDI channel (instrs a list; a
#                          channel beyond the list takes the last one), => one fragment
function midi->fragment-by-channel (path db instrs tech) {
    var ns (get (midi-read path) 'notes)
    var out (list)
    each ns (function (n) (each (midi-notes->fragment (list n) db (getidx instrs (min (get n 'channel) (- (length instrs) 1))) tech) (function (x) (push out x))))
    return out
}
# (velocity->dynamics v)   a MIDI velocity as one of pp p mp mf f ff
function velocity->dynamics (v) (getidx (list 'pp 'p 'mp 'mf 'f 'ff) (min 5 (floor (/ v 21.4))))
function midi-notes->fragment (ns db instr tech) (map ns (function (n) (list (get n 'at) (max 0.05 (get n 'dur)) (note db instr (get n 'pitch) (velocity->dynamics (get n 'velocity)) tech))))
# (midi->score path db instrs tech name)   a score from a MIDI file, an instrument per channel
function midi->score (path db instrs tech name) (fragment->score name 44100 (midi->fragment-by-channel path db instrs tech))

# --- orchestrations: results, segments, solutions, connections ----------------------------------------------
# Every orchestrator (orchestrate-granular here; orchestrate-mimetic and orchestrate-morphological to come) returns
# a *result*: a record with the method, its parameters, and segments; a segment covers a span of time and holds
# solutions, each a fragment (already at the segment's time) with a cost. A connection is one solution per segment
# joined into a fragment; the best connection takes the first solution of each.
# (orchestration method params segments)   a result record
function orchestration (method params segments) (record (list 'kind 'orchestration 'method method 'params params 'segments segments))
# (segment at dur solutions)   a segment: solutions a list of (list cost fragment), best first
function segment (at dur solutions) (record (list 'at at 'dur dur 'solutions (sort-by solutions head)))
# (solutions result k)     the solutions of segment k, best first, as (list cost fragment) pairs
function solutions (result k) (get (getidx (get result 'segments) k) 'solutions)
# (solution result k n)    the fragment of the n-th solution of segment k
function solution (result k n) (last (getidx (solutions result k) n))
# (connection result choices)   one solution per segment (choices: an index per segment) joined into a fragment; a
#                          note that continues across a boundary on the same instrument at the same pitch is merged
function connection (result choices) {
    var segs (get result 'segments)
    var out (list)
    each (zip segs choices) (function (p) (each (solution result (find segs (head p)) (last p)) (function (x) (push out x))))
    return (merge-continuations (sort-by out head))
}
# (best-connection result)   the first solution of every segment
function best-connection (result) (connection result (map (get result 'segments) (function (g) 0)))
# (connect! s at result choices)   a connection placed in a score
function connect! (s at result choices) (add! s at (connection result choices))
# (merge-continuations f)   in a fragment, a note starting where a note of the same instrument and pitch ends becomes
#                          the continuation of that note (one longer event), as an orchestration's connection does
function merge-continuations (f) {
    var out (list)
    each f (function (x) {
        var pl (last x)
        var prev (if (equal? (get pl 'kind) 'note) (find-first out (function (y) (and (equal? (get (last y) 'kind) 'note) (equal? (get (last y) 'instr) (get pl 'instr)) (== (get (last y) 'midi) (get pl 'midi)) (< (abs (- (+ (head y) (getidx y 1)) (head x))) 0.02)))) nil)
        if (equal? (type prev) "nil") { push out (list (head x) (getidx x 1) pl) } { setidx prev 1 (+ (getidx prev 1) (getidx x 1)) }
    })
    return out
}

# --- envelopes over time: parameters that change during a process ---------------------------------------------
# (env t0 v0 t1 v1 ...)    a value changing with time: at each breakpoint a value; between breakpoints a number or
#                          a range (list lo hi) is interpolated, and a set (a list of symbols, pitches or chords) is
#                          crossfaded: nearer the earlier breakpoint the earlier set is drawn more often. Given as a
#                          flat list: (env (list 0 (list 1 2) 10 (list 0.1 0.2)))
function env (kv) (record kv)
# (env-at e t)             the value at time t: an interpolated number or range, or a set chosen by the crossfade
function env-at (e t) {
    var ts (map e head)
    if (<= t (head ts)) { return (last (head e)) }
    if (>= t (last ts)) { return (last (last e)) }
    var k 0
    while (and (< (+ k 1) (length ts)) (>= t (getidx ts (+ k 1)))) { set k (+ k 1) }
    var a (last (getidx e k))
    var b (last (getidx e (+ k 1)))
    var u (/ (- t (getidx ts k)) (max 1e-9 (- (getidx ts (+ k 1)) (getidx ts k))))
    if (and (equal? (type a) "scalar") (equal? (type b) "scalar")) { return (+ a (* u (- b a))) }
    if (and (numeric-range? a) (numeric-range? b)) { return (list (+ (head a) (* u (- (head b) (head a)))) (+ (last a) (* u (- (last b) (last a))))) }
    return (if (< (rand) u) b a)                                   # sets: crossfaded by chance
}
function numeric-range? (v) (and (equal? (type v) "list") (== (length v) 2) (equal? (type (head v)) "scalar") (equal? (type (last v)) "scalar"))
# (draw-in range)          a random number in a range (or the number itself)
function draw-in (v) (if (equal? (type v) "list") (+ (head v) (* (rand) (- (last v) (head v)))) v)
# (pick-from set)          a random element of a set (or the value itself when it is not a list of alternatives)
function pick-from (v) (if (equal? (type v) "list") (getidx v (floor (* (rand) (length v)))) v)

# --- dynamics as a level: 0 (ppp) to 1 (fff), for gradual crescendi and diminuendi ---
var dynamics-scale (list 'ppp 'pp 'p 'mp 'mf 'f 'ff 'fff)
# (dynamics-level? v)      is a dynamics parameter a level (a number or a range) rather than a set of labels?
function dynamics-level? (v) (or (equal? (type v) "scalar") (numeric-range? v))
# (level->dynamics x)      a level 0..1 as the nearest label of ppp pp p mp mf f ff fff; (dynamics->level d) the reverse
function level->dynamics (x) (getidx dynamics-scale (max 0 (min 7 (round (* 7 x)))))
function dynamics->level (d) { var k (find (map dynamics-scale str) (str d))
                               return (if (< k 0) 0.5 (/ k 7)) }
# (level->gain x)          a level as a gain, 0.25 at ppp to 1 at fff (the sample carries the timbre of its dynamics,
#                          the gain the finer steps between the recorded ones)
function level->gain (x) (+ 0.25 (* 0.75 (max 0 (min 1 x))))

# --- the orchestra: players, pairs, ossia -------------------------------------------------------------------
# (orchestra spec)         the players: spec a list where a symbol is one player of that instrument, a string with
#                          | is one player alternating between instruments (an ossia: "Fl|Picc"), and a list is a
#                          group of players scheduled together (paired): (orchestra (list 'Fl 'Fl "Ob|EH" (list 'Vn 'Vc)))
#                          => a list of player records (instrs, group)
function orchestra (spec) {
    var players (list)
    var group 0
    each spec (function (item) {
        if (equal? (type item) "list") {
            each item (function (i) (push players (record (list 'instrs (ossia i) 'group group 'busy-until 0 'last-pitch nil))))
            set group (+ group 1)
        } {
            push players (record (list 'instrs (ossia item) 'group -1 'busy-until 0 'last-pitch nil))
        }
    })
    return players
}
function ossia (item) (map (split (str item) "|") identity)
# (orchestra-size orch)    how many players; (orchestra-instruments orch) every instrument they can play
function orchestra-size (orch) (length orch)
function orchestra-instruments (orch) (unique (reduce (map orch (function (p) (get p 'instrs))) concat-list (list)))

# --- the orchestral granulator ------------------------------------------------------------------------------------
# (orchestrate-granular db orch secs params)   orchestration as granular synthesis: a stochastic process realised by
#     the players of an orchestra, never more at once than there are players, each event a note. params is a record;
#     every parameter may be an env (see env) or a fixed value:
#       'density    events per second, a range: (env (list 0 (list 4 6) 10 (list 0.5 1)))
#       'register   octaves, a range: (env (list 0 (list 3 3) 10 (list 3 6)))   (octave 4 holds middle C)
#       'duration   seconds, a range (default (list 0.2 1))
#       'styles     a set of techniques: (env (list 0 (list 'ord) 10 (list 'pizz 'flatt)))
#       'dynamics   a set of labels, (env (list 0 (list 'pp) 10 (list 'ff))), crossfaded by chance; or a level from 0
#                   (ppp) to 1 (fff), a number or a range, interpolated: (env (list 0 0 40 1)) is a continuous
#                   crescendo, mapped to the scale ppp pp p mp mf f ff fff for the sample and to a gain for the rest
#       'chords     a set of chords (lists of pitches), or nil: any pitch of the register
#       'chord-weight 0..1: how often a pitch comes from the chord rather than from the whole register (1 by
#                   default); an envelope from 1 to 0 dissolves a chord into a cloud gradually
#       'method     how a pitch is chosen: 'random (in the register, or of the chord), 'pivots (each player around
#                   its chord pitch, within 'interval semitones), 'chordinterp (between the chords of the envelope,
#                   voice by voice, as time passes), 'markov (from 'markov-score: a score whose lines train a
#                   transition table per instrument), 'harmonic (a partial of 'fundamental with probability
#                   'harmonicity, else random)
#       'polyphony  how many players may sound at once at most (an envelope; the orchestra's size by default)
#       'coupling   0..1: how often a paired group actually sounds together (1: always; 0: its players independent),
#                   an envelope makes an orchestra synchronised at first and free later, or the reverse
#       'solutions  how many realisations to make (default 1), each with its own random draws
#     => an orchestration result with one segment; (best-connection result) is its fragment
function orchestrate-granular (db orch secs params) {
    var n (opt params 'solutions 1)
    var sols (map (vec->list (range n)) (function (k) (list k (granulate db orch secs params))))
    return (orchestration 'granular params (list (segment 0 secs sols)))
}
function param-at (params key t default) {
    var v (opt params key default)
    if (and (equal? (type v) "list") (> (length v) 0) (equal? (type (head v)) "list") (== (length (head v)) 2) (equal? (type (head (head v))) "scalar") (not (numeric-range? v))) { return (env-at v t) }   # an env
    return v
}
function granulate (db orch secs params) {
    var players (map orch (function (p) (map p (function (kv) (list (head kv) (last kv))))))   # fresh copies: the busy times
    var method (opt params 'method 'random)
    var table (if (equal? method 'markov) (markov-table (get params 'markov-score)) nil)
    var out (list)
    var t 0
    var voice-k 0
    while (< t secs) {
        var density (param-at params 'density t (list 1 1))
        var rate (max 0.01 (draw-in density))
        var reg (param-at params 'register t (list 3 5))
        var lo (* 12 (+ 1 (head reg)))
        var hi (- (* 12 (+ 2 (last reg))) 1)
        var dur (draw-in (param-at params 'duration t (list 0.2 1)))
        var tech (pick-from (param-at params 'styles t (list 'ord)))
        var dv (param-at params 'dynamics t (list 'mf))
        var dyn (if (dynamics-level? dv) (level->dynamics (draw-in dv)) (pick-from dv))   # a level 0..1 (interpolated) or a set of labels
        var gain (if (dynamics-level? dv) (level->gain (draw-in dv)) 1)
        var chord-set (param-at params 'chords t nil)
        var weight (draw-in (param-at params 'chord-weight t 1))
        var chord (if (or (equal? (type chord-set) "nil") (>= (rand) weight)) nil (pick-from chord-set))     # one chord of the set, or free
        # a free player (or a free paired group): the first whose last note is over; a group sounds together with
        # probability 'coupling (1 by default), else its members go one at a time
        var coupling (draw-in (param-at params 'coupling t 1))
        var free (filter players (function (p) (<= (get p 'busy-until) t)))
        var poly (round (draw-in (param-at params 'polyphony t 1000)))
        var busy (- (length players) (length free))
        if (and (> (length free) 0) (< busy poly)) {
            var p (getidx free (floor (* (rand) (length free))))
            var group (take (if (or (< (get p 'group) 0) (>= (rand) coupling)) (list p) (filter free (function (q) (== (get q 'group) (get p 'group))))) (max 1 (- poly busy)))   # within the polyphony
            if (equal? method 'target) {
                # the target's spectrum now, less what already sounds; a pursuit over the free players' sounds gives the
                # atoms: one note each (instrument, pitch, technique from the atom; the dynamics from the level; the cents
                # from the target's nearest peak); durations from the target's coherence time
                var target (get params 'target)
                var found (target-atoms db target t group (filter players (function (q) (> (get q 'busy-until) t))) params)
                each found (function (f) {
                    var q (head f)
                    var n (last f)
                    put! n 'gain gain
                    put! n 'dyn (str dyn)
                    var cdur (max (draw-in (param-at params 'duration t (list 0.1 0.3))) (atom-persistence target n t))   # how long this sound is heard in the target
                    push out (list t cdur n)
                    put! q 'busy-until (+ t cdur)
                    put! q 'last-pitch (get n 'midi)
                    put! q 'sounding n
                    set voice-k (+ voice-k 1)
                })
            } {
                each group (function (q) {
                    var pitch (choose-pitch db q method lo hi chord params t table voice-k)
                    var instr (instrument-for db (get q 'instrs) pitch)
                    push out (list t dur (put (note db instr pitch dyn tech) 'gain gain))
                    put! q 'busy-until (+ t dur)
                    put! q 'last-pitch pitch
                    set voice-k (+ voice-k 1)
                })
            }
        }
        set t (+ t (/ 1 rate))
    }
    return out
}
# the instrument of a player for a pitch: among its alternatives (an ossia), the one whose range holds the pitch,
# else the nearest range
function instrument-for (db instrs pitch) {
    var holding (filter instrs (function (i) { var r (try (db-range-available db i) catch e (list 0 0))
                                               return (and (>= pitch (head r)) (<= pitch (last r))) }))
    if (> (length holding) 0) { return (head holding) }
    return (min-by instrs (function (i) { var r (try (db-range-available db i) catch e (list 60 60))
                                          return (min (abs (- pitch (head r))) (abs (- pitch (last r)))) }))
}
# a pitch for a player by the method, within lo..hi (MIDI)
function choose-pitch (db player method lo hi chord params t table voice-k) {
    var in-register (function (m) {                          # moved by octaves into lo..hi, then clamped
        var q m
        while (< q lo) { set q (+ q 12) }
        while (> q hi) { set q (- q 12) }
        return (max lo (min hi q)) })
    var chord-pitches (if (equal? (type chord) "nil") nil (map chord pitch->number))
    if (equal? method 'pivots) {
        var centres (if (equal? (type chord-pitches) "nil") (list (/ (+ lo hi) 2)) chord-pitches)
        var centre (in-register (getidx centres (mod voice-k (length centres))))
        return (max lo (min hi (+ centre (round (* (draw-in (param-at params 'interval t 3)) (- (* 2 (rand)) 1))))))   # clamped, not folded
    }
    if (equal? method 'markov) {
        var prev (get player 'last-pitch)
        var row (if (equal? (type prev) "nil") nil (opt table prev nil))
        if (not (equal? (type row) "nil")) { return (in-register (pick-from row)) }
        if (> (length table) 0) { return (in-register (pick-from (keys table))) }    # start from a pitch the model knows
        return (random-pitch lo hi chord-pitches)
    }
    if (equal? method 'harmonic) {
        if (< (rand) (draw-in (param-at params 'harmonicity t 0.7))) {
            var partials (filter (harmonic-series (opt params 'fundamental "C2") 16) (function (m) (and (>= m lo) (<= m hi))))
            if (> (length partials) 0) { return (pick-from partials) }
        }
        return (random-pitch lo hi chord-pitches)
    }
    return (random-pitch lo hi chord-pitches)                    # 'random and 'chordinterp (the env already interpolates chords)
}
# a random pitch in lo..hi, from a chord's pitch classes when there is one (any octave in the register)
function random-pitch (lo hi chord-pitches) {
    if (equal? (type chord-pitches) "nil") { return (+ lo (floor (* (rand) (+ 1 (- hi lo))))) }
    var candidates (filter (vec->list (range lo (+ hi 1))) (function (m) (any? chord-pitches (function (c) (== (mod m 12) (mod c 12))))))
    if (== (length candidates) 0) { return (+ lo (floor (* (rand) (+ 1 (- hi lo))))) }
    return (pick-from candidates)
}
# (markov-table s)         transitions learnt from a score's notes: a record from a pitch to the list of pitches that
#                          followed it (per instrument, in time order); repeated followers weigh more
function markov-table (s) {
    var table (list)
    var by (group-by (sort-by (filter (get s 'events) (function (e) (equal? (get e 'kind) 'note))) (function (e) (get e 'at))) (function (e) (get e 'instr)))
    each by (function (g) {
        var line (map (last g) (function (e) (get e 'midi)))
        each (range (- (length line) 1)) (function (k) {
            var from (getidx line k)
            if (has? table from) { push (get table from) (getidx line (+ k 1)) } { put! table from (list (getidx line (+ k 1))) }
        })
    })
    return table
}
# --- the target of a morphological orchestration: a sound analysed into curves over time --------------------------
# (target-analyse x sr block hop)   the descriptors a morphological orchestration follows, computed once: the frame
#                          spectra (block and hop as the database's features, so they compare), the event rate,
#                          the polyphony, the register (centroid and spread), the loudness and the coherence time,
#                          each as a curve sampled every hop; => a record ('sr 'block 'hop 'seconds 'times 'spectra
#                          'rate 'polyphony 'centroid 'spread 'loudness 'coherence)
function target-analyse (x sr block hop) {
    var spectra (frame-spectra x block hop (/ block 2))
    var er (event-rate x sr 1 hop)
    var reg (register-curve spectra sr block)
    var n (min (length spectra) (length (getidx er 1)))
    return (record (list 'sr sr 'block block 'hop hop 'seconds (/ (length x) sr) 'times (take (head er) n) 'spectra (take spectra n)
                         'rate (take (getidx er 1) n) 'events (last er) 'polyphony (take (polyphony-estimate spectra) n)
                         'centroid (take (head reg) n) 'spread (take (getidx reg 1) n) 'low (take (last reg) n)
                         'loudness (take (loudness-curve x sr hop) n) 'coherence (take (coherence-time spectra hop sr 0.75) n)))
}
# (target-at target key t)   a curve's value at a time
function target-at (target key t) {
    var v (get target key)
    var k (max 0 (min (- (length v) 1) (floor (* t (/ (get target 'sr) (get target 'hop))))))
    return (getidx v k)
}
# (curve->env times values step)   an env from a curve: one breakpoint every step seconds (the values averaged over the
#                          step); ranged values come out as (list lo hi) when given a function making them
function curve->env (times values step maker) {
    var kv (list)
    var t 0
    var end (last times)
    while (<= t end) {
        var window (filter (vec->list (range (length times))) (function (k) (and (>= (getidx times k) t) (< (getidx times k) (+ t step)))))
        if (> (length window) 0) {
            var vals (vec (map window (function (k) (getidx values k))))
            push kv t
            push kv (maker (mean vals))
        }
        set t (+ t step)
    }
    return (env kv)
}
# (target-envelopes target orch opts)   the granulator's envelopes from a target's curves: density from the event rate
#                          (a range around it), polyphony from the polyphony estimate, register from centroid and
#                          spread (octaves), duration from the coherence time, dynamics as a level from the loudness
#                          (-50 dB -> 0, -6 dB -> 1); opts 'step (seconds per breakpoint, 0.25) and 'follow (0..1, how
#                          closely: 1 exact, lower widens every range)
function target-envelopes (target orch opts) {
    var step (opt opts 'step 0.25)
    var loose (- 1 (opt opts 'follow 1))
    var ts (get target 'times)
    var dens (curve->env ts (get target 'rate) step (function (v) (list (max 0.1 (* v (- 0.7 (* 0.5 loose)))) (max 0.2 (* v (+ 1.3 (* 2 loose)))))))
    var poly (curve->env ts (get target 'polyphony) step (function (v) (max 1 (min (orchestra-size orch) (round (+ v (* 3 loose)))))))
    var lo (curve->env ts (- (/ (- (get target 'low) 12) 12) 0.5) step (function (v) (max 0 (- v loose))))                  # from half an octave under the lowest partial
    var hi (curve->env ts (/ (- (get target 'centroid) 12) 12) step (function (v) (min 8 (+ v loose))))                     # up to the centroid
    var reg (env (reduce (zip lo hi) (function (acc p) (concat-list acc (list (head (head p)) (list (last (head p)) (max (+ 0.5 (last (head p))) (last (last p))))))) (list)))
    var dur (curve->env ts (get target 'coherence) step (function (v) (list (max 0.05 (* v 0.6)) (max 0.1 (* v (+ 1.2 loose))))))
    var dyn (curve->env ts (get target 'loudness) step (function (v) (max 0 (min 1 (/ (+ v 50) 44)))))
    return (record (list 'density dens 'polyphony poly 'register reg 'duration dur 'dynamics dyn))
}
# (target-atoms db target t group sounding params)   the notes for a group of free players at time t: the target's spectrum
#                          at t, less the spectra of the notes still sounding (an orchestral residual), pursued over
#                          the group's available sounds (every technique); => a list of (list player note)
function target-atoms (db target t group sounding params) {
    var spec (getidx (get target 'spectra) (max 0 (min (- (length (get target 'spectra)) 1) (floor (* t (/ (get target 'sr) (get target 'hop)))))))
    var ncoeff (length spec)
    var residual spec
    each sounding (function (q) {
        var n (opt q 'sounding nil)
        if (not (equal? (type n) "nil")) {
            var f (take (get (get n 'entry) 'features) ncoeff)
            var g (* (/ (norm residual) (max 1e-9 (norm f))) 0.5)         # half its share: what sounds is already there
            set residual (max 0 (- residual (* g f)))
        }
    })
    # the dictionary: every available sound of every instrument any player of the group can play
    var atoms (list)
    each group (function (q) (each (get q 'instrs) (function (i) (each (get (opt (db-index! db) (str i) (record (list 'all (list)))) 'all) (function (e) (push atoms (list q e)))))))
    if (== (length atoms) 0) { return (list) }
    var D (map atoms (function (a) (take (get (last a) 'features) ncoeff)))
    var r (mp (log (+ 1 residual)) (map D (function (f) (log (+ 1 f)))) (length group) (opt params 'threshold 0.2) (record (list 'nonneg 1)))
    var out (list)
    var taken (list)
    each (get r 'atoms) (function (k) {
        var a (getidx atoms k)
        var q (head a)
        if (not (contains? taken q)) {                                       # one note per player
            push taken q
            var e (last a)
            var n (note db (get e 'instr) (get e 'midi) (get e 'dyn) (get e 'tech))
            # the tuning: the target's peak nearest to the note's frequency, within a quarter tone, as cents
            var peaks (head (spectral-peaks spec (get target 'sr) (get target 'block) 12))
            var f0 (midi->hz (get e 'midi))
            if (> (length peaks) 0) {
                var near (getidx peaks (argmin (abs (- peaks f0))))
                var cents (* 1200 (log2 (/ (max near 1) f0)))
                if (< (abs cents) 50) { put! n 'cents (round cents) }
            }
            push out (list q n)
        }
    })
    return out
}
# (atom-persistence target n t)   how long a note's sound goes on being part of the target from t: the frames ahead
#                          whose spectrum still contains the atom (the correlation with the atom's spectrum stays at
#                          least 70 % of what it was at t), up to 8 s; the duration a granulated note should hold
function atom-persistence (target n t) {
    var spectra (get target 'spectra)
    var step (/ (get target 'hop) (get target 'sr))
    var k0 (max 0 (min (- (length spectra) 1) (floor (/ t step))))
    var f (log (+ 1 (take (get (get n 'entry) 'features) (length (head spectra)))))
    var start (dot f (log (+ 1 (getidx spectra k0))))
    if (<= start 1e-9) { return 0.1 }
    var k (+ k0 1)
    var cap (min (length spectra) (+ k0 (ceil (/ 8 step))))
    while (and (< k cap) (>= (dot f (log (+ 1 (getidx spectra k)))) (* 0.7 start))) { set k (+ k 1) }
    return (max 0.1 (* (- k k0) step))
}
# (orchestrate-morphological db orch target params)   orchestration as granular synthesis whose process is read off a
#     target sound (target-analyse): its event rate gives the density, its polyphony the number of players at once,
#     its centroid and spread the register, its loudness the dynamics, its coherence time the durations, and at every
#     event a matching pursuit over the free players' sounds picks instruments, pitches and techniques for the
#     target's spectrum at that instant (less what already sounds). No segmentation: a long sound under short ones
#     comes out as a long note under short ones. params: 'follow (0..1), 'step, 'threshold (the pursuit's), 'solutions,
#     and any granulator parameter to override the target's (a 'register or 'dynamics of your own, 'styles ...)
function orchestrate-morphological (db orch target params) {
    var envs (target-envelopes target orch params)
    var p (record (list 'method 'target 'target target))
    each envs (function (kv) (put! p (head kv) (last kv)))
    each params (function (kv) (if (not (contains? (list 'follow 'step 'threshold 'solutions) (head kv))) (put! p (head kv) (last kv))))
    put! p 'threshold (opt params 'threshold 0.2)
    var n (opt params 'solutions 1)
    var secs (get target 'seconds)
    var sols (map (vec->list (range n)) (function (k) (list k (granulate db orch secs p))))
    return (orchestration 'morphological params (list (segment 0 secs sols)))
}
# (chordinterp-env chords rhythm)   an env for 'chords that steps through chordinterp's chords over time: chords a
#                          list of two chords, rhythm the times; use with 'method 'chordinterp (or 'random)
function chordinterp-env (c1 c2 secs steps) {
    var kv (list)
    each (range steps) (function (k) {
        var u (/ k (max 1 (- steps 1)))
        var n (max (length c1) (length c2))
        var ch (map (vec->list (range n)) (function (j) (round (+ (pitch->number (getidx c1 (min j (- (length c1) 1)))) (* u (- (pitch->number (getidx c2 (min j (- (length c2) 1)))) (pitch->number (getidx c1 (min j (- (length c1) 1))))))))))
        push kv (* u secs)
        push kv (list ch)                                        # a set of one chord
    })
    return (env kv)
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
