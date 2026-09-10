# plot.mu — plotting, Musil half: builds figure descriptions for plot.h to render.
#
# Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#
# Not loaded automatically: (load "plot.mu"). Needs signals.mu (and so scientific.mu and std.mu).
# A figure is (list title layers options). Build one with (figure "title"), add layers
# with add-line / add-scatter / add-bars / add-image / add-surface, set options with
# set-option, then (show fig) or (save-png fig "file.png" [w h]). The one-call helpers
# below (plot, scatter, bars, image, surface) build and show in one go.
# show and save-png come from the host: the CLI opens a window (or none when the
# environment variable MUSIL_NOSHOW is set), the Listener shows the figure in a panel.
load "signals.mu"

# --- building figures ----------------------------------------------------------
# (figure title)           an empty figure
function figure (title) (list title (list) (list))
# (fig-title fig) (fig-layers fig) (fig-options fig) the three parts of a figure
function fig-title (fig) (head fig)
# (fig-layers fig) the layers
function fig-layers (fig) (getidx fig 1)
# (fig-options fig) the options
function fig-options (fig) (getidx fig 2)
# (add-line fig x y label)     x may be nil to use 0, 1, 2, ...
function add-line (fig x y label) (add-layer fig (list "line" (x-or-index x y) y label))
# (add-scatter fig x y label) points; (add-bars fig x y label) bars
function add-scatter (fig x y label) (add-layer fig (list "scatter" (x-or-index x y) y label))
# (add-bars fig x y label) bars
function add-bars (fig x y label) (add-layer fig (list "bars" (x-or-index x y) y label))
# (add-image fig M)        a matrix as a heat map, row 0 at the bottom
function add-image (fig M) (add-layer fig (list "image" M))
# (add-surface fig M)      a matrix as a 3D surface (a figure with a surface shows only that)
function add-surface (fig M) (add-layer fig (list "surface" M))
# (add-layer fig layer) add a raw layer (list kind data ...)
function add-layer (fig layer) {
    push (fig-layers fig) layer
    return fig
}
# (x-or-index x y) x, or 0 1 2 ... when x is nil
function x-or-index (x y) (if (equal? (type x) "nil") (range (length y)) x)
# (set-option fig key value)   keys: "xlabel" "ylabel" "xmin" "xmax" "ymin" "ymax"
function set-option (fig key value) {
    push (fig-options fig) (list key value)
    return fig
}
# (set-labels fig xlabel ylabel) axis labels; (set-xrange fig lo hi) (set-yrange fig lo hi) axis limits
function set-labels (fig xlabel ylabel) (set-option (set-option fig "xlabel" xlabel) "ylabel" ylabel)
# (set-xrange fig lo hi) x-axis limits
function set-xrange (fig lo hi) (set-option (set-option fig "xmin" lo) "xmax" hi)
# (set-yrange fig lo hi) y-axis limits
function set-yrange (fig lo hi) (set-option (set-option fig "ymin" lo) "ymax" hi)

# --- one-call helpers: build and show ------------------------------------------------
# (plot y) or (plot-xy x y)      a line
function plot (y) (show (add-line (figure "") nil y ""))
# (plot-xy x y) a line of y against x
function plot-xy (x y) (show (add-line (figure "") x y ""))
# (plot-lines ys labels)        several lines sharing the index axis
function plot-lines (ys labels) {
    var fig (figure "")
    each (zip ys labels) (function (p) (add-line fig nil (head p) (last p)))
    return (show fig)
}
# (scatter x y) (bars y) (image M) (surface M) build and show a one-layer figure
function scatter (x y) (show (add-scatter (figure "") x y ""))
# (bars y) build and show a bar chart
function bars (y) (show (add-bars (figure "") nil y ""))
# (image M) build and show a heat map
function image (M) (show (add-image (figure "") M))
# (surface M) build and show a 3D surface
function surface (M) (show (add-surface (figure "") M))

# --- ready-made figures for the other libraries ------------------------------------
# (waveform x sr)          a signal against time in seconds
function waveform (x sr) (set-labels (add-line (figure "waveform") (/ (range (length x)) sr) x "") "time (s)" "amplitude")
# (spectrum x sr)          magnitude spectrum in dB against frequency
function spectrum (x sr) {
    var m (magnitude-spectrum (* x (hann (length x))))
    var f (spectrum-freqs (* 2 (length m)) sr)
    return (set-labels (add-line (figure "spectrum") f (db m) "") "frequency (Hz)" "dB")
}
# (spectrogram x sr n hop)   STFT magnitudes in dB as an image: time along x, frequency along y
function spectrogram (x sr n hop) {
    var mags (stft-magnitudes (stft x n hop))
    var M (transpose (map mags db))            # rows = frequency bins, columns = frames
    return (set-labels (add-image (figure "spectrogram") M) "frame" "bin")
}
# (scatter-clusters X labels k)   the rows of an n x 2 matrix coloured by cluster
function scatter-clusters (X labels k) {
    var fig (figure "clusters")
    each (range k) (function (c) {
        var rows (filter (zip X (vec->list labels)) (function (p) (== (last p) c)))
        var pts (map rows head)
        if (> (length pts) 0) { add-scatter fig (vec (map pts head)) (vec (map pts last)) (concat "cluster " c) }
    })
    return fig
}
# (histogram-figure v bins)   a bar chart of (histogram v bins)
function histogram-figure (v bins) {
    var h (histogram v bins)
    var edges (last h)
    var centers (/ (+ (take edges bins) (drop edges 1)) 2)
    return (add-bars (figure "histogram") centers (head h) "")
}
