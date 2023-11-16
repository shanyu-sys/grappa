#include <Grappa.hpp>

#include <Collective.hpp>
#include <ParallelLoop.hpp>
#include <Delegate.hpp>
#include <AsyncDelegate.hpp>
#include <GlobalAllocator.hpp>
#include <Array.hpp>
#include <vector>
#include "Matrix.hpp"

using namespace Grappa;

// const size_t MATRIX_SIZE = 32768/4;
const size_t MATRIX_SIZE = 1024;

GlobalCompletionEvent gce;

void subadd(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c,
            size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start,
            size_t a_width, size_t b_width, size_t m)
{
    forall(c, m * m, [=](int64_t i, int32_t &c)
        {   
            // calculate the row and column index from i
            size_t row_index = i / m;
            size_t col_index = i % m;
            size_t a_index = (row_index + a_row_start) * a_width + a_col_start + col_index;
            size_t b_index = (row_index + b_row_start) * b_width + b_col_start + col_index;
            c = delegate::read(a + a_index) + delegate::read(b + b_index);
        });
}

void subsub(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c,
            size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start,
            size_t a_width, size_t b_width, size_t m)
{
    forall(c, m * m, [=](int64_t i, int32_t &c)
        {   
            // calculate the row and column index from i
            size_t row_index = i / m;
            size_t col_index = i % m;
            size_t a_index = (row_index + a_row_start) * a_width + a_col_start + col_index;
            size_t b_index = (row_index + b_row_start) * b_width + b_col_start + col_index;
            c = delegate::read(a + a_index) - delegate::read(b + b_index);
        });
}

void subcpy(GlobalAddress<int32_t> a, GlobalAddress<int32_t> result,
            size_t a_row_start, size_t a_col_start, size_t a_width, size_t m)
{
    forall(result, m * m, [=](int64_t i, int32_t &result)
        {   
            // calculate the row and column index from i
            size_t row_index = i / m;
            size_t col_index = i % m;
            size_t a_index = (row_index + a_row_start) * a_width + a_col_start + col_index;
            result = delegate::read(a + a_index);
        });
}

void constitute(GlobalAddress<int32_t> m11, GlobalAddress<int32_t> m12, GlobalAddress<int32_t> m21, GlobalAddress<int32_t> m22,
                GlobalAddress<int32_t> result, size_t m)
{
    forall(result, m * m * 4, [=](int64_t i, int32_t &result)
        {   
            // calculate the row and column index from i
            size_t row_index = i / (m * 2);
            size_t col_index = i % (m * 2);
            size_t m_index = row_index / m * 2 + col_index / m;
            size_t index = row_index % m * m + col_index % m;
            switch (m_index)
            {
            case 0:
                result = delegate::read(m11 + index);
                break;
            case 1:
                result = delegate::read(m12 + index);
                break;
            case 2:
                result = delegate::read(m21 + index);
                break;
            case 3:
                result = delegate::read(m22 + index);
                break;
            default:
                break;
            }
        });
}

void sub(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, size_t m)
{
    forall(a, m * m, [=](int64_t i, int32_t &a)
           { a = a - delegate::read(b + i); });
}

void add(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, size_t m)
{
    forall(a, m * m, [=](int64_t i, int32_t &a)
           { a = a + delegate::read(b + i); });
}

void remote_strassen_mul(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c, size_t m)
{
    std::vector<int32_t> local_a_vector(m * m);
    std::vector<int32_t> local_b_vector(m * m);

    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < m; j++)
        {
            local_a_vector[i * m + j] = delegate::read(a + i * m + j);
            local_b_vector[i * m + j] = delegate::read(b + i * m + j);
        }
    }

    global_free(a);
    global_free(b);

    Matrix local_a(m, m, local_a_vector);
    Matrix local_b(m, m, local_b_vector);

    Matrix local_c = strassen_mul(local_a, local_b);

    double write_start = walltime();
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < m; j++)
        {
            delegate::write(c + i * m + j, local_c.elements[i * m + j]);
        }
    }
    std::cout << "core " << mycore() << ": Write local_c to global c takes " << walltime() - write_start << " seconds" << std::endl;
}

