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

const size_t MATRIX_SIZE = 32768;
// const size_t MATRIX_SIZE = 8192;

GlobalCompletionEvent gce;
GlobalCompletionEvent level1_gce;
GlobalCompletionEvent level2_gce;
GlobalCompletionEvent level3_gce;
GlobalCompletionEvent level4_gce;

struct Params
{
    GlobalAddress<int32_t> a;
    GlobalAddress<int32_t> b;
    GlobalAddress<int32_t> c;
    size_t m;
    u_int32_t level;
};

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

void remote_strassen_mul(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress <int32_t> c, size_t m)
{

    std::cout << "level 3 core " << mycore()  << " locale " << mylocale() << ": remote_strassen_mul start" << std::endl;

    std::vector<int32_t> local_a_vector(m * m);
    std::vector<int32_t> local_b_vector(m * m);

    Matrix local_a(m, m, local_a_vector);
    Matrix local_b(m, m, local_b_vector);
    
    double read_start = walltime();
    GlobalAddress<Matrix> local_a_addr = make_global(&local_a);
    GlobalAddress<Matrix> local_b_addr = make_global(&local_b);
    forall<&level4_gce>(a, m * m, [=](int64_t i, int32_t &a)
           { 
            delegate::call(local_a_addr, [=](Matrix &local_a) {
             local_a.elements[i] = a; });
            });
    
    forall<&level4_gce>(b, m * m, [=](int64_t i, int32_t &b)
           { 
            delegate::call(local_b_addr, [=](Matrix &local_b) {
             local_b.elements[i] = b; });
            });
    

    level4_gce.wait();
    // for (int i = 0; i < m; i++)
    // {
    //     for (int j = 0; j < m; j++)
    //     {
    //         local_a_vector[i * m + j] = delegate::read(a + i * m + j);
    //         local_b_vector[i * m + j] = delegate::read(b + i * m + j);
    //     }
    // }
    std::cout << "level 3 core " << mycore()  << " locale " << mylocale() << ": read takes " << walltime() - read_start << " seconds" << std::endl;

    global_free(a);
    global_free(b);

    double matmul_start = walltime();
    Matrix local_c = strassen_mul(local_a, local_b);
    std::cout << "level 3 core " << mycore()  << " locale " << mylocale() << ": matmul takes " << walltime() - matmul_start << " seconds" << std::endl;

    double write_start = walltime();
    GlobalAddress <Matrix> local_c_addr = make_global(&local_c);
    forall<&level4_gce>(c, m * m, [=](int64_t i, int32_t &c)
           { c = delegate::call(local_c_addr, [=](Matrix &local_c) {
             return local_c.elements[i]; });
            });

    level4_gce.wait();
    // for (int i = 0; i < m; i++)
    // {
    //     for (int j = 0; j < m; j++)
    //     {
    //         delegate::write(c + i * m + j, local_c.elements[i * m + j]);
    //     }
    // }
    std::cout << "level 3 core " << mycore()  << " locale " << mylocale() << ": write takes " << walltime() - write_start << " seconds" << std::endl;
    std::cout << "level 3 core " << mycore()  << " locale " << mylocale() << ": remote_strassen_mul end" << std::endl;
}

