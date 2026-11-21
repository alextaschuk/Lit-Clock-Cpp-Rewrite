#include "clock/horologium.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include <exception>
#include <signal.h>
#include <thread>

// Should be nullptr until clock setup is done in main().
// Necessary for Handler() to clear the screen on SIGINT.
static Horologium* activeHorologium = nullptr;


// Listens for SIGINT signal to wipe and deactivate the screen 
// before shutting the clock down.
void Handler(int signo)
{
    std::string msg = "SIGINT detected. Turning the clock off...";
    activeHorologium->ClearAndExit(msg, false);
}


// Displays a message while the clock waits for the Pi to update its internal clock.
void displayStartupMsg(Horologium& horologium)
{
    std::unordered_map<std::string, std::string> startupMessage = 
    {
        {"time", "00:00"},
        {"timestring", "Literary Quote Clock is Starting…"},
        {"quote", "Literary Quote Clock is Starting…"},
        {"title", ""},
        {"author", ""},
    };
    std::vector<unsigned char> startupImage8bpp = horologium.writer.GenerateQuoteImage(startupMessage, false);
    std::vector<unsigned char> startupImage4bpp = horologium.ConvertTo4Bpp(startupImage8bpp);
    
    horologium.RefreshScreen(startupImage4bpp.data());
    spdlog::info("Startup image displayed.");
}


// Displays the first quote after the Pi's internal clock has updated.
// The first quote is not buffered.
void displayFirstImage(Horologium& horologium)
{
    horologium.GetTime(horologium.bufferedHour, horologium.bufferedMinute);
    std::vector<unsigned char> firstImage8bpp = horologium.GetImage(horologium.bufferedHour, horologium.bufferedMinute);
    std::vector<unsigned char> firstImage4bpp = horologium.ConvertTo4Bpp(firstImage8bpp);
    
    horologium.RefreshScreen(firstImage4bpp.data());
    spdlog::info("First quote displayed.");
}


// Determines how long the clock's loop in main() should sleep before the next
// call to TickClockForward().
//
// It takes ~3 seconds for TickClockForward() to complete (most of this is for
// the screen to update with a new image to display). So TickClockForward() is
// called at the 57th second of every minute. It's possible the function takes
// less than 3 seconds to return so we need to make sure that the loop sleeps
// until the next 57th second.
int sleepDuration(int currSecond)
{
    if (currSecond < 57)
        return 57 - currSecond;
    else
        return 57 - currSecond + 60;
}


// Creates the clock's Horologium object and runs the 
// clock's loop indefinitely to display quotes.
int main()
{
    Horologium horologium;
    activeHorologium = &horologium;

    signal(SIGINT, Handler); // Exception handling for ctrl + c


    displayStartupMsg(horologium);

    spdlog::info("Sleeping for 30 sec to let the RPi's internal clock update");
    std::this_thread::sleep_for(std::chrono::seconds(30));
    spdlog::info("Woke up. The RPi's clock should be synced.");
    
    
    displayFirstImage(horologium);
    
    // initialize the buffer
    for (int i = 0; i < MAX_IMAGES_TO_BUFFER; i++)
        horologium.refreshBuffer();
    spdlog::info("Image buffer initialized");

    // sleep until we're ready to start using the buffer
    std::time_t t = std::time(nullptr);
    std::tm* localTime = std::localtime(&t);
    int currSecond = localTime->tm_sec;
    std::this_thread::sleep_for(std::chrono::seconds(sleepDuration(currSecond))); // sleep until next min
    
    // This is bad practice, but it ensures that anything I might've missed is caught
    // so that the screen can be cleared before the program exits.
    try {
        while(true)
        {
            spdlog::info("Displaying the next quote...");
            horologium.TickClockForward();
            
            // sleep until the 57th second of the current min (leave ~3 sec for processing time
            // to update the screen)
            std::time_t t = std::time(nullptr);
            std::tm* localTime = std::localtime(&t);
            int currSecond = localTime->tm_sec;

            std::this_thread::sleep_for(std::chrono::seconds(sleepDuration(currSecond)));
        }
    } catch (const std::exception& e) {
        horologium.ClearAndExit(e.what(), true);
    }
}