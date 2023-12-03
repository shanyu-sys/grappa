////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010-2015, University of Washington and Battelle
// Memorial Institute.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//     * Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
//       disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.
//     * Neither the name of the University of Washington, Battelle
//       Memorial Institute, or the names of their contributors may be
//       used to endorse or promote products derived from this
//       software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// UNIVERSITY OF WASHINGTON OR BATTELLE MEMORIAL INSTITUTE BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
////////////////////////////////////////////////////////////////////////

#include <Grappa.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <iostream>
#include <Delegate.hpp>
#include <vector>
#include <array>

using namespace Grappa;

const int MATRIX_SIZE = 4096;
GlobalCompletionEvent gce;

int main(int argc, char *argv[])
{
  init(&argc, &argv);

  run([]
      {
        auto A = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);
        Grappa::memset(A, 1, MATRIX_SIZE * MATRIX_SIZE);

        auto B = global_alloc<int32_t>(MATRIX_SIZE * MATRIX_SIZE);
        // Grappa::memset(B, 1, MATRIX_SIZE * MATRIX_SIZE);

        on_all_cores([=]{
          std::cout << "core " << mycore() << " locale " << mylocale() << std::endl;
          int64_t m = MATRIX_SIZE;
        std::vector<int32_t> local_a_vector(m * m);
        std::vector<int32_t> local_b_vector(m * m);
        GlobalAddress<std::vector<int32_t>> local_a_addr = make_global(&local_a_vector);
        GlobalAddress<std::vector<int32_t>> local_b_addr = make_global(&local_b_vector);

        double start = walltime();

        // forall<&gce>(A, m * m, [=](int64_t i, int32_t& a){
        //   delegate::call(local_a_addr, [=](std::vector<int32_t>& local_a){
        //     local_a[i] = a;
        //   });

        // forall<&gce>(A, m*m, [=](int64_t start, int64_t n, int32_t* a){
        //   for(int i = 0; i < n; i++){
        //     auto val = a[i];
        //     delegate::call(local_a_addr, [=](std::vector<int32_t>& local_a){
        //       local_a[start + i] = val;
        //     });
        //   }
            // });

        forall<&gce>(A, m*m, [=](int64_t start, int64_t n, int32_t* a){
          std::cout << "core"<< mycore() << " :start = " << start << " n = " << n << std::endl;
          delegate::call(local_a_addr, [=](std::vector<int32_t>& local_a){
            for(int i = 0; i < n; i++){
              local_a[start + i] = a[i];
            }
          });

          
        });
        gce.wait();
        std::cout << "core"<< mycore() << " :read takes " << walltime() - start << std::endl;

        // check local_a_vector are all 1s
        for(int i = 0; i < m * m; i++){
          if(local_a_vector[i] != 1){
            std::cout << "local_a_vector[" << i << "] = " << local_a_vector[i] << std::endl;
          }
        }

        double write_start = walltime();
        // write local_a_vector to B
        forall<&gce>(B, m*m, [=](int64_t start, int64_t n, int32_t* b){
          delegate::call(local_a_addr, [=](std::vector<int32_t>& local_a){
            for(int i = 0; i < n; i++){
              b[i] = local_a[start + i];
            }
          });
        });
        gce.wait();
        std::cout << "core"<< mycore() << " :write takes " << walltime() - write_start << std::endl;

        //check B
        forall<&gce>(B, m*m, [=](int64_t i, int32_t& b){
          if(b != 1){
            std::cout << "B[" << i << "] = " << b << std::endl;
          }
        });


        });
        
      });

  finalize();

  return 0;
}
