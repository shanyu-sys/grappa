#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include "constants.h"
#include "video.hpp"

using namespace Grappa;

Video decode()
{
    // Video file path
    std::string videoFilePath = "/mnt/ssd/haoran/types_proj/dataset/12svideo.mp4";

    // Open the video file
    cv::VideoCapture cap(videoFilePath);
    if (!cap.isOpened())
    {
        std::cerr << "Error opening video file" << std::endl;
        throw std::runtime_error("Failed to open video file.");
    }

    uint32_t width = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_WIDTH)) * 0.25;
    uint32_t height = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)) * 0.25;

    Video video(width, height);
    int frameIndex = 0;

    while (true)
    {
        cv::Mat cvFrame;
        cv::Mat resizedFrame;
        // Read frame
        cap >> cvFrame;

        // Check if the frame is empty
        if (cvFrame.empty())
            break;

        // Resize the frame to a quarter of its original size
        cv::resize(cvFrame, resizedFrame, cv::Size(), 0.25, 0.25);

        // Check if the resized frame size matches the expected FRAME_SIZE
        if (resizedFrame.total() * resizedFrame.elemSize() != FRAME_SIZE)
        {
            std::cerr << "Frame size" << resizedFrame.total() * resizedFrame.elemSize() << "is not equal to expected frame size" << FRAME_SIZE << std::endl;
            break;
        }

        // Store the frame
        if (frameIndex < MAX_FRAMES)
        {
            video.addFrame(frameIndex, resizedFrame);
        }

        frameIndex++;
    }
    // Release the video capture object
    cap.release();
    cv::destroyAllWindows();
    return video;

}