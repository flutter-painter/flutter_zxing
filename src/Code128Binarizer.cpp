#include "Code128Binarizer.h"
#include "barcode_enhancer.h" // For Code128Enhancer
#include "zxing/core/src/BitMatrix.h"
#include "zxing/core/src/BitArray.h" // For BitArray
#include "zxing/core/src/Pattern.h" // For PatternView
#include "zxing/core/src/ZXAlgorithms.h" // For utility functions like Reduce, IndexOf, etc.

#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <numeric>
#include <iostream> // Added for std::cout logging

#include "zxing/core/src/oned/ODRowReader.h"         // For DecodeDigit
#include "zxing/core/src/oned/ODCode128Patterns.h" // For CODE_PATTERNS

namespace ZXing {

// Default variance values, similar to those in StockSparkPattern or used by 1D readers
static constexpr float MAX_AVG_VARIANCE = 0.42f;
static constexpr float MAX_INDIVIDUAL_VARIANCE = 0.8f;

// Code 128 pattern tables and constants are now in ODCode128Patterns.h and Code128Binarizer.h

Code128Binarizer::Code128Binarizer(const ImageView& iv) : HybridBinarizer(iv) {
    // Constructor
}

std::shared_ptr<const BitMatrix> Code128Binarizer::getBlackMatrix() const {
    std::cout << "[C128B_GBM] Code128Binarizer::getBlackMatrix() CALLED FOR ENHANCED PATH!" << std::endl;
    auto initialMatrix = HybridBinarizer::getBlackMatrix();
    if (!initialMatrix) {
        return nullptr;
    }
    
    int width = initialMatrix->width();
    int height = initialMatrix->height();
    
    // Create a synthetic grayscale image from the initial binarization for enhancement
    std::vector<uint8_t> tempGrayscaleImage(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            tempGrayscaleImage[y * width + x] = initialMatrix->get(x, y) ? 0 : 255; // 0 for black, 255 for white
        }
    }
    
    // Apply Code 128 specific grayscale enhancements
    Code128Enhancer::applyDirectionalFilter(tempGrayscaleImage, width, height, 5);
    Code128Enhancer::correctBarWidths(tempGrayscaleImage, width, height);
    
    // Re-binarize the enhanced grayscale image
    auto enhancedMatrix = std::make_shared<BitMatrix>(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (tempGrayscaleImage[y * width + x] < 128) { // Threshold
                enhancedMatrix->set(x, y);
            }
        }
    }
    
    // Validate patterns on the enhanced and re-binarized matrix
    if (validatePatterns(*enhancedMatrix)) {
        return enhancedMatrix; // Use enhanced matrix if patterns look valid
    } else {
        return initialMatrix; // Fallback to original HybridBinarizer output
    }
}

