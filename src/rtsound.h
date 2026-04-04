#ifndef RTSOUND_H
#define RTSOUND_H

#include "core.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
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
                                              const std::string& fn)
{
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
                                                      const std::string& fn)
{
    PlaybackBuffer pb;
    pb.sample_rate = sample_rate;

    // Case 1: mono vector
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

    // Case 2: array of vectors = multichannel
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

struct PlaybackState {
    const float* data = nullptr;
    uint64_t total_frames = 0;
    uint32_t channels = 0;
    std::atomic<uint64_t> cursor_frames {0};
};

static inline void playback_callback(ma_device* device,
                                     void* output,
                                     const void* input,
                                     ma_uint32 frame_count)
{
    (void)input;

    auto* state = static_cast<PlaybackState*>(device->pUserData);
    float* out  = static_cast<float*>(output);

    if (!state || !out) return;

    const uint64_t cursor = state->cursor_frames.load(std::memory_order_relaxed);
    const uint64_t remain = (cursor < state->total_frames)
        ? (state->total_frames - cursor)
        : 0;
    const uint64_t to_copy = std::min<uint64_t>(remain, frame_count);

    if (to_copy > 0) {
        const float* src = state->data + cursor * state->channels;
        std::memcpy(out, src,
                    static_cast<std::size_t>(to_copy * state->channels) * sizeof(float));
    }

    if (to_copy < frame_count) {
        float* tail = out + to_copy * state->channels;
        const std::size_t tail_samples =
            static_cast<std::size_t>((frame_count - to_copy) * state->channels);
        std::fill(tail, tail + tail_samples, 0.0f);
    }

    state->cursor_frames.store(cursor + to_copy, std::memory_order_relaxed);
}

static inline Value fn_play(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1 && args.size() != 2) {
        throw Error{interp.filename, interp.cur_line(),
                    "play: expected 1 or 2 arguments", {}};
    }

    uint32_t sample_rate = k_default_sample_rate;
    if (args.size() == 2) {
        sample_rate = parse_play_sample_rate(args[1], interp, "play");
    }

    PlaybackBuffer pb = value_to_playback_buffer(args[0], sample_rate, interp, "play");

    if (pb.channels == 0 || pb.frames == 0) {
        return NumVal{0.0};
    }
    std::cout << "play: playing " << pb.frames << " frames at " << pb.sample_rate
              << " Hz with " << pb.channels << " channels" << std::endl;
    PlaybackState state;
    state.data = pb.interleaved.data();
    state.total_frames = pb.frames;
    state.channels = pb.channels;
    state.cursor_frames.store(0, std::memory_order_relaxed);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = pb.channels;
    config.sampleRate        = pb.sample_rate;
    config.dataCallback      = playback_callback;
    config.pUserData         = &state;

    ma_device device;
    ma_result result = ma_device_init(nullptr, &config, &device);
    if (result != MA_SUCCESS) {
        throw Error{interp.filename, interp.cur_line(),
                    "play: could not initialize audio device", {}};
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        throw Error{interp.filename, interp.cur_line(),
                    "play: could not start audio device", {}};
    }

    while (state.cursor_frames.load(std::memory_order_relaxed) < state.total_frames) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ma_device_stop(&device);
    ma_device_uninit(&device);

    return NumVal{1.0};
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

#endif

// ── Registration ─────────────────────────────────────────────────────────────

inline void add_rtsound(Environment& env) {
    env.register_builtin("play", fn_play);
}

#endif // RTSOUND_H