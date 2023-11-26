#include "frame.hpp"

cv::Mat subFrametoMat(subFrameGlobalAddrs subframeaddrs)
{
    cv::Mat mat = cv::Mat::zeros(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);

    for (int i = 0; i < NUM_SUB_FRAMES; i++)
    {
        GlobalAddress<SubFrame> sf_addr = subframeaddrs.data[i];
        SubFrame sf = delegate::read(sf_addr);
        memcpy(mat.data + i * SUB_FRAME_SIZE, sf.data, SUB_FRAME_SIZE);
    }
    return mat;
}