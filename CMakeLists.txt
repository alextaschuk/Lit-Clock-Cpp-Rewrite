cmake_minimum_required(VERSION 3.20)
project(cpp-lit-clock)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(lit_clock img_gen.cpp)
target_compile_definitions(lit_clock PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

# cd build
# cmake ..
# cmake --build .
# ./lit_clock

# https://clang.llvm.org/extra/clang-tidy/