// live.h — real-time sound, C++ half: the audio device, the command queue, the clock and
// the sampler voices. The Musil half is live.mu.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Design
//   The interpreter never runs on the audio thread. It posts commands (play this buffer at
//   this time, stop that voice, set the gain) into a lock-free queue; the audio callback
//   drains the queue, advances a sample clock, mixes the active voices into an N-channel
//   bus and hands the bus to the device. Voices are samplers: a shared buffer, a fractional
//   read position and rate, gain and pan, a start time on the clock (sample-accurate) and a
//   loop flag. Commands carry a time in samples, so a program running with jitter can still
//   place sounds exactly, by asking for them slightly ahead.
//
//   The device is miniaudio (src/live/miniaudio.h, public domain; the same file raylib
//   carries, but no window and no raylib are needed). The null backend, chosen with the
//   option "device" "null" or automatically when there is no hardware, runs the callback
//   from a timer, so the tests and a headless machine exercise everything but the ears.
//
//   Synths. An instrument is an ordinary Musil function of its parameters whose body
//   combines ugens (osc, adsr, lowpass, delay, ...) with arithmetic. Called normally it
//   returns a buffer (every ugen is a vector function in signals). Handed to (synth f) its
//   body is compiled into a graph of nodes that run the same loops from signals.h one block
//   at a time, with the function's parameters as hot parameters: (set-param id 'freq 440)
//   changes a running sound, ramped. Only the streamable builtins (the table below) may
//   appear in such a function; anything else is refused with its name.

#pragma once
#include "core.h"
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DECODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
#include "live/miniaudio.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <map>
#include <random>
#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace musil {

// --- a single-producer single-consumer queue of commands (interpreter -> audio thread) ---
struct audio_command {
    enum kind_t { PLAY, STOP, STOP_ALL, GAIN, SET_VOICE, ADD_SYNTH, FREE_SYNTH, SET_PARAM } kind = PLAY;
    std::shared_ptr<struct synth_instance> synth;    // ADD_SYNTH
    int param_index = 0; int ramp_samples = 0;        // SET_PARAM
    long id = 0;                              // voice id
    long long at = 0;                         // start time in samples on the clock (0 = now)
    std::shared_ptr<const std::vector<float>> buffer;   // interleaved, `channels` wide
    int channels = 1; double buffer_sr = 44100;
    double rate = 1, amp = 1, pan = 0; bool loop = false; int param = 0; double value = 0;
};
struct command_queue {                        // fixed ring, no allocation on the audio side
    static const int CAP = 4096;
    audio_command slots[CAP];
    std::atomic<int> head{0}, tail{0};
    bool push(audio_command c) {              // called by the interpreter thread only
        int t = tail.load(std::memory_order_relaxed), n = (t + 1) % CAP;
        if (n == head.load(std::memory_order_acquire)) return false;
        slots[t] = std::move(c); tail.store(n, std::memory_order_release); return true;
    }
    bool pop(audio_command& c) {              // called by the audio thread only
        int h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        c = std::move(slots[h]); head.store((h + 1) % CAP, std::memory_order_release); return true;
    }
};

// --- a sampler voice ---
struct voice {
    long id = 0; bool active = false;
    std::shared_ptr<const std::vector<float>> buffer; int channels = 1; double buffer_sr = 44100;
    long long start = 0;                      // clock sample at which it begins
    double pos = 0, rate = 1, amp = 1, pan = 0; bool loop = false;
    std::atomic<double> target_amp{1}, target_pan{0}, target_rate{1};   // ramped towards, per block
};

