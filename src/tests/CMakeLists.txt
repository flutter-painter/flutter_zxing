cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Download and configure Catch2
Include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Create test executable
add_executable(barcode_tests barcode_test.cpp)

# Add include directories
target_include_directories(barcode_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/zxing/core/src
    ${cimg_SOURCE_DIR}
)

# Link against Catch2 and our libraries
target_link_libraries(barcode_tests PRIVATE 
    Catch2::Catch2WithMain
    ZXing
    flutter_zxing
)

# Add test to CTest
add_test(NAME barcode_tests COMMAND barcode_tests) 