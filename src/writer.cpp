#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include <cctype>

#include <chrono>
#include <iostream>

#include "constants.hpp"
#include "writer.hpp"


std::vector<std::string> Writer::split(std::string s, const std::string& delimiter)
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
int Writer::decodeUTF8(const std::string& s, size_t i, int& numBytes)
{
    unsigned char c = s[i];

    /**
     * All ASCII characters are encoded with 1 byte and their codepoints match their UTF-8 encoding.
     */
    if (c < 0x80) // 0xxxxxxx for 1 byte (ASCII character)
    {
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
    else if ((c & 0xE0) == 0xC0)  // 110xxxxx for 2 bytes
    {
        numBytes = 2;
        int byteZero = (c & 0x1F) << 6;
        int byteOne = s[i + 1] & 0x3F;
        return byteZero | byteOne;
    }

    /**
     * Encodings where the 4 MSbs of their first byte are 1110 (1110xxxx) use 3 bytes for encoding.
     */
    else if ((c & 0xF0) == 0xE0) // 1110xxxx for 3 bytes
    {
        numBytes = 3;
        int byteZero = (c & 0x0F) << 12;
        int byteOne = (s[i+1] & 0x3F) << 6;
        int byteTwo = s[i+2] & 0x3F;
        return byteZero | byteOne | byteTwo;
    }

    /**
     * Encodings where the 4 MSbs of their first byte are 1110 (1110xxxx) use 3 bytes for encoding.
     */
    else if ((c & 0xF8) == 0xF0) // 11110xxx for 4 bytes
    {
        numBytes = 4;
        int byteZero = ((c & 0x07) << 18);
        int byteOne = (s[i+1] & 0x3F) << 12;
        int byteTwo = (s[i+2] & 0x3F) << 6 ;
        int byteThree = s[i+3] & 0x3F;
        return byteZero | byteOne | byteTwo | byteThree;
    }

    numBytes = 1;
    std::println("Error: Malformed Byte: {}", c);
    return c;
}


int Writer::tallestGlyph(const std::string& line)
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


std::string Writer::formatChar(Pen& pen, std::string character)
{
    // skip if character is a WordDelimiter.
    std::vector<std::string> wordDelims = WordDelimiters().getWordDelims();
    if (std::find(wordDelims.begin(), wordDelims.end(), character) != wordDelims.end()) {
        return "";
    }

    size_t italicCount = 0;
    bool foundDelimiter = false;
    for (Delimiter& delim : charDelimiters)
    {
        if (character == delim.character)
        {
            delim.count += 1;
            character = "";
        }
        if (delim.count == 1)
        {
            foundDelimiter = true;
            if (delim.character == CharacterDelimiters().ITALIC)
            {
                italicCount = delim.count;
                pen.font = fonts.italic;
                pen.color = QUOTE_COLOR;
            }
            else if (delim.character == CharacterDelimiters().BOLD || delim.character == CharacterDelimiters().TIMESTR)
            {
                pen.font = (italicCount > 0) ? fonts.italicBold : fonts.bold;
                pen.color = TIME_COLOR;
            }
        }
        else if (delim.count >= 2)
        {
            pen.font = fonts.regular;
            pen.color = (textType == QUOTE) ? QUOTE_COLOR : TIME_COLOR;
        }
    }
    return character;
}


void Writer::formatWord(Pen& pen, std::string word, std::vector<std::string>& lines, const int& wordLength)
{
    bool addLine = false;
    if (word.contains(WordDelimiters().NEWLINE)) {
        addLine = true;
    } else if (word.contains(WordDelimiters().DOUBLE_NEWLINE)) {
        addLine = true;
        pen.x = bbox.topLeftX;
        pen.y += getLineHeight(pen.font, pen.fontScale);
        lines.push_back("");
    }

    if (pen.x + wordLength > bbox.bottomRightX || addLine) { // Current word would go past right side of image. Put on next line.
        pen.x = bbox.topLeftX;
        pen.y += getLineHeight(pen.font, pen.fontScale);
        lines.push_back(word);
    }  else {
        if (lines.empty()) {
            lines.push_back(word); // first word overall, so no line to append to yet
        } else {
            lines.back() += " " + word; // add the current word to the current (last) line
        }
    }
}


std::string Writer::wrapText(Pen& pen)
{
    std::vector<std::string> lines; // store wrapped text (e.g. ["this is a line", "this is another"])
    pen.x = bbox.topLeftX;
    pen.y = bbox.topLeftY;

    for (const std::string& word : split(text, " "))
    {
        /* Get the length of the word */
        float wordLengthF = 0.0f;
        int wordLength = 0;
        for (size_t i = 0; i < word.size(); )
        {
            int numBytes;
            int codePoint = decodeUTF8(word, i, numBytes);

            std::string currCharacter = formatChar(pen, word.substr(i, numBytes));
            if (currCharacter.empty()) {
                i += numBytes;
                continue; // skip metrics if the character is a delimiter
            }

            int advanceWidth, lsb;
            stbtt_GetCodepointHMetrics(&pen.font, codePoint, &advanceWidth, &lsb); 
            wordLengthF += advanceWidth * pen.fontScale;

            i += numBytes;
        }
        wordLength = static_cast<int>(wordLengthF);

        if (wordLength > bbox.bottomRightX - bbox.topLeftX)
        { // A single word cannot be longer than the bbox's width
            pen.x = bbox.topLeftX;
            pen.y = bbox.topLeftY;
            return "";
        }

        // add the length of a space after each word
        int advanceWidth;
        stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, 0); 
        wordLength += advanceWidth * pen.fontScale;
        formatWord(pen, word, lines, wordLength);

        pen.x += wordLength;

        int lineHeight = getLineHeight(pen.font, pen.fontScale);
        if (pen.y + lineHeight > bbox.bottomRightY)
        { /* current wrapping writes past text's bbox. Need to reduce font size. */
            pen.x = bbox.topLeftX;
            pen.y = bbox.topLeftY;
            return "";
        }
    }

    std::string wrapped;
    for (const auto& line : lines) {
        wrapped += line + "\n"; // e.g. ["It is", "12:00 P.M."] -> "It is\n12:00 P.M.\n"
    }
    wrapped.pop_back(); // remove the extra '\n' at the end of the string
    return wrapped;
}

