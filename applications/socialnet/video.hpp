#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include "constants.h"
#include "frame.hpp"
#include <Grappa.hpp>
#include <Delegate.hpp>

using namespace Grappa;
class Video {
public:
    int width;
    int height;
    GlobalAddress<Frame> frames = global_alloc<Frame>(MAX_FRAMES);
    
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
        frames = global_alloc<Frame>(MAX_FRAMES);
        memcpy(frames, v.frames, MAX_FRAMES);
    }

};

Video decode();



#endif // VIDEO_HPP
