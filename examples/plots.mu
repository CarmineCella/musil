# plots: sound and data as pictures. Each figure is saved to /tmp and, unless MUSIL_NOSHOW
# is set, also shown in a window of its own (the windows stay until closed).
# Usage: musil plots.mu [sound.wav]   (defaults to the bundled data/ file)
load "system.mu"
load "plot.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/cage.wav"))
var sr (head w)
var x (head (getidx w 1))
var n (next-pow2 (/ sr 16))

# 1. the waveform and its spectrum
var f1 (waveform x sr)
save-png f1 "/tmp/musil_plot_waveform.png"
show f1
var f2 (spectrum (take x (* 4 n)) sr)
set-xrange f2 0 (/ sr 2)
save-png f2 "/tmp/musil_plot_spectrum.png"
show f2

# 2. the spectrogram as an image
var f3 (spectrogram x sr n (/ n 4))
save-png f3 "/tmp/musil_plot_spectrogram.png"
show f3

# 3. descriptors over time, several lines on one figure
var frames (stft-magnitudes (stft x n (/ n 4)))
var freqs (spectrum-freqs n sr)
var centroid (vec (map frames (function (a) (spectral-centroid a freqs))))
var flat (vec (map frames spectral-flatness))
var f4 (figure "descriptors per frame")
add-line f4 nil (/ centroid (max centroid)) "centroid (normalised)"
add-line f4 nil flat "flatness"
add-line f4 nil (/ (envelope-follow x (/ n 4)) (max (envelope-follow x (/ n 4)))) "rms (normalised)"
set-labels f4 "frame" ""
save-png f4 "/tmp/musil_plot_descriptors.png"
show f4

# 4. a filter's frequency response, from its impulse response
var f5 (figure "biquad responses")
each (list (list "lowpass" 500) (list "highpass" 1500) (list "bandpass" 1000)) (function (spec) {
    var ir (apply-filter (impulse 2048) (biquad (head spec) sr (last spec) 2 0))
    add-line f5 (spectrum-freqs 2048 sr) (db (take (magnitudes (fft ir)) 1024)) (concat (head spec) " " (last spec))   # |H(f)|: the fft of the impulse response, unnormalised
})
set-labels f5 "frequency (Hz)" "dB"
set-yrange f5 -60 6
save-png f5 "/tmp/musil_plot_filters.png"
show f5

# 5. a 3D view: the spectrogram as a surface
var f6 (add-surface (figure "spectrogram surface") (getidx (head (fig-layers f3)) 1))
save-png f6 "/tmp/musil_plot_surface.png"
show f6
print "saved /tmp/musil_plot_{waveform,spectrum,spectrogram,descriptors,filters,surface}.png"