int Writer::findOptimalFontScale(std::string& wrappedLines)
{
    float min = MIN_FONT_SCALE;
    float max = MAX_FONT_SCALE;
    float optimalScale = 0.0f;
    Pen tempPen = this->pen;
    BoundingBox tempBbox = this->bbox;

    /* Binary search to find best font size. */
    while (min <= max)
    {
        float mid = std::floor(min + (max - min) / 2);
        tempPen.fontScale = stbtt_ScaleForPixelHeight(&pen.font, mid);
        
        std::string lines;
        lines = wrapText(tempPen);
        if (!lines.empty()) { //Text fits. Try a larger font scale
            optimalScale = mid;
            min = mid + 1;
            wrappedLines = lines;
        } else {
            max = mid - 1; // Text didn't fit
        }
    }
    
    if (optimalScale < MIN_FONT_SCALE) {
        std::println("Text doesn't fit in the pen's bbox. optimalscale:{}", optimalScale);
        return 0;
    }

    /* TODO: put in its own function? */
    float linesHeight = 0, longestLineWidth = 0;
    for (const std::string& line : split(wrappedLines, "\n"))
    {
        float currentLineWidth = 0;
        linesHeight += getLineHeight(tempPen.font, tempPen.fontScale);

        for (size_t i = 0; i < line.size(); )
        {
            int numBytes;
            int codepoint = decodeUTF8(line, i, numBytes);
            i += numBytes;

            int advanceWidth;
            stbtt_GetCodepointHMetrics(&tempPen.font, codepoint, &advanceWidth, 0); 
            currentLineWidth += advanceWidth * tempPen.fontScale;
        }
        longestLineWidth = (longestLineWidth < currentLineWidth) ? currentLineWidth : longestLineWidth;
    }
    if (textType == CREDITS)
    { /* Resize the credit bbox to optimize how big the quote bbox is. */
        bbox.topLeftX = static_cast<int>((bbox.bottomRightX - longestLineWidth));
        bbox.topLeftY = static_cast<int>((bbox.bottomRightY - linesHeight));
    }
    return optimalScale;
}


