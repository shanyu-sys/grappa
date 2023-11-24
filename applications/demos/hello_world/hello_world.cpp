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
const uint32_t FRAME_SIZE = 48600 * 2;

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

  Frame *frames;
  GlobalAddress<Frame> frameaddrs[5];
  // GlobalAddress<Frame> frames = global_alloc<Frame>(5);

  // initialize
  Video(int w, int h) : width(w), height(h)
  {
    std::cout << 10 << std::endl;
    frames = new Frame[5];

    std::cout << 11 << std::endl;
  }

  // add a frame
  void addFrame(int i, Frame *f)
  {
    frames[i] = *f;
    frameaddrs[i] = make_global(&frames[i]);
    // std :: cout << "frame " << i << " on core " << mycore() << " with value 0 of " << frames[i].x[0] << std::endl;
  }

  // deep copy
  Video(const Video &v)
  {
    width = v.width;
    height = v.height;

    // deep copy frames
    frames = new Frame[5];
    for (int i = 0; i < 5; i++)
    {
      frames[i] = v.frames[i];
      // std::cout << "Done copying frame " << i << " on core " << mycore() << " with value 0 of " << frames[i].x[0] << std::endl;
      frameaddrs[i] = make_global(&frames[i]);
      std::cout << "Frame " << i << " Global Address: " << frameaddrs[i] << std::endl;
    }

    // frames = global_alloc<Frame>(5);
    // memcpy(frames, v.frames, 5);
  }

  // Destructor
  // ~Video() {
  //     delete[] frames;
  // }

  GlobalAddress<Frame> getFrame(int i)
  {
    return frameaddrs[i];
  }
};

// create a video
Video create()
{
  Video v(10, 10);
  std::cout << 1 << std::endl;
  for (int i = 0; i < 5; i++)
  {
    Frame *f = new Frame;
    std::cout << 2 << std::endl;
    // std::cout << "start creating frame " << i << " on core " << mycore() << std::endl;
    // intialize frame to be i
    for (int j = 0; j < FRAME_SIZE; j++)
    {
      // f.x[j] = uint8_t(i);
      f->x[j] = i;
    }
    std::cout << "frame " << i << " x value " << f->x[FRAME_SIZE - 1] << std::endl;

    v.addFrame(i, f);

    // std::cout << "frame "<< i <<" written value " << delegate::read(v.getFrame(i)).x[FRAME_SIZE -1] << std::endl;
  }
  return v;
}

std::vector<Video> videos;

int main(int argc, char *argv[])
{
  init(&argc, &argv);

  run([]
      {

      on_all_cores([]{
      if(mycore() == 2 || mycore() == 3){
         std::cout << "core " << mycore() << " locale " << mylocale() << std::endl;
         Video v = create();
          std::cout << "Video created successfully!" << std::endl;
          std::cout << "video height: " << v.height << std::endl;
          std::cout << "video width: " << v.width << std::endl;

          Frame f = delegate::read(v.getFrame(3));
          std::cout << "Core " << mycore() << " test read back frame 3 x value " << f.x[0] << std::endl;
          std::cout << "Core " << mycore() << " frame 3 x value " << f.x[0] << std::endl;

          // create an array of videos
          // std::vector<Video> videos;
          // std::cout << "duplicating videos" << std::endl;
          // for(int i = 0; i < 10; i++){
          //   videos.emplace_back(v);
          // }
          // std::cout << "duplicated videos Done on core " << mycore() << std::endl;
          }
    });

    //     GlobalAddress<Frame> video_1_frames_3 = delegate::call(2, []{
    //       std::cout << "core " << mycore() << " locale " << mylocale() << std::endl;
    //       Video video = videos[1];
    //       return videos[1].getFrame(3);
    //     });
    // std::cout << "video_1_frames_3: " << video_1_frames_3 << std::endl; 

    // Frame f = delegate::read(video_1_frames_3);
    // std::cout << "frame 3 x value " << f.x[FRAME_SIZE - 1] << std::endl;

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
