#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "../zxing/core/src/ReadBarcode.h"
#include "../zxing/core/src/ReaderOptions.h"
#include "../zxing/core/src/ImageView.h"
#include "../zxing/core/src/ThresholdBinarizer.h"
#include "../zxing/core/src/BinaryBitmap.h"
#include "../zxing/core/src/GlobalHistogramBinarizer.h"
#include "../zxing/core/src/HybridBinarizer.h"
#include "../barcode_enhancer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb_image_write.h"

namespace fs = std::filesystem;

// Helper function to print first few bytes of image data for debugging
void printImageData(const std::vector<uint8_t>& data, size_t width, size_t height) {
    std::cout << "First 10x10 pixels:" << std::endl;
    for (size_t y = 0; y < std::min(size_t(10), height); ++y) {
        for (size_t x = 0; x < std::min(size_t(10), width); ++x) {
            std::cout << std::setw(3) << static_cast<int>(data[y * width + x]) << " ";
        }
        std::cout << std::endl;
    }

    // Add histogram information
    std::vector<int> histogram(256, 0);
    for (const auto& pixel : data) {
        histogram[pixel]++;
    }

    std::cout << "\nHistogram summary:" << std::endl;
    int min_val = 255, max_val = 0;
    double avg_val = 0;
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            min_val = std::min(min_val, i);
            max_val = std::max(max_val, i);
            avg_val += i * histogram[i];
        }
    }
    avg_val /= (width * height);

    std::cout << "Min value: " << min_val << std::endl;
    std::cout << "Max value: " << max_val << std::endl;
    std::cout << "Average value: " << avg_val << std::endl;
}

// Helper function to load image in grayscale format
std::tuple<std::vector<uint8_t>, int, int> loadImage(const fs::path& filepath) {
    std::string pathStr = filepath.string();
    CAPTURE(pathStr); // Catch2 will show this in test output
    
    int width, height, channels;
    uint8_t* data = stbi_load(pathStr.c_str(), &width, &height, &channels, 1);
    
    if (!data) {
        std::cerr << "Failed to load image: " << pathStr << std::endl;
        std::cerr << "STB error: " << stbi_failure_reason() << std::endl;
        FAIL("Image loading failed");
    }
    
    //INFO("Image loaded successfully:");
    //INFO("  Width: " << width);
    //INFO("  Height: " << height);
    //INFO("  Original channels: " << channels);
    
    REQUIRE(width > 0);
    REQUIRE(height > 0);
    REQUIRE(data != nullptr);
    
    std::vector<uint8_t> gray_data(data, data + width * height);
    //INFO("  Gray data size: " << gray_data.size());
    
    // Print first few bytes of image data
    std::cout << "\nImage data preview for: " << filepath.filename().string() << std::endl;
    printImageData(gray_data, width, height);
    
    stbi_image_free(data);
    
    return {gray_data, width, height};
}

// Helper function to print image statistics
void printImageStats(const std::vector<uint8_t>& image, const std::string& label) {
    if (image.empty()) {
        std::cout << label << ": Empty image" << std::endl;
        return;
    }

    auto minmax = std::minmax_element(image.begin(), image.end());
    double sum = std::accumulate(image.begin(), image.end(), 0.0);
    double mean = sum / image.size();

    std::cout << "=== " << label << " ===" << std::endl;
    std::cout << "Min value: " << static_cast<int>(*minmax.first) << std::endl;
    std::cout << "Max value: " << static_cast<int>(*minmax.second) << std::endl;
    std::cout << "Mean value: " << mean << std::endl;

    // Print histogram
    std::vector<int> histogram(256, 0);
    for (uint8_t pixel : image) {
        histogram[pixel]++;
    }

    std::cout << "Histogram peaks:" << std::endl;
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > image.size() / 100) { // Show bins with more than 1% of pixels
            std::cout << "  Value " << i << ": " << histogram[i] << " pixels" << std::endl;
        }
    }
    std::cout << std::endl;
}

// Helper function to save image for debugging
void saveDebugImage(const std::vector<uint8_t>& image, int width, int height, const std::string& filename) {
    stbi_write_png(filename.c_str(), width, height, 1, image.data(), width);
    std::cout << "Saved debug image: " << filename << std::endl;
}

