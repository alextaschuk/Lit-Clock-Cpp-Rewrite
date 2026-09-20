#include "image_generator/writer.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <print>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>
//#include <omp.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "constants.hpp"
#include "image_generator/delimiter.hpp"


int Writer::maxAscender(const std::string& line)
{
    int maxHeight = 0;
    for (const std::string& word : split(line, ' ')) {
        for (size_t i = 0; i < word.size(); ) {
            int numBytes;
            int codepoint = decodeUTF8(word, i, numBytes);
            i += numBytes;

            BoundingBox glyphBox;
            stbtt_GetCodepointBitmapBox(&pen.font, codepoint, pen.fontScale, pen.fontScale, &glyphBox.topLeftX, &glyphBox.topLeftY, &glyphBox.bottomRightX, &glyphBox.bottomRightY);
            maxHeight = std::min(maxHeight, glyphBox.topLeftY);
        }
    }
    return maxHeight;
}


float Writer::getLineWidth(Pen& pen, const std::string& line)
{
    if (line.size() == 0)
        return 0.0f;

    float lineWidth = 0.0f;
    int advanceWidth = 0;
    for (size_t i = 0; i < line.size(); )
    {
        int numBytes;
        int codepoint = decodeUTF8(line, i, numBytes);
        std::string currChar = line.substr(i, numBytes);
        std::string character = ""; // TODO: change currChar to output param in formatChar

        character = formatChar(pen, currChar);
        i += numBytes;

        if (character.empty())
            continue;

        stbtt_GetCodepointHMetrics(&pen.font, codepoint, &advanceWidth, 0); 
        lineWidth += advanceWidth * pen.fontScale;
    }
    return lineWidth;
}


void Writer::resizeCreditBbox(const std::string& wrappedLines)
{
    float longestLineWidthF = 0.0f;
    int linesHeight = 0, longestLineWidth = 0;
    for (const std::string& line : split(wrappedLines, '\n'))
    {
        linesHeight += getLineHeight(pen.font, pen.fontScale);
        longestLineWidthF = std::max(longestLineWidthF, getLineWidth(pen, line));
    }
    
    longestLineWidth = static_cast<int>(longestLineWidthF);
    bbox.topLeftX = bbox.bottomRightX - longestLineWidth;
    bbox.topLeftY = bbox.bottomRightY - linesHeight;
}


std::string Writer::formatChar(Pen& pen, std::string character)
{
    if (pen.pendingEscape) {
        pen.pendingEscape = false;
    } else {
        if (character == CharacterDelimiters().NEWLINE)
            return ""; // dealt with in formatWord()
        
        if (character == "\\") {
            pen.pendingEscape = true;
            character = "";
        } else if (auto* delim = findDelimiter(character)) {
            ++delim->count;
            character = "";
        }
    }

    const bool italicActive = getDelimiter(DelimiterType::Italic).count % 2 == 1;
    const bool boldActive = getDelimiter(DelimiterType::Bold).count % 2 == 1;
    const bool timeActive = getDelimiter(DelimiterType::Time).count % 2 == 1;

    if (italicActive) {
        pen.font = boldActive || timeActive ? fonts.italicBold : fonts.italic;
        pen.color = timeActive || textType == CREDITS ? TIME_COLOR : QUOTE_COLOR;
    } else if (boldActive) {
        pen.font = fonts.bold;
        pen.color = timeActive ? TIME_COLOR : QUOTE_COLOR;
    } else if (timeActive) {
        pen.font = fonts.bold;
        pen.color = TIME_COLOR;
    } else {
        pen.font = fonts.regular;
        pen.color = (textType == CREDITS) ? TIME_COLOR : QUOTE_COLOR;
    }
    
    return character;
}


