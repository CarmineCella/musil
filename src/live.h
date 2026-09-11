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
//   This is the first slice of live: the streaming graph (synth functions compiled to
//   nodes), the scheduler's patterns and the controls come next, on top of this queue,
//   this clock and these voices.

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

namespace musil {

// --- a single-producer single-consumer queue of commands (interpreter -> audio thread) ---
struct audio_command {
    enum kind_t { PLAY, STOP, STOP_ALL, GAIN, SET_VOICE } kind = PLAY;
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
    std::atomic<long> next_id{1};
    std::vector<float> bus;                   // channels * frames, reused

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
        case audio_command::SET_VOICE: for (auto& v : voices) if (v.active && v.id == c.id) { if (c.param == 0) v.target_amp = c.value; else if (c.param == 1) v.target_pan = c.value; else v.target_rate = c.value; } break;
        }
    }
    void render(float* out, int frames) {
        auto t0 = std::chrono::steady_clock::now();
        audio_command c; while (queue.pop(c)) apply(c);
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
        double m = master.load();
        for (int k = 0; k < frames * channels; k++) { out[k] *= (float)m; pk = std::max(pk, (double)std::fabs(out[k])); }
        active_voices = active; peak = pk; clock += frames;
        double us = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        load = us / ((double)frames / sr);
    }
};
inline audio_engine& engine() { static audio_engine e; return e; }

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
    ma_device_uninit(&e.device); e.open = false; e.running = false; return v_nil();
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

inline void add_live(Interp& i) {
    i.def("audio-open", live_open, 3, 4); i.def("audio-close", live_close, 0, 0);
    i.def("audio-start", live_start, 0, 0); i.def("audio-stop", live_stop, 0, 0);
    i.def("audio-status", live_status, 0, 0); i.def("audio-devices", live_devices, 0, 0); i.def("audio-time", live_time, 0, 0);
    i.def("play-buffer", live_play_buffer, 7, 7); i.def("stop", live_stop_voice, 1, 1); i.def("stop-all", live_stop_all, 0, 0);
    i.def("master-gain", live_master, 1, 1); i.def("voice-set", live_voice_set, 3, 3); i.def("voices", live_voices, 0, 0);
}

} // namespace musil