// Helper function to print barcode information
void printBarcodeInfo(const ZXing::Result& result, const std::string& label) {
    std::cout << "=== " << label << " ===" << std::endl;
    std::cout << "Valid: " << (result.isValid() ? "Yes" : "No") << std::endl;
    
    if (result.isValid()) {
        std::cout << "Format: " << ZXing::ToString(result.format()) << std::endl;
        std::cout << "Text: " << result.text() << std::endl;
        std::cout << "Error: " << static_cast<int>(result.error().type()) << std::endl;
        std::cout << "Line Count: " << result.lineCount() << std::endl;
        std::cout << "ECC Level: " << result.ecLevel() << std::endl;
        
        // Position analysis
        auto position = result.position();
        std::cout << "Position Analysis:" << std::endl;
        std::cout << "  TopLeft: (" << position.topLeft().x << ", " << position.topLeft().y << ")" << std::endl;
        std::cout << "  TopRight: (" << position.topRight().x << ", " << position.topRight().y << ")" << std::endl;
        std::cout << "  BottomLeft: (" << position.bottomLeft().x << ", " << position.bottomLeft().y << ")" << std::endl;
        std::cout << "  BottomRight: (" << position.bottomRight().x << ", " << position.bottomRight().y << ")" << std::endl;
        
        // Calculate barcode dimensions
        double width = std::hypot(position.topRight().x - position.topLeft().x,
                                position.topRight().y - position.topLeft().y);
        double height = std::hypot(position.bottomLeft().x - position.topLeft().x,
                                 position.bottomLeft().y - position.topLeft().y);
        double aspectRatio = width / height;
        double centerX = (position.topLeft().x + position.topRight().x +
                         position.bottomLeft().x + position.bottomRight().x) / 4.0;
        double centerY = (position.topLeft().y + position.topRight().y +
                         position.bottomLeft().y + position.bottomRight().y) / 4.0;
        
        std::cout << "Barcode Dimensions:" << std::endl;
        std::cout << "  Width: " << width << " pixels" << std::endl;
        std::cout << "  Height: " << height << " pixels" << std::endl;
        std::cout << "  Aspect Ratio: " << aspectRatio << std::endl;
        std::cout << "  Center Position: (" << centerX << ", " << centerY << ")" << std::endl;
        
        // Calculate skew angles
        double topAngle = std::atan2(position.topRight().y - position.topLeft().y,
                                   position.topRight().x - position.topLeft().x) * 180.0 / M_PI;
        double bottomAngle = std::atan2(position.bottomRight().y - position.bottomLeft().y,
                                      position.bottomRight().x - position.bottomLeft().x) * 180.0 / M_PI;
        double leftAngle = std::atan2(position.bottomLeft().y - position.topLeft().y,
                                    position.bottomLeft().x - position.topLeft().x) * 180.0 / M_PI;
        double rightAngle = std::atan2(position.bottomRight().y - position.topRight().y,
                                     position.bottomRight().x - position.topRight().x) * 180.0 / M_PI;
        
        std::cout << "Skew Analysis:" << std::endl;
        std::cout << "  Top Edge Angle: " << topAngle << " degrees" << std::endl;
        std::cout << "  Bottom Edge Angle: " << bottomAngle << " degrees" << std::endl;
        std::cout << "  Left Edge Angle: " << leftAngle << " degrees" << std::endl;
        std::cout << "  Right Edge Angle: " << rightAngle << " degrees" << std::endl;
    } else {
        std::cout << "Error: " << static_cast<int>(result.error().type()) << std::endl;
    }
    std::cout << std::endl;
}

