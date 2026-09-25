#pragma once

#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "spdlog/spdlog.h"
#include "stb/stb_truetype.h"

#include "delimiter.hpp"
#include "utils.hpp"


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


// Tells the pen if it is writing a quote or a quote's credits.
enum TextType {
    QUOTE,
    CREDITS // author and book title
};

// Stores information used to write a glyph on an image. It tracks the image's bounds, where on the image the glyph should be written, the
// font used to write the glyph, the font's scale, and the color of the glyph.
struct Pen {
    stbtt_fontinfo font;
    float fontScale; // value used to convert from font units to pixel units
    short int color = 128; // grayscale value (0-255) to draw text with.
    int x = 0; // X coordinate of the pen's position on the image.
    int y = 0; // Y coordinate of the pen's position on the image.
    
    // TODO: make static and move to formatChar
    bool pendingEscape = false; // insert a backslash before a delimiter to treat it as a normal character.
};


class Writer {
  public:
    BoundingBox bbox; // An area that the pen must write inside of.
    Fonts fonts; // Stores all fonts that could be used to write a quote or its credits.

    // Generate an image of a single quote.
    //
    // row: A single row from the CSV file.
    // includeCredits: `true` to write quote's author and title in the bottom right of the image, `false` to discard.
    //
    // Returns a bitmap of the image as a byte buffer.
    std::vector<unsigned char> generateQuoteImage(std::unordered_map<std::string, std::string> row, const bool& includeCredits);

    // Saves all quote images to an `images/` directory.
    //
    // Each row in the CSV is parsed to create an image of each quote.
    void saveImages();


    // Initalizes a stb font.
    //
    // fontPath: File path to a TrueType or OpenType file.
    // outBuf: output parameter. A byte buffer of the font file. 
    // outFont: output parameter. A font profile used for font-related tasks, such as drawing a glyph.
    void initFont(const std::string& fontPath, std::vector<unsigned char>& outBuf, stbtt_fontinfo& outFont) {
        std::ifstream fontStream(fontPath, std::ios::binary); // read the entire font file into a buffer
        if (!fontStream) {
            throw std::runtime_error("Failed to open font file: " + fontPath);
        }

        outBuf = std::vector<unsigned char>(std::istreambuf_iterator<char>(fontStream), {});
        if (!stbtt_InitFont(&outFont, outBuf.data(), 0)) {
            throw std::runtime_error("stbtt_InitFont failed.");
        }
    }
    
  private:  
    Pen pen; // The pen to write the text with.
    std::string text; // The text that the pen is writing.
    TextType textType = QUOTE; // Tells the writer if the current text is a quote or credits for a quote.

    // An array of `Delimiters` that are used to format one or more characters in a text.
    std::array<Delimiter, static_cast<int>(DelimiterType::Count)> charDelimiters = { 
    Delimiter{DelimiterType::Italic, CharacterDelimiters().ITALIC},
    Delimiter{DelimiterType::Bold, CharacterDelimiters().BOLD},
    Delimiter{DelimiterType::Time, CharacterDelimiters().TIMESTR},
    Delimiter{DelimiterType::EndOfLine, CharacterDelimiters().ENDOFLINE},
    };


    // Writes text inside the bounding box of an image.
    //
    // image: Bitmap of the image to write on.
    // timestr: Optional substring within a quote that contains the time. Only passed in if the text being written is a quote.
    void writeInBBox(std::vector<unsigned char>& image, std::unordered_map<std::string, std::string> row);


    // A helper to `writeInBBox()` that wraps `Writer::text` with the timestring delimiter, or replaces it with an error message if the
    // timestring is missing or is not found in the quote.
    //
    // row: A row from the CSV file.
    // timestrBegin: output parameter. Index where the first time string delim is found in the text.
    // timestrEnd: output parameter. Index where the second (last) time string delim is found in the text.
    // text: output parameter. Wraps the quote's substring containing the timestring with the timestring delim, and an error message otherwise.
    void wrapTimestring(std::unordered_map<std::string, std::string> row, std::string& text) {
        size_t timestrBegin = 0, timestrEnd = 0;
        const std::string errMsg = std::format("Time string not found in quote starting with \"{}...\"", row["quote"].substr(0,50));

        timestrBegin = toLower(row["quote"]).find(toLower(row["timestring"]));
        if (timestrBegin == std::string::npos)
        {
            spdlog::warn("{}", errMsg);
            text = std::format("*Error*: {}", errMsg);   
        }
        else
        {
            timestrEnd = timestrBegin + row["timestring"].size();
            std::string delim = CharacterDelimiters().TIMESTR;
            text = row["quote"].substr(0, timestrBegin);
            text += delim + row["quote"].substr(timestrBegin, timestrEnd - timestrBegin) + delim;
            text += row["quote"].substr(timestrEnd);
        }
    }


    // Draws a word onto the image, one character at a time.
    //
    // image: Bitmap of the image to write on.
    // word: The word to be written.
    // isLast: `true` if the word is the last word of a line (to skip adding a space after the last word in a line)
    void drawWord(std::vector<unsigned char>& image, std::string word, bool isLast);


