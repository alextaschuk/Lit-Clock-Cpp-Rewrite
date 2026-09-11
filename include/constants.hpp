// All global constant variables for configuring image and screen properties
// are stored here.
#pragma once

#include <string>

//inline constexpr int SCREEN_WIDTH = 800;
//inline constexpr int SCREEN_HEIGHT = 480;
inline constexpr int SCREEN_WIDTH = 1448;
inline constexpr int SCREEN_HEIGHT = 1072;
inline constexpr int IMAGE_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;
inline constexpr double VCOM = -2.79;// set to VCOM value that's on the screen's FPC

inline constexpr int BG_COLOR = 255;
inline constexpr int QUOTE_COLOR = 128;
inline constexpr int TIME_COLOR = 0;
inline constexpr int CREDIT_COLOR = 0;

inline const std::string QUOTES_PATH = "share/quotes.csv";
//inline const std::string QUOTES_PATH = "test.csv";
inline const std::string IMAGE_PATH = "images/";
inline const std::string IMAGE_FORMAT = "png";
inline constexpr bool INCLUDE_CREDITS = true;

inline constexpr float MIN_FONT_SCALE = 12.0f;
inline constexpr float MAX_FONT_SCALE = 500.0f;

inline constexpr int MAX_IMAGES_TO_BUFFER = 3; // buffer 3 images at a time.
inline constexpr int BUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT * MAX_IMAGES_TO_BUFFER;