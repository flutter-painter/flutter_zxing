#pragma once

#include "zxing/core/src/BinaryBitmap.h"
#include "zxing/core/src/HybridBinarizer.h"
#include "zxing/core/src/ImageView.h"
#include "zxing/core/src/oned/ODRowReader.h" // For RowReader::DecodeDigit, PatternMatchVariance
#include "zxing/core/src/oned/ODCode128Patterns.h" // Added for Code128::CODE_PATTERNS
#include <array>
#include <vector>
#include <tuple>

namespace ZXing {

class BitMatrix; // Forward declaration

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
     * @return std::shared_ptr<BitMatrix> The resulting bit matrix
     */
    std::shared_ptr<const BitMatrix> getBlackMatrix() const override;

private:
    // Code 128 constants
    static constexpr int QUIET_ZONE_SIZE = 10; // Modules, might be used by RecordPattern or logic around it
    static constexpr int CHAR_SIZE = 11;  // modules per character

    // Constants for pattern matching (from ODCode128Reader.cpp)
    static constexpr float MAX_AVG_VARIANCE = 0.25f;
    static constexpr float MAX_INDIVIDUAL_VARIANCE = 0.7f;

    // Pattern validation structures (original, can be removed or adapted if not used by new validatePatterns)
    // struct Pattern {
    //     std::array<int, 6> modules;  // bar/space widths in modules
    //     bool isValid;
    //     float confidence;
    // };

    /**
     * @brief Validate if the matrix likely contains Code 128 patterns.
     * 
     * @param matrix The binarized image matrix to check.
     * @return bool True if Code 128-like patterns are found, false otherwise.
     */
    static bool validatePatterns(const BitMatrix& matrix);

    // The following static methods were part of the old placeholder validatePatterns.
    // They might be useful if we build more complex pattern analysis, but for now,
    // the core logic will rely on ODReader::DecodeDigit.
    // static Pattern extractPattern(const std::array<int, 6>& widths, float moduleSize);
    // static bool isStartStopPattern(const Pattern& pattern);
    // static std::array<int, 6> normalizePattern(const std::array<int, 6>& widths, float moduleSize);
    // static float calculateConfidence(const std::array<int, 6>& normalized, 
    //                                const std::array<int, 6>& original,
    //                                float moduleSize);
};

} // namespace ZXing
