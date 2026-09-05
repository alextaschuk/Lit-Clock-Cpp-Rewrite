#pragma once

#include <algorithm>
#include <cctype>
#include <string>

// Get a file's path from the root of this project.
inline std::string projectPath(const std::string& relativePath) {
    return std::string(PROJECT_ROOT) + "/" + relativePath;
}

// Convert all characters in a string to lowercase.
// Borrowed from https://stackoverflow.com/a/313990
std::string toLower(const std::string& text) {
    std::string loweredText = text;
    std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(),
    [](unsigned char c){ return std::tolower(c); });
    return loweredText;
}

// Split a string into individual words using a delimiter.
// Borrowed from https://stackoverflow.com/a/14266139.
//
// s: The string to split from.
// delimiter: A substring of `s` to split the string with.
//
// Returns a vector of the split string.
static std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    tokens.push_back(s);

    return tokens;
}