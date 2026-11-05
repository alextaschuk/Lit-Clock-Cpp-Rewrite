cmake_minimum_required(VERSION 3.20)
project(LiteraryClock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(GPIO_BACKEND "BCM2835" CACHE STRING "GPIO backend: BCM2835 or LGPIO")
set_property(CACHE GPIO_BACKEND PROPERTY STRINGS BCM2835 LGPIO)

if(GPIO_BACKEND STREQUAL "BCM2835")
    add_library(gpio_backend STATIC extern/bcm2835/src/bcm2835.c)
    target_include_directories(gpio_backend PUBLIC extern/bcm2835/src)
    set(GPIO_DEFINE BCM)
elseif(GPIO_BACKEND STREQUAL "LGPIO")
    find_path(LGPIO_INCLUDE_DIR lgpio.h REQUIRED)
    find_library(LGPIO_LIBRARY lgpio REQUIRED)
    add_library(gpio_backend INTERFACE)
    target_include_directories(gpio_backend INTERFACE "${LGPIO_INCLUDE_DIR}")
    target_link_libraries(gpio_backend INTERFACE "${LGPIO_LIBRARY}")
    set(GPIO_DEFINE LGPIO)
else()
    message(FATAL_ERROR "Unsupported GPIO_BACKEND: ${GPIO_BACKEND}")
endif()

# sbdlog submodule for logging
add_subdirectory(extern/spdlog)

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

# Clock executable
add_executable(clock
    src/clock/clock.cpp
    src/clock/horologium.cpp
    src/clock/image_buffer.cpp
    src/image_generator/writer.cpp
    src/utils.cpp
)

# Executable to save images to folder
add_executable(save_images
    src/image_generator/save_images.cpp
    src/image_generator/writer.cpp
    src/utils.cpp
)

target_include_directories(clock PRIVATE include extern)
target_include_directories(save_images PRIVATE include extern)

target_link_libraries(clock PRIVATE it8951 spdlog::spdlog)
target_link_libraries(save_images PRIVATE it8951 spdlog::spdlog)

target_compile_definitions(clock PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(save_images PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")