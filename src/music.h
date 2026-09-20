// music.h — music, C++ half: sample databases in the SOL/Orchidea layout, pitch names.
// The Musil half (music.mu) holds the score: events in time, rendering, playback, the roll.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// A database is a folder of sound files and one feature file (a `.db`) at its root:
//   first line:   <type> <block size> <hop> <number of coefficients>
//   other lines:  <path relative to the root>;<f1>;<f2>;...
// Sound file names carry the metadata, separated by dashes: instrument-technique-pitch-dynamics-other
// (Vn-ord-D#5-mf-3c-N.wav); a technique may be several tokens (Vn-pizz-lv-C4-mf-...), so the pitch is the
// first token after the instrument that reads as one; a sharp may be written # or _ on disk. Only the metadata and the
// features are read here; the sounds are opened on demand (music.mu caches what it plays).

#pragma once
#include "core.h"
#include "system.h"
#include "signals.h"
#include "music/midi.h"
#include "music/orchidea.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

namespace musil {

// --- pitch names ---
// (pitch->midi "D#5") => 75: a note name with sharps (#, _) or flats (b) and an octave (C4 = 60); -1 if not a pitch
inline int pitch_to_midi(const std::string& s) {
    if (s.size() < 2) return -1;
    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    char c = (char)std::toupper((unsigned char)s[0]); if (c < 'A' || c > 'G') return -1;
    int semi = base[c - 'A']; size_t p = 1;
    while (p < s.size() && (s[p] == '#' || s[p] == '_' || s[p] == 'b' || s[p] == 'x')) { if (s[p] == 'b') semi--; else if (s[p] == 'x') semi += 2; else semi++; p++; }
    if (p >= s.size()) return -1;
    bool neg = s[p] == '-'; if (neg) p++;
    if (p >= s.size() || !std::isdigit((unsigned char)s[p])) return -1;
    int oct = 0; while (p < s.size() && std::isdigit((unsigned char)s[p])) oct = oct * 10 + (s[p++] - '0');
    if (p != s.size()) return -1;
    if (neg) oct = -oct;
    return (oct + 1) * 12 + semi;
}
inline vptr mus_pitch_to_midi(vlist& a, Interp& i) { return v_num(pitch_to_midi(i.str(a[0]))); }
// (midi->pitch 75) => "D#5"
inline std::string midi_to_pitch(int m) {
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int oct = m / 12 - 1, semi = m % 12; if (semi < 0) { semi += 12; oct--; }
    return std::string(names[semi]) + std::to_string(oct);
}
inline vptr mus_midi_to_pitch(vlist& a, Interp& i) { return v_str(midi_to_pitch((int)std::lround(i.scalar(a[0])))); }

// --- the database file ---
// (db-read path) => (list type block hop ncoeff entries): the feature file parsed; each entry a record
//   (file instr tech pitch dyn other midi features), the file relative to the database's root
inline vptr mus_db_read(vlist& a, Interp& i) {
    std::string path = i.read_path(i.str(a[0]));
    std::ifstream f(path); if (!f) i.bad("db-read: cannot open " + i.str(a[0]));
    std::string type; long block = 0, hop = 0, ncoeff = 0; f >> type >> block >> hop >> ncoeff;
    if (f.fail() || ncoeff <= 0) i.bad("db-read: the first line must be: type block hop ncoeff");
    std::string line; std::getline(f, line);
    vlist entries; long lineno = 1;
    while (std::getline(f, line)) {
        lineno++;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        size_t semi = line.find(';'); std::string file = line.substr(0, semi);
        if (file.empty()) continue;
        varr feats((size_t)ncoeff); size_t k = 0, p = semi == std::string::npos ? line.size() : semi + 1;
        while (p < line.size() && k < (size_t)ncoeff) { size_t q = line.find(';', p); feats[k++] = std::atof(line.c_str() + p); p = q == std::string::npos ? line.size() : q + 1; }
        if (k != (size_t)ncoeff) i.bad("db-read: line " + std::to_string(lineno) + " has " + std::to_string(k) + " features, expected " + std::to_string(ncoeff));
        // the name: instrument-technique-pitch-dynamics-other...
        std::string name = file; size_t sl = name.find_last_of("/\\"); if (sl != std::string::npos) name = name.substr(sl + 1);
        size_t dot = name.find_last_of('.'); if (dot != std::string::npos) name = name.substr(0, dot);
        std::vector<std::string> syms; { std::stringstream ss(name); std::string t; while (std::getline(ss, t, '-')) syms.push_back(t); }
        // instrument, then the technique (one token or several joined by dashes: pizz-lv, art-harm), then the first token
        // that is a pitch, the dynamics, and whatever follows; an unpitched sound (N, or no pitch) gets midi -1
        std::string instr = syms.empty() ? "N" : syms[0], tech = "N", pitch = "N", dyn = "N", other = "N"; int midi = -1;
        size_t pp = std::string::npos;
        for (size_t j = 1; j < syms.size(); j++) if (pitch_to_midi(syms[j]) >= 0) { pp = j; break; }
        if (pp == std::string::npos) {                        // no pitch: instrument-technique-N-dynamics... in the old layout, or unpitched
            if (syms.size() > 1) tech = syms[1]; if (syms.size() > 3) dyn = syms[3]; if (syms.size() > 2 && syms[2] != "N") pitch = syms[2];
            std::string rest; for (size_t j = 4; j < syms.size(); j++) rest += (j > 4 ? "-" : "") + syms[j]; if (!rest.empty()) other = rest;
        } else {
            std::string t; for (size_t j = 1; j < pp; j++) t += (j > 1 ? "-" : "") + syms[j]; if (!t.empty()) tech = t;
            pitch = syms[pp]; midi = pitch_to_midi(pitch); if (pp + 1 < syms.size()) dyn = syms[pp + 1];
            std::string rest; for (size_t j = pp + 2; j < syms.size(); j++) rest += (j > pp + 2 ? "-" : "") + syms[j]; if (!rest.empty()) other = rest;
        }
        entries.push_back(v_list({ v_list({ v_sym("file"), v_str(file) }), v_list({ v_sym("instr"), v_str(instr) }), v_list({ v_sym("tech"), v_str(tech) }),
                                   v_list({ v_sym("pitch"), v_str(pitch) }), v_list({ v_sym("dyn"), v_str(dyn) }), v_list({ v_sym("other"), v_str(other) }),
                                   v_list({ v_sym("midi"), v_num(midi) }), v_list({ v_sym("features"), v_arr(std::move(feats)) }) }));
    }
    return v_list({ v_str(type), v_num((double)block), v_num((double)hop), v_num((double)ncoeff), v_list(std::move(entries)) });
}

// --- making a database: the features of every sound under a folder (Orchidea's dbgen) -----------
// The features of a sound: its magnitude spectrum averaged over the frames (Hann window of block
// samples, every hop), each frame weighted by its energy, normalised by the total energy, at 44100 Hz
// (other rates are resampled, stereo is summed). Then, by type:
//   spectrum   the first ncoeff bins             logspec    their logs
//   specpeaks  the local maxima above the mean (others 0), first ncoeff bins
//   specenv    the cepstral envelope (160 coefficients), first ncoeff bins
//   mfcc       ncoeff mel-frequency cepstral coefficients (40 filters)
//   moments    centroid, spread, skewness, kurtosis (ncoeff >= 4)
inline std::vector<double> average_spectrum(const std::vector<double>& x, int block, int hop) {
    std::vector<double> win(block), avg(block, 0.0), fr(2 * block); double total = 0;
    for (int k = 0; k < block; k++) win[k] = 0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * k / block);
    for (size_t i = 0; i < x.size(); i += (size_t)hop) {
        std::fill(fr.begin(), fr.end(), 0.0); double nrg = 0; size_t rsize = std::min<size_t>((size_t)block, x.size() - i);
        for (size_t j = 0; j < rsize; j++) { fr[2 * j] = x[i + j] * win[j]; nrg += x[i + j] * x[i + j]; }
        nrg /= (double)rsize; total += nrg;
        fft_inplace(fr.data(), (size_t)block, -1);
        for (int j = 0; j < block; j++) avg[j] += std::hypot(fr[2 * j], fr[2 * j + 1]) * nrg;
    }
    if (total > 0) for (auto& v : avg) v /= total;
    return avg;
}
inline std::vector<double> db_features_of(const std::vector<double>& mono, int block, int hop, int ncoeff, const std::string& type) {
    std::vector<double> spec = average_spectrum(mono, block, hop), out((size_t)ncoeff, 0.0);
    const double sr = 44100.0, eps = 1e-12;
    if (type == "spectrum") { for (int j = 0; j < ncoeff; j++) out[j] = spec[j]; }
    else if (type == "logspec") { for (int j = 0; j < ncoeff; j++) out[j] = std::log(spec[j] + eps); }
    else if (type == "specpeaks") {
        int half = block / 2; double mean = 0; for (int j = 0; j < half; j++) mean += spec[j]; mean /= half;
        for (int j = 1; j < std::min(half - 1, ncoeff); j++) if (spec[j] > spec[j - 1] && spec[j] >= spec[j + 1] && spec[j] > mean) out[j] = spec[j];
    }
    else if (type == "specenv") {                     // cepstral smoothing: the first 160 quefrencies of the log spectrum
        std::vector<double> c(2 * block, 0.0); for (int j = 0; j < block; j++) c[2 * j] = std::log(spec[j] + eps);
        fft_inplace(c.data(), (size_t)block, -1);
        int keep = 160; for (int j = 0; j < block; j++) { bool k = j < keep || j > block - keep; c[2 * j] = k ? c[2 * j] / block : 0; c[2 * j + 1] = k ? c[2 * j + 1] / block : 0; }
        fft_inplace(c.data(), (size_t)block, 1);
        for (int j = 0; j < ncoeff; j++) out[j] = std::exp(c[2 * j]);
    }
    else if (type == "mfcc") {                        // 40 mel filters over the half spectrum, log energies, DCT-II
        int nf = 40, half = block / 2; auto mel = [](double f) { return 2595.0 * std::log10(1 + f / 700.0); }; auto imel = [](double m) { return 700.0 * (std::pow(10.0, m / 2595.0) - 1); };
        double mlo = mel(0), mhi = mel(sr / 2); std::vector<double> centers(nf + 2); for (int k = 0; k < nf + 2; k++) centers[k] = imel(mlo + (mhi - mlo) * k / (nf + 1)) / (sr / block);
        std::vector<double> e(nf, 0.0);
        for (int k = 0; k < nf; k++) { double a = centers[k], b = centers[k + 1], c2 = centers[k + 2];
            for (int j = 0; j < half; j++) { double w = j < a || j > c2 ? 0 : j <= b ? (j - a) / std::max(b - a, 1e-9) : (c2 - j) / std::max(c2 - b, 1e-9); e[k] += w * spec[j]; }
            e[k] = std::log(e[k] + eps); }
        for (int j = 0; j < ncoeff; j++) { double acc = 0; for (int k = 0; k < nf; k++) acc += e[k] * std::cos(3.14159265358979323846 * j * (k + 0.5) / nf); out[j] = acc; }
    }
    else if (type == "moments") {
        int half = block / 2; double bw = sr / block, sum = 0, c = 0; for (int j = 0; j < half; j++) { sum += spec[j]; c += j * bw * spec[j]; }
        double centroid = sum > 0 ? c / sum : 0, sp = 0, sk = 0, ku = 0;
        for (int j = 0; j < half; j++) { double d = j * bw - centroid; sp += d * d * spec[j]; }
        double spread = sum > 0 ? std::sqrt(sp / sum) : 0;
        for (int j = 0; j < half; j++) { double d = j * bw - centroid; sk += d * d * d * spec[j]; ku += d * d * d * d * spec[j]; }
        if (ncoeff < 4) throw std::runtime_error("moments: ncoeff must be >= 4");
        out[0] = centroid; out[1] = spread; out[2] = sum > 0 && spread > 0 ? sk / sum / std::pow(spread, 3) : 0; out[3] = sum > 0 && spread > 0 ? ku / sum / std::pow(spread, 4) : 0;
    }
    else throw std::runtime_error("unknown feature type " + type + " (spectrum, logspec, specpeaks, specenv, mfcc, moments)");
    return out;
}
// (db-gen folder path type block hop ncoeff) => the number of sounds analysed: every WAV under folder (recursively), its
//   features written to path in the database format (first line: type block hop ncoeff; then the file's path
//   relative to folder and the features); the file names carry the metadata (instr-tech-pitch-dyn-other).
//   Sounds that cannot be read are skipped with a message.
inline vptr mus_db_gen(vlist& a, Interp& i) {
    namespace fs = std::filesystem;
    std::string folder = i.read_path(i.str(a[0])), outpath = i.str(a[1]), type = i.str(a[2]);
    int block = (int)i.scalar(a[3]), hop = (int)i.scalar(a[4]), ncoeff = (int)i.scalar(a[5]);
    if (block < 16 || (block & (block - 1))) i.bad("block must be a power of 2 >= 16");
    if (hop < 1 || hop > block) i.bad("hop must be in 1..block");
    if (ncoeff < 1 || ncoeff > block / 2) i.bad("ncoeff must be in 1..block/2");
    std::error_code ec; if (!fs::is_directory(folder, ec)) i.bad("db-gen: not a folder: " + i.str(a[0]));
    std::vector<fs::path> files;
    for (auto& e : fs::recursive_directory_iterator(folder, ec)) { if (!e.is_regular_file(ec)) continue; std::string ext = e.path().extension().string(); for (auto& c : ext) c = (char)std::tolower((unsigned char)c); if (ext == ".wav" && e.path().filename().string()[0] != '.') files.push_back(e.path()); }
    std::sort(files.begin(), files.end());
    std::ofstream out(outpath); if (!out) i.bad("db-gen: cannot write " + outpath);
    out << type << " " << block << " " << hop << " " << ncoeff << "\n";
    size_t done = 0;
    for (size_t k = 0; k < files.size(); k++) {
        std::string rel = fs::relative(files[k], folder, ec).generic_string();
        WAVHeader h{}; std::vector<std::vector<double>> chans;
        try { chans = read_wav_raw(files[k].string().c_str(), h); } catch (std::exception& e) { *i.out << "db-gen: skipped " << rel << ": " << e.what() << "\n"; continue; }
        if (chans.empty()) continue;
        std::vector<double> mono(chans[0].size(), 0.0); for (auto& c : chans) for (size_t j = 0; j < mono.size() && j < c.size(); j++) mono[j] += c[j] / chans.size();
        if (h.sampleRate != 44100) { varr v(mono.size()); for (size_t j = 0; j < mono.size(); j++) v[j] = mono[j]; varr r = resample_sinc(v, 44100.0 / h.sampleRate); mono.assign(std::begin(r), std::end(r)); }
        std::vector<double> f;
        try { f = db_features_of(mono, block, hop, ncoeff, type); } catch (std::exception& e) { i.bad(std::string("db-gen: ") + e.what()); }
        out << "/" << rel; for (double v : f) out << ";" << (float)v; out << "\n";
        done++;
        if (files.size() >= 50 && (k + 1) % std::max<size_t>(1, files.size() / 10) == 0) { *i.out << "db-gen: " << (k + 1) << "/" << files.size() << "\n" << std::flush; }
        i.yield_check();
    }
    return v_num((double)done);
}

