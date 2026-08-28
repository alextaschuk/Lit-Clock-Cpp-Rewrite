#ifndef TCB_H
#define TCB_H
#pragma once // prevent contents of header from being read more than once if included more than once
#include <string>

const int& SCREEN_WIDTH = 800;
const int& SCREEN_HEIGHT = 480;
const float SCALE_MULTIPLIER = 0.99; // to constrain bboxes to make sure text fits

const int& BG_COLOR = 255;
const int& QUOTE_COLOR = 128;
const int& TIME_COLOR = 0;
const int& CREDIT_COLOR = 0;

const std::string& QUOTES_PATH = "quotes.csv";
const std::string& IMAGES_PATH = "images/";
const std::string& IMAGE_FORMAT = "bmp";
const bool& INCLUDE_CREDITS = true;

const float& MIN_FONT_SCALE = 12;
const float& MAX_FONT_SCALE = 150;

#endif