// --- the streaming graph -----------------------------------------------------------
// A node has input nodes and one output per channel. Control arguments (a filter's cutoff, an
// envelope's times) are read from their input's first sample each block, so a parameter, a
// constant or a slowly varying signal all work there.
struct gnode {
    std::vector<int> in;                                  // indices of the input nodes
    std::vector<std::vector<float>> out;                  // one buffer per channel
    int channels = 1;
    virtual ~gnode() {}
    virtual void process(std::vector<std::unique_ptr<gnode>>& g, int n) = 0;
    void size(int n) { out.resize(channels); for (auto& c : out) if ((int)c.size() != n) c.assign(n, 0.0f); }
    const float* sig(std::vector<std::unique_ptr<gnode>>& g, size_t k, int ch = 0) { gnode& s = *g[in[k]]; return s.out[std::min(ch, s.channels - 1)].data(); }
    float ctrl(std::vector<std::unique_ptr<gnode>>& g, size_t k) { return g[in[k]]->out[0][0]; }
};
struct const_node : gnode {
    double value; const_node(double v) : value(v) {}
    void process(std::vector<std::unique_ptr<gnode>>&, int n) override { size(n); std::fill(out[0].begin(), out[0].end(), (float)value); }
};
struct param_node : gnode {                               // a hot parameter with a linear ramp
    std::string name; std::atomic<double> target{0}; std::atomic<int> ramp{0}; double value = 0;
    const double* drive = nullptr;                        // offline rendering: a value per sample instead
    param_node(std::string nm, double v) : name(std::move(nm)), value(v) { target = v; }
    void process(std::vector<std::unique_ptr<gnode>>&, int n) override {
        size(n);
        if (drive) { for (int k = 0; k < n; k++) out[0][k] = (float)drive[k]; value = drive[n - 1]; target = value; return; }
        double tg = target.load(); int r = ramp.load();
        for (int k = 0; k < n; k++) { if (r > 0) { value += (tg - value) / r; r--; } else value = tg; out[0][k] = (float)value; }
        ramp = r;
    }
};
struct arith_node : gnode {                               // + - * / over any number of inputs, mono broadcast
    char op; arith_node(char o) : op(o) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        channels = 1; for (int k : in) channels = std::max(channels, g[k]->channels); size(n);
        for (int ch = 0; ch < channels; ch++) {
            const float* a = sig(g, 0, ch);
            for (int k = 0; k < n; k++) out[ch][k] = a[k];
            if (in.size() == 1 && op == '-') for (int k = 0; k < n; k++) out[ch][k] = -a[k];
            if (in.size() == 1 && op == '/') for (int k = 0; k < n; k++) out[ch][k] = a[k] != 0 ? 1.0f / a[k] : 0;
            for (size_t j = 1; j < in.size(); j++) { const float* b = sig(g, j, ch);
                for (int k = 0; k < n; k++) { float& o = out[ch][k]; o = op == '+' ? o + b[k] : op == '-' ? o - b[k] : op == '*' ? o * b[k] : (b[k] != 0 ? o / b[k] : 0); } }
        }
    }
};
struct math_node : gnode {                                // elementwise functions of one or two inputs
    std::string fn; math_node(std::string f) : fn(std::move(f)) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        channels = 1; for (int k : in) channels = std::max(channels, g[k]->channels); size(n);
        for (int ch = 0; ch < channels; ch++) {
            const float* a = sig(g, 0, ch); const float* b = in.size() > 1 ? sig(g, 1, ch) : nullptr; const float* c = in.size() > 2 ? sig(g, 2, ch) : nullptr;
            for (int k = 0; k < n; k++) {
                double x = a[k], y = b ? b[k] : 0, z = c ? c[k] : 0, r = 0;
                if (fn == "abs") r = std::fabs(x); else if (fn == "tanh") r = std::tanh(x); else if (fn == "sin") r = std::sin(x); else if (fn == "cos") r = std::cos(x);
                else if (fn == "exp") r = std::exp(x); else if (fn == "sqrt") r = std::sqrt(std::max(0.0, x)); else if (fn == "min") r = std::min(x, y); else if (fn == "max") r = std::max(x, y);
                else if (fn == "pow") r = std::pow(x, y); else if (fn == "clip") r = std::max(y, std::min(z, x)); else if (fn == "floor") r = std::floor(x);
                out[ch][k] = (float)r;
            }
        }
    }
};
struct osc_node : gnode {                                 // (osc sr freq table): freq is a signal
    std::vector<double> table; osc_state st; double sr; std::vector<double> fbuf, obuf;
    osc_node(std::vector<double> t, double s) : table(std::move(t)), sr(s) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); fbuf.resize(n); obuf.resize(n); const float* f = sig(g, 0);
        for (int k = 0; k < n; k++) fbuf[k] = f[k];
        osc_run(fbuf.data(), obuf.data(), n, table.data(), table.size(), sr, st);
        for (int k = 0; k < n; k++) out[0][k] = (float)obuf[k];
    }
};
struct noise_node : gnode {
    std::mt19937_64 rng{12345}; std::uniform_real_distribution<double> d{-1.0, 1.0};
    void process(std::vector<std::unique_ptr<gnode>>&, int n) override { size(n); for (int k = 0; k < n; k++) out[0][k] = (float)d(rng); }
};
struct adsr_node : gnode {                                // (adsr sr gate a d s r): gate is a signal, times controls
    adsr_state st; double sr; std::vector<double> gbuf, obuf;
    adsr_node(double s) : sr(s) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); gbuf.resize(n); obuf.resize(n); const float* gate = sig(g, 0);
        for (int k = 0; k < n; k++) gbuf[k] = gate[k];
        adsr_run(gbuf.data(), obuf.data(), n, ctrl(g, 1), ctrl(g, 2), ctrl(g, 3), ctrl(g, 4), sr, st);
        for (int k = 0; k < n; k++) out[0][k] = (float)obuf[k];
    }
};
struct lag_node : gnode {                                 // (lag x sr seconds)
    lag_state st; double sr; std::vector<double> xb, ob;
    lag_node(double s) : sr(s) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); xb.resize(n); ob.resize(n); const float* x = sig(g, 0); double secs = ctrl(g, 1);
        for (int k = 0; k < n; k++) xb[k] = x[k];
        lag_run(xb.data(), ob.data(), n, secs <= 0 ? 0.0 : std::exp(-1.0 / (secs * sr)), st);
        for (int k = 0; k < n; k++) out[0][k] = (float)ob[k];
    }
};
struct iir_node : gnode {                                 // (iir x b a) with constant coefficients
    std::vector<double> b, a; iir_state st; std::vector<double> xb, yb;
    iir_node(std::vector<double> bb, std::vector<double> aa) : b(std::move(bb)), a(std::move(aa)) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); xb.resize(n); yb.resize(n); const float* x = sig(g, 0);
        for (int k = 0; k < n; k++) xb[k] = x[k];
        iir_run(xb.data(), yb.data(), n, b.data(), b.size(), a.data(), a.size(), st);
        for (int k = 0; k < n; k++) out[0][k] = (float)yb[k];
    }
};
struct biquad_node : gnode {                              // (lowpass x sr f0 q) and the other RBJ types; f0, q, gain are controls
    std::string type; double sr; double b[3] = { 1, 0, 0 }, a[3] = { 1, 0, 0 }; double lf = -1, lq = -1, lg = -1; iir_state st; std::vector<double> xb, yb;
    biquad_node(std::string t, double s) : type(std::move(t)), sr(s) {}
    void coefficients(double f0, double q, double gain_db) {                    // the RBJ cookbook, as biquad in signals.mu
        double w0 = 2 * 3.14159265358979323846 * f0 / sr, c = std::cos(w0), sn = std::sin(w0), alpha = sn / (2 * std::max(q, 1e-3));
        double A = std::pow(10, gain_db / 40), sA = std::sqrt(A);
        if (type == "lowpass")       { b[0] = (1 - c) / 2; b[1] = 1 - c; b[2] = (1 - c) / 2; a[0] = 1 + alpha; a[1] = -2 * c; a[2] = 1 - alpha; }
        else if (type == "highpass") { b[0] = (1 + c) / 2; b[1] = -(1 + c); b[2] = (1 + c) / 2; a[0] = 1 + alpha; a[1] = -2 * c; a[2] = 1 - alpha; }
        else if (type == "bandpass") { b[0] = alpha; b[1] = 0; b[2] = -alpha; a[0] = 1 + alpha; a[1] = -2 * c; a[2] = 1 - alpha; }
        else if (type == "notch")    { b[0] = 1; b[1] = -2 * c; b[2] = 1; a[0] = 1 + alpha; a[1] = -2 * c; a[2] = 1 - alpha; }
        else if (type == "peak-eq")  { b[0] = 1 + alpha * A; b[1] = -2 * c; b[2] = 1 - alpha * A; a[0] = 1 + alpha / A; a[1] = -2 * c; a[2] = 1 - alpha / A; }
        else if (type == "lowshelf") { b[0] = A * ((A + 1) - (A - 1) * c + 2 * sA * alpha); b[1] = 2 * A * ((A - 1) - (A + 1) * c); b[2] = A * ((A + 1) - (A - 1) * c - 2 * sA * alpha);
                                       a[0] = (A + 1) + (A - 1) * c + 2 * sA * alpha; a[1] = -2 * ((A - 1) + (A + 1) * c); a[2] = (A + 1) + (A - 1) * c - 2 * sA * alpha; }
        else if (type == "highshelf"){ b[0] = A * ((A + 1) + (A - 1) * c + 2 * sA * alpha); b[1] = -2 * A * ((A - 1) + (A + 1) * c); b[2] = A * ((A + 1) + (A - 1) * c - 2 * sA * alpha);
                                       a[0] = (A + 1) - (A - 1) * c + 2 * sA * alpha; a[1] = 2 * ((A - 1) - (A + 1) * c); a[2] = (A + 1) - (A - 1) * c - 2 * sA * alpha; }
    }
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); xb.resize(n); yb.resize(n); const float* x = sig(g, 0);
        double f0 = ctrl(g, 1), q = ctrl(g, 2), gain = in.size() > 3 ? ctrl(g, 3) : 0;
        if (f0 != lf || q != lq || gain != lg) { coefficients(std::max(1.0, std::min(f0, sr * 0.49)), q, gain); lf = f0; lq = q; lg = gain; }
        for (int k = 0; k < n; k++) xb[k] = x[k];
        iir_run(xb.data(), yb.data(), n, b, 3, a, 3, st);
        for (int k = 0; k < n; k++) out[0][k] = (float)yb[k];
    }
};
struct delay_node : gnode {                               // (delay x samples): samples is a control, up to max_delay
    delay_state st; size_t max_delay; std::vector<double> xb, yb, db;
    delay_node(size_t m) : max_delay(m) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); xb.resize(n); yb.resize(n); db.resize(n); const float* x = sig(g, 0); const float* d = sig(g, 1);
        for (int k = 0; k < n; k++) { xb[k] = x[k]; db[k] = d[k]; }
        delay_run(xb.data(), yb.data(), n, db.data(), st, max_delay);
        for (int k = 0; k < n; k++) out[0][k] = (float)yb[k];
    }
};
struct comb_node : gnode {                                // (comb x d g): d constant, g a control
    comb_state st; size_t d; std::vector<double> xb, yb;
    comb_node(size_t dd) : d(dd) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); xb.resize(n); yb.resize(n); const float* x = sig(g, 0);
        for (int k = 0; k < n; k++) xb[k] = x[k];
        comb_run(xb.data(), yb.data(), n, d, ctrl(g, 1), st);
        for (int k = 0; k < n; k++) out[0][k] = (float)yb[k];
    }
};
struct allpass_node : gnode {
    allpass_state st; size_t d; std::vector<double> xb, yb;
    allpass_node(size_t dd) : d(dd) {}
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); xb.resize(n); yb.resize(n); const float* x = sig(g, 0);
        for (int k = 0; k < n; k++) xb[k] = x[k];
        allpass_run(xb.data(), yb.data(), n, d, ctrl(g, 1), st);
        for (int k = 0; k < n; k++) out[0][k] = (float)yb[k];
    }
};
struct conv_node : gnode {                                // (conv x ir): uniformly partitioned convolution, one block of latency
    size_t B, P; std::vector<std::vector<double>> irf; std::vector<std::vector<double>> fdl; size_t pos = 0;
    std::vector<double> inbuf, acc, prev; size_t filled = 0;
    conv_node(const std::vector<double>& ir, size_t block) : B(block) {
        P = (ir.size() + B - 1) / B; irf.resize(P); fdl.assign(P, std::vector<double>(4 * B, 0.0));
        for (size_t p = 0; p < P; p++) { irf[p].assign(4 * B, 0.0); for (size_t k = 0; k < B && p * B + k < ir.size(); k++) irf[p][2 * k] = ir[p * B + k]; fft_inplace(irf[p].data(), 2 * B, -1); }
        inbuf.assign(4 * B, 0.0); acc.assign(4 * B, 0.0); prev.assign(B, 0.0);
    }
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); const float* x = sig(g, 0);
        // the node runs on the engine's block; a different n is padded/truncated (n == B in practice)
        std::fill(inbuf.begin(), inbuf.end(), 0.0);
        for (size_t k = 0; k < B; k++) { inbuf[2 * k] = prev[k]; inbuf[2 * (B + k)] = k < (size_t)n ? x[k] : 0; }
        for (size_t k = 0; k < B; k++) prev[k] = k < (size_t)n ? x[k] : 0;
        fft_inplace(inbuf.data(), 2 * B, -1);
        fdl[pos] = inbuf;
        std::fill(acc.begin(), acc.end(), 0.0);
        for (size_t p = 0; p < P; p++) {
            const std::vector<double>& X = fdl[(pos + P - p) % P]; const std::vector<double>& H = irf[p];
            for (size_t k = 0; k < 2 * B; k++) { acc[2 * k] += X[2 * k] * H[2 * k] - X[2 * k + 1] * H[2 * k + 1]; acc[2 * k + 1] += X[2 * k] * H[2 * k + 1] + X[2 * k + 1] * H[2 * k]; }
        }
        pos = (pos + 1) % P;
        fft_inplace(acc.data(), 2 * B, 1);
        for (int k = 0; k < n; k++) out[0][k] = (size_t)k < B ? (float)(acc[2 * (B + k)] / (2.0 * B)) : 0;
    }
};
struct pan_node : gnode {                                 // (pan x pos): mono in, stereo out, equal power
    pan_node() { channels = 2; }
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        size(n); const float* x = sig(g, 0); double p = std::max(-1.0, std::min(1.0, (double)ctrl(g, 1)));
        double gl = std::sqrt(0.5 * (1 - p)), gr = std::sqrt(0.5 * (1 + p));
        for (int k = 0; k < n; k++) { out[0][k] = (float)(x[k] * gl); out[1][k] = (float)(x[k] * gr); }
    }
};
struct list_node : gnode {                                // (list a b ...): one channel per input
    void process(std::vector<std::unique_ptr<gnode>>& g, int n) override {
        channels = (int)in.size(); size(n);
        for (size_t j = 0; j < in.size(); j++) { const float* x = sig(g, j); for (int k = 0; k < n; k++) out[j][k] = x[k]; }
    }
};

