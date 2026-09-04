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
