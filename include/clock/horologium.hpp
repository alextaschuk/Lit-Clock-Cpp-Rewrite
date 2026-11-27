#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>

extern "C" {
    #include "waveshare-IT8951/lib/e-Paper/EPD_IT8951.h"
    #include "waveshare-IT8951/lib/GUI/GUI_Paint.h"
}

#include "spdlog/spdlog.h" // logger

#include "constants.hpp"
#include "image_buffer.hpp"
#include "image_generator/writer.hpp"

// Determines which quote image should be displayed.
//
// This class contains all logic to ensure that the correct quote is
// displayed at each minute and information about the EPD that is necessary for
// Waveshare's libraries to display images. It is used in clock.cpp's main()
// function to continuously update the screen once every minute with a new
// quote.
class Horologium
{
    public:
        Horologium()
        {
            if (DEV_Module_Init()!= 0) {
                spdlog::critical("Error: Failed to initialize the BCM2835 hardware module.");
                std::exit(1);
            }

            std::uint16_t VCOM_Scaled = static_cast<std::uint16_t>(std::round(std::abs(VCOM) * 1000.0));

            Dev_Info = EPD_IT8951_Init(VCOM_Scaled);
            Panel_Width = Dev_Info.Panel_W; //TODO: use this instead of SCREEN WIDTH/ SCREEN HEIGHT 
            Panel_Height = Dev_Info.Panel_H;   
            Init_Target_Memory_Addr = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);

            EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, INIT_Mode);
            spdlog::info("Waveshare EPD initialized.");

            int numCachedQuotes = CacheQuotes();
            if ( numCachedQuotes == -1) {
                std::string errMsg = "Failed to open CSV file containing clock's quotes. Exiting clock startup.";
                ClearAndExit(errMsg, true);
            } else {
                spdlog::info("Cached {0:d} quotes.", numCachedQuotes);
            }

            try {
                writer.InitFont(projectPath("/share/fonts/Bookerly.ttf"), writer.fonts.regularBuf, writer.fonts.regular);
                writer.InitFont(projectPath("/share/fonts/Bookerly-Italic.ttf"), writer.fonts.italicBuf, writer.fonts.italic);
                writer.InitFont(projectPath("/share/fonts/Bookerly-Bold.ttf"), writer.fonts.boldBuf, writer.fonts.bold);
                writer.InitFont(projectPath("/share/fonts/Bookerly-Bold-Italic.ttf"), writer.fonts.italicBoldBuf, writer.fonts.italicBold);
                writer.InitFont(projectPath("/share/fonts/Bookerly-Bold.ttf"), writer.fonts.creditBuf, writer.fonts.credit);
            } catch (const std::runtime_error&) {
                std::string errMsg = "Failed to initialize fonts. Exiting clock startup.";
                ClearAndExit(errMsg, true);
            }
        }

        Writer writer;

        ImageBuffer buffer; // Ring buffer containing the next `MAX_IMAGES_TO_BUFFER` number of images.
        
        // The minute (hh:mm:00) of the quote most recently pushed onto the buffer.
        std::chrono::system_clock::time_point bufferedTime;

        std::vector<std::vector<Row>> quotes; // A cache for the quotes from a CSV file.

        std::mt19937 rng{std::random_device{}()}; // Used to select a random row for times with >1 possible quotes.

        IT8951_Dev_Info Dev_Info = {0, 0}; // Contains info about the screen for Waveshare's drivers.
        UWORD Panel_Width;  // width of the screen, in pixels
        UWORD Panel_Height; // height of the screen, in pixels
        UDOUBLE Init_Target_Memory_Addr; // The memory address on the IT8951 controller's onboard memory where pixel data should be written before a display refresh.


        // Refresh the screen with a new image.
        //
        // img: A flat buffer of a 4bpp image to display on the screen.
        void RefreshScreen(UBYTE* img);

        
        // Wakes the screen's controller. Call this before sending anything to the
        // screen (e.g. a refresh command to display an image).
        void WakeScreen();


        // Waits for the screen to finish refreshing, then puts its controller in
        // low-power mode.
        void SleepScreen();
        

        // Parses the CSV of quotes and store them in a vector.
        //
        // This function assumes that all quotes are in order and
        // that none are missing.
        //
        // Returns the number of cached quotes, or -1 on failure.
        int CacheQuotes();

        
        // Preempts the next image by popping and uploading the image at the front of
        // the buffer to the IT8951 controller's memory.
        //
        // This function is usually called right after `ShowPreparedQuote()` at the
        // top of the while loop in `main()` of clock.cpp. An exception is made when
        // the next image is for the top of the hour (i.e., HH:00) because a full
        // refresh is performed at the top of every hour: the display sleeps until the
        // next minute before this function is called, which prevents
        // `clearScreen=true` from immediately wiping the screen after the image for
        // HH:59 is displayed.
        //
        // clearScreen: true to clear the screen before uploading (done once an hour
        // to prevent ghosting). Clearing overwrites the controller's memory, so it
        // must happen before the upload, not after.
        void PrepareNextQuote(bool clearScreen);
 

        // Refreshes the display with the uploaded image in `PrepareNextQuote()`.
        // 
        // Call this at the exact start of the minute, after `PrepareNextQuote()`.
        void ShowPreparedQuote();


        // Renders the image for the given time (hour:minute), converts it to 4bpp,
        // and pushes it onto the buffer.
        //
        // hour: The hour (0-23) of the quote to render.
        // minute: The minute (0-60) of the quote to render.
        void BufferImage(size_t hour, size_t minute);

        
        // Renders and buffers the image for the minute after quote that the buffer's
        // `tail` is pointing to.
        void RefreshBuffer();


        // Empties the buffer and refills it with the quotes for the minutes *after* `thisMinute`.
        void ResetBuffer(std::chrono::system_clock::time_point thisMinute);

        
        // Converts an 8bpp image buffer to a 4bpp buffer and returns the
        // new 4bpp buffer.
        //
        // `Writer::GenerateQuoteImage()` returns a buffer for an image where each
        // byte represents one pixel (thus, the image has a grayscale range of 0-255).
        // Waveshare recommends using 4bpp for refreshing (grayscale range of 0-15)
        // because "the amount of transmitted data is reduced by half, the
        // transmission speed is twice as fast, and there is no difference in display
        // effect."
        //
        // image: A flat buffer of an 8bpp image to convert to 4bpp
        std::vector<unsigned char> ConvertTo4Bpp(const std::vector<unsigned char>& image);


        // Returns an the image buffer of an 8bpp (bits per pixel) rendered image of
        // random row from the quote cache for the given time (hour:minute).
        //
        // quoteHour: The hour (0-23) of the quote to render.
        // quoteMin: The minute (0-59) of the quote to render.
        //
        // Returns a bitmap of the rendered image.
        std::vector<unsigned char> GetImage(const size_t& quoteHour, const size_t& quoteMin);


        // Returns the hour (0-23) and minute (0-59), local to the RPi's system clock,
        // of a moment in time.
        //
        // moment: The time which the local hour and minute is returned for.
        static std::pair<size_t, size_t> LocalHourMinute(std::chrono::system_clock::time_point moment);


        // Clears the screen, deactivates it, and shuts the clock down.
        //
        // shutdownMsg: The reason for the shutdown to log.
        // isCritical: true to print errMsg via spdlog::critical(), or false to print via spdlog::info().
        void ClearAndExit(const std::string& shutdownMsg, bool isCritical);
};
