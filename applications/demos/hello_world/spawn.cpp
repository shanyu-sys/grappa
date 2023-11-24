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

using namespace Grappa;


int main( int argc, char * argv[] ) {
  init( &argc, &argv );
   
  run([]{
    LOG(INFO) << "'main' task started";
    
    std::cout << "total cores " << cores() << std::endl; 
    std::cout << "total locales " << locales() << std::endl;

    CompletionEvent joiner;
    
    spawn(&joiner, []{
      LOG(INFO) << "task ran on " << mycore();
      // do some work
      int64_t x = 0;
      for (int i=0; i<100000000; i++) {
        x += i;
      }
      LOG(INFO) << "task done on " << mycore();
    });
    
    spawn<TaskMode::Unbound>(&joiner, []{
      LOG(INFO) << "unbound task 1 ran on core " << mycore() << " locale " << mylocale();
      // do some work
      int64_t x = 0;
      for (int i=0; i<100000000; i++) {
        x *= i;
      }
    });

    int64_t x = 0;
    GlobalAddress<int64_t> x_addr = make_global(&x, 11);

    delegate::call<async>(x_addr.core(), [x_addr]{
      LOG(INFO) << "delegate call task ran on " << mycore();
      // do some work
      int64_t x = 0;
      for (int i=0; i<100000000; i++) {
        x += i;
      }
      LOG(INFO) << "task done on " << mycore();
    });

    // spawn<TaskMode::Unbound>(&joiner, []{
    //   LOG(INFO) << "unbound task 2 ran on core " << mycore() << " locale " << mylocale();
    //   // do some work
    //   int64_t x = 0;
    //   for (int i=0; i<100000000; i++) {
    //     x *= i;
    //   }
    // });
    
    joiner.wait();
    
    on_all_cores([]{
      LOG(INFO) << "Hello world from core " << mycore() << " locale " << mylocale();
    });

    LOG(INFO) << "all tasks completed, exiting...";
  });

  
  finalize();
  
  return 0;
}
