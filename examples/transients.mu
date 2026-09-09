# transients: split a sound into two components by clustering its spectral frames
# Usage: musil transients.mu [sound.wav]   (defaults to the bundled data/ files)
load "system.mu"
load "signals.mu"

var w (read-wav (if (> (length args) 0) (getidx args 0) "data/Gambale_cut.wav"))
var sr (head w)
var x (head (getidx w 1))
var n (next-pow2 (/ sr 16))
var hop (/ n 4)
var frames (stft x n hop)
var polar (map frames car->pol)
var mags (map polar head)
print (length frames) "frames"

# k-means on the magnitude spectra: frames with a burst look alike, so do steady ones
var km (kmeans mags 2)
var labels (kmeans-labels km)
print "cluster sizes:" (cluster-sizes labels 2)
# the cluster whose centroid is brighter (more high-frequency content) is the transient one
var brightness (vec (map (kmeans-centroids km) (function (c) (hfc (take c (/ n 2))))))
var transient-cluster (argmax brightness)
print "transient cluster:" transient-cluster " (brightness" (fixed brightness 2) ")"

function keep (frame-polar is-transient want-transient) {
    var gate (if (== is-transient want-transient) 1 0)
    return (pol->car (list (* (head frame-polar) gate) (last frame-polar)))
}
var transient-frames (map (zip polar (vec->list labels)) (function (p) (keep (head p) (== (last p) transient-cluster) 1)))
var steady-frames (map (zip polar (vec->list labels)) (function (p) (keep (head p) (== (last p) transient-cluster) 0)))
var transients (istft transient-frames n hop)
var steady (istft steady-frames n hop)
print "transient part rms:" (fixed (rms transients) 4) "  steady part rms:" (fixed (rms steady) 4)
print "frames flagged transient, in time (1 = transient):" (vec->list (take (== labels transient-cluster) 40))
write-wav "/tmp/musil_transients.wav" sr transients
write-wav "/tmp/musil_steady.wav" sr steady
