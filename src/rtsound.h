#ifndef RTSOUND_H
#define RTSOUND_H

#include "core.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef BUILD_MUSIL_RTSOUND
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#endif

// ── Helpers ─────────────────────────────────────────────────────────────────

static constexpr uint32_t k_default_sample_rate = 44100;

struct PlaybackBuffer {
    std::vector<float> interleaved;
    uint32_t sample_rate = k_default_sample_rate;
    uint32_t channels    = 0;
    uint64_t frames      = 0;
};

static inline float clamp_sample(double x) {
    if (x >  1.0) return  1.0f;
    if (x < -1.0) return -1.0f;
    return static_cast<float>(x);
}

static inline uint32_t parse_play_sample_rate(const Value& v,
        Interpreter& interp,
        const std::string& fn) {
    double sr = scalar(v, fn);
    if (sr <= 0.0) {
        throw Error{interp.filename, interp.cur_line(),
                    fn + ": sample rate must be > 0", {}};
    }
    return static_cast<uint32_t>(sr);
}

static inline PlaybackBuffer value_to_playback_buffer(const Value& v,
        uint32_t sample_rate,
        Interpreter& interp,
        const std::string& fn) {
    PlaybackBuffer pb;
    pb.sample_rate = sample_rate;

    if (std::holds_alternative<NumVal>(v)) {
        const auto& nv = std::get<NumVal>(v);
        pb.channels = 1;
        pb.frames   = static_cast<uint64_t>(nv.size());
        pb.interleaved.resize(static_cast<std::size_t>(pb.frames));

        for (std::size_t i = 0; i < nv.size(); ++i) {
            pb.interleaved[i] = clamp_sample(nv[i]);
        }

        return pb;
    }

    if (std::holds_alternative<ArrayPtr>(v)) {
        auto arr = std::get<ArrayPtr>(v);
        if (arr->elems.empty()) {
            throw Error{interp.filename, interp.cur_line(),
                        fn + ": channel array is empty", {}};
        }

        pb.channels = static_cast<uint32_t>(arr->elems.size());

        std::size_t nframes = 0;
        bool first = true;
        std::vector<const NumVal*> chans;
        chans.reserve(arr->elems.size());

        for (const auto& elem : arr->elems) {
            if (!std::holds_alternative<NumVal>(elem)) {
                throw Error{interp.filename, interp.cur_line(),
                            fn + ": expected a vector or an array of vectors", {}};
            }

            const auto& nv = std::get<NumVal>(elem);

            if (first) {
                nframes = nv.size();
                first = false;
            } else if (nv.size() != nframes) {
                throw Error{interp.filename, interp.cur_line(),
                            fn + ": all channel vectors must have the same length", {}};
            }

            chans.push_back(&nv);
        }

        pb.frames = static_cast<uint64_t>(nframes);
        pb.interleaved.resize(static_cast<std::size_t>(pb.frames) * pb.channels);

        for (std::size_t i = 0; i < nframes; ++i) {
            for (std::size_t ch = 0; ch < chans.size(); ++ch) {
                pb.interleaved[i * pb.channels + ch] = clamp_sample((*(chans[ch]))[i]);
            }
        }

        return pb;
    }

    throw Error{interp.filename, interp.cur_line(),
                fn + ": expected a vector or an array of vectors", {}};
}

#ifdef BUILD_MUSIL_RTSOUND

struct Voice {
    std::shared_ptr<PlaybackBuffer> buffer;
    std::atomic<uint64_t> cursor_frames {0};
    std::atomic<bool> finished {false};
};

inline std::mutex g_mutex;
inline std::thread g_thread;

inline std::vector<std::shared_ptr<Voice>> g_voices;

inline std::atomic<bool> g_running {false};
inline std::atomic<bool> g_engine_stop_requested {false};

inline uint32_t g_sample_rate = 0;
inline uint32_t g_channels    = 0;

// ── Helpers ───────────────────────────────────────────────────────────────

