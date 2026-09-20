// orchidea.h — mimetic (assisted) orchestration: Orchidea's search core inside Musil.
//
// After Orchidea, (c) 2018-2020 Carmine E. Cella, HEM, Ircam (GeneticOrchestra.h, forecasts.h, connections.h,
// Session.h, SessionI.h, Solution.h, OrchestrationModel.h, segmentations.h, analysis.h): the genetic search over
// the database, the additive forecast, the asymmetric fitness with its sparsity, hysteresis and regularisation,
// the closest/best connection between segments and the continuity model are ported as they are. What Orchidea
// did with files (reading the databases, the WAVs, exporting connection.txt and the mixes) Musil already does:
// db-load holds the entries, the score plays and renders, score-save writes the connection. The target's
// analysis uses Musil's feature code (the same that db-gen uses: average_spectrum, db_features_of) and Musil's
// pitch names, C4 = 261.6 Hz, as the databases' file names have them.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.

#pragma once
#include <vector>
#include <map>
#include <string>
#include <random>
#include <deque>
#include <algorithm>
#include <cmath>
#include <functional>
#include <thread>

namespace musil { namespace orchidea {

// --- the parameters (Orchidea's, with their defaults; the Musil names are in music.mu) ---
struct params {
    int pop_size = 300, max_epochs = 300, pursuit = 0;
    double xover_rate = 0.8, mutation_rate = 0.01, sparsity = 0.001;
    double positive = 0.5, negative = 10.0, hysteresis = 0.0, regularization = 0.0;
    int max_solutions = 10;                               // how many of the best unique solutions are kept per segment
    unsigned seed = 1;
    std::function<bool(int, int)> progress;               // (epoch, epochs) => false to stop the search
};
// a database entry: what the search needs of a sound (the index is its place in the database's entries)
struct entry { int index = 0; std::string instr, tech, pitch, dyn, other; std::vector<float> features; };
// a segment of the target: its features (standardised, as Orchidea's normalize2), the pitches of its partials with
// their cents, its place in seconds
struct segment { double start = 0, length = 0; std::vector<float> features; std::map<std::string, int> notes; };
// a solution: one database index per player (-1: the player silent), a duration per player, the fitness
struct solution {
    std::vector<int> indices; std::vector<double> durations; float fitness = 0;
    bool operator<(const solution& r) const { return fitness < r.fitness; }
    bool is_empty() const { for (int k : indices) if (k != -1) return false; return true; }
};
// the search space of one segment: the entries that pass the filters, by instrument, and the orchestra restricted
// to the instruments present
struct model {
    const segment* seg = nullptr;
    std::vector<const entry*> database;
    std::map<std::string, std::vector<int>> instruments;
    std::vector<std::string> orchestra;
    std::vector<int> slots;                               // for each player kept, its index in the orchestra given
    std::vector<solution> solutions;
    std::vector<float> curve;                             // the population's total fitness per epoch
    float cost_of(const solution& s) const { return s.fitness > 0 ? (float)(1.0 / std::sqrt((double)s.fitness)) : 1e9f; }
};

// --- the numerics of the original (algorithms.h) ---
inline float norm_of(const std::vector<float>& v) { double s = 0; for (float x : v) s += (double)x * x; return (float)std::sqrt(s); }
inline float inner_prod(const std::vector<float>& a, const std::vector<float>& b) { double s = 0; size_t n = std::min(a.size(), b.size()); for (size_t k = 0; k < n; k++) s += (double)a[k] * b[k]; return std::isfinite(s) ? (float)s : 0.0f; }
// Orchidea's normalize2: the values standardised (mean 0, deviation 1)
inline void standardise(std::vector<float>& v) {
    if (v.empty()) return; double m = 0; for (float x : v) m += x; m /= v.size();
    double s = 0; for (float x : v) s += (x - m) * (x - m); s = std::sqrt(s / v.size());
    const float fm = (float)m, inv = s != 0 ? (float)(1.0 / s) : 0.0f;
    for (auto& x : v) x = (x - fm) * inv;
}
inline void tokenize(const std::string& s, std::vector<std::string>& out, char sep) { std::string cur; for (char c : s) { if (c == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); } else cur += c; } if (!cur.empty()) out.push_back(cur); }

// --- the forecast: the features of a solution, the sum of its sounds' (AdditiveForecast) ---
inline void additive_forecast(const solution& id, const std::vector<const entry*>& db, std::vector<float>& forecast) {
    std::fill(forecast.begin(), forecast.end(), 0.0f);
    const float inv = 1.0f / (float)std::max<size_t>(1, id.indices.size()); float* out = forecast.data(); size_t n = forecast.size();
    for (int k : id.indices) { if (k == -1) continue; const entry* e = db[(size_t)k]; const float* f = e->features.data(); size_t m = std::min(n, e->features.size());
        for (size_t j = 0; j < m; j++) out[j] += f[j] * inv; }
}
// the asymmetric distance: a partial the target has not is penalised more than one it has and the solution lacks
inline float asym_distance(const std::vector<float>& values, const std::vector<float>& target, double pos, double neg) {
    float sp = 0, sn = 0; const float* a = values.data(); const float* b = target.data(); size_t n = std::min(values.size(), target.size());
    for (size_t k = 0; k < n; k++) { float d = a[k] - b[k]; if (d >= 0) sp += d; else sn -= d; }
    return (float)(pos * sp + neg * sn);
}

// --- the search space of a segment (SessionI::make_model) ---
// filters: the target's pitches (when partials were found), and the styles, dynamics and others asked for
inline void make_model(const std::vector<entry>& db, const std::vector<std::string>& orchestra, const segment& seg,
                       const std::vector<std::string>& styles, const std::vector<std::string>& dynamics, const std::vector<std::string>& others, model& m) {
    m.database.clear(); m.instruments.clear(); m.orchestra.clear(); m.slots.clear(); m.seg = &seg;
    for (const entry& e : db) {
        bool note_ok = seg.notes.empty(), style_ok = styles.empty(), dyn_ok = dynamics.empty(), other_ok = others.empty();
        if (!note_ok) note_ok = seg.notes.count(e.pitch) > 0;
        for (auto& s : styles) if (e.tech == s) { style_ok = true; break; }
        for (auto& s : dynamics) if (e.dyn == s) { dyn_ok = true; break; }
        for (auto& s : others) if (e.other.find(s) != std::string::npos) { other_ok = true; break; }
        if (note_ok && style_ok && dyn_ok && other_ok) m.database.push_back(&e);
    }
    if (m.database.empty()) throw std::runtime_error("empty search space: no sound passes the filters (the pitches found in the target, the styles, the dynamics)");
    for (size_t k = 0; k < m.database.size(); k++) m.instruments[m.database[k]->instr].push_back((int)k);
    for (size_t k = 0; k < orchestra.size(); k++) {
        const std::string& player = orchestra[k];
        if (player.find('|') != std::string::npos) {          // a doubling: every instrument of it must be in the space
            std::vector<std::string> alts; tokenize(player, alts, '|'); bool missing = false;
            for (auto& a : alts) if (!m.instruments.count(a)) { missing = true; break; }
            if (!missing) { m.orchestra.push_back(player); m.slots.push_back((int)k); }
        } else if (m.instruments.count(player)) { m.orchestra.push_back(player); m.slots.push_back((int)k); }
    }
    if (m.orchestra.empty()) throw std::runtime_error("empty orchestra: none of its instruments has a sound in the search space");
}

// --- the genetic search (GeneticOrchestra) ---
struct genetic {
    params p; std::mt19937 rng; std::deque<std::vector<float>> history;   // the best forecasts of the segments before (hysteresis)
    static const int MAX_EQUAL_EPOCHS = 150;
    genetic(const params& pr) : p(pr), rng(pr.seed) {}
    double frand() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); }
    int irand(int n) { return n <= 1 ? 0 : (int)(rng() % (unsigned)n); }
    std::string instrument_of(const model& m, size_t slot) {   // the instrument a slot plays now (a doubling: one of them, drawn)
        const std::string& s = m.orchestra[slot];
        if (s.find('|') == std::string::npos) return s;
        std::vector<std::string> alts; tokenize(s, alts, '|'); return alts[(size_t)irand((int)alts.size())];
    }
    void random_chromosome(model& m, std::vector<int>& f) {
        f.resize(m.orchestra.size());
        for (size_t k = 0; k < f.size(); k++) { auto& pool = m.instruments[instrument_of(m, k)]; f[k] = pool[(size_t)irand((int)pool.size())]; }
    }
    // the stochastic pursuit: each slot one of the kth sounds of its instrument nearest the target (by projection, less the norm)
    void pursuit_chromosome(model& m, std::vector<int>& f, const std::vector<float>& target, int kth) {
        f.resize(m.orchestra.size());
        for (size_t k = 0; k < f.size(); k++) {
            auto& pool = m.instruments[instrument_of(m, k)];
            std::vector<std::pair<float, int>> scored;
            for (int idx : pool) { const entry* e = m.database[(size_t)idx]; float d = inner_prod(target, e->features); scored.push_back({ std::fabs(d) - norm_of(e->features), idx }); }
            int n = std::min<int>(kth, (int)scored.size());
            std::partial_sort(scored.begin(), scored.begin() + n, scored.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
            f[k] = scored[(size_t)irand(n)].second;
        }
    }
    void mutate(model& m, solution& id) {
        for (size_t k = 0; k < id.indices.size(); k++) { if (id.indices[k] == -1) continue;
            if (frand() < p.mutation_rate) { auto& pool = m.instruments[instrument_of(m, k)]; id.indices[k] = pool[(size_t)irand((int)pool.size())]; } }
    }
    void apply_sparsity(solution& id) { for (auto& k : id.indices) if (frand() < p.sparsity) k = -1; }
    float evaluate_individual(const solution& id, const std::vector<float>& target, const std::vector<const entry*>& db, std::vector<float>& values) {
        additive_forecast(id, db, values); standardise(values);
        float s = asym_distance(values, target, p.positive, p.negative);
        long sum = 0; for (int k : id.indices) if (k != -1) sum++;
        float reg = (float)(p.regularization * sum);
        float memory = 0; double forget = 1;
        for (size_t k = 0; k < history.size() && forget > 0.0001; k++) { float h = asym_distance(values, history[k], p.positive, p.negative); forget *= p.hysteresis; memory += (float)(h * forget); }
        return s + reg + memory;
    }
    // the population evaluated, the individuals shared among the cores (the evaluation reads only; the draws stay sequential)
    float evaluate_population(std::vector<solution>& pop, const std::vector<float>& target, const std::vector<const entry*>& db, std::vector<float>& values) {
        unsigned cores = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
        if (pop.size() < 64 || cores < 2) {
            for (auto& id : pop) { float v = evaluate_individual(id, target, db, values); id.fitness = v == 0 ? 1e9f : (float)std::pow(1.0 / v, 2.0); }
        } else {
            std::vector<std::thread> pool; size_t share = (pop.size() + cores - 1) / cores;
            for (unsigned c = 0; c < cores; c++) {
                size_t from = c * share, to = std::min(pop.size(), from + share); if (from >= to) break;
                pool.emplace_back([this, &pop, &target, &db, from, to]() { std::vector<float> vals(target.size(), 0.0f);
                    for (size_t k = from; k < to; k++) { float v = evaluate_individual(pop[k], target, db, vals); pop[k].fitness = v == 0 ? 1e9f : (float)std::pow(1.0 / v, 2.0); } });
            }
            for (auto& t : pool) t.join();
        }
        double total = 0; for (auto& id : pop) total += id.fitness;
        return (float)total;
    }
    const solution& select_parent(const std::vector<solution>& pop, float total) {
        double wheel = frand() * total, psum = 0; size_t sel = 0;
        for (size_t k = 0; k < pop.size(); k++) { psum += pop[k].fitness; if (psum >= wheel) { sel = k; break; } }
        return pop[sel];
    }
    void offspring(model& m, const std::vector<solution>& old, std::vector<solution>& fresh, float total) {
        while (fresh.size() < (size_t)p.pop_size) {
            const solution& a = select_parent(old, total); const solution& b = select_parent(old, total);
            solution o1, o2; o1.indices = a.indices; o2.indices = b.indices;
            if (frand() < p.xover_rate) { size_t cp = (size_t)(frand() * (double)o1.indices.size()); for (size_t k = cp; k < o1.indices.size(); k++) { o1.indices[k] = b.indices[k]; o2.indices[k] = a.indices[k]; } }
            mutate(m, o1); mutate(m, o2); apply_sparsity(o1); apply_sparsity(o2);
            fresh.push_back(o1); fresh.push_back(o2);
        }
        fresh.erase(std::remove_if(fresh.begin(), fresh.end(), [](const solution& s) { return s.is_empty(); }), fresh.end());
    }
    // the search of one segment: => the best total fitness reached; the model's solutions are the unique individuals
    // of the best population, ranked, the best first
    float search(model& m) {
        const std::vector<float>& target = m.seg->features;
        std::vector<solution> population((size_t)p.pop_size);
        for (auto& id : population) { if (p.pursuit == 0) random_chromosome(m, id.indices); else { pursuit_chromosome(m, id.indices, target, p.pursuit); mutate(m, id); } }
        std::vector<float> values(target.size(), 0.0f);
        m.curve.clear(); m.solutions.clear();
        float max_fit = 0, old_fit = 0; int fit_count = 0; std::vector<solution> best_pop;
        for (int epoch = 0; epoch < p.max_epochs; epoch++) {
            if (p.progress && !p.progress(epoch, p.max_epochs)) break;
            float total = evaluate_population(population, target, m.database, values);
            std::vector<solution> fresh; offspring(m, population, fresh, total);
            for (size_t k = 0; k < population.size() && k < fresh.size(); k++) { population[k].indices = fresh[k].indices; population[k].fitness = fresh[k].fitness; }
            if (max_fit < total) { max_fit = total; best_pop = fresh; }
            m.curve.push_back(total);
            if (old_fit == max_fit) fit_count++; else fit_count = 0;
            if (fit_count > MAX_EQUAL_EPOCHS) break;
            old_fit = max_fit;
        }
        if (best_pop.empty()) best_pop = population;
        std::map<std::vector<int>, solution> uniques; for (auto& s : best_pop) uniques[s.indices] = s;
        for (auto& kv : uniques) m.solutions.push_back(kv.second);
        evaluate_population(m.solutions, target, m.database, values);
        std::sort(m.solutions.begin(), m.solutions.end()); std::reverse(m.solutions.begin(), m.solutions.end());
        if (p.max_solutions > 0 && m.solutions.size() > (size_t)p.max_solutions) m.solutions.resize((size_t)p.max_solutions);
        for (auto& s : m.solutions) s.durations.assign(s.indices.size(), m.seg->length);
        if (!m.solutions.empty()) { std::vector<float> forecast(target.size(), 0.0f); additive_forecast(m.solutions[0], m.database, forecast); history.push_front(forecast); }
        return max_fit;
    }
};