void Writer::formatWord(Pen& pen, std::string word, std::string& lines, const int& wordLength)
{
    const auto newline = CharacterDelimiters().NEWLINE;
    bool on_newline = false;
    size_t pos = 0;
    while ((pos = word.find("\\n", pos)) != std::string::npos)
    { // replace literal "\" and "n" strings from the CSV with a proper EOL character (U+000A).
        word.replace(pos, 2, CharacterDelimiters().NEWLINE);
        ++pos;
    }

    size_t newlineCount = 0;
    pos = 0;
    while ((pos = word.find(newline, pos)) != std::string::npos) {
        pos += CharacterDelimiters().NEWLINE.size();
        ++newlineCount;
        on_newline = true;
    }
 
    if (newlineCount > 0) {
        pen.x = bbox.topLeftX;
        for (size_t i = 0; i < newlineCount; ++i) 
            pen.y += getLineHeight(pen.font, pen.fontScale);
    }
    
    if (pen.x + wordLength > bbox.bottomRightX) {
        pen.x = bbox.topLeftX;
        pen.y += getLineHeight(pen.font, pen.fontScale);
        lines += CharacterDelimiters().NEWLINE + word;
    } else {
        lines += (!lines.empty()) ? " " + word : word;
    }
}


std::string Writer::wrapText(Pen& pen)
{
    pen.x = bbox.topLeftX;
    pen.y = bbox.topLeftY;
    std::string wrapped_lines = "";
    std::vector<std::string> words = split(text, ' ');

    for (size_t i = 0; i < words.size(); ++i)
    {
        const std::string& currWord= words[i];
        int currWordLength = static_cast<int>(getLineWidth(pen, currWord));
        if (currWordLength >= bbox.bottomRightX - bbox.topLeftX)
        { // A single word cannot be longer than the bbox's width.
            pen.x = bbox.topLeftX;
            pen.y = bbox.topLeftY;
            return "";
        }

        if (i != words.size() - 1) 
        { /* add the length of a space after each word (except for the last) */
            int advanceWidth;
            stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, 0); 
            currWordLength += advanceWidth * pen.fontScale;
        }
        
        formatWord(pen, currWord, wrapped_lines, currWordLength);
        pen.x += currWordLength;

        int lineHeight = getLineHeight(pen.font, pen.fontScale);
        if (pen.y + lineHeight > bbox.bottomRightY)
        { /* current wrapping writes past text's bbox. Need to reduce font size. */
            pen.x = bbox.topLeftX;
            pen.y = bbox.topLeftY;
            return "";
        }
    }

    return wrapped_lines;
}


void Writer::findOptimalFontScale(std::string& wrappedLines)
{
    float min = MIN_FONT_SCALE;
    float max = MAX_FONT_SCALE;
    float optimalScale = 0.0f;
    Pen tempPen = this->pen;
    tempPen.font = fonts.regular;
    BoundingBox tempBbox = this->bbox;

    /* Binary search to find best font size. */
    while (min <= max)
    {
        float mid = std::floor(min + (max - min) / 2);
        tempPen.fontScale = stbtt_ScaleForPixelHeight(&fonts.regular, mid);
        std::string lines = wrapText(tempPen);
        if (!lines.empty()) { /* Text fits. Try a larger font scale */
            optimalScale = mid;
            min = mid + 1;
            wrappedLines = lines;
        } else {
            max = mid - 1; // Text didn't fit
        }
        resetCharDelimCount();
    }

    if (optimalScale > 0) {
        pen.fontScale = stbtt_ScaleForPixelHeight(&fonts.regular, optimalScale);
    } else {
        std::println("Error: text cannot fit in its bbox.");
        return; // TODO: better error handling.
    }
}


