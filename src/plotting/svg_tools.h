// svg_tools.h
//
// Minimal SVG backend for plotting.
// - Uses title-based filenames with sanitization and unique suffix:
//     "Line demo" -> "Line_demo.svg"
//     ""          -> "plot.svg"
//     "plot" (exists) -> "plot_1.svg", "plot_2.svg", ...
//
// Typical use from plotting.h:
//   using T = Real;
//   std::vector<svg_tools::Series<T>> series;
//   svg_tools::save_svg_plot<T>(title, series, style, scatter_mode);

#ifndef SVG_TOOLS_H
#define SVG_TOOLS_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <limits>
#include <iomanip>
#include <cmath>


namespace fs = std::filesystem;

namespace svg_tools {

template <typename T>
struct Series {
    std::vector<T> x;
    std::vector<T> y;
    std::string    legend;
};

template <typename T>
struct AxisRange {
    T min_x;
    T max_x;
    T min_y;
    T max_y;
};

inline const std::vector<std::string>& svg_palette() {
    static std::vector<std::string> palette = {
        "#e41a1c", "#377eb8", "#4daf4a", "#984ea3",
        "#ff7f00", "#ffff33", "#a65628", "#f781bf", "#999999"
    };
    return palette;
}

// ------------------------------------------------------------
// Filename utilities
// ------------------------------------------------------------

static std::string get_home_directory() {
    std::string home;
#ifdef _WIN32
    if (const char* up = std::getenv("USERPROFILE")) {
        home = up;
    } else {
        const char* hd = std::getenv("HOMEDRIVE");
        const char* hp = std::getenv("HOMEPATH");
        if (hd && hp) home = std::string(hd) + hp;
    }
#else // POSIX
    if (const char* h = std::getenv("HOME")) home = h;
#endif
    if (home.empty()) home = ".";
    return home;
}

inline bool file_exists(const std::string& path) {
    std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);
    return f.good();
}

// Turn an arbitrary title into a safe base name.
// - Replace spaces with '_'
// - Replace illegal/suspicious chars with '_'
// - If result is empty, return "plot"
inline std::string sanitize_filename(const std::string& title) {
    if (title.empty()) {
        return "plot";
    }

    std::string out;
    out.reserve(title.size());

    for (char c : title) {
        // allowed: letters, digits, '.', '-', '_'
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_') {
            out.push_back(c);
        } else if (c == ' ') {
            out.push_back('_');
        } else {
            out.push_back('_');
        }
    }

    // avoid empty or all underscores
    bool all_underscores = !out.empty();
    for (char c : out) {
        if (c != '_') {
            all_underscores = false;
            break;
        }
    }
    if (out.empty() || all_underscores) {
        return "plot";
    }
    return out;
}

// base + ext, with auto-increment suffix if needed.
// E.g. base="plot", ext=".svg":
//   "plot.svg", "plot_1.svg", "plot_2.svg", ...
inline std::string make_unique_filename(const std::string& base,
                                        const std::string& ext)
{
    std::string candidate = base + ext;
    if (!file_exists(candidate)) {
        return candidate;
    }

    int idx = 1;
    while (true) {
        std::ostringstream oss;
        oss << base << "_" << idx << ext;
        candidate = oss.str();
        if (!file_exists(candidate)) {
            return candidate;
        }
        ++idx;
    }
}

// ------------------------------------------------------------
// Range computation
// ------------------------------------------------------------

