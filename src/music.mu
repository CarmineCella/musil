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
        if (same? (get e 'source) s) { error "event: a score cannot contain itself" }
        if (== dur 0) { set dur (+ (score-duration (get e 'source)) 0.6) }        # 0: the whole sub-score, with its releases
    }
    put! e 'at at
    put! e 'dur dur
    put! e 'az az
    put! e 'el el
    if (not (has? e 'gain)) { put! e 'gain 1 }                               # a payload may carry its gain (fragment-gain)
    put! e 'id (score-next-id! s)
    push (get s 'events) e
    return e
}
# (score-next-id! s)       a fresh event number: ids are never reused, even after events are removed (the roll and
#                          score-choose! find events by their id)
function score-next-id! (s) {
    var n (opt s 'next-id nil)
    if (equal? (type n) "nil") { set n (+ 1 (if (== (length (get s 'events)) 0) 0 (max-of (map (get s 'events) (function (e) (get e 'id)))))) }
    put! s 'next-id (+ n 1)
    return n
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
    set sound-cache-bytes (+ sound-cache-bytes (* 8 (length (getidx w 1)) (length (head (getidx w 1)))))
    while (and (> sound-cache-bytes sound-cache-limit) (> (length sound-cache) 1)) {          # bounded: a long score with thousands of files must not fill the memory
        var old (last (head sound-cache))
        set sound-cache-bytes (- sound-cache-bytes (* 8 (length (getidx old 1)) (length (head (getidx old 1)))))
        set sound-cache (drop sound-cache 1)
    }
    return w
}
var sound-cache-bytes 0
var sound-cache-limit 400000000                                             # 400 MB of samples kept; (set sound-cache-limit ...) to change
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
    set note-cache-bytes (+ note-cache-bytes (* 8 (length chans) (length (head chans))))
    while (and (> note-cache-bytes 200000000) (> (length note-cache) 1)) {          # a bounded cache: 200 MB
        var old (last (head note-cache))
        set note-cache-bytes (- note-cache-bytes (* 8 (length old) (length (head old))))
        set note-cache (drop note-cache 1)
    }
    return chans
}
var note-cache-bytes 0
# (clear-sound-cache)      forget the loaded files and the notes made from them
function clear-sound-cache () { set sound-cache (list)
                                 set note-cache (list)
                                 set sound-cache-bytes 0
                                 set note-cache-bytes 0 }
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
        breathe                                                            # the windows stay alive during a long render
    })
    return out
}
# (render-buffer s layout)   the score as channels, in the concert hall as the roll plays it (score-reverb!: the dry
#                          and wet amounts; (score-reverb! s 1 0) renders dry), scaled down when it would clip; the
#                          buffer to keep working on; (score-render s layout) is the dry mix itself
function render-buffer (s layout) {
    var sr (get s 'sr)
    var rv (opt s 'reverb (list 0.7 0.3))
    var dry (score-render s layout)
    var out (if (== (last rv) 0) (map dry (function (c) (* (head rv) c))) (concerthall dry sr (head rv) (last rv)))
    var peak (max-of (map out (function (c) (max (abs c)))))
    if (> peak 0.98) { print "render: the mix peaked at" (fixed peak 2) "and was scaled to 0.98"
                       return (map out (function (c) (* (/ 0.98 peak) c))) }
    return out
}
# (render s path layout)   render-buffer written as a WAV at the score's rate; => the channels
function render (s path layout) {
    var out (render-buffer s layout)
    write-wav path (get s 'sr) out
    return out
}
# (render-hall s path)     the score in the hall, in stereo, as a WAV (the roll's Render... button): (render s path "stereo")
function render-hall (s path) {
    var out (render s path "stereo")
    print "rendered" path "(in the hall)"
    return out
}

# --- playing: the score handed to the engine, its sounds read by a thread ahead of the clock ---------------------
# A score is played by the engine's cue player (play-cues of live): score-play-now turns every event into a cue (a
# file to read, or a buffer already made) at a clock time, hands the whole list to the engine and returns; from then
# on the interpreter is not involved: the loader thread reads each sound a few seconds before it is due (a cache of
# decoded files, 2 GB by default: cue-cache-limit), cuts it to its duration at its own rate, and queues it as a voice
# that the audio thread plays at the note's pitch shift. The concert hall sits on the output bus (bus-reverb, on a
# thread of its own). So Play starts at once whatever the score's length, and neither the windows nor the interpreter
# can stall the music, nor the music them.
# (play-score s gain)      play the score now (the device is opened if needed) and wait until it ends or is stopped
#                          (stop-score); gain scales it, with the score's 'level (0.5 by default: many players at
#                          fff add up; the bus limiter catches the rest); the roll's cursor follows
function play-score (s gain) (play-score-from s gain 0)
# (play-score-from s gain from)   the same from a time in seconds
function play-score-from (s gain from) {
    score-play-now s gain from
    while (head (playhead)) { sleep 0.05 }
    return nil
}
var score-player nil                                                         # what is playing: the score, its synths, the end
var bus-hall-state nil
# (bus-hall! sr dry wet)   the concert hall on the output bus at a rate and mix, set only when it changes (setting it
#                          again would cut the tail)
function bus-hall! (sr dry wet) {
    var want (list sr dry wet)
    if (not (equal? bus-hall-state want)) { bus-reverb (hall-ir sr) dry wet
                                           set bus-hall-state want }
    return nil
}
# (score-play-now s gain from)   start playing and return at once (what the roll's Play button does); => the end time on
#                          the clock. stop-score stops it.
function score-play-now (s gain from) {
    if (not (opt (audio-status) "open" 0)) { audio-init }
    stop-score
    var sr (audio-sr)
    var rv (opt s 'reverb (list 0.7 0.3))
    bus-hall! sr (head rv) (last rv)
    var level (* gain (opt s 'level 0.5))
    var t0 (+ (audio-time) 0.3)
    var cues (list)
    var synths (list)
    each (score-flatten s 0) (function (e) (if (> (event-end e) from) {
        var when (+ t0 (max 0 (- (get e 'at) from)))
        var skip (max 0 (- from (get e 'at)))
        var kind (get e 'kind)
        if (or (equal? kind 'note) (equal? kind 'file)) {
            var rate (if (equal? kind 'note) (pow 2 (/ (+ (opt e 'shift 0) (/ (opt e 'cents 0) 100)) 12)) 1)
            push cues (list when (if (equal? kind 'note) (note-path e) (get e 'source)) skip (- (get e 'dur) skip) rate (* level (get e 'gain)) (azimuth->pan (get e 'az)))
        } {
            if (equal? kind 'synth) {
                var id (try (synth (get e 'source)) catch err nil)
                if (equal? (type id) "nil") { print "play-score: the instrument of event" (get e 'id) "does not stream, left out" } {
                    push synths id
                    if (contains? (synth-params id) "amp") { set-param id 'amp (* level (get e 'gain)) 0 when }
                    each (get e 'params) (function (q) (set-param id (head q) (last q) 0 when))
                    note-at when id (- (get e 'dur) skip)
                }
            } {                                                                # a buffer, a call, a chord: made now
                var made (try (place (render-event e sr) "stereo" (get e 'az) (get e 'el) sr) catch err nil)
                if (equal? (type made) "nil") { print "play-score: event" (get e 'id) (get e 'label) "could not be made, left out" } {
                    var chans (if (> skip 0) (map made (function (c) (drop c (min (length c) (floor (* skip sr)))))) made)
                    push cues (list when chans sr 0 (- (get e 'dur) skip) 1 level 0)
                }
            }
        }
    }))
    var end (+ t0 (- (score-duration s) from) 1)
    play-cues cues
    set score-player (record (list 'score s 'synths synths 'end end 'cues (length cues)))
    playhead! (+ t0 (bus-reverb-latency)) from end (register-score s)     # the cursor lags by the hall's latency, as the sound does
    return end
}
# (azimuth->pan az)        an azimuth in degrees (0 in front, left positive) as the engine's pan, -1 (left) to 1 (right)
function azimuth->pan (az) {
    var a (max -90 (min 90 (if (> az 90) (- 180 az) (if (< az -90) (- -180 az) az))))
    return (/ (- 0 a) 90)
}
# (score-flatten s depth)  the score's events with those of the scores inside brought to the top: copies at absolute
#                          times, the gains multiplied, cut to their score's event; => a list
function score-flatten (s depth) {
    if (> depth 16) { error "score-flatten: scores nested more than 16 deep (a score inside itself?)" }
    var out (list)
    each (get s 'events) (function (e) {
        if (equal? (get e 'kind) 'score) {
            var inner (get e 'source)
            var window (get e 'dur)
            each (score-flatten inner (+ depth 1)) (function (x) {
                var at (+ (get e 'at) (get x 'at))
                if (< (get x 'at) window) {
                    var c (put (put (put x 'at at) 'dur (min (get x 'dur) (- window (get x 'at)))) 'gain (* (get e 'gain) (get x 'gain)))
                    push out c
                }
            })
        } { push out e }
    })
    return out
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
# (stop-score)             stop whatever score is playing, at once
function stop-score () {
    if (opt (audio-status) "open" 0) { stop-cues
                                       stop-all }
    if (not (equal? (type score-player) "nil")) {
        each (get score-player 'synths) (function (id) (try (free id) catch err nil))
        set score-player nil
    }
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
    var segs (list)                                           # the orchestrations' segments: (list start end chosen costs orch seg), for the roll's solution menu
    each (zip (score-solutions s) (vec->list (range (length (opt s 'orchestrations (list)))))) (function (p) {
        var at (head (head p))
        each (zip (last (head p)) (vec->list (range (length (last (head p)))))) (function (q) {
            var g (head q)
            push segs (list (+ at (head g)) (+ at (head g) (getidx g 1)) (getidx g 2) (getidx g 3) (last p) (last q))
        })
    })
    return (list (get s 'name) (list (list "roll" row-specs bars (register-score s) segs)) (list (list "xlabel" "time (s)")))
}
# --- scores on display: a registry, so that a window can name a score and an event to play -------------------
var displayed-scores (list)
# (register-score s)       remember a score under a number (the roll carries it); => the number
function register-score (s) {
    var hit (find-first displayed-scores (function (p) (same? (last p) s)))
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
# (roll-save score-id path)   the score's notes as a connection file (the roll's Save button; score-save)
function roll-save (score-id path) { var n (score-save (displayed-score score-id) path)
                                     print "saved" n "notes to" path
                                     return n }
# (roll-choose score-id orch seg n)   solution n of segment seg of the orch-th orchestration in a displayed score (the
#                          roll's solution menu): the score's notes replaced (score-choose!) and the roll redrawn
function roll-choose (score-id orch seg n) {
    var s (displayed-score score-id)
    score-choose! s orch seg n
    roll-refresh (score-roll s)
    print "solution" (+ n 1) "of segment" (+ seg 1) "chosen:" (length (score-events s)) "events in the score"
    return nil
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
    bus-hall! sr (head rv) (last rv)                                                     # the hall is on the bus
    play-buffer (place (render-event e sr) "stereo" (get e 'az) (get e 'el) sr) sr (opt s 'level 0.5) 0 1 0 (+ (audio-time) 0.05)
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
    if (has? slot 'range) { return (get slot 'range) }
    var ms (vec (map (get slot 'all) (function (e) (get e 'midi))))
    put! slot 'range (list (min ms) (max ms))                                # kept: the orchestrators ask for it at every note
    return (get slot 'range)
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
    # the pitch first: a sample at that very pitch with the technique, at the dynamics asked for or the nearest
    # recorded (a player can play any dynamics; a transposed sample is a last resort, for a pitch not recorded at all)
    var at-pitch (filter (db-candidates db instr nil tech) (function (e) (== (get e 'midi) want)))
    var exact (filter same (function (e) (== (get e 'midi) want)))
    var entry (if (> (length exact) 0) (head exact)
                  (if (> (length at-pitch) 0) (min-by at-pitch (function (e) (abs (- (dynamics->level (get e 'dyn)) (dynamics->level dyn)))))
                      (min-by same (function (e) (abs (- (get e 'midi) want))))))
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
# (just-cents fundamental pitch n)   the cents a tempered pitch is away from the nearest of the first n partials of a
#                          fundamental (a just overtone chord: the 7th partial -31, the 11th -49, the 13th +41, the
#                          14th -31 cents); nil when the pitch is not one of them (within a quarter tone)
function just-cents (fundamental pitch n) {
    var f0 (midi->hz (pitch->number fundamental))
    var f (midi->hz (pitch->number pitch))
    var k (max 1 (round (/ f f0)))
    if (> k n) { return nil }
    var c (* 1200 (log2 (/ (* k f0) f)))
    return (if (< (abs c) 50) (round c) nil)
}
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
    each (range (length segs)) (function (k) (each (solution result k (getidx choices k)) (function (x) (push out x))))
    return (merge-continuations-within (sort-by out head) (opt result 'hold 1e9))
}
# (best-connection result)   the first solution of every segment
function best-connection (result) (connection result (map (get result 'segments) (function (g) 0)))
# (connect! s at result choices)   a connection placed in a score; the score remembers the orchestration, so that the
#                          roll (or score-choose!) can put another solution of a segment in its place
function connect! (s at result choices) {
    var evs (add! s at (connection result choices))
    put! s 'orchestrations (concat-list (opt s 'orchestrations (list)) (list (record (list 'at at 'result result 'choices (map choices identity) 'events evs))))   # remembered: the roll lets another solution be chosen
    return evs
}
# (score-choose! s k seg n)   in the k-th orchestration connected into the score (connect!, in order), solution n of
#                          segment seg in place of the one chosen: its notes replaced; => the new events
function score-choose! (s k seg n) {
    var os (opt s 'orchestrations (list))
    if (or (< k 0) (>= k (length os))) { error "score-choose!: no orchestration " k " in the score" }
    var o (getidx os k)
    var ids (map (get o 'events) (function (e) (get e 'id)))
    put! s 'events (reject (get s 'events) (function (e) (contains? ids (get e 'id))))
    setidx (get o 'choices) seg n
    var evs (add! s (get o 'at) (connection (get o 'result) (get o 'choices)))
    put! o 'events evs
    score-clear-cache! s
    return evs
}
# (score-solutions s)      the orchestrations connected into a score, with their segments and the solution chosen in
#                          each: a list of (list at (list (list seg-at seg-dur chosen costs) ...)); what the roll shows
function score-solutions (s) (map (opt s 'orchestrations (list)) (function (o) (list (get o 'at) (map (zip (get (get o 'result) 'segments) (get o 'choices)) (function (p) (list (get (head p) 'at) (get (head p) 'dur) (last p) (map (get (head p) 'solutions) head)))))))
# (merge-continuations f)   in a fragment, a note starting where a note of the same instrument and pitch ends becomes
#                          the continuation of that note (one longer event), as an orchestration's connection does;
#                          notes that carry their 'player (the granulator's do) merge only with the same player's
function merge-continuations (f) (merge-continuations-within f 1e9)
# (merge-continuations-within f hold)   the same, a continued note never longer than hold seconds: a player who
#                          cannot hold a note that long (an oboe without a breath) starts it again instead
function merge-continuations-within (f hold) {
    var out (list)
    var by-key (list)                                                       # instrument|pitch -> the notes so far of that sound (a few), not the whole list
    each f (function (x) {
        var pl (last x)
        var prev nil
        var key (if (equal? (get pl 'kind) 'note) (concat (get pl 'instr) "|" (str (get pl 'midi)) "|" (str (opt pl 'player ""))) nil)   # the same player, when the notes say who played them
        if (not (equal? (type key) "nil")) {
            var same (opt by-key key nil)
            if (not (equal? (type same) "nil")) { set prev (find-first same (function (y) (and (< (abs (- (+ (head y) (getidx y 1)) (head x))) 0.03) (<= (- (+ (head x) (getidx x 1)) (head y)) hold)))) }
        }
        if (equal? (type prev) "nil") {
            var fresh (list (head x) (getidx x 1) pl)
            push out fresh
            if (not (equal? (type key) "nil")) { if (has? by-key key) { push (get by-key key) fresh } { put! by-key key (list fresh) } }
        } { setidx prev 1 (max (getidx prev 1) (- (+ (head x) (getidx x 1)) (head prev))) }   # ends where the continuation ends (not lengthened by an overlap of a few ms)
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
#                          => a list of player records: instrs, group, and the state the granulator keeps for each
#                          (busy-until, last-pitch, last-tech, seat)
function orchestra (spec) {
    var players (list)
    var group 0
    each spec (function (item) {
        if (equal? (type item) "list") {
            each item (function (i) (push players (player-record i group)))
            set group (+ group 1)
        } {
            push players (player-record item -1)
        }
    })
    return players
}
function player-record (item group) (record (list 'instrs (ossia item) 'group group 'busy-until 0 'last-pitch nil 'last-tech nil 'seat nil 'seat-since 0))
function ossia (item) (map (split (str item) "|") identity)
# (orchestra-rest! orch)   every player free again and its memory cleared (the last pitch, the technique, the seat):
#                          for a run of granulators that share one orchestra ('continue) when a new piece begins
function orchestra-rest! (orch) {
    each orch (function (p) { put! p 'busy-until 0
                              put! p 'last-pitch nil
                              put! p 'last-tech nil
                              put! p 'seat nil
                              put! p 'seat-since 0
                              put! p 'starts (vec)
                              put! p 'ends (vec) })
    return orch
}
# (orchestra-size orch)    how many players; (orchestra-instruments orch) every instrument they can play
function orchestra-size (orch) (length orch)
function orchestra-instruments (orch) (unique (reduce (map orch (function (p) (get p 'instrs))) concat-list (list)))
# (orchestra-range db orch)   the lowest and highest MIDI pitch any instrument of the orchestra was recorded at (on disk);
#                          (orchestra-octaves db orch) the same as octaves, the granulator's register unit: what to clip a
#                          band taken from a recording to, so that it asks nothing the orchestra cannot play
function orchestra-range (db orch) {
    var rs (map (filter (orchestra-instruments orch) (function (i) (not (equal? (type (opt (db-index! db) (str i) nil)) "nil")))) (function (i) (db-range-available db i)))
    if (== (length rs) 0) { error "orchestra-range: no instrument of the orchestra has a sound on disk" }
    return (list (min-of (map rs head)) (max-of (map rs last)))
}
function orchestra-octaves (db orch) { var r (orchestra-range db orch)
                                       return (list (/ (- (head r) 12) 12) (/ (- (last r) 12) 12)) }
# (instrument-family instr)   'winds 'brass 'percussion 'keyboard 'plucked or 'strings by the instrument code (the
#                          orchestral order); 'other for a code it does not know. What a 'balance may be given by
function instrument-family (instr) {
    var k (find orchestral-order (str instr))
    if (< k 0) { return 'other }
    if (< k 17) { return 'winds }
    if (< k 23) { return 'brass }
    if (< k 25) { return 'percussion }
    if (contains? (list "Hp" "Gtr") (str instr)) { return 'plucked }
    if (< k 29) { return 'keyboard }
    return 'strings
}
# (balance-gain balance instr)   the gain of an instrument under a 'balance: a record from instrument codes or families
#                          to decibels, (record (list 'brass -6 'Hn -3)): the instrument's own entry, else its family's,
#                          else 0 dB; => a linear gain
function balance-gain (balance instr) {
    if (equal? (type balance) "nil") { return 1 }
    var own (find-first balance (function (kv) (equal? (str (head kv)) (str instr))))
    var fam (find-first balance (function (kv) (equal? (str (head kv)) (str (instrument-family instr)))))
    var d (if (not (equal? (type own) "nil")) (last own) (if (not (equal? (type fam) "nil")) (last fam) 0))
    return (pow 10 (/ d 20))
}

# --- the orchestral granulator ------------------------------------------------------------------------------------
# (orchestrate-granular db orch secs params)   orchestration as granular synthesis: a stochastic process realised by
#     the players of an orchestra, never more at once than there are players, each event a note that a player of the
#     orchestra can play as recorded (the pitch, the technique, and with 'dynamics-strict the dynamics, are those of a
#     sample of the instrument: the constraints select and are never relaxed; what no player can play is reported and
#     left out). params is a record; every parameter may be an env (see env) or a fixed value:
#       'density    events per second, a range: (env (list 0 (list 4 6) 10 (list 0.5 1)))
#       'group-density   one density per paired group of the orchestra (a list, in the order of the groups): the
#                   event's rate is their sum and the event goes to a group with a probability proportional to its
#                   rate; players outside any group keep 'density
#       'register   octaves, a range: (env (list 0 (list 3 3) 10 (list 3 6)))   (octave 4 holds middle C)
#       'duration   seconds, a range (default (list 0.2 1)); 'duration-law how it is drawn in the range: 'uniform
#                   (default), 'log (log-uniform: as many short as long by ratio, a granular texture), or a number,
#                   the exponent of the draw (2: mostly short, near the minimum; 0.5: mostly long)
#       'styles     a set of techniques: (env (list 0 (list 'ord) 10 (list 'pizz 'flatt))); each player takes one of
#                   the set it has samples of in the register
#       'dynamics   a set of labels, (env (list 0 (list 'pp) 10 (list 'ff))), crossfaded by chance; or a level from 0
#                   (ppp) to 1 (fff), a number or a range, interpolated: (env (list 0 0 40 1)) is a continuous
#                   crescendo, mapped to the scale ppp pp p mp mf f ff fff for the sample and to a gain for the rest
#       'dynamics-strict   1: the dynamics is a constraint like the technique: a player must have a sample at the
#                   dynamics asked for (0 by default: the nearest recorded dynamics is taken and the substitution
#                   counted in the report)
#       'balance    a record from instrument codes or families ('strings 'winds 'brass 'percussion 'keyboard 'plucked)
#                   to decibels, added to every note of them: (record (list 'brass -6 'Hn -3)) keeps the brass under
#                   the strings at the same marking
#       'chords     a set of chords (lists of pitches), or nil: any pitch of the register
#       'chord-weight 0..1: how often a pitch comes from the chord rather than from the whole register (1 by
#                   default); an envelope from 1 to 0 dissolves a chord into a cloud gradually
#       'method     how a pitch is chosen: 'random (in the register, or of the chord), 'cluster (every pitch of the
#                   band once before any is repeated: the players between them saturate the band chromatically, as
#                   in a Ligeti cluster), 'pivots (each player around its chord pitch, within 'interval semitones),
#                   'chordinterp (between the chords of the envelope, voice by voice, as time passes), 'markov (from
#                   'markov-score: a score whose lines train a transition table per instrument), 'harmonic (a partial
#                   of 'fundamental with probability 'harmonicity, else random; with 'just 1 the partials are tuned
#                   in just intonation, the note carrying its cents (just-cents): an overtone chord)
#       'weight     -1..1: where in the band the pitches fall: 0 evenly (default), towards 1 the middle of the band
#                   more often, towards -1 its edges; 'tilt -1..1 the same between the bottom (-1) and the top (1)
#       'leap       semitones: a player's next pitch is within this interval of its last one when it has samples
#                   there (2: chromatic lines, a micropolyphony; nil, the default: anywhere in the band); a signed
#                   range (list -3 -1) is a direction: each next pitch one to three semitones below the last, a
#                   descending line that starts again anywhere (at the top, with 'tilt) when it runs out of band
#       'inertia    0..1: how often a player keeps the technique of its last note when the set still holds it (0 by
#                   default: drawn anew at every note; 0.9: a player changes bowing rarely)
#       'seat       seconds: an ossia player that has taken an instrument keeps it at least this long (a person
#                   playing one instrument), and is only eligible for what that instrument can play meanwhile; 0
#                   (default): the band decides the instrument at every note; a large value: a fixed seating for
#                   the whole process
#       'spread     seconds: the players of a paired group struck together are scattered within this time (0.03:
#                   an orchestra's attack rather than a sampler's); 0 by default
#       'polyphony  how many players may sound at once at most (an envelope; the orchestra's size by default)
#       'steal      in the 'target method (1 by default): when an event is due and no player is free, the oldest note
#                   ends and gives up its player, so the target's density is honoured
#       'coupling   0..1: how often a paired group actually sounds together (1: always; 0: its players independent),
#                   an envelope makes an orchestra synchronised at first and free later, or the reverse
#       'continue   1: the orchestra's own player records carry the state (every note booked so far, last pitches,
#                   seats) into this call and out of it, so a run of granulators placed one after another, or
#                   overlapping, in any order, share one orchestra and never book a player twice (a note is cut
#                   where the player's next booking begins); give 'start, the time in the piece this call begins
#                   at, so the bookings compare (orchestra-rest! clears the state)
#       'solutions  how many realisations to make (default 1), each with its own random draws
#     => an orchestration result with one segment; (best-connection result) is its fragment; (get result 'report)
#     the run's report (granulation-report prints it): events due, events that wrote notes and the notes written (a
#     paired group writes several a event), events skipped because every
#     able player was busy (the saturation), events unplayable by the orchestra, dynamics substituted, and each
#     player's notes and busy fraction; 'reports one per solution
function orchestrate-granular (db orch secs params) {
    var n (opt params 'solutions 1)
    var runs (map (vec->list (range n)) (function (k) (granulate db orch secs params)))
    var sols (map (zip (vec->list (range n)) runs) (function (p) (list (head p) (head (last p)))))
    var r (orchestration 'granular params (list (segment 0 secs sols)))
    put! r 'reports (map runs last)
    put! r 'report (last (head runs))
    return r
}
# (value-at v t)           an env at a time, or the value itself; (param-at params key t default) the same for a parameter
function value-at (v t) {
    if (and (equal? (type v) "list") (> (length v) 0) (equal? (type (head v)) "list") (== (length (head v)) 2) (equal? (type (head (head v))) "scalar") (not (numeric-range? v))) { return (env-at v t) }   # an env
    return v
}
function param-at (params key t default) (value-at (opt params key default) t)
# (draw-duration range law)   a duration drawn in a range by a law: 'uniform, 'log (log-uniform), or an exponent
function draw-duration (v law) {
    if (not (equal? (type v) "list")) { return v }
    var a (head v)
    var b (last v)
    var u (rand)
    if (equal? law 'log) { return (* (max a 1e-4) (pow (/ (max b 1e-4) (max a 1e-4)) u)) }
    if (equal? (type law) "scalar") { return (+ a (* (- b a) (pow u law))) }
    return (+ a (* u (- b a)))
}
# (pick-weighted items f)  a random element, each weighed by f (a non-negative number per element)
function pick-weighted (items f) {
    var ws (map items (function (x) (max 0 (f x))))
    var total (sum (vec ws))
    if (<= total 0) { return (pick-from items) }
    var u (* (rand) total)
    var k 0
    var acc (head ws)
    while (and (< (+ k 1) (length items)) (>= u acc)) { set k (+ k 1)
                                                        set acc (+ acc (getidx ws k)) }
    return (getidx items k)
}
# (band-weigher lo hi weight tilt)   the weight of a pitch in the band lo..hi (MIDI) under 'weight (middle vs edges) and
#                          'tilt (bottom vs top); => a function of the pitch
function band-weigher (lo hi weight tilt) {
    var span (max 1 (- hi lo))
    return (function (m) {
        var u (max 0 (min 1 (/ (- m lo) span)))
        var centre (- 1 (abs (- (* 2 u) 1)))                               # 1 in the middle of the band, 0 at its edges
        var w (+ (- 1 (abs weight)) (* (abs weight) (if (> weight 0) centre (- 1 centre))))
        var v (+ (- 1 (abs tilt)) (* (abs tilt) (if (> tilt 0) u (- 1 u))))
        return (+ 0.01 (* w v))
    })
}
function granulate (db orch secs params) {
    var carry (opt params 'continue 0)
    var players (if carry orch (orchestra-rest! (map orch (function (p) (map p (function (kv) (list (head kv) (last kv))))))))   # fresh copies, at rest, unless the orchestra carries its state
    var start (if carry (opt params 'start 0) 0)                             # this call's time 0 in the piece (with 'continue)
    each (zip players (vec->list (range (length players)))) (function (p) (put! (head p) 'k (last p)))
    var method (opt params 'method 'random)
    var table (if (equal? method 'markov) (markov-table (get params 'markov-score)) nil)
    var out (list)
    var t 0
    var voice-k 0
    var phase 0                                                              # a slow rate's share of an event, accumulated a quarter second at a time
    var unplayable (list)                                                    # what was asked of the orchestra that it cannot play, reported once each
    var stats (map players (function (p) (list 0 0)))                        # per player: notes, seconds played, in this call
    var due-count 0
    var skipped-busy 0
    var skipped-unplayable 0
    var substituted 0
    var events 0                                                             # events that wrote notes
    var state (record (list 'covered (list)))                                # the 'cluster method's memory: how often each pitch has sounded
    var groups (unique (filter (map players (function (p) (get p 'group))) (function (g) (>= g 0))))
    var has-loose (any? players (function (p) (< (get p 'group) 0)))
    var gd (opt params 'group-density nil)
    var strict (opt params 'dynamics-strict 0)
    var balance (opt params 'balance nil)
    var just (opt params 'just 0)
    while (< t secs) {
        var T (+ start t)                                                    # the time in the piece: what the players' busy times are in
        # the rate: 'density, or with 'group-density one rate per paired group (their sum, the event then going to a
        # group with a probability proportional to its rate; players outside any group keep 'density)
        var rates (if (equal? (type gd) "nil") nil (map groups (function (g) (max 0 (draw-in (value-at (getidx gd (min g (- (length gd) 1))) t))))))
        var loose-rate (if (or (equal? (type gd) "nil") has-loose) (max 0 (draw-in (param-at params 'density t (list 1 1)))) 0)
        var rate (max 0.001 (if (equal? (type rates) "nil") loose-rate (+ loose-rate (sum (vec rates)))))
        # time advances by the event's interval when the rate is at least one a second (exact, for a regular pulse);
        # a slower rate advances a quarter second at a time and the event comes when its share is due, so a change
        # of density during a long wait (a silence ending) is seen without a jump over it
        var step (if (<= (/ 1 rate) 1) (/ 1 rate) 0.25)
        var due 1
        if (> (/ 1 rate) 1) { set phase (+ phase (* 0.25 rate))
                              set due (>= phase 1)
                              if due { set phase (- phase 1) } }
        var gsel nil                                                         # the group this event goes to (with 'group-density); -1 the players outside any group
        if (and due (not (equal? (type rates) "nil"))) {
            var u (* (rand) rate)
            if (< u loose-rate) { set gsel -1 }
            var acc loose-rate
            var j 0
            while (and (equal? (type gsel) "nil") (< j (length groups))) {
                set acc (+ acc (getidx rates j))
                if (< u acc) { set gsel (getidx groups j) }
                set j (+ j 1)
            }
            if (equal? (type gsel) "nil") { set gsel (last groups) }
        }
        var reg (param-at params 'register t (list 3 5))
        var lo (* 12 (+ 1 (head reg)))
        var hi (- (* 12 (+ 2 (last reg))) 1)
        var dur (draw-duration (param-at params 'duration t (list 0.2 1)) (param-at params 'duration-law t 'uniform))
        var style-set (param-at params 'styles t (list 'ord))                 # the styles the event may use: each player takes one it has
        set style-set (if (equal? (type style-set) "list") style-set (list style-set))
        var tech (pick-from style-set)                                         # for the target method (the others choose per player below)
        var dv (param-at params 'dynamics t (list 'mf))
        var dyn (if (dynamics-level? dv) (level->dynamics (draw-in dv)) (pick-from dv))   # a level 0..1 (interpolated) or a set of labels
        var gain (if (dynamics-level? dv) (level->gain (draw-in dv)) 1)
        var dkey (if strict dyn nil)                                           # the dynamics as a constraint on the samples, when strict
        var seat-secs (param-at params 'seat t 0)
        var inertia (draw-in (param-at params 'inertia t 0))
        var leap (param-at params 'leap t nil)
        var spread (param-at params 'spread t 0)
        var weigh (band-weigher lo hi (draw-in (param-at params 'weight t 0)) (draw-in (param-at params 'tilt t 0)))
        var chord-set (param-at params 'chords t nil)
        var weight (draw-in (param-at params 'chord-weight t 1))
        var chord (if (or (equal? (type chord-set) "nil") (>= (rand) weight)) nil (pick-from chord-set))     # one chord of the set, or free
        # a free player (or a free paired group): the first whose last note is over; a group sounds together with
        # probability 'coupling (1 by default), else its members go one at a time
        var coupling (draw-in (param-at params 'coupling t 1))
        # a player is free when its last note is over; with 'continue, when none of its bookings (the notes of every
        # call so far, in any order of time) covers this moment, and it keeps a note only until its next booking
        var free-till (function (q) {                                        # when the player is next booked, from T (1e9: never); nil: not free now
            if (not carry) { return (if (<= (get q 'busy-until) T) 1e9 nil) }
            var st (opt q 'starts nil)
            if (or (equal? (type st) "nil") (== (length st) 0)) { return 1e9 }
            if (> (sum (* (<= st T) (> (get q 'ends) T))) 0) { return nil }
            return (min (+ st (* 1e9 (<= st T))))
        })
        var free (filter players (function (p) { var f (free-till p)
                                                 if (equal? (type f) "nil") { return 0 }
                                                 put! p 'till f
                                                 return (> (- f T) 0.05) }))
        var poly (round (draw-in (param-at params 'polyphony t 1000)))
        if (and due (== (length free) 0) (equal? method 'target) (opt params 'steal 1)) {   # the density is due and nobody is free: the oldest note gives up its player
            each (range 1) (function (k) {
                var held (filter players (function (p) (> (get p 'busy-until) T)))
                if (> (length held) 0) {
                    var oldest (min-by held (function (p) (opt p 'since 0)))
                    var cur (opt oldest 'current nil)
                    if (not (equal? (type cur) "nil")) { setidx cur 1 (max 0.05 (- t (head cur))) }
                    put! oldest 'busy-until T
                    push free oldest
                }
            })
        }
        var busy (- (length players) (length free))
        # the event goes to players able to play it: those with samples of a style of the set (and, when strict, at
        # the dynamics) in the register, on the instrument they are seated at (an ossia player with a 'seat keeps its
        # instrument; without one the band decides the instrument). The target method chooses its own from the
        # target's peaks. None free but some in the orchestra: the event is skipped (a saturation, counted); none in
        # the orchestra at all: the parameters ask for what this orchestra cannot play, and that is reported
        var instrs-of (function (q) { var s (opt q 'seat nil)
                                      if (and (> seat-secs 0) (not (equal? (type s) "nil")) (< T (+ (opt q 'seat-since 0) seat-secs))) { return (list s) }
                                      return (get q 'instrs) })
        var memo (list)                                                        # the styles a set of instruments has here, once per set this event
        var styles-for (function (ins) { var key (join ins "|")
                                         if (has? memo key) { return (get memo key) }
                                         var got (player-styles db (list (list 'instrs ins)) style-set dkey lo hi)
                                         put! memo key got
                                         return got })
        var styles-of (function (q) (styles-for (instrs-of q)))
        var in-group (function (q) (or (equal? (type gsel) "nil") (== (get q 'group) gsel)))
        var pool (filter free in-group)
        var able (if (equal? method 'target) free (filter pool (function (q) (> (length (styles-of q)) 0))))
        if due { set due-count (+ due-count 1) }
        if (and due (not (equal? method 'target)) (== (length able) 0)) {
            if (not (any? (filter players in-group) (function (q) (> (length (styles-for (get q 'instrs))) 0)))) {
                set skipped-unplayable (+ skipped-unplayable 1)
                var key (concat (str style-set) "|" (str (round (head reg))) "|" (str (round (last reg))) "|" (if strict (str dyn) ""))
                if (not (has? unplayable key)) {
                    put! unplayable key (record (list 'at t 'styles style-set 'register reg 'dynamics (if strict dyn nil)))
                    if (<= (length unplayable) 4) { print "orchestrate-granular: no instrument of the orchestra has samples of" (concat (str style-set) (if strict (concat " at " (str dyn)) "")) "in octaves" (fixed (head reg) 1) "-" (fixed (last reg) 1) "(at" (fixed t 1) "s): unplayable, nothing written" }
                    if (== (length unplayable) 5) { print "orchestrate-granular: ... and more of the same (each style set and register once)" }
                }
            } { set skipped-busy (+ skipped-busy 1) }
        }
        if (and due (> (length able) 0) (>= busy poly)) { set skipped-busy (+ skipped-busy 1) }
        if (and due (> (length able) 0) (< busy poly)) {
            set events (+ events 1)
            var p (getidx able (floor (* (rand) (length able))))
            var group (take (if (or (< (get p 'group) 0) (>= (rand) coupling)) (list p) (filter able (function (q) (== (get q 'group) (get p 'group))))) (max 1 (- poly busy)))   # within the polyphony
            if (equal? method 'target) {
                # a morphological orchestration: the notes for the free players from the target's spectrum now, less what
                # still sounds (target-notes: a pursuit at the target's peaks), each lasting as long as the target keeps
                # its sound, unless a 'duration of your own is given
                var sounding (map (filter players (function (q) (> (get q 'busy-until) T))) (function (q) (get q 'current-note)))
                each (target-notes db (get params 'target) t free sounding params) (function (f) {
                    var q (head f)
                    var n (getidx f 1)
                    put! n 'player (get q 'k)
                    var cdur (if (has? params 'duration) (draw-in (param-at params 'duration t (list 0.1 0.3))) (last f))
                    var triple (list t cdur n)
                    push out triple
                    put! q 'current triple
                    put! q 'current-note n
                    put! q 'since t
                    put! q 'busy-until (+ T cdur)
                    if carry { put! q 'starts (vec (opt q 'starts (vec)) (vec T))
                               put! q 'ends (vec (opt q 'ends (vec)) (vec (+ T cdur))) }
                    put! q 'last-pitch (get n 'midi)
                    var st (getidx stats (get q 'k))
                    setidx st 0 (+ (head st) 1)
                    setidx st 1 (+ (last st) cdur)
                    set voice-k (+ voice-k 1)
                })
            } {
                each group (function (q) {
                    var have (styles-of q)
                    var lt (opt q 'last-tech nil)
                    var qtech (if (and (not (equal? (type lt) "nil")) (< (rand) inertia) (contains? (map have str) (str lt))) lt (pick-from have))   # a style of the set this player has samples of, in the register; its last one with 'inertia
                    var ins (instrs-of q)
                    var playable (player-pitches db (list (list 'instrs ins)) qtech dkey lo hi)     # those samples' pitches
                    var lp (get q 'last-pitch)
                    if (and (not (equal? (type leap) "nil")) (not (equal? (type lp) "nil"))) {   # within the leap of the last pitch, when the player has samples there
                        var near (if (equal? (type leap) "list")
                                     (filter playable (function (m) (and (>= (- m lp) (head leap)) (<= (- m lp) (last leap)))))   # a signed range: (list -3 -1) descends
                                     (filter playable (function (m) (<= (abs (- m lp)) leap))))
                        if (> (length near) 0) { set playable near }
                    }
                    var pitch (choose-pitch db q method lo hi chord params t table voice-k playable state weigh)
                    var instr (instrument-for db ins pitch qtech dkey)
                    var n (note db instr pitch dyn qtech)
                    put! n 'player (get q 'k)                                       # who played it (the player's index in the orchestra)
                    if (and just (equal? method 'harmonic)) {                       # just intonation: a partial of the fundamental carries its cents
                        var c (just-cents (opt params 'fundamental "C2") pitch 16)
                        if (not (equal? (type c) "nil")) { note-cents! n c }
                    }
                    if (not (equal? (get (get n 'entry) 'dyn) (str dyn))) { set substituted (+ substituted 1) }
                    var off (* (rand) (draw-in spread))
                    var ndur (if carry (max 0.05 (min dur (- (get q 'till) T off))) dur)        # until the player's next booking
                    push out (list (+ t off) ndur (put n 'gain (* gain (balance-gain balance instr))))
                    put! q 'busy-until (+ T off ndur)
                    if carry { put! q 'starts (vec (opt q 'starts (vec)) (vec (+ T off)))
                               put! q 'ends (vec (opt q 'ends (vec)) (vec (+ T off ndur))) }
                    put! q 'last-pitch pitch
                    put! q 'last-tech qtech
                    if (and (> seat-secs 0) (> (length (get q 'instrs)) 1) (not (equal? (opt q 'seat nil) instr))) { put! q 'seat instr
                                                                                      put! q 'seat-since T }
                    var st (getidx stats (get q 'k))
                    setidx st 0 (+ (head st) 1)
                    setidx st 1 (+ (last st) ndur)
                    set voice-k (+ voice-k 1)
                })
            }
        }
        set t (+ t step)
    }
    var report (record (list 'seconds secs 'due due-count 'events events 'notes (length out) 'skipped-busy skipped-busy 'skipped-unplayable skipped-unplayable
                             'substituted substituted 'saturation (/ skipped-busy (max 1 due-count)) 'unplayable (values unplayable)
                             'players (map (zip players stats) (function (p) (record (list 'instrs (get (head p) 'instrs) 'notes (head (last p)) 'busy (min 1 (/ (last (last p)) (max 1e-9 secs))) 'seat (opt (head p) 'seat nil)))))))
    return (list out report)
}
# (granulation-report result)   a granular orchestration's report printed: the events due and written, the saturation
#                          (events skipped with every able player busy), the unplayable ones, the dynamics substituted,
#                          and every player's notes and busy fraction; => the report record
function granulation-report (result) {
    var r (get result 'report)
    print "granulation:" (get r 'due) "events due," (get r 'events) "played," (get r 'notes) "notes written in" (fixed (get r 'seconds) 1) "s;" (get r 'skipped-busy) "skipped, every able player busy (saturation" (fixed (* 100 (get r 'saturation)) 0) "%);" (get r 'skipped-unplayable) "unplayable;" (get r 'substituted) "at another dynamics than asked"
    each (get r 'unplayable) (function (u) (print "  unplayable:" (concat (str (get u 'styles)) (if (equal? (type (get u 'dynamics)) "nil") "" (concat " at " (str (get u 'dynamics))))) "in octaves" (fixed (head (get u 'register)) 1) "-" (fixed (last (get u 'register)) 1) "(first at" (fixed (get u 'at) 1) "s)"))
    each (get r 'players) (function (p) (print "  " (concat (join (get p 'instrs) "|") (if (equal? (type (get p 'seat)) "nil") "" (concat " (seated: " (str (get p 'seat)) ")")) ":") (get p 'notes) "notes, busy" (fixed (* 100 (get p 'busy)) 0) "%"))
    return r
}
# the instrument of a player for a pitch: among its alternatives (an ossia), one recorded at that pitch with the
# technique (and the dynamics, when one is given)
function instrument-for (db instrs pitch tech dyn) {
    if (== (length instrs) 1) { return (head instrs) }
    var with-tech (filter instrs (function (i) (contains? (db-pitches-at db i tech dyn) pitch)))       # an instrument recorded at that pitch with that technique
    if (> (length with-tech) 0) { return (pick-from with-tech) }
    error "instrument-for: none of " instrs " was recorded at " (midi->pitch pitch) " with " tech
}
# (db-pitches-of db instr tech)   the MIDI pitches an instrument was recorded at with a technique (any dynamics), sorted;
#                          tech nil: with any technique. From the index, kept once computed. What a granulated
#                          player may be asked to play: a real player's pitch is one the database proves.
function db-pitches-of (db instr tech) (db-pitches-at db instr tech nil)
# (db-pitches-at db instr tech dyn)   the MIDI pitches an instrument was recorded at with a technique at one dynamics
#                          (nil: any dynamics; tech nil: any technique), sorted; what a strict granulator draws from
function db-pitches-at (db instr tech dyn) {
    var slot (opt (db-index! db) (str instr) nil)
    if (equal? (type slot) "nil") { return (list) }
    var key (pitches-key tech dyn)
    if (has? slot key) { return (get slot key) }
    var es (if (equal? (type tech) "nil") (get slot 'all) (filter (get slot 'all) (function (e) (equal? (get e 'tech) (str tech)))))
    if (not (equal? (type dyn) "nil")) { set es (filter es (function (e) (equal? (get e 'dyn) (str dyn)))) }
    var ps (sort-list (unique (map es (function (e) (get e 'midi)))))
    put! slot key ps
    put! slot (concat key "|vec") (if (== (length ps) 0) (vec) (vec ps))                 # as a vector too: the range test is then three vector ops
    return ps
}
function pitches-key (tech dyn) (concat "pitches|" (if (equal? (type tech) "nil") "*" (str tech)) (if (equal? (type dyn) "nil") "" (concat "|" (str dyn))))
# (instrument-has? db instr tech dyn lo hi)   has an instrument samples with a technique (and, dyn given, at that
#                          dynamics) inside lo..hi (MIDI)? (a vector test: what the granulator asks hundreds of times an event)
function instrument-has? (db instr tech dyn lo hi) {
    var slot (opt (db-index! db) (str instr) nil)
    if (equal? (type slot) "nil") { return 0 }
    var key (concat (pitches-key tech dyn) "|vec")
    if (not (has? slot key)) { db-pitches-at db instr tech dyn }
    var v (get slot key)
    if (== (length v) 0) { return 0 }
    return (> (sum (* (>= v lo) (<= v hi))) 0)
}
# (player-styles db player styles dyn lo hi)   the styles of a set a player has samples of inside lo..hi (MIDI), at a
#                          dynamics when one is given (nil: any): what the event's style set leaves this player; empty
#                          when it can play none of them there
function player-styles (db player styles dyn lo hi) (filter styles (function (tech) (any? (get player 'instrs) (function (i) (instrument-has? db i tech dyn lo hi)))))
# (player-pitches db player tech dyn lo hi)   the pitches a player may be given for an event: the samples of its
#                          instruments with the technique asked for (and the dynamics, when one is given), inside the
#                          register lo..hi (MIDI); the constraints select, they are never relaxed: an empty list means
#                          this player cannot play what the event asks for (no instrument of it was recorded so in that
#                          range) and the event goes to another player; => a sorted list
function player-pitches (db player tech dyn lo hi) {
    var all (vec)
    each (get player 'instrs) (function (i) {                           # each instrument's sorted pitch vector, sliced to the register
        var slot (opt (db-index! db) (str i) nil)
        if (not (equal? (type slot) "nil")) {
            var key (concat (pitches-key tech dyn) "|vec")
            if (not (has? slot key)) { db-pitches-at db i tech dyn }
            var v (get slot key)
            if (> (length v) 0) {
                var start (sum (< v lo))
                var end (sum (<= v hi))
                if (> end start) { set all (vec all (slice v start (- end start))) }
            }
        }
    })
    if (== (length all) 0) { return (list) }
    return (sort-list (unique (vec->list all)))
}
# a pitch for a player by the method, within lo..hi (MIDI), among the playable ones; weigh the band's weighting
function choose-pitch (db player method lo hi chord params t table voice-k playable state weigh) {
    var nearest (function (m) (min-by playable (function (p) (abs (- p m)))))       # the playable pitch nearest to a wanted one
    var in-register (function (m) {                          # moved by octaves into lo..hi, then clamped
        var q m
        while (< q lo) { set q (+ q 12) }
        while (> q hi) { set q (- q 12) }
        return (max lo (min hi q)) })
    var chord-pitches (if (equal? (type chord) "nil") nil (map chord pitch->number))
    if (equal? method 'cluster) {
        # coverage: the playable pitches that have sounded the least so far, one of them; every pitch of the band
        # is thus given once before any comes twice (the counts are shared by the players)
        var covered (get state 'covered)
        var least (min-of (map playable (function (m) (opt covered m 0))))
        var fresh (filter playable (function (m) (== (opt covered m 0) least)))
        var m (random-pitch fresh chord-pitches weigh)
        put! covered m (+ 1 (opt covered m 0))
        return m
    }
    if (equal? method 'pivots) {
        var centres (if (equal? (type chord-pitches) "nil") (list (/ (+ lo hi) 2)) chord-pitches)
        var centre (in-register (getidx centres (mod voice-k (length centres))))
        var reach (draw-in (param-at params 'interval t 3))
        var around (filter playable (function (m) (<= (abs (- m centre)) reach)))         # the playable pitches within the interval of the pivot
        if (> (length around) 0) { return (pick-weighted around weigh) }
        return (nearest centre)
    }
    if (equal? method 'markov) {
        var prev (get player 'last-pitch)
        var row (if (equal? (type prev) "nil") nil (opt table prev nil))
        if (not (equal? (type row) "nil")) { var can (filter (map row in-register) (function (m) (contains? playable m)))
                                             return (if (> (length can) 0) (pick-from can) (nearest (in-register (pick-from row)))) }
        if (> (length table) 0) { return (nearest (in-register (pick-from (keys table)))) }    # start from a pitch the model knows
        return (random-pitch playable chord-pitches weigh)
    }
    if (equal? method 'harmonic) {
        if (< (rand) (draw-in (param-at params 'harmonicity t 0.7))) {
            var partials (filter (harmonic-series (opt params 'fundamental "C2") 16) (function (m) (contains? playable m)))   # the partials this player can play
            if (> (length partials) 0) { return (pick-weighted partials weigh) }
        }
        return (random-pitch playable chord-pitches weigh)
    }
    return (random-pitch playable chord-pitches weigh)                # 'random and 'chordinterp (the env already interpolates chords)
}
# a random pitch among the playable ones, from a chord's pitch classes when there is one (any octave among them); the
# playable ones when the chord has none of them; weigh a weighting of the band (nil: even)
function random-pitch (playable chord-pitches weigh) {
    var pick (function (L) (if (equal? (type weigh) "nil") (pick-from L) (pick-weighted L weigh)))
    if (equal? (type chord-pitches) "nil") { return (pick playable) }
    var candidates (filter playable (function (m) (any? chord-pitches (function (c) (== (mod m 12) (mod c 12))))))
    if (== (length candidates) 0) { return (pick playable) }
    return (pick candidates)
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
# (target-analyse x sr block hop)   what a morphological orchestration reads off a sound, computed once: the frame
#                          spectra (block as the database's features, so a frame and a sound's spectrum compare), the
#                          attacks (the peaks of the spectral flux) and their density in events a second, the register
#                          (the centroid in MIDI pitch, the spread around it in octaves, the lowest strong partial) and
#                          the loudness in dB, each a curve sampled every hop; => a record ('sr 'block 'hop 'seconds
#                          'times 'spectra 'fine 'attacks 'density 'centroid 'spread 'low 'loudness); 'fine holds a
#                          second set of frame spectra with a long window ('fine-block, 8192: 5 Hz bins at 44.1 kHz)
#                          from which the peaks are read, since the database's block tells a low note's pitch too
#                          coarsely. (target-analyse-with x sr block hop opts) the same with options: 'threshold (2: a
#                          flux peak counts above this times the local median; 1.5 counts the ripples of a held sound
#                          too), 'window (1 s: the span the density is counted over), 'fine
function target-analyse (x sr block hop) (target-analyse-with x sr block hop (record (list)))
function target-analyse-with (x sr block hop opts) {
    var spectra (frame-spectra x block hop (/ block 2))
    var fine-block (opt opts 'fine 8192)
    var n (length spectra)
    var fine (frame-spectra x fine-block hop (/ fine-block 2))
    while (< (length fine) n) { push fine (last fine) }                        # the long window ends a few frames early: the last one held
    set fine (take fine n)
    var step (/ hop sr)
    var secs (/ (length x) sr)
    var attacks (flux-peaks x sr 1024 256 (opt opts 'threshold 2) 21)
    var reg (register-curve spectra sr block)
    var smooth (function (v) (moving-median (take v n) 5))                  # the frame-to-frame jitter of the estimates taken out
    return (record (list 'sr sr 'block block 'hop hop 'seconds secs 'times (* (range n) step) 'spectra spectra 'fine fine 'fine-block fine-block
                         'attacks attacks 'density (take (peak-density attacks secs (opt opts 'window 1) step) n)
                         'centroid (smooth (head reg)) 'spread (smooth (getidx reg 1)) 'low (smooth (last reg))
                         'loudness (smooth (loudness-curve x sr hop))))
}
# (moving-median v n)      each value replaced by the median of the n around it
function moving-median (v n) {
    var h (floor (/ n 2))
    return (vec (map (vec->list (range (length v))) (function (k) (median (slice v (max 0 (- k h)) (- (min (length v) (+ k h 1)) (max 0 (- k h))))))))
}
# (target-frame target t)  the index of the frame at a time; (target-at target key t) a curve's value at a time
function target-frame (target t) (max 0 (min (- (length (get target 'spectra)) 1) (floor (* t (/ (get target 'sr) (get target 'hop))))))
function target-at (target key t) (getidx (get target key) (target-frame target t))
# (target-spectrum target k n)   the spectrum of frame k, its first n bins (a database may keep fewer coefficients than the block gives)
function target-spectrum (target k n) (take (getidx (get target 'spectra) k) n)
# (target-now target t n look)   the target's spectrum at t as the pursuit sees it: the frames from t over the next look
#                          seconds averaged (0.1 s: the attack's smear evened out, the frequencies of what starts read
#                          from the sound settled), the first n bins
function target-now (target t n look) (frames-mean (get target 'spectra) (target-frame target t) (target-frame target (+ t look)) n)
function frames-mean (spectra k0 k1 n) {
    var acc (take (getidx spectra k0) n)
    var k (+ k0 1)
    while (<= k k1) { set acc (+ acc (take (getidx spectra k) n))
                      set k (+ k 1) }
    return (/ acc (+ 1 (- k1 k0)))
}
# (target-level target t range)   the loudness at t as a dynamics level 0..1: 1 at the target's loudest point, 0 at
#                          range dB under it (40 by default)
function target-level (target t range) (max 0 (min 1 (+ 1 (/ (- (target-at target 'loudness t) (max (get target 'loudness))) range))))
# (curve->env times values step maker)   an env from a curve: one breakpoint every step seconds (the values averaged over
#                          the step), each passed through maker (a number or a range out)
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
# (target-envelopes target opts)   the granulator's envelopes from a target's curves: 'density from the flux peaks a
#                          second times 'density-scale (1: as the target; 2 twice as busy), at least 'min-density
#                          (0.5: a held sound is re-attacked now and then), 'register
#                          from the centroid and the spread (centroid ± 'width times the spread, in octaves, extended
#                          down to the lowest strong partial: a bass note lies under the centroid of its own spectrum),
#                          'dynamics as a level from the loudness (target-level, 'range dB); opts also 'step (seconds
#                          per breakpoint, 0.25)
function target-envelopes (target opts) {
    var step (opt opts 'step 0.25)
    var width (opt opts 'width 1)
    var floor-d (opt opts 'min-density 0.5)
    var scale (opt opts 'density-scale 1)
    var range-db (opt opts 'range 40)
    var ts (get target 'times)
    var dens (curve->env ts (get target 'density) step (function (v) (max floor-d (* scale v))))
    var oct (function (m) (/ (- m 12) 12))
    var lo (curve->env ts (min (get target 'low) (- (get target 'centroid) (* width 12 (get target 'spread)))) step (function (v) (max 0 (oct v))))
    var hi (curve->env ts (+ (get target 'centroid) (* width 12 (get target 'spread))) step (function (v) (min 8.5 (oct v))))
    var reg (env (reduce (zip lo hi) (function (acc p) (concat-list acc (list (head (head p)) (list (last (head p)) (max (+ 0.5 (last (head p))) (last (last p))))))) (list)))
    var loud (get target 'loudness)
    var dyn (curve->env ts (max 0 (min 1 (+ 1 (/ (- loud (max loud)) range-db)))) step (function (v) v))
    return (record (list 'density dens 'register reg 'dynamics dyn))
}
# (target-peaks target t n)   the frequencies (Hz) of the n strongest spectral peaks of the target at t (the fine frames
#                          over the next 0.1 s averaged), above 30 Hz and a twentieth of the strongest, each refined by
#                          a parabola: the pitches the notes may take, as the partials of a sound are the pitches an
#                          orchestra can double it with; => a vector
function target-peaks (target t n) {
    var block (get target 'fine-block)
    var m (frames-mean (get target 'fine) (target-frame target t) (target-frame target (+ t 0.1)) (/ block 2))
    var top (max m)
    if (<= top 0) { return (vec) }
    var ks (filter (vec->list (local-maxima m)) (function (k) (and (> (getidx m k) (* 0.05 top)) (> (* k (/ (get target 'sr) block)) 30))))
    var best (take (sort-by ks (function (k) (- 0 (getidx m k)))) n)
    return (vec (map best (function (k) {
        var a (getidx m (- k 1))
        var b (getidx m k)
        var c (getidx m (+ k 1))
        var d (- (+ a c) (* 2 b))
        return (* (+ k (if (== d 0) 0 (/ (- a c) (* 2 d)))) (/ (get target 'sr) block)) })))
}
# (entry-atom e)           a database entry's spectrum in the pursuit's space, kept on the entry once computed
function entry-atom (e) {
    if (not (has? e 'atom)) { put! e 'atom (pursuit-space (get e 'features)) }
    return (get e 'atom)
}
# (entry-f0 e sr block)    the frequency a database entry actually sounds at: the peak of its average spectrum nearest
#                          its nominal pitch (within a semitone, refined by a parabola), kept on the entry; the nominal
#                          frequency when the spectrum has no peak there. sr and block as the features were computed
function entry-f0 (e sr block) {
    if (has? e 'f0) { return (get e 'f0) }
    var m (get e 'features)
    var nominal (midi->hz (get e 'midi))
    var bin (/ nominal (/ sr block))
    var lo (max 1 (floor (/ bin 1.06)))
    var hi (min (- (length m) 2) (ceil (* bin 1.06)))
    var best -1
    var k lo
    while (<= k hi) { if (and (> (getidx m k) (getidx m (- k 1))) (>= (getidx m k) (getidx m (+ k 1))) (or (< best 0) (> (getidx m k) (getidx m best)))) { set best k }
                      set k (+ k 1) }
    if (< best 0) { put! e 'f0 nominal } {
        var a (getidx m (- best 1))
        var b (getidx m best)
        var c (getidx m (+ best 1))
        var d (- (+ a c) (* 2 b))
        put! e 'f0 (* (+ best (if (== d 0) 0 (/ (- a c) (* 2 d)))) (/ sr block)) }
    return (get e 'f0)
}
# (target-dictionary db peaks players lo hi dyn styles)   the sounds a pursuit may draw on: for every player the
#                          available sounds of its instruments whose pitch is one of the target's peaks (frequencies;
#                          within 60 cents) and within lo..hi (MIDI), with the dynamics dyn (any when the instrument has
#                          not got it) and one of the styles (all when nil); => a list of (list player entry)
function target-dictionary (db peaks players lo hi dyn styles) {
    var idx (db-index! db)
    var keys (list)                                                        # the MIDI pitches the peaks allow, each once
    each (vec->list peaks) (function (f) { var m (hz->midi f)
                                          each (list -1 0 1) (function (d) { var p (+ (round m) d)
                                                                             if (and (<= (abs (- m p)) 0.6) (>= p lo) (<= p hi) (not (contains? keys p))) { push keys p } }) })
    var out (list)
    each players (function (q) (each (get q 'instrs) (function (i) {
        var slot (opt idx (str i) nil)
        if (not (equal? (type slot) "nil")) {
            var by (db-by-midi! slot)
            var ok (list)
            each keys (function (p) (each (opt by (str p) (list)) (function (e) (if (or (equal? (type styles) "nil") (contains? (map styles str) (get e 'tech))) (push ok e)))))
            var same-dyn (filter ok (function (e) (equal? (get e 'dyn) (str dyn))))
            each (if (> (length same-dyn) 0) same-dyn ok) (function (e) (push out (list q e)))
        }
    })))
    return out
}
# (db-by-midi! slot)       an instrument's index slot grouped by MIDI pitch (built once): a record from the pitch (as a
#                          string) to its entries
function db-by-midi! (slot) {
    if (has? slot 'by-midi) { return (get slot 'by-midi) }
    var by (list)
    each (get slot 'all) (function (e) { var key (str (get e 'midi))
                                        if (has? by key) { push (get by key) e } { put! by key (list e) } })
    put! slot 'by-midi by
    return by
}
# (pursuit-space m)        a magnitude spectrum as the pursuit compares it: its square root, so that the quieter notes of
#                          a chord weigh against the loudest one (in the linear magnitudes one strong partial decides
#                          everything; in the log the noise between the partials does)
function pursuit-space (m) (sqrt (max m 0))
# (target-residual target t sounding n)   the target's spectrum at t (its first n bins, in the pursuit's space) less what
#                          the sounding notes account for (their spectra refitted to the frame together, weights
#                          non-negative); => (list residual frame)
function target-residual (target t sounding n) {
    var frame (pursuit-space (target-now target t n 0.1))
    if (== (length sounding) 0) { return (list frame frame) }
    var D (map sounding (function (n) (get n 'atom)))
    var fit (mp frame D (length sounding) 0 (record (list 'nonneg 1 'orthogonal 1)))
    return (list (max 0 (get fit 'residual)) frame)
}
# (atom-persistence target features t persist max-length)   how long a sound goes on being part of the target from t:
#                          the frames ahead in which the target still holds it (the projection of the frame on the
#                          sound's spectrum, in the linear magnitudes, stays at least persist of its peak within the
#                          first 0.1 s: 0.5 is 6 dB down), in seconds up to max-length; the length of a note whose
#                          sound the target keeps, short for one it drops
function atom-persistence (target features t persist max-length) {
    var spectra (get target 'spectra)
    var step (/ (get target 'hop) (get target 'sr))
    var k0 (target-frame target t)
    var k1 (target-frame target (+ t 0.1))
    var n (length features)
    var peak 0
    var k k0
    while (<= k k1) { set peak (max peak (dot features (target-spectrum target k n)))
                      set k (+ k 1) }
    if (<= peak 1e-12) { return 0 }
    var cap (min (length spectra) (+ k0 (ceil (/ max-length step))))
    while (and (< k cap) (>= (dot features (target-spectrum target k n)) (* persist peak))) { set k (+ k 1) }
    return (* (- k k0) step)
}
# techniques that are short by nature (an attack and little else): what a note whose life turned out short takes
var short-techniques (list "pizz" "stac" "secco" "col-legno-batt" "slap" "key" "bartok" "snap" "tongue-ram" "sforz" "attack")
# (technique-short? t)     does a technique name belong to the short ones?
function technique-short? (t) (any? short-techniques (function (w) (contains? (str t) w)))
# (target-notes db target t free sounding params)   the notes to start at time t on the free players: the target's spectrum
#                          now less what still sounds (target-residual), pursued (mp, non-negative) over the players'
#                          sounds at the target's peaks (target-dictionary: the register and the dynamics of the moment
#                          narrow it); the best atom always makes a note, further ones ('voices at most) when each
#                          accounts for 'min-gain of the residual; when the sounding notes leave less than 'floor of the
#                          spectrum the pursuit is over the whole spectrum instead (a doubling). Each note lasts as long
#                          as the target holds its sound (atom-persistence, 'persist, 'max-length), at least 'min-length,
#                          and a short life takes a short technique of the same sound when there is one ('short); its
#                          tuning is the nearest peak's, in cents; => a list of (list player note dur)
function target-notes (db target t free sounding params) {
    if (< (target-at target 'loudness t) (- (max (get target 'loudness)) (opt params 'range 40))) { return (list) }   # silence: under the dynamics' span
    var level (target-level target t (opt params 'range 40))
    var reg (param-at params 'register t (list 0 8))
    var lo (* 12 (+ 1 (head reg)))
    var hi (- (* 12 (+ 2 (last reg))) 1)
    var dyn (level->dynamics level)
    var peaks (target-peaks target t 24)
    var atoms (target-dictionary db peaks free lo hi dyn (opt params 'styles nil))
    if (== (length atoms) 0) { set atoms (target-dictionary db peaks free 0 127 dyn (opt params 'styles nil)) }     # nothing in the register: any
    if (== (length atoms) 0) { return (list) }
    var rf (target-residual target t sounding (length (get (last (head atoms)) 'features)))
    var frame (last rf)
    var residual (if (< (norm (head rf)) (* (opt params 'floor 0.1) (norm frame))) frame (head rf))
    if (<= (norm frame) 1e-9) { return (list) }                                 # silence
    var D (map atoms (function (a) (entry-atom (last a))))
    var r (mp residual D (max 1 (min (length free) (opt params 'voices 2))) 0.05 (record (list 'nonneg 1)))
    var rnorm (norm residual)
    var out (list)
    var taken (list)
    each (zip (get r 'atoms) (get r 'weights)) (function (aw) {
        var a (getidx atoms (head aw))
        var q (head a)
        var e (last a)
        var explains (/ (* (last aw) (norm (entry-atom e))) (max 1e-9 rnorm))
        if (opt params 'trace 0) { print "  " t (get e 'instr) (get e 'pitch) (get e 'tech) "weight" (fixed (last aw) 3) "explains" (fixed explains 2) }
        if (and (not (contains? taken q)) (> (last aw) 0) (or (== (length out) 0) (>= explains (opt params 'min-gain 0.2)))) {
            push taken q
            var life (max (opt params 'min-length 0.1) (atom-persistence target (get e 'features) t (opt params 'persist 0.5) (opt params 'max-length 8)))
            var entry e
            if (and (< life (opt params 'short 0.4)) (not (technique-short? (get e 'tech)))) {          # a short life: a short technique of the same sound, the best fitting
                var shorts (filter (get (opt (db-index! db) (get e 'instr) (record (list 'all (list)))) 'all) (function (b) (and (== (get b 'midi) (get e 'midi)) (technique-short? (get b 'tech)) (or (equal? (type (opt params 'styles nil)) "nil") (contains? (map (get params 'styles) str) (get b 'tech))))))   # any dynamics
                if (> (length shorts) 0) { set entry (min-by shorts (function (s) (- 0 (/ (dot residual (entry-atom s)) (max 1e-9 (norm (entry-atom s))))))) }
            }
            var n (note db (get entry 'instr) (get entry 'midi) (get entry 'dyn) (get entry 'tech))   # the sound the pursuit chose, exactly
            put! n 'dyn (str dyn)                                                                    # played at the target's level
            put! n 'label (concat (get entry 'instr) " " (get n 'pitch) " " (str dyn))
            put! n 'gain (level->gain level)
            put! n 'atom (entry-atom entry)
            var f0 (entry-f0 entry (get target 'sr) (get target 'block))
            if (> (length peaks) 0) {                                                  # the tuning: the nearest peak, against the sound's own frequency, within a quarter tone
                var near (getidx peaks (argmin (abs (- peaks f0))))
                var cents (* 1200 (log2 (/ (max near 1) f0)))
                put! n 'cents (if (< (abs cents) 50) (round cents) 0)
            } { put! n 'cents 0 }
            push out (list q n life)
        }
    })
    return out
}
# (orchestrate-morphological db orch target params)   orchestration as granular synthesis whose process is read off a
#     target sound (target-analyse), no segmentation: the density of the flux peaks gives the events a second, the
#     centroid and the spread the register, the loudness the dynamics (target-envelopes); at every event a matching
#     pursuit over the free players' sounds at the target's spectral peaks picks instrument, pitch and technique for
#     the target's spectrum at that instant, less what already sounds (target-notes), and each note lasts as long as
#     the target keeps its sound (atom-persistence). When every player is busy and an event is due, the oldest note
#     gives way ('steal, 1 by default). params: 'voices (2: notes an event may start), 'min-gain (0.2), 'floor (0.2),
#     'persist (0.5), 'min-length (0.1), 'max-length (8), 'short (0.4), 'width (1: the register's width in spreads),
#     'range (40 dB: the dynamics' span), 'density-scale (1), 'min-density (0.5), 'step, 'styles (the techniques
#     allowed; all by default),
#     'solutions, 'trace (1: every atom considered is printed), and 'density, 'register, 'dynamics or 'duration of
#     your own in place of the target's
function orchestrate-morphological (db orch target params) {
    var envs (target-envelopes target params)
    var p (record (list 'method 'target 'target target))
    each envs (function (kv) (put! p (head kv) (last kv)))
    each params (function (kv) (if (not (contains? (list 'step 'width 'range 'min-density 'density-scale 'solutions) (head kv))) (put! p (head kv) (last kv))))
    var n (opt params 'solutions 1)
    var secs (get target 'seconds)
    var runs (map (vec->list (range n)) (function (k) (granulate db orch secs p)))
    var sols (map (zip (vec->list (range n)) runs) (function (q) (list (head q) (head (last q)))))
    var r (orchestration 'morphological params (list (segment 0 secs sols)))
    put! r 'reports (map runs last)
    put! r 'report (last (head runs))
    return r
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

# --- is a score playable by an orchestra? ------------------------------------------------------------------------
# (score-validate s orch)  checks a score against an orchestra, as a copyist would: at every moment each sounding
#                          note must have a player of its own able to play its instrument (an ossia player "Vn|Va"
#                          counts for either, a paired group is just players), and every note must be at a pitch the
#                          instrument was recorded at (a shifted note is one the database does not prove). Notes of
#                          scores inside the score count too. => a record: 'ok; 'overbooked the moments where the
#                          players ran out, each (list at instr sounding), one per note that had no player; 'transposed
#                          the notes with a shift, (list at instr pitch shift); 'peak how many notes of each instrument
#                          sounded at once at most, a record; 'players the orchestra's size. (validation-print r) prints it
function score-validate (s orch) {
    var notes (sort-by (filter (score-flatten s 0) (function (e) (equal? (get e 'kind) 'note))) (function (e) (get e 'at)))
    var n (length notes)
    var np (length orch)
    var able (map orch (function (p) (map (get p 'instrs) str)))            # the instruments of each player
    var holder (map orch (function (p) -1))                                  # the note (index) each player holds, or -1
    var of-note (map notes (function (e) -1))                                # the player of each note, or -1
    var overbooked (list)
    var transposed (list)
    var peak (list)
    var sounding (list)                                                      # the notes now sounding (indices), by end time
    # the matching kept maximum note by note: a new note takes a free able player, or takes an able player whose
    # note moves to another (an augmenting path), and only fails when no such chain exists
    var seek nil
    set seek (function (j visited) {
        var instr (get (getidx notes j) 'instr)
        var found 0
        var k 0
        while (and (not found) (< k np)) {
            if (and (not (getidx visited k)) (contains? (getidx able k) instr)) {
                setidx visited k 1
                var h (getidx holder k)
                if (or (< h 0) (seek h visited)) { setidx holder k j
                                                   setidx of-note j k
                                                   set found 1 }
            }
            set k (+ k 1)
        }
        return found
    })
    each (range n) (function (j) {
        var e (getidx notes j)
        var at (get e 'at)
        # the notes over by now give up their players
        set sounding (filter sounding (function (i) {
            var over (<= (event-end (getidx notes i)) (+ at 1e-6))
            if over { var k (getidx of-note i)
                      if (>= k 0) { setidx holder k -1 } }
            return (not over) }))
        if (!= (opt e 'shift 0) 0) { push transposed (list at (get e 'instr) (get e 'pitch) (get e 'shift)) }
        push sounding j
        var instr (get e 'instr)
        var now (length (filter sounding (function (i) (equal? (get (getidx notes i) 'instr) instr))))
        if (> now (opt peak instr 0)) { put! peak instr now }
        if (not (seek j (map orch (function (p) 0)))) { push overbooked (list at instr (length sounding)) }
    })
    return (record (list 'ok (and (== (length overbooked) 0) (== (length transposed) 0)) 'overbooked overbooked 'transposed transposed 'peak peak 'players np 'notes n))
}
# (validation-print r)     a validation printed: playable or not, and why not
function validation-print (r) {
    if (get r 'ok) { print "playable:" (get r 'notes) "notes by" (get r 'players) "players; at most" (join (map (get r 'peak) (function (kv) (concat (head kv) " " (str (last kv))))) ", ") "at once" } {
      print "NOT playable:" (length (get r 'overbooked)) "notes with no player free (of" (get r 'notes) "notes by" (get r 'players) "players)," (length (get r 'transposed)) "at a pitch the instrument was not recorded at"
      each (take (get r 'overbooked) 6) (function (o) (print "  at" (fixed (head o) 2) "s:" (getidx o 1) "has no player;" (last o) "notes sounding"))
      if (> (length (get r 'overbooked)) 6) { print "  ..." (- (length (get r 'overbooked)) 6) "more" }
      each (take (get r 'transposed) 6) (function (o) (print "  at" (fixed (head o) 2) "s:" (getidx o 1) (getidx o 2) "shifted by" (last o) "semitones"))
      if (> (length (get r 'transposed)) 6) { print "  ..." (- (length (get r 'transposed)) 6) "more" } }
    return (get r 'ok)
}

# --- mimetic orchestration: Orchidea --------------------------------------------------------------------------------
# Assisted orchestration: a target sound is cut into segments at its onsets, each segment analysed into the features
# of the database (as db-gen makes them) and into the pitches of its partials, and for each segment a genetic search
# looks for the combination of sounds of the orchestra (one sound per player, or none) whose summed features are
# nearest the target's, with an asymmetric distance (a partial the target lacks costs more than one it has and the
# solution lacks). The search keeps its best solutions, ranked; a connection chooses one per segment so that the
# segments follow each other (Orchidea's search core: src/music/orchidea.h).
# (mimetic-target db x sr params)   the target analysed: x a vector (a sound read with read-wav, a rendered score, a
#                          synthesis) at sr; params a record: 'segmentation ('flux: at the peaks of the spectral flux
#                          above 'threshold (2; above 1 no peak passes: one segment, a static target) and at least a
#                          'timegate apart (0.1 s); 'adaptive: the peaks of the flux against their local median
#                          (flux-peaks, 'ratio 2: twice the median), so the sensitivity follows the sound rather than
#                          its loudest attack; 'frames: every block; 'none: one segment; or a list of onsets in
#                          seconds), 'partials-window (32768: the window the pitches are read with; large for a low
#                          target), 'partials (0.2: the threshold on the partials, 0..1; 0: no pitch filter, every
#                          sound of the database may serve: for noisy targets), 'extra-pitches (names added to every
#                          segment's pitches). => a record ('sr 'seconds 'segments (records: at dur features notes)
#                          'params); (mimetic-print target) prints the segments and their pitches
function mimetic-target (db x sr params) {
    var segmentation (opt params 'segmentation 'flux)
    if (equal? segmentation 'adaptive) {                                   # the onsets of signals' flux-peaks: a peak counts against its local median, not the file's maximum
        var peaks (vec->list (flux-peaks x sr (get db 'block) (get db 'hop) (opt params 'ratio 2) 21))
        var gate (opt params 'timegate 0.1)
        var kept (list)
        each peaks (function (t) (if (or (== (length kept) 0) (> (- t (last kept)) gate)) (push kept t)))
        set segmentation (if (== (length kept) 0) (list 0) kept)
    }
    var opts (record (list 'segmentation segmentation 'threshold (opt params 'threshold 2) 'timegate (opt params 'timegate 0.1)
                           'partials-window (opt params 'partials-window 32768) 'partials (opt params 'partials 0.2) 'extra-pitches (map (opt params 'extra-pitches (list)) str)))
    var segs (orchidea-analyse x sr (get db 'type) (get db 'block) (get db 'hop) (get db 'ncoeff) opts)
    return (record (list 'sr sr 'seconds (/ (length x) sr) 'segments segs 'params params))
}
function mimetic-print (target) {
    var segs (get target 'segments)
    print "target:" (fixed (get target 'seconds) 2) "s," (length segs) (if (== (length segs) 1) "segment (static)" "segments")
    each (zip (take segs (min 12 (length segs))) (vec->list (range (min 12 (length segs))))) (function (p) {
        var g (head p)
        var notes (get g 'notes)
        print "  " (+ 1 (last p)) ": at" (fixed (get g 'at) 2) "s," (fixed (get g 'dur) 2) "s;" (if (== (length notes) 0) "no pitch filter" (concat (str (length notes)) " pitches: " (join (map notes (function (kv) (concat (head kv) (if (== (last kv) 0) "" (concat " (" (str (last kv)) ")"))))) " ")))
    })
    if (> (length segs) 12) { print "  ..." (- (length segs) 12) "more" }
    return target
}
# (note-of-entry db e)     a note event from a database entry itself (the very sound the search chose, not the nearest
#                          by pitch and dynamics as note picks)
function note-of-entry (db e) (record (list 'kind 'note 'source (get e 'file) 'db db 'entry e 'instr (get e 'instr) 'pitch (get e 'pitch) 'midi (get e 'midi)
                                            'dyn (get e 'dyn) 'tech (get e 'tech) 'shift 0 'cents 0 'label (concat (get e 'instr) " " (get e 'pitch) " " (get e 'dyn))))
# (mimetic-seating instr)  where an instrument sits, as an azimuth in degrees (left positive): the seating Orchidea's
#                          mixes use (violins left, cellos right, the winds in the middle); 0 for one it does not know
var mimetic-seats (record (list 'Vn 30 'Va -5 'Vc -30 'Cb -20 'Fl 0 'Picc 0 'Ob -10 'ClBb 10 'Cl 10 'Bn -5 'Hn 20 'TpC 10 'Tp 10 'Tbn -5 'BTb -15 'Hp 20 'Acc 20 'ASax -20))
function mimetic-seating (instr) { var hit (find-first mimetic-seats (function (kv) (equal? (str (head kv)) (str instr))))
                                   return (if (equal? (type hit) "nil") 0 (last hit)) }
# (orchestrate-mimetic db orch x sr params)   Orchidea: the target x (a vector at sr, or a target from mimetic-target)
#     orchestrated by the players of orch (an orchestra: a symbol is one player, "Fl|Picc" a doubling), one sound
#     per player per segment (or a rest), chosen by the genetic search. params is a record (Orchidea's names in
#     brackets):
#       the target   'segmentation 'threshold 'timegate 'partials-window 'partials 'extra-pitches (mimetic-target)
#       the search   'population (pop_size, 300), 'epochs (max_epochs, 300), 'pursuit (0: a random first population;
#                    k: from the k sounds nearest the target per player), 'crossover (xover_rate, 0.8), 'mutation
#                    (mutation_rate, 0.01), 'sparsity (0.001: how often a player is dropped; 0.1 for a single
#                    instrument as target), 'positive and 'negative (0.5 and 10: the penalisations of a partial the
#                    solution has and the target lacks, and the reverse), 'regularization (0: > 0 sponsors sparse
#                    solutions)
#       the space    'styles 'dynamics 'others: lists of names; only sounds with one of them enter the search
#       the memory   'hysteresis (0: > 0 makes each segment's search lean, in timbre, towards the solutions chosen
#                    for the segments before, fading by that factor a segment back), 'dovetail (0: > 0 raises the cost
#                    of a candidate by that fraction of the pitches of the previous chosen solution it drops, on any
#                    player: pivot notes passed between instruments), 'octaves (the pitch filter widened: (list 0 -1)
#                    admits the partials' pitches and the octave below), 'hold (seconds: a note continued across
#                    segments on the same player and pitch is never longer; the player starts it again)
#       the result   'solutions (10: the best kept per segment), 'connection ('closest: each segment's solution
#                    nearest, in timbre, to the one chosen for the segment before, Orchidea's; 'best: the best of
#                    each; 'path: the shortest path through the solutions, each costing its distance from its
#                    segment's best in percent plus 'movement (1) per semitone a player moves, or per player that
#                    starts or stops: the least melodic movement, the continuity of lines), 'seating (1: the notes
#                    placed as Orchidea's mixes seat the orchestra; 0: centred), 'seed, 'quiet (1: no progress)
#     => an orchestration result ('mimetic) whose segments hold the ranked solutions as (list cost fragment), each
#     fragment the notes at their time in the target (the very sounds chosen, at the target's cents, with 'player
#     the player's index); 'choices the connection's solution per segment, (mimetic-connection result) that
#     connection as a fragment, (best-connection result) the best of each; 'target the analysed target; 'curves
#     the fitness per epoch of each segment. connect! places a connection in a score (a note continued across a
#     boundary on the same player and pitch becomes one longer note)
function orchestrate-mimetic (db orch x sr params) {
    var target (if (and (equal? (type x) "list") (has? x 'segments)) x (mimetic-target db x sr params))
    var players (map orch (function (p) (join (get p 'instrs) "|")))
    var seating (opt params 'seating 1)
    var t0 (clock)
    var r (orchidea-search (get db 'entries) players (get target 'segments) params)
    var segs (list)
    each (zip (get target 'segments) (get r 'segments)) (function (pair) {
        var g (head pair)
        var m (last pair)
        var slots (get m 'players)
        var notes (get g 'notes)
        var sols (map (get m 'solutions) (function (sol) {
            var f (list)
            each (zip (last sol) slots) (function (q) {
                if (>= (head q) 0) {
                    var e (getidx (get db 'entries) (head q))
                    var n (note-of-entry db e)
                    put! n 'player (last q)
                    put! n 'cents (opt notes (get e 'pitch) 0)
                    if seating { put! n 'az (mimetic-seating (get e 'instr)) }
                    push f (list (get g 'at) (get g 'dur) n)
                }
            })
            return (list (head sol) f)
        }))
        push segs (segment (get g 'at) (get g 'dur) sols)
    })
    var out (orchestration 'mimetic params segs)
    put! out 'choices (get r 'choices)
    put! out 'hold (opt params 'hold 1e9)
    put! out 'target target
    put! out 'players players
    put! out 'curves (map (get r 'segments) (function (m) (get m 'curve)))
    if (not (opt params 'quiet 0)) { var costs (map segs (function (g) (fixed (head (head (get g 'solutions))) 3)))
                                     print "orchestrate-mimetic:" (length segs) (if (== (length segs) 1) "segment," "segments,") (sum (vec (map segs (function (g) (length (get g 'solutions)))))) "solutions in" (fixed (- (clock) t0) 1) "s; the best costs" (join (take costs (min 12 (length costs))) " ") (if (> (length costs) 12) "..." "") }
    return out
}
# (mimetic-connection result)   the connection Orchidea chose (result 'choices) as a fragment; (connect! s at result
#                          (get result 'choices)) places it
function mimetic-connection (result) (connection result (get result 'choices))
# (solution-print result k)   the solutions of segment k printed, with their costs and sounds
function solution-print (result k) {
    var g (getidx (get result 'segments) k)
    print "segment" (+ k 1) "at" (fixed (get g 'at) 2) "s," (fixed (get g 'dur) 2) "s:" (length (get g 'solutions)) "solutions"
    each (zip (get g 'solutions) (vec->list (range (length (get g 'solutions))))) (function (p) {
        print "  " (+ 1 (last p)) (concat "(cost " (str (fixed (head (head p)) 3)) "):") (join (map (last (head p)) (function (x) (concat (get (last x) 'instr) " " (get (last x) 'pitch) " " (get (last x) 'dyn) " " (get (last x) 'tech)))) ", ")
    })
    return nil
}

# --- saving and loading: Orchidea's connection format --------------------------------------------------------
# A score's notes can be written as an Orchidea "connection" (the text Orchidea exports and its Max patches read):
#   [ orchestra Fl Ob Vn ... ]
#   [ segment <onset ms>
#       [ solution 1
#           [ note <duration ms> <instrument> <style> <pitch> <dynamics> <other> <file> <cents> ]
#       ] ]
# One segment per distinct onset, one solution each. Only notes travel: a file, buffer, synth, call or inner score has
# no place in the format and is left out (score-save says how many); a note's gain is not kept either (the sample's
# dynamics are). Loading finds each note's sound in a database by its file (any of the database's folders), or by
# instrument, pitch, dynamics and style when the file is not there; an unpitched sound (pitch N) becomes a file event.
# (score-save s path)      write the score's notes as a connection; => the number of notes written
function score-save (s path) {
    var notes (sort-by (filter (get s 'events) (function (e) (equal? (get e 'kind) 'note))) (function (e) (get e 'at)))
    var skipped (- (length (get s 'events)) (length notes))
    var lines (list (concat "[ orchestra " (join (unique (map notes (function (e) (get e 'instr)))) " ") " ]"))
    var k 0
    while (< k (length notes)) {
        var at (get (getidx notes k) 'at)
        push lines (concat "[ segment " (fixed (* 1000 at) 2))
        push lines "\t[ solution 1"
        while (and (< k (length notes)) (< (abs (- (get (getidx notes k) 'at) at)) 0.0005)) {
            var e (getidx notes k)
            var entry (get e 'entry)
            push lines (concat "\t\t[ note " (fixed (* 1000 (get e 'dur)) 2) " " (get e 'instr) " " (get e 'tech) " " (get e 'pitch) " " (get e 'dyn) " " (opt entry 'other "N") " " (get e 'source) " " (str (round (opt e 'cents 0))) " ]")
            set k (+ k 1)
        }
        push lines "\t]"
        push lines "]"
    }
    write path (join lines "\n")
    if (> skipped 0) { print "score-save:" skipped "events that are not notes left out (files, buffers, synths, calls, scores)" }
    return (length notes)
}
# (score-load db path name)   a score from a connection file, its notes' sounds from db; => the score
function score-load (db path name) {
    var s (score name 44100)
    var at 0
    var missing 0
    each (split (read path) "\n") (function (line) {
        var toks (filter (split (trim line) " ") (function (t) (not (equal? t ""))))
        if (>= (length toks) 3) {
            if (equal? (getidx toks 1) "segment") { set at (/ (num (getidx toks 2)) 1000) }
            if (and (equal? (getidx toks 1) "note") (>= (length toks) 10)) {
                var dur (/ (num (getidx toks 2)) 1000)
                var instr (getidx toks 3)
                var tech (getidx toks 4)
                var pitch (getidx toks 5)
                var dyn (getidx toks 6)
                var file (getidx toks 8)
                var cents (num (getidx toks 9))
                var n (try (note-from-file db file instr pitch dyn tech) catch err nil)
                if (equal? (type n) "nil") { set missing (+ missing 1) } {
                    if (not (equal? (get n 'kind) 'note)) { event s at dur n } {
                        put! n 'cents cents
                        put! n 'dyn dyn
                        put! n 'label (concat instr " " pitch " " dyn)
                        event s at dur n
                    }
                }
            }
        }
    })
    if (> missing 0) { print "score-load:" missing "notes whose sound is not in the database were left out" }
    return s
}
# (note-from-file db file instr pitch dyn tech)   a note whose sound is the database entry with that file name (the
#                          folder may differ); when the file is not there, the note by its fields; an unpitched sound
#                          (pitch N) as a file event
function note-from-file (db file instr pitch dyn tech) {
    var name (filename file)
    var slot (opt (db-index! db) instr nil)
    var hit (if (equal? (type slot) "nil") nil (find-first (get slot 'all) (function (e) (equal? (filename (get e 'file)) name))))
    if (equal? (type hit) "nil") { set hit (find-first (get db 'entries) (function (e) (and (equal? (filename (get e 'file)) name) (db-available? db e)))) }
    if (and (not (equal? (type hit) "nil")) (< (get hit 'midi) 0)) { return (payload->event (db-path db hit)) }
    if (not (equal? (type hit) "nil")) { return (note db (get hit 'instr) (get hit 'midi) (get hit 'dyn) (get hit 'tech)) }
    if (equal? pitch "N") { error "note-from-file: unpitched sound not in the database: " file }
    return (note db instr pitch dyn tech)
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
