#pragma once

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include <print>
#include <string>

#include "utils.hpp"

static float SCALE_MULTIPLIER = 0.99; // to constrain bboxes to make sure text fits


// Stores the (x,y) coordinate pairs of the top left and bottom right corners of a bounding box.
struct BoundingBox {
    int topLeftX = 0;
    int topLeftY = 0;
    int bottomRightX = 0;
    int bottomRightY = 0;
};

// Stores all of the fonts that may be used and their raw byte buffers.
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


// Stores all delimiting characters to format one or more glyphs.
//
// `ITALIC`: Text wrapped with this delimiter is written using an italicized version of the font.
//
// `BOLD`: Text wrapped with this delimiter is written using a bolded version of the font.
//  Note: This can be combined with the `ITALIC` delimiter to write text that is bold and italic.
//
// `TIMESTR`: The timestring part of the quote is automatically wrapped with this delimiter.
//  Note: A timestring should never be manually wrapped in the quote CSV file because it is
//      automatically found and wrapped when a quote is drawn.
struct CharacterDelimiters {
    std::string ITALIC  = "◻";  // U+25FB (White Medium Square)
    std::string BOLD    = "◯";  // U+25EF (Large Circle)
    std::string TIMESTR = "|";  // U+007C (Vertical Line)

    std::vector<std::string> getCharDelims() const {
        return { ITALIC, BOLD, TIMESTR };
    }
};

// Stores delimiting characters to format one or more words.
// 
// `NEWLINE`: Insert a newline between the current and succeeding text. (Equivalent to pressing the
//          enter/return key).
// `DOUBLE_NEWLINE`: Insert two newlines between the current and succeeding text. (Equivalent to pressing
//          the enter/return twice).
// TODO: Just use \n for both
struct WordDelimiters {
    std::string NEWLINE         = "␤";  // U+2424 (Symbol For Newline)
    std::string DOUBLE_NEWLINE  = "⇇";  // U+21C7 (Leftwards Paired Arrows

    std::vector<std::string> getWordDelims() const {
        return { NEWLINE, DOUBLE_NEWLINE };
    }
};

// Defines a formatting delimiter and a variable to track how many of the delimiter have been seen in the text.
//
// character: The delmiting character (e.g. "◻").
// count: How many times the character has been seen in the text.
struct Delimiter {
    std::string character;
    int count = 0;

    Delimiter(const std::string& c) : character(c) {}
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
    int x = 0; // X coordinate of the pen's position on the image.
    int y = 0; // Y coordinate of the pen's position on the image.
};


class Writer {
  public:
    BoundingBox bbox; // An area that the pen must write inside of.
    
    // The image to write a quote on, represented as a bitmap.
    //std::vector<unsigned char> image;

    // Generate an image of a single quote.
    //
    // row: A single row from the CSV file.
    // includeCredits: `true` to write quote's author and title in the bottom right of the image, `false` to discard.
    //
    // Returns a bitmap of the image.
    std::vector<unsigned char> getImage(std::unordered_map<std::string, std::string> row, bool includeCredits);
    
  private:  
    Pen pen;
    std::string text; // The text that the pen is writing.
    TextType textType = QUOTE; // is the text that is being written a quote or credits for a quote?
    Fonts fonts;
    std::vector<Delimiter> charDelimiters = {
        Delimiter(CharacterDelimiters().ITALIC),
        Delimiter(CharacterDelimiters().BOLD),
        Delimiter(CharacterDelimiters().TIMESTR),
    };

    // Move the pen to some (x,y) coordinate and and set all delimiter counters to 0.
    void resetPen (int x_pos, int y_pos)
    {
        pen.x = x_pos;
        pen.y = y_pos;
        for (Delimiter& d : charDelimiters) { d.count = 0; }
    }


    // Write text inside the bounding box of an image.
    //
    // image: Bitmap of the image to write on.
    // timestr: Optional substring within a quote that contains the time. Only passed in if the text being written is a quote.
    void writeInBBox(std::vector<unsigned char>& image, std::unordered_map<std::string, std::string> row);

    // Find the indices where the timestring begins and ends in a quote.
    //
    // row: A row from the CSV file.
    // timestrBegin: output parameter. Index where the first time string delim is found in the text.
    // timestrEnd: output parameter. Index where the second (last) time string delim is found in the text.
    //
    // Returns -1 if the timestring is not found, 0 on success.
    int findTimestrIndices(std::unordered_map<std::string, std::string> row, size_t& timestrBegin, size_t& timestrEnd)
    {
        if (row["timestring"].empty()) {
            return -1;
        }
        timestrBegin = toLower(row["quote"]).find(toLower(row["timestring"]));
        if (timestrBegin == std::string::npos) {
            return -1; // timestring not found
        }
        timestrEnd = timestrBegin + row["timestring"].size();
        return 0;
    }