bool Code128Binarizer::validatePatterns(const BitMatrix& matrix) {
    std::cout << "[VP] Code128Binarizer::validatePatterns called." << std::endl;
    int width = matrix.width();
    int height = matrix.height();
    std::cout << "[VP] Matrix dimensions: " << width << "x" << height << std::endl;

    std::vector<int> rowsToScan;
    if (height > 0) rowsToScan.push_back(height / 2);
    if (height > 20) rowsToScan.push_back(height / 4);
    if (height > 20) rowsToScan.push_back(height * 3 / 4);
    if (rowsToScan.empty() && height > 0) rowsToScan.push_back(0); // For very short images
    
    std::cout << "[VP] Rows to scan: ";
    for(size_t i = 0; i < rowsToScan.size(); ++i) {
        std::cout << rowsToScan[i] << (i == rowsToScan.size() - 1 ? "" : ", ");
    }
    std::cout << std::endl;

    std::array<int, 6> counters; // Code128 patterns have 6 bars/spaces

    for (int r : rowsToScan) {
        if (r >= height) continue; // Should not happen
        std::cout << "[VP] Scanning row: " << r << std::endl;

        BitArray rowBitArray(width);
        for (int x_col = 0; x_col < width; ++x_col) {
            if (matrix.get(x_col, r)) {
                rowBitArray.set(x_col, true);
            }
        }
        int searchOffset = 0;

        while (searchOffset < width) {
            // Find the beginning of the next black bar
            int barStartOffset = searchOffset;
            while (barStartOffset < width && !rowBitArray.get(barStartOffset)) {
                barStartOffset++;
            }

            if (barStartOffset >= width) {
                break; // No more bars in this row
            }

            // We are at the start of a potential pattern. Create a range from this point.
            ZXing::BitArray::Range bitArrayRange(rowBitArray.begin() + barStartOffset, rowBitArray.end());
            ZXing::PatternRow localPatternRow;
            ZXing::GetPatternRow(bitArrayRange, localPatternRow); // Fills localPatternRow with [bar, space, bar, space, ...]

            if (localPatternRow.empty()) {
                searchOffset = barStartOffset + 1; // Couldn't get a pattern, advance past this black pixel
                continue;
            }

            // Check if localPatternRow has enough elements for 'counters' (std::array<int, 6>)
            if (localPatternRow.size() >= counters.size()) {
                std::copy_n(localPatternRow.begin(), counters.size(), counters.begin());
                
                std::cout << "[VP] Row " << r << ", Offset " << barStartOffset << ": Attempting DecodeDigit. Counters: [";
                for(size_t i = 0; i < counters.size(); ++i) {
                    std::cout << counters[i] << (i == counters.size() - 1 ? "" : ", ");
                }
                std::cout << "]" << std::endl;

                int sum = 0;
                for (int count : counters) {
                    sum += count;
                }
                std::cout << "[VP] Counters sum: " << sum << std::endl;

                if (sum == 0) { // Should not happen if pattern elements are positive and non-empty
                     std::cout << "[VP] Counters sum is 0, advancing searchOffset." << std::endl;
                     searchOffset = barStartOffset + 1; // Advance past the starting black pixel
                     continue;
                }

                // Try to match against known Code128 patterns using DecodeDigit
                int bestMatch = ZXing::OneD::RowReader::DecodeDigit(counters, ZXing::OneD::Code128::CODE_PATTERNS, MAX_AVG_VARIANCE, MAX_INDIVIDUAL_VARIANCE);

                if (bestMatch != -1) {
                    std::cout << "[VP] Row " << r << ", Offset " << barStartOffset << ": DecodeDigit SUCCESS with CODE_PATTERN index " << bestMatch << ". Returning true." << std::endl;
                    return true; // Exit early on first success
                } else {
                    std::cout << "[VP] Row " << r << ", Offset " << barStartOffset << ": No CODE_PATTERN matched by DecodeDigit for these counters." << std::endl;
                }
                // Advance searchOffset past the first bar of the just-checked pattern to look for the next pattern.
                searchOffset = barStartOffset + localPatternRow[0]; 
            } else {
                // localPatternRow too small for 'counters'
                // std::cout << "[VP] Row " << r << ", Offset " << barStartOffset << ": localPatternRow too small (" << localPatternRow.size() << " vs " << counters.size() << ")." << std::endl;
                // Advance past the first bar found.
                if (!localPatternRow.empty()) { // Ensure localPatternRow[0] is safe to access
                    searchOffset = barStartOffset + localPatternRow[0];
                } else {
                    searchOffset = barStartOffset + 1; // Fallback if localPatternRow was unexpectedly empty after all
                }
            }
        }
    }
    std::cout << "[VP] validatePatterns returning false (no patterns found after scanning all selected rows)." << std::endl;
    return false; 
}


// The following static methods are placeholders from the old implementation and are not used by the new validatePatterns.
// They can be removed or adapted if more complex local pattern analysis is desired later.
/*
Pattern Code128Binarizer::extractPattern(const std::array<int, 6>& widths, float moduleSize) {
    Pattern pattern;
    pattern.modules = widths;
    pattern.isValid = true;
    pattern.confidence = 1.0f;
    return pattern;
}

bool Code128Binarizer::isStartStopPattern(const Pattern& pattern) {
    return true;
}

std::array<int, 6> Code128Binarizer::normalizePattern(
    const std::array<int, 6>& widths, float moduleSize) {
    return widths;
}

float Code128Binarizer::calculateConfidence(
    const std::array<int, 6>& normalized,
    const std::array<int, 6>& original,
    float moduleSize) {
    return 1.0f;
}
*/

} // namespace ZXing
