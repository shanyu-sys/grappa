#include <cstdint>

#ifndef CONSTANTS_H
#define CONSTANTS_H


const uint32_t FRAME_HEIGHT = 1080 / 4;
const uint32_t FRAME_WIDTH = 1920 / 4;
const std::size_t FRAME_SIZE = static_cast<std::size_t>(FRAME_HEIGHT) * static_cast<std::size_t>(FRAME_WIDTH) * 3;
const uint32_t MAX_FRAMES = 5;
const uint32_t MAX_VIDEOS = 10;
const uint32_t SUB_FRAME_SIZE = 4860;
const uint32_t NUM_SUB_FRAMES = FRAME_SIZE / SUB_FRAME_SIZE;

#endif // CONSTANTS_H