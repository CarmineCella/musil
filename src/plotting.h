#ifndef PLOTTING_H
#define PLOTTING_H

#include "core.h"
#include "plotting/svg_tools.h"

#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <memory>

#ifdef BUILD_MUSIL_IDE
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#endif

namespace fs = std::filesystem;

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

#ifdef BUILD_MUSIL_IDE

namespace musil_plot_ide {

struct PlotRequest {
    std::string title;
    std::vector<svg_tools::Series<double>> series;
    char style = '-';
    bool scatter_mode = false;
};

static inline Fl_Color series_color(int i) {
    static Fl_Color colors[] = {
        FL_RED, FL_BLUE, FL_DARK_GREEN, FL_DARK_MAGENTA,
        FL_DARK_CYAN, FL_DARK_YELLOW, FL_BLACK
    };
    return colors[i % (sizeof(colors) / sizeof(colors[0]))];
}

class PlotCanvas :
    public Fl_Widget {
public:
    PlotCanvas(int X, int Y, int W, int H, const PlotRequest& req)
        : Fl_Widget(X, Y, W, H), m_req(req) {
        reset_view();
    }

    void reset_view() {
        compute_bounds();
        if (m_xmax <= m_xmin) {
            m_xmin = 0.0;
            m_xmax = 1.0;
        }
        if (m_ymax <= m_ymin) {
            m_ymin = 0.0;
            m_ymax = 1.0;
        }

        const double dx = m_xmax - m_xmin;
        const double dy = m_ymax - m_ymin;

        m_view_xmin = m_xmin - 0.05 * dx;
        m_view_xmax = m_xmax + 0.05 * dx;
        m_view_ymin = m_ymin - 0.05 * dy;
        m_view_ymax = m_ymax + 0.05 * dy;

        if (m_view_xmax <= m_view_xmin) {
            m_view_xmin = 0.0;
            m_view_xmax = 1.0;
        }
        if (m_view_ymax <= m_view_ymin) {
            m_view_ymin = 0.0;
            m_view_ymax = 1.0;
        }

        redraw();
    }

    void zoom(double factor) {
        if (factor <= 0.0) return;

        const double cx = 0.5 * (m_view_xmin + m_view_xmax);
        const double cy = 0.5 * (m_view_ymin + m_view_ymax);
        const double hx = 0.5 * (m_view_xmax - m_view_xmin) / factor;
        const double hy = 0.5 * (m_view_ymax - m_view_ymin) / factor;

        m_view_xmin = cx - hx;
        m_view_xmax = cx + hx;
        m_view_ymin = cy - hy;
        m_view_ymax = cy + hy;
        redraw();
    }

    void zoom_at(double factor, int mouse_x, int mouse_y) {
        if (factor <= 0.0) return;

        const int pl = plot_left();
        const int pr = plot_right();
        const int pt = plot_top();
        const int pb = plot_bottom();

        if (pr <= pl || pb <= pt) return;

        int mx = mouse_x;
        int my = mouse_y;

        if (mx < pl) mx = pl;
        if (mx > pr) mx = pr;
        if (my < pt) my = pt;
        if (my > pb) my = pb;

        const double tx = double(mx - pl) / double(pr - pl);
        const double ty = double(pb - my) / double(pb - pt);

        const double anchor_x = m_view_xmin + tx * (m_view_xmax - m_view_xmin);
        const double anchor_y = m_view_ymin + ty * (m_view_ymax - m_view_ymin);

        const double new_w = (m_view_xmax - m_view_xmin) / factor;
        const double new_h = (m_view_ymax - m_view_ymin) / factor;

        m_view_xmin = anchor_x - tx * new_w;
        m_view_xmax = m_view_xmin + new_w;

        m_view_ymin = anchor_y - ty * new_h;
        m_view_ymax = m_view_ymin + new_h;

        redraw();
    }

    int handle(int ev) override {
        switch (ev) {
        case FL_PUSH:
            if (Fl::event_button() == FL_LEFT_MOUSE) {
                m_dragging = true;
                m_drag_x = Fl::event_x();
                m_drag_y = Fl::event_y();
                return 1;
            }
            return 0;

        case FL_DRAG:
            if (m_dragging) {
                const int dx_pix = Fl::event_x() - m_drag_x;
                const int dy_pix = Fl::event_y() - m_drag_y;
                m_drag_x = Fl::event_x();
                m_drag_y = Fl::event_y();

                const int pl = plot_left();
                const int pr = plot_right();
                const int pt = plot_top();
                const int pb = plot_bottom();
                const int pw = std::max(1, pr - pl);
                const int ph = std::max(1, pb - pt);

                const double dx = (double)dx_pix / (double)pw * (m_view_xmax - m_view_xmin);
                const double dy = (double)dy_pix / (double)ph * (m_view_ymax - m_view_ymin);

                m_view_xmin -= dx;
                m_view_xmax -= dx;
                m_view_ymin += dy;
                m_view_ymax += dy;
                redraw();
                return 1;
            }
            return 0;

        case FL_RELEASE:
            m_dragging = false;
            return 1;

        case FL_MOUSEWHEEL: {
            const int dy = Fl::event_dy();
            if (dy < 0) zoom_at(1.2, Fl::event_x(), Fl::event_y());
            else if (dy > 0) zoom_at(1.0 / 1.2, Fl::event_x(), Fl::event_y());
            return 1;
        }

        default:
            return Fl_Widget::handle(ev);
        }
    }

    void draw() override {
        fl_push_clip(x(), y(), w(), h());

        fl_color(FL_WHITE);
        fl_rectf(x(), y(), w(), h());

        draw_title();
        draw_plot_area();
        draw_axes();
        draw_series();
        draw_legend();

        fl_color(FL_LIGHT2);
        fl_rect(x(), y(), w(), h());

        fl_pop_clip();
    }

private:
    PlotRequest m_req;

    double m_xmin = 0.0, m_xmax = 1.0;
    double m_ymin = 0.0, m_ymax = 1.0;

    double m_view_xmin = 0.0, m_view_xmax = 1.0;
    double m_view_ymin = 0.0, m_view_ymax = 1.0;

    bool m_dragging = false;
    int  m_drag_x = 0;
    int  m_drag_y = 0;

    int plot_left()   const { return x() + 60; }
    int plot_right()  const { return x() + w() - 20; }
    int plot_top()    const { return y() + 45; }
    int plot_bottom() const { return y() + h() - 45; }

    void compute_bounds() {
        bool first = true;
        for (const auto& s : m_req.series) {
            const std::size_t n = std::min(s.x.size(), s.y.size());
            for (std::size_t i = 0; i < n; ++i) {
                const double px = s.x[i];
                const double py = s.y[i];
                if (!std::isfinite(px) || !std::isfinite(py)) continue;
                if (first) {
                    m_xmin = m_xmax = px;
                    m_ymin = m_ymax = py;
                    first = false;
                } else {
                    m_xmin = std::min(m_xmin, px);
                    m_xmax = std::max(m_xmax, px);
                    m_ymin = std::min(m_ymin, py);
                    m_ymax = std::max(m_ymax, py);
                }
            }
        }
        if (first) {
            m_xmin = 0.0;
            m_xmax = 1.0;
            m_ymin = 0.0;
            m_ymax = 1.0;
        }
    }

    int sx(double vx) const {
        const int pl = plot_left();
        const int pr = plot_right();
        const double t = (vx - m_view_xmin) / (m_view_xmax - m_view_xmin);
        return pl + (int)std::lround(t * (pr - pl));
    }

    int sy(double vy) const {
        const int pt = plot_top();
        const int pb = plot_bottom();
        const double t = (vy - m_view_ymin) / (m_view_ymax - m_view_ymin);
        return pb - (int)std::lround(t * (pb - pt));
    }

    bool point_visible(double vx, double vy) const {
        return vx >= m_view_xmin && vx <= m_view_xmax &&
        vy >= m_view_ymin && vy <= m_view_ymax;
    }

    void draw_title() {
        fl_color(FL_BLACK);
        fl_font(FL_HELVETICA_BOLD, 14);
        std::string t = m_req.title.empty() ? "Plot" : m_req.title;
        fl_draw(t.c_str(), x() + 10, y() + 20);
    }

    void draw_plot_area() {
        const int pl = plot_left();
        const int pr = plot_right();
        const int pt = plot_top();
        const int pb = plot_bottom();

        fl_color(fl_rgb_color(250, 250, 250));
        fl_rectf(pl, pt, pr - pl, pb - pt);

        fl_color(FL_LIGHT2);
        for (int i = 1; i < 10; ++i) {
            int gx = pl + i * (pr - pl) / 10;
            int gy = pt + i * (pb - pt) / 10;
            fl_line(gx, pt, gx, pb);
            fl_line(pl, gy, pr, gy);
        }

        fl_color(FL_BLACK);
        fl_rect(pl, pt, pr - pl, pb - pt);
    }

    void draw_axes() {
        const int pl = plot_left();
        const int pr = plot_right();
        const int pt = plot_top();
        const int pb = plot_bottom();

        if (0.0 >= m_view_xmin && 0.0 <= m_view_xmax) {
            int x0 = sx(0.0);
            fl_color(FL_DARK3);
            fl_line(x0, pt, x0, pb);
        }

        if (0.0 >= m_view_ymin && 0.0 <= m_view_ymax) {
            int y0 = sy(0.0);
            fl_color(FL_DARK3);
            fl_line(pl, y0, pr, y0);
        }

        fl_font(FL_HELVETICA, 11);
        fl_color(FL_DARK3);

        char buf[64];

        std::snprintf(buf, sizeof(buf), "%.4g", m_view_xmin);
        fl_draw(buf, pl - 10, pb + 18);

        std::snprintf(buf, sizeof(buf), "%.4g", m_view_xmax);
        fl_draw(buf, pr - 35, pb + 18);

        std::snprintf(buf, sizeof(buf), "%.4g", m_view_ymin);
        fl_draw(buf, pl - 50, pb + 4);

        std::snprintf(buf, sizeof(buf), "%.4g", m_view_ymax);
        fl_draw(buf, pl - 50, pt + 4);
    }

    void draw_series() {
        for (std::size_t si = 0; si < m_req.series.size(); ++si) {
            const auto& s = m_req.series[si];
            fl_color(series_color((int)si));

            const std::size_t n = std::min(s.x.size(), s.y.size());
            if (n == 0) continue;

            if (m_req.style == '-' && !m_req.scatter_mode) {
                bool have_prev = false;
                int prevx = 0, prevy = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    const double px = s.x[i];
                    const double py = s.y[i];
                    if (!std::isfinite(px) || !std::isfinite(py)) {
                        have_prev = false;
                        continue;
                    }
                    const int cx = sx(px);
                    const int cy = sy(py);
                    if (have_prev) fl_line(prevx, prevy, cx, cy);
                    prevx = cx;
                    prevy = cy;
                    have_prev = true;
                }
            }

            for (std::size_t i = 0; i < n; ++i) {
                const double px = s.x[i];
                const double py = s.y[i];
                if (!std::isfinite(px) || !std::isfinite(py)) continue;
                if (!point_visible(px, py)) continue;

                const int cx = sx(px);
                const int cy = sy(py);

                if (m_req.scatter_mode || m_req.style == '.' || m_req.style == '*') {
                    const int r = (m_req.style == '*') ? 3 : 2;
                    fl_rectf(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
                }
            }
        }
    }

    void draw_legend() {
        int lx = plot_right() - 140;
        int ly = plot_top() + 8;
        int row_h = 16;

        bool any = false;
        for (const auto& s : m_req.series) {
            if (!s.legend.empty()) {
                any = true;
                break;
            }
        }
        if (!any) return;

        int visible_rows = 0;
        for (const auto& s : m_req.series) {
            if (!s.legend.empty()) ++visible_rows;
        }

        int box_h = visible_rows * row_h + 10;
        fl_color(fl_rgb_color(255, 255, 255));
        fl_rectf(lx, ly, 130, box_h);
        fl_color(FL_LIGHT2);
        fl_rect(lx, ly, 130, box_h);

        fl_font(FL_HELVETICA, 11);

        int row = 0;
        for (std::size_t i = 0; i < m_req.series.size(); ++i) {
            if (m_req.series[i].legend.empty()) continue;
            const int yy = ly + 16 + row * row_h;
            fl_color(series_color((int)i));
            fl_rectf(lx + 8, yy - 8, 10, 10);
            fl_color(FL_BLACK);
            fl_draw(m_req.series[i].legend.c_str(), lx + 24, yy);
            ++row;
        }
    }
};

struct PlotWindow {
    Fl_Double_Window* win = nullptr;
    PlotCanvas* canvas = nullptr;
    Fl_Button* btn_zoom_in = nullptr;
    Fl_Button* btn_zoom_out = nullptr;
    Fl_Button* btn_reset = nullptr;
    Fl_Button* btn_save = nullptr;
    Fl_Button* btn_close = nullptr;
    PlotRequest req;
};

static void plot_close_cb(Fl_Widget*, void* userdata) {
    PlotWindow* pw = static_cast<PlotWindow*>(userdata);
    if (!pw) return;
    if (pw->win) {
        pw->win->hide();
        delete pw->win;
        pw->win = nullptr;
    }
    delete pw;
}

static void plot_zoom_in_cb(Fl_Widget*, void* userdata) {
    PlotWindow* pw = static_cast<PlotWindow*>(userdata);
    if (pw && pw->canvas) pw->canvas->zoom(1.25);
}

static void plot_zoom_out_cb(Fl_Widget*, void* userdata) {
    PlotWindow* pw = static_cast<PlotWindow*>(userdata);
    if (pw && pw->canvas) pw->canvas->zoom(1.0 / 1.25);
}

static void plot_reset_cb(Fl_Widget*, void* userdata) {
    PlotWindow* pw = static_cast<PlotWindow*>(userdata);
    if (pw && pw->canvas) pw->canvas->reset_view();
}

static void plot_save_cb(Fl_Widget*, void* userdata) {
    PlotWindow* pw = static_cast<PlotWindow*>(userdata);
    if (!pw) return;

    Fl_Native_File_Chooser fc;
    fc.title("Save plot as SVG");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter("SVG\t*.svg");
    fc.preset_file("plot.svg");

    if (fc.show() != 0) return;

    std::string dst = fc.filename() ? fc.filename() : "";
    if (dst.empty()) return;
    if (fs::path(dst).extension() != ".svg") dst += ".svg";

    try {
        std::string tmp = svg_tools::save_svg_plot<double>(
                              pw->req.title, pw->req.series, pw->req.style, pw->req.scatter_mode
                          );

        std::error_code ec;
        fs::copy_file(tmp, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            fl_alert("Failed to save SVG:\n%s", dst.c_str());
            return;
        }
        fl_message("Saved plot to:\n%s", dst.c_str());
    } catch (...) {
        fl_alert("Failed to save plot.");
    }
}

static void create_plot_window_cb(void* userdata) {
    std::unique_ptr<PlotRequest> req(static_cast<PlotRequest*>(userdata));
    if (!req) return;

    PlotWindow* pw = new PlotWindow;
    pw->req = *req;

    const int W = 900;
    const int H = 650;
    const int toolbar_h = 36;

    std::string win_title = req->title.empty() ? "Musil Plot" : ("Musil Plot - " + req->title);

    pw->win = new Fl_Double_Window(W, H, win_title.c_str());
    pw->win->begin();

    pw->btn_zoom_in  = new Fl_Button(10,  6, 70, 24, "Zoom +");
    pw->btn_zoom_out = new Fl_Button(90,  6, 70, 24, "Zoom -");
    pw->btn_reset    = new Fl_Button(170, 6, 70, 24, "Reset");
    pw->btn_save     = new Fl_Button(W - 170, 6, 70, 24, "Save");
    pw->btn_close    = new Fl_Button(W - 90,  6, 70, 24, "Close");

    pw->canvas = new PlotCanvas(10, toolbar_h + 4, W - 20, H - toolbar_h - 14, *req);

    pw->btn_zoom_in->callback(plot_zoom_in_cb, pw);
    pw->btn_zoom_out->callback(plot_zoom_out_cb, pw);
    pw->btn_reset->callback(plot_reset_cb, pw);
    pw->btn_save->callback(plot_save_cb, pw);
    pw->btn_close->callback(plot_close_cb, pw);

    pw->win->resizable(pw->canvas);
    pw->win->callback(plot_close_cb, pw);
    pw->win->end();
    pw->win->set_non_modal();
    pw->win->show();
}

static std::string plotting_do_plot_ide(const std::string& title,
                                        const std::vector<svg_tools::Series<double>>& series,
                                        char style,
                                        bool scatter_mode) {
    PlotRequest* req = new PlotRequest;
    req->title = title;
    req->series = series;
    req->style = style;
    req->scatter_mode = scatter_mode;

    Fl::awake(create_plot_window_cb, req);
    return "[plot]";
}

} // namespace musil_plot_ide

#endif // BUILD_MUSIL_IDE

static std::string plotting_do_plot(const std::string& title,
                                    const std::vector<svg_tools::Series<double>>& series,
                                    char style,
                                    bool scatter_mode) {
#ifdef BUILD_MUSIL_IDE
    return musil_plot_ide::plotting_do_plot_ide(title, series, style, scatter_mode);
#else
    return svg_tools::save_svg_plot<double>(title, series, style, scatter_mode);
#endif
}

// plot(title?, y1, legend1?, y2, legend2?, ..., style) -> filename or "[plot]"
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
        }
        else if (y.size() != expected_len) {
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

// scatter(title?, x1, y1, legend1?, x2, y2, legend2?, ..., style) -> filename or "[plot]"
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