// A compiled instrument: its nodes in evaluation order, its parameters, its output node
struct synth_instance {
    long id = 0; std::vector<std::unique_ptr<gnode>> nodes; std::vector<int> params; int output = -1; std::string name;
    std::atomic<bool> active{true};
    void render(int n) { for (auto& nd : nodes) nd->process(nodes, n); }
};

// --- the engine ---
struct audio_engine {
    ma_device device{}; bool open = false, running = false, null_backend = false;
    double sr = 44100; int block = 256, channels = 2;
    std::atomic<long long> clock{0};          // samples since start
    std::atomic<double> master{1.0}, load{0.0}, peak{0.0};
    std::atomic<int> active_voices{0};
    command_queue queue;
    static const int MAX_VOICES = 128;
    voice voices[MAX_VOICES];
    std::vector<std::shared_ptr<synth_instance>> synths;   // owned by the audio thread once added
    std::map<long, std::shared_ptr<synth_instance>> compiled;   // interpreter side: every instance created, for parameter lookups
    std::atomic<long> next_id{1};
    std::vector<float> bus;                   // channels * frames, reused
    std::vector<audio_command> pending;       // timed commands not yet due (audio thread only)

    static void callback(ma_device* dev, void* out, const void*, ma_uint32 frames) {
        static_cast<audio_engine*>(dev->pUserData)->render(static_cast<float*>(out), (int)frames);
    }
    void apply(const audio_command& c) {
        switch (c.kind) {
        case audio_command::PLAY: {
            voice* v = nullptr; for (auto& w : voices) if (!w.active) { v = &w; break; }
            if (!v) return;                   // all voices busy: the sound is dropped
            v->id = c.id; v->buffer = c.buffer; v->channels = c.channels; v->buffer_sr = c.buffer_sr;
            v->start = c.at; v->pos = 0; v->rate = c.rate; v->amp = c.amp; v->pan = c.pan; v->loop = c.loop;
            v->target_amp = c.amp; v->target_pan = c.pan; v->target_rate = c.rate; v->active = true; break; }
        case audio_command::STOP: for (auto& v : voices) if (v.active && v.id == c.id) v.active = false; break;
        case audio_command::STOP_ALL: for (auto& v : voices) v.active = false; break;
        case audio_command::GAIN: master = c.value; break;
        case audio_command::ADD_SYNTH: synths.push_back(c.synth); break;
        case audio_command::FREE_SYNTH: for (auto& sy : synths) if (sy->id == c.id) sy->active = false; break;
        case audio_command::SET_PARAM: for (auto& sy : synths) if (sy->id == c.id && c.param_index < (int)sy->params.size()) {
                auto* p = static_cast<param_node*>(sy->nodes[sy->params[c.param_index]].get()); p->target = c.value; p->ramp = c.ramp_samples; } break;
        case audio_command::SET_VOICE: for (auto& v : voices) if (v.active && v.id == c.id) { if (c.param == 0) v.target_amp = c.value; else if (c.param == 1) v.target_pan = c.value; else v.target_rate = c.value; } break;
        }
    }
    void render(float* out, int frames) {
        auto t0 = std::chrono::steady_clock::now();
        audio_command c; while (queue.pop(c)) { if (c.at > clock.load() + frames && c.kind != audio_command::PLAY) pending.push_back(std::move(c)); else apply(c); }
        for (size_t k = 0; k < pending.size();) { if (pending[k].at <= clock.load() + frames) { apply(pending[k]); pending.erase(pending.begin() + k); } else k++; }
        std::memset(out, 0, sizeof(float) * frames * channels);
        long long now = clock.load(); int active = 0; double pk = 0;
        for (auto& v : voices) {
            if (!v.active) continue;
            if (v.start > now + frames) { active++; continue; }         // scheduled later
            const std::vector<float>& b = *v.buffer; size_t len = b.size() / v.channels;
            double step = v.rate * v.buffer_sr / sr;
            // per-block ramps towards the targets (a few ms), so changes do not click
            double k = 0.2; v.amp += (v.target_amp - v.amp) * k; v.pan += (v.target_pan - v.pan) * k; v.rate += (v.target_rate - v.rate) * k;
            double gl = v.amp * (channels >= 2 ? std::sqrt(0.5 * (1 - v.pan)) : 1), gr = v.amp * std::sqrt(0.5 * (1 + v.pan));
            for (int f = 0; f < frames; f++) {
                if (v.start > now + f) continue;
                if (v.pos >= (double)len) { if (v.loop) v.pos -= len; else { v.active = false; break; } }
                size_t i0 = (size_t)v.pos; double fr = v.pos - i0; size_t i1 = (i0 + 1 < len) ? i0 + 1 : (v.loop ? 0 : i0);
                for (int ch = 0; ch < channels; ch++) {
                    int src = v.channels == 1 ? 0 : std::min(ch, v.channels - 1);
                    double s = (1 - fr) * b[i0 * v.channels + src] + fr * b[i1 * v.channels + src];
                    double g = v.channels == 1 && channels >= 2 ? (ch == 0 ? gl : ch == 1 ? gr : v.amp) : v.amp;
                    out[f * channels + ch] += (float)(s * g);
                }
                v.pos += step;
            }
            if (v.active) active++;
        }
        // synths: timed commands were applied above; each instance renders its graph and is mixed in
        for (auto& sy : synths) {
            if (!sy->active) continue;
            sy->render(frames);
            gnode& o = *sy->nodes[sy->output];
            for (int ch = 0; ch < channels; ch++) { const float* src = o.out[std::min(ch, o.channels - 1)].data(); for (int f = 0; f < frames; f++) out[f * channels + ch] += src[f]; }
        }
        synths.erase(std::remove_if(synths.begin(), synths.end(), [](const std::shared_ptr<synth_instance>& sy) { return !sy->active; }), synths.end());
        double m = master.load();
        for (int k = 0; k < frames * channels; k++) { out[k] *= (float)m; pk = std::max(pk, (double)std::fabs(out[k])); }
        active_voices = active; peak = pk; clock += frames;
        double us = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        load = us / ((double)frames / sr);
    }
};
inline audio_engine& engine() { static audio_engine e; return e; }

