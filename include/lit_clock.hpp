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
#include "writer.hpp"
#include "image_buffer.hpp"


class LitClock {
    public:
        LitClock() {
            //Init the BCM2835 Device
            if(DEV_Module_Init()!=0) {
                spdlog::critical("Error: Failed to initialize hardware module.");
                std::exit(1);
            }

            // waveshare screen init 
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
            spdlog::info("Initial screen config complete");

            cacheQuotes();
        }

        Writer writer;
        ImageBuffer buffer;
        size_t bufferedHour = 0; // hour of the last buffered quote
        size_t bufferedMinute = 0; // minute of the last buffered quote
        std::vector<std::vector<std::unordered_map<std::string, std::string>>> quotes;
        IT8951_Dev_Info Dev_Info = {0, 0};
        UWORD Panel_Width;
        UWORD Panel_Height;
        UDOUBLE Init_Target_Memory_Addr;
        
        void cacheQuotes();
        void tick_forward();
        void bufferImage(size_t hour, size_t minute); // Renders and pushes the image for the given (hour, minute) onto the buffer.
        void refreshBuffer();
        void displayQuote();
        void clearScreen();
        std::vector<unsigned char> getImage(const size_t& quoteHour, const size_t& quoteMin);
        
        // Get a device's current hour and minute, in its timezone.
        void getTime(size_t& hour, size_t& minute);
        
        // Advances (hour, minute) by one minute, wrapping hour forward at the top of the hour.
        void advanceTime(size_t& hour, size_t& minute) {
            if (minute == 59) {
                minute = 0;
                hour = (hour == 23) ? 0 : hour + 1;
            } else {
                minute += 1;
            }
        }
};