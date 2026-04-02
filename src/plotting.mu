# plotting.mu
# Convenience helpers on top of core plot(...) and scatter(...)

load("scientific.mu")

var PLOT_DEFAULT_STYLE = "."
var SCATTER_DEFAULT_STYLE = "."

proc qplot(y) {
    return plot("", y, PLOT_DEFAULT_STYLE)
}

proc qplot2(y1, y2) {
    return plot("", y1, "series1", y2, "series2", PLOT_DEFAULT_STYLE)
}

proc qplot3(y1, y2, y3) {
    return plot("", y1, "series1", y2, "series2", y3, "series3", PLOT_DEFAULT_STYLE)
}

proc qscatter(x, y) {
    return scatter("", x, y, SCATTER_DEFAULT_STYLE)
}

proc qscatter2(x1, y1, x2, y2) {
    return scatter("", x1, y1, "set1", x2, y2, "set2", SCATTER_DEFAULT_STYLE)
}

proc qscatter3(x1, y1, x2, y2, x3, y3) {
    return scatter("", x1, y1, "set1", x2, y2, "set2", x3, y3, "set3", SCATTER_DEFAULT_STYLE)
}

# plot-col(M, col)
# Uses scientific.mu matcol helper / builtin if present.
proc plot_col(M, col) {
    var c = matcol(M, col)
    return qplot(c)
}

proc plot_cols2(M, col1, col2) {
    var c1 = matcol(M, col1)
    var c2 = matcol(M, col2)
    return qplot2(c1, c2)
}

proc scatter_cols(M, colx, coly) {
    var xs = matcol(M, colx)
    var ys = matcol(M, coly)
    return qscatter(xs, ys)
}

proc qplot_t(title, y) {
    return plot(title, y, PLOT_DEFAULT_STYLE)
}

proc qscatter_t(title, x, y) {
    return scatter(title, x, y, SCATTER_DEFAULT_STYLE)
}
