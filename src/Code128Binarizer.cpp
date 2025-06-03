#include "Code128Binarizer.h"
#include "barcode_enhancer.h"
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ZXing {

Code128Binarizer::Code128Binarizer(const ImageView& iv) : HybridBinarizer(iv) {
    // Constructor just passes the image to the parent class
}

std::shared_ptr<BitMatrix> Code128Binarizer::getBlackMatrix() const {
    // First get the standard bitmap from the parent class
    auto matrix = HybridBinarizer::getBlackMatrix();
    if (!matrix) {
        return nullptr;
    }
    
    // Get dimensions
    int width = matrix->width();
    int height = matrix->height();
    
    // Create a copy of the matrix data as a grayscale image for processing
    std::vector<uint8_t> image(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Convert binary values to grayscale (0 or 255)
            image[y * width + x] = matrix->get(x, y) ? 0 : 255;
        }
    }
    
    // Apply Code 128 specific optimizations
    
    // 1. Apply directional filtering to enhance horizontal bars
    Code128Enhancer::applyDirectionalFilter(image, width, height, 5);
    
    // 2. Apply bar width correction specific to Code 128
    Code128Enhancer::correctBarWidths(image, width, height);
    
    // 3. Apply adaptive thresholding with multiple block sizes
    const int minBlockSize = 15;
    const int maxBlockSize = 51;
    std::vector<uint8_t> adaptiveResult(width * height, 255);
    
    for (int blockSize = minBlockSize; blockSize <= maxBlockSize; blockSize += 12) {
        std::vector<uint8_t> tempResult = image;
        Code128Enhancer::adaptiveThreshold(tempResult, width, height, blockSize, 0.15f);
        
        // Combine results using minimum value (darker pixel wins)
        for (int i = 0; i < width * height; i++) {
            adaptiveResult[i] = std::min(adaptiveResult[i], tempResult[i]);
        }
    }
    
    // 4. Validate and correct bar patterns
    const float expectedRatios[] = {2.0f, 1.0f, 2.0f, 2.0f}; // Code 128 bar width ratios
    validateBarPatterns(adaptiveResult, width, height, expectedRatios);
    
    // Create a new bit matrix from the processed image
    auto result = std::make_shared<BitMatrix>(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (adaptiveResult[y * width + x] < 128) {
                result->set(x, y);
            }
        }
    }
    
    return result;
}

void Code128Binarizer::validateBarPatterns(std::vector<uint8_t>& image, int width, int height, const float* expectedRatios) {
    const int patternLength = 4; // Code 128 uses patterns of 4 bars
    
    for (int y = 0; y < height; y++) {
        std::vector<int> barWidths;
        int currentWidth = 0;
        bool currentBar = false;
        
        for (int x = 0; x < width; x++) {
            bool pixel = image[y * width + x] < 128;
            if (pixel != currentBar) {
                if (currentWidth > 0) {
                    barWidths.push_back(currentWidth);
                }
                currentWidth = 1;
                currentBar = pixel;
            } else {
                currentWidth++;
            }
        }
        
        // Analyze patterns
        if (barWidths.size() >= patternLength) {
            for (size_t i = 0; i <= barWidths.size() - patternLength; i++) {
                float sum = 0;
                for (int j = 0; j < patternLength; j++) {
                    sum += barWidths[i + j];
                }
                
                // Normalize and compare to expected ratios
                float moduleSize = sum / 6.0f; // Code 128 uses 11 modules per character
                bool needsCorrection = false;
                
                for (int j = 0; j < patternLength; j++) {
                    float ratio = barWidths[i + j] / moduleSize;
                    float expectedRatio = expectedRatios[j];
                    
                    if (std::abs(ratio - expectedRatio) > 0.5f) {
                        needsCorrection = true;
                        break;
                    }
                }
                
                // Correct pattern if needed
                if (needsCorrection) {
                    correctPattern(image, width, y, i, barWidths, moduleSize, expectedRatios);
                }
            }
        }
    }
}

void Code128Binarizer::correctPattern(std::vector<uint8_t>& image, int width, int y, size_t startIdx, 
                                    const std::vector<int>& barWidths, float moduleSize, const float* expectedRatios) {
    int x = 0;
    for (size_t i = 0; i < startIdx; i++) {
        x += barWidths[i];
    }
    
    // Adjust bar widths to match expected ratios
    for (int i = 0; i < 4; i++) {
        int expectedWidth = static_cast<int>(moduleSize * expectedRatios[i] + 0.5f);
        int currentWidth = barWidths[startIdx + i];
        
        // Adjust pixels to match expected width
        bool isBar = (i % 2) == 0;
        for (int j = 0; j < expectedWidth; j++) {
            if (x + j < width) {
                image[y * width + x + j] = isBar ? 0 : 255;
            }
        }
        x += currentWidth;
    }
}

} // namespace ZXing
