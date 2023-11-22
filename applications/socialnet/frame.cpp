#include <opencv2/opencv.hpp>

struct Frame {
    void *data;
};

const int FRAME_SIZE = 1080 / 4 * 1920 / 4 * 3;
const int FRAME_WIDTH = 1920 / 4;
const int FRAME_HEIGHT = 1080 / 4;

cv::Vec3b tint(const cv::Vec3b& gray, const cv::Vec3b& color) {
    float dist_from_mid = std::abs(gray[0] - 128.0f) / 255.0f;
    float scale_factor = 1.0f - 4.0f * std::pow(dist_from_mid, 2);
    cv::Vec3b result;
    for (int i = 0; i < 3; ++i) {
        result[i] = static_cast<uchar>(gray[0] + scale_factor * (color[i] - gray[0]));
    }
    return result;
}

cv::Vec3b color_gradient(const cv::Vec3b& gray, const cv::Vec3b& low, const cv::Vec3b& mid, const cv::Vec3b& high) {
    float fraction = gray[0] / 255.0f;
    cv::Vec3b lower, upper;
    float offset;

    if (fraction < 0.5) {
        lower = low;
        upper = mid;
        offset = 0.0;
    } else {
        lower = mid;
        upper = high;
        offset = 0.5;
    }

    float right_weight = 2.0 * (fraction - offset);
    float left_weight = 1.0 - right_weight;
    cv::Vec3b result;

    for (int i = 0; i < 3; ++i) {
        result[i] = static_cast<uchar>(left_weight * lower[i] + right_weight * upper[i]);
    }

    return result;
}


void drawText(cv::Mat &image, const std::string &text, const cv::Point &org) {
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 1;
    int thickness = 2;
    cv::Scalar color(0, 0, 255); // white color

    cv::putText(image, text, org, fontFace, fontScale, color, thickness);
}


void process_frame(Frame& frame) {
    // Convert frame buffer to OpenCV Mat (assuming frame.frame is a std::vector<unsigned char>)
    cv::Mat image = cv::Mat(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3, frame.data);

    // Apply color tint and gradient
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            cv::Vec3b& pixel = image.at<cv::Vec3b>(y, x);
            pixel = tint(pixel, cv::Vec3b(0, 0, 255)); // blue
            pixel = color_gradient(pixel, cv::Vec3b(0, 0, 0), cv::Vec3b(255, 0, 0), cv::Vec3b(255, 255, 0)); // black, red, yellow
        }
    }

    // Draw text (this is a simplified example, actual text drawing with FreeType is more complex)
    const char* text = "Hello, world!";

    drawText(image, text, cv::Point(50, 50));

    const char* text2 = "HollandMa!";
    drawText(image, text2, cv::Point(100, 100));

    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            cv::Vec3b& pixel = image.at<cv::Vec3b>(y, x);
            pixel = tint(pixel, cv::Vec3b(0, 0, 255)); // blue
            pixel = color_gradient(pixel, cv::Vec3b(0, 0, 0), cv::Vec3b(255, 0, 0), cv::Vec3b(255, 255, 0)); // black, red, yellow
        }
    }

    // Convert the processed image back to frame buffer
    std::memcpy(frame.data, image.data, FRAME_SIZE);

}

// write a function to write the frame to a file
void write_frame(Frame& frame) {
    cv::Mat image = cv::Mat(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3, frame.data);
    cv::imwrite("output.png", image);
}

// int main() {
//     Frame frame;
//     frame.data = new unsigned char[FRAME_SIZE];
//     // initialize frame.data to some value
//     for (int i = 0; i < FRAME_SIZE; ++i) {
//         ((uint8_t *) frame.data)[i] = 255;
//     }
//     process_frame(frame);
//     write_frame(frame);

//     return 0;
// }