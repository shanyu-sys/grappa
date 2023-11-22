#include <cstdint>

#ifndef CONSTANTS_H
#define CONSTANTS_H


const uint32_t FRAME_HEIGHT = 1080 / 4;
const uint32_t FRAME_WIDTH = 1920 / 4;
const std::size_t FRAME_SIZE = static_cast<std::size_t>(FRAME_HEIGHT) * static_cast<std::size_t>(FRAME_WIDTH) * 3;


#endif // CONSTANTS_H