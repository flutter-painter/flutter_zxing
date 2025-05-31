#pragma once

#include "ReadBarcode.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ZXing {

class BarcodeEnhancer {
public:
    // Enhance image specifically for 1D barcode detection
    // Returns enhanced image in ZXing ImageView format
    static ImageView enhance1DBarcode(const ImageView& input);

private:
    // Core enhancement steps
    static std::vector<uint8_t> convertToGrayscale(const ImageView& input);
    static void gaussianBlur(std::vector<uint8_t>& image, int width, int height, float sigma = 1.0f);
    static void adaptiveThreshold(std::vector<uint8_t>& image, int width, int height, int windowSize = 15, float C = 5.0f);
    static void erode(std::vector<uint8_t>& image, int width, int height, int kernelSize = 3);
    static void dilate(std::vector<uint8_t>& image, int width, int height, int kernelSize = 3);
    
    // New contrast enhancement functions
    static void enhanceContrast(std::vector<uint8_t>& image);
    static void normalizeImage(std::vector<uint8_t>& image);
    static void stretchHistogram(std::vector<uint8_t>& image, float lowPercentile = 1.0f, float highPercentile = 99.0f);
    
    // Helper functions
    static ImageView toImageView(const std::vector<uint8_t>& image, int width, int height);
    static std::vector<uint8_t> createKernel(int size);
    
    // Constants for enhancement
    static constexpr int MIN_BAR_WIDTH = 2;
    static constexpr int MAX_BAR_WIDTH = 20;
    static constexpr float MIN_CONTRAST = 30.0f;
};

} // namespace ZXing 