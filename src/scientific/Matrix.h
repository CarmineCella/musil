// Matrix.h

#ifndef MATRIX_H
#define MATRIX_H

#include <vector>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>

#define MAX_COLS 12
#define MAX_ROWS 200

template <typename T>
class Matrix {
private:
    T ** _m;
    int _rows;
    int _cols;
    size_t number_of_digits(T n) {
        std::ostringstream strs;
        strs << n;
        return strs.str().size();
    }
    Matrix remove_row_col(int r, int c) { // remove rth row and cth col
        assert(r < _rows && c < _cols);
        Matrix temp(_rows-1, _cols-1);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                if (i != r && j != c) {
                    int cur_i = i, cur_j = j;
                    if (i > r) {
                        cur_i--;
                    }
                    if (j > c) {
                        cur_j--;
                    }
                    temp[cur_i][cur_j] = _m[i][j];
                }
            }
        }
        return temp;
    }
    void clear () {
        if (!_m) {
            return;
        }
        for (int i = 0; i < _rows; i++) {
            delete[] _m[i];
        }
        delete[] _m;
        _m = NULL;
        _rows = 0;
        _cols = 0;
    }
public:
    Matrix() {
        _rows = 0;
        _cols = 0;
        _m = NULL;
    }
    Matrix(int r, int c, T num = 0) {
        _rows = r;
        _cols = c;
        _m = new T*[r];
        for(int i=0; i<r; i++) {
            _m[i] = new T[c];
        }
        for (int i = 0; i < r; i++) {
            for (int j = 0; j < c; j++) {
                _m[i][j] = num;
            }
        }
    }
    Matrix (double ** rhs, int r, int c) { // shallow copy
        _rows = r;
        _cols = c;
        _m = rhs;
    }
    Matrix(const Matrix & rhs) { // copy constructor
        _rows = rhs.rows();
        _cols = rhs.cols();
        _m = new T*[_rows];
        for (int i = 0; i<_rows; i++) {
            _m[i] = new T[_cols];
        }
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                _m[i][j] = rhs._m[i][j];
            }
        }
    }
    virtual ~Matrix () {
        clear ();
    }
    int rows () const {
        return _rows;
    }
    int cols () const {
        return _cols;
    }
    T& operator()(int r, int c) {
        return _m[r][c];
    }
    const T& operator()(int r, int c) const {
        return _m[r][c];
    }
    T* operator[](int r) {
        return _m[r];
    }
    const T* operator[](int r) const {
        return _m[r];
    }
    T** data () {
        return _m;
    }
    const T** data () const {
        return _m;
    }
    std::vector<T> flatten_row_major() const {
        std::vector<T> out;
        out.reserve(_rows * _cols);

        for (std::size_t i = 0; i < _rows; ++i) {
            for (std::size_t j = 0; j < _cols; ++j) {
                out.push_back(_m[i][j]);
            }
        }
        return out;
    }
    Matrix get_rows(int start, int end) {
        assert(start < _rows && end < _rows);
        Matrix temp(end-start+1, _cols);
        for (int i = start; i <= end; i++) { // end included
            for (int j = 0; j < _cols; j++) {
                temp._m[i-start][j] = _m[i][j];
            }
        }
        return temp;
    }
    Matrix get_cols(int start, int end) {
        assert(start < _cols && end < _cols);
        Matrix temp(_rows, end - start + 1);
        for (int i = 0; i < _rows; i++) {
            for (int j = start; j <= end; j++) { // end included
                temp._m[i][j - start] = _m[i][j];
            }
        }
        return temp;
    }
    std::ostream& print (std::ostream& out) {
        out << "[" << std::endl;
        std::vector<int> max_len_per_col;
        for (size_t j = 0; j < _cols; ++j) {
            size_t max_len {};

            for (size_t i = 0; i < _rows; ++i) {
                int num_length = number_of_digits(_m[i][j]);
                if (num_length > max_len) {
                    max_len = num_length;
                }
            }
            max_len_per_col.push_back (max_len);
        }
        for (std::size_t row = 0; row < _rows; ++row) {
            for (std::size_t col = 0; col < _cols; ++col) {
                out << std::setw (max_len_per_col[col]) << _m[row][col];
                if (col != _cols - 1) out << ", ";
                if (col > MAX_COLS) {
                    out << "...";
                    break;
                }
            }
            if (row != _rows - 1) out << ";\n";
            if (row > MAX_ROWS) {
                out << "...";
                break;
            }
        }
        out << "\n]\n";
        return out;
    }
    void null () {
        for (unsigned i = 0; i < _rows; ++i) {
            for (unsigned j = 0; j < _cols; ++j) {
                _m[i][j] = 0;
            }
        }
    }
    void id () {
        if (_rows != _cols) throw std::runtime_error ("identity matrices must be square");
        null ();
        for (unsigned i = 0; i < _rows; ++i) _m[i][i] = 1;
    }
    Matrix operator+(const Matrix & rhs) const { // sum two same sized matrices
        assert(_rows == rhs.rows() && _cols == rhs.cols());
        Matrix res(_rows, _cols);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                res[i][j] = _m[i][j] + rhs[i][j];
            }
        }
        return res;
    }
    Matrix operator-(const Matrix & rhs) const { // subtract two same sized matrices
        assert(_rows == rhs.rows() && _cols == rhs.cols());
        Matrix res(_rows, _cols);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                res[i][j] = _m[i][j] - rhs[i][j] ;
            }
        }
        return res;
    }
    Matrix operator*(const Matrix & rhs)const { // matrix multiplication mxn * nxk
        assert(_cols == rhs.rows());
        Matrix res(_rows, rhs.cols());
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < rhs.cols(); j++) {
                for (int k = 0; k < _cols; k++) {
                    res._m[i][j] += _m[i][k] * rhs._m[k][j];
                }
            }
        }
        return res;
    }
    const Matrix & operator=(const Matrix & rhs) { // assignment operator
        if (!(_rows == rhs.rows() && _cols == rhs.cols())) {
            clear ();
            _rows = rhs.rows();
            _cols = rhs.cols();
            _m = new T*[_rows];
            for (int i = 0; i<_rows; i++) {
                _m[i] = new T[_cols];
            }
        }
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                _m[i][j] = rhs._m[i][j];
            }
        }
        return *this;
    }
    Matrix operator*(T rhs)const {
        Matrix temp(*this);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                temp._m[i][j] = _m[i][j] * rhs;
            }
        }
        return temp;
    }
    Matrix operator+(T rhs)const {
        Matrix temp(*this);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                temp._m[i][j] = _m[i][j] + rhs;
            }
        }
        return temp;
    }
    Matrix operator-(T rhs)const {
        Matrix temp(*this);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                temp._m[i][j] = _m[i][j] - rhs;
            }
        }
        return temp;
    }
    Matrix operator/(T rhs)const {
        Matrix temp(*this);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                temp._m[i][j] = _m[i][j] / rhs;
            }
        }
        return temp;
    }
    bool operator==(const Matrix & rhs)const {
        assert(_rows == rhs.rows() && _cols == rhs.cols());
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                if (_m[i][j] != rhs._m[i][j]) {
                    return false;
                }
            }
        }
        return true;
    }
    bool operator!=(const Matrix & rhs)const {
        return !(*this == rhs);
    }
    Matrix transpose () {
        Matrix temp(_cols, _rows);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                temp._m[j][i] = _m[i][j];
            }
        }
        return temp;
    }
    T det() {
        assert(_rows == _cols);
        if (_rows == 1) { // trivial case
            return _m[0][0];
        } else if (_rows == 2) { // leibniz rule
            return _m[0][0] * _m[1][1] - _m[0][1] * _m[1][0];
        }
        T det = 0;
        int sign = 1;
        for (int i = 0; i < _cols; i++) { // iterate over first row
            Matrix reduced_mat = remove_row_col(0, i);
            det += sign* _m[0][i] * reduced_mat.det(); // recursively find reduced matrix's determinant
            sign *= -1; // alternate the sign
        }
        return det;
    }
    Matrix cofactor() { // computes the cofactor matrix of the object
        Matrix res(_rows, _cols);
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                Matrix temp = remove_row_col(i, j);
                res[i][j] = temp.det() * pow(-1, i+j);
            }
        }
        return res;
    }
    void trim() { // removes false elements due to precision
        for (int i = 0; i < _rows; i++) {
            for (int j = 0; j < _cols; j++) {
                T s = abs(_m[i][j]);
                if (s < 1e-11) {
                    _m[i][j] = 0;
                }
            }
        }
    }
    Matrix inverse() { // returns the inverse of the object
        T d = det ();
        if (d == 0) {
            throw std::runtime_error ("matrix is singular");
        }
        Matrix inv = cofactor().transpose()/d; // 1/det(A) * cofactor(A)^T
        return inv;
    }
    Matrix sum(int axis) {
        Matrix temp;
        if (axis == 0) {
            temp = Matrix(_rows,1);
            for (int i = 0; i < _rows; i++) {
                for (int j = 0; j < _cols; j++) {
                    temp[i][0] += _m[i][j];
                }
            }
        } else if (axis == 1) {
            temp = Matrix(1, _cols);
            for (int i = 0; i < _rows; i++) {
                for (int j = 0; j < _cols; j++) {
                    temp[0][j] += _m[i][j];
                }
            }
        } else {
            temp = Matrix(1, 1);
            for (int i = 0; i < _rows; i++) {
                for (int j = 0; j < _cols; j++) {
                    temp[0][0] += _m[i][j];
                }
            }
        }
        return temp;
    }
};

#endif // MATRIX_H

