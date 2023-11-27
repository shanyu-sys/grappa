#ifndef FRAME_HPP
#define FRAME_HPP

#include <cstdint>
#include "constants.h"
#include <vector>
#include <memory>
#include <Grappa.hpp>
#include <Delegate.hpp>
#include <iostream>
#include <array>
#include <opencv2/opencv.hpp>

using namespace Grappa;

struct SubFrame
{
    uint8_t data[SUB_FRAME_SIZE] = {0xff};
};

struct subFrameGlobalAddrs
{
    GlobalAddress<SubFrame> data[NUM_SUB_FRAMES];

    subFrameGlobalAddrs(std::vector<GlobalAddress<SubFrame>> subframeaddrs)
    {
        assert(subframeaddrs.size() == NUM_SUB_FRAMES);
        for (int i = 0; i < NUM_SUB_FRAMES; i++)
        {
            this->data[i] = subframeaddrs[i];
        }
    }

    subFrameGlobalAddrs() {}
};

class Frame
{
public:
    std::vector<std::shared_ptr<SubFrame>> subframes;
    std::vector<GlobalAddress<SubFrame>> subframeaddrs;

    // public:
    // default constructor: all sub frames as zeros
    Frame()
    {
        for (int i = 0; i < NUM_SUB_FRAMES; i++)
        {
            std::shared_ptr<SubFrame> sf = std::make_shared<SubFrame>();
            subframes.push_back(sf);
            subframeaddrs.push_back(make_global(subframes[i].get()));
        }
        assert(subframes.size() == NUM_SUB_FRAMES);
    }

    // Frame is read from a cv::Mat
    Frame(cv::Mat &mat)
    {
        for (int i = 0; i < NUM_SUB_FRAMES; i++)
        {
            std::shared_ptr<SubFrame> sf = std::make_shared<SubFrame>();
            memcpy(sf->data, mat.data + i * SUB_FRAME_SIZE, SUB_FRAME_SIZE);
            subframes.push_back(sf);
            subframeaddrs.push_back(make_global(subframes[i].get()));
        }
        assert(subframes.size() == NUM_SUB_FRAMES);
    }

    // deep copy
    Frame(const Frame &f)
    {
        for (int i = 0; i < NUM_SUB_FRAMES; i++)
        {
            std::shared_ptr<SubFrame> sf = std::make_shared<SubFrame>();
            memcpy(sf->data, f.subframes[i]->data, SUB_FRAME_SIZE);
            subframes.push_back(sf);
            subframeaddrs.push_back(make_global(subframes[i].get()));
        }
        assert(subframes.size() == NUM_SUB_FRAMES);
        // verify the copy
        for (int i = 0; i < NUM_SUB_FRAMES; i++)
        {
            GlobalAddress<SubFrame> sf_addr = subframeaddrs[i];
            SubFrame sf = delegate::read(sf_addr);
            assert(memcmp(sf.data, f.subframes[i]->data, SUB_FRAME_SIZE) == 0);
        }
    }
};

cv::Mat subFrametoMat(subFrameGlobalAddrs subframeaddrs);

void process_frame(cv::Mat &image);

#endif // FRAME_HPP