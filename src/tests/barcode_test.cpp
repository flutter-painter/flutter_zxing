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

#include <chrono>
#include <map>

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

// Structure to hold decode result metrics
struct DecodeResult {
    bool success;
    std::string text;
    double processingTimeMs;
    std::string format;
    int rotation;
    std::string method;
    
    DecodeResult(bool s = false, const std::string& t = "", double time = 0.0, 
                const std::string& fmt = "", int rot = 0, const std::string& mthd = "")
        : success(s), text(t), processingTimeMs(time), format(fmt), rotation(rot), method(mthd) {}
};

// Global variables for tracking statistics
int totalImages = 0;
int originalSuccesses = 0;
int enhancedSuccesses = 0;
int enhancedInvertedSuccesses = 0;
int originalOnlySuccesses = 0;
int enhancedOnlySuccesses = 0;
int bothSuccesses = 0;
double totalOriginalTime = 0.0;
double totalEnhancedTime = 0.0;
double totalEnhancedInvertedTime = 0.0;

// Map to store all results
std::map<std::string, std::vector<DecodeResult>> allResults;

// Helper function to test an image with specific configuration and record metrics
DecodeResult testImageWithConfig(const ZXing::ImageView& imageView, const ZXing::ReaderOptions& opts, 
                            const std::string& methodName, int rotation = 0) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    ZXing::ImageView rotatedView = imageView;
    if (rotation > 0) {
        rotatedView = ZXing::Code128Enhancer::rotateImage(imageView, rotation);
    }
    
    auto result = ZXing::ReadBarcode(rotatedView, opts);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    std::string formatName = "Unknown";
    if (result.isValid()) {
        formatName = ZXing::ToString(result.format());
    }
    
    return DecodeResult(result.isValid(), result.text(), duration, formatName, rotation, methodName);
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
    
    // Reset global statistics for this test run
    totalImages = 0;
    originalSuccesses = 0;
    enhancedSuccesses = 0;
    enhancedInvertedSuccesses = 0;
    enhancedOnlySuccesses = 0;
    originalOnlySuccesses = 0;
    bothSuccesses = 0;
    totalOriginalTime = 0.0;
    totalEnhancedTime = 0.0;
    totalEnhancedInvertedTime = 0.0;
    
    // Clear previous results
    allResults.clear();
    
    // Focus on images with their actual expected text values
    std::vector<std::pair<std::string, std::string>> testImages = {
        {"257670HA64SM.jpg", "257670HA64SM"},
        {"257670HA64SM_zoom.jpg", "257670HA64SM"},
        {"800165E_01L_zoom.jpg", "800165E01L"},
        {"800446E_01XL.jpg", "800446E01XL"},
        {"800446E_01XL_zoom.jpg", "800446E01XL"},
        {"8001653_01L_ugly.jpg", "800165301L"},
        {"8001653_01L_ugly2.jpg", "800165301L"},
        {"8001653_01L_ugly3.jpg", "800165301L"},
        {"8001653_01L_pretty.jpg", "800165301L"},
        {"8001653_01L_perfect.jpg", "800165301L"}
    };
    
    // Create debug directory if it doesn't exist
    fs::path debugDir = rootPath / "debug_images";
    if (!fs::exists(debugDir)) {
        fs::create_directory(debugDir);
    }
    
    for (const auto& [imageName, expectedText] : testImages) {
        totalImages++;
        SECTION("Testing " + imageName) {
            // Load the image
            fs::path imagePath = rootPath / "barcode_images" / imageName;
            if (!fs::exists(imagePath)) {
                std::cout << "WARNING: Image file not found: " << imagePath.string() << std::endl;
                continue; // Skip this image and move to the next one
            }
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
            
            // Store results for this image
            std::vector<DecodeResult> imageResults;
            
            // Try original image
            std::cout << "\nTrying Code 128 Config on original image..." << std::endl;
            auto originalResult = testImageWithConfig(originalView, opts, "Original");
            imageResults.push_back(originalResult);
            
            // Save debug image
            std::vector<uint8_t> originalImageData(buffer.get(), buffer.get() + width * height);
            saveDebugImage(originalImageData, width, height, 
                          (debugDir / ("original_" + imageName)).string());
            
            ZXing::Result originalZXResult;
            if (originalResult.success) {
                originalZXResult = ZXing::ReadBarcode(originalView, opts);
            }
            printBarcodeInfo(originalZXResult, "Original Image Result (Code 128 Config)");
            
            if (originalResult.success) {
                anySuccess = true;
                decodedText = originalResult.text;
                originalSuccesses++;
                totalOriginalTime += originalResult.processingTimeMs;
            }
            
            // Try enhancement
            std::cout << "\nApplying Code 128 specific enhancements..." << std::endl;
            auto enhancedImage = ZXing::Code128Enhancer::enhanceBarcode(originalView, true, true);
            
            // Save enhanced debug image
            std::vector<uint8_t> enhancedImageData;
            enhancedImageData.assign(enhancedImage.data(), enhancedImage.data() + enhancedImage.width() * enhancedImage.height());
            saveDebugImage(enhancedImageData, enhancedImage.width(), enhancedImage.height(),
                          (debugDir / ("enhanced_" + imageName)).string());
            
            std::cout << "\nTrying Code 128 Config on enhanced image..." << std::endl;
            auto enhancedResult = testImageWithConfig(enhancedImage, opts, "Enhanced");
            imageResults.push_back(enhancedResult);
            
            ZXing::Result enhancedZXResult;
            if (enhancedResult.success) {
                enhancedZXResult = ZXing::ReadBarcode(enhancedImage, opts);
            }
            printBarcodeInfo(enhancedZXResult, "Enhanced Image Result (Code 128 Config)");
            
            if (enhancedResult.success) {
                anySuccess = true;
                decodedText = enhancedResult.text;
                enhancedSuccesses++;
                totalEnhancedTime += enhancedResult.processingTimeMs;
                
                if (!originalResult.success) {
                    enhancedOnlySuccesses++;
                }
            }
            
            // Try with inversion
            std::cout << "\nTrying Code 128 Config on enhanced+inverted image..." << std::endl;
            auto enhancedInvertedImage = ZXing::Code128Enhancer::enhanceBarcode(originalView, true, true);
            
            // Save enhanced+inverted debug image
            std::vector<uint8_t> enhancedInvertedImageData;
            enhancedInvertedImageData.assign(enhancedInvertedImage.data(), enhancedInvertedImage.data() + enhancedInvertedImage.width() * enhancedInvertedImage.height());
            saveDebugImage(enhancedInvertedImageData, enhancedInvertedImage.width(), enhancedInvertedImage.height(),
                          (debugDir / ("enhanced_inverted_" + imageName)).string());
            
            auto enhancedInvertedResult = testImageWithConfig(enhancedInvertedImage, opts, "EnhancedInverted");
            imageResults.push_back(enhancedInvertedResult);
            
            ZXing::Result enhancedInvertedZXResult;
            if (enhancedInvertedResult.success) {
                enhancedInvertedZXResult = ZXing::ReadBarcode(enhancedInvertedImage, opts);
            }
            printBarcodeInfo(enhancedInvertedZXResult, "Enhanced+Inverted Image Result (Code 128 Config)");
            
            if (enhancedInvertedResult.success) {
                anySuccess = true;
                decodedText = enhancedInvertedResult.text;
                enhancedInvertedSuccesses++;
                totalEnhancedInvertedTime += enhancedInvertedResult.processingTimeMs;
                
                if (!originalResult.success && !enhancedResult.success) {
                    // Only enhanced+inverted worked
                    enhancedOnlySuccesses++;
                }
            }
            
            // Track if original was successful but enhanced methods weren't
            if (originalResult.success && !enhancedResult.success && !enhancedInvertedResult.success) {
                originalOnlySuccesses++;
            }
            
            // Track if both original and any enhanced method were successful
            if (originalResult.success && (enhancedResult.success || enhancedInvertedResult.success)) {
                bothSuccesses++;
            }
            
            // Store all results for this image
            allResults[imageName] = imageResults;
            
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
    auto result = Catch::Session().run(argc, argv);
    
    // After all tests complete, print enhancement effectiveness report
    if (result == 0) {
        std::cout << "\n\n=== ENHANCEMENT EFFECTIVENESS REPORT ===" << std::endl;
        std::cout << "Total images tested: " << totalImages << std::endl;
        
        if (totalImages > 0) {
            double originalRate = (originalSuccesses * 100.0) / totalImages;
            double enhancedRate = (enhancedSuccesses * 100.0) / totalImages;
            double enhancedInvertedRate = (enhancedInvertedSuccesses * 100.0) / totalImages;
            double combinedEnhancedRate = ((enhancedSuccesses + enhancedInvertedSuccesses - bothSuccesses) * 100.0) / totalImages;
            
            std::cout << "Original decode success rate: " << originalRate << "%" << std::endl;
            std::cout << "Enhanced decode success rate: " << enhancedRate << "%" << std::endl;
            std::cout << "Enhanced+Inverted decode success rate: " << enhancedInvertedRate << "%" << std::endl;
            
            std::cout << "\nImages decoded ONLY with original: " << originalOnlySuccesses 
                      << " (" << (originalOnlySuccesses * 100.0 / totalImages) << "%)" << std::endl;
            std::cout << "Images decoded ONLY with enhancement: " << enhancedOnlySuccesses 
                      << " (" << (enhancedOnlySuccesses * 100.0 / totalImages) << "%)" << std::endl;
            std::cout << "Images decoded with both methods: " << bothSuccesses 
                      << " (" << (bothSuccesses * 100.0 / totalImages) << "%)" << std::endl;
            
            std::cout << "\nNet improvement from enhancement: " 
                      << (enhancedOnlySuccesses * 100.0 / totalImages) << "%" << std::endl;
            
            // Performance metrics
            if (originalSuccesses > 0) {
                std::cout << "\nAverage processing time (original): " 
                          << (totalOriginalTime / originalSuccesses) << " ms" << std::endl;
            }
            if (enhancedSuccesses > 0) {
                std::cout << "Average processing time (enhanced): " 
                          << (totalEnhancedTime / enhancedSuccesses) << " ms" << std::endl;
            }
            if (enhancedInvertedSuccesses > 0) {
                std::cout << "Average processing time (enhanced+inverted): " 
                          << (totalEnhancedInvertedTime / enhancedInvertedSuccesses) << " ms" << std::endl;
            }
            
            // Save report to file
            std::ofstream reportFile("enhancement_report.txt");
            if (reportFile.is_open()) {
                reportFile << "=== ENHANCEMENT EFFECTIVENESS REPORT ===\n";
                reportFile << "Total images tested: " << totalImages << "\n";
                reportFile << "Original decode success rate: " << originalRate << "%\n";
                reportFile << "Enhanced decode success rate: " << enhancedRate << "%\n";
                reportFile << "Enhanced+Inverted decode success rate: " << enhancedInvertedRate << "%\n\n";
                reportFile << "Images decoded ONLY with original: " << originalOnlySuccesses 
                          << " (" << (originalOnlySuccesses * 100.0 / totalImages) << "%)\n";
                reportFile << "Images decoded ONLY with enhancement: " << enhancedOnlySuccesses 
                          << " (" << (enhancedOnlySuccesses * 100.0 / totalImages) << "%)\n";
                reportFile << "Images decoded with both methods: " << bothSuccesses 
                          << " (" << (bothSuccesses * 100.0 / totalImages) << "%)\n\n";
                reportFile << "Net improvement from enhancement: " 
                          << (enhancedOnlySuccesses * 100.0 / totalImages) << "%\n";
                reportFile.close();
            }
        }
    }
    
    return result;
}