struct scheduler_state;
inline scheduler_state& sched();
inline void sched_reset();
// --- builtins ---
inline audio_engine& need_open(Interp& i) { if (!engine().open) i.bad("no audio device open: (audio-open sr block channels) first"); return engine(); }

// (audio-open sr block channels [opts]) open the audio device; opts: (list "device" "null") for the
//   silent backend (tests, headless machines). Fails if a device is already open.
inline vptr live_open(vlist& a, Interp& i) {
    audio_engine& e = engine();
    if (e.open) i.bad("a device is already open: (audio-close) first");
    e.sr = i.scalar(a[0]); e.block = (int)i.scalar(a[1]); e.channels = (int)i.scalar(a[2]);
    if (e.sr < 8000 || e.block < 16 || e.block > 8192 || e.channels < 1 || e.channels > 64) i.bad("invalid parameters (sr >= 8000, 16 <= block <= 8192, 1..64 channels)");
    bool want_null = false;
    if (a.size() > 3) for (auto& o : i.list(a[3])) { vlist& kv = i.list(o); if (kv.size() == 2 && str_of(kv[0]) == "device" && str_of(kv[1]) == "null") want_null = true; }
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32; cfg.playback.channels = (ma_uint32)e.channels; cfg.sampleRate = (ma_uint32)e.sr;
    cfg.periodSizeInFrames = (ma_uint32)e.block; cfg.dataCallback = audio_engine::callback; cfg.pUserData = &e;
    ma_result r = MA_ERROR;
    if (!want_null) r = ma_device_init(nullptr, &cfg, &e.device);
    if (r != MA_SUCCESS) {                     // no hardware, or asked for: the null backend keeps time and runs the callback
        ma_backend nb = ma_backend_null; ma_context* ctx = new ma_context();
        if (ma_context_init(&nb, 1, nullptr, ctx) != MA_SUCCESS) i.bad("cannot initialise audio (not even the null backend)");
        r = ma_device_init(ctx, &cfg, &e.device);
        if (r != MA_SUCCESS) i.bad("cannot open an audio device");
        e.null_backend = true;
    } else e.null_backend = false;
    e.sr = e.device.sampleRate; e.channels = e.device.playback.channels; e.block = (int)e.device.playback.internalPeriodSizeInFrames;
    e.clock = 0; e.open = true; e.running = false;
    for (auto& v : e.voices) v.active = false;
    return v_nil();
}
// (audio-close) stop and close the device
inline vptr live_close(vlist&, Interp&) {
    audio_engine& e = engine(); if (!e.open) return v_nil();
    ma_device_uninit(&e.device); e.open = false; e.running = false; e.synths.clear(); e.compiled.clear(); e.pending.clear();
    sched_reset(); return v_nil();
}
// (audio-start) (audio-stop) run or pause the callback; the clock advances only while running
inline vptr live_start(vlist&, Interp& i) { audio_engine& e = need_open(i); if (!e.running) { if (ma_device_start(&e.device) != MA_SUCCESS) i.bad("cannot start the device"); e.running = true; } return v_nil(); }
inline vptr live_stop(vlist&, Interp& i) { audio_engine& e = need_open(i); if (e.running) { ma_device_stop(&e.device); e.running = false; } return v_nil(); }
// (audio-status) => (list (list "open" 0/1) (list "running" 0/1) (list "sr" sr) (list "block" n) (list "channels" n)
//   (list "device" name) (list "time" seconds) (list "voices" n) (list "load" fraction) (list "peak" x))
inline vptr live_status(vlist&, Interp&) {
    audio_engine& e = engine(); vlist out;
    auto add = [&](const char* k, vptr v) { out.push_back(v_list({ v_str(k), v })); };
    add("open", v_bool(e.open)); add("running", v_bool(e.running)); add("sr", v_num(e.sr)); add("block", v_num(e.block)); add("channels", v_num(e.channels));
    std::string name = "none"; if (e.open) { char n[256] = { 0 }; ma_device_get_name(&e.device, ma_device_type_playback, n, sizeof n, nullptr); name = e.null_backend ? "null" : n; }
    add("device", v_str(name)); add("time", v_num((double)e.clock.load() / e.sr)); add("voices", v_num(e.active_voices.load())); add("load", v_num(e.load.load())); add("peak", v_num(e.peak.load()));
    return v_list(std::move(out));
}
// (audio-devices) => list of the playback device names
inline vptr live_devices(vlist&, Interp& i) {
    ma_context ctx; if (ma_context_init(nullptr, 0, nullptr, &ctx) != MA_SUCCESS) i.bad("cannot enumerate devices");
    ma_device_info* infos; ma_uint32 n; vlist out;
    if (ma_context_get_devices(&ctx, &infos, &n, nullptr, nullptr) == MA_SUCCESS) for (ma_uint32 k = 0; k < n; k++) out.push_back(v_str(infos[k].name));
    ma_context_uninit(&ctx); return v_list(std::move(out));
}
// (audio-time) => seconds on the engine's clock (sample-accurate, advances while running)
inline vptr live_time(vlist&, Interp& i) { audio_engine& e = need_open(i); return v_num((double)e.clock.load() / e.sr); }

// (play-buffer buffer sr amp pan rate loop at) => voice id. buffer is a vector (mono) or a list of
//   channel vectors, at its own sample rate; at is the start time in seconds on the clock (0 = now)
inline vptr live_play_buffer(vlist& a, Interp& i) {
    audio_engine& e = need_open(i);
    double bsr = i.scalar(a[1]), amp = i.scalar(a[2]), pan = i.scalar(a[3]), rate = i.scalar(a[4]); bool loop = truthy(a[5]); double at = i.scalar(a[6]);
    if (bsr <= 0 || rate <= 0) i.bad("sample rate and rate must be > 0");
    std::vector<const varr*> ch;
    if (a[0]->t == Value::NUM) ch.push_back(&a[0]->num); else for (auto& c : i.list(a[0])) ch.push_back(&i.num(c));
    if (ch.empty() || ch[0]->size() == 0) i.bad("empty buffer");
    for (auto* c : ch) if (c->size() != ch[0]->size()) i.bad("all channels must have the same length");
    auto buf = std::make_shared<std::vector<float>>(ch[0]->size() * ch.size());
    for (size_t k = 0; k < ch[0]->size(); k++) for (size_t c = 0; c < ch.size(); c++) (*buf)[k * ch.size() + c] = (float)(*ch[c])[k];
    audio_command cmd; cmd.kind = audio_command::PLAY; cmd.id = e.next_id++; cmd.buffer = buf; cmd.channels = (int)ch.size(); cmd.buffer_sr = bsr;
    cmd.rate = rate; cmd.amp = amp; cmd.pan = std::max(-1.0, std::min(1.0, pan)); cmd.loop = loop;
    cmd.at = at > 0 ? (long long)(at * e.sr) : 0;
    if (!e.queue.push(std::move(cmd))) i.bad("the command queue is full");
    return v_num((double)cmd.id);
}
// (stop voice) stop one voice; (stop-all) every voice
inline vptr live_stop_voice(vlist& a, Interp& i) { audio_engine& e = need_open(i); audio_command c; c.kind = audio_command::STOP; c.id = (long)i.scalar(a[0]); e.queue.push(c); return v_nil(); }
inline vptr live_stop_all(vlist&, Interp& i) { audio_engine& e = need_open(i); audio_command c; c.kind = audio_command::STOP_ALL; e.queue.push(c); return v_nil(); }
// (master-gain g) the output gain, applied to the whole bus
inline vptr live_master(vlist& a, Interp& i) { audio_engine& e = need_open(i); audio_command c; c.kind = audio_command::GAIN; c.value = i.scalar(a[0]); e.queue.push(c); return v_nil(); }
// (voice-set voice "amp"|"pan"|"rate" value) change a playing voice, ramped over a few blocks
inline vptr live_voice_set(vlist& a, Interp& i) {
    audio_engine& e = need_open(i); const std::string& p = i.str(a[1]);
    audio_command c; c.kind = audio_command::SET_VOICE; c.id = (long)i.scalar(a[0]); c.value = i.scalar(a[2]);
    c.param = p == "amp" ? 0 : p == "pan" ? 1 : p == "rate" ? 2 : -1; if (c.param < 0) i.bad("parameter is amp, pan or rate");
    e.queue.push(c); return v_nil();
}
// (voices) => the ids of the voices playing or scheduled
inline vptr live_voices(vlist&, Interp& i) { audio_engine& e = need_open(i); vlist out; for (auto& v : e.voices) if (v.active) out.push_back(v_num((double)v.id)); return v_list(std::move(out)); }