    // Finds the maximum possible pixel height that can be used for a given bounding box and determiens how the text
    // should be wrapped to fit in the bbox horizontally.
    //
    // wrappedLines: output parameter. Stores the text broken up with newline delimiters to fit in the bbox horizontally.
    //      If the text cannot fit (the optimal font scale is < `MIN_PIXEL_HEIGHT`), this stores an empty string.
    //
    // Returns the optimal font scale that is found, or -1 if the text cannot fit.
    float findOptimalPixelHeight(std::string& wrappedLines);


    // A helper to `findOptimalPixelHeight()` that Wraps text using a given font scale such that the text doesn't overflow past
    // the rightmost x coordinate of the bbox.
    //
    // pen: A temporary pen that is created and destroyed in `findOptimalPixelHeight()`.
    // 
    // Returns the text to be written with newline delimiters if it fits. Otherwise, an empty string is returned.
    std::string wrapText(Pen& pen);


    // Determines which font and color should be used to write a character. If the character is a delimiter, an empty string is
    // returned.
    //
    // pen: A pen to track changes to the character's font and color.
    // character: output parameter. The character whose formatting is to be checked.
    // `pendingEscape` should be true (since it is only true for escaping delimiters).
    //
    // Returns an empty string if `character` is a `CharacterDelimiter` or the escape character ("\\"). Otherwise `character` is returned. 
    std::string formatChar(Pen& pen, std::string character);


    // A helper to `wrapText()` that checks if a word needs to be moved onto a new line, either due to text wrapping
    // (it doesn't fit on the current line) or custom formatting (contains one or more "\n").
    //
    // pen: A temporary pen that is created and destroyed in `findOptimalPixelHeight()`.
    // word: The word to be formatted.
    // lines: The text to be written onto an image.
    // wordLength: The length of the word in pixels.
    void formatWord(Pen& pen, std::string word, std::string& lines, const int& wordLength);


    // Retrieves a Delimiter from `charDelimiters` using the passed in type.
    Delimiter& getDelimiter(DelimiterType type) {
        return charDelimiters[static_cast<size_t>(type)];
    }


    // Finds which delimiter a character represents. If the character is not a delimiter,
    // a nullptr is returned.
    Delimiter* findDelimiter(const std::string& character) {
        for (auto& delim : charDelimiters) {
            if (delim.character == character) return &delim;
        }
        return nullptr;
    }


    // Sets the count values in all delimiters to 0.
    void resetDelimCount() {
        for (Delimiter& d : charDelimiters) { d.count = 0; }
    }


    // Moves the pen to some (x,y) coordinate.
    void resetPen (int x_pos, int y_pos) {
        pen.x = x_pos;
        pen.y = y_pos;
    }


    // Calculates a glyph's advance width.
    //
    // codepoint: A decoded Unicode codepoint in base 10.
    // font: The font that a pen uses (pen.font) to draw glyphs.
    // fontScale: A pen's font scale (pen.fontScale) to convert from font units to pixel units
    //
    // Returns the product of the advance width retrieved from stbtt_GetCodepointHMetrics
    // and a pen's font scale.
    float getAdvanceWidth( const int& codepoint, const stbtt_fontinfo& font, const float& fontScale)
    {
        int advanceWidth = 0;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advanceWidth, 0); 
        return advanceWidth * fontScale;
    }
    

    // Finds the vertical extent of the tallest glyph's ascender in a line of text, measured as pixels above the baseline.
    //
    // Rasterizes each character in the line, tracking the smallest top-left Y offset seen (glyphs are measured relative to
    // their baseline, so an offset of -n pixels means n pixels above the baseline).
    //
    // line: A single line of text, with no newline characters.
    //
    // Returns the top-left Y coordinate of the tallest (most negative) glyph box, or 0 if the line is empty.
    int maxAscender(const std::string& line);


    // Calculates a font's recommended distance between two lines of text (the vertical distance from one line's baseline to
    // the next line's baseline) in pixels, at a given font scale.
    //
    // font: The font whose line spacing is being measured.
    // fontScale: The font's scale to convert the spacing units from font to pixel.
    //
    // Returns the font's recommended pixel distance to increase the pen's Y coordinate by to move to the
    // next line.
    int getLineHeight(const stbtt_fontinfo& font, const float& fontScale)
    {
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
        // (ascent - descent) is the height of the font's tallest glyph.
        // lineGap is the font's recommended spacing between the bottom of one row's descent and the top
        // of the next row's ascent.
        return static_cast<int>((ascent - descent + lineGap) * fontScale);
    }

    // Calculates a line of text's width.
    //
    // pen: 
    // line: A single line of text, one or more words long.
    float getLineWidth(Pen& pen, const std::string& line);


    // Shrinks the credits bbox to fit tightly around its text, allowing the quote bbox to be enlarged and fill the blank space.
    //
    // Should be called after the optimal font scale for the credits text has been applied to `pen`, and after that text has been
    // wrapped (see `findOptimalPixelHeight` / `wrapText`). Measures the actual rendered width & height of `wrappedLines`
    // at the credits font scale and uses it to move the top-left corner of the credits bbox's inward so that the bbox's width and
    // height match the credits text's actual width and height.
    //
    // wrappedLines: The credits text, already wrapped with newline delimiters, measured at `pen`'s current font and font scale.
    //
    // Mutates: `bbox.topLeftX` and `bbox.topLeftY`, in place, on the calling Writer.
    void resizeCreditBbox(const std::string& wrappedLines);
};
