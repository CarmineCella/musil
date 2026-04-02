#ifndef PLOTTING_H
#define PLOTTING_H

#include "core.h"
#include "plotting/svg_tools.h"

#include <vector>
#include <string>
#include <sstream>
#include <filesystem>

static std::string plot_to_string(const Value& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<NumVal>(v)) {
        const NumVal& nv = std::get<NumVal>(v);
        if (nv.size() == 1) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", nv[0]);
            return std::string(buf);
        }
        std::string s = "<";
        for (std::size_t i = 0; i < nv.size(); ++i) {
            if (i) s += " ";
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", nv[i]);
            s += buf;
        }
        s += ">";
        return s;
    }
    if (std::holds_alternative<ProcVal>(v)) return "<proc>";
    return "[array]";
}

static char plot_style_from_value(const Value& v, const std::string& fn) {
    std::string s = plot_to_string(v);
    if (s.empty()) throw Error{"plotting", -1, fn + ": invalid style"};
    char c = s[0];
    if (c != '*' && c != '.' && c != '-')
        throw Error{"plotting", -1, fn + ": style must be '*', '.', or '-'"};
    return c;
}

static const NumVal& plot_vec(const Value& v, const std::string& fn) {
    if (!std::holds_alternative<NumVal>(v))
        throw Error{"plotting", -1, fn + ": expected numeric vector"};
    return std::get<NumVal>(v);
}

static std::string plotting_do_plot(const std::string& title,
                                    const std::vector<svg_tools::Series<double>>& series,
                                    char style,
                                    bool scatter_mode) {
    return svg_tools::save_svg_plot<double>(title, series, style, scatter_mode);
}

// plot(title?, y1, legend1?, y2, legend2?, ..., style) -> filename
static Value fn_plot(std::vector<Value>& args, Interpreter& I) {
    if (args.size() < 2)
        throw Error{I.filename, I.cur_line(), "plot: at least one dataset and a style are required"};

    char style = plot_style_from_value(args.back(), "plot");
    std::size_t last = args.size() - 1;
    std::size_t i = 0;

    std::string title;
    if (i < last && std::holds_alternative<std::string>(args[i])) {
        title = std::get<std::string>(args[i]);
        ++i;
    }

    std::vector<svg_tools::Series<double>> series;
    std::size_t expected_len = 0;
    bool have_expected = false;

    while (i < last) {
        const NumVal& y = plot_vec(args[i], "plot");
        if (y.size() == 0)
            throw Error{I.filename, I.cur_line(), "plot: empty data array"};
        if (!have_expected) {
            expected_len = y.size();
            have_expected = true;
        } else if (y.size() != expected_len) {
            throw Error{I.filename, I.cur_line(), "plot: all data arrays must have the same length"};
        }

        svg_tools::Series<double> ser;
        ser.x.reserve(y.size());
        ser.y.reserve(y.size());
        for (std::size_t k = 0; k < y.size(); ++k) {
            ser.x.push_back((double)k);
            ser.y.push_back(y[k]);
        }
        ++i;
        if (i < last && std::holds_alternative<std::string>(args[i])) {
            ser.legend = std::get<std::string>(args[i]);
            ++i;
        }
        series.push_back(std::move(ser));
    }

    if (series.empty())
        throw Error{I.filename, I.cur_line(), "plot: no data series provided"};

    return plotting_do_plot(title, series, style, false);
}

// scatter(title?, x1, y1, legend1?, x2, y2, legend2?, ..., style) -> filename
static Value fn_scatter(std::vector<Value>& args, Interpreter& I) {
    if (args.size() < 3)
        throw Error{I.filename, I.cur_line(), "scatter: at least one (x,y) dataset and a style are required"};

    char style = plot_style_from_value(args.back(), "scatter");
    std::size_t last = args.size() - 1;
    std::size_t i = 0;

    std::string title;
    if (i < last && std::holds_alternative<std::string>(args[i])) {
        title = std::get<std::string>(args[i]);
        ++i;
    }

    std::vector<svg_tools::Series<double>> series;
    while (i < last) {
        if (i + 1 >= last)
            throw Error{I.filename, I.cur_line(), "scatter: each dataset must provide x and y arrays"};
        const NumVal& x = plot_vec(args[i], "scatter");
        const NumVal& y = plot_vec(args[i + 1], "scatter");
        if (x.size() != y.size())
            throw Error{I.filename, I.cur_line(), "scatter: x and y arrays must have the same length"};
        if (x.size() == 0)
            throw Error{I.filename, I.cur_line(), "scatter: empty (x,y) dataset"};

        svg_tools::Series<double> ser;
        ser.x.reserve(x.size());
        ser.y.reserve(y.size());
        for (std::size_t k = 0; k < x.size(); ++k) {
            ser.x.push_back(x[k]);
            ser.y.push_back(y[k]);
        }
        i += 2;
        if (i < last && std::holds_alternative<std::string>(args[i])) {
            ser.legend = std::get<std::string>(args[i]);
            ++i;
        }
        series.push_back(std::move(ser));
    }

    if (series.empty())
        throw Error{I.filename, I.cur_line(), "scatter: no (x,y) datasets provided"};

    return plotting_do_plot(title, series, style, true);
}

inline void add_plotting(Environment& env) {
    env.register_builtin("plot", fn_plot);
    env.register_builtin("scatter", fn_scatter);
}

#endif // PLOTTING_H
