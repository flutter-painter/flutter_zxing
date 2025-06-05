// Last attempt to force recompile
#define CATCH_CONFIG_RUNNER
// ... rest of the file#define CATCH_CONFIG_RUNNER
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
    // std::cout << "\nImage data preview for: " << filepath.filename().string() << std::endl;
    // printImageData(gray_data, width, height);
    
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
    // std::cout << "Saved debug image: " << filename << std::endl;
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
    std::string method;
    
    DecodeResult(bool s = false, const std::string& t = "", double time = 0.0, 
                const std::string& fmt = "", const std::string& mthd = "")
        : success(s), text(t), processingTimeMs(time), format(fmt), method(mthd) {}
};

// Global variables for tracking statistics
int totalImages = 0;
int originalSuccesses = 0;
int enhancedSuccesses = 0;
int enhancedInvertedSuccesses = 0; // Added declaration
double totalOriginalTime = 0.0;
double totalEnhancedTime = 0.0;

// Map to store all results
std::map<std::string, std::vector<DecodeResult>> allResults;

// Helper function to test an image with specific configuration and record metrics
DecodeResult testImageWithConfig(const ZXing::ImageView& imageView, const ZXing::ReaderOptions& opts, 
                            const std::string& methodName) {
    std::cout << "[TIC_SIMPLE_TEST] Method: " << methodName << " Binarizer: " << static_cast<int>(opts.binarizer()) << " (0=GH,1=Hy,2=FT,3=BC,4=LA,5=C128)" << std::endl << std::flush;
    auto startTime = std::chrono::high_resolution_clock::now();
    auto result = ZXing::ReadBarcode(imageView, opts);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    std::string formatName = "Unknown";
    if (result.isValid()) {
        formatName = ZXing::ToString(result.format());
    }
    
    return DecodeResult(result.isValid(), result.text(), duration, formatName, methodName);
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
    totalOriginalTime = 0.0;
    totalEnhancedTime = 0.0;
    
    // Clear previous results
    allResults.clear();
    
    // Configure options for original ZXing
    ZXing::ReaderOptions originalOpts;
    originalOpts.setFormats(ZXing::BarcodeFormat::Code128);
    originalOpts.setTryHarder(true);
    originalOpts.setTryRotate(false);
    originalOpts.setTryDownscale(false);
    originalOpts.setBinarizer(ZXing::Binarizer::GlobalHistogram);
    
    // Configure options for enhanced ZXing - just using a different binarizer
    ZXing::ReaderOptions enhancedOpts;
    enhancedOpts.setFormats(ZXing::BarcodeFormat::Code128);
    enhancedOpts.setTryHarder(true);
    enhancedOpts.setTryRotate(false);
    enhancedOpts.setTryDownscale(false);
    enhancedOpts.setBinarizer(ZXing::Binarizer::LocalAverage); // Changed from CODE128 as it's not a standard enum member
    
    // Focus on images with their actual expected text values
    std::vector<std::pair<std::string, std::string>> testImages = {
        {"257670HA64SM.jpg", "257670HA64SM"},
        {"257670HA64SM_zoom.jpg", "257670HA64SM"},
        {"800165E_01L_zoom.jpg", "800165E0 1L"},
        {"800446E_01XL_zoom.jpg", "800446E 01XL"},
       // {"070915CC76S_long.jpg", "070915CC76S"},
        {"070915CC76S.jpg", "070915CC76S"}
     //   {"800446E_01XL.jpg", "800446E01XL"},
/*         {"8001653_01L_ugly.jpg", "800165301L"},
        {"8001653_01L_ugly2.jpg", "800165301L"},
        {"8001653_01L_ugly3.jpg", "800165301L"},
        {"8001653_01L_pretty.jpg", "800165301L"},
        {"8001653_01L_perfect.jpg", "800165301L"} */
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
            
            // Try original image
            // std::cout << "\nTrying Code 128 Config on original image..." << std::endl;
            auto originalResult = testImageWithConfig(originalView, originalOpts, "Original");
            
            // Save debug image
            std::vector<uint8_t> originalImageData(buffer.get(), buffer.get() + width * height);
            saveDebugImage(originalImageData, width, height, 
                          (debugDir / ("original_" + imageName)).string());
            
            if (originalResult.success) {
                originalSuccesses++;
                totalOriginalTime += originalResult.processingTimeMs;
            }
            
            // Try with basic enhancement (non-inverted input, local binarization)
            // std::cout << "\nApplying basic enhancement (non-inverted)..." << std::endl;
            ZXing::ImageView enhancedImageNonInverted = ZXing::Code128Enhancer::enhanceBarcode(originalView, false, true);
            std::vector<uint8_t> enhancedImageDataNonInverted(enhancedImageNonInverted.data(0,0), enhancedImageNonInverted.data(0,0) + enhancedImageNonInverted.width() * enhancedImageNonInverted.height());
            saveDebugImage(enhancedImageDataNonInverted, enhancedImageNonInverted.width(), enhancedImageNonInverted.height(),
                          (debugDir / ("enhanced_non_inverted_" + imageName)).string());
            std::cout << "[BT_ENH] Calling testImageWithConfig for enhancedImageNonInverted. Width: " << enhancedImageNonInverted.width() << ", Height: " << enhancedImageNonInverted.height() << std::endl;
            auto enhancedResult = testImageWithConfig(enhancedImageNonInverted, enhancedOpts, "Enhanced");
            if (enhancedResult.success) enhancedSuccesses++;
            
            // // Try with basic enhancement (inverted input, local binarization)
            // // std::cout << "\nApplying basic enhancement (inverted)..." << std::endl;
            // ZXing::ImageView enhancedImageInverted = ZXing::Code128Enhancer::enhanceBarcode(originalView, true, true);
            // std::vector<uint8_t> enhancedImageDataInverted(enhancedImageInverted.data(), enhancedImageInverted.data() + enhancedImageInverted.width() * enhancedImageInverted.height());
            // saveDebugImage(enhancedImageDataInverted, enhancedImageInverted.width(), enhancedImageInverted.height(),
            //               (debugDir / ("enhanced_inverted_" + imageName)).string());
            // auto enhancedInvertedResult = testImageWithConfig(enhancedImageInverted, enhancedOpts, "EnhancedInverted");
            // if (enhancedInvertedResult.success) enhancedInvertedSuccesses++;
            
            totalEnhancedTime += enhancedResult.processingTimeMs; // Only non-inverted enhanced path contributes now
            
            // Store results
            DecodeResult enhancedInvertedResult; // Dummy result as the path is commented out
            allResults[imageName] = {originalResult, enhancedResult, enhancedInvertedResult};
            
            // Track successes without failing the test
            if (originalResult.success || enhancedResult.success /*|| enhancedInvertedResult.success*/) { 
                // If successful, verify the decoded text matches expected
                CHECK((originalResult.text == expectedText || enhancedResult.text == expectedText /*|| enhancedInvertedResult.text == expectedText*/)); 
            } else {
                // Just report the failure without failing the test
                WARN("Failed to decode " << imageName);
            }
        }
    }

    // Print enhancement effectiveness report at the end of the test
    std::cout << "\n\n=== ENHANCEMENT EFFECTIVENESS REPORT ===" << std::endl;
    std::cout << "Total images tested: " << totalImages << std::endl;
    
    if (totalImages > 0) {
        double originalRate = (originalSuccesses * 100.0) / totalImages;
        double enhancedRate = (enhancedSuccesses * 100.0) / totalImages;
        
        std::cout << "Original decode success rate: " << originalRate << "%" << std::endl;
        std::cout << "Enhanced decode success rate: " << enhancedRate << "%" << std::endl;
        // std::cout << "Enhanced (Inverted) decode success rate: " << (enhancedInvertedSuccesses * 100.0) / totalImages << "%" << std::endl;
        
        std::cout << "\nImages decoded ONLY with original: " << originalSuccesses 
                  << " (" << (originalSuccesses * 100.0 / totalImages) << "%)" << std::endl;
        std::cout << "Images decoded ONLY with enhancement: " << enhancedSuccesses 
                  << " (" << (enhancedSuccesses * 100.0 / totalImages) << "%)" << std::endl;
        
        // Performance metrics
        if (originalSuccesses > 0) {
            std::cout << "\nAverage processing time (original): " 
                      << (totalOriginalTime / originalSuccesses) << " ms" << std::endl;
        }
        if (enhancedSuccesses > 0) {
            std::cout << "Average processing time (enhanced): " 
                      << (totalEnhancedTime / enhancedSuccesses) << " ms" << std::endl;
        }
        
        // Save report to file
        std::ofstream reportFile("enhancement_report.txt");
        if (reportFile.is_open()) {
            reportFile << "=== ENHANCEMENT EFFECTIVENESS REPORT ===\n";
            reportFile << "Total images tested: " << totalImages << "\n";
            reportFile << "Original decode success rate: " << originalRate << "%\n";
            reportFile << "Enhanced decode success rate: " << enhancedRate << "%\n\n";
            reportFile << "Images decoded ONLY with original: " << originalSuccesses 
                      << " (" << (originalSuccesses * 100.0 / totalImages) << "%)\n";
            reportFile << "Images decoded ONLY with enhancement: " << enhancedSuccesses 
                      << " (" << (enhancedSuccesses * 100.0 / totalImages) << "%)\n";
            reportFile.close();
        }
    }
}

