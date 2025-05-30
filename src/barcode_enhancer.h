#pragma once

#include <CImg.h>
#include "ReadBarcode.h"

using namespace cimg_library;

namespace ZXing {

class BarcodeEnhancer {
public:
    // Enhance image specifically for 1D barcode detection
    // Returns enhanced image in ZXing ImageView format
    static ImageView enhance1DBarcode(const ImageView& input);

    // Made public for testing
    static void anisotropicSmoothing(CImg<uint8_t>& image);
    static void verticalEdgeEnhancement(CImg<uint8_t>& image);
    static void normalizeBarWidths(CImg<uint8_t>& image);
    static void enforceBarWidthRatios(CImg<uint8_t>& image, int baseWidth);
    static void normalizeQuietZones(CImg<uint8_t>& image, int barWidth);
    static void morphologicalClean(CImg<uint8_t>& image);
    static void adaptiveThreshold(CImg<uint8_t>& image);
    static int estimateBarWidth(const CImg<uint8_t>& image);

private:
    // Core enhancement steps
    static CImg<uint8_t> convertToGrayscale(const ImageView& input);
    static void directionalEnhance(CImg<uint8_t>& image);
    static void enhanceCode128(CImg<uint8_t>& image);
    
    // Analysis helpers
    static float detectOrientation(const CImg<uint8_t>& image);
    static void rotateToHorizontal(CImg<uint8_t>& image, float angle);
    static std::vector<int> analyzeBarWidths(const CImg<uint8_t>& image);
    
    // Advanced image processing using CImg features
    static void sharpenVerticalEdges(CImg<uint8_t>& image);
    static void enhanceLocalContrast(CImg<uint8_t>& image, int blockSize = 16);
    
    // Utility functions
    static void applyDirectionalSobel(CImg<uint8_t>& image, bool vertical);
    static ImageView toImageView(const CImg<uint8_t>& image);
    
    // Constants for enhancement
    static constexpr int MIN_BAR_WIDTH = 2;  // Minimum expected bar width in pixels
    static constexpr int MAX_BAR_WIDTH = 20; // Maximum expected bar width in pixels
    static constexpr float MIN_CONTRAST = 30.0f; // Minimum contrast between bars
    static constexpr int DIRECTION_BLOCK_SIZE = 32; // Block size for direction analysis
    
    // Constants for Code 128 enhancement
    static constexpr int QUIET_ZONE_MULT = 10;    // Quiet zone should be 10x narrow bar width
    static constexpr float BAR_WIDTH_TOLERANCE = 0.2f; // 20% tolerance for width ratios
    static constexpr int VERTICAL_SMOOTH_RADIUS = 5;   // Vertical smoothing radius
    static constexpr float EDGE_SHARPEN_AMOUNT = 1.5f; // Edge sharpening factor
};
} 