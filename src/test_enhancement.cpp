#include "barcode_enhancer.h"
#include "ReadBarcode.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace ZXing;
namespace fs = std::filesystem;

// Helper to load image using CImg
CImg<uint8_t> loadImage(const std::string& path) {
    return CImg<uint8_t>(path.c_str());
}

// Helper to save debug images
void saveDebugImage(const CImg<uint8_t>& image, const std::string& prefix, const std::string& suffix) {
    std::string outPath = "debug_output/" + prefix + "_" + suffix + ".png";
    image.save(outPath.c_str());
}

// Test enhancement pipeline with debug output
void testEnhancement(const std::string& imagePath) {
    std::cout << "\nTesting: " << imagePath << std::endl;
    
    // Extract expected barcode value from filename
    std::string filename = fs::path(imagePath).stem().string();
    std::cout << "Expected barcode: " << filename << std::endl;
    
    try {
        // Load and convert image
        CImg<uint8_t> originalImage = loadImage(imagePath);
        std::string prefix = fs::path(imagePath).stem().string();
        
        // Save original
        saveDebugImage(originalImage, prefix, "original");
        
        // Create ImageView from original
        ImageView originalView(
            originalImage.data(),
            originalImage.width(),
            originalImage.height(),
            ImageFormat::Lum
        );
        
        // Try reading original
        std::cout << "Attempting detection on original image..." << std::endl;
        auto resultOriginal = ReadBarcode(originalView, ReaderOptions()
            .setFormats(BarcodeFormat::Code128)
            .setTryHarder(true)
            .setTryRotate(true)
            .setIsPure(false));
            
        if (resultOriginal.isValid()) {
            std::cout << "Original detection succeeded: " << resultOriginal.text() << std::endl;
        } else {
            std::cout << "Original detection failed: " << resultOriginal.error().msg() << std::endl;
        }
        
        // Apply our enhancement pipeline with debug output
        std::cout << "Applying enhancement pipeline..." << std::endl;
        
        // Convert to grayscale if needed
        CImg<uint8_t> image = originalImage;
        if (image.spectrum() > 1) {
            image.channel(0);
        }
        saveDebugImage(image, prefix, "gray");
        
        // Apply each enhancement step and save intermediate results
        BarcodeEnhancer::anisotropicSmoothing(image);
        saveDebugImage(image, prefix, "smoothed");
        
        BarcodeEnhancer::verticalEdgeEnhancement(image);
        saveDebugImage(image, prefix, "edges");
        
        int baseWidth = BarcodeEnhancer::estimateBarWidth(image);
        BarcodeEnhancer::normalizeBarWidths(image);
        saveDebugImage(image, prefix, "normalized");
        
        BarcodeEnhancer::enforceBarWidthRatios(image, baseWidth);
        saveDebugImage(image, prefix, "ratios");
        
        BarcodeEnhancer::normalizeQuietZones(image, baseWidth);
        saveDebugImage(image, prefix, "quietzones");
        
        BarcodeEnhancer::morphologicalClean(image);
        BarcodeEnhancer::adaptiveThreshold(image);
        saveDebugImage(image, prefix, "final");
        
        // Try reading enhanced image
        ImageView enhancedView(
            image.data(),
            image.width(),
            image.height(),
            ImageFormat::Lum
        );
        
        std::cout << "Attempting detection on enhanced image..." << std::endl;
        auto resultEnhanced = ReadBarcode(enhancedView, ReaderOptions()
            .setFormats(BarcodeFormat::Code128)
            .setTryHarder(true)
            .setTryRotate(true)
            .setIsPure(false));
            
        if (resultEnhanced.isValid()) {
            std::cout << "Enhanced detection succeeded: " << resultEnhanced.text() << std::endl;
        } else {
            std::cout << "Enhanced detection failed: " << resultEnhanced.error().msg() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error processing image: " << e.what() << std::endl;
    }
}

int main() {
    // Create output directory
    fs::create_directory("debug_output");
    
    // Test all images in barcode_images directory
    std::string testDir = "barcode_images";
    for (const auto& entry : fs::directory_iterator(testDir)) {
        if (entry.path().extension() == ".jpg" || entry.path().extension() == ".png") {
            testEnhancement(entry.path().string());
        }
    }
    
    return 0;
} 