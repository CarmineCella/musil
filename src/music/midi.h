// midi.h — standard MIDI files read into notes with times in seconds.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Formats 0 and 1; the tempo map (meta 0x51) of every track applies to all; note-on with velocity 0 is a
// note-off; SMPTE divisions are refused. Each note is a record: track, channel, pitch (MIDI), velocity,
// at and dur in seconds, and the track's name when it has one.

#pragma once
#include "../core.h"
#include <fstream>
#include <map>

namespace musil {

struct midi_note { int track, channel, pitch, velocity; double at, dur; };
struct midi_file { int ppq = 480; std::vector<midi_note> notes; std::vector<std::string> track_names; std::vector<std::pair<long, double>> tempos; double seconds = 0; };

namespace midi_detail {
inline uint32_t be32(const unsigned char* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
inline uint16_t be16(const unsigned char* p) { return (uint16_t)((p[0] << 8) | p[1]); }
inline uint32_t varlen(const unsigned char* d, size_t n, size_t& p) { uint32_t v = 0; for (int k = 0; k < 4 && p < n; k++) { unsigned char b = d[p++]; v = (v << 7) | (b & 0x7f); if (!(b & 0x80)) break; } return v; }
}

inline midi_file read_midi(const std::string& path) {
    using namespace midi_detail;
    std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error("cannot open MIDI file " + path);
    std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 14 || std::memcmp(d.data(), "MThd", 4) != 0) throw std::runtime_error("not a MIDI file: " + path);
    midi_file m; int format = be16(d.data() + 8); int ntracks = be16(d.data() + 10); int division = be16(d.data() + 12);
    if (division & 0x8000) throw std::runtime_error("SMPTE time division is not supported"); m.ppq = division > 0 ? division : 480;
    (void)format;
    struct raw { long tick; int track, channel, pitch, velocity; bool on; }; std::vector<raw> events;
    size_t p = 8 + be32(d.data() + 4); int track = 0;
    while (p + 8 <= d.size() && track < ntracks) {
        if (std::memcmp(d.data() + p, "MTrk", 4) != 0) throw std::runtime_error("bad track chunk");
        size_t len = be32(d.data() + p + 4), end = std::min(d.size(), p + 8 + len); p += 8; long tick = 0; unsigned char status = 0; std::string name;
        while (p < end) {
            tick += varlen(d.data(), end, p); if (p >= end) break;
            unsigned char b = d[p];
            if (b == 0xff) {                                   // meta
                p++; unsigned char type = d[p++]; uint32_t l = varlen(d.data(), end, p);
                if (type == 0x51 && l == 3) m.tempos.push_back({ tick, (double)((d[p] << 16) | (d[p + 1] << 8) | d[p + 2]) });
                if (type == 0x03 && name.empty()) name.assign((const char*)d.data() + p, l);
                p += l; continue;
            }
            if (b == 0xf0 || b == 0xf7) { p++; uint32_t l = varlen(d.data(), end, p); p += l; continue; }
            if (b & 0x80) { status = b; p++; }
            int kind = status & 0xf0, ch = status & 0x0f;
            if (kind == 0x90 || kind == 0x80) { int pitch = d[p], vel = d[p + 1]; p += 2; events.push_back({ tick, track, ch, pitch, vel, kind == 0x90 && vel > 0 }); }
            else if (kind == 0xa0 || kind == 0xb0 || kind == 0xe0) p += 2;
            else if (kind == 0xc0 || kind == 0xd0) p += 1;
            else break;
        }
        m.track_names.push_back(name); p = end; track++;
    }
    // the tempo map: microseconds per quarter, from tick 0 (500000 = 120 bpm by default)
    std::sort(m.tempos.begin(), m.tempos.end()); if (m.tempos.empty() || m.tempos[0].first > 0) m.tempos.insert(m.tempos.begin(), { 0, 500000.0 });
    auto seconds_at = [&](long tick) { double s = 0; for (size_t k = 0; k < m.tempos.size(); k++) { long t0 = m.tempos[k].first, t1 = k + 1 < m.tempos.size() ? m.tempos[k + 1].first : tick; if (tick <= t0) break; long span = std::min(tick, t1) - t0; s += span * m.tempos[k].second / 1e6 / m.ppq; if (tick <= t1) break; } return s; };
    std::stable_sort(events.begin(), events.end(), [](const raw& a, const raw& b) { return a.tick < b.tick; });
    std::map<int, raw> open;                                  // key: track * 4096 + channel * 128 + pitch
    for (auto& e : events) {
        int key = e.track * 4096 + e.channel * 128 + e.pitch;
        if (e.on) { open[key] = e; }
        else { auto it = open.find(key); if (it != open.end()) { midi_note n{ e.track, e.channel, e.pitch, it->second.velocity, seconds_at(it->second.tick), std::max(0.0, seconds_at(e.tick) - seconds_at(it->second.tick)) }; m.notes.push_back(n); m.seconds = std::max(m.seconds, n.at + n.dur); open.erase(it); } }
    }
    std::stable_sort(m.notes.begin(), m.notes.end(), [](const midi_note& a, const midi_note& b) { return a.at < b.at; });
    return m;
}

// (midi-read path) => a record: ppq, seconds, tracks (names), and notes (records: track channel pitch velocity at dur),
//   times in seconds after the file's tempo map
inline vptr mus_midi_read(vlist& a, Interp& i) {
    midi_file m; try { m = read_midi(i.read_path(i.str(a[0]))); } catch (std::exception& e) { i.bad(std::string("midi-read: ") + e.what()); }
    vlist notes; for (auto& n : m.notes) notes.push_back(v_list({ v_list({ v_sym("track"), v_num(n.track) }), v_list({ v_sym("channel"), v_num(n.channel) }), v_list({ v_sym("pitch"), v_num(n.pitch) }), v_list({ v_sym("velocity"), v_num(n.velocity) }), v_list({ v_sym("at"), v_num(n.at) }), v_list({ v_sym("dur"), v_num(n.dur) }) }));
    vlist names; for (auto& s : m.track_names) names.push_back(v_str(s));
    return v_list({ v_list({ v_sym("ppq"), v_num(m.ppq) }), v_list({ v_sym("seconds"), v_num(m.seconds) }), v_list({ v_sym("tracks"), v_list(std::move(names)) }), v_list({ v_sym("notes"), v_list(std::move(notes)) }) });
}

inline void add_midi(Interp& i) { i.def("midi-read", mus_midi_read, 1, 1); }

} // namespace musil
