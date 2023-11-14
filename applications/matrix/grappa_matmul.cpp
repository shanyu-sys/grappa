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


void subadd(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c, 
            size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start,
            size_t a_width, size_t b_width, size_t m) {
    for (int i = 0; i < m; ++i) {
        int a_start = (i + a_row_start) * a_width + a_col_start;
        int b_start = (i + b_row_start) * b_width + b_col_start;
        for (int j = 0; j < m; ++j) {
            delegate::write(c + i * m + j, delegate::read(a + a_start + j) + delegate::read(b + b_start + j));
        }
    }
}

void subsub(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c, 
            size_t a_row_start, size_t a_col_start, size_t b_row_start, size_t b_col_start,
            size_t a_width, size_t b_width, size_t m) {
    for (int i = 0; i < m; ++i) {
        int a_start = (i + a_row_start) * a_width + a_col_start;
        int b_start = (i + b_row_start) * b_width + b_col_start;
        for (int j = 0; j < m; ++j) {
            delegate::write(c + i * m + j, delegate::read(a + a_start + j) - delegate::read(b + b_start + j));
        }
    }
}

void subcpy(GlobalAddress<int32_t> a, GlobalAddress<int32_t> result,
            size_t a_row_start, size_t a_col_start, size_t a_width, size_t m) {
    for (int i = 0; i < m; ++i) {
        size_t a_start = (i + a_row_start) * a_width + a_col_start;
        for (int j = 0; j < m; ++j) {
            delegate::write(result + i * m + j, delegate::read(a + a_start + j));
        }
    }
}

void constitute(GlobalAddress<int32_t> m11, GlobalAddress<int32_t> m12, GlobalAddress<int32_t> m21, GlobalAddress<int32_t> m22,
                GlobalAddress<int32_t> result, size_t m) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < m; ++j){
            delegate::write(result + i * m * 2 + j, delegate::read(m11 + i * m + j));
            delegate::write(result + i * m * 2 + m + j, delegate::read(m12 + i * m + j));
            delegate::write(result + (i + m) * m * 2 + j, delegate::read(m21 + i * m + j));
            delegate::write(result + (i + m) * m * 2 + m + j, delegate::read(m22 + i * m + j));
        }
    }
}

void sub(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b){
    forall(a, MATRIX_SIZE * MATRIX_SIZE, [=](int64_t i, int32_t& a){
        a = a - delegate::read(b + i);
    });
}

void add(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b){
    forall(a, MATRIX_SIZE * MATRIX_SIZE, [=](int64_t i, int32_t& a){
        a = a + delegate::read(b + i);
    });
}


void remote_strassen_mul(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b, GlobalAddress<int32_t> c, size_t m){
    std::vector<int32_t> local_a_vector(m * m);
    std::vector<int32_t> local_b_vector(m * m);

    for(int i = 0; i < m; i++){
        for(int j = 0; j < m; j++){
            local_a_vector[i * m + j] = delegate::read(a + i * m + j);
            local_b_vector[i * m + j] = delegate::read(b + i * m + j);
        }
    }

    global_free(a);
    global_free(b);
    
    Matrix local_a(m, m, local_a_vector);
    Matrix local_b(m, m, local_b_vector);

    Matrix local_c = strassen_mul(local_a, local_b);

    for(int i = 0; i < m; i++){
        for(int j = 0; j < m; j++){
            delegate::write(c + i * m + j, local_c.elements[i * m + j]);
        }
    }

}