TEST_CASE("Basic barcode decoding", "[decoding]") {
    // Get the absolute path to the test images
    fs::path currentPath = fs::current_path();
    fs::path rootPath = currentPath;
    
    while (!fs::exists(rootPath / "barcode_images") && rootPath.has_parent_path()) {
        rootPath = rootPath.parent_path();
    }
    
    //INFO("Test images path: " << rootPath / "barcode_images");
    //INFO("Current working directory: " << currentPath);
    //INFO("Root path: " << rootPath);
    
    REQUIRE(fs::exists(rootPath / "barcode_images"));
    
    // Focus on images with their actual expected text values
    std::vector<std::pair<std::string, std::string>> testImages = {
        {"257670HA64SM.jpg", "257670HA64SM"},
        {"257670HA64SM_zoom.jpg", "257670HA64SM"},
        {"800165E_01L_zoom.jpg", "800165E01L"},
        {"800446E_01XL.jpg", "800446E01XL"},
        {"800446E_01XL_zoom.jpg", "800446E01XL"},
        {"800446E_01XL_ugly.jpg", "800446E01XL"},
        {"800446E_01XL_ugly2.jpg", "800446E01XL"},
        {"800446E_01XL_ugly3.jpg", "800446E01XL"},
        {"800446E_01XL_pretty.jpg", "800446E01XL"},
        {"800446E_01XL_perfect.jpg", "800446E01XL"}
    };
    
    for (const auto& [imageName, expectedText] : testImages) {
        SECTION("Testing " + imageName) {
            // Load the image
            fs::path imagePath = rootPath / "barcode_images" / imageName;
            REQUIRE(fs::exists(imagePath));
            
            int width, height, channels;
            std::unique_ptr<uint8_t[]> buffer(stbi_load(imagePath.string().c_str(), &width, &height, &channels, 1));
            REQUIRE(buffer != nullptr);
            
            // Create ImageView for ZXing
            ZXing::ImageView originalView(buffer.get(), width, height, ZXing::ImageFormat::Lum);
            
            // Configure reader for Code 128
            ZXing::ReaderOptions opts;
            opts.setTryHarder(true);
            opts.setTryRotate(true);
            opts.setIsPure(false);  // The image might contain other elements
            opts.setBinarizer(ZXing::Binarizer::LocalAverage);
            opts.setFormats(ZXing::BarcodeFormat::Code128);  // Only look for Code 128
            opts.setMinLineCount(2);  // Require at least 2 scan lines to match
            opts.setTryInvert(true);  // Try both regular and inverted images
            opts.setTryDownscale(true);  // Try downscaling for better detection
            
            bool anySuccess = false;
            std::string decodedText;
            
            // Try original image
            std::cout << "\nTrying Code 128 Config on original image..." << std::endl;
            auto result = ZXing::ReadBarcode(originalView, opts);
            printBarcodeInfo(result, "Original Image Result (Code 128 Config)");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            // Try enhancement
            std::cout << "\nApplying Code 128 specific enhancements..." << std::endl;
            auto enhancedImage = ZXing::Code128Enhancer::enhanceBarcode(originalView, false);
            std::cout << "\nTrying Code 128 Config on enhanced image..." << std::endl;
            result = ZXing::ReadBarcode(enhancedImage, opts);
            printBarcodeInfo(result, "Enhanced Image Result (Code 128 Config)");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            // Try with inversion
            std::cout << "\nTrying Code 128 Config on enhanced+inverted image..." << std::endl;
            auto enhancedInvertedImage = ZXing::Code128Enhancer::enhanceBarcode(originalView, true);
            result = ZXing::ReadBarcode(enhancedInvertedImage, opts);
            printBarcodeInfo(result, "Enhanced+Inverted Image Result (Code 128 Config)");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            // Try different binarizers
            std::cout << "\nTrying LocalAverage binarizer on original image..." << std::endl;
            opts.setBinarizer(ZXing::Binarizer::LocalAverage);
            result = ZXing::ReadBarcode(originalView, opts);
            printBarcodeInfo(result, "LocalAverage Binarizer Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            std::cout << "\nTrying GlobalHistogram binarizer on original image..." << std::endl;
            opts.setBinarizer(ZXing::Binarizer::GlobalHistogram);
            result = ZXing::ReadBarcode(originalView, opts);
            printBarcodeInfo(result, "GlobalHistogram Binarizer Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            std::cout << "\nTrying FixedThreshold binarizer on original image..." << std::endl;
            opts.setBinarizer(ZXing::Binarizer::FixedThreshold);
            result = ZXing::ReadBarcode(originalView, opts);
            printBarcodeInfo(result, "FixedThreshold Binarizer Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            // Try different rotations
            std::cout << "\nTrying 0 degree rotation..." << std::endl;
            result = ZXing::ReadBarcode(originalView, opts);
            printBarcodeInfo(result, "0 Degree Rotation Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            std::cout << "\nTrying 90 degree rotation..." << std::endl;
            result = ZXing::ReadBarcode(ZXing::Code128Enhancer::rotateImage(originalView, 90), opts);
            printBarcodeInfo(result, "90 Degree Rotation Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            std::cout << "\nTrying 180 degree rotation..." << std::endl;
            result = ZXing::ReadBarcode(ZXing::Code128Enhancer::rotateImage(originalView, 180), opts);
            printBarcodeInfo(result, "180 Degree Rotation Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            std::cout << "\nTrying 270 degree rotation..." << std::endl;
            result = ZXing::ReadBarcode(ZXing::Code128Enhancer::rotateImage(originalView, 270), opts);
            printBarcodeInfo(result, "270 Degree Rotation Result");
            if (result.isValid()) {
                anySuccess = true;
                decodedText = result.text();
            }
            
            // Verify that at least one attempt was successful
            REQUIRE(anySuccess);
            
            // If successful, verify the decoded text matches expected
            if (anySuccess) {
                REQUIRE(decodedText == expectedText);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
} 