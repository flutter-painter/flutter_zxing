#pragma once

#include "ReadBarcode.h"
#include <vector>
#include <cmath>
#include "ImageView.h"
#include "MultiFormatReader.h"
#include "Result.h"
#include "BinaryBitmap.h"
#include <cstdint>

// Include CImg for FFT processing
#define cimg_display 0
#define cimg_use_png 0
#define cimg_use_jpeg 0
#define cimg_use_tiff 0
#include "../third_party/CImg/CImg.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ZXing {

class Code128Enhancer {
public:
    // Main enhancement function - currently used in production
    static ImageView enhanceBarcode(const ImageView& image, bool invert = false, bool useLocalBinarization = false);
    
    // Multi-scale enhancement for Code 128 barcodes
    static ImageView enhanceBarcodeMultiScale(const ImageView& input, bool shouldInvert = false);
    
    // New enhancement approaches
    static ImageView enhanceBarcodeWithFFT(const ImageView& input, bool shouldInvert = false);
    static ImageView enhanceWithAdaptiveThresholding(const ImageView& input, bool shouldInvert = false);
    static ImageView enhanceWithDirectionalProcessing(const ImageView& input, bool shouldInvert = false);

    // Image manipulation helpers - currently used in production
    static ImageView rotateImage(const ImageView& input, int degrees);
    static ImageView cropImage(const ImageView& input, int x, int y, int width, int height);
    static ImageView scaleImage(const ImageView& input, float scaleFactor);

    // Image manipulation helpers - moved from private for Code128Binarizer access
    static void applyDirectionalFilter(std::vector<uint8_t>& image, int width, int height, int kernelSize = 5);
    static void correctBarWidths(std::vector<uint8_t>& image, int width, int height);

private:
    // Core enhancement steps - currently used in production
    static std::vector<uint8_t> convertToGrayscale(const ImageView& input);
    static void adaptiveThreshold(std::vector<uint8_t>& image, int width, int height, int blockSize, float C);
    static ImageView toImageView(const std::vector<uint8_t>& image, int width, int height);
    
    // New enhancement helper functions
    static std::vector<uint8_t> enhanceWithFrequencyDomain(const std::vector<uint8_t>& image, int width, int height);
    static std::vector<uint8_t> applyCode128AdaptiveThreshold(const std::vector<uint8_t>& image, int width, int height);
    static std::vector<uint8_t> applyCode128DirectionalFilter(const std::vector<uint8_t>& image, int width, int height);

    // === Experimental/Unused Enhancement Functions ===
    // These functions were developed during testing but were found to be unnecessary
    // or potentially harmful to the decoding process. They are kept for reference
    // and potential future experimentation.

    // Gaussian blur - Not used: Added complexity without improving decode rate
    static void gaussianBlur(std::vector<uint8_t>& image, int width, int height, float sigma);

    // Quiet zone validation - Not used: ZXing handles quiet zones well enough internally
    static bool validateQuietZones(const std::vector<uint8_t>& image, int width, int height, int quietZoneWidth);

    // Contrast enhancement - Not used: Could make some images worse
    static void enhanceContrast(std::vector<uint8_t>& image);

    // Image normalization - Not used: Adaptive thresholding proved more effective
    static void normalizeImage(std::vector<uint8_t>& image);
};

} // namespace ZXing 