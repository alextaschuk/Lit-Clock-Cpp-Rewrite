/**
 * Codepoints (e.g. 65 for 'A') are rendered as glyphs. 
 * Glyph index: A font-specific int ID representing a glyph
 * Current Point: The origin of each character (where it is on the screen)
 * Vertical font metrics: Used to vertically space and position characters
 */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include <print>


#include "constants.hpp"
#include "writer.cpp"


void initFont(const std::string& fontPath, std::vector<unsigned char>& outBuf, stbtt_fontinfo& outFont)
{
    std::ifstream fontStream(fontPath, std::ios::binary);
    if (!fontStream) { throw std::runtime_error("Failed to open font file: " + fontPath); }

    outBuf = std::vector<unsigned char>(std::istreambuf_iterator<char>(fontStream), {});
    if (!stbtt_InitFont(&outFont, outBuf.data(), 0)) { throw std::runtime_error("stbtt_InitFont failed."); }
}


void formatWord(std::string word, std::vector<std::string>& lines, const int& wordLength, Pen& pen)
{
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&pen.font, &ascent, &descent, &lineGap);
    int lineHeight = static_cast<int>((ascent - descent + lineGap) * pen.fontScale);

    /* TODO: Add word formatting for delim chars*/
    if (pen.x + wordLength > pen.bbox.bottomRightX)
    { // Current word would go past right side of image. Put on next line.

        // Reset the x coord, move to the next line, and add the current word to the new line.
        pen.x = pen.bbox.topLeftX; // reset pen to left side of image
        
        // move down 1 line
        // (ascent - descent) is the glyph's height. 
        // lineGap is the font's recommended spacing between the bottom of one row's descent
        // and the top of the next row's ascent. Then, we need to scale it from font units to pixel units.
        //int ascent, descent, lineGap;
        //stbtt_GetFontVMetrics(&pen.font, &ascent, &descent, &lineGap);
        //pen.y += (ascent - descent + lineGap) * pen.fontScale;
        pen.y += lineHeight;
        lines.push_back(word);
    } 
    else
    {
        if (lines.empty()) {
            lines.push_back(word); // first word of the first line
        }
        else {
            lines.back() += " " + word; // add the current word to the current line
        }
    }
}

/**
 * Split a string of text into individual words. Modified from https://stackoverflow.com/a/14266139
 */
std::vector<std::string> split(std::string s, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;

    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    tokens.push_back(s);
    return tokens;
}

std::string wrapText(Pen& pen)
{
    std::vector<std::string> lines; // store wrapped text (e.g. ["this is a line", "this is another"])
    pen.x = pen.bbox.topLeftX;
    pen.y = pen.bbox.topLeftY;

    for (const std::string& word : split(pen.text, " "))
    {
        float wordLengthF = 0.0f;
        int wordLength = 0;
        for (size_t i = 0; i < word.size(); ++i)
        { /* Get the length of the word */
            char c = word[i];
            int advanceWidth, lsb;
            stbtt_GetCodepointHMetrics(&pen.font, c, &advanceWidth, &lsb); 
            wordLengthF += advanceWidth * pen.fontScale;

            if (i + 1 < word.size())
            {
                int kern = stbtt_GetCodepointKernAdvance(&pen.font, c, word[i + 1]);
                wordLengthF += kern * pen.fontScale;
            }
        }
        wordLength = static_cast<int>(wordLengthF);

        if (wordLength > pen.bbox.bottomRightX - pen.bbox.topLeftX)
        { // A single word cannot be longer than the bbox's width
            pen.reset(pen.bbox.topLeftX, pen.bbox.topLeftY);
            return "";
        }

        // add the length of a space after each word
        int advanceWidth;
        stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, 0); 
        wordLength += advanceWidth * pen.fontScale;
        formatWord(word, lines, wordLength, pen);

        pen.x += wordLength;

        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&pen.font, &ascent, &descent, &lineGap);
        int lineHeight = static_cast<int>((ascent - descent + lineGap) * pen.fontScale);

        if (pen.y + lineHeight > pen.bbox.bottomRightY)
        { /* current wrapping writes past text's bbox. Need to reduce font size. */
            //std::cout << "pen's y pos is past bbox bottom right." << std::endl;
            pen.reset(pen.bbox.topLeftX, pen.bbox.topLeftY);
            return "";
        }
    }

    std::string wrapped;
    for (const auto& line : lines) {
        wrapped += line + "\n"; // e.g. ["It is", "12:00 P.M."] -> "It is\n12:00 P.M.\n"
    }
    if (!wrapped.empty())
    {
        wrapped.pop_back(); // remove the extra '\n' at the end of the string
    }
    //std::println("wrapped:{}\n", wrapped);
    return wrapped;
}