// distributed strassen multiplication
void distributed_strassen(GlobalAddress<int32_t> a, GlobalAddress<int32_t> b,  GlobalAddress<int32_t> c, size_t m0, u_int32_t level){
    std::cout << "level: " << level << " core: " << mycore() << std::endl;
    size_t m = m0/2;
    // Submatrix indices
    size_t tl_row_start = 0, tl_col_start = 0;
    size_t tr_row_start = 0, tr_col_start = m;
    size_t bl_row_start = m, bl_col_start = 0;
    size_t br_row_start = m, br_col_start = m;

    GlobalAddress <int32_t> a11 = global_alloc<int32_t>(m*m);
    subadd(a, a, a11, tl_row_start, tl_col_start, br_row_start, br_col_start, m0, m0, m);
    GlobalAddress <int32_t> aa2 = global_alloc<int32_t>(m*m);
    subadd(a, a, aa2, bl_row_start, bl_col_start, br_row_start, br_col_start, m0, m0, m);
    GlobalAddress <int32_t> aa3 = global_alloc<int32_t>(m*m);
    subcpy(a, aa3, tl_row_start, tl_col_start, m0, m);
    GlobalAddress <int32_t> aa4 = global_alloc<int32_t>(m*m);
    subcpy(a, aa4, br_row_start, br_col_start, m0, m);
    GlobalAddress <int32_t> aa5 = global_alloc<int32_t>(m*m);
    subsub(a, a, aa5, tl_row_start, tl_col_start, tr_row_start, tr_col_start, m0, m0, m);
    GlobalAddress <int32_t> aa6 = global_alloc<int32_t>(m*m);
    subsub(a, a, aa6, bl_row_start, bl_col_start, tl_row_start, tl_col_start, m0, m0, m);
    GlobalAddress <int32_t> aa7 = global_alloc<int32_t>(m*m);
    subsub(a, a, aa7, tr_row_start, tr_col_start, br_row_start, br_col_start, m0, m0, m);

    GlobalAddress <int32_t> b11 = global_alloc<int32_t>(m*m);
    subadd(b, b, b11, tl_row_start, tl_col_start, br_row_start, br_col_start, m0, m0, m);
    GlobalAddress <int32_t> bb2 = global_alloc<int32_t>(m*m);
    subcpy(b, bb2, tl_row_start, tl_col_start, m0, m);
    GlobalAddress <int32_t> bb3 = global_alloc<int32_t>(m*m);
    subsub(b, b, bb3, tr_row_start, tr_col_start, br_row_start, br_col_start, m0, m0, m);
    GlobalAddress <int32_t> bb4 = global_alloc<int32_t>(m*m);
    subsub(b, b, bb4, bl_row_start, bl_col_start, tl_row_start, tl_col_start, m0, m0, m);
    GlobalAddress <int32_t> bb5 = global_alloc<int32_t>(m*m);
    subcpy(b, bb5, br_row_start, br_col_start, m0, m);
    GlobalAddress <int32_t> bb6 = global_alloc<int32_t>(m*m);
    subadd(b, b, bb6, tl_row_start, tl_col_start, tr_row_start, tr_col_start, m0, m0, m);
    GlobalAddress <int32_t> bb7 = global_alloc<int32_t>(m*m);
    subadd(b, b, bb7, bl_row_start, bl_col_start, br_row_start, br_col_start, m0, m0, m);

    std::cout << "level: " << level << std::endl;
    global_free(a);
    std::cout << "a freed" << std::endl;
    global_free(b);
    std::cout << "b freed" << std::endl;

    GlobalCompletionEvent local_gce;

    if(level == 1 || level == 2){
        GlobalAddress<int32_t> m1 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(a11, b11, m1, m, level + 1);
        });

        GlobalAddress<int32_t> m2 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(aa2, bb2, m2, m, level + 1);
        });

        GlobalAddress<int32_t> m3 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(aa3, bb3, m3, m, level + 1);
        });

        GlobalAddress<int32_t> m4 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(aa4, bb4, m4, m, level + 1);
        });

        GlobalAddress<int32_t> m5 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(aa5, bb5, m5, m, level + 1);
        });

        GlobalAddress<int32_t> m6 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(aa6, bb6, m6, m, level + 1);
        });

        GlobalAddress<int32_t> m7 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            distributed_strassen(aa7, bb7, m7, m, level + 1);
        });

        local_gce.wait();

        sub(m7, m5);
        add(m7, m4);
        add(m7, m1);
        
        add(m5, m3);
        add(m4, m2);
        
        sub(m1, m2);
        add(m1, m3);
        add(m1, m6);

        constitute(m7, m5, m4, m1, c, m);

        global_free(m1);
        global_free(m2);
        global_free(m3);
        global_free(m4);
        global_free(m5);
        global_free(m6);
        global_free(m7);
    } else {
        // level = 3
        GlobalAddress<int32_t> m1 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(a11, b11, m1, m);
        });

        GlobalAddress<int32_t> m2 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(aa2, bb2, m2, m);
        });

        GlobalAddress<int32_t> m3 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(aa3, bb3, m3, m);
        });

        GlobalAddress<int32_t> m4 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(aa4, bb4, m4, m);
        });

        GlobalAddress<int32_t> m5 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(aa5, bb5, m5, m);
        });

        GlobalAddress<int32_t> m6 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(aa6, bb6, m6, m);
        });

        GlobalAddress<int32_t> m7 = global_alloc<int32_t>(m*m);
        spawn<TaskMode::Unbound>(&local_gce, [=]{
            remote_strassen_mul(aa7, bb7, m7, m);
        });

        local_gce.wait();

        sub(m7, m5);
        add(m7, m4);
        add(m7, m1);
        
        add(m5, m3);
        add(m4, m2);

        sub(m1, m2);
        add(m1, m3);
        add(m1, m6);

        constitute(m7, m5, m4, m1, c, m);

        global_free(m1);
        global_free(m2);
        global_free(m3);
        global_free(m4);
        global_free(m5);
        global_free(m6);
        global_free(m7);
    
    }
}


int main( int argc, char * argv[] ) {

    init( &argc, &argv );
  
    run( [] {
        auto A = global_alloc<int32_t>(MATRIX_SIZE*MATRIX_SIZE);
        Grappa::memset( A, 1, MATRIX_SIZE*MATRIX_SIZE );

        auto B = global_alloc<int32_t>(MATRIX_SIZE*MATRIX_SIZE);
        Grappa::memset( B, 1, MATRIX_SIZE*MATRIX_SIZE );
        
        CompletionEvent ce;

        auto C = global_alloc<int32_t>(MATRIX_SIZE*MATRIX_SIZE);

        std::cout << "start" << std::endl;
        spawn(&ce, [=]{
            distributed_strassen(A, B, C, MATRIX_SIZE, 1);
            });
        
        ce.wait();

        for (int i = MATRIX_SIZE - 16; i < MATRIX_SIZE; i++) {
            for (int j = MATRIX_SIZE - 16; j < MATRIX_SIZE; j++) {
                std::cout << "matrix_c[" << i << ", " << j << "] = " << delegate::read(C+ i * MATRIX_SIZE + j) << std::endl;
            }
            std::cout << std::endl;
        }

    });

    finalize();

    return 0;
}
