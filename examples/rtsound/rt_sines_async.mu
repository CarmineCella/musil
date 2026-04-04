load("signals.mu")
load ("stdlib.mu")

# --- simple sine generator

proc sine(freq, dur, amp, sr) {
    var n = floor(dur * sr)
    var x = zeros(n)

    var i = 0
    while (i < n) {
        x[i] = amp * sin(2 * PI * freq * i / sr)
        i = i + 1
    }

    return x
}

var sr = 48000

# the sounds are mixed
# sound 1
play_async(sine(220, 10, 0.2, sr))
sleep(2000)

# sound 2 
play_async(sine(330, 10, 0.2, sr))
sleep(500)

# sound 3 
play_async(sine(660, 10, 0.2, sr))