template <typename T>
AxisRange<T> compute_range(const std::vector<Series<T>>& series) {
    AxisRange<T> r;
    r.min_x =  std::numeric_limits<T>::infinity();
    r.max_x = -std::numeric_limits<T>::infinity();
    r.min_y =  std::numeric_limits<T>::infinity();
    r.max_y = -std::numeric_limits<T>::infinity();

    for (const auto& s : series) {
        for (std::size_t i = 0; i < s.x.size(); ++i) {
            T x = s.x[i];
            T y = s.y[i];
            if (x < r.min_x) r.min_x = x;
            if (x > r.max_x) r.max_x = x;
            if (y < r.min_y) r.min_y = y;
            if (y > r.max_y) r.max_y = y;
        }
    }

    auto finite_or_default = [](T& minv, T& maxv) {
        double dmin = static_cast<double>(minv);
        double dmax = static_cast<double>(maxv);
        if (!std::isfinite(dmin) || !std::isfinite(dmax)) {
            minv = static_cast<T>(0);
            maxv = static_cast<T>(1);
        }
    };

    finite_or_default(r.min_x, r.max_x);
    finite_or_default(r.min_y, r.max_y);

    if (r.min_x == r.max_x) {
        r.min_x -= static_cast<T>(0.5);
        r.max_x += static_cast<T>(0.5);
    }
    if (r.min_y == r.max_y) {
        r.min_y -= static_cast<T>(0.5);
        r.max_y += static_cast<T>(0.5);
    }
    return r;
}

// ------------------------------------------------------------
// Main entry point
// ------------------------------------------------------------

