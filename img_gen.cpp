#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include <print>

#include "constants.hpp"
#include "writer.cpp"

/**
 * Split a string of text into individual words. Source: https://stackoverflow.com/a/14266139
 */
std::vector<std::string> split(std::string s, const std::string& delimiter) {
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

void initFont(const std::string& fontPath, std::vector<unsigned char>& outBuf, stbtt_fontinfo& outFont)
{
    std::ifstream fontStream(fontPath, std::ios::binary);
    if (!fontStream) { throw std::runtime_error("Failed to open font file: " + fontPath); }

    outBuf = std::vector<unsigned char>(std::istreambuf_iterator<char>(fontStream), {});
    if (!stbtt_InitFont(&outFont, outBuf.data(), 0)) { throw std::runtime_error("stbtt_InitFont failed."); }
}


/**
 * stb expects a unicode codepoint (a unique ID for unicode characters).
 * Strings store raw bytes as data, and they are encoded as UTF-8. All 
 * ASCII characters are 1 byte large, so grabbing them from a string by 
 * index (e.g., `char c = word[i]`) works. However, there are non-ASCII
 * UTF-8 characters that are encoded as multiple bytes.
 * - For example an em dash is encoded as 3 bytes: `0xE2 0x80 0x94`.
 */
int decodeUTF8(const std::string& s, size_t i, int& numBytes)
{
    unsigned char c = s[i];

    /**
     * How the check works:
     * The leading bits of the byte tell us how many bytes
     * are used to make the entire character. We perform an
     * AND operation on the character and a mask to determine
     * how many bytes make up the character. 
     * 
     * The return statements:
     * Will use a 2-byte char as an example. The leading bits of
     * the first byte contain how many bytes make up the char (110xxxxx),
     * so there are 5 bits that are available to be used for data. The last 
     * byte is always the continuation byte. The two leading bits of the 
     * continuation byte are always 10yyyyyy, so there are 6 bits available
     * to be used for data in this byte. 11 bits in total.
     * 
     * First, we do the opposite of the check in the if statement: c & 0x1F.
     * This zeroes out the leading bits that tell us how many bytes the char is.
     * E.g. (110xxxxx & 0x1F) -> 000xxxxx This gives us only the 5 data bits.
     * 
     * Then we shift byte 0's bits over 6 spots to make room for the 6 bits of data
     * that are in the continuation byte. (110xxxxx & 0x1F) << 6 -> 000xxxxx000000
     * 
     * Then we extract the continuation byte's data: s[i + 1] & 0x3F
     * 
     * Lastly merge the two pieces together with an OR operation: ((110xxxxx & 0x1F) << 6) | (s[i+1] & 0x3F)
     */
    if (c < 0x80) // 0xxxxxxx for 1 byte (ASCII character)
    {
        numBytes = 1;
        return c;
    }
    else if ((c & 0xE0) == 0xC0)  // 110xxxxx for 2 bytes
    {
        numBytes = 2;
        int byteZero = (c & 0x1F) << 6;
        int byteOne = s[i + 1] & 0x3F;
        return byteZero | byteOne;
        //return ((c & 0x1F) << 6) | (s[i+1] & 0x3F);
    }
    else if ((c & 0xF0) == 0xE0) // 1110xxxx for 3 bytes
    {
        numBytes = 3;
        int byteZero = (c & 0x0F) << 12;
        int byteOne = (s[i+1] & 0x3F) << 6;
        int byteTwo = s[i+2] & 0x3F;
        return byteZero | byteOne | byteTwo;
        //return ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
    }
    else if ((c & 0xF8) == 0xF0) // 11110xxx for 4 bytes
    {
        numBytes = 4;
        int byteZero = ((c & 0x07) << 18);
        int byteOne = (s[i+1] & 0x3F) << 12;
        int byteTwo = (s[i+2] & 0x3F) << 6 ;
        int byteThree = s[i+3] & 0x3F;
        return byteZero | byteOne | byteTwo | byteThree;
        //return ((c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
    }

    numBytes = 1; // the byte is malformed so we should skip it.
    return c;

}


/**
 * Calculate the height of a line using a given font scale.
 * (ascent - descent) is the glyph's height.
 * lineGap is the font's recommended spacing between the bottom of one row's descent and the top of the next row's ascent.
 * then we need to scale it from font units to pixel units.
 */
int getLineHeight(const stbtt_fontinfo& font, const float& fontScale)
{
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    return static_cast<int>((ascent - descent + lineGap) * fontScale);
}

/**
 * For a given line of text, find the height of the tallest 
 * glyph.
 */
int tallestGlyph(const std::string& line, const Pen& pen)
{
    int maxHeight = 0;
    for (const std::string& word : split(line, " "))
    {
        for (size_t i = 0; i < word.size(); )
        {
            int numBytes;
            int codepoint = decodeUTF8(word, i, numBytes);
            i += numBytes;

            BoundingBox glyphBox;
            stbtt_GetCodepointBitmapBox(&pen.font, codepoint, pen.fontScale, pen.fontScale, &glyphBox.topLeftX, &glyphBox.topLeftY, &glyphBox.bottomRightX, &glyphBox.bottomRightY);
            maxHeight = (glyphBox.topLeftY < maxHeight) ? glyphBox.topLeftY : maxHeight;
        }
    }
    return maxHeight;
}




void formatWord(std::string word, std::vector<std::string>& lines, const int& wordLength, Pen& pen)
{
    /* TODO: Add word formatting for delim chars*/
    if (pen.x + wordLength > pen.bbox.bottomRightX)
    { // Current word would go past right side of image. Put on next line.
        pen.x = pen.bbox.topLeftX; // reset pen to left side of image
        pen.y += getLineHeight(pen.font, pen.fontScale); // move down 1 line
        lines.push_back(word); // add the current word to the new line
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

std::string wrapText(Pen& pen)
{
    std::vector<std::string> lines; // store wrapped text (e.g. ["this is a line", "this is another"])
    pen.x = pen.bbox.topLeftX;
    pen.y = pen.bbox.topLeftY;

    for (const std::string& word : split(pen.text, " "))
    {
        float wordLengthF = 0.0f;
        int wordLength = 0;
        for (size_t i = 0; i < word.size(); )
        { /* Get the length of the word */
            int numBytes;
            int codePoint = decodeUTF8(word, i, numBytes);
            i += numBytes;

            int advanceWidth, lsb;
            stbtt_GetCodepointHMetrics(&pen.font, codePoint, &advanceWidth, &lsb); 
            wordLengthF += advanceWidth * pen.fontScale;

            if (i + 1 < word.size())
            {
                int kern = stbtt_GetCodepointKernAdvance(&pen.font, codePoint, word[i + 1]);
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

        int lineHeight = getLineHeight(pen.font, pen.fontScale);
        if (pen.y + lineHeight > pen.bbox.bottomRightY)
        { /* current wrapping writes past text's bbox. Need to reduce font size. */
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

    /* Binary search to find best font size. */
    while (min <= max)
    {
        float mid = std::floor(min + (max - min) / 2);
        pen.fontScale = stbtt_ScaleForPixelHeight(&pen.font, mid);
        
        std::string lines;
        lines = wrapText(pen);
        if (!lines.empty())
        { /* Text fits. try a larger font scale */
            optimalScale = mid;
            min = mid + 1;
            wrappedLines = lines;
        }
        else
        { /* Text didn't fit. */
            max = mid - 1;
        }
    }
    
    if (optimalScale < MIN_FONT_SCALE) {
        std::println("Text doesn't fit in the pen's bbox.");
    }
    else {
        pen.fontScale = stbtt_ScaleForPixelHeight(&pen.font, optimalScale);
    }

    float linesHeight, longestLineWidth = 0;
    for (const std::string& line : split(wrappedLines, "\n"))
    {
        float currentLineWidth = 0;
        linesHeight += getLineHeight(pen.font, pen.fontScale);

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
        pen.bbox.topLeftX = static_cast<int>((pen.bbox.bottomRightX - longestLineWidth) * SCALE_MULTIPLIER);
        pen.bbox.topLeftY = static_cast<int>((pen.bbox.bottomRightY - linesHeight) * SCALE_MULTIPLIER);
    }

}


int formatChar(int& c, Pen pen)
{
    //TODO
    return 1;
}

void drawWord(std::vector<unsigned char>& image, std::string word, Pen& pen)
{   
    for (size_t i = 0; i < word.length(); )
    {
        int numBytes;
        int codepoint = decodeUTF8(word, i, numBytes);
        i += numBytes;
        // TODO: format char
        if (formatChar(codepoint, pen) == 0) { i += numBytes; continue; }

        int advanceWidth; // how far the pen should move after drawing a glyph (in font units)
        stbtt_GetCodepointHMetrics(&pen.font, codepoint, &advanceWidth, 0);

        /**
         * We get a bbox around a single glyph's rendered ink, relative to the glyph's origin. (not the full em-square/advance-width box)
         * pen.x/pen.y track where the cursor is on the image. Specifically, pen.y tracks where the glyph's baseline is.
         * Example for glyph 'A':
         *  glyphBox.topLeftX  = 1      // ink starts 1px right of the origin
         *  glyphBox.topLeftY  = -18    // ink starts 18px above the baseline
         *  glyphBox.bottomRightX = 15
         *  glyphBox.bottomRightY = 0   // ink ends right at the baseline (for something like 'j' this would be negative.)
         */
        BoundingBox glyphBox;
        stbtt_GetCodepointBitmapBox(&pen.font, codepoint, pen.fontScale, pen.fontScale, &glyphBox.topLeftX, &glyphBox.topLeftY, &glyphBox.bottomRightX, &glyphBox.bottomRightY); // rasterize glyph c
        int glyphWidth = glyphBox.bottomRightX - glyphBox.topLeftX;
        int glyphHeight = glyphBox.bottomRightY - glyphBox.topLeftY;

        /**
         * (drawX, drawY) is the coordinate on the image where the top-left of the glyph's bbox should be placed.
         * TODO: come up with better variable names
         */
        float drawX = pen.x + glyphBox.topLeftX;
        float drawY = pen.y + glyphBox.topLeftY;

        /**
         * Handle case when a glyph's ink starts outside the left of 
         * or above the image's bounding box. Necessary since glyphWidth
         * and glyphHeight can be negative.
         */
        if (drawX < pen.bbox.topLeftX)
        {
            float shiftX = pen.bbox.topLeftX - drawX;
            pen.x += shiftX; // move the pen right so that the glyph fits.
            drawX += shiftX;
        }

        if (drawY < pen.bbox.topLeftY)
        {
            float shiftY = pen.bbox.topLeftY - drawY;
            pen.y += shiftY; // move the pen down so that the glyph fits.
            drawY += shiftY;
        }

        /**
         * Make a temporary buffer for the rasterized glyph, then copy it 
         * onto the image (aka blitting).
         */
        std::vector<unsigned char> glyphBuf(glyphWidth * glyphHeight, 0);
        stbtt_MakeCodepointBitmap(&pen.font, glyphBuf.data(), glyphWidth, glyphHeight, glyphWidth, pen.fontScale, pen.fontScale, codepoint);
        
        for (int row = 0; row < glyphHeight; ++row) {
            for (int col = 0; col < glyphWidth; ++col) {
                int destX = static_cast<int>(drawX + col); // The rasterized glyph's x coordinate of the current pixel
                int destY = static_cast<int>(drawY + row); // The rasterized glyph's y coordinate of the current pixel

                // make sure the pixel fits
                if (destX >= pen.bbox.topLeftX && destX < pen.bbox.bottomRightX &&
                    destY >= pen.bbox.topLeftY && destY < pen.bbox.bottomRightY)
                {
                    unsigned char glyphPixel = glyphBuf[row * glyphWidth + col]; // foreground pixel
                    if (glyphPixel > 0) // only want non-background pixels
                    {
                        /**
                         * alpha = (object's opacity * its pixel coverage)
                            * an obj that is 60% opaque and covers 30% of a pixel's 
                            * area has an alpha value of 18% in that pixel.
                            * "alpha" and "opacity" are often synonomous.
                         * use linear interpolation between two colors, weighted by
                         * an alpha value to determine the pixel's color.
                         * Alpha compositing is generally: result = foreground * alpha + background * (1 - alpha)
                            * alpha is usually normalized to [0, 1] but we are using 8-bit space here (TODO: reduce to 4-bit?)
                            * so we use a range of [0, 255] instead. 
                            * glyphPixel (0-255) plays the role of alpha * 255.
                            * (255 - glyphPixel) plays the role of (1 - alpha * 255)
                            * dividing the sum by 255 at the end normalizes it to [0, 255].
                         */
                        int pixelIdx = destY * pen.bbox.bottomRightX + destX; // this converts the 2D coords of the pixel into a 1D array index
                        int backgroundPixel = image[pixelIdx];
                        image[pixelIdx] = (pen.color * glyphPixel + backgroundPixel * (255 - glyphPixel)) / 255;
                    }
                }
            }
        }

        pen.x += (advanceWidth * pen.fontScale); // move the pen to the right for the next glyph.

        if (i + 1 < word.length())
        { // apply any necessary kerning as well
            int kerningAdvance = stbtt_GetCodepointKernAdvance(&pen.font, codepoint, word[i + 1]);
            pen.x += kerningAdvance * pen.fontScale;
        }
    }
    
    // add the length of a space after each word
    int advanceWidth;
    stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, 0);
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
    pen.reset(pen.bbox.topLeftX, pen.bbox.topLeftY); // TODO: fix call to findOptimalFontScale so that it doesn't modify pen's x and y val
    std::vector<std::string> lines = split(wrappedLines, "\n");
    
    if (pen.textType == CREDITS)
    {
        pen.bbox.bottomRightY = static_cast<int>((SCREEN_HEIGHT + getLineHeight(pen.font, pen.fontScale)) * SCALE_MULTIPLIER);
    }
    else
    {
        int firstLineHeight = tallestGlyph(lines[0], pen);
        //pen.y -= firstLineHeight; // shift baseline down just enough for this line's actual tallest glyph
    }

    for (const std::string& line : lines)
    {
        pen.x = pen.bbox.topLeftX;
        int lineHeight = tallestGlyph(line, pen);
        pen.y -= lineHeight;
        for (const std::string& word : split(line, " "))
        {
            drawWord(image, word, pen);
        }
        pen.y += getLineHeight(pen.font, pen.fontScale) + lineHeight; // move to the next line and continue drawing
        //pen.y += getLineHeight(pen.font, pen.fontScale); // move to the next line and continue drawing
    }
}


void textToImage(Row row, bool includeCredits, Pen& pen)
{
    std::vector<unsigned char> image(SCREEN_WIDTH * SCREEN_HEIGHT, BG_COLOR);

    Fonts fonts;
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly.ttf", fonts.regularBuf, fonts.regular);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Italic.ttf", fonts.italicBuf, fonts.italic);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Bold.ttf", fonts.boldBuf, fonts.bold);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Bold-Italic.ttf", fonts.italicBoldBuf, fonts.italicBold);
    initFont(std::string(PROJECT_ROOT) + "/fonts/Bookerly-Bold.ttf", fonts.creditBuf, fonts.credit);

    pen.fonts = fonts;
    pen.font = fonts.regular;

    /* leave some room around the screen so that text isn't written right up to its edges.*/
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
        pen.color = CREDIT_COLOR;
        pen.text = "—" + row.title + ", " + row.author; // TODO: format credits properly 
        writeInBBox(image, pen);
        quoteBBox.bottomRightY = static_cast<int>(std::floor(pen.bbox.topLeftY * SCALE_MULTIPLIER));
    }

    pen.textType = QUOTE;
    pen.bbox = quoteBBox;
    pen.text = row.quote;
    pen.color = QUOTE_COLOR;
    writeInBBox(image, pen, row.timestring);
    pen.reset(pen.bbox.topLeftX, pen.bbox.topLeftY);

    std::string outPath = "out.png"; // TODO replace with logic to properly name image files.
    stbi_write_png(outPath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, image.data(), SCREEN_WIDTH);
}

int main() {
    Pen drawing_pen;
    Row row;
    row.time = "02:00";
    row.timestring = "2 A.M.";
    //row.quote = "Jessica heard the disturbance in the great hall, turned on the light beside her bed. The clock there had not been properly adjusted to local time, and she had to subtract twenty-one minutes to determine that it was about 2 A.M.";
    row.quote = "Jessica heard the disturbance in the great hall, turned on the light beside her bed. The clock there had not been properly adjusted to local time, and she had to subtract twenty-one minutes to determine that it was about 2 A.M.";
    row.title = "Dune";
    row.author = "Frank Herbert";
    // CSV Parser: https://github.com/ben-strasser/fast-cpp-csv-parser
    textToImage(row, true, drawing_pen);
    return 0;
}