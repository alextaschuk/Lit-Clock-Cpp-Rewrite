#include "clock/horologium.hpp"

#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <string>
#include <print>
#include <random>
#include <unordered_map>
#include <vector>

#include "spdlog/spdlog.h"
#include "waveshare-IT8951/lib/Config/DEV_Config.h"
#include "image_generator/writer.hpp"

#include "constants.hpp"
#include "utils.hpp"

int Horologium::cacheQuotes()
{    
    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open()) { return -1; }

    std::string line;
    std::getline(quoteFile, line); // skip the header row

    std::string currTime = "00:00";
    std::string prevTime = "00:00";
    std::vector<std::unordered_map<std::string, std::string>> minRows = {}; // all rows for a given minute
    std::unordered_map<std::string, std::string> currRow = 
    {
        {"time", ""},
        {"timestring", ""},
        {"quote", ""},
        {"title", ""},
        {"author", ""},
    };

    int imgNum = -1;
    size_t quoteCount = 0;

    while (std::getline(quoteFile, line))
    {
        quoteCount++;
        std::vector<std::string> splitRow = split(line, '|');
        if (splitRow.size() != 5){
            std::println("Error: Row {} is missing a column.", quoteCount);
            continue;
        }

        currRow["time"]       = splitRow[0];
        currRow["timestring"] = splitRow[1];
        currRow["quote"]      = splitRow[2];
        currRow["title"]      = splitRow[3];
        currRow["author"]     = splitRow[4];

        int rowMin = std::stoi(currRow["time"].substr(3));
        int rowHour = std::stoi(currRow["time"].substr(0, 2));
        int currMin = std::stoi(currTime.substr(3));
        int currHour = std::stoi(currTime.substr(0, 2));

        if(rowMin - 1 == currMin)
        { /* Roll over to the next minute */
            quotes.push_back(minRows);
            minRows.clear();
            currTime = currTime.substr(0, 2) + ":" + currRow["time"].substr(3);
        }
        else if (rowHour - 1 == currHour)
        { /* Roll over to the next hour */
            quotes.push_back(minRows);
            minRows.clear();
            currTime = currRow["time"].substr(0, 2) + ":00";
        }
        minRows.push_back(currRow);
    }
    
    minRows.push_back(currRow);
    return quoteCount;
}


std::vector<unsigned char> Horologium::getImage(const size_t& quoteHour, const size_t& quoteMin)
{
    std::string minute = (quoteMin < 10) ? "0" + std::to_string(quoteMin) : std::to_string((quoteMin));
    std::string hour = (quoteHour < 10) ? "0" + std::to_string(quoteHour) : std::to_string(quoteHour);
    bool includeCredits = INCLUDE_CREDITS;

    int rowsIdx = quoteHour * 60 + quoteMin;
    std::vector<std::unordered_map<std::string, std::string>> usableRows = quotes[rowsIdx];

    /* random number generator borrowed from https://stackoverflow.com/a/7560564 */
    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(0, usableRows.size() - 1); // define the range
    
    std::unordered_map<std::string, std::string> selectedRow = usableRows[distr(gen)];
    return writer.generateQuoteImage(selectedRow, includeCredits);
}


std::vector<unsigned char> Horologium::convertTo4bpp(const std::vector<unsigned char>& image)
{
    const int bytesPerRow = (Panel_Width + 1) / 2;
    std::vector<unsigned char> packed(bytesPerRow * Panel_Height, 0);

    for (int row = 0; row < Panel_Height; ++row)
    {
        for (int col = 0; col < Panel_Width; ++col)
        {
            const unsigned char pixel4bpp = image[row * Panel_Width + col] >> 4;
            const int outByteIdx = row * bytesPerRow + col / 2;

            // According to Waveshare's docs, the even pixel goes in the low nibble,
            // and the odd pixel goes in the even nibble.
            // See https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#About_bpp
            packed[outByteIdx] |= ((col & 1) == 0) ? pixel4bpp : pixel4bpp << 4;
        }
    }

    return packed;
}


void Horologium::displayQuote()
{
    UBYTE* currentImagePtr = buffer.popImage();
    Paint_SelectImage(currentImagePtr);
    EPD_IT8951_4bp_Refresh(currentImagePtr, 0, 0, Panel_Width, Panel_Height, false, Init_Target_Memory_Addr, false);
}


void Horologium::bufferImage(size_t hour, size_t minute)
{
    std::vector<unsigned char> image8bpp = getImage(hour, minute);
    std::vector<unsigned char> packed4bpp = convertTo4bpp(getImage(hour, minute));
    buffer.pushImage(packed4bpp);
}


void Horologium::refreshBuffer()
{
    advanceTime(bufferedHour,bufferedMinute);
    bufferImage(bufferedHour, bufferedMinute);
}


void Horologium::getTime(size_t& hour, size_t& minute)
{
    std::time_t t = std::time(nullptr);
    std::tm* localTime = std::localtime(&t);
    hour = localTime->tm_hour;
    minute = localTime->tm_min;
}


void Horologium::advanceTime(size_t& hour, size_t& minute)
{
    if (minute == 59) {
        minute = 0;
        hour = (hour == 23) ? 0 : hour + 1;
    } else {
        minute += 1;
    }
}


void Horologium::tick_forward()
{
    size_t currHour = 0, currMin = 0;
    getTime(currHour, currMin);
    if (currMin == 59) {
        std::println("hour has passed. full refresh.");
        EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, GC16_Mode);
    }
    
    displayQuote();
    spdlog::info("displayed a new quote");
    refreshBuffer();
    spdlog::info("refreshed the buffer");
}