// style:
//   '*' : line + markers
//   '.' : markers only
//   '-' : line only
// scatter_mode:
//   false -> “function style” plot
//   true  -> scatter semantics (slightly larger markers)
//
// Returns the filename (empty string on failure).
template <typename T>
std::string save_svg_plot(const std::string& title,
                          const std::vector<Series<T>>& series,
                          char style,
                          bool scatter_mode)
{
    const int width  = 800;
    const int height = 600;
    const int margin_left   = 70;
    const int margin_right  = 40;
    const int margin_top    = 50;
    const int margin_bottom = 60;

    AxisRange<T> r = compute_range(series);

    auto fx = [&](T x) -> double {
        double dx = static_cast<double>(x);
        double dmin_x = static_cast<double>(r.min_x);
        double dmax_x = static_cast<double>(r.max_x);
        double t = (dx - dmin_x) / (dmax_x - dmin_x);
        return margin_left + t * (width - margin_left - margin_right);
    };
    auto fy = [&](T y) -> double {
        double dy = static_cast<double>(y);
        double dmin_y = static_cast<double>(r.min_y);
        double dmax_y = static_cast<double>(r.max_y);
        double t = (dy - dmin_y) / (dmax_y - dmin_y);
        // SVG y grows downward
        return margin_top + (1.0 - t) * (height - margin_top - margin_bottom);
    };

    // Title-based filename (sanitized + unique, .svg)
    std::string base = sanitize_filename(title);
    std::string local_name = make_unique_filename(base, ".svg");

    // Get current working dir, with a safe fallback.
    fs::path cwd;
    try {
        cwd = fs::current_path();
    } catch (...) {
        cwd = fs::path(get_home_directory());
    }

    // Decide the base directory:
    // - if cwd is a root path ( "/" or "C:\\" etc.), use home
    // - otherwise, use cwd as-is
    fs::path home = fs::path(get_home_directory());
    fs::path base_dir = (cwd == cwd.root_path()) ? home : cwd;

    // Compose final path
    fs::path fullpath = base_dir / local_name;
    std::string filename = fullpath.string();    

    std::cout << base << " " << local_name << " " << filename << std::endl;
    std::ofstream out(filename.c_str());
    if (!out) {
        return std::string();
    }

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
           "width=\"" << width << "\" height=\"" << height
        << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";

    // Background
    out << "  <rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\""
        << height << "\" fill=\"white\" />\n";

    // Title
    if (!title.empty()) {
        out << "  <text x=\"" << (width / 2)
            << "\" y=\"" << (margin_top / 2)
            << "\" text-anchor=\"middle\" "
               "font-family=\"sans-serif\" font-size=\"18\">"
            << title << "</text>\n";
    }

    double x0 = fx(r.min_x);
    double x1 = fx(r.max_x);
    double y0 = fy(r.min_y);
    double y1 = fy(r.max_y);

    // Axes along x=0 and y=0 (very simple; could be refined later)
    out << "  <line x1=\"" << x0 << "\" y1=\"" << fy(static_cast<T>(0))
        << "\" x2=\"" << x1 << "\" y2=\"" << fy(static_cast<T>(0))
        << "\" stroke=\"#000\" stroke-width=\"1\" />\n";

    out << "  <line x1=\"" << fx(static_cast<T>(0)) << "\" y1=\"" << y0
        << "\" x2=\"" << fx(static_cast<T>(0)) << "\" y2=\"" << y1
        << "\" stroke=\"#000\" stroke-width=\"1\" />\n";

    // Tick marks (5 per axis)
    int nticks = 5;
    for (int i = 0; i <= nticks; ++i) {
        double tx = static_cast<double>(r.min_x) +
                    (static_cast<double>(r.max_x) - static_cast<double>(r.min_x)) * i / nticks;
        double ty = static_cast<double>(r.min_y) +
                    (static_cast<double>(r.max_y) - static_cast<double>(r.min_y)) * i / nticks;

        double px = fx(static_cast<T>(tx));
        double py = fy(static_cast<T>(ty));

        // X ticks
        out << "  <line x1=\"" << px << "\" y1=\"" << fy(static_cast<T>(0)) - 4
            << "\" x2=\"" << px << "\" y2=\"" << fy(static_cast<T>(0)) + 4
            << "\" stroke=\"#000\" stroke-width=\"1\" />\n";

        out << "  <text x=\"" << px << "\" y=\"" << fy(static_cast<T>(0)) + 18
            << "\" text-anchor=\"middle\" "
               "font-family=\"sans-serif\" font-size=\"10\">"
            << std::fixed << std::setprecision(2) << tx << "</text>\n";

        // Y ticks
        out << "  <line x1=\"" << fx(static_cast<T>(0)) - 4 << "\" y1=\"" << py
            << "\" x2=\"" << fx(static_cast<T>(0)) + 4 << "\" y2=\"" << py
            << "\" stroke=\"#000\" stroke-width=\"1\" />\n";

        out << "  <text x=\"" << fx(static_cast<T>(0)) - 8 << "\" y=\"" << py + 3
            << "\" text-anchor=\"end\" "
               "font-family=\"sans-serif\" font-size=\"10\">"
            << std::fixed << std::setprecision(2) << ty << "</text>\n";
    }

    const auto& palette = svg_palette();

    bool draw_line    = (style == '*' || style == '-');
    bool draw_markers = (style == '*' || style == '.');

    for (std::size_t s = 0; s < series.size(); ++s) {
        const Series<T>& ser = series[s];
        if (ser.x.empty() || ser.y.empty()) continue;

        std::string color = palette[s % palette.size()];

        // Polyline
        if (draw_line && ser.x.size() >= 2) {
            out << "  <polyline fill=\"none\" stroke=\"" << color
                << "\" stroke-width=\"1\" points=\"";
            for (std::size_t i = 0; i < ser.x.size(); ++i) {
                out << fx(ser.x[i]) << "," << fy(ser.y[i]);
                if (i + 1 < ser.x.size()) out << " ";
            }
            out << "\" />\n";
        }

        // Markers
        if (draw_markers) {
            double radius = scatter_mode ? 3.0 : 2.0;
            for (std::size_t i = 0; i < ser.x.size(); ++i) {
                out << "  <circle cx=\"" << fx(ser.x[i])
                    << "\" cy=\"" << fy(ser.y[i])
                    << "\" r=\"" << radius
                    << "\" fill=\"" << color << "\" />\n";
            }
        }
    }

    // Legend
    double legend_x = margin_left + 10;
    double legend_y = margin_top + 10;
    for (std::size_t s = 0; s < series.size(); ++s) {
        const Series<T>& ser = series[s];
        if (ser.legend.empty()) continue;

        std::string color = palette[s % palette.size()];
        out << "  <rect x=\"" << legend_x << "\" y=\"" << legend_y - 10
            << "\" width=\"12\" height=\"12\" "
            << "fill=\"" << color << "\" stroke=\"#000\" stroke-width=\"0.5\" />\n";
        out << "  <text x=\"" << (legend_x + 18) << "\" y=\"" << legend_y
            << "\" font-family=\"sans-serif\" font-size=\"12\">"
            << ser.legend << "</text>\n";
        legend_y += 18;
    }

    out << "</svg>\n";
    out.close();

    return filename;
}

} // namespace svg_tools

#endif // SVG_TOOLS_H

// eof

