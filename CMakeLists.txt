cmake_minimum_required(VERSION 3.20)
project(LiteraryClock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Define and determine build types: Debug and Release
set(BUILD_TYPES Debug Release)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type: Debug or Release" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${BUILD_TYPES})

if(NOT CMAKE_BUILD_TYPE IN_LIST BUILD_TYPES)
    message(FATAL_ERROR "Unsupported CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE} (use Debug or Release)")
endif()

message(STATUS "LiteraryClock build type: ${CMAKE_BUILD_TYPE}")


# If compiling on RPi, the image generator and the clock are compiled by default.
# If compiling on a non-RPi system, only the image generator is compiled by default.
# To override the system default, pass -DBUILD_CLOCK=ON/OFF, depending on what you
# want.
set(BUILD_CLOCK_DEFAULT OFF)

if(EXISTS "/proc/device-tree/model")
    file(READ "/proc/device-tree/model" DEVICE_MODEL)
    if(DEVICE_MODEL MATCHES "Raspberry Pi")
        set(BUILD_CLOCK_DEFAULT ON)
    endif()
endif()

option(BUILD_CLOCK "Build the clock (needs the display driver and a GPIO library)" ${BUILD_CLOCK_DEFAULT})
message(STATUS "LiteraryClock build clock: ${BUILD_CLOCK}")


if(BUILD_CLOCK)
    # Set the GPIO library that the display driver is built against for the clock.
    # There is no default, so a -DGPIO_BACKEND has to be passed with BCM2835 for
    # RPi <= 4, or LGPIO for RPi >= 5.
    set(GPIO_BACKEND "" CACHE STRING "GPIO backend: BCM2835 or LGPIO")
    set_property(CACHE GPIO_BACKEND PROPERTY STRINGS BCM2835 LGPIO)

    if(GPIO_BACKEND STREQUAL "")
        message(FATAL_ERROR
            "GPIO_BACKEND is not set. Pass -DGPIO_BACKEND=BCM2835 if compiling on RPi <= 4, "
            "-DGPIO_BACKEND=LGPIO if compiling on RPi >= 5, or configure with -DBUILD_CLOCK=OFF "
            "to build only save_images.")

    elseif(GPIO_BACKEND STREQUAL "BCM2835")
        # look for system-wide bcm2835 library
        find_path(BCM2835_INCLUDE_DIR bcm2835.h)
        find_library(BCM2835_LIBRARY bcm2835)

        if(NOT BCM2835_INCLUDE_DIR OR NOT BCM2835_LIBRARY)
            message(FATAL_ERROR
                "The bcm2835 library is not installed. Run scripts/install_clock.sh to install it,"
                "follow Waveshare's instructions to install it manually:"
                "(https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#Working_with_Raspberry_Pi_.28SPI.29),"
                "or configure with -DBUILD_CLOCK=OFF to build only save_images.")
        endif()

        add_library(gpio_backend INTERFACE)
        target_include_directories(gpio_backend INTERFACE "${BCM2835_INCLUDE_DIR}")
        target_link_libraries(gpio_backend INTERFACE "${BCM2835_LIBRARY}")
        set(GPIO_DEFINE BCM)

    elseif(GPIO_BACKEND STREQUAL "LGPIO")
        find_path(LGPIO_INCLUDE_DIR lgpio.h REQUIRED)
        find_library(LGPIO_LIBRARY lgpio REQUIRED)
        if(NOT LGPIO_INCLUDE_DIR OR NOT LGPIO_LIBRARY)
            message(FATAL_ERROR
                "The lg library is not installed. Run scripts/install_clock.sh to install it,"
                "follow Waveshare's instructions to install it manually:"
                "(https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT#Working_with_Raspberry_Pi_.28SPI.29),"
                "or configure with -DBUILD_CLOCK=OFF to build only save_images.")
        endif()

        add_library(gpio_backend INTERFACE)
        target_include_directories(gpio_backend INTERFACE "${LGPIO_INCLUDE_DIR}")
        target_link_libraries(gpio_backend INTERFACE "${LGPIO_LIBRARY}")
        set(GPIO_DEFINE LGPIO)

    else()
        message(FATAL_ERROR "Unsupported GPIO_BACKEND: ${GPIO_BACKEND} (use BCM2835 or LGPIO)")
    endif()


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
    target_compile_definitions(it8951 PUBLIC ${GPIO_DEFINE})
    target_link_libraries(it8951 PUBLIC gpio_backend pthread m)
endif()


# spdlog submodule for logging
add_subdirectory(extern/spdlog)


# core code used by the clock and image generator
add_library(core STATIC
    src/image_generator/writer.cpp
    src/utils.cpp
)

target_include_directories(core PUBLIC include)
target_include_directories(core SYSTEM PUBLIC extern)
target_link_libraries(core PUBLIC spdlog::spdlog)
target_compile_definitions(core PUBLIC PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_options(core PRIVATE -Wall -Wextra)


# Executable to save images to folder (./save_images)
add_executable(save_images
    src/image_generator/save_images.cpp
)

target_compile_options(save_images PRIVATE -Wall -Wextra)
target_link_libraries(save_images PRIVATE core)


# Clock executable to run the clock (./clock)
if(BUILD_CLOCK)
    add_executable(clock
        src/clock/clock.cpp
        src/clock/horologium.cpp
        src/clock/image_buffer.cpp
    )

    target_compile_options(clock PRIVATE -Wall -Wextra)
    target_link_libraries(clock PRIVATE core it8951)
endif()
