#include "utils.hpp"

#include <algorithm>
#include <print>

std::string projectPath(const std::string& relativePath) {
    return std::string(PROJECT_ROOT) + "/" + relativePath;
}

std::string toLower(const std::string& text) {
    std::string loweredText = text;
    std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(),
    [](unsigned char c){ return std::tolower(c); });
    return loweredText;
}

std::vector<std::string> split(std::string s, const std::string& delimiter)
{
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

/**
 * `std::string` stores data as raw bytes in UTF-8 encoding, and it retrieves the
 * data 1 byte at a time. stb expects a unicode codepoint to know which character
 * is being used. Unicode characters use 1-4 bytes to encode their codepoint. For
 * example, the UTF-8 encoding for an em dash is 0xE2 0x80 0x94. Its codepoint
 * is U+2014. Therefore, an em dash is stored in a `std::string` as [0xE2, 0x80, 0x94].
 * So if we call something like `char c = s[0]`, `c` would store 0xE2 instead of the
 * entire encoding. 
 * 
 * Thus, this function has two purposes. First, it determines how many bytes are used
 * for a given character in a string. Then, it decodes the UTF-8 encoding of the character
 * and returns the character's codepoint (`numBytes` also holds the number of bytes that 
 * are used to encode the character so that we know how much to increment `i` by).
 */
int decodeUTF8(const std::string& s, size_t byteOffset, int& numBytes)
{
    unsigned char c = s[byteOffset];

    // All ASCII characters are encoded with 1 byte and their codepoints match their UTF-8 encoding.
    if (c < 0x80) {
        numBytes = 1;
        return c;
    }

    /**
     * Characters where the 3 MSbs of their first byte in their UTF-8 encoding are 110
     * (110xxxxx) use 2 bytes for encoding. The last byte of all encodings with 2+ bytes is called the
     * continuation byte. The 2 MSbs of this byte are always 10 (10xxxxxx).
     * 
     * So, for this case, we have two bytes to work with: For the first byte (byte 0) we need to AND away
     * the 3 MSBs since they only tell us how many bytes make up the encoding and don't relate to data
     * of the codepoint's encoding. Then we shift the remaining bytes over to move room for the next byte
     * (byte 1/ continuation byte).
     * 
     * For the second byte, we AND away the 2 MSBs so that only the bits pertaining to encoding remain.
     * Lastly, we OR the two bytes together. What we are left with is the character's codepoint
     */ 
    else if ((c & 0xE0) == 0xC0)
    {
        numBytes = 2;
        int byteZero = (c & 0x1F) << 6;
        int byteOne = s[byteOffset + 1] & 0x3F;
        return byteZero | byteOne;
    }

    // Encodings where the 4 MSbs of their first byte are 1110 (1110xxxx) use 3 bytes for encoding.
    else if ((c & 0xF0) == 0xE0)
    {
        numBytes = 3;
        int byteZero = (c & 0x0F) << 12;
        int byteOne = (s[byteOffset+1] & 0x3F) << 6;
        int byteTwo = s[byteOffset+2] & 0x3F;
        return byteZero | byteOne | byteTwo;
    }

    // Encodings where the 4 MSbs of their first byte are 1110 (1110xxxx) use 3 bytes for encoding.
    else if ((c & 0xF8) == 0xF0)
    {
        numBytes = 4;
        int byteZero = ((c & 0x07) << 18);
        int byteOne = (s[byteOffset+1] & 0x3F) << 12;
        int byteTwo = (s[byteOffset+2] & 0x3F) << 6 ;
        int byteThree = s[byteOffset+3] & 0x3F;
        return byteZero | byteOne | byteTwo | byteThree;
    }

    numBytes = 1;
    std::println("Error: Malformed Byte: {:#x}", static_cast<int>(c));
    return c;
}