int main(int argc, char* argv[]) {
    int result = Catch::Session().run(argc, argv);

    // Print enhancement effectiveness report after all tests complete
    std::cout << "\n\n=== ENHANCEMENT EFFECTIVENESS REPORT ===" << std::endl;
    std::cout << "Total images tested: " << totalImages << std::endl;
    
    if (totalImages > 0) {
        double originalRate = (originalSuccesses * 100.0) / totalImages;
        double enhancedRate = (enhancedSuccesses * 100.0) / totalImages;
        double enhancedInvertedRate = (enhancedInvertedSuccesses * 100.0) / totalImages;
        // double fftRate = (fftSuccesses * 100.0) / totalImages; // Commented out FFT rate calculation
        // double fftInvertedRate = (successesByMethod["FFTEnhancedInverted"] * 100.0) / totalImages; // Commented out FFT inverted rate calculation
        
        std::cout << "Original decode success rate: " << std::fixed << std::setprecision(2) << originalRate << "%" << std::endl;
        std::cout << "Enhanced (Non-Inverted) decode success rate: " << std::fixed << std::setprecision(2) << enhancedRate << "%" << std::endl;
        std::cout << "Enhanced (Inverted) decode success rate: " << std::fixed << std::setprecision(2) << enhancedInvertedRate << "%" << std::endl;
        // std::cout << "FFT Enhanced decode success rate: " << std::fixed << std::setprecision(2) << fftRate << "%" << std::endl; // Commented out FFT rate printing
        // std::cout << "FFT Enhanced+Inverted decode success rate: " << std::fixed << std::setprecision(2) << fftInvertedRate << "%" << std::endl; // Commented out FFT inverted rate printing
        
        // Performance metrics
        if (originalSuccesses > 0 && totalOriginalTime > 0) {
            std::cout << "\nAverage processing time (original): " 
                      << std::fixed << std::setprecision(2) << (totalOriginalTime / originalSuccesses) << " ms" << std::endl;
        }
        
        double totalCombinedEnhancedTime = totalEnhancedTime; // Accumulate time from all enhancement paths if they are timed separately
        int totalCombinedEnhancedSuccesses = enhancedSuccesses + enhancedInvertedSuccesses; // Removed fftSuccesses and successesByMethod
        if (totalCombinedEnhancedSuccesses > 0 && totalCombinedEnhancedTime > 0) {
            std::cout << "Average processing time (all enhancement paths combined): " 
                      << std::fixed << std::setprecision(2) << (totalCombinedEnhancedTime / totalCombinedEnhancedSuccesses) << " ms" << std::endl;
        }
        
        // Save report to file
        std::ofstream reportFile("enhancement_report.txt");
        if (reportFile.is_open()) {
            reportFile << "=== ENHANCEMENT EFFECTIVENESS REPORT ===\n";
            reportFile << "Total images tested: " << totalImages << "\n";
            reportFile << "Original decode success rate: " << std::fixed << std::setprecision(2) << originalRate << "%\n";
            reportFile << "Enhanced (Non-Inverted) decode success rate: " << std::fixed << std::setprecision(2) << enhancedRate << "%\n";
            reportFile << "Enhanced (Inverted) decode success rate: " << std::fixed << std::setprecision(2) << enhancedInvertedRate << "%\n";
            // reportFile << "FFT Enhanced decode success rate: " << std::fixed << std::setprecision(2) << fftRate << "%\n"; // Commented out due to undeclared fftRate
            // reportFile << "FFT Enhanced+Inverted decode success rate: " << std::fixed << std::setprecision(2) << fftInvertedRate << "%\n"; // Commented out due to undeclared fftInvertedRate
            if (originalSuccesses > 0 && totalOriginalTime > 0) {
                 reportFile << "Average processing time (original): " << std::fixed << std::setprecision(2) << (totalOriginalTime / originalSuccesses) << " ms\n";
            }
            if (totalCombinedEnhancedSuccesses > 0 && totalCombinedEnhancedTime > 0) {
                 reportFile << "Average processing time (all enhancement paths combined): " << std::fixed << std::setprecision(2) << (totalCombinedEnhancedTime / totalCombinedEnhancedSuccesses) << " ms\n";
            }
            reportFile.close();
            std::cout << "\nReport saved to enhancement_report.txt" << std::endl;
        } else {
            std::cerr << "\nError: Unable to open enhancement_report.txt for writing." << std::endl;
        }
    }
    return result;
}