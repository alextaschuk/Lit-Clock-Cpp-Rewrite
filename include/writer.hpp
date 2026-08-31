#ifndef WRITER_H_
#define WRITER_H_

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include <print>
#include <string>


// Stores the (x,y) coordinate pairs of the top left and bottom right corners of a bounding box.
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

// Holds a row from the quotes CSV file.
struct Row {
    std::string time;
    std::string timestring;
    std::string quote;
    std::string title;
    std::string author;
};

// Tells the pen if it is writing a quote or a quote's credits.
enum TextType {
    QUOTE,
    CREDITS // author and book title
};

// Used to write a glyph on an image. Tracks the image's bounds, where on the image the glyph should be written, the font used to
// write the glyph, the font's scale, and the color of the glyph.
struct Pen {
    stbtt_fontinfo font;
    float fontScale; // value used to convert something from font units to pixel units
    short int color = 128;
    float x = 0; // X coordinate of the pen's position on the image.
    float y = 0; // Y coordinate of the pen's position on the image.
    // TODO: character delimiters
};



class Writer {
  public:
    // The image to write a quote on, represented as a bitmap.
    std::vector<unsigned char> image;

    // Generate an image of a single quote.
    //
    // row: A single row from the CSV file.
    // includeCredits: `true` to write quote's author and title in the bottom right of the image, `false` to discard.
    // pen: TODO: remove since it's a private struct
    //
    // Returns TODO: logic to switch between saving image and returning bitmap. 
    void getImage(Row row, bool includeCredits);
    
  private:  
    Pen pen;
    Fonts fonts;
    TextType textType = QUOTE; // is the text that is being written a quote or credits for a quote?
    std::string text; // The text that the pen is writing.
    BoundingBox bbox; // An area that the pen must write inside of.

    // Move the pen and set all delimiter counters to 0.
    void resetPen (float x_pos, float y_pos)
    {
        pen.x = x_pos;
        pen.y = y_pos;
        // TODO: reset all char delim counters to 0
    }

    // Write text inside the bounding box of an image.
    //
    // image: TODO: remove since it's a class var.
    // pen: TODO: remove since it's a class struct.
    // timestr: Optional substring within a quote that contains the time. Only passed in if the text being written is a quote.
    void writeInBBox(std::vector<unsigned char>& image, std::string timestr = "");

    // Draws a word onto the image, one glyph at a time.
    //
    // image: TODO: remove since it's a class var.
    // word: The word to be written.
    // pen: TODO: remove since it's a class struct.
    void drawWord(std::vector<unsigned char>& image, std::string word);

    // Determines which font and color should be used to write a glyph. If the character is a delimiter, an empty string is returned.
    //
    // c: The glyph (character) whose formatting is to be checked.
    // pen: TODO: remove since it's a class struct.
    int formatChar(int& c, Pen pen);

    // Finds the maximum possible font scale (in font units) that can be used for a given bounding box and determiens how the text
    // should be wrapped to fit in the bbox horizontally.
    //
    // pen: TODO: remove since it's a class struct.
    // wrappedLines: output parameter. Stores the text broken up with newline delimiters to fit in the bbox horizontally.
    //      If the text cannot fit (the optimal font scale is < `MIN_FONT_SCALE`), this stores an empty string.
    //
    // Returns the optimal font scale that is found, or 0 if the text cannot fit
    int findOptimalFontScale(std::string& wrappedLines);

    // Helper to `find_optimal_font_size()`. Wraps text using a given font scale such that the text doesn't overflow past
    // the rightmost x coordinate of the bbox.
    //
    // pen: A temporary pen that is used only in `find_optimal_font_size`.
    // 
    // Returns the text to be written with newline delimiters if it fits. Otherwise, an empty string is returned.
    std::string wrapText(Pen& pen);

    // Checks if a word needs to be moved onto a new line, either due to text wrapping or custom formatting.
    //
    // word: The word to be formatted.
    // lines: The text to be written onto an image.
    // wordLength: The length of the word in pixels.
    // pen: A temp pen
    void formatWord(Pen& pen, std::string word, std::vector<std::string>& lines, const int& wordLength);
    
    // Finds the height of the tallest glyph in a string.
    //
    // line: The line of text to find the tallest glyph in.
    // pen: TODO: remove since it's a class struct.
    int tallestGlyph(const std::string& line, const Pen& pen);

    // Calculates a font's recommended vertical spacing between two rows of text.
    //
    // font: The font used to calculate the spacing between the rows.
    // fontScale: The font's scale to convert the spacing units from font to pixel.
    //
    // Returns the font's recommended vertical spacing between two rows of text.
    int getLineHeight(const stbtt_fontinfo& font, const float& fontScale)
    {
        // (ascent - descent) is the height of the font's tallest glyph.
        // lineGap is the font's recommended spacing between the bottom of one row's descent and the top
        // of the next row's ascent.
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
        return static_cast<int>((ascent - descent + lineGap) * fontScale);
    }

    // Decodes a single Unicode codepoint from a UTF-8 encoded string, starting at the given byte offset.
    //
    // s: The UTF-8-encoded string to decode from.
    // i: the byte offset within s at which to begin decoding. Must point to the first byte of a valid UTF-8 sequence 
    //      (not a continuation byte).
    // numBytes: output parameter. Set to the number of bytes consumed by the decoded codepoint (1-4), so the caller can
    //      advance their index by this amount. Any prior value is overwritten.
    //
    // Returns the decoded Unicode codepoint in base 10.
    //
    // See the function's definition for more details.
    int decodeUTF8(const std::string& s, size_t i, int& numBytes);

    // Initalize a stb font
    //
    // fontPath: File path to a TrueType or OpenType file.
    // outBuf: output parameter. A byte buffer of the font file. 
    // outFont: output parameter. A font profile used for font-related tasks, such as drawing a glyph.
    void initFont(const std::string& fontPath, std::vector<unsigned char>& outBuf, stbtt_fontinfo& outFont)
    {
        std::ifstream fontStream(fontPath, std::ios::binary); // read the entire font file into a buffer
        if (!fontStream) { throw std::runtime_error("Failed to open font file: " + fontPath); }

        outBuf = std::vector<unsigned char>(std::istreambuf_iterator<char>(fontStream), {});
        if (!stbtt_InitFont(&outFont, outBuf.data(), 0)) {
            throw std::runtime_error("stbtt_InitFont failed.");
        }
    }

    // Split a string into individual words using a delimiter.
    // Borrowed from https://stackoverflow.com/a/14266139.
    //
    // s: The string to split from.
    // delimiter: A substring of `s` to split the string with.
    //
    // Returns a vector of the split string.
    std::vector<std::string> split(std::string s, const std::string& delimiter);
};
   

int decodeUTF8(const std::string& s, size_t i, int& numBytes);

#endif // WRITER_H_