#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
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
    
    INFO("Image loaded successfully:");
    INFO("  Width: " << width);
    INFO("  Height: " << height);
    INFO("  Original channels: " << channels);
    
    REQUIRE(width > 0);
    REQUIRE(height > 0);
    REQUIRE(data != nullptr);
    
    std::vector<uint8_t> gray_data(data, data + width * height);
    INFO("  Gray data size: " << gray_data.size());
    
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

void printBarcodeInfo(const ZXing::Result& result, const std::string& label) {
    std::cout << "=== " << label << " ===" << std::endl;
    std::cout << "Valid: " << (result.isValid() ? "Yes" : "No") << std::endl;
    if (result.isValid()) {
        std::cout << "Format: " << ZXing::ToString(result.format()) << std::endl;
        std::cout << "Text: " << result.text() << std::endl;
        std::cout << "Error: " << result.error().msg() << std::endl;
        std::cout << "Line Count: " << result.lineCount() << std::endl;
        std::cout << "ECC Level: " << result.ecLevel() << std::endl;
        
        // Print position information
        auto pos = result.position();
        std::cout << "Position: " << std::endl;
        std::cout << "  TopLeft: (" << pos.topLeft().x << ", " << pos.topLeft().y << ")" << std::endl;
        std::cout << "  TopRight: (" << pos.topRight().x << ", " << pos.topRight().y << ")" << std::endl;
        std::cout << "  BottomLeft: (" << pos.bottomLeft().x << ", " << pos.bottomLeft().y << ")" << std::endl;
        std::cout << "  BottomRight: (" << pos.bottomRight().x << ", " << pos.bottomRight().y << ")" << std::endl;
    } else {
        std::cout << "Error: " << result.error().msg() << std::endl;
    }
    std::cout << std::endl;
}

TEST_CASE("Basic barcode decoding", "[barcode]") {
    // Get the absolute path to the test images
    fs::path currentPath = fs::current_path();
    fs::path rootPath = currentPath;
    
    // Keep going up until we find the barcode_images directory or hit the root
    while (!fs::exists(rootPath / "barcode_images") && rootPath.has_parent_path()) {
        INFO("Checking path: " << rootPath.string());
        rootPath = rootPath.parent_path();
    }
    
    REQUIRE(fs::exists(rootPath / "barcode_images"));
    fs::path imagesPath = rootPath / "barcode_images";
    
    INFO("Test images path: " << imagesPath.string());
    INFO("Current working directory: " << fs::current_path().string());
    INFO("Root path: " << rootPath.string());

    // List all files in the barcode_images directory
    INFO("Files in barcode_images directory:");
    for (const auto& entry : fs::directory_iterator(imagesPath)) {
        INFO("  " << entry.path().string());
    }

    std::vector<std::string> testImages = {
        "800446E_01XL.jpg",
        "800165E_01L.jpg",
        "2E3918D7L736.jpg",
        "257670HA64SM.jpg",
        "1H1406D40232.jpg"
    };

    SECTION("Testing each image") {
        for (const auto& imageName : testImages) {
            INFO("\n=== Testing image: " << imageName << " ===");
            fs::path imagePath = imagesPath / imageName;
            INFO("Full image path: " << imagePath.string());
            
            REQUIRE(fs::exists(imagePath));
            
            try {
                // Load and decode image
                auto [gray_data, width, height] = loadImage(imagePath);
                REQUIRE(gray_data.size() == width * height);
                
                INFO("Creating ImageView with:");
                INFO("  Width: " << width);
                INFO("  Height: " << height);
                INFO("  Stride: " << width);
                INFO("  Data size: " << gray_data.size());
                
                // Ensure data is not empty and dimensions are valid
                REQUIRE(!gray_data.empty());
                REQUIRE(width > 0);
                REQUIRE(height > 0);
                REQUIRE(gray_data.size() == static_cast<size_t>(width * height));
                
                // Additional checks for ImageView parameters
                INFO("Validating ImageView parameters:");
                INFO("  Data pointer valid: " << (gray_data.data() != nullptr));
                INFO("  Width * Height = " << (width * height));
                INFO("  Data size = " << gray_data.size());
                INFO("  Width * Height <= Data size: " << ((width * height) <= gray_data.size()));
                
                // Create ImageView for ZXing with buffer size
                int stride = width; // For grayscale images, stride equals width
                ZXing::ImageView imageView(gray_data.data(), gray_data.size(), width, height, ZXing::ImageFormat::Lum, stride);
                
                // Configure reader options
                ZXing::ReaderOptions options;
                options.setFormats(ZXing::BarcodeFormat::Any); // Try all formats
                options.setTryHarder(true);
                options.setTryRotate(true);
                options.setIsPure(false); // The image might not be a "pure" barcode
                
                // Try to read barcode
                auto result = ZXing::ReadBarcode(imageView, options);
                
                REQUIRE(result.isValid());
                
                // Extract expected barcode from filename (remove extension)
                std::string expectedBarcode = fs::path(imageName).stem().string();
                
                // Compare result with expected barcode
                REQUIRE(result.text() == expectedBarcode);
            } catch (const std::exception& e) {
                FAIL("Exception: " << e.what());
            }
        }
    }
}

