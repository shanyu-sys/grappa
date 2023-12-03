#ifndef __TEXT_HPP__
#define __TEXT_HPP__

#include <string>
#include <vector>
#include <iostream>
#include <tuple>
#include "text.hpp"

struct ProcessedText {
    std::string text;
    std::vector<std::string> mentions;
    std::vector<std::string> urls;
};

ProcessedText process_text(std::string text);

#endif // __TEXT_HPP__