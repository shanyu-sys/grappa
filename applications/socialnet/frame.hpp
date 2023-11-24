#ifndef FRAME_HPP
#define FRAME_HPP

#include <opencv2/opencv.hpp>
#include "constants.h"

const uint32_t BLOCK_SIZE = 65536*8;

struct Frame
{
    uint8_t data[FRAME_SIZE] = {0};

    uint8_t data_padding[BLOCK_SIZE - FRAME_SIZE] = {0};

    Frame(const cv::Mat &mat)
    {
        if (mat.isContinuous())
        { 
            memcpy(data, mat.data, FRAME_SIZE);
        }
        else
        {
            // Handle non-continuous case
            for (int i = 0; i < mat.rows; ++i)
            {
                memcpy(data + i * mat.cols * mat.elemSize(), mat.ptr<uint8_t>(i), mat.cols * mat.elemSize());
            }
        }
    }
    Frame() {}
};

// static check that Frame is the right size
static_assert(sizeof(Frame) == BLOCK_SIZE, "Frame is not the right size");

#endif // FRAME_HPP