void Writer::drawWord(std::vector<unsigned char>& image, std::string word, bool isLast)
{
    for (size_t i = 0; i < word.length(); )
    {
        int numBytes;
        int codepoint = decodeUTF8(word, i, numBytes);
        std::string currChar = word.substr(i, numBytes);
        std::string currCharacter = formatChar(pen, currChar);

        i += numBytes; 
        if (currCharacter.empty())
            continue;
        /**
        * We get a bbox around a glyph's rendered ink, relative to the its origin (which is the pen's x and y coord).
        * pen.x/pen.y track where the cursor is on the image. Specifically, pen.y tracks where the glyph's baseline is on the image.
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

        // (drawX, drawY) is the coordinate on the image where the top-left of the glyph's bbox should be placed.
        int drawX = pen.x + glyphBox.topLeftX;
        int drawY = pen.y + glyphBox.topLeftY;

        // Handle the case when a glyph's ink starts outside the left of or above the image's bounding box.
        // Necessary since glyphWidth and glyphHeight can be negative.
        if (drawX < bbox.topLeftX)
        {
            int shiftX = bbox.topLeftX - drawX;
            pen.x += shiftX; // move the pen right so that the glyph fits.
            drawX += shiftX;
        }

        if (drawY < bbox.topLeftY)
        {
            int shiftY = bbox.topLeftY - drawY;
            pen.y += shiftY; // move the pen down so that the glyph fits.
            drawY += shiftY;
        }

        // Make a temporary buffer for the rasterized glyph, then copy it onto the image (aka blitting).
        std::vector<unsigned char> glyphBuf(glyphWidth * glyphHeight, 0);
        stbtt_MakeCodepointBitmap(&pen.font, glyphBuf.data(), glyphWidth, glyphHeight, glyphWidth, pen.fontScale, pen.fontScale, codepoint);
            
        for (int row = 0; row < glyphHeight; ++row)
        {
            for (int col = 0; col < glyphWidth; ++col)
            {
                int destX = drawX + col; // The rasterized glyph's x coordinate of the current pixel
                int destY = drawY + row; // The rasterized glyph's y coordinate of the current pixel

                // make sure the pixel fits
                if (destX >= bbox.topLeftX &&
                    destX < bbox.bottomRightX &&
                    destY >= bbox.topLeftY &&
                    destY < bbox.bottomRightY
                    )
                {
                    unsigned char glyphPixel = glyphBuf[row * glyphWidth + col]; // foreground pixel
                    if (glyphPixel > 0) // only want non-background pixels
                    {
                        /**
                        * use linear interpolation between two colors, weighted by an alpha value to determine the pixel's color.
                        * Alpha compositing is generally: result = foreground * alpha + background * (1 - alpha)
                        * alpha is usually normalized to [0, 1] but we are using 8-bit space here (TODO: reduce to 4-bit?)
                        * so we use a range of [0, 255] instead. 
                        * glyphPixel (0-255) plays the role of alpha * 255.
                        * (255 - glyphPixel) plays the role of (1 - alpha * 255)
                        * dividing the sum by 255 at the end normalizes it to [0, 255].
                        */
                        int pixelIdx = destY * SCREEN_WIDTH + destX; // this converts the 2D coords of the pixel into a 1D array index
                        int backgroundPixel = image[pixelIdx];
                        image[pixelIdx] = (pen.color * glyphPixel + backgroundPixel * (255 - glyphPixel)) / 255;
                    }
                }
            }
        }
            
        int advanceWidth; // how far the pen should move after drawing a glyph (in font units)
        stbtt_GetCodepointHMetrics(&pen.font, codepoint, &advanceWidth, 0);
        pen.x += advanceWidth * pen.fontScale; // move the pen to the right for the next glyph.
    }
        
    if (!isLast)
    { /* add the length of a space after each word (except for the last) */
        int advanceWidth;
        stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, 0);
        pen.x += advanceWidth * pen.fontScale;
    }

    for (Delimiter &delim : charDelimiters)
    { /* reset delimiters whose wrapping is complete*/
        if (delim.count == 2) {
            delim.count = 0;
            pen.font = fonts.regular;
        }
    }
}


