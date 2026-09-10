# test_plot.mu — self-checking test of the plot library (plot.h + plot.mu).
#
# Renders to PNG files in /tmp through a hidden window; needs a display (or Xvfb).
# Run with: MUSIL_NOSHOW=1 musil tests/test_plot.mu

load "test.mu"
load "system.mu"
load "plot.mu"

var out "/tmp/musil_test_plot.png"
function png-ok? (fig) {
    remove out
    save-png fig out
    return (and (exists? out) (> (file-size out) 1000))
}

# --- building figures ---
var f (figure "t")
check (equal? (fig-title f) "t") "figure: title"
check (equal? (fig-layers f) (list)) "figure: no layers"
check (equal? (fig-options f) (list)) "figure: no options"
add-line f nil (vec 1 2 3) "a"
check (== (length (fig-layers f)) 1) "add-line: one layer"
check (equal? (head (head (fig-layers f))) "line") "add-line: kind"
check (equal? (getidx (head (fig-layers f)) 1) (vec 0 1 2)) "add-line: nil x becomes the index"
check (equal? (getidx (head (fig-layers f)) 3) "a") "add-line: label"
add-scatter f (vec 1 2) (vec 3 4) "b"
add-bars f nil (vec 1 2) ""
check (== (length (fig-layers f)) 3) "add-scatter, add-bars"
set-labels f "x" "y"
set-xrange f 0 10
check (== (length (fig-options f)) 4) "set-labels, set-xrange"
check (equal? (head (fig-options f)) (list "xlabel" "x")) "set-option: shape"
check (equal? (add-image (figure "i") (list (vec 1 2) (vec 3 4))) (list "i" (list (list "image" (list (vec 1 2) (vec 3 4)))) (list))) "add-image"
check (equal? (head (head (fig-layers (add-surface (figure "s") (list (vec 1 2) (vec 3 4)))))) "surface") "add-surface"

# --- rendering to png ---
check (png-ok? f) "save-png: lines, scatter and bars"
check (png-ok? (add-image (figure "image") (list (vec 1 2 3) (vec 4 5 6)))) "save-png: image"
check (png-ok? (add-surface (figure "surface") (map (vec->list (range 10)) (function (r) (sin (+ r (range 12))))))) "save-png: surface"
check (png-ok? (waveform (sine 8000 100 0.02) 8000)) "waveform"
check (png-ok? (spectrum (sine 8000 1000 0.064) 8000)) "spectrum"
check (png-ok? (spectrogram (sine 8000 500 0.2) 8000 128 32)) "spectrogram"
check (png-ok? (histogram-figure (rand 200) 8)) "histogram-figure"
var X (list (vec 0 0) (vec 0.1 0) (vec 5 5) (vec 5.1 5))
check (png-ok? (scatter-clusters X (vec 0 0 1 1) 2)) "scatter-clusters"
check (== (length (fig-layers (scatter-clusters X (vec 0 0 1 1) 2))) 2) "scatter-clusters: one layer per cluster"
remove out
save-png f out 300 200
check (exists? out) "save-png: custom size"

# --- errors ---
check (contains? (error-of (function () (save-png (list "bad" 1 2) out))) "layers must be a list") "save-png: malformed figure"
check (contains? (error-of (function () (save-png (figure "e") out))) "no layers") "save-png: empty figure"
check (contains? (error-of (function () (save-png (add-line (figure "e") (vec 1 2) (vec 1) "") out))) "same length") "save-png: x/y length"
check (contains? (error-of (function () (save-png (add-image (figure "e") (list (vec 1 2) (vec 1))) out))) "ragged") "save-png: ragged image"
check (contains? (error-of (function () (save-png (set-option (add-line (figure "e") nil (vec 1) "") "nope" 1) out))) "unknown option") "save-png: unknown option"
check (contains? (error-of (function () (save-png (add-layer (figure "e") (list "circle" (vec 1))) out))) "unknown layer kind") "save-png: unknown kind"
check (contains? (error-of (function () (save-png f "/no/such/dir/x.png"))) "cannot write") "save-png: bad path"

# --- show is a no-op when MUSIL_NOSHOW is set (so this test can run unattended) ---
check (equal? (type (show f)) "nil") "show: returns nil"
check (equal? (type (plot (vec 1 2 3))) "nil") "plot: one-call helper"
check (equal? (type (image (list (vec 1 2)))) "nil") "image: one-call helper"
remove out
report "test_plot"
