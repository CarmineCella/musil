#ifndef MUSIL_SYSTEM_LIBRARY_H
#define MUSIL_SYSTEM_LIBRARY_H

#include "core.h"
#include "system/wav_tools.h"
#include "system/csv_tools.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>

// ── Helpers ─────────────────────────────────────────────────────────────────

static const std::string& sref(const Value& v, const std::string& fn) {
    if (!std::holds_alternative<std::string>(v))
        throw Error{"system", -1, fn + ": expected string", {}};
    return std::get<std::string>(v);
}

static ArrayPtr aref(const Value& v, const std::string& fn) {
    if (!std::holds_alternative<ArrayPtr>(v))
        throw Error{"system", -1, fn + ": expected array", {}};
    return std::get<ArrayPtr>(v);
}

static ArrayPtr strings_to_array(const std::vector<std::string>& xs) {
    auto out = std::make_shared<Array>();
    for (const auto& s : xs) out->elems.push_back(s);
    return out;
}

static ArrayPtr csv_table_to_array(const std::vector<std::vector<std::string>>& table) {
    auto rows = std::make_shared<Array>();
    for (const auto& row : table) {
        auto r = std::make_shared<Array>();
        for (const auto& cell : row) {
            char* end = nullptr;
            double x = std::strtod(cell.c_str(), &end);
            if (!cell.empty() && end && *end == '\0') r->elems.push_back(NumVal{x});
            else r->elems.push_back(cell);
        }
        rows->elems.push_back(r);
    }
    return rows;
}

static std::vector<std::vector<std::string>> array_to_csv_table(const ArrayPtr& table) {
    std::vector<std::vector<std::string>> out;
    out.reserve(table->elems.size());
    for (const auto& rowv : table->elems) {
        ArrayPtr row = aref(rowv, "writecsv");
        std::vector<std::string> r;
        r.reserve(row->elems.size());
        for (const auto& cell : row->elems) {
            if (std::holds_alternative<std::string>(cell)) r.push_back(std::get<std::string>(cell));
            else if (std::holds_alternative<NumVal>(cell)) r.push_back(to_str(cell));
            else throw Error{"system", -1, "writecsv: cells must be strings or numeric values", {}};
        }
        out.push_back(std::move(r));
    }
    return out;
}

static Value wav_channels_to_value(uint32_t sr, const std::vector<std::vector<double>>& channels) {
    auto res = std::make_shared<Array>();
    res->elems.push_back(NumVal{static_cast<double>(sr)});
    auto chs = std::make_shared<Array>();
    for (const auto& ch : channels) {
        if (ch.empty()) chs->elems.push_back(NumVal{});
        else chs->elems.push_back(NumVal(ch.data(), ch.size()));
    }
    res->elems.push_back(chs);
    return res;
}

static std::vector<std::vector<double>> value_to_wav_channels(const ArrayPtr& chs) {
    std::vector<std::vector<double>> out;
    out.reserve(chs->elems.size());
    std::size_t n = 0;
    bool first = true;
    for (const auto& v : chs->elems) {
        if (!std::holds_alternative<NumVal>(v))
            throw Error{"system", -1, "writewav: channel list must contain vectors", {}};
        const auto& nv = std::get<NumVal>(v);
        if (first) { n = nv.size(); first = false; }
        if (nv.size() != n)
            throw Error{"system", -1, "writewav: all channels must have same length", {}};
        out.emplace_back(std::begin(nv), std::end(nv));
    }
    if (out.empty())
        throw Error{"system", -1, "writewav: channel list is empty", {}};
    return out;
}

// ── Builtins ────────────────────────────────────────────────────────────────

static Value fn_sleep(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "sleep: expected 1 argument", {}};
    int delay_ms = static_cast<int>(scalar(args[0], "sleep"));
    if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    return NumVal{0.0};
}

static Value fn_clock(std::vector<Value>& args, Interpreter& interp) {
    if (!args.empty()) throw Error{interp.filename, interp.cur_line(), "clock: expected 0 arguments", {}};
    return NumVal{static_cast<double>(std::clock())};
}

static Value fn_dirlist(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "dirlist: expected 1 argument", {}};
    std::string path = sref(args[0], "dirlist");
    DIR* dir = opendir(path.c_str());
    if (!dir) throw Error{interp.filename, interp.cur_line(), "dirlist: cannot open '" + path + "'", {}};
    std::vector<std::string> names;
    while (auto* ent = readdir(dir)) names.push_back(ent->d_name);
    closedir(dir);
    return strings_to_array(names);
}

static Value fn_filestat(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "filestat: expected 1 argument", {}};
    std::string filename = sref(args[0], "filestat");
    struct stat st {};
    auto out = std::make_shared<Array>();
    int r = stat(filename.c_str(), &st);
    out->elems.push_back(NumVal{r == 0 ? 1.0 : 0.0});
    if (r != 0) return out;
    out->elems.push_back(NumVal{static_cast<double>(st.st_size)});
    out->elems.push_back(NumVal{static_cast<double>(st.st_nlink)});
    std::stringstream tt;
    tt << (S_ISDIR(st.st_mode) ? "d" : "-")
       << ((st.st_mode & S_IRUSR) ? "r" : "-")
       << ((st.st_mode & S_IWUSR) ? "w" : "-")
       << ((st.st_mode & S_IXUSR) ? "x" : "-")
       << ((st.st_mode & S_IRGRP) ? "r" : "-")
       << ((st.st_mode & S_IWGRP) ? "w" : "-")
       << ((st.st_mode & S_IXGRP) ? "x" : "-")
       << ((st.st_mode & S_IROTH) ? "r" : "-")
       << ((st.st_mode & S_IWOTH) ? "w" : "-")
       << ((st.st_mode & S_IXOTH) ? "x" : "-");
    out->elems.push_back(tt.str());
    return out;
}

