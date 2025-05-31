cmake_minimum_required(VERSION 3.16)
project(barcode_enhancement_test)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags for C++20 features
if(MSVC)
    add_compile_options(/std:c++20 /Zc:__cplusplus)
else()
    add_compile_options(-std=c++20)
endif()

# Configure ZXing library
set(ZXING_READERS ON CACHE BOOL "Enable barcode reading support")
set(ZXING_WRITERS OFF CACHE BOOL "Disable barcode writing support")
set(ZXING_USE_SYSTEM_LIBS OFF CACHE BOOL "Don't use system libraries")
set(ZXING_EXPERIMENTAL_API OFF CACHE BOOL "Don't use experimental API")
set(ZXING_BUILD_EXAMPLES OFF CACHE BOOL "Don't build examples")
set(ZXING_BUILD_BLACKBOX_TESTS OFF CACHE BOOL "Don't build blackbox tests")
set(ZXING_BUILD_UNIT_TESTS OFF CACHE BOOL "Don't build unit tests")

# Add compile definitions
add_definitions(-DUNICODE -D_UNICODE -DZXING_READERS)

# Add stb_image header-only library
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Add ZXing library
add_subdirectory(src/zxing/core)

# Add Catch2 for testing
Include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)

FetchContent_MakeAvailable(Catch2)

# Add source files
add_executable(barcode_tests
    src/tests/barcode_test.cpp
    src/barcode_enhancer.cpp
)

# Link against ZXing library
target_link_libraries(barcode_tests PRIVATE ZXing Catch2::Catch2WithMain)

# Add include directories
target_include_directories(barcode_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/zxing/core/src
) 