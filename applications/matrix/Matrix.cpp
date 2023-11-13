#include <vector>
#include <stdexcept>
#include "Matrix.hpp"

const size_t SINGLE_SIZE = 16;

Matrix::Matrix(size_t row_size, int element_value) 
        : row(row_size), col(row_size), 
          internal_elements(row_size * row_size, element_value), 
          elements(internal_elements) {
        // 'elements' now refers to the vector managed by 'internal_elements_ptr'
    }

Matrix& Matrix::add(const Matrix& other) {
    if (row != other.row || col != other.col)
        throw std::invalid_argument("Matrix size not match for addition");

    for (size_t i = 0; i < elements.size(); ++i) {
        elements[i] += other.elements[i];
    }
    return *this;
}

Matrix& Matrix::sub(const Matrix& other) {
    if (row != other.row || col != other.col)
        throw std::invalid_argument("Matrix size not match for subtraction");

    for (size_t i = 0; i < elements.size(); ++i) {
        elements[i] -= other.elements[i];
    }
    return *this;
}

Matrix mul_simple(const Matrix& a, const Matrix& b) {
    if (a.col != b.row)
        throw std::invalid_argument("Matrix size not match for multiplication");

    Matrix result(a.row, 0);
    for (int i = 0; i < a.row; ++i) {
        for (int j = 0; j < b.col; ++j) {
            int sum = 0;
            for (int k = 0; k < a.col; ++k) {
                sum += a.elements[i * a.col + k] * b.elements[k * b.col + j];
            }
            result.elements[i * a.row + j] = sum;
        }
    }
    return result;
}

Matrix subadd(const Matrix& a, const Matrix& b, size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start, size_t m) {
    Matrix result(m, 0);
    for (int i = 0; i < m; ++i) {
        int a_start = (i + a_row_start) * a.row + a_col_start;
        int b_start = (i + b_row_start) * b.row + b_col_start;
        for (int j = 0; j < m; ++j) {
            result.elements[i * m + j] = a.elements[a_start + j] + b.elements[b_start + j];
        }
    }
    return result;
}

Matrix subsub(const Matrix& a, const Matrix& b, size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start, size_t m) {
    Matrix result(m, 0);
    for (int i = 0; i < m; ++i) {
        int a_start = (i + a_row_start) * a.row + a_col_start;
        int b_start = (i + b_row_start) * b.row + b_col_start;
        for (int j = 0; j < m; ++j) {
            result.elements[i * m + j] = a.elements[a_start + j] - b.elements[b_start + j];
        }
    }
    return result;
}

Matrix subcpy(const Matrix& a, size_t a_row_start, size_t a_col_start, size_t m) {
    Matrix result(m, 0);
    for (int i = 0; i < m; ++i) {
        size_t a_start = (i + a_row_start) * a.row + a_col_start;
        for (int j = 0; j < m; ++j) {
            result.elements[i * m + j] = a.elements[a_start + j];
        }
    }
    return result;
}

Matrix constitute(const Matrix& m11, const Matrix& m12, const Matrix& m21, const Matrix& m22) {
    size_t m = m11.row;
    Matrix result(m * 2, 0);
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < m; ++j) {
            result.elements[i * 2 * m + j] = m11.elements[i * m + j];
            result.elements[i * 2 * m + j + m] = m12.elements[i * m + j];
            result.elements[(i + m) * 2 * m + j] = m21.elements[i * m + j];
            result.elements[(i + m) * 2 * m + j + m] = m22.elements[i * m + j];
        }
    }
    return result;
}

Matrix strassen_mul(const Matrix& a, const Matrix& b) {
    if (a.row == 2) {
        return mul_simple(a, b);
    }

    size_t m = a.row / 2;
    // Submatrix indices
    size_t tl_row_start = 0, tl_col_start = 0;
    size_t tr_row_start = 0, tr_col_start = m;
    size_t bl_row_start = m, bl_col_start = 0;
    size_t br_row_start = m, br_col_start = m;

    // Submatrices of `a`
    Matrix aa1 = subadd(a, a, tl_row_start, tl_col_start, br_row_start, br_col_start, m);
    Matrix aa2 = subadd(a, a, bl_row_start, bl_col_start, br_row_start, br_col_start, m);
    Matrix aa3 = subcpy(a, tl_row_start, tl_col_start, m);
    Matrix aa4 = subcpy(a, br_row_start, br_col_start, m);
    Matrix aa5 = subadd(a, a, tl_row_start, tl_col_start, tr_row_start, tr_col_start, m);
    Matrix aa6 = subsub(a, a, bl_row_start, bl_col_start, tl_row_start, tl_col_start, m);
    Matrix aa7 = subsub(a, a, tr_row_start, tr_col_start, br_row_start, br_col_start, m);

    // Submatrices of `b`
    Matrix bb1 = subadd(b, b, tl_row_start, tl_col_start, br_row_start, br_col_start, m);
    Matrix bb2 = subcpy(b, tl_row_start, tl_col_start, m);
    Matrix bb3 = subsub(b, b, tr_row_start, tr_col_start, br_row_start, br_col_start, m);
    Matrix bb4 = subsub(b, b, bl_row_start, bl_col_start, tl_row_start, tl_col_start, m);
    Matrix bb5 = subcpy(b, br_row_start, br_col_start, m);
    Matrix bb6 = subadd(b, b, tl_row_start, tl_col_start, tr_row_start, tr_col_start, m);
    Matrix bb7 = subadd(b, b, bl_row_start, bl_col_start, br_row_start, br_col_start, m);

    // Recursive multiplications
    Matrix m1 = strassen_mul(aa1, bb1);
    Matrix m2 = strassen_mul(aa2, bb2);
    Matrix m3 = strassen_mul(aa3, bb3);
    Matrix m4 = strassen_mul(aa4, bb4);
    Matrix m5 = strassen_mul(aa5, bb5);
    Matrix m6 = strassen_mul(aa6, bb6);
    Matrix m7 = strassen_mul(aa7, bb7);

    // Combine results
    m7.sub(m5).add(m4).add(m1);
    m5.add(m3);
    m4.add(m2);
    m1.sub(m2).add(m3).add(m6);

    return constitute(m7, m5, m4, m1);
}
