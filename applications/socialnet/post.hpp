#ifndef POST_HPP
#define POST_HPP

#include <string>
#include <vector>
#include <cstdint>

struct Post
{
    size_t post_id;
    size_t req_id;
    std::string text;
    std::vector<std::string> mentions;
    uint32_t preview_width;
    uint32_t preview_height;
    size_t preview;
    std::vector<std::string> urls;
    uint64_t timestamp;
    uint8_t post_type;
};

#endif // POST_HPP