// distributed strassen multiplication
void distributed_strassen(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c, size_t m0, u_int32_t level, u_int32_t offset)
{
    std::cout << "level " << level << " core " << mycore()  << " locale " << mylocale() << " thread offset " << offset << ": distributed_strassen start" << std::endl;
    double distributed_start = walltime();
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

    global_free(a);
    global_free(b);

    GlobalAddress<int32_t> m1 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m2 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m3 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m4 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m5 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m6 = global_alloc<int32_t>(m*m);
    GlobalAddress<int32_t> m7 = global_alloc<int32_t>(m*m);

    Params params1 = {aa1, bb1, m1, m, level + 1};
    Params params2 = {aa2, bb2, m2, m, level + 1};
    Params params3 = {aa3, bb3, m3, m, level + 1};
    Params params4 = {aa4, bb4, m4, m, level + 1};
    Params params5 = {aa5, bb5, m5, m, level + 1};
    Params params6 = {aa6, bb6, m6, m, level + 1};
    Params params7 = {aa7, bb7, m7, m, level + 1};

    GlobalAddress <Params> params1_addr = make_global(&params1, (offset*7+1)%cores());
    GlobalAddress <Params> params2_addr = make_global(&params2, (offset*7+2)%cores());
    GlobalAddress <Params> params3_addr = make_global(&params3, (offset*7+3)%cores());
    GlobalAddress <Params> params4_addr = make_global(&params4, (offset*7+4)%cores());
    GlobalAddress <Params> params5_addr = make_global(&params5, (offset*7+5)%cores());
    GlobalAddress <Params> params6_addr = make_global(&params6, (offset*7+6)%cores());
    GlobalAddress <Params> params7_addr = make_global(&params7, (offset*7+7)%cores());

    double matmul_start = walltime();

    if (level == 1)
    {
        forall<SyncMode::Async, &level1_gce>(params1_addr, 1, [=](int64_t i, Params &params1){
            distributed_strassen(aa1, bb1, m1, m, level + 1, offset*7+1);
        });

        forall<SyncMode::Async, &level1_gce>(params2_addr, 1, [=](int64_t i, Params &params2){
            distributed_strassen(aa2, bb2, m2, m, level + 1, offset*7+2);
        });

        forall<SyncMode::Async, &level1_gce>(params3_addr, 1, [=](int64_t i, Params &params3){
            distributed_strassen(aa3, bb3, m3, m, level + 1, offset*7+3);
        });

        forall<SyncMode::Async, &level1_gce>(params4_addr, 1, [=](int64_t i, Params &params4){
            distributed_strassen(aa4, bb4, m4, m, level + 1, offset*7+4);
        });

        forall<SyncMode::Async, &level1_gce>(params5_addr, 1, [=](int64_t i, Params &params5){
            distributed_strassen(aa5, bb5, m5, m, level + 1, offset*7+5);
        });

        forall<SyncMode::Async, &level1_gce>(params6_addr, 1, [=](int64_t i, Params &params6){
            distributed_strassen(aa6, bb6, m6, m, level + 1, offset*7+6);
        });

        forall<SyncMode::Async, &level1_gce>(params7_addr, 1, [=](int64_t i, Params &params7){
            distributed_strassen(aa7, bb7, m7, m, level + 1, offset*7+7);
        });
        level1_gce.wait();
    }
    else if (level == 2){
        forall<SyncMode::Async, &level2_gce>(params1_addr, 1, [=](int64_t i, Params &params1){
            distributed_strassen(aa1, bb1, m1, m, level + 1, offset*7+1);
        });

        forall<SyncMode::Async, &level2_gce>(params2_addr, 1, [=](int64_t i, Params &params2){
            distributed_strassen(aa2, bb2, m2, m, level + 1, offset*7+2);
        });

        forall<SyncMode::Async, &level2_gce>(params3_addr, 1, [=](int64_t i, Params &params3){
            distributed_strassen(aa3, bb3, m3, m, level + 1, offset*7+3);
        });

        forall<SyncMode::Async, &level2_gce>(params4_addr, 1, [=](int64_t i, Params &params4){
            distributed_strassen(aa4, bb4, m4, m, level + 1, offset*7+4);
        });

        forall<SyncMode::Async, &level2_gce>(params5_addr, 1, [=](int64_t i, Params &params5){
            distributed_strassen(aa5, bb5, m5, m, level + 1, offset*7+5);
        });

        forall<SyncMode::Async, &level2_gce>(params6_addr, 1, [=](int64_t i, Params &params6){
            distributed_strassen(aa6, bb6, m6, m, level + 1, offset*7+6);
        });

        forall<SyncMode::Async, &level2_gce>(params7_addr, 1, [=](int64_t i, Params &params7){
            distributed_strassen(aa7, bb7, m7, m, level + 1, offset*7+7);
        });
        level2_gce.wait();
    }
    else
    {
        forall<SyncMode::Async, &level3_gce>(params1_addr, 1, [=](int64_t i, Params &remote_params1){
            remote_strassen_mul(aa1, bb1, m1, m);
        });
        
        forall<SyncMode::Async, &level3_gce>(params2_addr, 1, [=](int64_t i, Params &remote_params2){
            remote_strassen_mul(aa2, bb2, m2, m);
        });

        forall<SyncMode::Async, &level3_gce>(params3_addr, 1, [=](int64_t i, Params &remote_params3){
            remote_strassen_mul(aa3, bb3, m3, m);
        });

        forall<SyncMode::Async, &level3_gce>(params4_addr, 1, [=](int64_t i, Params &remote_params4) {
            remote_strassen_mul(aa4, bb4, m4, m);
        });

        forall<SyncMode::Async, &level3_gce>(params5_addr, 1, [=](int64_t i, Params &remote_params5) {
            remote_strassen_mul(aa5, bb5, m5, m);
        });

        forall<SyncMode::Async, &level3_gce>(params6_addr, 1, [=](int64_t i, Params &remote_params6) {
            remote_strassen_mul(aa6, bb6, m6, m);
        });

        forall<SyncMode::Async, &level3_gce>(params7_addr, 1, [=](int64_t i, Params &remote_params7) {
            remote_strassen_mul(aa7, bb7, m7, m);
        });
        level3_gce.wait();
    }


    std::cout << "level " << level << " core " << mycore()  << " locale " << mylocale() << ": Sub Matrix size: " << m << ", sub 7 matmul takes " << walltime() - matmul_start << " seconds" << std::endl;

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
    std::cout << "level " << level << " core " << mycore()  << " locale " << mylocale() << ": distributed_strassen takes " << walltime() - distributed_start << " seconds" << std::endl;
    return;
}

int main(int argc, char *argv[])
{

    Grappa::init(&argc, &argv);

    run([]
        {
            std::cout << "Number of nodes: " << locales() << std::endl;
            std::cout << "Number of cores: " << cores() << std::endl;
            std::cout << "Matrix size: " << MATRIX_SIZE << std::endl;
            auto A = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);
            Grappa::memset(A, 1, MATRIX_SIZE * MATRIX_SIZE);

            auto B = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);
            Grappa::memset(B, 1, MATRIX_SIZE * MATRIX_SIZE);

            auto C = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);

            double start = walltime();
            spawn<&gce>([=]
                        { distributed_strassen(A, B, C, MATRIX_SIZE, 1, 0); });

            gce.wait();
            double end = walltime();

            for (int i = MATRIX_SIZE - 16; i < MATRIX_SIZE; i++)
            {
                for (int j = MATRIX_SIZE - 16; j < MATRIX_SIZE; j++)
                {
                    std::cout << "matrix_c[" << i << ", " << j << "] = " << delegate::read(C + i * MATRIX_SIZE + j) << std::endl;
                }
                std::cout << std::endl;
            }

            std::cout << "Execution time: " << end - start << " seconds" << std::endl;
             });

    finalize();

    return 0;
}