void findOptimalFontScale(Pen& pen, std::string& wrappedLines)
{
    float min = MIN_FONT_SCALE;
    float max = MAX_FONT_SCALE;
    float optimalScale = 0.0f;

    while (min <= max)
    { /* Binary search to find best font size. */
        float mid = std::floor(min + (max - min) / 2);
        std::string lines;

        //std::cout << " min size:" << min << std::endl;
        //std::cout << " mid size:" << mid << std::endl;
        //std::cout << " max size:" << max << std::endl;
        pen.fontScale = stbtt_ScaleForPixelHeight(&pen.font, mid);

        lines = wrapText(pen);
        //std::cout << "Lines: " << lines << std::endl;
        if (!lines.empty())
        { // Text fits. try a larger font scale
            optimalScale = mid;
            min = mid + 1;
            wrappedLines = lines;
        }
        else
        { /* Text didn't fit. */
            max = mid - 1;
        }
    }
    
    if (optimalScale < MIN_FONT_SCALE){
        std::println("Text doesn't fit in the pen's bbox.");
    }
    else {
        pen.fontScale = stbtt_ScaleForPixelHeight(&pen.font, optimalScale);
    }

    float linesHeight, longestLineWidth = 0;
    for (const std::string& line : split(wrappedLines, "\n"))
    {
        float currentLineWidth = 0;
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&pen.fonts.regular, &ascent, &descent, &lineGap);

        linesHeight += (ascent - descent + lineGap) * pen.fontScale;

        for (const char c : line)
        {
            int advanceWidth;
            stbtt_GetCodepointHMetrics(&pen.font, c, &advanceWidth, 0); 
            currentLineWidth += advanceWidth * pen.fontScale;
        }
        if (longestLineWidth < currentLineWidth) {
            longestLineWidth = currentLineWidth;
        }
    }
    if (pen.textType == CREDITS)
    { /* Resize the credit bbox to optimize how big the quote bbox is. */
        pen.bbox.topLeftX = pen.bbox.bottomRightX - longestLineWidth;
        pen.bbox.topLeftY = pen.bbox.bottomRightY - linesHeight;
    }

}

int formatChar(char& c, Pen pen)
{
    //TODO
    return 1;
}

void drawWord(std::vector<unsigned char>& image, std::string word, Pen& pen)
{   
    int advanceWidth, lsb;
    int imgStride = pen.bbox.bottomRightX;

    for (size_t i = 0; i < word.length(); ++i)
    {
        char c = word[i];
        // TODO: format char
        if (formatChar(c, pen) == 0) continue;

        stbtt_GetCodepointHMetrics(&pen.font, c, &advanceWidth, &lsb);

        BoundingBox glyphBox; // a bbox around a single glyph
        stbtt_GetCodepointBitmapBox(&pen.font, c, pen.fontScale, pen.fontScale, &glyphBox.topLeftX, &glyphBox.topLeftY, &glyphBox.bottomRightX, &glyphBox.bottomRightY); // rasterize glyph c
        int glyphWidth = glyphBox.bottomRightX - glyphBox.topLeftX; // glyph's x offset (in pixel coords from 
        int glyphHeight = glyphBox.bottomRightY - glyphBox.topLeftY; // glyph's pixel height

        float drawX = pen.x + glyphBox.topLeftX; // pixel x coord where the glyph starts (topLeftX and topLeftY can be negative)
        float drawY = pen.y + glyphBox.topLeftY; // pixel y coord where the glyph starts (pen.y must be the text baseline, not the top edge of the bbox)

        if (drawX < pen.bbox.topLeftX)
        {
            float shiftX = pen.bbox.topLeftX - drawX;
            pen.x += shiftX;
            drawX += shiftX;
        }

        if (drawY < pen.bbox.topLeftY)
        {
            float shiftY = pen.bbox.topLeftY - drawY;
            pen.y += shiftY;
            drawY += shiftY;
        }

        // make a temp buf for the current glyph (this is called blitting)
        std::vector<unsigned char> glyphBuf(glyphWidth * glyphHeight, QUOTE_COLOR);
        stbtt_MakeCodepointBitmap(&pen.font, glyphBuf.data(), glyphWidth, glyphHeight, glyphWidth, pen.fontScale, pen.fontScale, c);
        
        // copy (blit) the glyph's buffer onto the image's buffer pixel by pixel
        for (int row = 0; row < glyphHeight; ++row) {
            for (int col = 0; col < glyphWidth; ++col) {
                int destX = static_cast<int>(drawX + col);
                int destY = static_cast<int>(drawY + row);

                if (destX >= pen.bbox.topLeftX && destX < pen.bbox.bottomRightX &&
                    destY >= pen.bbox.topLeftY && destY < pen.bbox.bottomRightY)
                {
                    unsigned char glyphPixel = glyphBuf[row * glyphWidth + col];
                    if (glyphPixel > 0) {
                        int pixelIdx = destY * imgStride + destX;
                        image[pixelIdx] = 255 - glyphPixel;
                    }
                }
            }
        }

        pen.x += (advanceWidth * pen.fontScale);

        if (i + 1 < word.length())
        {
            int kern = stbtt_GetCodepointKernAdvance(&pen.font, c, word[i + 1]);
            pen.x += kern * pen.fontScale;
        }
    }
    
    // add the length of a space after each word
    stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, &lsb);
    pen.x += advanceWidth * pen.fontScale;

    // TODO: check if any char delims >= 2
}