// --- finding the sounds: an index of every WAV under a folder, by file name ------------------------
// The paths in a feature file and the folders on disk do not always agree (TinySOL's file says
// /Strings/Violin/ordinario/..., a copy has Strings/Vn/ordinario/...; a sharp is # in one and _ in the
// other), so a sound is found by its file name: the folder is walked once and indexed by name, with the
// case and the spelling of sharps ignored.
inline std::string db_norm_name(std::string s) { size_t sl = s.find_last_of("/\\"); if (sl != std::string::npos) s = s.substr(sl + 1); for (auto& c : s) { c = (char)std::tolower((unsigned char)c); if (c == '#') c = '_'; } return s; }
inline std::map<std::string, std::string>& db_index_of(const std::string& root) {
    static std::map<std::string, std::map<std::string, std::string>> cache;
    auto it = cache.find(root); if (it != cache.end()) return it->second;
    auto& idx = cache[root]; std::error_code ec; namespace fs = std::filesystem;
    if (fs::is_directory(root, ec)) for (auto& e : fs::recursive_directory_iterator(root, ec)) { if (!e.is_regular_file(ec)) continue; std::string ext = e.path().extension().string(); for (auto& c : ext) c = (char)std::tolower((unsigned char)c); if (ext == ".wav" || ext == ".aif" || ext == ".aiff") idx.emplace(db_norm_name(e.path().filename().string()), e.path().string()); }
    return idx;
}
// (db-locate root file) => the path on disk of a sound named file (a path from a feature file) under root, found by its
//   file name whatever the folders are called; "" when it is not there. (db-index-size root) => how many sounds root holds
inline vptr mus_db_locate(vlist& a, Interp& i) {
    std::string root = i.read_path(i.str(a[0])), rel = i.str(a[1]);
    std::filesystem::path direct = std::filesystem::path(root) / (rel.size() && (rel[0] == '/' || rel[0] == '\\') ? rel.substr(1) : rel); std::error_code ec;
    if (std::filesystem::exists(direct, ec)) return v_str(direct.string());
    auto& idx = db_index_of(root); auto it = idx.find(db_norm_name(rel)); return v_str(it == idx.end() ? "" : it->second);
}
inline vptr mus_db_index_size(vlist& a, Interp& i) { return v_num((double)db_index_of(i.read_path(i.str(a[0]))).size()); }

