#include "clock/lit_clock.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <print>
#include <vector>
#include <random>
#include <thread>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <signal.h>

#include "constants.hpp"
#include "spdlog/spdlog.h"
#include "utils.hpp"
#include "waveshare-IT8951/lib/Config/DEV_Config.h"
#include "image_generator/writer.hpp"

void LitClock::cacheQuotes()
{    
    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open()) {
        std::println("Error: Failed to open quotes file.");
        return;
    }

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
        std::vector<std::string> splitRow = split(line, "|");
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
    spdlog::info("Successfully cached {0:d} quotes.", quoteCount);
}


std::vector<unsigned char> LitClock::getImage(const size_t& quoteHour, const size_t& quoteMin)
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


std::vector<unsigned char> LitClock::convertTo4bpp(const std::vector<unsigned char>& image)
{
    const int bytesPerRow = (Panel_Width + 1) / 2;
    std::vector<unsigned char> packed(bytesPerRow * Panel_Height, 0);

    for (int row = 0; row < Panel_Height; ++row) {
        for (int col = 0; col < Panel_Width; ++col) {
            const unsigned char pixel4bpp = image[row * Panel_Width + col] >> 4;
            const int outByteIdx = row * bytesPerRow + col / 2;

            // According to Waveshare's docs, the even pixel goes in the low nibble,
            // and the odd pixel goes in the even nibble.
            // See https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#About_bpp
            if ((col & 1) == 0) {
                packed[outByteIdx] |= pixel4bpp;
            } else {
                // Waveshare: odd pixel goes in HIGH nibble.
                packed[outByteIdx] |= pixel4bpp << 4;
            }
        }
    }

    return packed;
}


void LitClock::displayQuote()
{
    UBYTE* currentImagePtr = buffer.popImage();
    Paint_SelectImage(currentImagePtr);
    EPD_IT8951_4bp_Refresh(currentImagePtr, 0, 0, Panel_Width, Panel_Height, false, Init_Target_Memory_Addr, true);
}


void LitClock::bufferImage(size_t hour, size_t minute)
{
    std::vector<unsigned char> image8bpp = getImage(hour, minute);
    std::vector<unsigned char> packed4bpp = convertTo4bpp(getImage(hour, minute));
    buffer.pushImage(packed4bpp);
}


void LitClock::refreshBuffer()
{
    advanceTime(bufferedHour,bufferedMinute);
    bufferImage(bufferedHour, bufferedMinute);
}


void LitClock::getTime(size_t& hour, size_t& minute)
{
    std::time_t t = std::time(nullptr);   // current time, as a raw timestamp
    std::tm* localTime = std::localtime(&t);   // breaks it into local-time components

    hour = localTime->tm_hour;
    minute = localTime->tm_min;
}


void LitClock::advanceTime(size_t& hour, size_t& minute)
{
    if (minute == 59) {
        minute = 0;
        hour = (hour == 23) ? 0 : hour + 1;
    } else {
        minute += 1;
    }
}


void LitClock::tick_forward()
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


void Handler(int signo)
{
    std::println("ctrl + c detected.");
    DEV_Module_Exit();
    exit(0);
}


int main()
{
    //Exception handling:ctrl + c
    signal(SIGINT, Handler);

    LitClock lit_clock;

    //display startup screen
    std::unordered_map<std::string, std::string> startupMessage = 
    {
        {"time", "00:00"},
        {"timestring", "Literary Quote Clock is Starting…"},
        {"quote", "Literary Quote Clock is Starting…"},
        {"title", ""},
        {"author", ""},
    };
    std::vector<unsigned char> startupImage8bpp = lit_clock.writer.generateQuoteImage(startupMessage, false);
    std::vector<unsigned char> startupImage4bpp = lit_clock.convertTo4bpp(startupImage8bpp);
    UBYTE* startupMsgPtr = startupImage4bpp.data();
    EPD_IT8951_4bp_Refresh(startupMsgPtr, 0, 0, lit_clock.Panel_Width, lit_clock.Panel_Height, false, lit_clock.Init_Target_Memory_Addr, true);
    spdlog::info("Displayed startup image");
    
    spdlog::info("Sleeping for 30 sec to let the RTC update");
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // display the first quote
    lit_clock.getTime(lit_clock.bufferedHour, lit_clock.bufferedMinute);
    std::vector<unsigned char> firstImage8bpp = lit_clock.getImage(lit_clock.bufferedHour, lit_clock.bufferedMinute);
    std::vector<unsigned char> firstImage4bpp = lit_clock.convertTo4bpp(firstImage8bpp);
    UBYTE* firstQuotePtr = firstImage4bpp.data();
    EPD_IT8951_4bp_Refresh(firstQuotePtr, 0, 0, lit_clock.Panel_Width, lit_clock.Panel_Height, false, lit_clock.Init_Target_Memory_Addr, true);
    spdlog::info("Displayed first quote");
    
    // initialize the buffer
    for (int i = 0; i < MAX_IMAGES_TO_BUFFER; i++) {
        lit_clock.refreshBuffer();
    }
    spdlog::info("Image buffer initialized");

    // sleep until we're ready to start using the buffer
    std::time_t t = std::time(nullptr);
    std::tm* localTime = std::localtime(&t);
    int currSecond = localTime->tm_sec;
    spdlog::info("sleeping for {} seconds", 59 - currSecond);
    std::this_thread::sleep_for(std::chrono::seconds(59 - currSecond)); // sleep until next min
    
    // This is bad practice, but it ensures that anything I might've missed is caught
    // so that the screen can be cleared before the program exits.
    try {
        while(true)
        {
            lit_clock.tick_forward();
            
            // sleep until the 59th second of the current min (leave 1 sec for processing time
            // to change image on the screen)
            std::time_t t = std::time(nullptr);
            std::tm* localTime = std::localtime(&t);
            int currSecond = localTime->tm_sec;
            spdlog::info("going to sleep for {} seconds", 59 - currSecond);
            std::this_thread::sleep_for(std::chrono::seconds(59 - currSecond));
            spdlog::info("woke up to display next quote.");
        }
    } catch (...)
    {
        std::println("error");
        Paint_Clear(0xFF); //TODO: fix to actually clear screen
        exit(0);
    }
}