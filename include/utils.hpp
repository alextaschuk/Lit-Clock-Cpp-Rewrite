#pragma once

#include <cctype>
#include <string>
#include <vector>

// Get a file's path from the root of this project.
std::string projectPath(const std::string& relativePath);

// Convert all characters in a string to lowercase.
// Borrowed from https://stackoverflow.com/a/313990
std::string toLower(const std::string& text);

// Split a string into individual words using a delimiter.
// Borrowed from https://stackoverflow.com/a/14266139.
//
// s: The string to split from.
// delimiter: A substring of `s` to split the string with.
//
// Returns a vector of the split string.
std::vector<std::string> split(std::string s, const std::string& delimiter);


// Decodes a single Unicode codepoint from a UTF-8 encoded string, starting at the given byte offset.
//
// s: The UTF-8-encoded string to decode from.
// byteOffset: the byte offset within s at which to begin decoding. Must point to the first byte of a valid UTF-8 sequence 
//      (not a continuation byte).
// numBytes: output parameter. Set to the number of bytes consumed by the decoded codepoint (1-4), so the caller can
//      advance their index by this amount. Any prior value is overwritten.
//
// Returns the decoded Unicode codepoint in base 10.
//
// See the function's definition for more details.
int decodeUTF8(const std::string& s, size_t byteOffset, int& numBytes);