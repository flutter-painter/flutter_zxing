#pragma once

#include "zxing/core/src/BinaryBitmap.h"
#include "zxing/core/src/HybridBinarizer.h"
#include "zxing/core/src/ImageView.h"
#include <array>
#include <vector>
#include <tuple>

namespace ZXing {

/**
 * @brief A specialized binarizer for Code 128 barcodes
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
    // Code 128 constants
    static constexpr int QUIET_ZONE_SIZE = 10;
    static constexpr int CHAR_SIZE = 11;  // modules per character
    static constexpr float MAX_VARIANCE = 0.25f;

    // Pattern validation structures
    struct Pattern {
        std::array<int, 6> modules;  // bar/space widths in modules
        bool isValid;
        float confidence;
    };

    /**
     * @brief Validate and correct patterns against Code 128 specifications
     * 
     * @param image The image to process
     * @param width Image width
     * @param height Image height
     * @return std::vector<uint8_t> Processed image with corrected patterns
     */
    static std::vector<uint8_t> validatePatterns(const std::vector<uint8_t>& image, int width, int height);

    /**
     * @brief Extract pattern from a sequence of bar/space widths
     * 
     * @param widths Array of bar and space widths
     * @param moduleSize Estimated module size
     * @return Pattern Extracted pattern with validation info
     */
    static Pattern extractPattern(const std::array<int, 6>& widths, float moduleSize);

    /**
     * @brief Check if a pattern matches Code 128 start/stop patterns
     * 
     * @param pattern Pattern to check
     * @return bool True if pattern is a valid start/stop pattern
     */
    static bool isStartStopPattern(const Pattern& pattern);

    /**
     * @brief Normalize pattern widths to module counts
     * 
     * @param widths Raw width measurements
     * @param moduleSize Estimated module size
     * @return std::array<int, 6> Normalized module counts
     */
    static std::array<int, 6> normalizePattern(const std::array<int, 6>& widths, float moduleSize);

    /**
     * @brief Calculate confidence score for a pattern
     * 
     * @param normalized Normalized module counts
     * @param original Original width measurements
     * @param moduleSize Estimated module size
     * @return float Confidence score (0.0 to 1.0)
     */
    static float calculateConfidence(const std::array<int, 6>& normalized, 
                                   const std::array<int, 6>& original,
                                   float moduleSize);
};

} // namespace ZXing