// --- the compiler: a Musil function's body -> a graph ---------------------------------
// The streamable table: builtin name -> how its call becomes a node. "sig" arguments are
// compiled to nodes (signals or controls); "const" arguments are evaluated once, at build time.
struct ugen_spec { std::vector<char> args; };            // 's' signal/control input, 'c' build-time constant, 'r' the sample rate (any expression, ignored: the engine's sr is used)
inline const std::map<std::string, ugen_spec>& ugen_table() {
    static const std::map<std::string, ugen_spec> t = {
        { "osc",      { { 'r', 's', 'c' } } },           // (osc sr freq table)
        { "noise",    { { 'c' } } },                     // (noise n): n ignored when streaming
        { "adsr",     { { 'r', 's', 's', 's', 's', 's' } } },   // (adsr sr gate a d s r)
        { "lag",      { { 's', 'r', 's' } } },           // (lag x sr seconds)
        { "iir",      { { 's', 'c', 'c' } } },           // (iir x b a)
        { "lowpass",  { { 's', 'r', 's', 's' } } }, { "highpass", { { 's', 'r', 's', 's' } } }, { "bandpass", { { 's', 'r', 's', 's' } } }, { "notch", { { 's', 'r', 's', 's' } } },
        { "peak-eq",  { { 's', 'r', 's', 's', 's' } } }, { "lowshelf", { { 's', 'r', 's', 's', 's' } } }, { "highshelf", { { 's', 'r', 's', 's', 's' } } },
        { "dc-block", { { 's' } } },                     // (dc-block x)
        { "delay",    { { 's', 's' } } },                // (delay x samples)
        { "comb",     { { 's', 'c', 's' } } }, { "allpass", { { 's', 'c', 's' } } },   // (comb x d g)
        { "conv",     { { 's', 'c' } } },                // (conv x ir)
        { "pan",      { { 's', 's' } } },                // (pan x pos) -> stereo
        { "list",     { {} } },                          // (list a b ...) -> one channel each
        { "+", { {} } }, { "-", { {} } }, { "*", { {} } }, { "/", { {} } },
        { "abs", { { 's' } } }, { "tanh", { { 's' } } }, { "sin", { { 's' } } }, { "cos", { { 's' } } }, { "exp", { { 's' } } }, { "sqrt", { { 's' } } }, { "floor", { { 's' } } },
        { "min", { { 's', 's' } } }, { "max", { { 's', 's' } } }, { "pow", { { 's', 's' } } }, { "clip", { { 's', 's', 's' } } },
    };
    return t;
}
struct synth_compiler {
    Interp& I; eptr env; const std::vector<std::string>& params; synth_instance& out; double sr; int block;
    std::map<std::string, int> param_nodes;
    synth_compiler(Interp& i, eptr e, const std::vector<std::string>& p, synth_instance& o, double s, int b) : I(i), env(e), params(p), out(o), sr(s), block(b) {}
    [[noreturn]] void fail(const std::string& m) { I.bad(m); }
    bool is_param(const vptr& f) { return f->t == Value::SYM && std::find(params.begin(), params.end(), f->s) != params.end(); }
    bool uses_signal(const vptr& f) {                     // does this form depend on a parameter or a ugen?
        if (is_param(f)) return true;
        if (f->t != Value::LIST || f->l.empty()) return false;
        if (f->l[0]->t == Value::SYM && ugen_table().count(f->l[0]->s) && f->l[0]->s != "+" && f->l[0]->s != "-" && f->l[0]->s != "*" && f->l[0]->s != "/" && f->l[0]->s != "list") return true;
        for (auto& e : f->l) if (uses_signal(e)) return true;
        return false;
    }
    int add(gnode* n) { out.nodes.emplace_back(n); return (int)out.nodes.size() - 1; }
    vptr constant(const vptr& f) { return I.eval(f, env); }   // a build-time constant: evaluated like any Musil expression
    std::vector<double> vec_of(const vptr& v, const char* what) { if (v->t != Value::NUM) fail(std::string(what) + ": expected a number or vector"); return std::vector<double>(std::begin(v->num), std::end(v->num)); }
    int compile(const vptr& f) {
        if (f->t == Value::NUM) { if (f->num.size() != 1) fail("a vector constant cannot be a signal"); return add(new const_node(f->num[0])); }
        if (is_param(f)) {
            auto it = param_nodes.find(f->s); if (it != param_nodes.end()) return it->second;
            int k = add(new param_node(f->s, 0)); param_nodes[f->s] = k; out.params.push_back(k); return k;
        }
        if (!uses_signal(f)) { vptr v = constant(f); if (v->t != Value::NUM || v->num.size() != 1) fail("a constant in a signal position must be a number: " + str_of(f)); return add(new const_node(v->num[0])); }
        if (f->t != Value::LIST || f->l.empty() || f->l[0]->t != Value::SYM) fail("cannot stream " + str_of(f));
        const std::string& h = f->l[0]->s; vlist args(f->l.begin() + 1, f->l.end());
        auto it = ugen_table().find(h);
        if (it == ugen_table().end()) fail("not streamable: " + h + " (see (streamable))");
        // inputs are compiled (and so added) before the node that reads them: the node list is its own evaluation order
        if (h == "+" || h == "-" || h == "*" || h == "/") { std::vector<int> ins; for (auto& a : args) ins.push_back(compile(a)); arith_node* n = new arith_node(h[0]); n->in = ins; return add(n); }
        if (h == "list") { std::vector<int> ins; for (auto& a : args) ins.push_back(compile(a)); list_node* n = new list_node(); n->in = ins; return add(n); }
        const ugen_spec& spec = it->second;
        if (args.size() < spec.args.size() && !(h == "noise")) fail(h + ": expected " + std::to_string(spec.args.size()) + " arguments");
        std::vector<int> sigs; std::vector<vptr> consts;
        for (size_t k = 0; k < spec.args.size() && k < args.size(); k++) {
            if (spec.args[k] == 's') sigs.push_back(compile(args[k]));
            else if (spec.args[k] == 'c') consts.push_back(constant(args[k]));
        }
        gnode* n = nullptr;
        if (h == "osc") { auto t = vec_of(consts[0], "osc table"); if (t.size() < 2) fail("osc: table too short"); n = new osc_node(t, sr); }
        else if (h == "noise") n = new noise_node();
        else if (h == "adsr") n = new adsr_node(sr);
        else if (h == "lag") n = new lag_node(sr);
        else if (h == "iir") n = new iir_node(vec_of(consts[0], "iir b"), vec_of(consts[1], "iir a"));
        else if (h == "dc-block") n = new iir_node({ 1, -1 }, { 1, -0.995 });
        else if (h == "delay") n = new delay_node((size_t)(sr * 2));       // up to two seconds
        else if (h == "comb") { long d = (long)consts[0]->num[0]; if (d < 1) fail("comb: delay must be >= 1"); n = new comb_node((size_t)d); }
        else if (h == "allpass") { long d = (long)consts[0]->num[0]; if (d < 1) fail("allpass: delay must be >= 1"); n = new allpass_node((size_t)d); }
        else if (h == "conv") { auto ir = vec_of(consts[0], "conv ir"); if (ir.empty()) fail("conv: empty impulse response"); n = new conv_node(ir, (size_t)block); }
        else if (h == "pan") n = new pan_node();
        else if (h == "lowpass" || h == "highpass" || h == "bandpass" || h == "notch" || h == "peak-eq" || h == "lowshelf" || h == "highshelf") n = new biquad_node(h, sr);
        else n = new math_node(h);
        int k = add(n); n->in = sigs; return k;
    }
};

