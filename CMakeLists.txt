cmake_minimum_required(VERSION 3.20)
project(lit_clock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(lit_clock
    src/writer.cpp
    src/clock.cpp
)

target_include_directories(lit_clock PRIVATE
    include
    extern
)

target_compile_definitions(lit_clock PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
# cd build
# cmake ..
# cmake --build .
# ./img_gen

# https://clang.llvm.org/extra/clang-tidy/