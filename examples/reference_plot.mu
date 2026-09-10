# musil — library reference: plot (plot.h + plot.mu)
#
# A figure is (list title layers options). plot.mu builds it; the host renders it:
# (show fig) opens a window (the CLI) or a panel (the Listener), (save-png fig path
# [w h]) writes a file. The one-call helpers plot, scatter, bars, image, surface build
# and show in one go. plot.mu loads signals.mu (and so scientific and std).
# Run with: musil reference_plot.mu     (set MUSIL_NOSHOW=1 to skip the windows)

load "system.mu"
load "plot.mu"
print ""
print "================================================================"
print "  musil: library reference (plot)"
print "================================================================"
var sr 8000

# --- 1. Building a figure by hand --------------------------------------------------
print ""
print "--- building ---"
var fig (figure "two sines")
add-line fig nil (sin (/ (range 100) 8)) "sin"
add-line fig nil (cos (/ (range 100) 8)) "cos"
add-scatter fig (range 0 100 10) (zeros 10) "zeros"
set-labels fig "sample" "value"
set-yrange fig -1.5 1.5
print "title   :" (fig-title fig)
print "layers  :" (length (fig-layers fig)) "; kinds:" (map (fig-layers fig) head)
print "options :" (fig-options fig)
print "a layer :" (list (head (head (fig-layers fig))) "x y ..." (last (head (fig-layers fig))))
save-png fig "/tmp/musil_ref_lines.png"
print "save-png: /tmp/musil_ref_lines.png" (file-size "/tmp/musil_ref_lines.png") "bytes"
save-png fig "/tmp/musil_ref_small.png" 400 250
print "save-png with a size:" (> (file-size "/tmp/musil_ref_small.png") 0)

# --- 2. Layer kinds ----------------------------------------------------------------
print ""
print "--- layer kinds ---"
print "line, scatter, bars : x y label (x may be nil for 0, 1, 2, ...)"
print "image               : a matrix as a heat map, row 0 at the bottom"
print "surface             : a matrix as a 3D surface, drag or arrow keys to orbit"
var M (map (vec->list (range 30)) (function (r) (* (sin (/ r 5)) (cos (/ (range 40) 5)))))
save-png (add-bars (figure "bars") nil (vec 3 1 4 1 5 9 2 6) "") "/tmp/musil_ref_bars.png"
save-png (add-image (figure "image") M) "/tmp/musil_ref_image.png"
save-png (add-surface (figure "surface") M) "/tmp/musil_ref_surface.png"
print "wrote bars, image, surface:" (map (list "/tmp/musil_ref_bars.png" "/tmp/musil_ref_image.png" "/tmp/musil_ref_surface.png") exists?)

# --- 3. One-call helpers -------------------------------------------------------------
print ""
print "--- one-call helpers (they show; here MUSIL_NOSHOW may be set) ---"
plot (sin (/ (range 50) 5))
plot-xy (range 5) (vec 1 4 9 16 25)
plot-lines (list (sin (/ (range 50) 5)) (cos (/ (range 50) 5))) (list "sin" "cos")
scatter (rand 20) (rand 20)
bars (vec 1 2 3)
image M
surface M
print "plot plot-xy plot-lines scatter bars image surface: shown"

# --- 4. Ready-made figures for sound and data ------------------------------------------
print ""
print "--- ready-made ---"
var tone (* (sine sr 440 0.5) (exp (* -3 (/ (range 4000) sr))))
print "waveform        :" (fig-title (waveform tone sr)) (map (fig-layers (waveform tone sr)) head)
print "spectrum        :" (fig-title (spectrum tone sr)) "(dB against Hz)"
var sg (spectrogram tone sr 256 64)
print "spectrogram     :" (fig-title sg) "; image of" (nrows (getidx (head (fig-layers sg)) 1)) "bins x" (ncols (getidx (head (fig-layers sg)) 1)) "frames"
print "histogram-figure:" (fig-title (histogram-figure (rand 100) 5)) (length (getidx (head (fig-layers (histogram-figure (rand 100) 5))) 2)) "bars"
var pts (list (vec 0 0) (vec 0.2 0.1) (vec 4 4) (vec 4.1 3.9))
var km (kmeans pts 2)
print "scatter-clusters:" (length (fig-layers (scatter-clusters pts (kmeans-labels km) 2))) "layers, one per cluster"
save-png (waveform tone sr) "/tmp/musil_ref_wave.png"
save-png sg "/tmp/musil_ref_spec.png"
print "wrote waveform and spectrogram:" (exists? "/tmp/musil_ref_wave.png") (exists? "/tmp/musil_ref_spec.png")

# --- 5. Errors -------------------------------------------------------------------------
print ""
print "--- errors ---"
print "empty figure    :" (try (save-png (figure "e") "/tmp/x.png") catch e e)
print "length mismatch :" (try (save-png (add-line (figure "e") (vec 1 2) (vec 1) "") "/tmp/x.png") catch e e)
print "unknown option  :" (try (save-png (set-option (add-line (figure "e") nil (vec 1) "") "nope" 1) "/tmp/x.png") catch e e)

each (list "lines" "small" "bars" "image" "surface" "wave" "spec") (function (n) (remove (concat "/tmp/musil_ref_" n ".png")))
print ""
print "================================================================"
print "  end of plot reference"
print "================================================================"
