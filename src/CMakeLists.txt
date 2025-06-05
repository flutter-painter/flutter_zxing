cmake_minimum_required(VERSION 3.16)

project(flutter_zxing LANGUAGES CXX)

# Set C++17 as required
set(CMAKE_CXX_STANDARD 17)

# Add /FS flag for MSVC to prevent PDB access conflicts (C1041 error)
if(MSVC)
  add_compile_options(/FS)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    flutter_zxing
    SHARED
    zxcommon.cpp
    native_zxing.cpp
    barcode_enhancer.cpp
    Code128Binarizer.cpp
)

# Enable both readers and writers in ZXing
set(ZXING_READERS ON)
set(ZXING_WRITERS ON)

# Add include directories
target_include_directories(flutter_zxing PUBLIC
    "${PROJECT_SOURCE_DIR}/zxing/core/src"  # For headers like ZXing/Barcode.h
    "${PROJECT_BINARY_DIR}/zxing/core"      # For generated headers like Version.h
)

target_link_libraries(flutter_zxing PRIVATE ZXing) # Changed to PRIVATE, as include dirs are now explicit

# use log-lib only for android platform
if (ANDROID)
    find_library(log-lib log)
    target_link_libraries(flutter_zxing ${log-lib})
endif (ANDROID)

# Enable testing and add test directory
enable_testing()
#add_subdirectory(tests)

# Command to update zxing submodule
# cd src/zxing
# git checkout v2.2.0
# git submodule update --init --recursive