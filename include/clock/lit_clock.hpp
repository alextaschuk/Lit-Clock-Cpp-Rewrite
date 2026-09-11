#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

extern "C" {
    #include "waveshare-IT8951/lib/e-Paper/EPD_IT8951.h"
    #include "waveshare-IT8951/lib/GUI/GUI_Paint.h"
}

#include "spdlog/spdlog.h" // logger

#include "constants.hpp"
#include "image_generator/writer.hpp"
#include "image_buffer.hpp"


class LitClock
{
    public:
        LitClock()
        {
            if(DEV_Module_Init()!=0) {
                spdlog::critical("Error: Failed to initialize the BCM2835 hardware module.");
                std::exit(1);
            }

            std::uint16_t VCOM_Scaled = static_cast<std::uint16_t>(std::round(std::abs(VCOM) * 1000.0));
            Dev_Info = EPD_IT8951_Init(VCOM_Scaled);

            Panel_Width = Dev_Info.Panel_W; //TODO: use this instead of SCREEN WIDTH/ SCREEN HEIGHT 
            Panel_Height = Dev_Info.Panel_H;   
            Init_Target_Memory_Addr = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);
            spdlog::info("EPD intialized");
            EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, INIT_Mode);

            // waveshare screen config/setup
            Paint_NewImage(buffer.data.data(), SCREEN_WIDTH, SCREEN_HEIGHT, 0, WHITE); // rotate=0, adjust WHITE per Waveshare's enum
            Paint_SelectImage(buffer.data.data());
            Paint_SetRotate(0); // ROTATE_0, per Waveshare.
            Paint_SetMirroring(0); // MIRROR_NONE, per Waveshare.
            Paint_SetBitsPerPixel(8);
            spdlog::info("Initial Waveshare screen config complete");

            cacheQuotes();
        }

        Writer writer;

        ImageBuffer buffer; // Ring buffer containing the next `MAX_IMAGES_TO_BUFFER` number of images.
        size_t bufferedHour = 0;    // Hour of the last buffered quote
        size_t bufferedMinute = 0;  // Minute of the last buffered quote

        std::vector<std::vector<std::unordered_map<std::string, std::string>>> quotes; // A cache for the quotes from a CSV file.

        IT8951_Dev_Info Dev_Info = {0, 0}; // Contains info about the screen for Waveshare's drivers.
        UWORD Panel_Width;  // width of the screen, in pixels
        UWORD Panel_Height; // height of the screen, in pixels
        UDOUBLE Init_Target_Memory_Addr; // The memory address on the IT8951 controller's onboard memory where pixel data should be written before a display refresh.
        
        // Parses the CSV of quotes and store them in a vector.
        void cacheQuotes();

        // Advances the display by one minute: displays the image at the front of the buffer and refills the buffer.
        // 
        // Is intended be called at the 59th second of every minute by `main()`. At the top of every hour (e.g. 12:00), a 
        // refresh is performed on the screen (all pixels are set to white) prior to displaying the next image. This helps 
        // prevent ghosting.
        void tick_forward();

        // Renders the image for the given time (hour:minute), converts it to 4bpp, and pushes it onto the buffer.
        //
        // hour: The hour (0-23) of the quote to render.
        // minute: The minute (0-60) of the quote to render.
        void bufferImage(size_t hour, size_t minute);

        // Renders and buffers the image for the minute after quote that the buffer's `tail` is pointing to.
        void refreshBuffer();

        // Converts an image's 8bpp (bits per pixel) buffer to 4bpp and returns the new 4bpp buffer.
        //
        // `Writer::generateQuoteImage()` returns a buffer for an image where each byte represents one pixel (thus,
        // the image has a grayscale range of 0-255). Waveshare recommends using 4bpp for refreshing (grayscale
        // range of 0-15) because " the amount of transmitted data is reduced by half, the transmission speed is
        // twice as fast, and there is no difference in display effect."
        //
        // image: A flat buffer of an 8bpp image to convert to 4bpp
        std::vector<unsigned char> convertTo4bpp(const std::vector<unsigned char>& image);

        // Pops the image from the front of the buffer (the image that the buffer's `head` is pointing to) and
        // displays it onto the screen.
        void displayQuote();

        // Chooses a random row from the quote cache for the given time (hour:minute) and returns the row's rendered image.
        //
        // quoteHour: The hour (0-23) of the quote to render.
        // quoteMin: The minute (0-59) of the quote to render.
        //
        // Returns a bitmap of the rendered image.
        std::vector<unsigned char> getImage(const size_t& quoteHour, const size_t& quoteMin);
        
        // Gets the current local hour and minute from the system clock.
        //
        // hour: output parameter. Set to the current local hour (0-23).
        // minute: output parameter. Set to the current local minute (0-59).
        void getTime(size_t& hour, size_t& minute);
        
        // Advances the given time (hour:minute) by one minute, wrapping at the top of the hour.
        //
        // hour: output parameter. The hour of the next minute (0-23).
        // minute: output parameter. The next minute (0-59).
        void advanceTime(size_t& hour, size_t& minute);
};