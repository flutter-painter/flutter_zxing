#pragma once

#include "ReadBarcode.h"
#include <vector>
#include <cmath>
#include "zxing/core/src/ImageView.h"
#include "zxing/core/src/MultiFormatReader.h"
#include "zxing/core/src/Barcode.h"
#include "zxing/core/src/BinaryBitmap.h"
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ZXing {

class Code128Enhancer {
public:
    // Main enhancement function - currently used in production
    static ImageView enhanceBarcode(const ImageView& input, bool shouldInvert = false);

    // Image manipulation helpers - currently used in production
    static ImageView rotateImage(const ImageView& input, int degrees);
    static ImageView cropImage(const ImageView& input, int x, int y, int width, int height);

private:
    // Core enhancement steps - currently used in production
    static std::vector<uint8_t> convertToGrayscale(const ImageView& input);
    static void adaptiveThreshold(std::vector<uint8_t>& image, int width, int height, int blockSize, float C);
    static ImageView toImageView(const std::vector<uint8_t>& image, int width, int height);

    // === Experimental/Unused Enhancement Functions ===
    // These functions were developed during testing but were found to be unnecessary
    // or potentially harmful to the decoding process. They are kept for reference
    // and potential future experimentation.

    // Gaussian blur - Not used: Added complexity without improving decode rate
    static void gaussianBlur(std::vector<uint8_t>& image, int width, int height, float sigma);

    // Bar width correction - Not used: Sometimes interfered with ZXing's built-in bar detection
    static void correctBarWidths(std::vector<uint8_t>& image, int width, int height);

    // Quiet zone validation - Not used: ZXing handles quiet zones well enough internally
    static bool validateQuietZones(const std::vector<uint8_t>& image, int width, int height, int quietZoneWidth);

    // Contrast enhancement - Not used: Could make some images worse
    static void enhanceContrast(std::vector<uint8_t>& image);

    // Image normalization - Not used: Adaptive thresholding proved more effective
    static void normalizeImage(std::vector<uint8_t>& image);
};

} // namespace ZXing 