    // Draws a word onto the image, one glyph at a time.
    //
    // image: Bitmap of the image to write on.
    // word: The word to be written.
    void drawWord(std::vector<unsigned char>& image, std::string word);


    // Determines which font and color should be used to write a character. If the character is a delimiter, an empty string is returned.
    //
    // pen: A pen to track changes to the character's font and color.
    // character: The character whose formatting is to be checked.
    //
    // Returns an empty string if `character` is a `CharacterDelimiter` or a `WordDelimiter`. Otherwise `character` is returned. 
    std::string formatChar(Pen& pen, std::string character);


    // Finds the maximum possible font scale (in font units) that can be used for a given bounding box and determiens how the text
    // should be wrapped to fit in the bbox horizontally.
    //
    // wrappedLines: output parameter. Stores the text broken up with newline delimiters to fit in the bbox horizontally.
    //      If the text cannot fit (the optimal font scale is < `MIN_FONT_SCALE`), this stores an empty string.
    //
    // Returns the optimal font scale that is found, or 0 if the text cannot fit
    void findOptimalFontScale(std::string& wrappedLines);


    // Helper to `findOptimalFontScale()`. Wraps text using a given font scale such that the text doesn't overflow past
    // the rightmost x coordinate of the bbox.
    //
    // pen: A temporary pen that is created and destroyed in `findOptimalFontScale()`.
    // 
    // Returns the text to be written with newline delimiters if it fits. Otherwise, an empty string is returned.
    std::string wrapText(Pen& pen);


    // Helper to `wrapText()`. Checks if a word needs to be moved onto a new line, either due to text wrapping
    // or custom formatting.
    //
    // pen: A temporary pen that is created and destroyed in `findOptimalFontScale()`.
    // word: The word to be formatted.
    // lines: The text to be written onto an image.
    // wordLength: The length of the word in pixels.
    void formatWord(Pen& pen, std::string word, std::vector<std::string>& lines, const int& wordLength);
    

    // Finds the vertical extent of the tallest glyph's ascender in a line of text, measured as pixels above the baseline.
    //
    // Rasterizes each character in the line, tracking the smallest top-left Y offset seen (glyphs are measured relative to
    // their baseline, so an offset of -n pixels means n pixels above the baseline).
    //
    // line: A single line of text, with no newline characters.
    //
    // Returns the top-left Y coordinate of the tallest (most negative) glyph box, or 0 if the line is empty.
    int maxAscender(const std::string& line);


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

    // Shrinks the credits bbox to fit tightly around its text, allowing the quote bbox to be enlarged and fill the blank space.
    //
    // Should be called after the optimal font scale for the credits text has been applied to `pen`, and after that text has been
    // wrapped (see `findOptimalFontScale` / `wrapText`). Measures the actual rendered width & height of `wrappedLines`
    // at the credits font scale and uses it to move the top-left corner of the credits bbox's inward so that the bbox's width and
    // height match the credits text's actual width and height.
    //
    // wrappedLines: The credits text, already wrapped with newline delimiters, measured at `pen`'s current font and font scale.
    //
    // Mutates: `bbox.topLeftX` and `bbox.topLeftY`, in place, on the calling Writer.
    void resizeCreditBbox(const std::string& wrappedLines);


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


    // Initalize a stb font.
    //
    // fontPath: File path to a TrueType or OpenType file.
    // outBuf: output parameter. A byte buffer of the font file. 
    // outFont: output parameter. A font profile used for font-related tasks, such as drawing a glyph.
    void initFont(const std::string& fontPath, std::vector<unsigned char>& outBuf, stbtt_fontinfo& outFont)
    {
        std::ifstream fontStream(fontPath, std::ios::binary); // read the entire font file into a buffer
        if (!fontStream) {
            throw std::runtime_error("Failed to open font file: " + fontPath);
        }

        outBuf = std::vector<unsigned char>(std::istreambuf_iterator<char>(fontStream), {});
        if (!stbtt_InitFont(&outFont, outBuf.data(), 0)) {
            throw std::runtime_error("stbtt_InitFont failed.");
        }
    }
};
