#include <string>
#include <vector>
#include <iostream>
#include <tuple>

// define a struct for returning multiple values
struct ProcessedText {
    std::string text;
    std::vector<std::string> mentions;
    std::vector<std::string> urls;
};

ProcessedText process_text(std::string text) {
    std::vector<std::string> mentions;
    std::vector<std::string> urls;
    size_t length = text.length();
    size_t i = 0;

    while (i < length) {
        if (i < length - 7 && (text.substr(i, 7) == "http://" || text.substr(i, 8) == "https://")) {
            size_t j = i + (text.substr(i, 7) == "http://" ? 7 : 8);
            while (j < length && text[j] != ' ') {
                j++;
            }
            urls.push_back(text.substr(i, j - i));
            i = j;
            continue;
        }
        if (text[i] == '@') {
            size_t j = i;
            while (j < length && text[j] != ' ') {
                j++;
            }
            mentions.push_back(text.substr(i, j - i));
            for (size_t k = i; k < j; k++) {
                text[k] = '*';
            }
            i = j;
        }
        i++;
    }



    return ProcessedText{ text, mentions, urls};
}

// int main() {
//     std::string text = "Hello @world! This is a test of the emergency broadcast system. https://www.google.com/ https://www.youtube.com/";
//     ProcessedText processed_text = process_text(text);
//     std::cout << "Processed text: " << processed_text.text << std::endl;
//     std::cout << "Mentions: ";
//     for (const std::string& mention : processed_text.mentions) {
//         std::cout << mention << " ";
//     }
//     std::cout << std::endl;
//     std::cout << "URLs: ";
//     for (const std::string& url : processed_text.urls) {
//         std::cout << url << " ";
//     }
//     std::cout << std::endl;

//     return 0;
// }