// (synth f) => id: compile the function f into a running instrument (silent until its parameters say otherwise).
//   f's parameters become hot parameters; its body may use only the streamable builtins (see (streamable))
inline vptr live_synth(vlist& a, Interp& i) {
    audio_engine& e = need_open(i); const vptr& f = a[0];
    if (f->t != Value::FN || f->op) i.bad("expected a Musil function (not a builtin)");
    vptr body = f->body;
    if (body->t == Value::LIST && !body->l.empty() && body->l[0]->t == Value::SYM && body->l[0]->s == "do" && body->l.size() == 2) body = body->l[1];
    auto inst = std::make_shared<synth_instance>(); inst->id = e.next_id++; inst->name = f->s;
    synth_compiler c(i, f->closure ? f->closure : i.global, f->params, *inst, e.sr, e.block);
    inst->output = c.compile(body);
    for (auto& p : f->params) if (!c.param_nodes.count(p)) { int k = c.add(new param_node(p, 0)); c.param_nodes[p] = k; inst->params.push_back(k); }   // unused parameters still exist
    e.compiled[inst->id] = inst;
    audio_command cmd; cmd.kind = audio_command::ADD_SYNTH; cmd.synth = inst;
    if (!e.queue.push(std::move(cmd))) i.bad("the command queue is full");
    return v_num((double)inst->id);
}
// (set-param id name value [ramp-seconds]) change a synth's parameter, immediately or ramped
inline vptr live_set_param(vlist& a, Interp& i) {
    audio_engine& e = need_open(i); long id = (long)i.scalar(a[0]);
    std::string name = a[1]->t == Value::SYM || a[1]->t == Value::STR ? a[1]->s : str_of(a[1]);
    synth_instance* s = nullptr;
    { auto it = e.compiled.find(id); if (it != e.compiled.end()) s = it->second.get(); }
    if (!s) i.bad("no synth " + std::to_string(id));
    int idx = -1; for (size_t k = 0; k < s->params.size(); k++) if (static_cast<param_node*>(s->nodes[s->params[k]].get())->name == name) idx = (int)k;
    if (idx < 0) i.bad("synth has no parameter " + name);
    audio_command cmd; cmd.kind = audio_command::SET_PARAM; cmd.id = id; cmd.param_index = idx; cmd.value = i.scalar(a[2]);
    cmd.ramp_samples = a.size() > 3 ? (int)(i.scalar(a[3]) * e.sr) : 0;
    if (a.size() > 4) cmd.at = (long long)(i.scalar(a[4]) * e.sr);
    if (!e.queue.push(cmd)) i.bad("the command queue is full");
    return v_nil();
}
// (synth-render f params seconds [sr]) => the sound of f as the engine would stream it, computed offline block
//   by block with its parameters set to params (a list of (list name value) or (list name buffer) for a value
//   per sample); the same graph as (synth f), so the result equals f called on the same signals. No device needed.
inline vptr live_synth_render(vlist& a, Interp& i) {
    const vptr& f = a[0]; vlist& ps = i.list(a[1]); double secs = i.scalar(a[2]);
    double sr = a.size() > 3 ? i.scalar(a[3]) : (engine().open ? engine().sr : 44100); int block = engine().open ? engine().block : 256;
    if (f->t != Value::FN || f->op) i.bad("expected a Musil function (not a builtin)");
    vptr body = f->body;
    if (body->t == Value::LIST && !body->l.empty() && body->l[0]->t == Value::SYM && body->l[0]->s == "do" && body->l.size() == 2) body = body->l[1];
    synth_instance inst; synth_compiler c(i, f->closure ? f->closure : i.global, f->params, inst, sr, block);
    inst.output = c.compile(body);
    for (auto& p : f->params) if (!c.param_nodes.count(p)) { int k = c.add(new param_node(p, 0)); c.param_nodes[p] = k; inst.params.push_back(k); }
    std::map<std::string, std::vector<double>> curves;
    for (auto& e : ps) { vlist& kv = i.list(e); if (kv.size() != 2) i.bad("params are (list name value)"); std::string nm = kv[0]->t == Value::SYM || kv[0]->t == Value::STR ? kv[0]->s : str_of(kv[0]);
        if (!c.param_nodes.count(nm)) i.bad("no parameter " + nm);
        const varr& v = i.num(kv[1]); curves[nm] = std::vector<double>(std::begin(v), std::end(v)); }
    size_t total = (size_t)(secs * sr); gnode& o = *inst.nodes[inst.output];
    std::vector<std::vector<double>> out;
    for (size_t pos = 0; pos < total; pos += block) {
        int n = (int)std::min<size_t>(block, total - pos);
        for (auto& kv : curves) {
            auto* p = static_cast<param_node*>(inst.nodes[c.param_nodes[kv.first]].get());
            if (kv.second.size() == 1) { p->target = kv.second[0]; p->ramp = 0; p->drive = nullptr; }
            else { if (kv.second.size() < total) kv.second.resize(total, kv.second.back()); p->drive = &kv.second[pos]; }   // a value per sample
        }
        inst.render(n);
        if (out.empty()) out.resize(o.channels);
        for (int ch = 0; ch < o.channels; ch++) for (int k = 0; k < n; k++) out[ch].push_back(o.out[ch][k]);
    }
    if (out.size() == 1) return from_vec(out[0]);
    vlist chans; for (auto& ch : out) chans.push_back(from_vec(ch)); return v_list(std::move(chans));
}
// (free id) remove a synth from the graph
inline vptr live_free(vlist& a, Interp& i) { audio_engine& e = need_open(i); audio_command c; c.kind = audio_command::FREE_SYNTH; c.id = (long)i.scalar(a[0]); e.queue.push(c); e.compiled.erase(c.id); return v_nil(); }
// (synth-params id) => the parameter names of a synth, in the order of its function
inline vptr live_synth_params(vlist& a, Interp& i) {
    audio_engine& e = need_open(i); long id = (long)i.scalar(a[0]); synth_instance* s = nullptr;
    { auto it = e.compiled.find(id); if (it != e.compiled.end()) s = it->second.get(); }
    if (!s) i.bad("no synth " + std::to_string(id));
    vlist out; for (int k : s->params) out.push_back(v_str(static_cast<param_node*>(s->nodes[k].get())->name)); return v_list(std::move(out));
}
// (synths) => the ids of the running synths
inline vptr live_synths(vlist&, Interp& i) { audio_engine& e = need_open(i); vlist out; for (auto& kv : e.compiled) if (kv.second->active) out.push_back(v_num((double)kv.first)); return v_list(std::move(out)); }
// (streamable) => the names of the builtins a synth function may use
inline vptr live_streamable(vlist&, Interp&) { vlist out; for (auto& kv : ugen_table()) out.push_back(v_str(kv.first)); return v_list(std::move(out)); }

