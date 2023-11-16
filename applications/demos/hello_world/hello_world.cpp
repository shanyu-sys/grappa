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

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool finished = false;

public:
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(value);
        cv.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&](){ return !queue.empty() || finished; });
        if (queue.empty()) {
            return false;
        }
        value = queue.front();
        queue.pop();
        return true;
    }

    void signal_finished() {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
        cv.notify_all();
    }
};


// grappa_thread = none;
// input_queue = none;
// output_queue = None;

// def connect():
//     input_queue = Queue()
//     output_queue = Queue()
//     grappa_thread = thread() {
//       grappa.init()
//       grappa.run() {
//         run_on_all_cores {
//           while (true) {
//             if (input_queue.empty()) {
//               break;
//             }
//             taks = input_queue.pop()
//             case task:
//               read: Grappa::delegate::read;
//               write: Grappa::delegate::write;

//              output_queue.put(task_result)
//           }
//         }
//       }
//     }

//     thread.start()

//     input_queue.push(connect_task)


// def read():

//    input_queue.push(read_taks)

//    while output_queue.empty():
//      pass
//    result = output_queue.pop()
//    return result


// main() {
//   connect()
//   xxx = read(xxx, xxx)
//   write()
// }

int main( int argc, char * argv[] ) {
  init( &argc, &argv );
  std::mutex mtx;
  std::condition_variable cv;
  bool finished = false;
  int current_core = mycore();
  std::cout << "current core is " << current_core << std::endl;

  ThreadSafeQueue<std::string> queue;


  std::thread t([&]() {
        // Simulate work
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // std::cout << "Hello world from thread " << std::this_thread::get_id() << std::endl;

        // Signal that the thread has finished its work
        {
          std::lock_guard<std::mutex> lock(mtx);
          finished = true;
        }
        cv.notify_one();
        queue.push("Hello world from new thread of thread " + std::to_string(mycore()) + " of " + std::to_string(cores()));
        
        // Signal that production is finished
        queue.signal_finished();
  });
     // Consumer code to be run in parallel
    auto consumer_func = [&]() {

    };


  // Detach the thread so it will run independently
  t.detach();
  run([&]{
    for (int i=0; i<cores(); i++) {
      Grappa::delegate::call(i, [i, current_core, &queue]{
        std::cout << "i is " << i << " current_core is " << current_core << std::endl;
        // if(i == current_core){
          std::string value;
          while (queue.pop(value)) {
              // Process the value
              LOG(INFO) << value;
          }
        // }
      });
    }

    
    on_all_cores([&]{
      // LOG(INFO) << "Hello world from locale " << mylocale() << " core " << mycore() << " hostname " << hostname();
      // std::string value;
      // while (queue.pop(value)) {

      //   std::cout << value << std::endl;
      // }
    });
    
  });
  {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&](){ return finished; });
  }

  
  finalize();
  
  return 0;
}
