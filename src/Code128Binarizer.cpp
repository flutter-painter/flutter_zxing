#include "Code128Binarizer.h"
#include "barcode_enhancer.h"
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <numeric>

namespace ZXing {

// Code 128 pattern tables
namespace {
    // Start patterns (bars and spaces) for Code128
    const std::array<std::array<int, 6>, 6> START_PATTERNS = {{
        {2,1,1,2,3,2}, // Start Code A
        {2,1,1,2,3,2}, // Start Code B
        {2,1,1,3,2,2}, // Start Code C
        {2,3,3,1,1,1}, // FNC1
        {2,1,3,2,1,2}, // FNC2
        {2,1,3,1,2,2}  // FNC3
    }};

    // Stop pattern
    const std::array<int, 7> STOP_PATTERN = {2,3,3,1,1,1,2};

    // Helper function to calculate total modules in a pattern
    int calculateTotalModules(const std::array<int, 6>& pattern) {
        return std::accumulate(pattern.begin(), pattern.end(), 0);
    }
}

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
    
    // Analyze image and get adaptive parameters
    auto params = analyzeImageCharacteristics(image, width, height);
    
    // Find promising regions for Code 128 barcodes using adaptive parameters
    auto regions = findPromsingRegions(image, width, height, params);
    
    // Process each region and combine results
    auto result = std::make_shared<BitMatrix>(width, height);
    for (const auto& region : regions) {
        if (region.score < params.patternConfidence) {
            continue;  // Skip regions unlikely to contain barcodes
        }
        
        // Process the region with adaptive parameters
        auto processedRegion = processRegion(image, width, height, region, params);
        
        // Copy processed region back to result matrix
        for (int y = region.y; y < region.y + region.height && y < height; y++) {
            for (int x = region.x; x < region.x + region.width && x < width; x++) {
                if (processedRegion[(y - region.y) * region.width + (x - region.x)] < 128) {
                    result->set(x, y);
                }
            }
        }
    }
    
    return result;
}

Code128Binarizer::AdaptiveParams Code128Binarizer::analyzeImageCharacteristics(
    const std::vector<uint8_t>& image, int width, int height) const {
    auto [contrast, noise, sharpness] = calculateImageMetrics(image, width, height);
    
    AdaptiveParams params = AdaptiveParams::createDefault();
    
    // Adjust edge threshold based on contrast and noise
    params.edgeThreshold = std::max(30.0f, std::min(80.0f, 50.0f * contrast / (1.0f + noise)));
    
    // Adjust module variance based on sharpness
    params.moduleVariance = std::max(0.15f, std::min(0.35f, MAX_VARIANCE * (1.0f + (1.0f - sharpness))));
    
    // Adjust pattern confidence threshold based on image quality
    float quality = (contrast + sharpness) / (2.0f * (1.0f + noise));
    params.patternConfidence = std::max(0.3f, std::min(0.7f, 0.5f * quality));
    
    // Adjust minimum region dimensions based on image size and quality
    float scaleFactor = std::min(1.0f, std::max(0.5f, quality));
    params.minRegionWidth = static_cast<int>(MIN_ROI_WIDTH * scaleFactor);
    params.minRegionHeight = static_cast<int>(MIN_ROI_HEIGHT * scaleFactor);
    
    return params;
}

