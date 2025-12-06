// plotting.h
//
// Language-level plotting primitives for Musil:
//   - (plot "title" y1 "legend1" y2 "legend2" ... style)
//   - (scatter "title" x1 y1 "legend1" x2 y2 "legend2" ... style)
//
// style is the last argument:
//   "*" : line + markers
//   "." : markers only
//   "-" : line only
//
// Plot files are saved as SVG using svg_tools.h.
//
// The important semantic: BOTH plot and scatter RETURN the filename (string)
// of the generated SVG. They do NOT print anything by themselves.

#ifndef PLOTTING_H
#define PLOTTING_H

#include "core.h"
#include "plotting/svg_tools.h"

#include <vector>
#include <string>
#include <sstream>

// Helper: convert Atom to string via existing pretty-printer.
inline std::string atom_to_string_for_plot(AtomPtr a) {
    std::stringstream ss;
    print(a, ss);
    return ss.str();
}

// Common entry point: returns the SVG filename
inline std::string plotting_do_plot(const std::string& title,
                                    const std::vector<svg_tools::Series<Real>>& series,
                                    char style,
                                    bool scatter_mode)
{
    // Just delegate to svg_tools and pass the filename back.
    // svg_tools::save_svg_plot should:
    //   - save into std::filesystem::current_path()
    //   - return a non-empty filename on success.
    return svg_tools::save_svg_plot<Real>(title, series, style, scatter_mode);
}

// ---------------------------------------------------------------------
// (plot "title" y1 "legend1" y2 "legend2" ... style)
// Returns: string with SVG filename
// ---------------------------------------------------------------------
AtomPtr fn_plot(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 2) {
        error("[plot] at least one dataset and a style are required", node);
    }

    // Last argument: style
    AtomPtr style_atom = node->tail.back();
    std::string style_str = atom_to_string_for_plot(style_atom);
    if (style_str.empty()) {
        error("[plot] invalid style (expected \"*\", \".\", or \"-\")", node);
    }
    char style_ch = style_str[0];
    if (style_ch != '*' && style_ch != '.' && style_ch != '-') {
        error("[plot] style must be \"*\", \".\" or \"-\"", node);
    }

    const std::size_t last_index = node->tail.size() - 1;
    std::size_t i = 0;

    // Optional title
    std::string title;
    if (node->tail[i]->type == STRING && i < last_index) {
        title = atom_to_string_for_plot(node->tail[i]);
        ++i;
    }

    std::vector<svg_tools::Series<Real>> series;
    int expected_len = -1;

    while (i < last_index) {
        // Expect ARRAY of y-values
        AtomPtr y_atom = type_check(node->tail[i], ARRAY);
        std::valarray<Real>& y_arr = y_atom->array;
        int len = static_cast<int>(y_arr.size());
        if (len <= 0) {
            error("[plot] empty data array", node);
        }
        if (expected_len < 0) {
            expected_len = len;
        } else if (len != expected_len) {
            error("[plot] all data arrays must have the same length", node);
        }

        svg_tools::Series<Real> ser;
        ser.x.reserve(static_cast<std::size_t>(len));
        ser.y.reserve(static_cast<std::size_t>(len));
        for (int k = 0; k < len; ++k) {
            ser.x.push_back(static_cast<Real>(k));
            ser.y.push_back(y_arr[static_cast<std::size_t>(k)]);
        }

        ++i;
        // Optional legend STRING, but not the style
        if (i < last_index && node->tail[i]->type == STRING) {
            ser.legend = atom_to_string_for_plot(node->tail[i]);
            ++i;
        }

        series.push_back(ser);
    }

    if (series.empty()) {
        error("[plot] no data series provided", node);
    }

    // Core: do the plot and get the filename
    std::string fname = plotting_do_plot(title, series, style_ch, /*scatter_mode=*/false);

    // Return filename as string atom (language-level contract)
    return make_atom(fname);
}

// ---------------------------------------------------------------------
// (scatter "title" x1 y1 "legend1" x2 y2 "legend2" ... style)
// Returns: string with SVG filename
// ---------------------------------------------------------------------
AtomPtr fn_scatter(AtomPtr node, AtomPtr env) {
    (void)env;

    if (node->tail.size() < 3) {
        error("[scatter] at least one (x,y) dataset and a style are required", node);
    }

    // Last arg: style
    AtomPtr style_atom = node->tail.back();
    std::string style_str = atom_to_string_for_plot(style_atom);
    if (style_str.empty()) {
        error("[scatter] invalid style (expected \"*\", \".\", or \"-\")", node);
    }
    char style_ch = style_str[0];
    if (style_ch != '*' && style_ch != '.' && style_ch != '-') {
        error("[scatter] style must be \"*\", \".\" or \"-\"", node);
    }

    const std::size_t last_index = node->tail.size() - 1;
    std::size_t i = 0;

    // Optional title
    std::string title;
    if (node->tail[i]->type == STRING && i < last_index) {
        title = atom_to_string_for_plot(node->tail[i]);
        ++i;
    }

    std::vector<svg_tools::Series<Real>> series;

    while (i < last_index) {
        if (i + 1 >= last_index) {
            error("[scatter] each dataset must provide x and y arrays", node);
        }

        AtomPtr x_atom = type_check(node->tail[i],     ARRAY);
        AtomPtr y_atom = type_check(node->tail[i + 1], ARRAY);
        std::valarray<Real>& x_arr = x_atom->array;
        std::valarray<Real>& y_arr = y_atom->array;

        if (x_arr.size() != y_arr.size()) {
            error("[scatter] x and y arrays must have the same length", node);
        }
        if (x_arr.size() == 0) {
            error("[scatter] empty (x,y) dataset", node);
        }

        svg_tools::Series<Real> ser;
        ser.x.reserve(x_arr.size());
        ser.y.reserve(y_arr.size());
        for (std::size_t k = 0; k < x_arr.size(); ++k) {
            ser.x.push_back(x_arr[k]);
            ser.y.push_back(y_arr[k]);
        }

        i += 2;
        // Optional legend
        if (i < last_index && node->tail[i]->type == STRING) {
            ser.legend = atom_to_string_for_plot(node->tail[i]);
            ++i;
        }

        series.push_back(ser);
    }

    if (series.empty()) {
        error("[scatter] no (x,y) datasets provided", node);
    }

    std::string fname = plotting_do_plot(title, series, style_ch, /*scatter_mode=*/true);

    // Return filename as string atom
    return make_atom(fname);
}

// Registration helper
inline AtomPtr add_plotting(AtomPtr env) {
    add_op("plot",    fn_plot,    2, env);
    add_op("scatter", fn_scatter, 3, env);
    return env;
}

#endif // PLOTTING_H
// eof