static Value fn_getvar(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "getvar: expected 1 argument", {}};
    const std::string& name = sref(args[0], "getvar");
    const char* c = std::getenv(name.c_str());
    return c ? Value{std::string(c)} : Value{std::string{}};
}

static Value fn_udprecv(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "udprecv: expected 2 arguments", {}};
    std::string host = sref(args[0], "udprecv");
    int port = static_cast<int>(scalar(args[1], "udprecv"));

    sockaddr_in server {};
    char message[4096] = {};
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) return NumVal{0.0};

    server.sin_addr.s_addr = inet_addr(host.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (::bind(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        ::close(sock);
        return NumVal{0.0};
    }

    sockaddr_in client {};
    socklen_t c = sizeof(client);
    if (recvfrom(sock, message, sizeof(message), 0, reinterpret_cast<sockaddr*>(&client), &c) < 0) {
        ::close(sock);
        return NumVal{0.0};
    }

    ::close(sock);
    return std::string(message);
}

static Value fn_udpsend(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 3 && args.size() != 4)
        throw Error{interp.filename, interp.cur_line(), "udpsend: expected 3 or 4 arguments", {}};

    std::string host = sref(args[0], "udpsend");
    int port = static_cast<int>(scalar(args[1], "udpsend"));
    bool is_osc = (args.size() == 4) ? (scalar(args[3], "udpsend") != 0.0) : false;
    std::string payload = to_str(args[2]);

    sockaddr_in server {};
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) return NumVal{0.0};

    server.sin_addr.s_addr = inet_addr(host.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    int res = 0;
    if (is_osc) {
        std::size_t in_sz = payload.size();
        auto align = [](std::size_t n) { return (n + 3) & ~std::size_t(3); };
        std::size_t pad = align(in_sz) - in_sz;
        std::size_t out_sz = in_sz + pad + 4;
        std::vector<char> out(out_sz, '\0');
        std::memcpy(out.data(), payload.data(), in_sz);
        out[in_sz + pad] = ',';
        res = sendto(sock, out.data(), out.size(), 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
    } else {
        res = sendto(sock, payload.c_str(), payload.size(), 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
    }

    ::close(sock);
    return NumVal{res < 0 ? 0.0 : 1.0};
}

static Value fn_readcsv(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "readcsv: expected 1 argument", {}};
    std::ifstream file(sref(args[0], "readcsv"));
    if (!file) throw Error{interp.filename, interp.cur_line(), "readcsv: cannot open file", {}};
    return csv_table_to_array(readCSV(file));
}

static Value fn_writecsv(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 2) throw Error{interp.filename, interp.cur_line(), "writecsv: expected 2 arguments", {}};
    std::ofstream file(sref(args[0], "writecsv"));
    if (!file) throw Error{interp.filename, interp.cur_line(), "writecsv: cannot open file for writing", {}};
    auto table = aref(args[1], "writecsv");
    auto rows = array_to_csv_table(table);
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            file << csv_escape_field(row[i]);
            if (i + 1 < row.size()) file << ',';
        }
        file << '\n';
    }
    return NumVal{0.0};
}

static Value fn_readwav(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 1) throw Error{interp.filename, interp.cur_line(), "readwav: expected 1 argument", {}};
    WAVHeader header{};
    auto channels = read_wav_raw(sref(args[0], "readwav").c_str(), header);
    return wav_channels_to_value(header.sampleRate, channels);
}

static Value fn_writewav(std::vector<Value>& args, Interpreter& interp) {
    if (args.size() != 3) throw Error{interp.filename, interp.cur_line(), "writewav: expected 3 arguments", {}};
    std::string fname = sref(args[0], "writewav");
    double sr = scalar(args[1], "writewav");
    auto chs = aref(args[2], "writewav");
    if (sr <= 0) throw Error{interp.filename, interp.cur_line(), "writewav: sample rate must be > 0", {}};

    WAVHeader header{};
    std::memcpy(header.riff, "RIFF", 4);
    std::memcpy(header.wave, "WAVE", 4);
    std::memcpy(header.fmt,  "fmt ", 4);
    std::memcpy(header.data, "data", 4);
    header.subchunk1Size = 16;
    header.audioFormat   = 1;
    header.numChannels   = static_cast<uint16_t>(chs->elems.size());
    header.sampleRate    = static_cast<uint32_t>(sr);
    header.bitsPerSample = 16;
    header.blockAlign    = static_cast<uint16_t>(header.numChannels * header.bitsPerSample / 8);
    header.byteRate      = header.sampleRate * header.blockAlign;

    auto channels = value_to_wav_channels(chs);
    write_wav_raw(fname.c_str(), channels, header);
    return NumVal{1.0};
}

// ── Registration ─────────────────────────────────────────────────────────────

inline void add_system(Environment& env) {
    env.register_builtin("sleep",    fn_sleep);
    env.register_builtin("clock",    fn_clock);
    env.register_builtin("dirlist",  fn_dirlist);
    env.register_builtin("filestat", fn_filestat);
    env.register_builtin("getvar",   fn_getvar);
    env.register_builtin("udprecv",  fn_udprecv);
    env.register_builtin("udpsend",  fn_udpsend);
    env.register_builtin("readcsv",  fn_readcsv);
    env.register_builtin("writecsv", fn_writecsv);
    env.register_builtin("readwav",  fn_readwav);
    env.register_builtin("writewav", fn_writewav);
}

#endif