std::tuple<float, float, float> Code128Binarizer::calculateImageMetrics(
    const std::vector<uint8_t>& image, int width, int height) const {
    // Calculate contrast
    uint8_t minVal = 255, maxVal = 0;
    float sum = 0.0f, sumSq = 0.0f;
    
    for (const auto& pixel : image) {
        minVal = std::min(minVal, pixel);
        maxVal = std::max(maxVal, pixel);
        sum += pixel;
        sumSq += pixel * pixel;
    }
    
    float mean = sum / image.size();
    float variance = (sumSq / image.size()) - (mean * mean);
    float contrast = (maxVal - minVal) / 255.0f;
    
    // Calculate noise level using local variance
    float noiseSum = 0.0f;
    int noiseCount = 0;
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            float localVariance = 0.0f;
            float localMean = 0.0f;
            
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    float val = image[(y + dy) * width + (x + dx)];
                    localMean += val;
                    localVariance += val * val;
                }
            }
            
            localMean /= 9.0f;
            localVariance = (localVariance / 9.0f) - (localMean * localMean);
            noiseSum += localVariance;
            noiseCount++;
        }
    }
    
    float noise = noiseCount > 0 ? std::min(1.0f, noiseSum / (noiseCount * 255.0f)) : 0.0f;
    
    // Calculate sharpness using gradient magnitude
    float sharpnessSum = 0.0f;
    int sharpnessCount = 0;
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            float gx = static_cast<float>(image[y * width + (x + 1)] - image[y * width + (x - 1)]) / 2.0f;
            float gy = static_cast<float>(image[(y + 1) * width + x] - image[(y - 1) * width + x]) / 2.0f;
            float gradientMagnitude = std::sqrt(gx * gx + gy * gy);
            sharpnessSum += gradientMagnitude;
            sharpnessCount++;
        }
    }
    
    float sharpness = sharpnessCount > 0 ? std::min(1.0f, sharpnessSum / (sharpnessCount * 255.0f)) : 0.0f;
    
    return {contrast, noise, sharpness};
}

std::vector<Code128Binarizer::Region> Code128Binarizer::findPromsingRegions(
    const std::vector<uint8_t>& image, int width, int height, const AdaptiveParams& params) const {
    std::vector<Region> regions;
    
    // Sliding window parameters - adapt based on image quality
    int windowWidth = std::min(width, 200);
    int windowHeight = std::min(height, 100);
    int stepX = windowWidth - REGION_OVERLAP;
    int stepY = windowHeight - REGION_OVERLAP;
    
    // Scan the image with overlapping windows
    for (int y = 0; y + windowHeight <= height; y += stepY) {
        for (int x = 0; x + windowWidth <= width; x += stepX) {
            Region region{x, y, windowWidth, windowHeight, 0.0f};
            region.score = calculateRegionScore(image, width, height, region);
            
            if (region.score >= params.patternConfidence && 
                region.width >= params.minRegionWidth && 
                region.height >= params.minRegionHeight) {
                regions.push_back(region);
            }
        }
    }
    
    // Sort regions by score in descending order
    std::sort(regions.begin(), regions.end(),
              [](const Region& a, const Region& b) { return a.score > b.score; });
    
    return regions;
}

float Code128Binarizer::calculateRegionScore(
    const std::vector<uint8_t>& image, int width, int height, const Region& region) const {
    int edgeCount = 0;
    int totalPixels = 0;
    
    // Calculate horizontal edge density with adaptive threshold
    for (int y = region.y; y < region.y + region.height && y < height - 1; y++) {
        for (int x = region.x; x < region.x + region.width && x < width; x++) {
            int diff = std::abs(static_cast<int>(image[y * width + x]) - 
                              static_cast<int>(image[(y + 1) * width + x]));
            if (diff > 50) {  // Edge threshold
                edgeCount++;
            }
            totalPixels++;
        }
    }
    
    // Calculate vertical edge density
    for (int y = region.y; y < region.y + region.height && y < height; y++) {
        for (int x = region.x; x < region.x + region.width - 1 && x < width - 1; x++) {
            int diff = std::abs(static_cast<int>(image[y * width + x]) - 
                              static_cast<int>(image[y * width + x + 1]));
            if (diff > 50) {  // Edge threshold
                edgeCount++;
            }
            totalPixels++;
        }
    }
    
    return static_cast<float>(edgeCount) / totalPixels;
}

std::vector<uint8_t> Code128Binarizer::processRegion(
    const std::vector<uint8_t>& image, int width, int height, 
    const Region& region, const AdaptiveParams& params) const {
    std::vector<uint8_t> regionData(region.width * region.height);
    
    // Extract region data
    for (int y = 0; y < region.height && y + region.y < height; y++) {
        for (int x = 0; x < region.width && x + region.x < width; x++) {
            regionData[y * region.width + x] = image[(y + region.y) * width + (x + region.x)];
        }
    }
    
    // Apply Code 128 specific optimizations to the region with adaptive parameters
    Code128Enhancer::applyDirectionalFilter(regionData, region.width, region.height, 
                                          static_cast<int>(5.0f * (1.0f + params.moduleVariance)));
    Code128Enhancer::correctBarWidths(regionData, region.width, region.height);
    return validatePatterns(regionData, region.width, region.height);
}

