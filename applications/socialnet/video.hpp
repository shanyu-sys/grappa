#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include "constants.h"
#include <Grappa.hpp>
#include <Delegate.hpp>
#include "frame.hpp"

using namespace Grappa;

class Video
{
public:
    int width;
    int height;
    std::vector<Frame> frames;

    // initialize
    Video(int w, int h) : width(w), height(h)
    {
    }

    // add a frame
    void addFrame(int i, cv::Mat &f)
    {
        Frame frame = Frame(f);
        frames.push_back(frame);
    }

    // deep copy
    Video(const Video &v)
    {
        width = v.width;
        height = v.height;

        // deep copy frames
        for (int i = 0; i < MAX_FRAMES; i++)
        {
            Frame fCopy = v.frames[i];
            frames.push_back(fCopy);
        }
        assert(frames.size() == MAX_FRAMES);
    }

    Frame *getFrame(int i)
    {
        return &frames[i];
    }

    subFrameGlobalAddrs getSubFrameAddrs(int i)
    {
        Frame *f = &frames[i];
        return subFrameGlobalAddrs(f->subframeaddrs);
    }
};

Video decode();

#endif // VIDEO_HPP