void writeInBBox(std::vector<unsigned char>& image, Pen& pen, std::string timestr = "")
{
    if (pen.textType == QUOTE && !timestr.empty())
    {
        // TODO: check for timestr and wrap with '|'
    }

    std::string wrappedLines;
    findOptimalFontScale(pen, wrappedLines);
    //std::println("wrappedLines: {}", wrappedLines);

    pen.y = pen.bbox.topLeftY;
    //std::println("wrappedLines split: {}", wrappedLines);
    for (const std::string& line : split(wrappedLines, "\n"))
    {
        pen.x = pen.bbox.topLeftX;
        for (const std::string& word : split(line, " "))
        {
            drawWord(image, word, pen);
        }
        // move to the next line and continue drawing
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&pen.fonts.regular, &ascent, &descent, &lineGap);
        pen.y += (ascent - descent + lineGap) * pen.fontScale;
    }
}


void textToImage(Row row, bool includeCredits, Pen& pen)
{
    std::vector<unsigned char> image(SCREEN_WIDTH * SCREEN_HEIGHT, BG_COLOR);

    const float SCALE_MULTIPLIER = 0.99; // to constrain bboxes to make sure text fits

    Fonts fonts;
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly.ttf", fonts.regularBuf, fonts.regular);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Italic.ttf", fonts.italicBuf, fonts.italic);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Bold.ttf", fonts.boldBuf, fonts.bold);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Bold-Italic.ttf", fonts.italicBoldBuf, fonts.italicBold);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Bold.ttf", fonts.creditBuf, fonts.credit);

    pen.fonts = fonts;
    pen.font = fonts.regular;

    BoundingBox quoteBBox;
    quoteBBox.topLeftX =  static_cast<int>(std::floor(SCREEN_WIDTH - SCREEN_WIDTH * SCALE_MULTIPLIER));
    quoteBBox.topLeftY = static_cast<int>(std::floor(SCREEN_HEIGHT - SCREEN_HEIGHT * SCALE_MULTIPLIER));
    quoteBBox.bottomRightX = SCREEN_WIDTH;
    quoteBBox.bottomRightY = static_cast<int>(std::floor(SCREEN_HEIGHT * SCALE_MULTIPLIER));

    if (includeCredits)
    {
        pen.textType = CREDITS;
        BoundingBox creditsBBox;
        creditsBBox.topLeftX =  static_cast<int>(std::floor(SCREEN_WIDTH * 0.45));
        creditsBBox.topLeftY = static_cast<int>(std::floor(SCREEN_HEIGHT * 0.85));
        creditsBBox.bottomRightX = SCREEN_WIDTH;
        creditsBBox.bottomRightY = static_cast<int>(std::floor(SCREEN_HEIGHT * SCALE_MULTIPLIER));
        pen.bbox = creditsBBox;
        //std::println("{}", pen.bbox.topLeftX);
        //std::println("{}", pen.bbox.topLeftY);
        //std::println("{}", pen.bbox.bottomRightX);
        //std::println("{}", pen.bbox.bottomRightY);

        // TODO: format credits properly 
        pen.text = "—" + row.title + ", " + row.author;
        writeInBBox(image, pen);
        quoteBBox.bottomRightY = static_cast<int>(std::floor(pen.bbox.topLeftY * SCALE_MULTIPLIER));
    }

    pen.textType = QUOTE;
    pen.bbox = quoteBBox;
    pen.text = row.quote;
    writeInBBox(image, pen, row.timestring);
    pen.reset(pen.bbox.topLeftX, pen.bbox.topLeftY);

    std::string outPath = "out.png"; // TODO replace with logic to properly name image files.
    stbi_write_png(outPath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, image.data(), SCREEN_WIDTH);
}

int main() {
    std::cout << "MAIN STARTED" << std::endl;  // temporary
    Pen drawing_pen;
    Row row;
    row.time = "01:17";
    row.timestring = "1:17";
    row.quote = "Jessica heard the disturbance in the great hall, turned on the light beside her bed. The clock there had not been properly adjusted to local time, and she had to subtract twenty-one minutes to determine that it was about 2 A.M.";
    row.title = "The Road";
    row.author = "Cormac McCarthy";
    // CSV Parser: https://github.com/ben-strasser/fast-cpp-csv-parser
    textToImage(row, true, drawing_pen);
    return 0;
}