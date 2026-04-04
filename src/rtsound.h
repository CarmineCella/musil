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

// ───────────────────────────────────────────────────────────
// Helpers
// ───────────────────────────────────────────────────────────

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
                                              const std::string& fn)
{
    double sr = scalar(v, fn);
    if (sr <= 0.0) {
        throw Error{interp.filename, interp.cur_line(),
                    fn + ": sample rate must be > 0", {}};
    }
    return static_cast<uint32_t>(sr);
}

// ───────────────────────────────────────────────────────────
// Value → buffer
// ───────────────────────────────────────────────────────────

static inline PlaybackBuffer value_to_playback_buffer(const Value& v,
                                                      uint32_t sample_rate,
                                                      Interpreter& interp,
                                                      const std::string& fn)
{
    PlaybackBuffer pb;
    pb.sample_rate = sample_rate;

    if (std::holds_alternative<NumVal>(v)) {
        const auto& nv = std::get<NumVal>(v);
        pb.channels = 1;
        pb.frames   = nv.size();
        pb.interleaved.resize(pb.frames);

        for (size_t i = 0; i < nv.size(); ++i)
            pb.interleaved[i] = clamp_sample(nv[i]);

        return pb;
    }

    if (std::holds_alternative<ArrayPtr>(v)) {
        auto arr = std::get<ArrayPtr>(v);
        if (arr->elems.empty())
            throw Error{interp.filename, interp.cur_line(),
                        fn + ": empty channel array", {}};

        pb.channels = arr->elems.size();

        size_t nframes = 0;
        bool first = true;

        std::vector<const NumVal*> chans;

        for (auto& e : arr->elems) {
            if (!std::holds_alternative<NumVal>(e))
                throw Error{interp.filename, interp.cur_line(),
                            fn + ": expected vectors", {}};

            const auto& nv = std::get<NumVal>(e);

            if (first) {
                nframes = nv.size();
                first = false;
            } else if (nv.size() != nframes) {
                throw Error{interp.filename, interp.cur_line(),
                            fn + ": inconsistent lengths", {}};
            }

            chans.push_back(&nv);
        }

        pb.frames = nframes;
        pb.interleaved.resize(pb.frames * pb.channels);

        for (size_t i = 0; i < nframes; ++i)
            for (size_t ch = 0; ch < chans.size(); ++ch)
                pb.interleaved[i * pb.channels + ch] =
                    clamp_sample((*(chans[ch]))[i]);

        return pb;
    }

    throw Error{interp.filename, interp.cur_line(),
                fn + ": invalid input", {}};
}

// ───────────────────────────────────────────────────────────
// RT AUDIO ENGINE
// ───────────────────────────────────────────────────────────

#ifdef BUILD_MUSIL_RTSOUND

struct Voice {
    std::shared_ptr<PlaybackBuffer> buffer;
    std::atomic<uint64_t> cursor {0};
    std::atomic<bool> finished {false};
};

inline std::mutex g_mutex;
inline std::thread g_thread;
inline std::vector<std::shared_ptr<Voice>> g_voices;

inline std::atomic<bool> g_running {false};
inline std::atomic<bool> g_stop_requested {false};
inline std::atomic<bool> g_device_initialized {false};

inline uint32_t g_sample_rate = 0;
inline uint32_t g_channels = 0;

inline ma_device g_device;

// ───────────────────────────────────────────────────────────
// CALLBACK
// ───────────────────────────────────────────────────────────

static inline void playback_callback(ma_device*, void* out, const void*, ma_uint32 nframes)
{
    float* output = (float*)out;
    size_t nsamp = nframes * g_channels;

    std::fill(output, output + nsamp, 0.0f);

    std::lock_guard<std::mutex> lock(g_mutex);

    for (auto& v : g_voices) {
        if (!v || v->finished) continue;

        auto& buf = *v->buffer;
        uint64_t cursor = v->cursor;

        if (cursor >= buf.frames) {
            v->finished = true;
            continue;
        }

        uint64_t remain = buf.frames - cursor;
        uint64_t to_copy = std::min<uint64_t>(remain, nframes);

        const float* src = buf.interleaved.data() + cursor * buf.channels;

        for (uint64_t i = 0; i < to_copy * buf.channels; ++i)
            output[i] += src[i];

        v->cursor = cursor + to_copy;

        if (v->cursor >= buf.frames)
            v->finished = true;
    }

    for (size_t i = 0; i < nsamp; ++i)
        output[i] = clamp_sample(output[i]);

    g_voices.erase(
        std::remove_if(g_voices.begin(), g_voices.end(),
            [](auto& v){ return !v || v->finished; }),
        g_voices.end());
}