// --- the scheduler: loops in beats, run ahead of the clock ---------------------------------
// A loop is a name and a length in beats. Each cycle, the function bound to that name (looked
// up every time, so redefining it is the live coding) is called with the cycle number and
// returns events: (list beat dur thunk), thunk a function of the absolute time in seconds that
// schedules whatever it wants (note-at, play-at, set-param with a time). The scheduler runs
// from the interpreter's idle hook, a little ahead of the audio clock, so late calls still
// land on time.
struct live_loop { std::string name; double beats; long cycle = 0; double next_start = 0; bool active = true; };
struct scheduler_state { double bpm = 120; double beat0_time = 0; double beat0 = 0; double lookahead = 0.25; std::vector<live_loop> loops; bool running = false; };
inline scheduler_state& sched() { static scheduler_state s; return s; }
inline void sched_reset() { sched().loops.clear(); sched().running = false; }
inline double live_now() { return engine().open ? (double)engine().clock.load() / engine().sr : 0; }
inline double live_beat_at(double t) { scheduler_state& s = sched(); return s.beat0 + (t - s.beat0_time) * s.bpm / 60.0; }
inline double live_time_of_beat(double b) { scheduler_state& s = sched(); return s.beat0_time + (b - s.beat0) * 60.0 / s.bpm; }

// --- controls: named values with a range, bound to synth parameters, shown by the hosts ------
struct control { std::string name; double lo, hi, value; bool toggle; std::vector<std::pair<long, std::string>> bindings; std::string osc; };
inline std::vector<control>& controls() { static std::vector<control> c; return c; }
inline control* find_control(const std::string& n) { for (auto& c : controls()) if (c.name == n) return &c; return nullptr; }
inline void apply_control(Interp& i, control& c) {
    for (auto& b : c.bindings) {
        vlist a = { v_num((double)b.first), v_sym(b.second), v_num(c.value), v_num(0.02) };
        try { live_set_param(a, i); } catch (...) {}
    }
}

// --- OSC: a minimal encoder and decoder (f i s types) over UDP -----------------------------
inline void osc_pad(std::string& b) { while (b.size() % 4) b += '\0'; }
inline std::string osc_encode(const std::string& addr, const vlist& args) {
    std::string out = addr; out += '\0'; osc_pad(out); std::string tags = ",";
    std::string data;
    for (auto& a : args) {
        if (a->t == Value::NUM && a->num.size() == 1) { tags += 'f'; float f = (float)a->num[0]; uint32_t u; std::memcpy(&u, &f, 4); for (int k = 3; k >= 0; k--) data += (char)((u >> (8 * k)) & 255); }
        else { tags += 's'; data += str_of(a); data += '\0'; osc_pad(data); }
    }
    out += tags; out += '\0'; osc_pad(out); out += data; return out;
}
inline bool osc_decode(const std::string& msg, std::string& addr, vlist& args) {
    size_t p = msg.find('\0'); if (p == std::string::npos) return false; addr = msg.substr(0, p); p = (p + 4) & ~size_t(3);
    if (p >= msg.size() || msg[p] != ',') return true;
    size_t q = msg.find('\0', p); if (q == std::string::npos) return false; std::string tags = msg.substr(p + 1, q - p - 1); p = (q + 4) & ~size_t(3);
    for (char t : tags) {
        if (t == 'f' && p + 4 <= msg.size()) { uint32_t u = 0; for (int k = 0; k < 4; k++) u = (u << 8) | (unsigned char)msg[p + k]; float f; std::memcpy(&f, &u, 4); args.push_back(v_num(f)); p += 4; }
        else if (t == 'i' && p + 4 <= msg.size()) { int32_t v = 0; for (int k = 0; k < 4; k++) v = (v << 8) | (unsigned char)msg[p + k]; args.push_back(v_num(v)); p += 4; }
        else if (t == 's') { size_t e = msg.find('\0', p); if (e == std::string::npos) return false; args.push_back(v_str(msg.substr(p, e - p))); p = (e + 4) & ~size_t(3); }
        else return false;
    }
    return true;
}
#ifndef _WIN32
struct osc_listener { int sock = -1; int port = 0; };
inline osc_listener& osc_in() { static osc_listener l; return l; }
#endif

// The idle work: due loops, incoming OSC. Called from the interpreter's idle hook (never re-entered).
inline void live_idle(Interp& i) {
    scheduler_state& s = sched();
    if (engine().open && engine().running && s.running) {
        double now = live_now();
        for (auto& lp : s.loops) {
            if (!lp.active) continue;
            int guard = 0;
            while (live_time_of_beat(lp.next_start) < now + s.lookahead && guard++ < 8) {
                vptr fn = i.global->find(lp.name);
                if (!fn || fn->t != Value::FN) { lp.active = false; *i.out << "live-loop " << lp.name << ": no function of that name, stopped\n" << std::flush; break; }
                double start_time = live_time_of_beat(lp.next_start), spb = 60.0 / s.bpm;
                vptr events;
                try { events = i.call_fn(fn, { v_num((double)lp.cycle) }); }
                catch (std::exception& e) { *i.out << "live-loop " << lp.name << ": " << e.what() << "\n" << std::flush; events = v_list({}); }
                if (events && events->t == Value::LIST) for (auto& ev : events->l) {
                    if (ev->t != Value::LIST || ev->l.size() < 3 || ev->l[0]->t != Value::NUM || ev->l[2]->t != Value::FN) continue;
                    double t = start_time + ev->l[0]->num[0] * spb, d = ev->l[1]->t == Value::NUM ? ev->l[1]->num[0] * spb : 0;
                    try { i.call_fn(ev->l[2], { v_num(t), v_num(d) }); } catch (std::exception& e) { *i.out << "live-loop " << lp.name << ": " << e.what() << "\n" << std::flush; }
                }
                lp.cycle++; lp.next_start += lp.beats;
            }
        }
        s.loops.erase(std::remove_if(s.loops.begin(), s.loops.end(), [](const live_loop& l) { return !l.active; }), s.loops.end());
    }
#ifndef _WIN32
    if (osc_in().sock >= 0) {
        char buf[4096]; sockaddr_in from{}; socklen_t fl = sizeof from;
        for (int k = 0; k < 64; k++) {
            long n = recvfrom(osc_in().sock, buf, sizeof buf, MSG_DONTWAIT, (sockaddr*)&from, &fl);
            if (n <= 0) break;
            std::string addr; vlist args;
            if (!osc_decode(std::string(buf, (size_t)n), addr, args)) continue;
            for (auto& c : controls()) if (c.osc == addr && !args.empty() && args[0]->t == Value::NUM) { c.value = std::max(c.lo, std::min(c.hi, args[0]->num[0])); apply_control(i, c); }
        }
    }
#endif
}

// (tempo bpm) set the tempo; beats are counted from now
inline vptr live_tempo(vlist& a, Interp& i) {
    double bpm = i.scalar(a[0]); if (bpm <= 0) i.bad("bpm must be > 0");
    scheduler_state& s = sched(); double now = live_now(); s.beat0 = live_beat_at(now); s.beat0_time = now; s.bpm = bpm; return v_nil();
}
// (beat) => the current beat (a number, fractional); (beat-time b) => the clock time of beat b; (bpm) => the tempo
inline vptr live_beat(vlist&, Interp&) { return v_num(live_beat_at(live_now())); }
inline vptr live_beat_time(vlist& a, Interp& i) { return v_num(live_time_of_beat(i.scalar(a[0]))); }
inline vptr live_bpm(vlist&, Interp&) { return v_num(sched().bpm); }
// (live-loop name beats) start (or restart) the loop named name: the function of that name is called every
//   cycle of `beats` beats; (stop-loop name) (stop-loops) (loops) => the names
inline vptr live_loop_start(vlist& a, Interp& i) {
    need_open(i); std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]); double beats = i.scalar(a[1]);
    if (beats <= 0) i.bad("beats must be > 0");
    vptr fn = i.global->find(name); if (!fn || fn->t != Value::FN) i.bad("no function named " + name);
    scheduler_state& s = sched(); s.running = true;
    double now_beat = live_beat_at(live_now()), prev = std::floor(now_beat / beats) * beats;
    double start = now_beat - prev < 0.25 ? prev : prev + beats;     // just after a boundary: start now; otherwise at the next one
    for (auto& lp : s.loops) if (lp.name == name) { lp.beats = beats; lp.active = true; return v_nil(); }
    s.loops.push_back({ name, beats, 0, start, true }); return v_nil();
}
inline vptr live_loop_stop(vlist& a, Interp&) { std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]); for (auto& lp : sched().loops) if (lp.name == name) lp.active = false; return v_nil(); }
inline vptr live_loops_stop(vlist&, Interp&) { for (auto& lp : sched().loops) lp.active = false; return v_nil(); }
inline vptr live_loops(vlist&, Interp&) { vlist out; for (auto& lp : sched().loops) if (lp.active) out.push_back(v_str(lp.name)); return v_list(std::move(out)); }
// (lookahead seconds) how far ahead of the clock loops are scheduled (default 0.25)
inline vptr live_lookahead(vlist& a, Interp& i) { double v = i.scalar(a[0]); if (v < 0.01) i.bad("lookahead must be >= 0.01"); sched().lookahead = v; return v_nil(); }