void Writer::drawWord(std::vector<unsigned char>& image, std::string word)
{   
    for (size_t i = 0; i < word.length(); )
    {
        int numBytes;
        int codepoint = decodeUTF8(word, i, numBytes);

        std::string currCharacter = formatChar(pen, word.substr(i, numBytes));
        if (currCharacter.empty())
        {
            i += numBytes; 
            continue;   
        }

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
        if (drawX < bbox.topLeftX)
        {
            float shiftX = bbox.topLeftX - drawX;
            pen.x += shiftX; // move the pen right so that the glyph fits.
            drawX += shiftX;
        }

        if (drawY < bbox.topLeftY)
        {
            float shiftY = bbox.topLeftY - drawY;
            pen.y += shiftY; // move the pen down so that the glyph fits.
            drawY += shiftY;
        }

        /**
         * Make a temporary buffer for the rasterized glyph, then copy it 
         * onto the image (aka blitting).
         */
        std::vector<unsigned char> glyphBuf(glyphWidth * glyphHeight, 0);
        stbtt_MakeCodepointBitmap(&pen.font, glyphBuf.data(), glyphWidth, glyphHeight, glyphWidth, pen.fontScale, pen.fontScale, codepoint);
        
        for (int row = 0; row < glyphHeight; ++row)
        {
            for (int col = 0; col < glyphWidth; ++col)
            {
                int destX = static_cast<int>(drawX + col); // The rasterized glyph's x coordinate of the current pixel
                int destY = static_cast<int>(drawY + row); // The rasterized glyph's y coordinate of the current pixel

                // make sure the pixel fits
                if (destX >= bbox.topLeftX && destX < bbox.bottomRightX &&
                    destY >= bbox.topLeftY && destY < bbox.bottomRightY)
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

        pen.x += (advanceWidth * pen.fontScale); // move the pen to the right for the next glyph.
        i += numBytes;
    }
    
    // add the length of a space after each word
    int advanceWidth;
    stbtt_GetCodepointHMetrics(&pen.font, ' ', &advanceWidth, 0);
    pen.x += advanceWidth * pen.fontScale;

    for (Delimiter &delim : charDelimiters)
    {
        if (delim.count >= 2)
        {
            delim.count = 0;
            pen.font = fonts.regular;
        }
    }
}


void Writer::writeInBBox(std::vector<unsigned char>& image, std::unordered_map<std::string, std::string> row)
{
    // Wrap the timestring with "|" so that it can be found again when writing the quote.
    // If the timestring isn't found, write an error message instead.
    if (textType == QUOTE && !row["timestring"].empty())
    {
        size_t timestrBegin = 0, timestrEnd = 0;
        if (findTimestrIndices(row, timestrBegin, timestrEnd) < 0) {
            std::string msg = std::format("time string not found in quote starting with \"{}...\"", row["quote"].substr(0,50));
            std::println("Error: {}", msg);
            text = std::format("|Error| {}", msg);
        } else {
            std::string delim = CharacterDelimiters().TIMESTR;
            text = row["quote"].substr(0, timestrBegin);
            text += delim + row["quote"].substr(timestrBegin, timestrEnd - timestrBegin) + delim;
            text += row["quote"].substr(timestrEnd);
        }
    }

    std::string wrappedLines;
    int optimalScale = findOptimalFontScale(wrappedLines);
    if (optimalScale > 0) {
        pen.fontScale = stbtt_ScaleForPixelHeight(&pen.font, optimalScale);
    } else {
        std::println("Invalid font scale: {}", optimalScale);
    }

    resetPen(bbox.topLeftX, bbox.topLeftY); // TODO: fix call to findOptimalFontScale so that it doesn't modify pen's x and y val

    if (textType == CREDITS) { // prevent credit glyphs with descenders from being drawn past the bottom of the screen (necessary?)
        bbox.bottomRightY = static_cast<int>((SCREEN_HEIGHT + getLineHeight(pen.font, pen.fontScale)) * SCALE_MULTIPLIER);
    }

    for (const std::string& line : split(wrappedLines, "\n"))
    {
        int lineHeight = tallestGlyph(line);
        pen.x = bbox.topLeftX;
        pen.y -= lineHeight;

        for (const std::string& word : split(line, " ")) {
            drawWord(image, word);
        }
        pen.y += getLineHeight(pen.font, pen.fontScale) + lineHeight; // move to the next line and continue drawing
    }
}


std::vector<unsigned char> Writer::getImage(std::unordered_map<std::string, std::string> row, bool includeCredits)
{
    std::vector<unsigned char> image(SCREEN_WIDTH * SCREEN_HEIGHT, BG_COLOR);

    try {
        initFont(projectPath("/share/fonts/Bookerly.ttf"), fonts.regularBuf, fonts.regular);
        initFont(projectPath("/share/fonts/Bookerly-Italic.ttf"), fonts.italicBuf, fonts.italic);
        initFont(projectPath("/share/fonts/Bookerly-Bold.ttf"), fonts.boldBuf, fonts.bold);
        initFont(projectPath("/share/fonts/Bookerly-Bold-Italic.ttf"), fonts.italicBoldBuf, fonts.italicBold);
        initFont(projectPath("/share/fonts/Bookerly-Bold.ttf"), fonts.creditBuf, fonts.credit);
    } catch (const std::runtime_error) {
        std::println("Error: Failed to initialize fonts.");
    }

    pen.font = fonts.regular;

    /* leave some room around the screen so that text isn't written right up to its edges. */
    BoundingBox quoteBBox;
    quoteBBox.topLeftX =  static_cast<int>(std::floor(SCREEN_WIDTH - SCREEN_WIDTH * SCALE_MULTIPLIER));
    quoteBBox.topLeftY = static_cast<int>(std::floor(SCREEN_HEIGHT - SCREEN_HEIGHT * SCALE_MULTIPLIER));
    quoteBBox.bottomRightX = static_cast<int>(std::floor(SCREEN_WIDTH * SCALE_MULTIPLIER));
    quoteBBox.bottomRightY = static_cast<int>(std::floor(SCREEN_HEIGHT * SCALE_MULTIPLIER));

    if (includeCredits)
    {
        textType = CREDITS;
        BoundingBox creditsBBox;
        creditsBBox.topLeftX =  static_cast<int>(std::floor(SCREEN_WIDTH * 0.45));
        creditsBBox.topLeftY = static_cast<int>(std::floor(SCREEN_HEIGHT * 0.85));
        creditsBBox.bottomRightX = static_cast<int>(std::floor(SCREEN_WIDTH * SCALE_MULTIPLIER));
        creditsBBox.bottomRightY = static_cast<int>(std::floor(SCREEN_HEIGHT * SCALE_MULTIPLIER));
        bbox = creditsBBox;
        pen.color = CREDIT_COLOR;
        text = "—" + row["title"] + ", " + WordDelimiters().NEWLINE + row["author"];
        writeInBBox(image, row);
        quoteBBox.bottomRightY = static_cast<int>(std::floor(bbox.topLeftY * SCALE_MULTIPLIER));
    }

    textType = QUOTE;
    bbox = quoteBBox;
    text = row["quote"];
    pen.color = QUOTE_COLOR;
    writeInBBox(image, row);
    resetPen(bbox.topLeftX, bbox.topLeftY);

    return image;
}

// Saves all quote images to an `images/` directory.
//
// Each row in the CSV is parsed to create an image of each quote.
void saveImages(Writer &writer)
{
    std::unordered_map<std::string, std::string> row = 
    {
        {"time", ""},
        {"timestring", ""},
        {"quote", ""},
        {"title", ""},
        {"author", ""},
    };

    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open())
    {
        std::println("Error: Failed to open quotes file.");
        return;
    }

    std::string line;
    std::getline(quoteFile, line); // skip the header row
    
    std::string previousTime = "00:00";
    int imgNum = -1;
    size_t quoteCount = 0;

    try { // make an images directory if one doesn't exist yet
        std::filesystem::create_directories(projectPath("images"));
    } catch (const std::filesystem::filesystem_error& e) {
        std::println("Error: Failed to make images/ directory");
    }

    while (std::getline(quoteFile, line))
    {
        quoteCount++;
        std::vector<std::string> splitRow = Writer::split(line, CharacterDelimiters().TIMESTR);
        if (splitRow.size() != 5) {
            std::println("Error: Row {} is missing a column.", quoteCount);
            continue;
        }

        row["time"]       = splitRow[0];
        row["timestring"] = splitRow[1];
        row["quote"]      = splitRow[2];
        row["title"]      = splitRow[3];
        row["author"]     = splitRow[4];

        if (row["time"] == previousTime) {
            imgNum++;
        } else {
            std::string currTime = row["time"];
            int prevMin = std::stoi(previousTime.substr(3));
            int currMin = std::stoi(currTime.substr(3));

            if ((currMin - 1 != prevMin) && (previousTime.substr(3) != "59"))
            {
                int missingMin = prevMin + 1;
                std::string missingTime = std::format("{}{:02}", previousTime.substr(0, 2), missingMin);
                std::println("Error: Missing or out-of-order quote for {}", missingTime);
            }

            imgNum = 0;
            previousTime = currTime;
        }

        std::string time = row["time"].replace(2, 1, "");
        std::string filepath = projectPath(IMAGE_PATH + "quote_" + time + "_" + std::to_string(imgNum) + "." + IMAGE_FORMAT);
        std::vector<unsigned char> imgOut = writer.getImage(row, true);

        if (IMAGE_FORMAT == "bmp") {
            stbi_write_bmp(filepath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, imgOut.data());
        } else if (IMAGE_FORMAT == "png") {
            stbi_write_png(filepath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, imgOut.data(), SCREEN_WIDTH);
        } else {
            std::println("Error: {} is an invalid image type", IMAGE_FORMAT);
        }

        std::string progressBar = "Creating images... " + std::to_string(quoteCount);
        std::cout << progressBar << '\r' << std::flush;
    }
    quoteFile.close();
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    Writer writer;
    saveImages(writer);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed: " << elapsed.count() << " seconds" << std::endl;
    return 0;
}