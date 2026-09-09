cmake_minimum_required(VERSION 3.20)
project(lit_clock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(clock
    src/writer.cpp
    src/lit_clock.cpp
)

add_executable(save_images
    src/writer.cpp
    src/save_images.cpp
)

target_include_directories(clock PRIVATE include extern)
target_include_directories(save_images PRIVATE include extern)


target_compile_definitions(clock PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_definitions(save_images PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# config for waveshare it8951 libraries
file(GLOB IT8951_SOURCES
    extern/waveshare-IT8951/lib/e-Paper/*.c
    extern/waveshare-IT8951/lib/Config/*.c
    extern/waveshare-IT8951/lib/GUI/*.c
    extern/waveshare-IT8951/lib/Fonts/*.c
)

add_library(it8951 STATIC ${IT8951_SOURCES})
target_include_directories(it8951 PUBLIC extern)
target_compile_definitions(it8951 PUBLIC BCM)
target_link_libraries(it8951 PUBLIC bcm2835 pthread m)

target_link_libraries(clock PRIVATE it8951)

# cd build
# cmake ..
# cmake --build .
# ./img_gen

# https://clang.llvm.org/extra/clang-tidy/