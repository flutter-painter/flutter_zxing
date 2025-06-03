#pragma once

#include "zxing/core/src/BinaryBitmap.h"
#include "zxing/core/src/HybridBinarizer.h"
#include "zxing/core/src/ImageView.h"

namespace ZXing {

/**
 * @brief A specialized binarizer for Code 128 barcodes.
 * 
 * This binarizer extends the HybridBinarizer with specific optimizations
 * for Code 128 barcodes, including enhanced edge detection and
 * bar width correction.
 */
class Code128Binarizer : public HybridBinarizer {
public:
    /**
     * @brief Construct a new Code128Binarizer object
     * 
     * @param iv The input image
     */
    Code128Binarizer(const ImageView& iv);
    
    /**
     * @brief Get a new bitmap with the black/white points initialized
     * 
     * @param luminance The luminance source
     * @return std::shared_ptr<BitMatrix> The resulting bit matrix
     */
    std::shared_ptr<BitMatrix> getBlackMatrix() const override;

private:
    /**
     * @brief Validate and correct bar patterns against Code 128 specifications
     * 
     * @param image The image to process
     * @param width Image width
     * @param height Image height
     * @param expectedRatios Array of expected bar width ratios
     */
    static void validateBarPatterns(std::vector<uint8_t>& image, int width, int height, const float* expectedRatios);
    
    /**
     * @brief Correct a detected bar pattern to match expected ratios
     * 
     * @param image The image to process
     * @param width Image width
     * @param y Current y position
     * @param startIdx Starting index in the barWidths array
     * @param barWidths Vector of detected bar widths
     * @param moduleSize Calculated module size
     * @param expectedRatios Array of expected bar width ratios
     */
    static void correctPattern(std::vector<uint8_t>& image, int width, int y, size_t startIdx, 
                             const std::vector<int>& barWidths, float moduleSize, const float* expectedRatios);
};

} // namespace ZXing