// --- the connection between the segments (connections.h): the solution of each segment chosen ---
// 'closest': from the best solution of the first segment, each next segment's solution is the one whose forecast is
// nearest the previous segment's target (the asymmetric distance); 'best': the best of each
inline std::vector<int> connect(std::vector<model>& models, const params& p, bool closest) {
    std::vector<int> choices;
    if (models.empty()) return choices;
    choices.push_back(0);
    const segment* current = models[0].seg;
    for (size_t k = 1; k < models.size(); k++) {
        model& m = models[k]; int argmin = 0;
        if (closest && !m.solutions.empty()) {
            float best = 1e30f; std::vector<float> values(m.seg->features.size(), 0.0f);
            for (size_t j = 0; j < m.solutions.size(); j++) { additive_forecast(m.solutions[j], m.database, values); standardise(values);
                float s = asym_distance(values, current->features, p.positive, p.negative); if (s < best) { best = s; argmin = (int)j; } }
        }
        choices.push_back(argmin); current = m.seg;
    }
    return choices;
}

// --- the target's analysis ---
// the onsets by the spectral flux (segmentations.h: the rectified difference of consecutive magnitude spectra, its
// local maxima above a threshold of its maximum, at least a timegate apart); => seconds
inline std::vector<double> flux_onsets(const std::vector<double>& x, int bsize, int hop, double sr, double threshold, double timegate, const std::function<void(double*, size_t, int)>& fft) {
    std::vector<double> win((size_t)bsize), old((size_t)bsize, 0.0), spec((size_t)bsize), fr(2 * (size_t)bsize), flux;
    for (int k = 0; k < bsize; k++) win[(size_t)k] = 0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * k / (bsize - 1));
    for (size_t i = 0; i < x.size(); i += (size_t)hop) {
        std::fill(fr.begin(), fr.end(), 0.0); size_t rsize = std::min<size_t>((size_t)bsize, x.size() - i);
        for (size_t j = 0; j < rsize; j++) fr[2 * j] = x[i + j] * win[j];
        fft(fr.data(), (size_t)bsize, -1);
        for (size_t j = 0; j < (size_t)bsize; j++) spec[j] = std::hypot(fr[2 * j], fr[2 * j + 1]);
        double sf = 0; for (size_t j = 0; j < (size_t)bsize; j++) { double a = spec[j] - old[j]; old[j] = spec[j]; sf += a < 0 ? 0 : a; }
        flux.push_back(sf);
    }
    std::vector<double> onsets; if (flux.empty()) return onsets;
    double ma = *std::max_element(flux.begin(), flux.end()); if (ma > 0) for (auto& v : flux) v /= ma;
    std::vector<size_t> peaks; if (flux.size() >= 2 && flux[0] > flux[1]) peaks.push_back(0);
    for (size_t k = 1; k + 1 < flux.size(); k++) if (flux[k] > flux[k - 1] && flux[k] > flux[k + 1]) peaks.push_back(k);
    double prev = 0;
    for (size_t k = 0; k < peaks.size(); k++) if (flux[peaks[k]] > threshold) { double pos = (double)peaks[k] * hop / sr; if (std::fabs(pos - prev) > timegate || k == 0) { onsets.push_back(pos); prev = pos; } }
    return onsets;
}
// the partials of a segment as pitches with their cents (analysis.h's partials_to_notes): the averaged spectrum over
// a long window, scaled to 0..1, its local maxima above the threshold read as frequencies (quadratic interpolation)
// and named as the nearest tempered pitch, C4 = 261.6 Hz, with the deviation in cents; a later partial at the same
// pitch replaces the earlier
inline std::map<std::string, int> partials_to_notes(const std::vector<double>& x, int window, int hop, double threshold, double sr,
                                                    const std::function<std::vector<double>(const std::vector<double>&, int, int)>& avg_spectrum,
                                                    const std::function<std::string(int)>& name_of) {
    std::map<std::string, int> notes;
    std::vector<double> spec = avg_spectrum(x, window, hop); size_t N = (size_t)window / 2; spec.resize(N);
    double lo = spec[0], hi = spec[0]; for (double v : spec) { lo = std::min(lo, v); hi = std::max(hi, v); }
    for (auto& v : spec) v = hi != lo ? (v - lo) / (hi - lo) : 0;
    double per_bin = sr / (double)window;
    for (size_t k = 1; k + 1 < N; k++) {
        if (!(spec[k] > spec[k - 1] && spec[k] > spec[k + 1]) || spec[k] <= threshold) continue;
        double y1 = spec[k - 1], y2 = spec[k], y3 = spec[k + 1], den = 2 * (2 * y2 - y1 - y3), d = den != 0 ? (y3 - y1) / den : 0;
        double f = per_bin * ((double)k + d); if (f <= 27.5) continue;
        double midi = 69 + 12 * std::log2(f / 440.0); int m = (int)std::lround(midi); int cents = (int)std::lround(1200 * std::log2(f / (440.0 * std::pow(2.0, (m - 69) / 12.0))));
        notes[name_of(m)] = cents;
    }
    return notes;
}

} } // namespace musil::orchidea
