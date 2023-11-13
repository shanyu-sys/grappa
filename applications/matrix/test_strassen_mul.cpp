#include <iostream>
#include "Matrix.hpp"

// Helper function to print matrix for debugging purposes.
void print_matrix(const Matrix& mat) {
    for (int i = 0; i < mat.row; ++i) {
        for (int j = 0; j < mat.col; ++j) {
            std::cout << mat.elements[i * mat.col + j] << " ";
        }
        std::cout << "\n";
    }
}

// Function to compare two matrices.
bool compare_matrices(const Matrix& a, const Matrix& b) {
    if (a.row != b.row || a.col != b.col) {
        return false;
    }
    for (int i = 0; i < a.row; ++i) {
        for (int j = 0; j < a.col; ++j) {
            if (a.elements[i * a.col + j] != b.elements[i * b.col + j]) {
                return false;
            }
        }
    }
    return true;
}

// Test case function.
void test_strassen_mul_1() {
    // Initialize two 2x2 matrices.
    std::vector<int> a_elements = {1, 2, 3, 4};
    std::vector<int> b_elements = {5, 6, 7, 8};

    Matrix a(2, 2, a_elements);
    Matrix b(2, 2, b_elements);

    // Expected result of multiplication.
    std::vector<int> expected_elements = {19, 22, 43, 50};
    Matrix expected_result(2, 2, expected_elements);

    // Perform multiplication using Strassen's algorithm.
    Matrix result = strassen_mul(a, b);

    // Check if the result matches the expected result.
    if (compare_matrices(result, expected_result)) {
        std::cout << "Strassen's matrix multiplication test 1 passed." << std::endl;
    } else {
        std::cout << "Strassen's matrix multiplication test 1 failed." << std::endl;
        std::cout << "Expected matrix:" << std::endl;
        print_matrix(expected_result);
        std::cout << "Result matrix:" << std::endl;
        print_matrix(result);
    }
}

void test_strassen_mul_2() {
    // Create two test matrices
    std::vector<int> a_elements = {1, 1, 1, 1};
    std::vector<int> b_elements = {2, 2, 2, 2};

    Matrix a(2, 2, a_elements); // 2x2 matrix filled with 1s
    Matrix b(2, 2, b_elements); // 2x2 matrix filled with 2s

    // Perform multiplication using Strassen's algorithm
    Matrix result = strassen_mul(a, b);

    // Expected result of multiplication
    std::vector<int> expected_elements = {4, 4, 4, 4};
    Matrix expected_result(2, 2, expected_elements);

    if (compare_matrices(result, expected_result)) {
        std::cout << "Strassen's matrix multiplication test 2 passed." << std::endl;
    } else {
        std::cout << "Strassen's matrix multiplication test 2 failed." << std::endl;
        std::cout << "Expected matrix:" << std::endl;
        print_matrix(expected_result);
        std::cout << "Result matrix:" << std::endl;
        print_matrix(result);
    }

}




int main() {
    test_strassen_mul_1();
    test_strassen_mul_2();
    return 0;
}