static inline void join_thread_if_needed_locked() {
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

static inline void cleanup_finished_voices_locked() {
    g_voices.erase(
        std::remove_if(g_voices.begin(), g_voices.end(),
    [](const std::shared_ptr<Voice>& v) {
        return !v || v->finished.load(std::memory_order_relaxed);
    }),
    g_voices.end()
    );
}

static inline bool has_active_voices_locked() {
    cleanup_finished_voices_locked();
    return !g_voices.empty();
}

static inline void clear_all_voices_locked() {
    for (auto& v : g_voices) {
        if (v) v->finished.store(true, std::memory_order_relaxed);
    }
    g_voices.clear();
}

static inline std::size_t active_voice_count_locked() {
    cleanup_finished_voices_locked();
    return g_voices.size();
}

// ── Callback ──────────────────────────────────────────────────────────────

static inline void playback_callback(ma_device* device,
                                     void* output,
                                     const void* input,
                                     ma_uint32 frame_count) {
    (void)input;
    (void)device;

    float* out = static_cast<float*>(output);
    if (!out) return;

    const std::size_t nsamp = static_cast<std::size_t>(frame_count) * g_channels;
    std::fill(out, out + nsamp, 0.0f);

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_engine_stop_requested.load(std::memory_order_relaxed)) {
        return;
    }

    for (auto& voice : g_voices) {
        if (!voice || voice->finished.load(std::memory_order_relaxed) || !voice->buffer) {
            continue;
        }

        const auto& buf = *voice->buffer;

        const uint64_t cursor = voice->cursor_frames.load(std::memory_order_relaxed);
        if (cursor >= buf.frames) {
            voice->finished.store(true, std::memory_order_relaxed);
            continue;
        }

        const uint64_t remain  = buf.frames - cursor;
        const uint64_t to_copy = std::min<uint64_t>(remain, frame_count);

        const float* src = buf.interleaved.data() + cursor * buf.channels;

        for (uint64_t f = 0; f < to_copy; ++f) {
            for (uint32_t ch = 0; ch < buf.channels; ++ch) {
                const std::size_t idx = static_cast<std::size_t>(f * buf.channels + ch);
                out[idx] += src[idx];
            }
        }

        voice->cursor_frames.store(cursor + to_copy, std::memory_order_relaxed);

        if (cursor + to_copy >= buf.frames) {
            voice->finished.store(true, std::memory_order_relaxed);
        }
    }

    for (std::size_t i = 0; i < nsamp; ++i) {
        out[i] = clamp_sample(out[i]);
    }

    cleanup_finished_voices_locked();
}

// ── Engine thread ──────────────────────────────────────────────────────────

static inline void thread_main(uint32_t sample_rate, uint32_t channels) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate        = sample_rate;
    config.dataCallback      = playback_callback;
    config.pUserData         = nullptr;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
        std::cerr << "rtsound: could not initialize audio device" << std::endl;
        g_running.store(false, std::memory_order_relaxed);
        return;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        std::cerr << "rtsound: could not start audio device" << std::endl;
        ma_device_uninit(&device);
        g_running.store(false, std::memory_order_relaxed);
        return;
    }

    while (true) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            cleanup_finished_voices_locked();

            if (g_engine_stop_requested.load(std::memory_order_relaxed)) {
                break;
            }

            if (g_voices.empty()) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ma_device_stop(&device);
    ma_device_uninit(&device);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        clear_all_voices_locked();
        g_sample_rate = 0;
        g_channels = 0;
    }

    g_engine_stop_requested.store(false, std::memory_order_relaxed);
    g_running.store(false, std::memory_order_relaxed);
}

// ── Builtins ───────────────────────────────────────────────────────────────

static inline Value fn_play_async(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1 && args.size() != 2) {
        throw Error{interp.filename, interp.cur_line(),
                    "play_async: expected 1 or 2 arguments", {}};
    }

    const uint32_t sample_rate =
        (args.size() == 2)
        ? parse_play_sample_rate(args[1], interp, "play_async")
        : k_default_sample_rate;

    auto buffer = std::make_shared<PlaybackBuffer>(
                      value_to_playback_buffer(args[0], sample_rate, interp, "play_async")
                  );

    if (buffer->channels == 0 || buffer->frames == 0) {
        return NumVal{0.0};
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_running.load(std::memory_order_relaxed)) {
        join_thread_if_needed_locked();

        g_sample_rate = buffer->sample_rate;
        g_channels    = buffer->channels;
        g_engine_stop_requested.store(false, std::memory_order_relaxed);
        g_voices.clear();

        auto voice = std::make_shared<Voice>();
        voice->buffer = buffer;
        voice->cursor_frames.store(0, std::memory_order_relaxed);
        voice->finished.store(false, std::memory_order_relaxed);
        g_voices.push_back(voice);

        g_running.store(true, std::memory_order_relaxed);
        g_thread = std::thread(thread_main, g_sample_rate, g_channels);
        return NumVal{1.0};
    }

    if (buffer->sample_rate != g_sample_rate) {
        throw Error{interp.filename, interp.cur_line(),
                    "play_async: sample rate must match currently running DAC", {}};
    }

    if (buffer->channels != g_channels) {
        throw Error{interp.filename, interp.cur_line(),
                    "play_async: channel count must match currently running DAC", {}};
    }

    auto voice = std::make_shared<Voice>();
    voice->buffer = buffer;
    voice->cursor_frames.store(0, std::memory_order_relaxed);
    voice->finished.store(false, std::memory_order_relaxed);
    g_voices.push_back(voice);

    return NumVal{1.0};
}