// (control name lo hi value) declare a control (a range and a value); (toggle name value) an on/off one
inline vptr live_control(vlist& a, Interp& i) {
    std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]);
    double lo = i.scalar(a[1]), hi = i.scalar(a[2]), v = i.scalar(a[3]); if (hi <= lo) i.bad("hi must be > lo");
    control* c = find_control(name); if (!c) { controls().push_back({ name, lo, hi, v, false, {}, "" }); c = &controls().back(); }
    c->lo = lo; c->hi = hi; c->value = std::max(lo, std::min(hi, v)); c->toggle = false; apply_control(i, *c); return v_nil();
}
inline vptr live_toggle(vlist& a, Interp& i) {
    std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]); double v = truthy(a[1]) ? 1 : 0;
    control* c = find_control(name); if (!c) { controls().push_back({ name, 0, 1, v, true, {}, "" }); c = &controls().back(); }
    c->lo = 0; c->hi = 1; c->value = v; c->toggle = true; apply_control(i, *c); return v_nil();
}
// (set-control name value) (control-value name) (controls-list) => (list (list name lo hi value toggle?) ...)
inline vptr live_set_control(vlist& a, Interp& i) {
    std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]);
    control* c = find_control(name); if (!c) i.bad("no control " + name);
    c->value = std::max(c->lo, std::min(c->hi, i.scalar(a[1]))); apply_control(i, *c); return v_num(c->value);
}
inline vptr live_control_value(vlist& a, Interp& i) { std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]); control* c = find_control(name); if (!c) i.bad("no control " + name); return v_num(c->value); }
inline vptr live_controls_list(vlist&, Interp&) { vlist out; for (auto& c : controls()) out.push_back(v_list({ v_str(c.name), v_num(c.lo), v_num(c.hi), v_num(c.value), v_bool(c.toggle) })); return v_list(std::move(out)); }
// (bind-control name synth-id param) a synth parameter follows the control (several bindings allowed); (unbind-control name)
inline vptr live_bind_control(vlist& a, Interp& i) {
    std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]);
    control* c = find_control(name); if (!c) i.bad("no control " + name);
    std::string param = a[2]->t == Value::SYM || a[2]->t == Value::STR ? a[2]->s : str_of(a[2]);
    c->bindings.push_back({ (long)i.scalar(a[1]), param }); apply_control(i, *c); return v_nil();
}
inline vptr live_unbind_control(vlist& a, Interp& i) { std::string name = a[0]->t == Value::SYM || a[0]->t == Value::STR ? a[0]->s : str_of(a[0]); control* c = find_control(name); if (!c) i.bad("no control " + name); c->bindings.clear(); return v_nil(); }
// (clear-controls) forget every control
inline vptr live_clear_controls(vlist&, Interp&) { controls().clear(); return v_nil(); }

// (osc-send host port address args...) send an OSC message (numbers as floats, anything else as strings)
inline vptr live_osc_send(vlist& a, Interp& i) {
#ifndef _WIN32
    std::string host = i.str(a[0]); int port = (int)i.scalar(a[1]); std::string addr = i.str(a[2]);
    vlist args(a.begin() + 3, a.end()); std::string msg = osc_encode(addr, args);
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); if (sock < 0) return v_bool(false);
    sockaddr_in srv{}; srv.sin_family = AF_INET; srv.sin_port = htons((uint16_t)port); srv.sin_addr.s_addr = inet_addr(host.c_str());
    long r = sendto(sock, msg.data(), msg.size(), 0, (sockaddr*)&srv, sizeof srv); ::close(sock); return v_bool(r >= 0);
#else
    return v_bool(false);
#endif
}
// (osc-listen port) receive OSC on a port (polled in the background); (osc-map "/address" control-name) the first
//   number of a message at that address sets the control; (osc-stop)
inline vptr live_osc_listen(vlist& a, Interp& i) {
#ifndef _WIN32
    int port = (int)i.scalar(a[0]); osc_listener& l = osc_in();
    if (l.sock >= 0) ::close(l.sock);
    l.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); if (l.sock < 0) i.bad("cannot open a socket");
    int yes = 1; setsockopt(l.sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port); addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(l.sock, (sockaddr*)&addr, sizeof addr) < 0) { ::close(l.sock); l.sock = -1; i.bad("cannot bind port " + std::to_string(port)); }
    l.port = port; return v_nil();
#else
    i.bad("OSC input is not available on this platform");
#endif
}
inline vptr live_osc_map(vlist& a, Interp& i) { std::string addr = i.str(a[0]); std::string name = a[1]->t == Value::SYM || a[1]->t == Value::STR ? a[1]->s : str_of(a[1]); control* c = find_control(name); if (!c) i.bad("no control " + name); c->osc = addr; return v_nil(); }
inline vptr live_osc_stop(vlist&, Interp&) {
#ifndef _WIN32
    osc_listener& l = osc_in(); if (l.sock >= 0) ::close(l.sock); l.sock = -1; l.port = 0;
#endif
    return v_nil();
}

inline void add_live(Interp& i) {
    i.def("audio-open", live_open, 3, 4); i.def("audio-close", live_close, 0, 0);
    i.def("audio-start", live_start, 0, 0); i.def("audio-stop", live_stop, 0, 0);
    i.def("audio-status", live_status, 0, 0); i.def("audio-devices", live_devices, 0, 0); i.def("audio-time", live_time, 0, 0);
    i.def("play-buffer", live_play_buffer, 7, 7); i.def("stop", live_stop_voice, 1, 1); i.def("stop-all", live_stop_all, 0, 0);
    i.def("master-gain", live_master, 1, 1); i.def("voice-set", live_voice_set, 3, 3); i.def("voices", live_voices, 0, 0);
    i.def("synth", live_synth, 1, 1); i.def("set-param", live_set_param, 3, 5); i.def("free", live_free, 1, 1);
    i.def("synth-params", live_synth_params, 1, 1); i.def("synths", live_synths, 0, 0); i.def("streamable", live_streamable, 0, 0);
    i.def("synth-render", live_synth_render, 3, 4);
    i.def("tempo", live_tempo, 1, 1); i.def("beat", live_beat, 0, 0); i.def("beat-time", live_beat_time, 1, 1); i.def("bpm", live_bpm, 0, 0);
    i.def("live-loop", live_loop_start, 2, 2); i.def("stop-loop", live_loop_stop, 1, 1); i.def("stop-loops", live_loops_stop, 0, 0); i.def("loops", live_loops, 0, 0);
    i.def("lookahead", live_lookahead, 1, 1);
    i.def("control", live_control, 4, 4); i.def("toggle", live_toggle, 2, 2); i.def("set-control", live_set_control, 2, 2); i.def("control-value", live_control_value, 1, 1);
    i.def("controls-list", live_controls_list, 0, 0); i.def("bind-control", live_bind_control, 3, 3); i.def("unbind-control", live_unbind_control, 1, 1); i.def("clear-controls", live_clear_controls, 0, 0);
    i.def("osc-send", live_osc_send, 3, -1); i.def("osc-listen", live_osc_listen, 1, 1); i.def("osc-map", live_osc_map, 2, 2); i.def("osc-stop", live_osc_stop, 0, 0);
    i.idle_fn = [&i]() { live_idle(i); };
}

} // namespace musil
