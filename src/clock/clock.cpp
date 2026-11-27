#include "clock/horologium.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include <ctime>
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


// Displays a message while the clock waits for the RPi to update its internal clock.
void displayStartupMsg(Horologium& horologium)
{
    Row startupMessage = {
        "00:00",
        "Literary Quote Clock is Starting…",
        "Literary Quote Clock is Starting…",
        "", 
        ""
    };
    std::vector<unsigned char> startupImage8bpp = horologium.writer.GenerateQuoteImage(startupMessage, false);
    std::vector<unsigned char> startupImage4bpp = horologium.ConvertTo4Bpp(startupImage8bpp);
    
    horologium.RefreshScreen(startupImage4bpp.data());
    spdlog::info("Startup image displayed.");
}


// Displays the first quote (after the RPi's internal clock has updated) for the
// minute that `thisMinute` falls in.
// The first quote is not buffered.
void displayFirstImage(Horologium& horologium, std::chrono::system_clock::time_point thisMinute)
{
    auto [hour, minute] = Horologium::LocalHourMinute(thisMinute);
    std::vector<unsigned char> firstImage8bpp = horologium.GetImage(hour, minute);
    std::vector<unsigned char> firstImage4bpp = horologium.ConvertTo4Bpp(firstImage8bpp);

    horologium.RefreshScreen(firstImage4bpp.data());
    spdlog::info("First quote displayed.");
}


// Creates the clock's Horologium object and runs the 
// clock's loop indefinitely to display quotes.
int main()
{
    Horologium horologium;
    activeHorologium = &horologium;

    signal(SIGINT, Handler); // Exception handling for ctrl + c


    displayStartupMsg(horologium);

    spdlog::info("Sleeping for 30 seconds to let the RPi's internal clock update...");
    std::this_thread::sleep_for(std::chrono::seconds(30));
    spdlog::info("Wake from 30 second sleep. The RPi's clock should be synced.");
    
    auto thisMinute = std::chrono::floor<std::chrono::minutes>(std::chrono::system_clock::now()); // e.g. 13:52
    displayFirstImage(horologium, thisMinute);
    auto nextMinute = thisMinute+ std::chrono::minutes(1);

    horologium.ResetBuffer(thisMinute);
    spdlog::info("Initialized the image buffer.");
    
    // TODO: Maybe push full refresh image (all white) onto controller's memory ahead of time for HH:00
    // could speed up refresh process.

    // Re-init the buffer if the RPi's system clock is more than 2 minutes ahead
    // of the loop.
    constexpr auto MAX_LAG = std::chrono::minutes(2);
    
    // This is bad practice, but it ensures that anything I might've missed is
    // caught so that the screen can be cleared before the program exits.
    try {
        while(true)
        {
            // If next image isn't for HH:00, prepare it immediately.
            bool isTopOfHour = Horologium::LocalHourMinute(nextMinute).second == 0;
            if (!isTopOfHour)
                horologium.PrepareNextQuote(false);

            // If next image is for HH:00, don't prepare until HH:00.
            std::this_thread::sleep_until(nextMinute);

            // make sure system clock hasn't jumped forward.
            if (std::chrono::system_clock::now() - nextMinute > MAX_LAG)
            {
                spdlog::warn("The system clock jumped forward. Re-initializing the buffer from the current time.");
                auto currentMinute = std::chrono::floor<std::chrono::minutes>(std::chrono::system_clock::now());
                horologium.ResetBuffer(currentMinute);
                nextMinute = currentMinute + std::chrono::minutes(1);
                continue; // the next pass uploads a quote from the rebuilt buffer
            }

            if (isTopOfHour)
                horologium.PrepareNextQuote(true);

            horologium.ShowPreparedQuote();
            nextMinute += std::chrono::minutes(1);
        }
    } catch (const std::exception& e) {
        horologium.ClearAndExit(e.what(), true);
    }
}
