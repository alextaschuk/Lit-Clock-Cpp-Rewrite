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

int Horologium::CacheQuotes()
{    
    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open())
        return -1;

    std::string line;
    std::getline(quoteFile, line); // skip the header row

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

    size_t quoteCount = 0;
    while (std::getline(quoteFile, line))
    {
        quoteCount++;
        std::vector<std::string> splitRow = split(line, '|');
        if (splitRow.size() != 5){
            std::println("Error: Row {} is missing a column", quoteCount);
            continue;
        }

        currRow["time"]       = splitRow[0];
        currRow["timestring"] = splitRow[1];
        currRow["quote"]      = splitRow[2];
        currRow["title"]      = splitRow[3];
        currRow["author"]     = splitRow[4];

        int rowMin = std::stoi(currRow["time"].substr(3));
        int rowHour = std::stoi(currRow["time"].substr(0, 2));
        int prevMin = std::stoi(prevTime.substr(3));
        int prevHour = std::stoi(prevTime.substr(0, 2));

        if(rowMin - 1 == prevMin)
        { /* Roll over to the next minute */
            quotes.push_back(minRows);
            minRows.clear();
            prevTime = prevTime.substr(0, 2) + ":" + currRow["time"].substr(3);
        }
        else if (rowHour - 1 == prevHour)
        { /* Roll over to the next hour */
            quotes.push_back(minRows);
            minRows.clear();
            prevTime = currRow["time"].substr(0, 2) + ":00";
        }
        minRows.push_back(currRow);
    }
    
    quotes.push_back(minRows);
    return quoteCount;
}


std::vector<unsigned char> Horologium::GetImage(const size_t& quoteHour, const size_t& quoteMin)
{
    std::string minute = (quoteMin < 10) ? "0" + std::to_string(quoteMin) : std::to_string((quoteMin));
    std::string hour = (quoteHour < 10) ? "0" + std::to_string(quoteHour) : std::to_string(quoteHour);
    bool includeCredits = INCLUDE_CREDITS;

    int rowsIdx = quoteHour * 60 + quoteMin;
    std::vector<std::unordered_map<std::string, std::string>> usableRows = quotes[rowsIdx];

    // random number generator borrowed from https://stackoverflow.com/a/7560564
    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(0, usableRows.size() - 1); // define the range
    
    std::unordered_map<std::string, std::string> selectedRow = usableRows[distr(gen)];
    spdlog::info("Generating image for {}:{}, includeCredits={}, start of quote: \"{}...\"", hour, minute, includeCredits, selectedRow["quote"].substr(0,50));
    return writer.GenerateQuoteImage(selectedRow, includeCredits);
}


std::vector<unsigned char> Horologium::ConvertTo4Bpp(const std::vector<unsigned char>& image)
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


void Horologium::RefreshScreen(UBYTE* img)
{
    WakeScreen();
    EPD_IT8951_4bp_Refresh(img, 0, 0, Panel_Width, Panel_Height, false, Init_Target_Memory_Addr, false);
    SleepScreen();
}


void Horologium::DisplayQuoteFromBuffer()
{
    UBYTE* currentImagePtr = buffer.PopImage();
    RefreshScreen(currentImagePtr);
    spdlog::info("Displayed a new quote.");
}


void Horologium::BufferImage(size_t hour, size_t minute)
{
    std::vector<unsigned char> image8bpp = GetImage(hour, minute);
    std::vector<unsigned char> packed4bpp = ConvertTo4Bpp(GetImage(hour, minute));
    buffer.PushImage(packed4bpp);
}


void Horologium::refreshBuffer()
{
    AdvanceTime(bufferedHour,bufferedMinute);
    BufferImage(bufferedHour, bufferedMinute);
    spdlog::info("Refreshed the buffer with a new image");
}


void Horologium::GetTime(size_t& hour, size_t& minute)
{
    std::time_t t = std::time(nullptr);
    std::tm* localTime = std::localtime(&t);
    hour = localTime->tm_hour;
    minute = localTime->tm_min;
}


void Horologium::AdvanceTime(size_t& hour, size_t& minute)
{
    if (minute == 59) {
        minute = 0;
        hour = (hour == 23) ? 0 : hour + 1;
    } else {
        minute += 1;
    }
}


void Horologium::TickClockForward()
{
    WakeScreen();

    size_t currHour = 0, currMin = 0;
    GetTime(currHour, currMin);
    if (currMin == 59) {
        spdlog::info("An hour has passed. Performing a full refresh on the display.");
        EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, GC16_Mode);
    }
    
    DisplayQuoteFromBuffer();
    refreshBuffer();
    SleepScreen();
}


void Horologium::WakeScreen()
{
    EPD_IT8951_SystemRun();
}


void Horologium::SleepScreen()
{
    EPD_IT8951_WaitForDisplayReady();
    EPD_IT8951_Sleep();
}


void Horologium::ClearAndExit(std::string shutdownMsg, bool isCritical) {
    if (isCritical)
        spdlog::critical(shutdownMsg);
    else
        spdlog::info(shutdownMsg);

    WakeScreen();
    EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, INIT_Mode);

    // wait for display wipe to finish before resetting the controller
    EPD_IT8951_WaitForDisplayReady();

    spdlog::info("Cleared the screen.");
    EPD_IT8951_Sleep();
    DEV_Module_Exit();
    exit(0);
}