TEST_CASE("Barcode Enhancement", "[enhancement]") {
    fs::path currentPath = fs::current_path();
    fs::path rootPath = currentPath;
    
    while (!fs::exists(rootPath / "barcode_images") && rootPath.has_parent_path()) {
        rootPath = rootPath.parent_path();
    }
    
    INFO("Test images path: " << rootPath / "barcode_images");
    INFO("Current working directory: " << currentPath);
    INFO("Root path: " << rootPath);
    
    REQUIRE(fs::exists(rootPath / "barcode_images"));
    
    // Focus on the specific image
    fs::path imagePath = rootPath / "barcode_images" / "2E3918D7L736.jpg";
    REQUIRE(fs::exists(imagePath));
    
    SECTION("Testing enhancement pipeline") {
        INFO("=== Testing enhancement on image: " << imagePath.filename().string() << " ===");
        
        // Load image
        int width, height, channels;
        std::unique_ptr<uint8_t, void(*)(void*)> data(
            stbi_load(imagePath.string().c_str(), &width, &height, &channels, 1),
            stbi_image_free
        );
        REQUIRE(data != nullptr);
        
        // Create ImageView
        ZXing::ImageView originalView(data.get(), width, height, ZXing::ImageFormat::Lum);
        
        // Print original image stats
        std::vector<uint8_t> originalData(data.get(), data.get() + width * height);
        printImageStats(originalData, "Original Image");
        saveDebugImage(originalData, width, height, "original.png");
        
        // Apply enhancement
        INFO("Applying image enhancement");
        auto enhancedView = ZXing::BarcodeEnhancer::enhance1DBarcode(originalView);
        
        // Convert enhanced view to vector for stats
        std::vector<uint8_t> enhancedData(enhancedView.data(0, 0), enhancedView.data(0, 0) + width * height);
        printImageStats(enhancedData, "Enhanced Image");
        saveDebugImage(enhancedData, width, height, "enhanced.png");
        
        // Configure reader options with maximum debug info
        ZXing::ReaderOptions options;
        options.setTryHarder(true);
        options.setTryRotate(true);
        options.setIsPure(false);
        
        // Try all binarizers with original image
        options.setBinarizer(ZXing::Binarizer::GlobalHistogram);
        auto originalResult = ZXing::ReadBarcode(originalView, options);
        printBarcodeInfo(originalResult, "Original Image Result (Global Histogram)");
        
        options.setBinarizer(ZXing::Binarizer::LocalAverage);
        auto originalResultHybrid = ZXing::ReadBarcode(originalView, options);
        printBarcodeInfo(originalResultHybrid, "Original Image Result (Local Average)");
        
        // Try all binarizers with enhanced image
        options.setBinarizer(ZXing::Binarizer::GlobalHistogram);
        auto enhancedResult = ZXing::ReadBarcode(enhancedView, options);
        printBarcodeInfo(enhancedResult, "Enhanced Image Result (Global Histogram)");
        
        options.setBinarizer(ZXing::Binarizer::LocalAverage);
        auto enhancedResultHybrid = ZXing::ReadBarcode(enhancedView, options);
        printBarcodeInfo(enhancedResultHybrid, "Enhanced Image Result (Local Average)");
        
        // Check if any of the attempts succeeded
        bool success = originalResult.isValid() || originalResultHybrid.isValid() || 
                      enhancedResult.isValid() || enhancedResultHybrid.isValid();
        
        if (!success) {
            INFO("No valid barcode detected. Expected: 2E3918D7L736");
            FAIL("Barcode detection failed");
        }
        
        // If we have a valid result, verify it matches expected
        std::string expectedBarcode = "2E3918D7L736";
        if (originalResult.isValid())
            REQUIRE(originalResult.text() == expectedBarcode);
        if (originalResultHybrid.isValid())
            REQUIRE(originalResultHybrid.text() == expectedBarcode);
        if (enhancedResult.isValid())
            REQUIRE(enhancedResult.text() == expectedBarcode);
        if (enhancedResultHybrid.isValid())
            REQUIRE(enhancedResultHybrid.text() == expectedBarcode);
    }
}

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
} 