// distributed strassen multiplication
void distributed_strassen(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c, size_t m0, u_int32_t level)
{
    CompletionEvent local_gce;

    std::cout << "level " << level << " core " << mycore() << ": distributed_strassen start" << std::endl;
    size_t m = m0 / 2;
    // Submatrix indices
    size_t tl_row_start = 0, tl_col_start = 0;
    size_t tr_row_start = 0, tr_col_start = m;
    size_t bl_row_start = m, bl_col_start = 0;
    size_t br_row_start = m, br_col_start = m;

    GlobalAddress<int32_t> aa1 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> aa2 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> aa3 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> aa4 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> aa5 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> aa6 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> aa7 = global_alloc<int32_t>(m * m);

    GlobalAddress<int32_t> bb1 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> bb2 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> bb3 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> bb4 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> bb5 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> bb6 = global_alloc<int32_t>(m * m);
    GlobalAddress<int32_t> bb7 = global_alloc<int32_t>(m * m);

    double start = walltime();
    subadd(a, a, aa1, tl_row_start, tl_col_start, br_row_start, br_col_start, m0, m0, m);
    subadd(a, a, aa2, bl_row_start, bl_col_start, br_row_start, br_col_start, m0, m0, m);
    subcpy(a, aa3, tl_row_start, tl_col_start, m0, m);
    subcpy(a, aa4, br_row_start, br_col_start, m0, m);
    subadd(a, a, aa5, tl_row_start, tl_col_start, tr_row_start, tr_col_start, m0, m0, m);
    subsub(a, a, aa6, bl_row_start, bl_col_start, tl_row_start, tl_col_start, m0, m0, m);
    subsub(a, a, aa7, tr_row_start, tr_col_start, br_row_start, br_col_start, m0, m0, m);

    subadd(b, b, bb1, tl_row_start, tl_col_start, br_row_start, br_col_start, m0, m0, m);
    subcpy(b, bb2, tl_row_start, tl_col_start, m0, m);
    subsub(b, b, bb3, tr_row_start, tr_col_start, br_row_start, br_col_start, m0, m0, m);
    subsub(b, b, bb4, bl_row_start, bl_col_start, tl_row_start, tl_col_start, m0, m0, m);
    subcpy(b, bb5, br_row_start, br_col_start, m0, m);
    subadd(b, b, bb6, tl_row_start, tl_col_start, tr_row_start, tr_col_start, m0, m0, m);
    subadd(b, b, bb7, bl_row_start, bl_col_start, br_row_start, br_col_start, m0, m0, m);
    std::cout << "level " << level << " core " << mycore() << ": Get aa1-aa7 and bb1-bb7 takes " << walltime() - start << " seconds" << std::endl;

    GlobalAddress<int32_t> m1 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m2 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m3 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m4 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m5 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m6 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m7 = global_alloc<int32_t>(m*m);

    double matmul_start = walltime();

    if (level == 1)
    {
        spawn(&local_gce, [aa1, bb1, m1, m, level]{
            distributed_strassen(aa1, bb1, m1, m, level + 1);
        });

        spawn(&local_gce, [aa2, bb2, m2, m, level]{
            distributed_strassen(aa2, bb2, m2, m, level + 1);
        });

        spawn(&local_gce, [aa3, bb3, m3, m, level]{
            distributed_strassen(aa3, bb3, m3, m, level + 1);
        });

        spawn(&local_gce, [aa4, bb4, m4, m, level]{
            distributed_strassen(aa4, bb4, m4, m, level + 1);
        });

        spawn(&local_gce, [aa5, bb5, m5, m, level]{
            distributed_strassen(aa5, bb5, m5, m, level + 1);
        });

        spawn(&local_gce, [aa6, bb6, m6, m, level]{
            distributed_strassen(aa6, bb6, m6, m, level + 1);
        });

        spawn(&local_gce, [aa7, bb7, m7, m, level]{
            distributed_strassen(aa7, bb7, m7, m, level + 1);
        });

    }
    else
    {
        // level = 3
        spawn(&local_gce, [aa1, bb1, m1, m]{
            remote_strassen_mul(aa1, bb1, m1, m);
        });

        spawn(&local_gce, [aa2, bb2, m2, m]{
            remote_strassen_mul(aa2, bb2, m2, m);
        });

        spawn(&local_gce, [aa3, bb3, m3, m]{
            remote_strassen_mul(aa3, bb3, m3, m);
        });

        spawn(&local_gce, [aa4, bb4, m4, m]{
            remote_strassen_mul(aa4, bb4, m4, m);
        });

        spawn(&local_gce, [aa5, bb5, m5, m]{
            remote_strassen_mul(aa5, bb5, m5, m);
        });

        spawn(&local_gce, [aa6, bb6, m6, m]{
            remote_strassen_mul(aa6, bb6, m6, m);
        });

        spawn(&local_gce, [aa7, bb7, m7, m]{
            remote_strassen_mul(aa7, bb7, m7, m);
        });
    }
    local_gce.wait();

    std::cout << "level " << level << ": Matrix size: " << m << ", matmul takes " << walltime() - matmul_start << " seconds" << std::endl;

    sub(m7, m5, m);
    add(m7, m4, m);
    add(m7, m1, m);

    add(m5, m3, m);
    add(m4, m2, m);

    sub(m1, m2, m);
    add(m1, m3, m);
    add(m1, m6, m);

    constitute(m7, m5, m4, m1, c, m);

    global_free(m1);
    global_free(m2);
    global_free(m3);
    global_free(m4);
    global_free(m5);
    global_free(m6);
    global_free(m7);
    return;
}

int main(int argc, char *argv[])
{

    init(&argc, &argv);

    run([]
        {
            auto A = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);
            Grappa::memset(A, 1, MATRIX_SIZE * MATRIX_SIZE);

            auto B = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);
            Grappa::memset(B, 1, MATRIX_SIZE * MATRIX_SIZE);

            auto C = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);

            std::cout << "start" << std::endl;
            spawn<&gce>([=]
                        { distributed_strassen(A, B, C, MATRIX_SIZE, 1); });

            gce.wait();

            for (int i = MATRIX_SIZE - 16; i < MATRIX_SIZE; i++)
            {
                for (int j = MATRIX_SIZE - 16; j < MATRIX_SIZE; j++)
                {
                    std::cout << "matrix_c[" << i << ", " << j << "] = " << delegate::read(C + i * MATRIX_SIZE + j) << std::endl;
                }
                std::cout << std::endl;
            }

            std::cout << "end" << std::endl; });

    finalize();

    return 0;
}
