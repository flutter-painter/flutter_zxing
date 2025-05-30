cmake_minimum_required(VERSION 3.10)
project(barcode_enhancement_test)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add CImg header-only library
include_directories(${CMAKE_SOURCE_DIR}/third_party/CImg)

# Add ZXing library from Flutter project
include_directories(${CMAKE_SOURCE_DIR}/src/zxing)

# Add source files
add_executable(test_enhancement
    src/test_enhancement.cpp
    src/barcode_enhancer.cpp
)

# Include directories
target_include_directories(test_enhancement PRIVATE
    ${CMAKE_SOURCE_DIR}/src
) 