// ───────────────────────────────────────────────────────────
// THREAD
// ───────────────────────────────────────────────────────────

static inline void thread_main(uint32_t sr, uint32_t ch)
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = ch;
    config.sampleRate        = sr;
    config.dataCallback      = playback_callback;

    if (ma_device_init(nullptr, &config, &g_device) != MA_SUCCESS) {
        g_running = false;
        return;
    }

    g_device_initialized = true;

    if (ma_device_start(&g_device) != MA_SUCCESS) {
        ma_device_uninit(&g_device);
        g_device_initialized = false;
        g_running = false;
        return;
    }

    while (true) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_stop_requested || g_voices.empty())
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ma_device_stop(&g_device);
    ma_device_uninit(&g_device);

    g_device_initialized = false;
    g_running = false;
    g_stop_requested = false;
}

// ───────────────────────────────────────────────────────────
// BUILTINS
// ───────────────────────────────────────────────────────────

static inline Value fn_play_async(std::vector<Value>& args, Interpreter& interp)
{
    uint32_t sr = (args.size()==2)
        ? parse_play_sample_rate(args[1], interp, "play_async")
        : k_default_sample_rate;

    auto buf = std::make_shared<PlaybackBuffer>(
        value_to_playback_buffer(args[0], sr, interp, "play_async"));

    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_running) {
        g_sample_rate = buf->sample_rate;
        g_channels    = buf->channels;

        g_running = true;
        g_thread = std::thread(thread_main, g_sample_rate, g_channels);
    }

    auto v = std::make_shared<Voice>();
    v->buffer = buf;
    g_voices.push_back(v);

    return NumVal{1.0};
}

static inline Value fn_dacstop(std::vector<Value>&, Interpreter&)
{
    g_stop_requested = true;
    if (g_thread.joinable()) g_thread.join();
    return NumVal{1.0};
}

static inline Value fn_dacrunning(std::vector<Value>&, Interpreter&)
{
    return NumVal{g_running ? 1.0 : 0.0};
}

static inline Value fn_dacinfo(std::vector<Value>&, Interpreter&)
{
    std::ostringstream ss;

    ss << "running: " << (g_running ? "yes" : "no") << "\n";

    if (g_device_initialized) {
        const char* name =
            (g_device.playback.name[0] != '\0')
                ? g_device.playback.name : "unknown";

        const char* backend =
            (g_device.pContext)
                ? ma_get_backend_name(g_device.pContext->backend)
                : "unknown";

        ss << "device: " << name << "\n";
        ss << "backend: " << backend << "\n";
        ss << "sample_rate: " << g_device.sampleRate << "\n";
        ss << "channels: " << g_device.playback.channels << "\n";
        ss << "period_frames: " << g_device.playback.internalPeriodSizeInFrames << "\n";
    }

    ss << "voices: " << g_voices.size() << "\n";

    return ss.str();
}

#else

static inline Value fn_play_async(...) {
    std::cout << "RT disabled\n";
    return NumVal{0};
}
static inline Value fn_dacstop(...) { return NumVal{0}; }
static inline Value fn_dacrunning(...) { return NumVal{0}; }
static inline Value fn_dacinfo(...) { return std::string("RT disabled"); }

#endif

inline void add_rtsound(Environment& env) {
    env.register_builtin("play_async", fn_play_async);
    env.register_builtin("dacstop", fn_dacstop);
    env.register_builtin("dacrunning", fn_dacrunning);
    env.register_builtin("dacinfo", fn_dacinfo);
}

#endif