#include "image_generator/writer.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

#include "spdlog/spdlog.h"
#include "stb/stb_image_write.h"

#include "constants.hpp"
#include "utils.hpp"

void Writer::SaveImages()
{
    try {
        InitFont(projectPath("/share/fonts/Bookerly.ttf"), fonts.regularBuf, fonts.regular);
        InitFont(projectPath("/share/fonts/Bookerly-Italic.ttf"), fonts.italicBuf, fonts.italic);
        InitFont(projectPath("/share/fonts/Bookerly-Bold.ttf"), fonts.boldBuf, fonts.bold);
        InitFont(projectPath("/share/fonts/Bookerly-Bold-Italic.ttf"), fonts.italicBoldBuf, fonts.italicBold);
        InitFont(projectPath("/share/fonts/Bookerly-Bold.ttf"), fonts.creditBuf, fonts.credit);
    } catch (const std::runtime_error&) {
        spdlog::critical("Failed to initialize one or more fonts.");
        return;
    }

    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open()) {
        spdlog::critical("Failed to open CSV file at {}.", QUOTES_PATH);
        return;
    }
    
    int imgNum = -1; // appended to the end of an image file
    std::string previousTime = "00:00";
    std::string strRow;
    std::getline(quoteFile, strRow); // skip the header row
    size_t lineNum = 1;
    Row row;


    try { // make an images directory if one doesn't exist yet
        std::filesystem::create_directories(projectPath("images"));
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::critical("Failed to make an images/ directory.");
        return;
    }

    while (std::getline(quoteFile, strRow))
    { // TODO: function that returns a single row using rowNum for line #
        lineNum++;
        std::vector<std::string> splitRow = split(strRow, '|');
        if (splitRow.size() != 5) {
            spdlog::warn("Row {} was skipped because it is missing a column.", lineNum);
            continue;
        }

        row.lineNum     = lineNum;
        row.time        = splitRow[0];
        row.timestring  = splitRow[1];
        row.quote       = splitRow[2];
        row.title       = splitRow[3];
        row.author      = splitRow[4];

        if (row.time == previousTime) {
            imgNum++;
        } else {
            std::string currTime = row.time;
            try {
                // track number of minutes since 00:00. The current row's time may be at most 1 minute greater than the previous row.
                int prevTotal = std::stoi(previousTime.substr(0, 2)) * 60 + std::stoi(previousTime.substr(3));
                int currTotal = std::stoi(currTime.substr(0, 2)) * 60 + std::stoi(currTime.substr(3));
                int expectedTotal = (prevTotal + 1) % 1440;

                if (currTotal != expectedTotal) {
                    std::string expectedTime = std::format("{:02}:{:02}", expectedTotal / 60, expectedTotal % 60);
                    spdlog::warn("Row {}'s time is out-of-order (expected a quote for {}, got {}).", lineNum, expectedTime, currTime);
                }

                imgNum = 0;
                previousTime = currTime;
            }
            catch (const std::invalid_argument&) {
                spdlog::warn("previous time \"{}\" or current time \"{}\" doesn't have a valid minute", previousTime, currTime);
                continue;
            }
            catch (const std::out_of_range&) {
                spdlog::warn("previous time \"{}\" or current time \"{}\" has a minute too large for an int", previousTime, currTime);
                continue;
            }
        }

        std::string time = row.time;
        time = time.replace(2, 1, "");
        std::string filename = std::format("{}_{}.{}", time, std::to_string(imgNum), IMAGE_FORMAT); // e.g. 1357_2.png for 13:57's 3rd quote
        std::string filepath = projectPath(IMAGE_PATH + filename);

        std::vector<unsigned char> imgOut = GenerateQuoteImage(row, INCLUDE_CREDITS);
        
        if (IMAGE_FORMAT == "bmp")
            stbi_write_bmp(filepath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, imgOut.data());
        else if (IMAGE_FORMAT == "png")
            stbi_write_png(filepath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, imgOut.data(), SCREEN_WIDTH);
        else {
            spdlog::error("{} is an invalid image type (must be \"png\" or \"bmp\").", IMAGE_FORMAT);
            return;
        }

        std::string progressBar = "Creating images... " + std::to_string(lineNum);
        std::cout << progressBar << '\r' << std::flush;
    }
    quoteFile.close();
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();

    Writer writer;
    writer.SaveImages();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed: " << elapsed.count() << " seconds" << std::endl;
    return 0;
}