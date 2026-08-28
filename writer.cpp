#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <string>

struct BoundingBox {
    int topLeftX = 0;
    int topLeftY = 0;
    int bottomRightX = 0;
    int bottomRightY = 0;
};

struct Fonts {
    std::vector<unsigned char> regularBuf;
    stbtt_fontinfo regular;

    std::vector<unsigned char> boldBuf;
    stbtt_fontinfo bold;

    std::vector<unsigned char> italicBuf;
    stbtt_fontinfo italic;

    std::vector<unsigned char> italicBoldBuf;
    stbtt_fontinfo italicBold;

    std::vector<unsigned char> creditBuf;
    stbtt_fontinfo credit;
};

struct Row {
    std::string time;
    std::string timestring;
    std::string quote;
    std::string title;
    std::string author;
};

enum TextType {
    QUOTE,
    CREDITS // author and book title
};

class Pen {
    /**
     * stores info to write on an image.
     */
public:
    stbtt_fontinfo font;
    Fonts fonts;
    float fontScale;
    short int color = 128;
    TextType textType = QUOTE;
    std::string text;
    BoundingBox bbox;
    float x = 0;
    float y = 0;
    // TODO: character delimiters

    void reset (float x, float y);
};

void Pen::reset (float x_pos, float y_pos)
{
    x = x_pos;
    y = y_pos;
    // TODO: reset all char delim counters to 0
}