void Writer::writeInBBox(std::vector<unsigned char>& image, std::unordered_map<std::string, std::string> row)
{
    // Wrap the timestring with "|" so that it can be found again when writing the quote.
    // If the timestring isn't found, write an error message instead.
    if (textType == QUOTE)
    {
        size_t timestrBegin = 0, timestrEnd = 0;
        if (findTimestrIndices(row, timestrBegin, timestrEnd) < 0)
        {
            std::string msg = std::format("Time string not found in quote starting with \"{}...\"", row["quote"].substr(0,50));
            std::println("Error: {}", msg);
            text = std::format("*Error*: {}", msg);
        }
        else
        {
            std::string delim = CharacterDelimiters().TIMESTR;
            text = row["quote"].substr(0, timestrBegin);
            text += delim + row["quote"].substr(timestrBegin, timestrEnd - timestrBegin) + delim;
            text += row["quote"].substr(timestrEnd);
        }
    }

    std::string wrappedLines;
    findOptimalFontScale(wrappedLines);
    if (textType == CREDITS) { resizeCreditBbox(wrappedLines); } // Resize the credit bbox to optimize the quote bbox's size.
    resetPen(bbox.topLeftX, bbox.topLeftY); // move the pen to its starting position.
    resetCharDelimCount();

    for (const std::string& line : split(wrappedLines, '\n'))
    {
        int lineHeight = maxAscender(line);
        pen.x = bbox.topLeftX;
        pen.y -= lineHeight;

        std::vector<std::string> words = split(line, ' ');
        for (size_t i = 0; i < words.size(); ++i) {
            bool isLastWord = (i == words.size() - 1);
            drawWord(image, words[i], isLastWord);
        }
        
        pen.y += getLineHeight(pen.font, pen.fontScale) + lineHeight; // move to the next line and continue drawing
    }
    resetPen(bbox.topLeftX, bbox.topLeftY);
    resetCharDelimCount();
}


std::vector<unsigned char> Writer::generateQuoteImage(std::unordered_map<std::string, std::string> row, const bool& includeCredits)
{
    std::vector<unsigned char> image(SCREEN_WIDTH * SCREEN_HEIGHT, BG_COLOR);

    pen.font = fonts.regular;

    /* leave some room around the screen so that text isn't written right up to its edges. */
    BoundingBox quoteBBox;
    quoteBBox.topLeftX      =  static_cast<int>(std::floor(SCREEN_WIDTH - SCREEN_WIDTH * SCALE_MULTIPLIER));
    quoteBBox.topLeftY      = static_cast<int>(std::floor(SCREEN_HEIGHT - SCREEN_HEIGHT * SCALE_MULTIPLIER));
    quoteBBox.bottomRightX  = static_cast<int>(std::floor(SCREEN_WIDTH * SCALE_MULTIPLIER));
    quoteBBox.bottomRightY  = static_cast<int>(std::floor(SCREEN_HEIGHT * SCALE_MULTIPLIER));

    if (includeCredits)
    {
        textType            = CREDITS;
        pen.color           = CREDIT_COLOR;
        text                = "_" + row["title"] + "_, " + "\n" + row["author"]; // italicize book title, put author on new line
        bbox.topLeftX       = static_cast<int>(std::floor(SCREEN_WIDTH * 0.45)); // scale credit's bbox down to bottom right of screen
        bbox.topLeftY       = static_cast<int>(std::floor(SCREEN_HEIGHT * 0.85));
        bbox.bottomRightX   = static_cast<int>(std::floor(SCREEN_WIDTH * SCALE_MULTIPLIER));
        bbox.bottomRightY   = static_cast<int>(std::floor(SCREEN_HEIGHT * SCALE_MULTIPLIER));
        
        writeInBBox(image, row);
        quoteBBox.bottomRightY = static_cast<int>(std::floor(bbox.topLeftY * SCALE_MULTIPLIER));
    }

    textType    = QUOTE;
    pen.color   = QUOTE_COLOR;
    text        = row["quote"];
    bbox        = quoteBBox;

    writeInBBox(image, row);
    resetPen(bbox.topLeftX, bbox.topLeftY);
    resetCharDelimCount();

    return image;
}
