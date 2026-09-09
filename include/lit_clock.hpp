#pragma once

#include <print>
#include <vector>

extern "C" {
    #include "waveshare-IT8951/lib/e-Paper/EPD_IT8951.h"
    #include "waveshare-IT8951/lib/GUI/GUI_Paint.h"
}

#include "constants.hpp"
#include "writer.hpp"


// A buffer that contains the images to be displayed for the next `BUFFER_SIZE` minutes, including the currently displayed
// image.
struct ImageRingBuffer {
    std::vector<unsigned char> buffer = std::vector<unsigned char>(BUFFER_SIZE, 0);
    int currentIdx = 0; // TODO: fix w/ read & write indices

    int getImageOffset() const {
        return currentIdx * IMAGE_SIZE;
    }

    // call after writing to the buffer
    int nextSlot() const {
        return (currentIdx + 1) % NUM_BUFFERED_IMGS;
    }

};

class LitClock {
    public:
        LitClock() {
            //Init the BCM2835 Device
            if(DEV_Module_Init()!=0) {
                std::println("Error: Failed to initialize hardware module.");
                std::exit(1);
            }
            std::println("here");
            std::cout.flush();

            // waveshare screen init 
            Dev_Info = EPD_IT8951_Init(VCOM);
            Panel_Width = Dev_Info.Panel_W; //TODO: use this instead of SCREEN WIDTH/ SCREEN HEIGHT 
            Panel_Height = Dev_Info.Panel_H;   
            Init_Target_Memory_Addr = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);
            EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, INIT_Mode);
            
            cacheQuotes();     

            // waveshare screen config/setup
            Paint_NewImage(imageRingBuffer.buffer.data(), SCREEN_WIDTH, SCREEN_HEIGHT, 0, WHITE); // rotate=0, adjust WHITE per Waveshare's enum
            Paint_SelectImage(imageRingBuffer.buffer.data());
            Paint_SetRotate(0); // ROTATE_0, per Waveshare.
            Paint_SetMirroring(0); // MIRROR_NONE, per Waveshare.
            Paint_SetBitsPerPixel(8);
            Paint_Clear(0xFF); // clears the screen to white?
        }

        Writer writer;
        ImageRingBuffer imageRingBuffer;
        std::vector<std::vector<std::unordered_map<std::string, std::string>>> quotes;
        IT8951_Dev_Info Dev_Info = {0, 0};
        UWORD Panel_Width;
        UWORD Panel_Height;
        UDOUBLE Init_Target_Memory_Addr;
        
        void tick_forward();
        std::vector<unsigned char> getImage(const size_t& quoteHour, const size_t& quoteMin);
        void cacheQuotes();
        void bufferImage(const std::vector<unsigned char>& image);
        void displayQuote();
        void clearScreen();
        
        // Get a device's current hour and minute, in its timezone.
        void getTime(int& hour, int& minute);
};