# features.mu
# Low-level audio features in Musil

load("stdlib.mu")
load ("signals.mu")

var N     = 4096
var hop   = N / 4
var SR    = 44100
var frame = N / SR

# use only positive-frequency half of spectrum for descriptors
var Nspec = N / 2

# assume readwav returns [sr, channels]
# and channels is an array of vectors
var wav = readwav("../../data/Vox.wav")
var sig = wav[1][0]

# assume stft returns an array of spectra
var data    = stft(sig, N, hop)
var nframes = len(data)

# frequencies for 0 .. SR/2 over Nspec bins
var freqs = bpf(0, Nspec, SR / 2.0)

# previous magnitude spectrum (for flux)
var oamps = zeros(Nspec)

# pre-allocate feature vectors
var centr  = zeros(nframes)
var spread = zeros(nframes)
var skew   = zeros(nframes)
var kurt   = zeros(nframes)
var flux   = zeros(nframes)
var irr    = zeros(nframes)
var decr   = zeros(nframes)
var f0b    = zeros(nframes)
var nrg    = zeros(nframes)
var zx     = zeros(nframes)

var i = 0

while (i < nframes) {
    # frame -> mag/phase packed representation
    var magphi   = car2pol(data[i])
    var magphi_d = deinterleave(magphi)

    # first half = mags, second half = phases
    var amps_full = head(magphi_d)
    var phis      = tail(magphi_d)

    # keep only positive-frequency half
    var amps = vslice(amps_full, 0, Nspec)

    # ---------- frequency-domain features ----------
    var c  = speccent(amps, freqs)
    var s  = specspread(amps, freqs, c)

    var sk = specskew(amps, freqs, c, s)
    var ku = speckurt(amps, freqs, c, s)
    var fl = specflux(amps, oamps)
    var ir = specirr(amps)
    var de = specdecr(amps)

    centr[i]  = c
    spread[i] = s
    skew[i]   = sk
    kurt[i]   = ku
    flux[i]   = fl
    irr[i]    = ir
    decr[i]   = de

    # update previous magnitude for flux
    oamps = amps

    # ---------- time-domain features ----------
    var seg = vslice(sig, i * hop, N)

    var f0 = acorrf0(seg, SR)
    var en = energy(seg)
    var zc = zcr(seg)

    f0b[i] = f0
    nrg[i] = en
    zx[i]  = zc

    i = i + 1
}

print len(centr) " " len(f0b) "\n"

# ---------- plot spectral features ----------
var spec_svg =
    plot("Spectral features",
         centr,  "centroid",
         spread, "spread",
         skew,   "skewness",
         kurt,   "kurtosis",
         flux,   "flux",
         irr,    "irregularity",
         decr,   "decrease",
         "*")

# approximate ZCR in Hz (for comparison with f0)
var zx_hz = zx * (SR / 2.0)

# ---------- plot f0 and zero-crossing ----------
f0b = median(f0b, 5)
var len_f0     = len(f0b)
var zx_hz_plot = vslice(zx_hz, 0, len_f0)

var f0_svg =
    plot("Fundamental frequency",
        #  f0b,        "autocorr f0",
         zx_hz_plot, "zero-crossing (Hz approx.)",
         "*")

# ---------- plot energy ----------
var energy_svg =
    plot("Energy",
         nrg, "energy",
         "*")

# ---------- helper: build a piecewise-linear envelope from successive values ----------
proc make_env(v, hop) {
    var nv = len(v)
    if (nv < 2) {
        return zeros(0)
    }

    var out = zeros((nv - 1) * hop)
    var i = 0
    while (i < nv - 1) {
        var seg = bpf(v[i], hop, v[i + 1])
        out = vaddat(out, i * hop, seg)
        i = i + 1
    }
    return out
}

# ---------- resynthesis from f0 and energy ----------
var f0s      = make_env(f0b, hop)
var amps_env = make_env(nrg, hop)

var t1    = gen(4096, 1, 0.73, 0.6, 0.20)
var synth = amps_env * osc(SR, f0s, t1)

# assume writewav(path, sr, [channel1, channel2, ...])
writewav("/tmp/f0_synth.wav", SR, [synth])

# eof