std::vector<uint8_t> Code128Binarizer::validatePatterns(const std::vector<uint8_t>& image, int width, int height) {
    std::vector<uint8_t> result = image;
    
    // Process each row
    for (int y = 0; y < height; y++) {
        std::vector<std::array<int, 6>> patterns;
        std::array<int, 6> currentWidths = {0};
        int patternIndex = 0;
        bool inBar = false;
        float moduleSize = 0;
        bool foundStart = false;
        
        // First pass: collect patterns and look for start pattern
        for (int x = 0; x < width; x++) {
            bool isBlack = image[y * width + x] < 128;
            
            if (isBlack != inBar) {
                if (patternIndex < 6) {
                    currentWidths[patternIndex++] = 1;
                }
                inBar = isBlack;
            } else if (patternIndex < 6) {
                currentWidths[patternIndex - 1]++;
            }
            
            if (patternIndex == 6) {
                auto pattern = extractPattern(currentWidths, moduleSize);
                
                // If we haven't found a start pattern yet, check for one
                if (!foundStart) {
                    if (isStartStopPattern(pattern)) {
                        foundStart = true;
                        moduleSize = std::accumulate(currentWidths.begin(), currentWidths.end(), 0.0f) / CHAR_SIZE;
                        patterns.clear(); // Clear any patterns before start
                        patterns.push_back(currentWidths);
                    }
                } else {
                    // After start pattern, be more lenient with validation
                    pattern.isValid = pattern.confidence > 0.5f; // Lower threshold after start
                    if (pattern.isValid) {
                        patterns.push_back(currentWidths);
                    }
                }
                
                patternIndex = 0;
                currentWidths.fill(0);
            }
        }
        
        // Second pass: validate and correct patterns
        if (foundStart && !patterns.empty()) {
            int x = 0;
            float avgModuleSize = 0;
            int validPatternCount = 0;
            
            // Calculate average module size from valid patterns
            for (const auto& patternWidths : patterns) {
                auto pattern = extractPattern(patternWidths, moduleSize);
                if (pattern.isValid) {
                    avgModuleSize += std::accumulate(patternWidths.begin(), patternWidths.end(), 0.0f) / CHAR_SIZE;
                    validPatternCount++;
                }
            }
            avgModuleSize /= validPatternCount;
            
            // Apply patterns with dynamic module size adjustment
            for (const auto& patternWidths : patterns) {
                auto pattern = extractPattern(patternWidths, avgModuleSize);
                if (pattern.isValid) {
                    // Apply corrected pattern
                    for (size_t i = 0; i < pattern.modules.size(); i++) {
                        int width = std::max(1, static_cast<int>(pattern.modules[i] * avgModuleSize + 0.5f));
                        bool isBar = (i % 2 == 0);
                        for (int j = 0; j < width && x < width; j++, x++) {
                            result[y * width + x] = isBar ? 0 : 255;
                        }
                    }
                } else {
                    // For invalid patterns, try to recover using neighboring patterns
                    float localModuleSize = avgModuleSize;
                    if (x > 0 && x < width - 1) {
                        // Use local information to adjust module size
                        int prevWidth = 0;
                        int nextWidth = 0;
                        for (int i = 1; i <= 5; i++) {
                            if (x - i >= 0) prevWidth += (image[y * width + (x-i)] < 128) ? 1 : 0;
                            if (x + i < width) nextWidth += (image[y * width + (x+i)] < 128) ? 1 : 0;
                        }
                        localModuleSize = (prevWidth + nextWidth) / 10.0f;
                    }
                    
                    // Apply best-effort correction
                    for (size_t i = 0; i < patternWidths.size(); i++) {
                        int width = std::max(1, static_cast<int>(patternWidths[i] * localModuleSize / avgModuleSize + 0.5f));
                        bool isBar = (i % 2 == 0);
                        for (int j = 0; j < width && x < width; j++, x++) {
                            result[y * width + x] = isBar ? 0 : 255;
                        }
                    }
                }
            }
        }
    }
    
    return result;
}

