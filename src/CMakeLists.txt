cmake_minimum_required(VERSION 3.16)

project(flutter_zxing LANGUAGES C CXX)

# Add CImg header-only library
include(FetchContent)
FetchContent_Declare(
    cimg
    GIT_REPOSITORY https://github.com/dtschump/CImg.git
    GIT_TAG master  # You may want to pin to a specific version
)
FetchContent_MakeAvailable(cimg)

add_library(
    flutter_zxing
    SHARED
    zxcommon.cpp
    native_zxing.cpp
    barcode_enhancer.cpp
)

set(ZXING_WRITERS ON)

add_subdirectory(zxing/core EXCLUDE_FROM_ALL)

target_link_libraries(flutter_zxing ZXing)

# CImg configuration
target_include_directories(flutter_zxing PRIVATE ${cimg_SOURCE_DIR})
target_compile_definitions(flutter_zxing PRIVATE cimg_display=0 cimg_use_jpeg=0 cimg_use_png=0)

# use log-lib only for android platform
if (ANDROID)
    find_library(log-lib log)
    target_link_libraries(flutter_zxing ${log-lib})
endif (ANDROID)

# Command to update zxing submodule
# cd src/zxing
# git checkout v2.2.0
# git submodule update --init --recursive