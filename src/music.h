// music.h — music, C++ half: sample databases in the SOL/Orchidea layout, pitch names.
// The Musil half (music.mu) holds the score: events in time, rendering, playback, the roll.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// A database is a folder of sound files and one feature file (a `.db`) at its root:
//   first line:   <type> <block size> <hop> <number of coefficients>
//   other lines:  <path relative to the root>;<f1>;<f2>;...
// Sound file names carry the metadata, separated by dashes: instrument-technique-pitch-dynamics-other
// (Vn-ord-D#5-mf-3c-N.wav); a sharp may be written # or _ on disk. Only the metadata and the
// features are read here; the sounds are opened on demand (music.mu caches what it plays).

#pragma once
#include "core.h"
#include "system.h"
#include "signals.h"
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
        while (syms.size() < 4) syms.push_back("N");
        std::string other; for (size_t j = 4; j < syms.size(); j++) other += (j > 4 ? "-" : "") + syms[j];
        if (other.empty()) other = "N";
        int midi = pitch_to_midi(syms[2]);
        entries.push_back(v_list({ v_list({ v_sym("file"), v_str(file) }), v_list({ v_sym("instr"), v_str(syms[0]) }), v_list({ v_sym("tech"), v_str(syms[1]) }),
                                   v_list({ v_sym("pitch"), v_str(syms[2]) }), v_list({ v_sym("dyn"), v_str(syms[3]) }), v_list({ v_sym("other"), v_str(other) }),
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

inline void add_music(Interp& i) {
    i.def("pitch->midi", mus_pitch_to_midi, 1, 1); i.def("midi->pitch", mus_midi_to_pitch, 1, 1);
    i.def("db-read", mus_db_read, 1, 1); i.def("db-gen", mus_db_gen, 6, 6); i.def("db-locate", mus_db_locate, 2, 2); i.def("db-index-size", mus_db_index_size, 1, 1);
}

} // namespace musil