static inline Value fn_play(std::vector<Value>& args, Interpreter& interp) {
    Value r = fn_play_async(args, interp);
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    join_thread_if_needed_locked();
    return r;
}

static inline Value fn_dacstop(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) {
        throw Error{interp.filename, interp.cur_line(),
                    "dacstop: expected 0 arguments", {}};
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_engine_stop_requested.store(true, std::memory_order_relaxed);
        clear_all_voices_locked();
    }

    join_thread_if_needed_locked();
    g_running.store(false, std::memory_order_relaxed);
    return NumVal{1.0};
}

static inline Value fn_dacrunning(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) {
        throw Error{interp.filename, interp.cur_line(),
                    "dacrunning: expected 0 arguments", {}};
    }

    return NumVal{g_running.load(std::memory_order_relaxed) ? 1.0 : 0.0};
}

static inline Value fn_dacinfo(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) {
        throw Error{interp.filename, interp.cur_line(),
                    "dacinfo: expected 0 arguments", {}};
    }

    std::ostringstream ss;

    std::lock_guard<std::mutex> lock(g_mutex);

    cleanup_finished_voices_locked();

    ss << "dacrunning: "
       << (g_running.load(std::memory_order_relaxed) ? "yes" : "no") << "\n";

    if (g_running.load(std::memory_order_relaxed)) {
        ss << "sample_rate: " << g_sample_rate << "\n";
        ss << "channels: "    << g_channels    << "\n";
        ss << "voices: "      << g_voices.size() << "\n";
        ss << "stop_requested: "
           << (g_engine_stop_requested.load(std::memory_order_relaxed) ? "yes" : "no") << "\n";

        for (std::size_t i = 0; i < g_voices.size(); ++i) {
            const auto& v = g_voices[i];
            if (!v || !v->buffer) continue;

            ss << "voice[" << i << "]: frames="
               << v->buffer->frames
               << " cursor="
               << v->cursor_frames.load(std::memory_order_relaxed)
               << " channels="
               << v->buffer->channels
               << "\n";
        }
    } else {
        ss << "sample_rate: n/a\n";
        ss << "channels: n/a\n";
        ss << "voices: 0\n";
        ss << "stop_requested: no\n";
    }

    return ss.str();
}

#else

static inline Value fn_play(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1 && args.size() != 2) {
        throw Error{interp.filename, interp.cur_line(),
                    "play: expected 1 or 2 arguments", {}};
    }
    std::cout << "play: realtime sound system has not been enabled" << std::endl;
    return NumVal{0.0};
}

static inline Value fn_play_async(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1 && args.size() != 2) {
        throw Error{interp.filename, interp.cur_line(),
                    "play_async: expected 1 or 2 arguments", {}};
    }
    std::cout << "play_async: realtime sound system has not been enabled" << std::endl;
    return NumVal{0.0};
}

static inline Value fn_dacstop(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) {
        throw Error{interp.filename, interp.cur_line(),
                    "dacstop: expected 0 arguments", {}};
    }
    std::cout << "dacstop: realtime sound system has not been enabled" << std::endl;
    return NumVal{0.0};
}

static inline Value fn_dacrunning(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) {
        throw Error{interp.filename, interp.cur_line(),
                    "dacrunning: expected 0 arguments", {}};
    }
    return NumVal{0.0};
}

static inline Value fn_dacinfo(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) {
        throw Error{interp.filename, interp.cur_line(),
                    "dacinfo: expected 0 arguments", {}};
    }
    return std::string("realtime sound system has not been enabled");
}

#endif

// ── Registration ─────────────────────────────────────────────────────────────

inline void add_rtsound(Environment& env) {
    env.register_builtin("play",       fn_play);
    env.register_builtin("play_async", fn_play_async);
    env.register_builtin("dacstop",    fn_dacstop);
    env.register_builtin("dacrunning", fn_dacrunning);
    env.register_builtin("dacinfo",    fn_dacinfo);
}

#endif // RTSOUND_H