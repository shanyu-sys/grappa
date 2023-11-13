// Matrix.hpp

#ifndef MATRIX_HPP
#define MATRIX_HPP

#include <vector>
#include <memory>
#include <cstddef>

class Matrix {
public:
    size_t row, col;
    std::vector<int>& elements;
    std::vector<int> internal_elements; // Internal vector for default initialization

    Matrix(size_t row_size, int element_value = 0);
    Matrix(size_t rows, size_t cols, std::vector<int>& elements) : row(rows), col(cols), elements(elements) { }

    Matrix& add(const Matrix& other);
    Matrix& sub(const Matrix& other);
};

Matrix mul_simple(const Matrix& a, const Matrix& b);
Matrix subadd(const Matrix& a, const Matrix& b, size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start, size_t m);
Matrix subsub(const Matrix& a, const Matrix& b, size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start, size_t m);
Matrix subcpy(const Matrix& a, size_t a_row_start, size_t a_col_start, size_t m);
Matrix constitute(const Matrix& m11, const Matrix& m12, const Matrix& m21, const Matrix& m22);
Matrix strassen_mul(const Matrix& a, const Matrix& b);

#endif // MATRIX_HPP
