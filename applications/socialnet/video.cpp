#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include "constants.h"


struct Frame {
    std::vector<uint8_t> data;
    
    Frame(const cv::Mat& mat) {
        if (mat.isContinuous()) {
            data.assign(mat.data, mat.data + mat.total() * mat.elemSize());
        } else {
            // Handle non-continuous case
            for (int i = 0; i < mat.rows; ++i) {
                data.insert(data.end(), mat.ptr<uint8_t>(i), mat.ptr<uint8_t>(i) + mat.cols * mat.elemSize());
            }
        }
    }

};

class Video {
public:
    int width;
    int height;
    std::vector<Frame> frames;

    Video(int w, int h, const std::vector<Frame>& f) : width(w), height(h), frames(f) {}


};


Video decode() {
    // Video file path
    std::string videoFilePath = "/mnt/ssd/haoran/types_proj/dataset/12svideo.mp4";

    // Open the video file
    cv::VideoCapture cap(videoFilePath);
    if (!cap.isOpened()) {
        std::cerr << "Error opening video file" << std::endl;
        throw std::runtime_error("Failed to open video file.");
    }

    uint32_t width = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_WIDTH)) * 0.25;
    uint32_t height = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)) * 0.25;

    std::vector<Frame> frames;
    cv::Mat frame;
    int frameIndex = 0;
    const int maxFrames = 5;  // Number of frames to store

    while (true) {
        // Read frame
        cap >> frame;

        // Check if the frame is empty
        if (frame.empty())
            break;

        // Resize the frame to a quarter of its original size
        cv::Mat resizedFrame;
        cv::resize(frame, resizedFrame, cv::Size(), 0.25, 0.25);

        // Check if the resized frame size matches the expected FRAME_SIZE
        if (resizedFrame.total() * resizedFrame.elemSize() != FRAME_SIZE) {
            std::cerr << "Frame size" << resizedFrame.total() * resizedFrame.elemSize() << "is not equal to expected frame size" << FRAME_SIZE << std::endl;
            break;
        }

        Frame frameObj(resizedFrame);

        // Store the frame
        if (frameIndex < maxFrames) {
            frames.emplace_back(frameObj);
        }

        frameIndex++;

    }

    // Release the video capture object
    cap.release();
    cv::destroyAllWindows();

    // Frames are now stored in 'frames' vector
    // Further processing can be done here

    return Video(width, height, frames);
}

int main(){
    Video video = decode();
    std::cout << "width: " << video.width << std::endl;
    std::cout << "height: " << video.height << std::endl;
    std::cout << "frames: " << video.frames.size() << std::endl;
    // save frames to images on disk
    for (int i = 0; i < video.frames.size(); ++i) {
    std::cout << "Saving frame " << i << std::endl;
    std::string filename = "frame" + std::to_string(i) + ".png";

    // Ensure the data size matches the expected size
    if (video.frames[i].data.size() != video.width * video.height * 3) {
        std::cerr << "Data size mismatch for frame " << i << video.frames[i].data.size()<< std::endl;
        continue;
    }

    // Reconstruct the cv::Mat object
    cv::Mat image(video.height, video.width, CV_8UC3, video.frames[i].data.data());
    
    if (!cv::imwrite(filename, image)) {
        std::cerr << "Failed to write frame " << i << std::endl;
    }
}
    return 0;
}