Pattern Code128Binarizer::extractPattern(const std::array<int, 6>& widths, float moduleSize) {
    Pattern pattern;
    pattern.modules = widths;
    
    if (moduleSize <= 0) {
        // Calculate initial module size from the pattern
        int totalWidth = std::accumulate(widths.begin(), widths.end(), 0);
        moduleSize = static_cast<float>(totalWidth) / CHAR_SIZE;
    }
    
    if (moduleSize < 1.0f) {
        pattern.isValid = false;
        pattern.confidence = 0.0f;
        return pattern;
    }
    
    // Normalize the pattern
    auto normalized = normalizePattern(widths, moduleSize);
    
    // Calculate confidence
    pattern.confidence = calculateConfidence(normalized, widths, moduleSize);
    
    // Check if it's a valid pattern - more lenient threshold
    pattern.isValid = pattern.confidence > 0.5f;
    
    if (pattern.isValid) {
        pattern.modules = normalized;
    }
    
    return pattern;
}

bool Code128Binarizer::isStartStopPattern(const Pattern& pattern) {
    if (!pattern.isValid || pattern.confidence < 0.7f) {
        return false;
    }
    
    // Check against start patterns with higher tolerance
    for (const auto& startPattern : START_PATTERNS) {
        bool matches = true;
        for (size_t i = 0; i < 6; i++) {
            if (std::abs(pattern.modules[i] - startPattern[i]) > 0.7f) {
                matches = false;
                break;
            }
        }
        if (matches) return true;
    }
    
    // Check against stop pattern (first 6 elements) with higher tolerance
    bool matchesStop = true;
    for (size_t i = 0; i < 6; i++) {
        if (std::abs(pattern.modules[i] - STOP_PATTERN[i]) > 0.7f) {
            matchesStop = false;
            break;
        }
    }
    
    return matchesStop;
}

std::array<int, 6> Code128Binarizer::normalizePattern(const std::array<int, 6>& widths, float moduleSize) {
    std::array<int, 6> result;
    
    if (moduleSize <= 0) return result;
    
    // Calculate total modules for this pattern
    float totalModules = 0;
    for (size_t i = 0; i < 6; i++) {
        totalModules += static_cast<float>(widths[i]) / moduleSize;
    }
    
    // Adjust module size if total is off
    if (std::abs(totalModules - CHAR_SIZE) > 1.0f) {
        moduleSize = std::accumulate(widths.begin(), widths.end(), 0.0f) / CHAR_SIZE;
    }
    
    // Convert each width to module count with error distribution
    float remainingError = 0.0f;
    for (size_t i = 0; i < 6; i++) {
        float moduleCount = (static_cast<float>(widths[i]) / moduleSize) + remainingError;
        int roundedCount = static_cast<int>(moduleCount + 0.5f);
        remainingError = moduleCount - roundedCount;
        result[i] = roundedCount;
    }
    
    return result;
}

float Code128Binarizer::calculateConfidence(const std::array<int, 6>& normalized,
                                          const std::array<int, 6>& original,
                                          float moduleSize) {
    if (moduleSize <= 0) return 0.0f;
    
    float totalVariance = 0.0f;
    int totalModules = 0;
    
    // Calculate variance from expected widths with weighted importance
    for (size_t i = 0; i < 6; i++) {
        float expectedWidth = normalized[i] * moduleSize;
        float variance = std::abs(original[i] - expectedWidth) / moduleSize;
        
        // Weight the variance based on position (edges are more important)
        float weight = (i == 0 || i == 5) ? 1.5f : 1.0f;
        totalVariance += variance * weight;
        totalModules += normalized[i];
    }
    
    // Adjust confidence based on total modules
    float modulePenalty = std::abs(totalModules - CHAR_SIZE) * 0.2f;
    
    // Convert variance to confidence score (0.0 to 1.0)
    float avgVariance = totalVariance / 7.5f; // Adjusted for weights
    float confidence = 1.0f - (avgVariance / MAX_VARIANCE) - modulePenalty;
    
    return std::max(0.0f, std::min(1.0f, confidence));
}

} // namespace ZXing
