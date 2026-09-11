#include "image_generator/writer.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <print>

#include "stb_image_write.h"

#include "constants.hpp"
#include "utils.hpp"

void Writer::saveImages()
{
    std::unordered_map<std::string, std::string> row = 
    {
        {"time", ""},
        {"timestring", ""},
        {"quote", ""},
        {"title", ""},
        {"author", ""},
    };

    std::ifstream quoteFile(projectPath(QUOTES_PATH));
    if (!quoteFile.is_open()) {
        std::println("Error: Failed to open quotes file.");
        return;
    }

    std::string line;
    std::getline(quoteFile, line); // skip the header row
    
    std::string previousTime = "00:00";
    int imgNum = -1;
    size_t quoteCount = 0;

    try { // make an images directory if one doesn't exist yet
        std::filesystem::create_directories(projectPath("images"));
    } catch (const std::filesystem::filesystem_error& e) {
        std::println("Error: Failed to make images/ directory");
    }

    while (std::getline(quoteFile, line))
    { // TODO: function that returns a single row using quoteCount for line #
        quoteCount++;
        std::vector<std::string> splitRow = split(line, CharacterDelimiters().TIMESTR);
        if (splitRow.size() != 5) {
            std::println("Error: Row {} is missing a column.", quoteCount + 1);
            continue;
        }

        row["time"]       = splitRow[0];
        row["timestring"] = splitRow[1];
        row["quote"]      = splitRow[2];
        row["title"]      = splitRow[3];
        row["author"]     = splitRow[4];

        if (row["time"] == previousTime) {
            imgNum++;
        } else {
            std::string currTime = row["time"];
            int prevMin = std::stoi(previousTime.substr(3));
            int currMin = std::stoi(currTime.substr(3));

            if ((currMin - 1 != prevMin) && (previousTime.substr(3) != "59"))
            {
                int missingMin = prevMin + 1;
                std::string missingTime = std::format("{}{:02}", previousTime.substr(0, 2), missingMin);
                std::println("Error: Missing or out-of-order quote for {}", missingTime);
            }

            imgNum = 0;
            previousTime = currTime;
        }

        std::string time = row["time"].replace(2, 1, "");
        std::string filepath = projectPath(IMAGE_PATH + "quote_" + time + "_" + std::to_string(imgNum) + "." + IMAGE_FORMAT);
        std::vector<unsigned char> imgOut = generateQuoteImage(row, INCLUDE_CREDITS);

        if (IMAGE_FORMAT == "bmp") {
            stbi_write_bmp(filepath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, imgOut.data());
        } else if (IMAGE_FORMAT == "png") {
            stbi_write_png(filepath.c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, 1, imgOut.data(), SCREEN_WIDTH);
        } else {
            std::println("Error: {} is an invalid image type", IMAGE_FORMAT);
        }

        std::string progressBar = "Creating images... " + std::to_string(quoteCount);
        std::cout << progressBar << '\r' << std::flush;
    }
    quoteFile.close();
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();

    Writer writer;
    writer.saveImages();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed: " << elapsed.count() << " seconds" << std::endl;
    return 0;
}