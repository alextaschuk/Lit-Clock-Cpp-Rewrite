#include "lit_clock.hpp"

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
#include <cmath> 

#include "constants.hpp"
#include "spdlog/spdlog.h"
#include "utils.hpp"
#include "waveshare-IT8951/lib/Config/DEV_Config.h"
#include "writer.hpp"

void LitClock::cacheQuotes()
{
    std::string currTime = "00:00";
    
    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open()) {
        std::println("Error: Failed to open quotes file.");
        return;
    }

    std::string line;
    std::getline(quoteFile, line); // skip header row

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

void LitClock::displayQuote()
{
    UBYTE* currentImagePtr = buffer.popImage();
    Paint_SelectImage(currentImagePtr);
    EPD_IT8951_8bp_Refresh(currentImagePtr, 0, 0, Panel_Width, Panel_Height, false, Init_Target_Memory_Addr);
}


void LitClock::bufferImage(size_t hour, size_t minute)
{
    std::vector<unsigned char> image = getImage(hour, minute);
    buffer.pushImage(image);
}


void LitClock::refreshBuffer()
{
    advanceTime(bufferedHour,bufferedMinute);
    bufferImage(bufferedHour, bufferedMinute);
}


void LitClock::clearScreen()
{
    EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, GC16_Mode);
}


void LitClock::getTime(size_t& hour, size_t& minute)
{
    std::time_t t = std::time(nullptr);   // current time, as a raw timestamp
    std::tm* localTime = std::localtime(&t);   // breaks it into local-time components

    hour = localTime->tm_hour;
    minute = localTime->tm_min;
}


void LitClock::tick_forward()
{
    size_t currHour = 0, currMin = 0;
    getTime(currHour, currMin);
    
    if (currMin == 59) {
        std::println("hour has passed. full refresh.");
        clearScreen();
    }

    displayQuote();
    refreshBuffer();
    
    // sleep until the 59th second of the current min (leave 1 sec for processing time
    // to change image on the screen)
    std::time_t t = std::time(nullptr);
    std::tm* localTime = std::localtime(&t);
    int currSecond = localTime->tm_sec;
    std::this_thread::sleep_for(std::chrono::seconds(59 - currSecond));
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
    std::vector<unsigned char> startupImage = lit_clock.writer.generateQuoteImage(startupMessage, false);
    spdlog::info("Got the startup image");
    UBYTE* startupMsgPtr = startupImage.data();
    EPD_IT8951_8bp_Refresh(startupMsgPtr, 0, 0, lit_clock.Panel_Width, lit_clock.Panel_Height, false, lit_clock.Init_Target_Memory_Addr);
    
    spdlog::info("Sleeping for 30 sec to let the RTC update");
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // display the first quote
    lit_clock.getTime(lit_clock.bufferedHour, lit_clock.bufferedMinute);
    std::vector<unsigned char> firstQuote = lit_clock.getImage(lit_clock.bufferedHour, lit_clock.bufferedMinute);
    UBYTE* firstQuotePtr = firstQuote.data();
    EPD_IT8951_8bp_Refresh(firstQuotePtr, 0, 0, lit_clock.Panel_Width, lit_clock.Panel_Height, false, lit_clock.Init_Target_Memory_Addr);
    
    // initialize the buffer
    for (int i = 0; i < MAX_IMAGES_TO_BUFFER; i++) {
        lit_clock.refreshBuffer();
    }
    spdlog::info("Image buffer initialized");

    // sleep until we're ready to start using the buffer
    std::time_t t = std::time(nullptr);
    std::tm* localTime = std::localtime(&t);
    int currSecond = localTime->tm_sec;
    std::this_thread::sleep_for(std::chrono::seconds(59 - currSecond)); // sleep until next min
    
    // This is bad practice, but it ensures that anything I might've missed is caught
    // so that the screen can be cleared before the program exits.
    try {
        lit_clock.tick_forward();
    } catch (...)
    {
        std::println("error");
        Paint_Clear(0xFF);
        exit(0);
    }
}