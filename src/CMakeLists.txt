cmake_minimum_required(VERSION 3.16)

project(flutter_zxing LANGUAGES C CXX)

# Set C++17 as required
set(CMAKE_CXX_STANDARD 17)
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

add_subdirectory(zxing/core EXCLUDE_FROM_ALL)

target_link_libraries(flutter_zxing ZXing)

# use log-lib only for android platform
if (ANDROID)
    find_library(log-lib log)
    target_link_libraries(flutter_zxing ${log-lib})
endif (ANDROID)

# Enable testing and add test directory
enable_testing()
add_subdirectory(tests)

# Command to update zxing submodule
# cd src/zxing
# git checkout v2.2.0
# git submodule update --init --recursive