// --- mimetic orchestration: Orchidea's search (music/orchidea.h) -------------------------------------------------
// a record's value by key (a symbol or a string), or nil
inline vptr rec_get(const vptr& rec, const char* key) {
    if (!rec || rec->t != Value::LIST) return nullptr;
    for (auto& p : rec->l) if (p && p->t == Value::LIST && p->l.size() == 2 && p->l[0] && (p->l[0]->t == Value::SYM || p->l[0]->t == Value::STR) && p->l[0]->s == key) return p->l[1];
    return nullptr;
}
// (orchidea-analyse x sr type block hop ncoeff opts)   the target's segments for a mimetic orchestration: x a vector at
//   sr (resampled to 44100 Hz, the databases' rate), cut at its onsets, each segment analysed into the features of the
//   database's type (block, hop, ncoeff, as db-gen makes them) and standardised, and into the pitches of its partials
//   with their cents. opts a record: 'segmentation ("flux": the onsets of the spectral flux above 'threshold, a
//   'timegate apart; "frames": every block; "none": one segment; or a list of onset times in seconds), 'threshold
//   (2: above 1 there is one segment, a static target), 'timegate (0.1 s), 'partials-window (32768), 'partials
//   (0.2: the threshold on the partials, 0..1, 0 for no pitch filter), 'extra-pitches (names added to every segment)
//   => a list of records (at dur features notes), notes a record from pitch name to cents
inline vptr mus_orchidea_analyse(vlist& a, Interp& i) {
    varr x = i.num(a[0]); double sr = i.scalar(a[1]); std::string type = i.str(a[2]);
    int block = (int)i.scalar(a[3]), hop = (int)i.scalar(a[4]), ncoeff = (int)i.scalar(a[5]);
    vptr opts = a[6];
    auto opt_num = [&](const char* key, double dflt) { vptr v = rec_get(opts, key); return v && v->t == Value::NUM && v->num.size() == 1 ? v->num[0] : dflt; };
    auto opt_val = [&](const char* key) { return rec_get(opts, key); };
    if (sr != 44100.0) x = resample_sinc(x, 44100.0 / sr);
    std::vector<double> mono(std::begin(x), std::end(x)); const double SR = 44100.0;
    if (mono.size() < (size_t)block) mono.resize((size_t)block, 0.0);
    // the onsets
    std::vector<double> onsets; vptr segv = opt_val("segmentation"); std::string segmentation = "flux";
    if (segv && (segv->t == Value::STR || segv->t == Value::SYM)) segmentation = segv->s;
    if (segv && segv->t == Value::LIST) { for (auto& e : segv->l) onsets.push_back(i.scalar(e)); segmentation = "list"; }
    else if (segv && segv->t == Value::NUM) { for (double t : segv->num) onsets.push_back(t); segmentation = "list"; }
    double threshold = opt_num("threshold", 2), timegate = opt_num("timegate", 0.1);
    if (segmentation == "flux") onsets = orchidea::flux_onsets(mono, block, hop, SR, threshold, timegate, [](double* d, size_t n, int sg) { fft_inplace(d, n, sg); });
    else if (segmentation == "frames") { for (size_t p = 0; p < mono.size(); p += (size_t)block) onsets.push_back((double)p / SR); }
    else if (segmentation == "none" || segmentation == "static") onsets.clear();
    else if (segmentation != "list") i.bad("orchidea-analyse: segmentation is flux, frames, none, or a list of onsets");
    if (onsets.empty()) onsets.push_back(0);
    std::sort(onsets.begin(), onsets.end()); if (onsets[0] > 0) onsets.insert(onsets.begin(), 0.0);
    // the segments
    int pwin = (int)opt_num("partials-window", 32768); double pfilter = opt_num("partials", 0.2);
    if (pwin < 64 || (pwin & (pwin - 1))) i.bad("orchidea-analyse: partials-window must be a power of two");
    std::vector<std::string> extra; if (vptr ev = opt_val("extra-pitches")) { if (ev->t == Value::LIST) for (auto& e : ev->l) extra.push_back(str_of(e)); }
    vlist out;
    for (size_t k = 0; k < onsets.size(); k++) {
        size_t start = (size_t)std::floor(onsets[k] * SR); if (start >= mono.size()) break;
        size_t end = k + 1 < onsets.size() ? std::min(mono.size(), (size_t)std::floor(onsets[k + 1] * SR)) : mono.size();
        if (end <= start) continue;
        std::vector<double> piece(mono.begin() + (long)start, mono.begin() + (long)end);
        std::vector<double> f = db_features_of(piece, block, hop, ncoeff, type);
        std::vector<float> feats(f.begin(), f.end()); orchidea::standardise(feats);
        varr fv(feats.size()); for (size_t j = 0; j < feats.size(); j++) fv[j] = feats[j];
        vlist notes;
        if (pfilter > 0) {
            std::map<std::string, int> ns = orchidea::partials_to_notes(piece, pwin, pwin / 4, pfilter, SR,
                [](const std::vector<double>& y, int w, int h) { return average_spectrum(y, w, h); }, [](int m) { return midi_to_pitch(m); });
            for (auto& e : extra) ns[e] = 0;
            for (auto& kv : ns) notes.push_back(v_list({ v_str(kv.first), v_num(kv.second) }));
        }
        out.push_back(v_list({ v_list({ v_sym("at"), v_num((double)start / SR) }), v_list({ v_sym("dur"), v_num((double)(end - start) / SR) }),
                               v_list({ v_sym("features"), v_arr(std::move(fv)) }), v_list({ v_sym("notes"), v_list(std::move(notes)) }) }));
        i.yield_check();
    }
    return v_list(std::move(out));
}
// (orchidea-search entries orchestra segments opts)   Orchidea's genetic search: entries a database's entries (records
//   with instr tech pitch dyn other features), orchestra a list of players as strings ("Fl", "Fl|Picc"), segments as
//   orchidea-analyse gives them, opts a record: 'population 'epochs 'pursuit 'crossover 'mutation 'sparsity 'positive
//   'negative 'hysteresis 'regularization 'dovetail 'movement 'solutions (the best kept per segment) 'connection
//   ("closest", "best" or "path") 'octaves (the pitch filter widened by octaves) 'styles 'dynamics 'others (lists of
//   names that filter the search space) 'seed 'quiet
//   => a record: 'segments a list, one per segment, each a record ('solutions (list (list cost (list index...)) ...),
//   the best first, an index -1 for a silent player; 'curve the fitness per epoch; 'players the index in the
//   orchestra of each of the solution's slots: the players that had sounds in the search space), and 'choices the
//   connection's solution per segment
inline vptr mus_orchidea_search(vlist& a, Interp& i) {
    vptr entries = a[0]; if (entries->t != Value::LIST) i.bad("orchidea-search: entries must be a list");
    std::vector<orchidea::entry> db; db.reserve(entries->l.size());
    auto field = [&](const vptr& e, const char* key) { vptr v = rec_get(e, key); return v ? str_of(v) : std::string("N"); };
    for (size_t k = 0; k < entries->l.size(); k++) {
        const vptr& e = entries->l[k]; orchidea::entry x; x.index = (int)k; x.instr = field(e, "instr"); x.tech = field(e, "tech"); x.pitch = field(e, "pitch"); x.dyn = field(e, "dyn"); x.other = field(e, "other");
        if (vptr mv = rec_get(e, "midi")) if (mv->t == Value::NUM && mv->num.size() == 1) x.midi = (int)mv->num[0];
        vptr f = rec_get(e, "features"); if (!f || f->t != Value::NUM) i.bad("orchidea-search: an entry without features");
        x.features.assign(std::begin(f->num), std::end(f->num)); db.push_back(std::move(x));
    }
    std::vector<std::string> orchestra; if (a[1]->t != Value::LIST) i.bad("orchidea-search: the orchestra must be a list"); for (auto& p : a[1]->l) orchestra.push_back(str_of(p));
    if (orchestra.empty()) i.bad("orchidea-search: an empty orchestra");
    vptr segs = a[2]; if (segs->t != Value::LIST || segs->l.empty()) i.bad("orchidea-search: no segments");
    std::vector<orchidea::segment> segments;
    for (auto& s : segs->l) { orchidea::segment g; vptr at = rec_get(s, "at"), dur = rec_get(s, "dur"), f = rec_get(s, "features"), notes = rec_get(s, "notes");
        if (!at || !dur || !f) i.bad("orchidea-search: a segment is a record (at dur features notes)");
        g.start = at->num[0]; g.length = dur->num[0]; g.features.assign(std::begin(f->num), std::end(f->num));
        if (notes && notes->t == Value::LIST) for (auto& kv : notes->l) if (kv->t == Value::LIST && kv->l.size() == 2) g.notes[str_of(kv->l[0])] = (int)std::lround(kv->l[1]->num[0]);
        segments.push_back(std::move(g)); }
    vptr opts = a[3];
    auto opt_num = [&](const char* key, double dflt) { vptr v = rec_get(opts, key); return v && v->t == Value::NUM && v->num.size() == 1 ? v->num[0] : dflt; };
    auto opt_list = [&](const char* key) { std::vector<std::string> out; vptr v = rec_get(opts, key); if (v && v->t == Value::LIST) for (auto& e : v->l) out.push_back(str_of(e)); return out; };
    orchidea::params p;
    p.pop_size = std::max(2, (int)opt_num("population", 300)); p.pop_size = (p.pop_size / 2) * 2;
    p.max_epochs = std::max(1, (int)opt_num("epochs", 300)); p.pursuit = std::max(0, (int)opt_num("pursuit", 0));
    p.xover_rate = opt_num("crossover", 0.8); p.mutation_rate = opt_num("mutation", 0.01); p.sparsity = opt_num("sparsity", 0.001);
    p.positive = opt_num("positive", 0.5); p.negative = opt_num("negative", 10); p.hysteresis = opt_num("hysteresis", 0); p.regularization = opt_num("regularization", 0);
    p.max_solutions = (int)opt_num("solutions", 10); p.dovetail = opt_num("dovetail", 0); p.movement = opt_num("movement", 1);
    if (p.dovetail < 0) i.bad("orchidea-search: dovetail must be >= 0");
    // the pitch filter widened by octaves: 'octaves (list 0 -1) admits the partials' pitches and the octave below them
    std::vector<int> octaves; if (vptr ov = rec_get(opts, "octaves")) { if (ov->t == Value::LIST) for (auto& e : ov->l) octaves.push_back((int)std::lround(i.scalar(e))); else if (ov->t == Value::NUM) for (double d : ov->num) octaves.push_back((int)std::lround(d)); }
    if (octaves.empty()) octaves.push_back(0);
    bool widen = octaves.size() != 1 || octaves[0] != 0;
    if (widen) for (auto& g : segments) if (!g.notes.empty()) {
        std::map<std::string, int> wider;
        for (auto& kv : g.notes) { int m = pitch_to_midi(kv.first); for (int o : octaves) { if (m < 0 && o != 0) continue; wider[o == 0 ? kv.first : midi_to_pitch(m + 12 * o)] = kv.second; } }
        g.notes = wider;
    }
    if (p.mutation_rate <= 0 || p.mutation_rate > 1) i.bad("orchidea-search: mutation must be in (0, 1]");
    if (p.xover_rate <= 0 || p.xover_rate > 1) i.bad("orchidea-search: crossover must be in (0, 1]");
    if (p.sparsity < 0 || p.sparsity > 1) i.bad("orchidea-search: sparsity must be in 0..1");
    if (p.hysteresis < 0 || p.positive < 0 || p.negative < 0) i.bad("orchidea-search: the penalisations and the hysteresis must be >= 0");
    vptr sv = rec_get(opts, "seed"); p.seed = sv && sv->t == Value::NUM ? (unsigned)sv->num[0] : (unsigned)i.rng();
    bool quiet = opt_num("quiet", 0) != 0; std::string conn = "closest"; if (vptr cv = rec_get(opts, "connection")) conn = str_of(cv);
    std::vector<std::string> styles = opt_list("styles"), dynamics = opt_list("dynamics"), others = opt_list("others");
    if (conn != "closest" && conn != "best" && conn != "path") i.bad("orchidea-search: connection is closest, best or path");
    orchidea::genetic ga(p);
    std::vector<orchidea::model> models(segments.size());
    std::vector<int> choices; std::vector<float> prev_forecast;
    // segment by segment: the search, then the choice for this segment (closest to the previous choice, or the best;
    // the path is found at the end), which the next segment's search remembers (hysteresis, dovetailing)
    for (size_t k = 0; k < segments.size(); k++) {
        try { orchidea::make_model(db, orchestra, segments[k], styles, dynamics, others, models[k]); }
        catch (std::exception& e) { i.bad(std::string("segment ") + std::to_string(k + 1) + ": " + e.what()); }
        int last_shown = -1; size_t seg = k;
        ga.p.progress = [&, seg](int epoch, int epochs) -> bool {
            i.yield_check();                                                     // Esc in the IDE, the windows kept alive
            if (!quiet && epochs >= 10 && epoch % std::max(1, epochs / 10) == 0 && epoch != last_shown) { last_shown = epoch;
                *i.out << "orchestrate-mimetic: segment " << (seg + 1) << "/" << segments.size() << ", epoch " << epoch << "/" << epochs << "\n" << std::flush; }
            return true; };
        ga.search(models[k]);
        if (models[k].solutions.empty()) i.bad(std::string("segment ") + std::to_string(k + 1) + ": the search found no solution");
        int choice = conn == "closest" ? orchidea::closest_choice(models[k], prev_forecast, p) : 0;
        choices.push_back(choice);
        const orchidea::solution& chosen = models[k].solutions[(size_t)choice];
        ga.remember(models[k], chosen);
        prev_forecast.assign(models[k].seg->features.size(), 0.0f); orchidea::additive_forecast(chosen, models[k].database, prev_forecast); orchidea::standardise(prev_forecast);
    }
    if (conn == "path") choices = orchidea::shortest_path(models, p.movement);
    vlist out_segments;
    for (auto& m : models) {
        vlist sols;
        for (auto& s : m.solutions) { vlist idx; for (int k : s.indices) idx.push_back(v_num(k == -1 ? -1 : m.database[(size_t)k]->index)); sols.push_back(v_list({ v_num(m.cost_of(s)), v_list(std::move(idx)) })); }
        varr curve(m.curve.size()); for (size_t j = 0; j < m.curve.size(); j++) curve[j] = m.curve[j];
        vlist slots; for (int s : m.slots) slots.push_back(v_num(s));
        out_segments.push_back(v_list({ v_list({ v_sym("solutions"), v_list(std::move(sols)) }), v_list({ v_sym("curve"), v_arr(std::move(curve)) }), v_list({ v_sym("players"), v_list(std::move(slots)) }) }));
    }
    vlist ch; for (int c : choices) ch.push_back(v_num(c));
    return v_list({ v_list({ v_sym("segments"), v_list(std::move(out_segments)) }), v_list({ v_sym("choices"), v_list(std::move(ch)) }) });
}

inline void add_music(Interp& i) {
    i.def("pitch->midi", mus_pitch_to_midi, 1, 1); i.def("midi->pitch", mus_midi_to_pitch, 1, 1);
    i.def("db-read", mus_db_read, 1, 1); i.def("db-gen", mus_db_gen, 6, 6); add_midi(i); i.def("db-locate", mus_db_locate, 2, 2); i.def("db-index-size", mus_db_index_size, 1, 1);
    i.def("orchidea-analyse", mus_orchidea_analyse, 7, 7); i.def("orchidea-search", mus_orchidea_search, 4, 4);
}

} // namespace musil
