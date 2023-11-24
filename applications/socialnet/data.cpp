#include "data.hpp"

const u

#include <vector>

const int FRAME_SIZE = /* specify frame size here */;

class Frame {
public:
    std::vector<uint8_t> frame;

    Frame() : frame(FRAME_SIZE) {
        // Initializes the vector with FRAME_SIZE elements
    }

    // The default copy constructor and assignment operator work fine now
};

struct FrameWrapper {
    size_t frame;
};

class Video {
public:
    uint32_t width;
    uint32_t height;
    std::vector<Frame> frames;

    // Default copy constructor and assignment operator are sufficient
    // because std::vector handles copying internally
};

