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

// const uint32_t FRAME_SIZE = block_size;
const uint32_t FRAME_SIZE = 16;

struct Frame
{

  int x[FRAME_SIZE] = {0};

  //  uint8_t x[FRAME_SIZE] = {0};
};

class Video
{
public:
  int width;
  int height;

  GlobalAddress<Frame> frames = global_alloc<Frame>(5);

  // initialize
  Video(int w, int h) : width(w), height(h) {}

  // add a frame
  void addFrame(int i, Frame f)
  {
    delegate::write(frames + i, f);
  }

  // deep copy
  Video(const Video &v)
  {
    width = v.width;
    height = v.height;
    frames = global_alloc<Frame>(5);
    memcpy(frames, v.frames, 5);
  }

  GlobalAddress<Frame> getFrame(int i)
  {
    return frames + i;
  }
};

// create a video
Video create()
{
  Video v(10, 10);
  for (int i = 0; i < 5; i++)
  {
    Frame f;
    // intialize frame to be i
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      // f.x[j] = uint8_t(i);
      f.x[j] = i;
    }
    std::cout << "frame " << i << " x value " << f.x[0] << std::endl;

    v.addFrame(i, f);

    std::cout << "frame " << i << " written value " << delegate::read(v.frames + i).x[0] << std::endl;
  }
  return v;
}

std::vector<Video> videos;

int main(int argc, char *argv[])
{
  init(&argc, &argv);

  run([]
      {
        std::cout << "frame size " << FRAME_SIZE << std::endl;


    // LOG(INFO) << "'main' task started";

    // std::cout << "total cores " << cores() << std::endl;
    // std::cout << "total locales " << locales() << std::endl;

    // create a video on other core
    // int64_t x = 0;
    // GlobalAddress<int64_t> x_addr = make_global(&x, 1);

    on_all_cores([]{
      Video v = create();
      // std::cout << "video height: " << v.height << std::endl;
      // std::cout << "video width: " << v.width << std::endl;

      Frame f = delegate::read(v.frames + 3);
      // std::cout << "frame 3 x value " << f.x[0] << std::endl;

      // create an array of videos
      // std::vector<Video> videos;
      for(int i = 0; i < 10; i++){
        videos.emplace_back(v);
      }
    });

    
    GlobalAddress<Frame> frame1 = delegate::call(1, []{
      return videos[1].frames;
      

    });

    Frame f = delegate::read(frame1 + 3);
    std::cout << "frame 3 x value " << f.x[0] << std::endl;

    // // read one video back
    // Video v2 = v;
    // Video v2 = videos[5];
    // std::cout << "video 5 height: " << v2.height << std::endl;
    // std::cout << "video 5 width: " << v2.width << std::endl;

    // Frame f2 = delegate::read(v2.frames + 3);
    // std::cout << "frame 3 x value " << f2.x[0] << std::endl;

    


    // GlobalAddress<Video> videos = global_alloc<Video>(10);

    // forall( videos, 10, [](int64_t i, Video& v){
    //   v = create();
    //   std::cout << "Video " << i << " created" << "on core " << mycore() << std::endl;
    // });

    // // read one video back
    // Video v = delegate::read(videos + 5);

    // for(int i = 0; i < v.frames.size(); i++){
    //   std::cout << "frame " << i << "x value " << v.frames[i].x << std::endl;
    // }

    // on_all_cores([]{
    //   LOG(INFO) << "Hello world from core " << mycore() << " locale " << mylocale();
    // });

    LOG(INFO) << "all tasks completed, exiting..."; });

  finalize();

  return 0;
}
