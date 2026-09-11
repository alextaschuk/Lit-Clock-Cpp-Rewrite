cmake_minimum_required(VERSION 3.20)
project(lit_clock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# sbdlog submodule for logging
add_subdirectory(extern/spdlog)

# bcm2835 (library for GPIO/SPI backend)
add_library(bcm2835 STATIC extern/bcm2835/src/bcm2835.c)
target_include_directories(bcm2835 PUBLIC extern/bcm2835/src)

# Waveshare IT8951 Driver
file(GLOB IT8951_SOURCES
    extern/waveshare-IT8951/lib/e-Paper/*.c
    #extern/waveshare-IT8951/lib/Config/*.c
    extern/waveshare-IT8951/lib/GUI/*.c
    extern/waveshare-IT8951/lib/Fonts/*.c
)
# add only DEV_Config file from Config folder
list(APPEND IT8951_SOURCES extern/waveshare-IT8951/lib/Config/DEV_Config.c)

add_library(it8951 STATIC ${IT8951_SOURCES})
target_include_directories(it8951 PUBLIC extern)
target_compile_definitions(it8951 PUBLIC BCM)
target_link_libraries(it8951 PUBLIC bcm2835 pthread m)

# Clock executable
add_executable(clock
    src/writer.cpp
    src/utils.cpp
    src/lit_clock.cpp
)

# Executable to save images to folder
add_executable(save_images
    src/writer.cpp
    src/utils.cpp
    src/save_images.cpp
)

target_include_directories(clock PRIVATE include extern)
target_include_directories(save_images PRIVATE include extern)

target_link_libraries(clock PRIVATE it8951 spdlog::spdlog)
target_link_libraries(save_images PRIVATE it8951 spdlog::spdlog)

target_compile_definitions(clock PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(save_images PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")